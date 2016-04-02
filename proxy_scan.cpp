// Copyright 2015 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf

// An example tool that uses Mathilda
// g++ -o proxy_scan proxy_scan.cpp mathilda.cpp mathilda_utils.cpp mathilda_fork.cpp -std=c++11 -ggdb -lcurl -luv
#include "mathilda.h"
#include "mathilda_utils.h"
#include <sstream>
#include <fstream>
#include <string>

using namespace std;
const char *host_hdr = "Host: your.internal.example-host.com";

int main(int argc, char *argv[]) {
	unique_ptr<Mathilda> m(new Mathilda());

	vector <std::string> hosts;
	MathildaUtils::read_file(argv[1], hosts);

	for(auto y : hosts) {
		std::vector<std::string> out;
		// Expects format of "port:host"
		MathildaUtils::split(y, ':', out);
		
		Instruction *i = new Instruction("your.internal.example-host.com", "/");
		i->before = [](Instruction *i, CURL *c) {
						i->add_http_header(host_hdr);
					};

		i->proxy = out[1];

		if(out[0] == "443") {
			i->ssl = true;
			i->proxy_port = 443;
		} else {
			i->proxy_port = 80;
		}

		i->use_proxy = true;
		m->add_instruction(i);
	}

	m->slow_parallel = true;
	m->timeout_seconds = 5;
	m->safe_to_fork = true;
	m->execute_instructions();

	return OK;
}