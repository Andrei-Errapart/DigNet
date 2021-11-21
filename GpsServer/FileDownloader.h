#ifndef FileDownloader_h_
#define FileDownloader_h_

#include <string>
#include <cstdio>
#include <pthread.h>
#include "Thread.h"
#include "CBuffer.h"
/**
    @author Kalle Last <kalle@errapartengineering.com>
    A class that repeatedly downloads a file asynchronously. Run \c download() every time you want do redownload an URL and check progress with \c downloadReady() to see when it finishes. If file is downloaded you can get the contents of the file as \c std::string with \c file()
*/
class FileDownloader : public Thread {
    std::string     download_url_;          ///< URL to be downloaded
    CBuffer<std::string, 3> file_contents_; ///< Downloaded file content
    bool            run_;                   ///< Run thread while true
    
    pthread_mutex_t file_contents_mutex_;   ///< Synchronizes file_conetnts_ modification
    pthread_mutex_t download_mutex_;        ///< If true then start downloading
    pthread_cond_t  download_done_cond_;    ///< True if download ready
    int             download_frequency_;    ///< Delay between downloads in seconds
  public:
    /** Ctor
    @param url URL to download 
    @param downloadFrequency how often file is downloaded, in seconds, must be in [0..3600[
     */
    FileDownloader(
        std::string url,
        int downloadFrequency);

    ~FileDownloader();

    /** Returns true if file has been downloaded, fills \c s with file content */
    bool
    popContents(std::string& s);
protected:
    /** Thread initialization */
    void setup();

    /** downloading loop is here */
    void execute();
};

#endif /* FileDownloader_h_ */
