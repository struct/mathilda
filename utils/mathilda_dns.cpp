/// Copyright 2015/2016 Yahoo Inc.
/// Licensed under the BSD license, see LICENSE file for terms.
/// Written by Chris Rohlf

#include "mathilda.h"
#include "mathilda_utils.h"
#include "mathilda_dns.h"

/// Flushes the Mathilda global DNS cache
void MathildaDNS::flush_cache() {
	dns_cache.clear();
}

/// Disables the cache but does not flush it
void MathildaDNS::disable_cache() {
	use_cache = false;
}

/// Enables the cache but does not flush it
void MathildaDNS::enable_cache() {
	use_cache = true;
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
int MathildaDNS::name_to_addr(std::string const &host, std::vector<std::string> &results, bool fast) {
	char buf[INET6_ADDRSTRLEN];
	struct addrinfo *result = NULL, *res = NULL;
    struct addrinfo hints;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	int ret;

#ifdef DEBUG
	fprintf(stdout, "[MathildaDNS] Performing reverse DNS request for %s\n", host.c_str());
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

		if(use_cache == true) {
			dns_cache.insert(std::pair<std::string, std::string>(buf, host));
		}

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
void MathildaDNS::name_to_addr_a(std::vector<std::string> const &hostnames, std::vector<std::string> &results) {
	MathildaFork *mf = new MathildaFork();

	uint32_t num_cores = mf->cores;

	if(hostnames.size() < num_cores) {
		num_cores = hostnames.size()-1;
	}

	for(uint32_t proc_num = 0; proc_num < num_cores; proc_num++) {
		int p = mf->fork_child(true, true, SHM_SIZE, 60);

		if(p == ERR) {
	#ifdef DEBUG
			fprintf(stdout, "[MathildaDNS] Failed to fork!\n");
	#endif
		} else if(mf->parent == false) {
			uint32_t start = 0;
			uint32_t sz_of_work	= hostnames.size();

			if(num_cores > 0) {
				sz_of_work = hostnames.size() / num_cores;
			}

			start = sz_of_work * proc_num;
			uint32_t end = start + sz_of_work;

			if(end > hostnames.size()) {
				end = hostnames.size();
			}

			auto it = hostnames.begin();

			for(it = hostnames.begin()+start; it != hostnames.begin()+end; ++it) {
				std::vector<std::string> r;
				/// Sane timeout for our DNS lookups
				/// SIGALRM is caught by MathildaFork
				alarm(30);

				int ret = 0;

				if(use_cache == true) {
					disable_cache();
					ret = name_to_addr(*it, r, false);
					enable_cache();
				} else {
					ret = name_to_addr(*it, r, false);
				}

				if(ret == ERR) {
					continue;
				}

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
		if(r == ERR && wr.pid == ERR) {
			break;
		}

		if(wr.return_code == OK || wr.signal == SIGALRM) {
			ProcessInfo *s = mf->process_info_pid(wr.pid);

			if(s == NULL) {
#ifdef DEBUG
	fprintf(stdout, "[MathildaDNS] ProcessInfo for pid %d was NULL\n", wr.pid);
#endif			
				continue;
			}

			mf->shm_retrieve_strings(s, results);

			if(use_cache == true) {
				for(auto &s : results) {
					std::vector<std::string> tmp;
					MathildaUtils::split(s, ',', tmp);
					for(uint32_t z = 1; z < tmp.size(); z++) {
						dns_cache.insert(std::pair<std::string, std::string>(tmp[z], tmp[0]));
					}
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
int MathildaDNS::addr_to_name(std::string const &ip, std::string &result) {
	char host[NI_MAXHOST];
	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	inet_pton(AF_INET, ip.c_str(), &sa.sin_addr);

#ifdef DEBUG
	fprintf(stdout, "[MathildaDNS] Performing DNS request for %s\n", ip.c_str());
#endif

	int res = getnameinfo((struct sockaddr*)&sa, sizeof(sa), host, sizeof(host), NULL, 0, 0);

	if(res == 0) {
		result = host;

		if(use_cache == true) {
			dns_cache.insert(std::pair<std::string, std::string>(ip, result));
		}

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
void MathildaDNS::addr_to_name_a(std::vector<std::string> const &ips, std::vector<std::string> &results) {
	MathildaFork *mf = new MathildaFork();

	uint32_t num_cores = mf->cores;

	if(ips.size() < num_cores) {
		num_cores = ips.size()-1;
	}

	for(uint32_t proc_num = 0; proc_num < num_cores; proc_num++) {
		int p = mf->fork_child(true, true, SHM_SIZE, 60);

		if(p == ERR) {
	#ifdef DEBUG
			fprintf(stdout, "[MathildaDNS] Failed to fork!\n");
	#endif
		} else if(mf->parent == false) {
			uint32_t start = 0;
			uint32_t sz_of_work	= ips.size();

			if(num_cores > 0) {
				sz_of_work = ips.size() / num_cores;
			}

			start = sz_of_work * proc_num;
			uint32_t end = start + sz_of_work;

			if(end > ips.size()) {
				end = ips.size();
			}

			auto it = ips.begin();

			for(it = ips.begin()+start; it != ips.begin()+end; ++it) {
				std::string r;
				/// Sane timeout for our DNS lookups
				/// SIGALRM is caught by our fork class
				alarm(30);

				int ret = 0;

				if(use_cache == true) {
					disable_cache();
					ret = MathildaDNS::addr_to_name(*it, r);
					enable_cache();
				} else {
					ret = MathildaDNS::addr_to_name(*it, r);
				}

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
		if(r == ERR && wr.pid == ERR) {
			break;
		}

		if(wr.return_code == OK || wr.signal == SIGALRM) {
			ProcessInfo *s = mf->process_info_pid(wr.pid);

			if(s == NULL) {
#ifdef DEBUG
	fprintf(stdout, "[MathildaDNS] ProcessInfo for pid %d was NULL\n", wr.pid);
#endif			
				continue;
			}

			mf->shm_retrieve_strings(s, results);

			if(use_cache == true) {
				for(auto &s : results) {
					std::vector<std::string> tmp;
					MathildaUtils::split(s, ',', tmp);
					for(uint32_t z = 1; z < tmp.size(); z++) {
						dns_cache.insert(std::pair<std::string, std::string>(tmp[z], tmp[0]));
					}
				}
			}

			mf->remove_child_pid(s->pid);
		}
	}

	delete mf;
}