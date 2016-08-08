/// Copyright 2015/2016 Yahoo Inc.
/// Licensed under the BSD license, see LICENSE file for terms.
/// Written by Chris Rohlf

#include "mathilda.h"
#include "mathilda_utils.h"

/// Checks if a domain is a subdomain
///
/// This works by first calling extract_host_from_uri
/// and then checking the host to see if it would be
/// a subdomain or a tld
///
/// @param[in] uri A url, hostname or other string
/// @return true if uri is a subdomain, false if its not
bool MathildaUtils::is_subdomain(std::string const &uri) {
	std::string domain = MathildaUtils::extract_host_from_uri(uri);

	if((std::count(domain.begin(), domain.end(), '.')) < 3) {
		return true;
	}

	return false;
}

/// Compares a link against a known blacklist of links
///
/// Takes a link and compares it against a hardcoded blacklist
/// This is useful for when you don't want to make requests
/// to pages with endless redirects or bad URIs
///
/// @param[in] link A url, hostname or other string
/// @return true if blacklisted, false if not
bool MathildaUtils::link_blacklist(std::string const &link) {
	if(link[0] == '#') {
		return true;
	}

	const char *blacklist [] = { "example.com", "javascript:" };

	for(size_t i=0; i < (sizeof(blacklist) / sizeof(char *)); i++) {
		if(link.find(blacklist[i]) != string::npos) {
			return true;
		}
	}

	return false;
}

/// Compares a page against a known blacklist of page contents
///
/// Takes a blob of text from a webpage and checks for known
/// blacklisted contents. Useful for 404 pages.
///
/// @param[in] text A blob of text, usually from an HTML page
/// @return true if blacklisted, false if not
bool MathildaUtils::page_blacklist(std::string const &text) {
	if((text.find("Sorry, the page you requested was not found")) != string::npos
		|| (text.find("The requested URL was not found on this server")) != string::npos
		|| (text.find("<h2 align=\"center\">File does not exist.</h2>")) != string::npos) {
		return true;
	} else {
		return false;
	}
}

/// Returns true if your string is an http URI
///
/// Takes a string and determines if it starts with
/// the http:// URI. This is a simple helper function
/// for quickly determining whether to ignore a link
///
/// @param[in] uri A string, usually a URI
/// @return true if uri starts with http://, false if it doesn't
bool MathildaUtils::is_http_uri(std::string const &uri) {
	if(uri.substr(0, 7) == "http://") {
		return true;
	} else {
		return false;
	}
}

/// Returns true if your string is an https URI
///
/// Takes a string and determines if it starts with
/// the https:/// URI. This is a simple helper function
/// for quickly determining whether to ignore a link
///
/// @param[in] uri A string, usually a link from an HTML page
/// @return true if uri starts with https:///, false if it doesn't
bool MathildaUtils::is_https_uri(std::string const &uri) {
	if(uri.substr(0, 8) == "https://") {
		return true;
	} else {
		return false;
	}
}

/// Checks if a link will make request to domain
///
/// This function is useful for when you have a link
/// to a potentially new endpoint but want to constrain
/// all operations to a specific TLD
///
/// @param[in] domain A TLD you want to check for
/// @param[in] uri A string or a link usually from an HTML page
/// @return true if the uri goes to that domain, false if it doesn't
bool MathildaUtils::is_domain_host(std::string const &domain, std::string const &uri) {
	std::string d = MathildaUtils::extract_host_from_uri(uri);

	if(d.find(domain) != string::npos) {
		return true;
	} else {
		return false;
	}
}

/// Extracts the hostname from a URI string
///
/// This function extracts a hostname from a URL. Its used
/// heavily internally to the library to perform functions
/// like is_domain_host. Its helpful in situations where
/// you want to build new URI's from existing ones
///
/// @param[in] uri A string or a link usually from an HTML page
/// @return Returns a std::string which holds the extracted host
std::string MathildaUtils::extract_host_from_uri(std::string const &uri) {
	std::string host;

	if((MathildaUtils::is_http_uri(uri)) == true) {
		std::string t = uri.substr(strlen("http://"), uri.size());
		size_t e = t.find('/');

		if(e == string::npos) {
			e = t.size();
		}

		host = t.substr(0, e);
	} else if((MathildaUtils::is_https_uri(uri)) == true) {
		std::string t = uri.substr(strlen("https://"), uri.size());
		size_t e = t.find('/');

		if(e == string::npos) {
			e = t.size();
		}

		host = t.substr(0, e);
	} else {
		host = uri;
	}

	if(host.find("/") != string::npos) {
		size_t e = host.find('/');

		if(e == string::npos) {
			e = host.size();
		}

		return host.substr(0, e);
	} else {
		return host;
	}
}

