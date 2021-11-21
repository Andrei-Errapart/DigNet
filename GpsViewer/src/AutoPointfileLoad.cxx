#include "AutoPointfileLoad.h"
#include <string>
#include <fstream>
#include <algorithm> //std::count
#include <fx.h>

#include <utils/CMR.h>
#include <utils/util.h>
#include <utils/mystring.h>

using namespace std;
using namespace utils;


//=====================================================================================//
void 
AutoPointfileLoad::InitDownload(const PACKET_POINTSFILE_INFO& info)
{
    download_pointfile_name_ = ssprintf("points_%05d.ipt", info.number);
    ofstream out(download_pointfile_name_.c_str());
    string buf;
    buf.resize(info.size);
    out.write(&buf[0], info.size);
    const string trackname(ssprintf("%s.track", download_pointfile_name_.c_str()));
    ofstream track(trackname.c_str());
    track.write(reinterpret_cast<const char*>(&info.size), sizeof(info.size));
    buf.resize(info.size/CHUNK_SIZE+1);
    fill(buf.begin(), buf.end(), '0');
    track.write(buf.c_str(), buf.size());
    download_last_chunk_ = -1;
    download_pointfile_size_ = info.size;
    download_done_ = false;
    download_pointfile_number_ = info.number;
    downloaded_chunks_ = 0;
    popped_ = false;
}


//=====================================================================================//
string::size_type
AutoPointfileLoad::GetNextChunk()
{
    const string trackname(ssprintf("%s.track", download_pointfile_name_.c_str()));
    ifstream in(trackname.c_str());
    in.read(reinterpret_cast<char*>(&download_pointfile_size_), sizeof(uint32_t));
    const int numChunks = download_pointfile_size_/CHUNK_SIZE+1;
    string buf;
    buf.resize(numChunks);
    in.read(&buf[0], numChunks);
    if (++download_last_chunk_>numChunks){
        download_last_chunk_ = 0;
    }
    // Search fom last chunk to the end
    string::size_type pos = buf.find_first_of('0', download_last_chunk_);
    if (pos==string::npos){
        download_last_chunk_ = 0;
        // Search from beginning to last chunk
        pos = buf.find_first_of('0');
    }
    return pos;
}


//=====================================================================================//
AutoPointfileLoad::AutoPointfileLoad():
    download_pointfile_name_("")
    ,download_pointfile_number_(-1)
    ,download_pointfile_size_(-1)
    ,download_last_chunk_(-1)
    ,download_done_(true)
    ,downloaded_chunks_(-1)
    ,popped_(false)
{
}

//=====================================================================================//
bool
AutoPointfileLoad::GetRequest(
    PACKET_POINTSFILE_CHUNK_REQUEST& packet)
{
    if (download_done_){
        return false;
    }
    const string::size_type pos = GetNextChunk();
    if (pos == string::npos){
        return false;
    }
    packet.chunk_number = pos;
    packet.file_number = download_pointfile_number_;
    TRACE_PRINT("info", ("Requesting chunk # %d %d", packet.chunk_number, pos ));

    return true;
}


//=====================================================================================//
bool 
AutoPointfileLoad::HandlePacket(
    const CMR& packet)
{
    switch (packet.type){
    case PACKET_POINTSFILE_INFO::CMRTYPE: {
        if (packet.data.size()<sizeof(PACKET_POINTSFILE_INFO)){
            TRACE_PRINT("info", ("Wrong pointfile info size. Got %d, expected %d", packet.data.size(), sizeof(PACKET_POINTSFILE_INFO)));
            break;
        }
        const PACKET_POINTSFILE_INFO* info = reinterpret_cast<const PACKET_POINTSFILE_INFO*>(&packet.data[0]);
        string filename;
        if (!GetNewFile(filename)){
            filename="";
        }
        // File name is in fixed format points_XXXXX.ipt
        
        int latestNum = -1;
        if (filename==""){
            if (!download_done_){
                latestNum = download_pointfile_number_;
            }
        } else {
            latestNum = get_pointfile_number(filename);
        }
        if (latestNum<info->number && download_done_ || (!download_done_ && download_pointfile_number_ < info->number)){
            download_pointfile_number_ = info->number;
            InitDownload(*info);
        }
        return true;
    }
    case PACKET_POINTSFILE_CHUNK::CMRTYPE:{
        if (packet.data.size()<sizeof(PACKET_POINTSFILE_CHUNK)){
            TRACE_PRINT("info", ("Wrong pointfile chunk size. Got %d, expected %d", packet.data.size(), sizeof(PACKET_POINTSFILE_CHUNK)));
            break;
        }
        const PACKET_POINTSFILE_CHUNK* chunk = reinterpret_cast<const PACKET_POINTSFILE_CHUNK*>(&packet.data[0]);
        if (chunk->file_number != download_pointfile_number_){
            TRACE_PRINT("info", ("Got chunk from file #%d when expecting chunks from #%d, discarding", chunk->file_number, download_pointfile_number_));
            break;
        }
        const int chunk_size = chunk->chunk_number < download_pointfile_size_/CHUNK_SIZE ? CHUNK_SIZE : download_pointfile_size_%CHUNK_SIZE;
        fstream points(download_pointfile_name_.c_str(), ios::in|ios::out|ios::binary);
        string buf;
        buf.resize(download_pointfile_size_);
        points.read(&buf[0], download_pointfile_size_);
        memcpy(&buf[chunk->chunk_number*CHUNK_SIZE], chunk->data, chunk_size);
        points.seekp(0, ios::beg);
        points.write(buf.c_str(), download_pointfile_size_);

        string trackname(ssprintf("%s.track", download_pointfile_name_.c_str()));
        const int trackSize = download_pointfile_size_/CHUNK_SIZE+1+4;
        buf.resize(trackSize);
        fstream track(trackname.c_str(), ios::in|ios::out|ios::binary);
        track.read(&buf[0], trackSize);
        buf[chunk->chunk_number+4] = '1';
        track.seekp(0, ios::beg);
        track.write(buf.c_str(), trackSize);
        const int chunks_left = count(buf.begin()+4, buf.end(), '0');
        download_done_ = chunks_left == 0 ? true : false;
        downloaded_chunks_ = buf.size() - 4 - chunks_left;
        return true;
    }
    default:
        return false;
    }
    return false;
}


