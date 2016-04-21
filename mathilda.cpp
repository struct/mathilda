// Copyright 2015/2016 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf
// Mathilda - (Beta) A library for making web tools that scale

// Doxygen mainpage
/*! \mainpage Mathilda 
 *
 * \section intro_sec Introduction
 *
 * Please see the <a href="md_README.html">README</a> for more information!
 */

#include "mathilda.h"
#include "mathilda_utils.h"

void Mathilda::add_instruction(Instruction *i) {
	i->mathilda = this;
	instructions.push_back(i);
}

void Mathilda::clear_instructions() {
	instructions.clear();
}

int Mathilda::execute_instructions() {
	if((curl_global_init(CURL_GLOBAL_ALL)) != CURLE_OK) {
		return ERR;
	}

	return create_worker_processes();
}

int Mathilda::get_shm_id() {
	return shm_id;
}

uint8_t *Mathilda::get_shm_ptr() {
	return (uint8_t *) this->shm_ptr;
}

void Mathilda::wait_loop() {
	WaitResult wr;
	memset(&wr, 0x0, sizeof(WaitResult));

	int r = 0;

	while((r = mf->wait(&wr))) {
		// Check for any errors and break
		if(r == ERR && wr.pid == ERR) {
			break;
		}

		// The process exited normally or we simply
		// returned because it received a SIGALRM.
		// Execute the finish callback and collect
		// any data from shared memory. All other
		// signals are ignored for now
		if(wr.return_code == OK || wr.signal == SIGALRM) {
			ProcessInfo *s = mf->process_info_pid(wr.pid);

			if(s == NULL) {
				finish(NULL);
#ifdef DEBUG
	fprintf(stdout, "[Mathilda] ProcessInfo for pid %d was NULL\n", wr.pid);
#endif
				continue;
			}

			if(finish) {
				finish(s);
			}

			mf->remove_child_pid(s->pid);
		}
	}
}

int Mathilda::create_worker_processes() {
	if(instructions.empty()) {
		return ERR;
	}

	uint32_t num_cores = mf->cores;

	if(instructions.size() < num_cores) {
		num_cores = instructions.size()-1;
	}

	pid_t p = 0;

	if((safe_to_fork == true || getenv("MATHILDA_FORK")) && slow_parallel == false) {
		for(uint32_t proc_num = 0; proc_num <= num_cores; proc_num++) {
			p = mf->fork_child(false, use_shm, shm_sz, timeout_seconds);

			if(p == ERR) {
#ifdef DEBUG
				fprintf(stdout, "[Mathilda] Failed to fork!\n");
#endif
			} else if(mf->parent == false) {
				if(use_shm) {
					this->shm_ptr = mf->get_shm_ptr();
					this->shm_id = mf->get_shm_id();
				}

				// Child process
				uint32_t start = 0;
				uint32_t end = 0;
				uint32_t sz_of_work = instructions.size();

				if(num_cores > 0) {
					sz_of_work = instructions.size() / num_cores;
				}

				start = sz_of_work * proc_num;
				end = start + sz_of_work;

				if(set_cpu == true) {
					mf->set_affinity(proc_num);
				}

				mathilda_proc_init(proc_num, start, end, sz_of_work);
				exit(OK);
			}
		}

		wait_loop();
	} else if(slow_parallel == true && safe_to_fork == true) {
		// This mode means only 1 Instruction per core
		// which translates to a fork() per Instruction.
		// Its slower but more reliable because you don't
		// lose Instructions if the child receives SIGALRM.
		// If this mode is used then the timeout should
		// be relatively short
		uint32_t count = 0;

		slow:
		for(uint32_t proc_num = 0; proc_num <= num_cores; proc_num++) {
			count+=1;

			p = mf->fork_child(false, use_shm, shm_sz, timeout_seconds);

			if(p == ERR) {
#ifdef DEBUG
				fprintf(stdout, "[Mathilda] Failed to fork!\n");
#endif
			} else if(mf->parent == false) {
				if(use_shm) {
					this->shm_ptr = mf->get_shm_ptr();
					this->shm_id = mf->get_shm_id();
				}

				if(set_cpu == true) {
					mf->set_affinity(proc_num);
				}

				Instruction *i = instructions[count];
				instructions.clear();
				instructions.push_back(i);
				mathilda_proc_init(0, 0, 1, 1);
				exit(OK);
			}
		}

		wait_loop();

		if(count <= instructions.size()) {
			goto slow;
		}
	} else {
		// No children were forked
		mathilda_proc_init(0, 0, instructions.size(), instructions.size());

		if(finish) {
			finish(NULL);
		}
	}

	return OK;
}