/// Extracts the query path from a URI
///
/// This function makes extracting query paths from
/// arbitrary URI's much easier. Its useful when you
/// are scraping an HTML page for links.
///
/// @param[in] uri A string or a link usually from an HTML page
/// @returns Returns a std::string which holds the extracted query path
std::string MathildaUtils::extract_path_from_uri(std::string const &uri) {
	std::string t(uri);

	if((MathildaUtils::is_http_uri(uri)) == true) {
		t = uri.substr(strlen("http://"), uri.size());
	}

	if((MathildaUtils::is_https_uri(uri)) == true) {
		t = uri.substr(strlen("https://"), uri.size());
	}

	size_t e = t.find('/');

	if(e == string::npos) {
		return "";
	}

	return t.substr(e, t.size());
}

/// A wrapper for getaddrinfo
///
/// Performs a synchronous name to addr DNS lookup using
/// getaddrinfo.  This function makes it easier to know 
/// if a hostname has a valid DNS entry or not.
///
/// @param[in] host A std::string containing hostname to perform a 
///      	  DNS query for
/// @param[in,out] results A std::vector of std::string containing the
///			   the results of the DNS lookup
/// @param[in] fast A boolean indicating whether the results are
///			  needed or not. Use false if you just want to know
///			  if the hostname has a valid DNS record
/// @return Returns ERR if the hostname lookup failed, true if did not
///		   The results vector is only filled out if fast is false
int MathildaUtils::name_to_addr(std::string const &host, std::vector<std::string> &results, bool fast) {
	char buf[INET6_ADDRSTRLEN];
	struct addrinfo *result = NULL, *res = NULL;
    struct addrinfo hints;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	int ret;

#ifdef DEBUG
	fprintf(stdout, "[MathildaUtils] Performing reverse DNS request for %s\n", host.c_str());
#endif
	ret = getaddrinfo(host.c_str(), NULL, &hints, &result);

	if(ret != OK) {
		return ERR;
	}

	if(fast == true && result) {
		freeaddrinfo(result);
		return OK;
	} else if(fast == true && result == NULL) {
		return ERR;
	}

	for(res = result; res != NULL; res = res->ai_next) {
		inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, buf, sizeof(buf)-1);
		results.push_back(buf);
	}

    freeaddrinfo(result);

    return OK;
}

/// An asynchronous wrapper for getaddrinfo
///
/// Performs an asynchronous name to addr DNS lookup using
/// getaddrinfo across multiple processes.  This function makes
/// it easier to know if a hostname has a valid DNS entry or not.
/// Its written to be used against a vector of hostnames
///
/// @param[in] hostnames A std::vector containing hostnames to perform
///      	  a DNS query for
/// @param[in,out] results A std::vector of std::string containing the
///			   the results of the DNS lookups
void MathildaUtils::name_to_addr_a(std::vector<std::string> const &hostnames, std::vector<std::string> &results) {
	MathildaFork *mf = new MathildaFork();

	uint32_t num_cores = mf->cores;

	if(hostnames.size() < num_cores) {
		num_cores = hostnames.size()-1;
	}

	for(uint32_t proc_num = 0; proc_num <= num_cores; proc_num++) {
		int p = mf->fork_child(true, true, SHM_SIZE, 60);

		if(p == ERR) {
	#ifdef DEBUG
			fprintf(stdout, "[MathildaUtils] Failed to fork!\n");
	#endif
		} else if(mf->parent == false) {
			uint32_t start = 0;
			uint32_t end = 0;
			uint32_t sz_of_work	= hostnames.size();

			if(num_cores > 0) {
				sz_of_work = hostnames.size() / num_cores;
			}

			start = sz_of_work * proc_num;
			end = start + sz_of_work;

			auto it = hostnames.begin();

			for(it = hostnames.begin()+start; it != hostnames.begin()+end; ++it) {
				std::vector<std::string> r;
				/// Sane timeout for our DNS lookups
				/// SIGALRM is caught by our fork class
				alarm(30);

				MathildaUtils::name_to_addr(*it, r, false);

				std::string o = *it;

				for(auto s : r) {
					o += "," + s;
				}

				mf->shm_store_string(o.c_str());
			}

			exit(OK);
		}
	}

	WaitResult wr;
	memset(&wr, 0x0, sizeof(WaitResult));

	int r = 0;

	while((r = mf->wait(&wr))) {
		/// Check for any errors and break
		if(r == ERR && wr.pid == ERR) {
			break;
		}

		/// The process exited normally or we simply
		/// returned because it received a SIGALRM.
		/// Execute the finish callback and collect
		/// any data from shared memory. All other
		/// signals are ignored
		if(wr.return_code == OK || wr.signal == SIGALRM) {
			ProcessInfo *s = mf->process_info_pid(wr.pid);

			if(s == NULL) {
#ifdef DEBUG
	fprintf(stdout, "[MathildaUtils] ProcessInfo for pid %d was NULL\n", wr.pid);
#endif			
				continue;
			}

			if(s->shm_ptr != NULL) {
				StringEntry *head = (StringEntry *) s->shm_ptr;
				char *string;

				while(head->length != 0 && head->length < 16384) {
					string = (char *) head + sizeof(StringEntry);
					results.push_back(string);
					head += head->length;
				}
			}

			mf->remove_child_pid(s->pid);
		}
	}

	delete mf;
}

