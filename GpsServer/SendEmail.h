#ifndef SENDEMAIL_H
#define SENDEMAIL_H

#include <string>
#include <vector>


/**
	@author Kalle Last <kalle@errapartengineering.com>
    Uses custom commandline tool mail.sh to send out e-mails in background
*/
class SendEmail {
public:
    SendEmail();

    ~SendEmail();

    /**
        Sends e-mail from given address to list of receivers.
        Note: subject and message are enclosed with doublequotes. If they contain quotes they have to be escaped.
    
        @param to vector of receiver addresses
        @param subject e-mail subject
        @param message message body
    */
    void send(
        const std::vector<std::string>& to,
        const std::string& subject,
        const std::string& message
    );
};

#endif
