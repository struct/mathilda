// Copyright 2015/2016 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf
// A C++ Spider class for Mathilda

#include <gumbo.h>

using namespace std;

class Spider {

public:
	Spider(const vector<std::string> &p, const std::string &h, 
			const std::string &d, const std::string &c, uint16_t po) :
		restricted(true),
		port(po),
		host(h),
		domain(d),
		cookie_file(c),
		paths(p),
		m(NULL) {
#ifdef DEBUG
		fprintf(stdout, "[Spider] host(%s) : port(%d)\n", host.c_str(), port);
#endif
	}

	~Spider() {	}

	// When true (default) we do not spider hosts
	// other than the one specified by the caller
	bool restricted;

	// Web server port
	uint16_t port;

	// The host we are spidering
	std::string host;

	// Restrict spidering to this domain
	// Doesn't override 'restricted' flag
	std::string domain;

	// Curl cookie file
	std::string cookie_file;

	// A list of paths. Usually populated
	// by a dirbuster. This may contain
	// full URIs including http/https
	std::vector<std::string> paths;

	// POST links
	map<std::string, std::string> posts;

	// This is the final set of links which
	// is populated by the spider
	// These contain full URIs including http/https
	std::vector<std::string> links;
	std::vector<std::string> spider_links;

	Mathilda *m;

    bool search_link_duplicates(std::string s);
    void search_for_links(Instruction *i, GumboNode* node);
    void spider_after(Instruction *i, CURL *c, Response *r);
    void spider_finish(ProcessInfo *pi);
    void run(int times);
    void run();
};