/// A wrapper for getnameinfo
///
/// Performs a synchronous addr to name DNS lookup using
/// getnameinfo. This function makes it easier to know
/// if an IP address has a valid DNS entry or not. Most
/// of the time you want to make HTTP calls using the
/// hostname and not the IP address and set the Host:
/// header correctly
///
/// @param[in] ip A std::string containing the IP address
/// @param[out] result A std::string containing the result
/// @return Returns OK if successful, ERR if not
int MathildaUtils::addr_to_name(std::string const &ip, std::string &result) {
	char host[NI_MAXHOST];
	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	inet_pton(AF_INET, ip.c_str(), &sa.sin_addr);

#ifdef DEBUG
	fprintf(stdout, "[MathildaUtils] Performing DNS request for %s\n", ip.c_str());
#endif

	int res = getnameinfo((struct sockaddr*)&sa, sizeof(sa), host, sizeof(host), NULL, 0, 0);

	if(res == 0) {
		result = host;
		return OK;
	}

	return ERR;
}

/// An asynchronous wrapper for addr_to_name/getnameinfo
///
/// Performs asynchronous addr to name DNS lookup using
/// getnameinfo. This function makes it easier to know
/// if an IP address has a valid DNS entry or not. Most
/// of the time you want to make HTTP calls using the
/// hostname and not the IP address and set the Host:
/// header correctly
///
/// @param[in] ips A vector of std::string containing the IP addresses
/// @param[out] results A vector of std::string with the results
/// @return Returns OK if successful, ERR if not
void MathildaUtils::addr_to_name_a(std::vector<std::string> const &ips, std::vector<std::string> &results) {
	MathildaFork *mf = new MathildaFork();

	uint32_t num_cores = mf->cores;

	if(ips.size() < num_cores) {
		num_cores = ips.size()-1;
	}

	for(uint32_t proc_num = 0; proc_num <= num_cores; proc_num++) {
		int p = mf->fork_child(true, true, SHM_SIZE, 60);

		if(p == ERR) {
	#ifdef DEBUG
			fprintf(stdout, "[MathildaUtils] Failed to fork!\n");
	#endif
		} else if(mf->parent == false) {
			uint32_t start = 0;
			uint32_t end = 0;
			uint32_t sz_of_work	= ips.size();

			if(num_cores > 0) {
				sz_of_work = ips.size() / num_cores;
			}

			start = sz_of_work * proc_num;
			end = start + sz_of_work;

			auto it = ips.begin();

			for(it = ips.begin()+start; it != ips.begin()+end; ++it) {
				std::string r;
				/// Sane timeout for our DNS lookups
				/// SIGALRM is caught by our fork class
				alarm(30);

				int ret = MathildaUtils::addr_to_name(*it, r);

				if(ret == ERR) {
					continue;
				}

				std::string o = *it + "," + r;

				mf->shm_store_string(o.c_str());
			}

			exit(OK);
		}
	}

	WaitResult wr;
	memset(&wr, 0x0, sizeof(WaitResult));

	int r = 0;

	while((r = mf->wait(&wr))) {
		/// Check for any errors and break
		if(r == ERR && wr.pid == ERR) {
			break;
		}

		/// The process exited normally or we simply
		/// returned because it received a SIGALRM.
		/// Execute the finish callback and collect
		/// any data from shared memory. All other
		/// signals are ignored
		if(wr.return_code == OK || wr.signal == SIGALRM) {
			ProcessInfo *s = mf->process_info_pid(wr.pid);

			if(s == NULL) {
#ifdef DEBUG
	fprintf(stdout, "[MathildaUtils] ProcessInfo for pid %d was NULL\n", wr.pid);
#endif			
				continue;
			}

			if(s->shm_ptr != NULL) {
				StringEntry *head = (StringEntry *) s->shm_ptr;
				char *string;

				while(head->length != 0 && head->length < 16384) {
					string = (char *) head + sizeof(StringEntry);
					results.push_back(string);
					head += head->length;
				}
			}

			mf->remove_child_pid(s->pid);
		}
	}

	delete mf;
}

