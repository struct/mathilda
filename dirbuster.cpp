// Copyright 2015 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf

// An example tool that uses Mathilda
// g++ -o dirbuster dirbuster.cpp mathilda.cpp -std=c++11 -ggdb -lcurl -luv

#include "mathilda.h"
#include <sstream>
#include <fstream>
#include <string>

using namespace std;

vector<std::string> read_file(char *f) {
	std::ifstream file(f);
	std::string line;
	vector <std::string> ret;

	while(std::getline(file, line))
		ret.push_back(std::move(line));

	return ret;
}

// This only executes if we get a 200 OK
int dirbuster_after(Instruction *i, CURL *c, Response *r) {
	fprintf(stdout, "Found %s %s\n", i->host.c_str(), i->path.c_str());
	return OK;
}

int main(int argc, char *argv[]) {
	unique_ptr<Mathilda> m(new Mathilda());

	if(argc != 4) {
		fprintf(stdout, "dirbuster <hosts> <pages> <directories>\n");
		return -1;
	}

	vector <std::string> hosts;
	hosts = read_file(argv[1]);

	vector <std::string> pages;
	pages = read_file(argv[2]);

	vector <std::string> dirs;
	dirs = read_file(argv[3]);

	vector <std::string> methods;
	methods.push_back("GET");
	methods.push_back("POST");
	methods.push_back("HEAD");
	methods.push_back("PUT");

	for(auto h : hosts) {
		for(auto y : dirs) {
			for(auto z : pages) {
				for(auto j : methods) {
					std::string path = "/" + y + z;
					Instruction *i = new Instruction((char *)h.c_str(), (char *) path.c_str());
					i->after = dirbuster_after;
					i->follow_redirects = false;
					i->port = 80;
					i->response_code = 200;
					i->http_method = j;
					m->add_instruction(i);
				}
			}
		}
	}

	m->safe_to_fork = true;
	m->execute_instructions();

	return OK;
}
