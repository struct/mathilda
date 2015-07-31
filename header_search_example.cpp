// Copyright 2015 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf

// An example tool that uses Mathilda
// g++ -o hdr_srch header_search_example.cpp mathilda.cpp -std=c++11 -ggdb -lcurl -luv
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

int my_after(Instruction *i, CURL *c, Response *r) {
	if(r->size > 35) {
		std::string y(r->text);
		if((y.find("X-Forwarded-For")) != ERR) {
			printf("Found on host: (%s)\n%s\n", i->host.c_str(), r->text);
		}
	}

	return OK;
}

int my_before(Instruction *i, CURL *c) {
	curl_easy_setopt(c, CURLOPT_HEADER, 1L);
}

int main(int argc, char *argv[]) {
	unique_ptr<Mathilda> m(new Mathilda());

	vector <std::string> hosts;
	hosts = read_file("hosts.txt");

	for(auto y : hosts) {
		std::string path = "/";
		Instruction *i = new Instruction((char *) y.c_str(), (char *) path.c_str());
		i->before = my_before;
		i->after = my_after;
		m->add_instruction(i);
	}

	m->safe_to_fork = true;
	m->execute_instructions();

	return OK;
}