Build Number

Manages sequental build numbers of projects. 

There can be many projects. Each project has ID, name and sequence of builds. For every build it has ID, build time, build number and Mercurial head number.

When build script is executed it downloads a file from specific URL. That file is a regular C++ header that contains needed information as preprocessor defines. An example file could look like this:

----
#define BUILD_NUMBER 10
#define BUILD_NAME   "Project"
#define BUILD_VERSION 450
----

Parameters from build machine are sent to server as PHP _GET parameters. Possible parameters: 
name: name of the project, must match a project already in DB
headVersion: revision number reported by version control system. If not set then build number will not be updated, only information will be displayed

Program assumes that there exists entries in both tables. At minimum there has to be project name and one version with project_id the same as in 
ProjectName table



get_build_header.sh
Bash script that downloads the header when given project name. Assumes that it is run under Mercurial repository directory as it needs to 
check if there are uncommited changes and find the head revision number. It needs to be given project name as a parameter when run. It 
aborts if no project name is given or there are uncommited changes.



downloader.cpp/BuildNumber.*
C++ program for downloading the version.h header. Basically acts the same way as get_build_header.sh only it has different parameters. Usage:
BuildNumber download_URL project_name
	download_URL: URL of the .php file that manages builds, e.g http://194.204.26.104/~kalle/getBuild.php
	project_name: name of the project, must be in DB


TODO: 
	proper parameters with defaults (--url --name)
	add a simple web interface to see build information

Mercuriali head number baasi
Enne buildi hg update
Kui on commitimata muudatusi siis ei käivitu