void check_multi_info(Mathilda *m) {
	int32_t msgs_left;
	uint32_t response_code;
	CURLMsg *msg = NULL;
	CURLcode cc = CURLE_OK;
	Instruction *i = NULL;

	while((msg = curl_multi_info_read(m->multi_handle, &msgs_left))) {
		if(msg->msg == CURLMSG_DONE) {
			cc = curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &i);

			if(cc != CURLE_OK || i == NULL) {
				continue;
			}

			curl_easy_getinfo(i->easy, CURLINFO_RESPONSE_CODE, &response_code);

			i->curl_code = msg->data.result;

			if((response_code == i->response_code || i->response_code == 0) && i->after) {
				i->after(i, i->easy, &i->response);
			}

			if(i->response.text) {
				free(i->response.text);
				i->response.text = NULL;
			}

			curl_multi_remove_handle(m->multi_handle, i->easy);
			curl_easy_reset(i->easy);
			m->easy_handles.push_back(i->easy);
			delete i;
			i = NULL;
		}
	}
}

void on_timeout(uv_timer_t *req) {
	int running_handles;
	Mathilda *m = (Mathilda *) req->data;
	curl_multi_socket_action(m->multi_handle, CURL_SOCKET_TIMEOUT, 0, &running_handles);
	check_multi_info(m);
}

void start_timeout(CURLM *multi, uint64_t timeout_ms, void *userp) {
	if(timeout_ms <= 0) {
		timeout_ms = 1;
	}

	Mathilda *m = (Mathilda *) userp;
	m->timeout.data = m;

	uv_timer_start(&m->timeout, (uv_timer_cb) on_timeout, timeout_ms, 0);
}

void curl_close_cb(uv_handle_t *handle) {
	Socket_Info *si = (Socket_Info *) handle->data;
	delete si;
	si = NULL;
}

int handle_socket(CURL *easy, curl_socket_t s, int action, void *userp, void *socketp) {
	Socket_Info *si = 0;

	Mathilda *m = (Mathilda *) userp;

	if(action == CURL_POLL_IN || action == CURL_POLL_OUT || action == CURL_POLL_INOUT) {
		if(socketp) {
			si = (Socket_Info *) socketp;
		} else {
			si = new Socket_Info(s, m);
			curl_multi_assign(m->multi_handle, s, (void *) si);
		}
	}

	switch(action) {
		case CURL_POLL_IN:
			uv_poll_start(&si->poll_handle, UV_READABLE, curl_perform);
		break;
		case CURL_POLL_OUT:
			uv_poll_start(&si->poll_handle, UV_WRITABLE, curl_perform);
		break;
		case CURL_POLL_INOUT:
			uv_poll_start(&si->poll_handle, UV_READABLE | UV_WRITABLE, curl_perform);
		break;
		case CURL_POLL_REMOVE:
			if(socketp) {
	 	    	si = (Socket_Info *) socketp;
				uv_poll_stop(&si->poll_handle);
				uv_close((uv_handle_t *) &si->poll_handle, curl_close_cb);
				curl_multi_assign(m->multi_handle, s, NULL);
			}
		break;
		default:
			fprintf(stderr, "[Mathilda (%d)] Unknown libcurl action (%d). Aborting!\n", m->proc_num, action);
			abort();
	}

	return 0;
}

