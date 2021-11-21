#include <pthread.h>
#include <string>
#include <vector>
#include <iostream>
#include <utils/util.h>            // SimpleConfig, Error.

#include "SendEmail.h"

using namespace std;
using namespace utils;

SendEmail::SendEmail()
{
}


SendEmail::~SendEmail()
{
}


void SendEmail::send(
        const std::vector<std::string>& to, 
        const std::string& subject, 
        const std::string& message)
{
    if (to.size()>0){
        cout<<"Sending e-mail with subject "<< subject <<endl;
        string cmd;
        cmd=ssprintf("sh mail.sh -s \"%s\" ", subject.c_str());
        for (unsigned int i = 1;i<to.size();i++){
            cmd+=ssprintf("%s ", to[i].c_str());
        }
        cmd += ssprintf("-m \"%s\" ", message.c_str());
        cout<<"Calling '"<< cmd<<"'"<<endl;
        FILE* process = popen(cmd.c_str(), "r");
        pclose(process);
    }
}
