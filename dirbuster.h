// Copyright 2015/2016 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf
// A C++ Spider class for Mathilda

using namespace std;

class Dirbuster {

public:
	Dirbuster(const vector<std::string> &h, const vector<std::string> &p, const vector<std::string> &d,
				const std::string &c, uint16_t po) :
		hosts(h), pages(p), directories(d), cookies(c), port(po)
	{
#ifdef DEBUG
		fprintf(stdout, "[Dirbuster] host(%s) : port(%d)\n", host.c_str(), port);
#endif
	}

	~Dirbuster() {}

	// Host we are dirbustering
	std::vector<std::string> hosts;

	// Pages
	std::vector<std::string> pages;

	// Directories
	std::vector<std::string> directories;

	// Cookies file
	std::string cookies;

	// Stores all the valid paths we find
	std::vector<std::string> paths;

	// Web server port
	uint16_t port;

	void run();
	void dirbuster_finish(uint8_t *shm_ptr);
	void dirbuster_after(Instruction *i, CURL *c, Response *r);
};