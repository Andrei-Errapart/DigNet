#ifndef Auto_pointfile_Load_h_
#define Auto_pointfile_Load_h_

#include <string>

#include <utils/CMR.h> 
#include "DigNetMessages.h"


/** Manages pointfile download. It deals with pointfile info and pointfile chunk packets received from server. 
    Basically all that is needed is feeding all pointfile packets to \cHandlePacket() function. 
    \cTitle() returns pointfile information to be shown on titlebar as std::string
    \cGetRequest() fills in a chunk request packet to be sent to server
    \cGetNewFile() returns the latest successfully downloaded pointfile name
    \cPopNewFile() returns true if download has finished and returns latest pointfile name in parameter


    Class holds download status information in |.trac files. File format is as follows:
    uint32_t                        file_size:  pointfile size in bytes
    char[file_size/CHUNK_SIZE+1]    chunk_info: Basically a byte-sized boolean for every chunk. If a byte is '0' chunk is not downloaded, if '1' it is
    If chunk_info is all '1'-s then file is fully downloaded.
*/
class AutoPointfileLoad {
    std::string download_pointfile_name_;   ///< File to save pointfile information
    int         download_pointfile_number_; ///< Number of the file (0..65536)
    int         download_pointfile_size_;   ///< Total file size in bytes
    int         download_last_chunk_;       ///< Last received chunk number
    bool        download_done_;             ///< If false then ask for next chunk
    int         downloaded_chunks_;         ///< Number of chunks succcesfully downloaded

    int         latest_pointfile_;          ///< Latest finished pointfile
    bool        popped_;                    ///< True if completed file name has already been requested once.

    
    /** Initializes pointfile file downloading */
    void InitDownload(const PACKET_POINTSFILE_INFO& info);

    /** Returns the number of next chunk to be downloaded */
    std::string::size_type GetNextChunk();
public:
    AutoPointfileLoad();

    /** Fills a pointfile chunk request packet. Returns true if file is missing a chunk, false if file has been completely downloaded */
    bool 
    GetRequest(
        PACKET_POINTSFILE_CHUNK_REQUEST& packet
    );

    /** Handles pointfile packets (chunks and information). Takes in raw CMR and decodes it itself */
    bool 
    HandlePacket(
        const utils::CMR& packet
    );

    /** Returns information that goes to titlebar (currently used pointfile number, file being downloaded and progress */
    std::string 
    Title() const;

    /**  searches for latest completed file. Returns true if found a file, file name written to \c file */
    bool 
    GetNewFile(
        std::string& file
    );

    /** Returns true if new file has finished downloading. New file name is written to \c file */
    bool 
    PopNewFile(
        std::string& file
    );
};

/** Returns pointfile number extracted from given string */
int 
get_pointfile_number(
    const std::string& filename);


#endif
