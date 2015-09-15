// Copyright 2015 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf

// An example tool that uses Mathilda
// g++ -o proxy_scan proxy_scan.cpp mathilda.cpp -std=c++11 -ggdb -lcurl -luv
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

int main(int argc, char *argv[]) {
	unique_ptr<Mathilda> m(new Mathilda());

	vector <std::string> hosts;
	hosts = read_file(argv[1]);

	for(auto y : hosts) {
		Instruction *i = new Instruction((char *) y.c_str(), "host.you.want.to.proxy.to.example.com");
		i->proxy = y;
		i->proxy_port = 80;
		i->use_proxy = true;
		m->add_instruction(i);
	}

	m->safe_to_fork = true;
	m->execute_instructions();

	return OK;
}
