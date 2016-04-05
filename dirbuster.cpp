// Copyright 2015/2016 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf
// A C++ Dirbuster class for Mathilda

#include "mathilda.h"
#include "mathilda_utils.h"
#include "dirbuster.h"

// Runs the dirbuster
void Dirbuster::run() {
    unique_ptr<Mathilda> m(new Mathilda());

	for(auto const &y : directories) {
		for(auto const &z : pages) {
			auto path = "/" + y + z;
			Instruction *i = new Instruction((char *) host.c_str(), (char *) path.c_str());
			i->after = std::bind(&Dirbuster::dirbuster_after, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
			i->follow_redirects = false;
			i->port = port;
			i->cookie_file = cookies;

			if(i->port == 443) {
				i->ssl = true;
			}

			m->add_instruction(i);
		}
	}

	// We are going to fork, the dirbuster_after
	// callback will populate it
	paths.clear();

	// We use shm in dirbuster_finish to repopulate
	// the paths vector
	m->use_shm = true;
	m->safe_to_fork = true;
	m->finish = std::bind(&Dirbuster::dirbuster_finish, this, std::placeholders::_1);
	m->execute_instructions();
}

// We only get here with valid 200 responses
void Dirbuster::dirbuster_after(Instruction *i, CURL *c, Response *r) {
	// A naive check to remove blank return pages
	// or pages with only a comment, json etc...
	if(r->text == NULL || r->size < 75 || r->text[0] == '{') {
		return;
	}

	std::string f = r->text;

	// Query the blacklist
	if(MathildaUtils::page_blacklist(f.c_str())) {
		return;
	}

	std::string uri;

	if(i->ssl == true) {
		uri = "https://" + i->host + "/" + i->path;
	} else {
		uri = "http://" + i->host + "/" + i->path;
	}

	uri = MathildaUtils::normalize_uri(uri);

	MathildaFork *mf = i->mathilda->mf;

	if(i->mathilda->use_shm == true) {
		mf->shm_store_string(uri.c_str());
	} else {
		paths.push_back(uri);
	}

	return;
}

// Finish up by copying all valid paths from
// shared memory to our paths vector
void Dirbuster::dirbuster_finish(uint8_t *shm_ptr) {
	RET_IF_PTR_INVALID(shm_ptr);

	StringEntry *head = (StringEntry *) shm_ptr;
	char *string;

	while(head->length != 0 && head->length < 16384) {
		string = (char *) head + sizeof(StringEntry);
		paths.push_back(string);
		head += head->length;
	}
}