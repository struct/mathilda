// Copyright 2015/2016 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf

#include <arpa/inet.h>
#include <netdb.h>

class MathildaDNS {
public:
	MathildaDNS(bool uc = true) {
		use_cache = uc;
	}

	~MathildaDNS() {}

	void flush_cache();
	void disable_cache();
	void enable_cache();
	int name_to_addr(std::string const &host, std::vector<std::string> &results, bool fast);
	int addr_to_name(std::string const &ip, std::string &result);
	void name_to_addr_a(std::vector<std::string> const &hostnames, std::vector<std::string> &results);
	void addr_to_name_a(std::vector<std::string> const &ips, std::vector<std::string> &results);

	bool use_cache;

	// The cache structure is simple: <IP, Hostname>
	// Every IP will be unique, but its possible
	// that hostnames will be repeated. Both are
	// std::string for simplicity
	std::map<std::string, std::string> dns_cache;
};