void Mathilda::setup_uv() {
	if(loop == NULL) {
		loop = uv_default_loop();
		uv_timer_init(loop, &timeout);
	}
}

void Mathilda::mathilda_proc_init(uint32_t proc_num, uint32_t start, uint32_t end, uint32_t sz_of_work) {
	if(instructions.empty()) {
		return;
	}

	this->proc_num = proc_num;

	if(end > instructions.size()) {
		end = instructions.size();
	}

	setup_uv();

	if(multi_handle == NULL) {
		multi_handle = curl_multi_init();

		if(multi_handle == NULL) {
			fprintf(stdout, "[Mathilda (%d)] Failed to initialize curl multi handle\n", proc_num);
			return;
		}
	}

	curl_multi_setopt(multi_handle, CURLMOPT_SOCKETFUNCTION, handle_socket);
	curl_multi_setopt(multi_handle, CURLMOPT_SOCKETDATA, this);
	curl_multi_setopt(multi_handle, CURLMOPT_TIMERFUNCTION, start_timeout);
	curl_multi_setopt(multi_handle, CURLMOPT_TIMERDATA, this);

	std::vector<Instruction *>::const_iterator it;

	for(it = instructions.begin()+start; it != instructions.begin()+end; ++it) {

		if(it >= instructions.end()) {
			return;
		}

		Instruction *i = *it;
		CURLMcode res;

		if(i == NULL) {
			return;
		}

		std::string uri = i->ssl ? "https://" : "http://";

		if(i->path[0] != '/') {
			i->path = "/" + i->path;
		}

		if(i->path.substr(0,2) == "//") {
			i->path = i->path.substr(1, i->path.size());
		}

#ifdef DEBUG
		fprintf(stdout, "[Mathilda (%d)] uri=[%s] host=[%s] path=[%s]\n", proc_num, uri.c_str(), i->host.c_str(), i->path.c_str());
#endif

		auto url = uri + i->host + i->path;

		if(easy_handles.size() == 0) {
			i->easy = curl_easy_init();
		} else {
			i->easy = easy_handles.back();
			easy_handles.pop_back();
		}

		curl_easy_setopt(i->easy, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
		curl_easy_setopt(i->easy, CURLOPT_CUSTOMREQUEST, i->http_method.c_str());

		if(i->http_method == "GET") {
			curl_easy_setopt(i->easy, CURLOPT_HTTPGET, 1);
		}

		if(i->http_method == "POST") {
			curl_easy_setopt(i->easy, CURLOPT_POST, 1);
			curl_easy_setopt(i->easy, CURLOPT_POSTFIELDS, i->post_body.c_str());
		}

		if(i->http_method == "HEAD") {
			 curl_easy_setopt(i->easy, CURLOPT_NOBODY, 1);
		}

		// There are some internal libcurl bugs that
		// lead to crashes when setting this option.
		// You can still set Instruction::http_method to 'PUT'
		//if(i->http_method == "PUT") {
		//	 curl_easy_setopt(i->easy, CURLOPT_UPLOAD, 1);
		//}

		if(i->follow_redirects) {
			curl_easy_setopt(i->easy, CURLOPT_FOLLOWLOCATION, 1);
		}

		if(i->cookie_file != "") {
			curl_easy_setopt(i->easy, CURLOPT_COOKIEFILE, i->cookie_file.c_str());
		}

		if(i->verbose == true) {
			curl_easy_setopt(i->easy, CURLOPT_VERBOSE, 1L);
		}

		i->response.text = NULL;
		i->response.size = 0;

		if(i->use_proxy == true) {
			curl_easy_setopt(i->easy, CURLOPT_PROXY, i->proxy.c_str());
			curl_easy_setopt(i->easy, CURLOPT_PROXYPORT, i->proxy_port);
		}

		curl_easy_setopt(i->easy, CURLOPT_NOSIGNAL, 1); // do not remove
		curl_easy_setopt(i->easy, CURLOPT_DNS_CACHE_TIMEOUT, 1);

		if(i->include_headers == true) {
			curl_easy_setopt(i->easy, CURLOPT_HEADER, 1);
		}

		curl_easy_setopt(i->easy, CURLOPT_WRITEFUNCTION, _curl_write_callback);
		curl_easy_setopt(i->easy, CURLOPT_WRITEDATA, &i->response);
		curl_easy_setopt(i->easy, CURLOPT_USERAGENT, default_ua);
		curl_easy_setopt(i->easy, CURLOPT_URL, url.c_str());
		curl_easy_setopt(i->easy, CURLOPT_PORT, i->port);
		curl_easy_setopt(i->easy, CURLOPT_SSL_VERIFYPEER, false);
		curl_easy_setopt(i->easy, CURLOPT_PRIVATE, i);

		if(i->before) {
		    i->before(i, i->easy);
		}

#ifdef DEBUG
		fprintf(stdout, "[Mathilda (%d)(%d)] Making HTTP %s request to %s\n", getpid(), proc_num, i->http_method.c_str(), url.c_str());

		if(i->use_proxy == true) {
			fprintf(stdout, "[Mathilda (%d)(%d)] Using proxy %s on port %d\n", getpid(), proc_num, i->proxy.c_str(), i->proxy_port);
		}
#endif
		res = curl_multi_add_handle(multi_handle, i->easy);

		if(res != CURLM_OK) {
			// TODO: handle other errors
		}
	}

	uv_run(loop, UV_RUN_DEFAULT);
}

static size_t _curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	struct Response *mem = (struct Response *)userp;

	if(mem == NULL) {
		return ERR;
	}

	mem->text = (char *) realloc(mem->text, mem->size + realsize + 1);

	if(mem->text == NULL) {
		return ERR;
	}

	memcpy(&(mem->text[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->text[mem->size] = 0;

	return realsize;
}

void curl_perform(uv_poll_t *si, int status, int events) {
	int running_handles;
	int flags = 0;

	Socket_Info *s = (Socket_Info *) si->data;
	Mathilda *m = (Mathilda *) s->m;

	uv_timer_stop(&m->timeout);

	if(status < 0) {
		flags = CURL_CSELECT_ERR;
	}

	if(!status && events & UV_READABLE) {
		flags |= CURL_CSELECT_IN;
	}

	if(!status && events & UV_WRITABLE) {
		flags |= CURL_CSELECT_OUT;
	}

	curl_multi_socket_action(m->multi_handle, s->sock_fd, flags, &running_handles);
	check_multi_info(m);

	uv_timer_start(&m->timeout, (uv_timer_cb) on_timeout, 0, 0);
}

// Adds an HTTP header to a Curl easy handle
//
// This function takes a std::string and appends
// its contents to the linked list of HTTP headers
// that libcurl will add to every request for this
// particular Easy handle
//
// @param[in] header A std::string containing the header
// @return Returns OK if successful, ERR if not
int Instruction::add_http_header(const std::string &header) {
	http_header_slist = curl_slist_append(http_header_slist, header.c_str());

	if(http_header_slist == NULL) {
		return ERR;
	}

	curl_easy_setopt(easy, CURLOPT_HTTPHEADER, http_header_slist);

	return OK;
}

// Sets the user agent for this Instruction
//
// @param[in] ua A std::string containing the desired user agent
// @returns Returns OK if successfully set, ERR if not
int Instruction::set_user_agent(const std::string &ua) {
	if(curl_easy_setopt(easy, CURLOPT_USERAGENT, (char *) ua.c_str()) == CURLE_OK) {
		return OK;
	} else {
		return ERR;
	}
}

#ifdef MATHILDA_TESTING

#ifdef SPIDER
#include "spider.h"
#endif

#ifdef DIRBUSTER
#include "dirbuster.h"
#endif

void my_before(Instruction *i, CURL *c) {
	//curl_easy_setopt(c, CURLOPT_USERAGENT, "your user agent");
}

void my_after(Instruction *i, CURL *c, Response *r) {
	//fprintf(stdout, "Response: %s\n%s", i->host.c_str(), r->text);
}

void my_finish(ProcessInfo *sm) {
	//fprintf(stdout, "Shared memory @ %p", sm);
}

int main(int argc, char *argv[]) {
	bool ret = 0;
	int iret = 0;

	std::vector<std::string> out;
	iret = MathildaUtils::name_to_addr("www.yahoo.com", out, false);

	for(auto j : out) {
		fprintf(stdout, "name_to_addr(www.yahoo.com) = %d %s\n", iret, j.c_str());
	}

	iret = MathildaUtils::name_to_addr("aaaaaaa.test.com", out, true);
	fprintf(stdout, "name_to_addr(aaaaaaa.test.com) = %d\n", iret);

	out.clear();

	std::vector<std::string> hostnames;
	hostnames.push_back("www.yahoo.com");
	hostnames.push_back("answers.yahoo.com");
	hostnames.push_back("finance.yahoo.com");
	hostnames.push_back("mail.yahoo.com");
	hostnames.push_back("messenger.yahoo.com");
	hostnames.push_back("github.com");
	hostnames.push_back("facebook.com");
	hostnames.push_back("google.com");
	hostnames.push_back("facebook.com");
	hostnames.push_back("twitter.com");
	hostnames.push_back("abc.com");
	hostnames.push_back("cbs.com");
	hostnames.push_back("nbc.com");
	hostnames.push_back("slack.com");
	hostnames.push_back("microsoft.com");
	hostnames.push_back("nfl.com");
	hostnames.push_back("mail.microsoft.com");
	hostnames.push_back("mail.google.com");
	hostnames.push_back("gmail.com");
	hostnames.push_back("lastpass.com");
	hostnames.push_back("github.io");
	hostnames.push_back("mongodb.com");
	MathildaUtils::name_to_addr_a(hostnames, out);

	fprintf(stdout, "%zu/%zu results for async reverse DNS lookup:\n", out.size(), hostnames.size());

	for(auto o : out) {
		cout << o << endl;
	}

	std::string ip = "8.8.8.8";
	std::string out_r;

	iret = MathildaUtils::addr_to_name(ip, out_r);
	fprintf(stdout, "addr_to_name(8.8.8.8) = %s %d\n", out_r.c_str(), iret);

	std::vector<std::string> ips_v;
	ips_v.push_back("8.8.8.8");
	ips_v.push_back("4.4.4.4");
	ips_v.push_back("6.6.6.6");
	ips_v.push_back("127.0.0.1");
	std::vector<std::string> ips_v_out;
	MathildaUtils::addr_to_name_a(ips_v, ips_v_out);

	fprintf(stdout, "%zu/%zu results for async DNS lookup:\n", ips_v_out.size(), ips_v.size());

	for(auto v : ips_v_out) {
		fprintf(stdout, "%s\n", v.c_str());
	}

	const char *HTTP = "200 OK\r\nX-Test: 1abc\r\nX-Hdr: abc2\r\nX-Server: 3xyz\r\n\r\ndatas";
	std::map<std::string, std::string> hdrs;
	MathildaUtils::get_http_headers(HTTP, hdrs);

	fprintf(stdout, "HTTP Headers from:\n%s\n", HTTP);

	for(auto const &h : hdrs) {
		fprintf(stdout, "[%s] -> [%s]\n", h.first.c_str(), h.second.c_str());
	}

	auto url = MathildaUtils::normalize_uri("http://www.yahoo.com/test//dir//a");
	fprintf(stdout, "normalize_uri(http://www.yahoo.com/test//dir//a) = %s\n", url.c_str());

	auto host = MathildaUtils::extract_host_from_uri("http://example.test.example.com/something/");
	fprintf(stdout, "extract_host_from_uri(http://example.test.example.com/something) = %s\n", host.c_str());

	auto page = MathildaUtils::extract_path_from_uri("http://example.test.example.com/something/test.php");
	fprintf(stdout, "extract_path_from_uri(http://example.test.example.com/something/test.php) = %s\n", page.c_str());

	ret = MathildaUtils::is_domain_host(".example.com", "/example");
	fprintf(stdout, "is_domain_host(example.com, /example) | ret = %d\n", ret);

	ret = MathildaUtils::is_domain_host(".example.com", "http://www.example.com");
	fprintf(stdout, "is_domain_host(http://www.example.com, http://www.example.com) | ret = %d\n", ret);

	ret = MathildaUtils::is_domain_host(".example.com", "http://sports.exampleaaa.com");
	fprintf(stdout, "is_domain_host(.example.com, http://sports.exampleaaa.com) | ret = %d\n", ret);

	ret = MathildaUtils::is_domain_host(".example.com", "http://example.test.something.com/index.html");
	fprintf(stdout, "is_domain_host(.example.com, http://example.test.something.com/index.html) | ret = %d\n", ret);

	ret = MathildaUtils::is_subdomain("a.b.example.com");
	fprintf(stdout, "is_subdomain(a.b.example.com) | ret = %d\n", ret);

	ret = MathildaUtils::is_subdomain("www.example.com");
	fprintf(stdout, "is_subdomain(www.example.com) | ret = %d\n", ret);

	ret = MathildaUtils::is_http_uri("http://sports.example.com");
	fprintf(stdout, "is_http_uri('http://sports.example.com') | ret = %d\n", ret);

	ret = MathildaUtils::is_http_uri("https://www.example.com");
	fprintf(stdout, "is_http_uri('https://www.example.com') | ret = %d\n", ret);

	ret = MathildaUtils::is_https_uri("https://www.example.com");
	fprintf(stdout, "is_http_uri('https://www.example.com') | ret = %d\n", ret);

	ret = MathildaUtils::is_https_uri("http://www.example.com");
	fprintf(stdout, "is_http_uri('http://www.example.com') | ret = %d\n", ret);

	ret = MathildaUtils::link_blacklist("http://www.example.com");
	fprintf(stdout, "link_blacklist('http://www.example.com') | ret = %d\n", ret);

	ret = MathildaUtils::link_blacklist("http://mail.example2.com");
	fprintf(stdout, "link_blacklist('http://mail.example2.com') | ret = %d\n", ret);

#ifdef SPIDER
	cout << "Spidering..." << endl;
	std::vector<std::string> paths;
	paths.push_back("/index.php");
	auto hh = "your-example-host.com";
	auto d = "your-example-host.com";
	auto cookie_file = "";
	Spider *s = new Spider(paths, hh, d, cookie_file, 80);
    s->run(3);

    for(auto x : s->links) {
        cout << "Discovered Link: " << x.c_str() << endl;
    }

    delete s;
#endif

#ifdef DIRBUSTER
    cout << "Dirbuster..." << endl;

	std::vector <std::string> pages;
	std::vector <std::string> dirs;
	const char *p = "pages.txt";
	const char *d = "dirs.txt";

	MathildaUtils::read_file((char *) p, pages);
	MathildaUtils::read_file((char *) d, dirs);

	std::vector<std::string> hh;
	hh.push_back("your-example-host.com");
	auto cookie_file = "";

    Dirbuster *dirb = new Dirbuster(hh, pages, dirs, cookie_file, 80);
    dirb->run();

	fprintf(stdout, "Dirbuster results:\n");

	for(auto pt : dirb->paths) {
		fprintf(stdout, "%s\n", pt.c_str());
	}

    delete dirb;
#endif

#ifdef LOAD_TEST
	unique_ptr<Mathilda> m(new Mathilda());
	m->finish = my_finish;

	for(int j = 0; j < 20000; j++) {
		Instruction *i = new Instruction((char *) "example.test.example.com", (char *) "/test.html");
		i->before = my_before;
		i->after = my_after;
		m->add_instruction(i);
	}

	m->safe_to_fork = true;
	m->use_shm = false;
	m->execute_instructions();
	m->clear_instructions();
#endif

	return OK;
}
#endif
