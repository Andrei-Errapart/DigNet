
#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>


#include <utils/CMR.h>
/**
	Reads in raw file and outputs to stdout. Used to send stuff (like RTCM) to TCP ports with netcat.
*/



using namespace std;
using namespace utils;

void usage() {
	cout<<"Usage:\n rtcmecho inputfile"<<endl;
}


int main (	int	argc,
           char**	argv ) {
	ifstream in;
	if ( argc>=2 ) {
		in.open ( argv[1], ios::in );
		if ( !in.is_open() ) {
			cout<<"Unable to open file '"<<argv[1]<<"'"<<endl;
			usage();
			return 1;
		}
	} else {
		cout<<"No input file given"<<endl;
		usage();
		return 1;
	}
	
	//cout<<"initializing"<<endl;
	usleep(1000000);
	while ( !in.eof() ) {
		std::string s;
		getline ( in, s );
		const int numRead=in.gcount();
		
		vector<unsigned char> buf;
		CMR cmr;
		cmr.emit(buf, CMR::LAMPNET, s);
		cout.write ( &buf[0], buf.size());
		cout.flush();
		//cout.write ( &s[0], s.length());
		usleep(1000000);
	}
	return 0;
}