/// Performs some basic normalization of a URI
///
/// This function performs some basic normalization
/// of a URI string. Its needed because theres a lot
/// of broken HTML out there and our API's generally
/// require URI's in a specific format
///
/// @param[in,out] uri A string or a link usually from an HTML page
/// @return Returns a std::string uri thats been 'normalized'
std::string MathildaUtils::normalize_uri(std::string const &uri) {
	std::string prepend = "http://";
	std::string tmp;

	if((MathildaUtils::is_https_uri(uri)) == true) {
		prepend = "https://";
	}

	tmp = uri.substr(prepend.size(), uri.size());

    if(tmp[0] == '/') {
        tmp.erase(tmp.begin());
    }

	while((tmp.find("//")) != string::npos) {
		tmp.replace(tmp.find("//"), 2, "/");
	}

	tmp = prepend + tmp;

	return tmp;
}

/// Tokenizes and returns the headers from an HTTP response
///
/// Takes a blob of text containing HTTP headers and
/// a std::map of std::string and returns the map filled
/// out containing all the key/value header pairs
///
/// @param[in] text A char * pointer to an HTTP response payload
/// @param[in,out] hdrs A std::map of std::string that will hold the
///	           HTTP header key value pairs
void MathildaUtils::get_http_headers(const char *text, std::map<std::string, std::string> &hdrs) {
	std::stringstream ss(text);
	std::string item;

	while (std::getline(ss, item, '\n')) {
		if(item[0] == '\r') {
			break;
		}

		if(item.find(":") != string::npos) {
			hdrs[item.substr(0, item.find(":"))] = item.substr(item.find(":")+2, item.size()-item.find(":")-3);
		}
	}
}

/// Reads a file and returns its contents
///
/// Reads a file by filename and fills out a std::vector
/// of std::string with each string delimeted by newline
///
/// @param[in] fname The filename
/// @param[in,out] ret A std::vector of std::string containing
///				   the file contents
void MathildaUtils::read_file(char *fname, std::vector<std::string>& ret) {
	std::ifstream file(fname);
	std::string line;

	while(std::getline(file, line)) {
		ret.push_back(line);
	}
}

/// Removes the duplicates from a vector of strings
///
/// Takes a std::vector of std::string and removes
/// any duplicates from it in place
///
/// @param[in,out] strs A std::vector of std::string
void MathildaUtils::unique_string_vector(vector<std::string> &strs) {
	sort(strs.begin(), strs.end());
	strs.erase(unique(strs.begin(), strs.end()), strs.end());
}

/// Splits a string using a delimeter
///
/// Takes a string and a delimeter and splits it
/// up into separate strings and stores them in
/// the elems vector
///
/// @param[in] str A std::string to tokenize
/// @param[in] delim A character delimeter
/// @param[in,out] elems A std::vector of strings
void MathildaUtils::split(const std::string &str, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(str);
	std::string item;

	while (std::getline(ss, item, delim)) {
		if(!item.empty()) {
			elems.push_back(std::move(item));
		}
	}
}

/// String replacement function
///
/// Replaces all occurences of a string A within
/// string B with string C
///
/// @param[in,out] str A std::string to operate on
/// @param[in] from A std::string we want to replace in str with to
/// @param[in] to A std::string we want want to replace from with
void MathildaUtils::replaceAll(std::string& str, const std::string& from, const std::string& to) {
	size_t s;

	if(from.empty()) {
		return;
	}

	while((s = str.find(from, s)) != string::npos) {
		str.replace(s, from.length(), to);
		s += to.length();
	}
}