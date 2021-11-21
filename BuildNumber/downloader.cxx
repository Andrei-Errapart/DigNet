#include <cstdio>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>

using namespace std;

#if defined(_MSC_VER) || defined(__WATCOMC__)        // Microsoft Visual C++ or Watcom C++
#define popen _popen
#define pclose _pclose
#define NULL_FILE "nul"
#else
#define NULL_FILE "/dev/null"
#endif

const char* HEADER_FILE = "version.h";

/*********************************************************************************/
void
usage() 
{
    cout << "Usage:" << endl;
    cout << "\tBuildNumber download_url project_name" << endl;
    cout << endl;
    cout << "\t\tMust be run from the directory containing a mercurial repository!" << endl;
    cout << "\t\tServer running the script and hosting the database must already" <<endl;
    cout << "\t\thave the initial information about the project being requested." << endl;
}


/*********************************************************************************/
bool
runCommand(
       const string&    cmd,
       string&          output,
       const string&    dir = "")
{
    FILE* process;
    if (dir.length()==0){
        process = popen(cmd.c_str(), "rt");
    } else {
        // If directory is set first change to that directory before executing the command
        string realCmd("cd ");
        realCmd.append(dir);
        realCmd.append(" && ");
        realCmd.append(cmd);
        process = popen(realCmd.c_str(), "rt");
    }
    if (process == NULL){
        cout<<"Error, unable to run command '"<<cmd<<"'"<<(dir.length()==0?"": " in" +dir)<<endl;
        return false;
    }
    const int BUF_SIZE = 1024;
    char buf[BUF_SIZE];
    size_t bytesRead;
    // Read program output to buffer
    while ((bytesRead = fread(buf, 1, sizeof(buf), process)) != 0){
        output.append(buf, bytesRead);
    }
    pclose(process);
    return true;
}


/*********************************************************************************/
bool
hasUncommited(
        string&         msg,
        const string&   dir = "")
{
    // TODO detect used repository and use the tools needed
    string output;
    if (!runCommand("hg diff", output, dir)){
        msg="#error Unable to run hg diff"+(dir.length()==0?"": " in" +dir);
        return false;
    }
    // If something got returned by diff then there are uncommitted changes
    if (output.length() > 0) {
        msg = "#error uncommitted changes found"+(dir.length()==0?"": " in" +dir);
        return true;
    }
    return false;
}


/*********************************************************************************/
int
getRevision(
        string&         msg, 
        const string&   dir = "")
{
    string output;
    if (!runCommand("hg tip", output, dir)){
        msg="#error Unable to find revision"+(dir.length()==0?"": " in" +dir);
        return -1;
    }
    // Find changeset number from output
    stringstream changeset(output);
    string tmp;
    changeset>>tmp; // get rid of "changeset:
    changeset>>tmp; // get the rest of the line containing changeset number
    size_t last= tmp.find_first_of(":");
    tmp=tmp.substr(0, last);
    return atoi(tmp.c_str());
}


/*********************************************************************************/
void
printMsg(
        const string&   msg)
{
    ofstream out(HEADER_FILE);
    out << msg << endl;
    cout << msg << endl;
}


/*********************************************************************************/
int
main(
        int     argc, 
        char    *argv[]) 
{
    if (argc != 3) {
        string msg = "#error wrong parameters when getting version info";
        printMsg(msg);
        usage();
        return 1;
    }

    // Check for changes in base and in DigNet
    // FIXME: do not hardcode base location!
    string msg;
    runCommand("pwd", msg);
    printMsg("PWD: "+msg);
    if (hasUncommited(msg, "..\\..\\..\\base")){
        printMsg(msg);
        return 1;
    }
    if (hasUncommited(msg)){
        printMsg(msg);
        return 1;
    }

    // Get the revision number of base and DigNet
    int baseRev = getRevision(msg, "..\\..\\..\\base");
    if (baseRev == -1) {
        printMsg(msg);
        return 1;
    }
    int dignetRev = getRevision(msg);
    if (dignetRev == -1) {
        printMsg(msg);
        return 1;
    }
    
    // get the header
    stringstream cmd;
    cmd << "wget \"" << argv[1] << "?name=" << argv[2] << "&headVersion=" << dignetRev << "&baseVersion=" << baseRev<< "\" -O " << HEADER_FILE << " 2> " << NULL_FILE;
    if (runCommand(cmd.str(), msg)){
        cout << argv[2] << " version: " << dignetRev << ":" << baseRev << endl;
    } else {
        printMsg("Unable to get version information");
        return 1;
    }
    return 0;
}