//=====================================================================================//
string AutoPointfileLoad::Title() const
{
    string s;
    if (!download_done_ && download_pointfile_number_!=-1){
        // added pointfile_size_%CHUNK_SIZE so percentage won't get >100
        const int total_chunks = download_pointfile_size_/CHUNK_SIZE+1;
        const float progress = downloaded_chunks_==0 ? 0.0 : (downloaded_chunks_*100.0)/total_chunks;
        s.append(ssprintf(" (#%d, %3.1f%%)", download_pointfile_number_, progress));
    }
    return s;
}


//=====================================================================================//
bool
AutoPointfileLoad::GetNewFile(string& file)
{
    FXString* fileList;
    const int numFiles = FXDir::listFiles(fileList, ".", "points_*.ipt", FXDir::NoDirs);
    TRACE_PRINT("info", ("Found %d point files", numFiles));
    int maxDoneNum = -1, maxNotDoneNum = -1;
    int fileDoneNum = -1, fileNotDoneNum = -1;// Filename index in fileList array
    for (int i=0; i<numFiles; i++){
        TRACE_PRINT("info", ("File %d: %s", i, fileList[i].text()));
        const int pos = fileList[i].find_first_of('.');
        // '_' is always sixth in file name
        const FXString s = fileList[i].mid(7, pos-7);
        int num;
        if (int_of(s.text(), num)){
            // check if download has been finished
            download_pointfile_name_ = fileList[i].text();
            string::size_type chunk = GetNextChunk();
            if (chunk==string::npos){
                maxDoneNum = num;
                fileDoneNum = i;
            } else {
                maxNotDoneNum = num;
                fileNotDoneNum = i;
            }
        }
    }
    if (maxNotDoneNum>maxDoneNum){
        TRACE_PRINT("info", ("Resume downloading file %d", maxNotDoneNum));
        download_pointfile_number_ = maxNotDoneNum;
        download_pointfile_name_ = fileList[fileNotDoneNum].text();
        download_done_ = false;
        download_last_chunk_ = GetNextChunk();
        // kind of ugly way to get download status. Needed only when resuming download
        const string track(ssprintf("%s.track", download_pointfile_name_.c_str()));
        ifstream in(track.c_str());
        in.read(reinterpret_cast<char*>(&download_pointfile_size_), sizeof(uint32_t));
        const int numChunks = download_pointfile_size_/CHUNK_SIZE+1;
        string buf;
        buf.resize(numChunks);
        in.read(&buf[0], numChunks);
        downloaded_chunks_ = count(buf.begin(), buf.end(), '1');
    }
    if (maxDoneNum==-1){
        TRACE_PRINT("info", ("No downloaded files found"));
    } else {
        TRACE_PRINT("info", ("Biggest downloaded index is %d: file: %s", maxDoneNum, fileList[fileDoneNum].text()));
        file=fileList[fileDoneNum].text();
        return true;
    }
    return false;
}

//=====================================================================================//
bool 
AutoPointfileLoad::PopNewFile(
    string& file)
{
    // File has already been requested
    if (popped_){
        return false;
    }
    const string::size_type chunk = GetNextChunk();
    if (chunk != string::npos){
        return false;
    }
    GetNewFile(file);
    popped_ = true;
    return true;
}

//=====================================================================================//
int 
get_pointfile_number(
    const string& filename)
{
    string::size_type pos = filename.find_first_of('_');
    if (pos == string::npos){
        return -1;
    }
    const string num = filename.substr(pos+1, 5);
    return int_of(num);
}

