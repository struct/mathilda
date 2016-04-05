// Copyright 2015/2016 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf
// A C++ Spider class for Mathilda

#include <gumbo.h>
#include "mathilda.h"
#include "mathilda_utils.h"
#include "spider.h"

bool Spider::search_link_duplicates(std::string s) {
	for(auto const &t : spider_links) {
		if(t == s) {
	    	return true;
		}
	}

	return false;
}

void Spider::search_for_links(Instruction *i, GumboNode* node) {
	if(node->type != GUMBO_NODE_ELEMENT) {
		return;
	}

  	GumboAttribute *href =  NULL;

  	if(node->v.element.tag == GUMBO_TAG_A && (href = gumbo_get_attribute(&node->v.element.attributes, "href"))) {
		RET_IF_PTR_INVALID(href);

		std::string href_uri = href->value;

		if(href_uri.size() == 0) {
			return;
		}

		if(href_uri[0] == '#') {
			if(links.size() >= 1) {
				href_uri = links.back();
			} else {
				return;
			}
		}

		if((MathildaUtils::is_http_uri(href_uri)) || (MathildaUtils::is_https_uri(href_uri))) {

			std::string h = MathildaUtils::extract_host_from_uri(href_uri);

			if(h.size() == 0) {
				return;
			}

			if((MathildaUtils::is_domain_host(domain, h)) == false) {
				return;
			}

			if(restricted == true && h != i->host) {
				return;
			}

			if((MathildaUtils::link_blacklist(h)) == true) {
				return;
			}
		} else {
			if(href_uri[0] == '?') {
				href_uri = i->host + "/" + i->path + href->value;
			} else {
				if(href_uri[0] != '/') {
					std::vector<std::string> av;
					MathildaUtils::split(i->path, '/', av);

					if(av.empty()) {
						return;
					}

					av.pop_back();
					std::string avv;

					for(auto const &a : av) {
						avv += "/" + a;
					}

					href_uri = i->host + avv + "/" +  href_uri;
				} else {
					href_uri = i->host + "/" + href->value;
				}
			}

			if((MathildaUtils::link_blacklist(href_uri)) == true) {
				return;
			}
		}

		if((MathildaUtils::is_http_uri(href_uri)) == false && MathildaUtils::is_https_uri(href_uri) == false) {
			if(i->ssl == true) {
				href_uri = "https://" + href_uri;
			} else {
				href_uri = "http://" + href_uri;
			}
		}

		MathildaUtils::replaceAll(href_uri, "../", "");

		if((search_link_duplicates(href_uri)) == false) {
			spider_links.push_back(href_uri);
			links.push_back(href_uri);
		}
	}

	GumboAttribute *action = NULL;
	std::string action_uri;

	if(node->v.element.tag == GUMBO_TAG_FORM && (action = gumbo_get_attribute(&node->v.element.attributes, "action"))) {

		std::string host;

		RET_IF_PTR_INVALID(action);

		action_uri = action->value;

		if(action_uri.size() == 0) {
			return;
		}

		if(action_uri[0] == '#') {
			if(links.size() >= 1) {
				action_uri = links.back();
			} else {
				return;
			}
		}

		if((MathildaUtils::is_http_uri(action_uri)) || (MathildaUtils::is_https_uri(action_uri))) {

			std::string h = MathildaUtils::extract_host_from_uri(action_uri);

			if(h.size() == 0) {
				return;
			}

			if((MathildaUtils::is_domain_host(domain, h)) == false) {
				return;
			}

			if(restricted == true && h != i->host) {
				return;
			}

			if((MathildaUtils::link_blacklist(h)) == true) {
				return;
			}
		} else {
			if(action_uri[0] != '/') {
				std::vector<std::string> av;
				MathildaUtils::split(i->path, '/', av);

				if(av.empty()) {
					return;
				}

				av.pop_back();
				std::string avv;

				for(auto const &a : av) {
					avv += "/" + a;
				}
				action_uri = avv + "/" +  action_uri;
			}

			host = i->host + "/" + action_uri;

			if((MathildaUtils::link_blacklist(host)) == true) {
				return;
			}
		}

		if(i->ssl == true) {
			posts[action->value] = "https://" + i->host + "/" + action_uri + "?";
		} else {
			posts[action->value] = "http://" + i->host + "/" + action_uri + "?";
		}

		GumboNode *n = NULL;
		GumboVector *c = &node->v.element.children;
		GumboAttribute *a = NULL;

		a = gumbo_get_attribute(&node->v.element.attributes, "method");
		bool is_post = true;

		if(a != NULL) {
			std::string method = a->value;

			if(method != "POST" || method != "post") {
				is_post = false;
			}
		}

		// Sometimes we see HTML like this:
		// <form>
		//   <input>
		//    <xyz>
		// 	   <input>
		//    </xyz>...
		// We make a poor attempt at iterating
		// the children of the xyz element but
		// its not a perfect solution
		// This builds a query string from the form
		for(unsigned int x = 0; x < c->length; x++) {
			n = static_cast<GumboNode*>(c->data[x]);

			if(n->v.element.tag != GUMBO_TAG_INPUT) {
				for(unsigned int j = 0; j < n->v.element.children.length; j++) {
					n = static_cast<GumboNode*>(n->v.element.children.data[j]);

					if(n->v.element.tag != GUMBO_TAG_INPUT) {
						break;
					}
				}

				if(n == NULL) {
					continue;
				}
			}

			if(n->v.element.tag == GUMBO_TAG_INPUT) {
				a = gumbo_get_attribute(&n->v.element.attributes, "type");

				if(a != NULL) {
					std::string su = a->value;

					if(su == "submit") {
						continue;
					}
				}

				a = gumbo_get_attribute(&n->v.element.attributes, "name");

				if(a == NULL) {
					continue;
				}

				std::string name = a->value;
				posts[action->value] += name + "=";

				a = gumbo_get_attribute(&n->v.element.attributes, "value");

				if(a == NULL) {
					continue;
				}

				std::string value = a->value;
				posts[action->value] += value + "&";
			}

			if(is_post == true) {
				posts[action->value] = "POST|" + posts[action->value];
			}
	 	}
	}

	GumboVector *children = &node->v.element.children;

	if(node->v.element.tag == GUMBO_TAG_UNKNOWN || children->data == NULL) {
		return;
	}

	for(unsigned int x = 0; x < children->length; x++) {
		if(children->data[x] != 0) {
			search_for_links(i, static_cast<GumboNode*>(children->data[x]));
		}
	}
}

