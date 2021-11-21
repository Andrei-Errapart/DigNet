#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <utils/utils.h>
//#include <stropts.h>
#include <time.h>
#include <errno.h>
#include "FileDownloader.h"
#include "utils.h"

using namespace std;
using namespace utils;

/*****************************************************************************/

FileDownloader::FileDownloader(
    string url,
    int downloadFrequency):
    download_url_(url)
    ,run_(false)
    ,download_frequency_(downloadFrequency)
{
}

/*****************************************************************************/
FileDownloader::~FileDownloader()
{
    run_ = false;
    pthread_cond_signal(&download_done_cond_);
}

/*****************************************************************************/
bool
FileDownloader::popContents(string& s){
    pthread_mutex_lock(&file_contents_mutex_);
    const bool result = file_contents_.Pop(s);
    pthread_mutex_unlock(&file_contents_mutex_);
    return result;
}


/*****************************************************************************/
void 
FileDownloader::setup()
{
    run_ = true;
    pthread_mutex_init(&file_contents_mutex_, 0);
    pthread_mutex_init(&download_mutex_, 0);
    pthread_cond_init(&download_done_cond_, 0);
}


/*****************************************************************************/
void 
FileDownloader::execute()
{
    while(1){
        if (!run_) {
            cout<<"Exiting"<<endl;
            break;
        }

        // tempnam returns a new char*, must free later
        string fileName;
        tmpname("weather", fileName);
        const string download_cmd(ssprintf("wget \"%s\" -O \"%s\" -o /dev/null", download_url_.c_str(), fileName.c_str()));
        system(download_cmd.c_str());
        std::ifstream html(fileName.c_str(), ios::binary);
        html.seekg(0, ios::end);
        size_t fileSize = html.tellg();
        html.seekg(0, ios::beg);
        string contents;
        contents.resize(fileSize);
        html.read(reinterpret_cast<char*>(&contents[0]), fileSize);
        html.close();
        // File is no longer needed
        unlink(fileName.c_str());

        pthread_mutex_lock(&file_contents_mutex_);
        file_contents_.Push(contents);
        pthread_mutex_unlock(&file_contents_mutex_);
        // Sleep for specified amount of time or until awaken
        timespec curtime;
        clock_gettime(CLOCK_REALTIME, &curtime);
        curtime.tv_sec += download_frequency_;
        pthread_cond_timedwait(&download_done_cond_, &download_mutex_, &curtime);
    }
    pthread_cond_destroy(&download_done_cond_);
    pthread_mutex_destroy(&download_mutex_);
    pthread_exit(0);
}