void Spider::spider_after(Instruction *i, CURL *c, Response *r) {
	if(r->text == NULL || r->size < 10) {
		return;
	}

	r->text[r->size-1] = 0;

	if(i->ssl == true) {
		links.push_back("https://" + i->host + i->path);
	} else {
		links.push_back("http://" + i->host + i->path);
	}

	GumboOutput* output = gumbo_parse(r->text);
	search_for_links(i, output->root);

	if(i->mathilda->use_shm == true && i->mathilda->get_shm_ptr()) {
		MathildaFork *mf = i->mathilda->mf;

		for(auto const &pp : posts) {
			mf->shm_store_string(pp.second.c_str());
		}

		MathildaUtils::unique_string_vector(links);

		for(auto const &pp : links) {
			mf->shm_store_string(pp.c_str());
		}

		MathildaUtils::unique_string_vector(spider_links);

		for(auto const &pp : spider_links) {
			mf->shm_store_string(pp.c_str());
		}
	}

	return;
}

void Spider::spider_finish(uint8_t *shm_ptr) {
	RET_IF_PTR_INVALID(shm_ptr);

	StringEntry *head = (StringEntry *) shm_ptr;
	char *string;

	while(head->length != 0 && head->length < 16384) {
		string = (char *) head + sizeof(StringEntry);
		links.push_back(string);
		head += head->length;
	}
}

void Spider::run(int times) {
	while(times--) {
		run();
		paths = links;
	}
}

void Spider::run() {
	std::vector<std::string> pz = paths;

	Mathilda *m = new Mathilda();
	std::string h = host;

	for(auto const &p : pz) {

		if((MathildaUtils::is_domain_host(domain, h)) == false) {				
			continue;
		}

		if(h != host && restricted == true) {
			continue;
		}

		std::string start_path = MathildaUtils::extract_path_from_uri(p);

		if((MathildaUtils::link_blacklist(start_path)) == true) {
			continue;
		}

		Instruction *i = new Instruction((char *) h.c_str(), (char *) start_path.c_str());
		i->after = std::bind(&Spider::spider_after, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		i->port = port;

		if(i->port == 443) {
			i->ssl = true;
		}

		if(cookie_file.size() != 0)
			i->cookie_file = cookie_file;

		m->add_instruction(i);
	}

	m->use_shm = false;
	m->safe_to_fork = false;
	m->finish = std::bind(&Spider::spider_finish, this, std::placeholders::_1);
	m->execute_instructions();
	delete m;
	m = NULL;

	posts.clear();

	spider_links.insert(spider_links.end(), links.begin(), links.end());
	MathildaUtils::unique_string_vector(spider_links);

	if(spider_links.size() == 0) {
		return;
	}

	m = new Mathilda();

	for(auto const &sl : spider_links) {
		std::string link;
		link.assign(sl);

		if(link.find("POST|") != std::string::npos) {
			link = link.substr(5, link.size());
		}

		if((MathildaUtils::link_blacklist(link)) == true) {
			continue;
		}

		Instruction *i = new Instruction((char *) h.c_str(), (char *) "");

		if(MathildaUtils::is_http_uri(link) || MathildaUtils::is_https_uri(link) || (link.find(i->host)) != std::string::npos) {
			std::string link_host = MathildaUtils::extract_host_from_uri(link.c_str());

			if((MathildaUtils::is_domain_host(domain, link_host)) == false) {
				delete i;
				continue;
			}

			if(link_host.size() == 0) {
				delete i;
				continue;
			}

			if(host != link_host) {
				delete i;
				continue;
			}

			i->host = link_host;
			std::string l = MathildaUtils::extract_path_from_uri(link);
			i->path = l;
		} else {
			i->path = link;
		}

		i->after = std::bind(&Spider::spider_after, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		i->port = port;

		if(i->port == 443) {
			i->ssl = true;
		}

		if(cookie_file.size() != 0) {
			i->cookie_file = cookie_file;
		}

		m->add_instruction(i);
	}

	m->use_shm = true;
	m->safe_to_fork = true;
	m->finish = std::bind(&Spider::spider_finish, this, std::placeholders::_1);
	m->execute_instructions();
	delete m;
	m = NULL;

	MathildaUtils::unique_string_vector(links);
}