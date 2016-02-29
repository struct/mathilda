// Copyright 2015 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf
// Mathilda - (Beta) A library for making web tools that scale

#include "mathilda.h"

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

	create_worker_processes();

	return OK;
}

int Mathilda::get_shm_id() {
	return this->shm_id;
}

uint8_t *Mathilda::get_shm_ptr() {
	return (uint8_t *) this->shm_ptr;
}

int Mathilda::create_worker_processes() {
	uint32_t num_cores = MathildaUtils::count_cores();

	if(instructions.size() < num_cores) {
		num_cores = instructions.size()-1;
	}

	int shmid = 0;
	int status = 0;
	int signl = 0;
	int ret_code = 0;
	std::vector<Process_Info> pi;

	pid_t p = 0;

	if(safe_to_fork == true || getenv("MATHILDA_FORK")) {
		for(uint32_t proc_num = 0; proc_num < num_cores+1; proc_num++) {
			if(use_shm) {
				shmid = shmget(IPC_PRIVATE, shm_sz, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

				if(shmid == ERR) {
					fprintf(stderr, "[Mathilda] Could not allocate shared memory. Aborting!\n(%s)\n", strerror(errno));
					abort();
				}

				shm_ptr = (uint8_t *) shmat(shmid, 0, 0);

				if(shm_ptr == (void *) ERR) {
					fprintf(stderr, "[Mathilda] Could not get handle to shared memory. Aborting!\n(%s)\n", strerror(errno));
					abort();
				}

				this->shm_id = shmid;
			}

			Process_Info p_i;

			memset(&p_i, 0x0, sizeof(Process_Info));

			if(use_shm) {
				p_i.shm_id = shm_id;
				p_i.shm_ptr = shm_ptr;
			}

			p = fork();

			if(p == ERR) {
#ifdef DEBUG
				fprintf(stdout, "[Mathilda Debug] Failed to fork!\n");
#endif
			} else if(p == 0) {
				uint32_t start = 0;
				uint32_t end = 0;
				uint32_t sz_of_work = instructions.size();

				// Connect the shm in the child process
				shm_ptr = (uint8_t *) shmat(shm_id, 0, 0);

				if(num_cores > 0) {
					sz_of_work = instructions.size() / num_cores;
				}

				start = sz_of_work * proc_num;

				end = start + sz_of_work;

				if(set_cpu == true) {
					MathildaUtils::set_affinity(proc_num);
				}

				alarm(timeout_seconds);

				mathilda_proc_init(proc_num, start, end, sz_of_work);
				exit(OK);
			}

			p_i.proc_pid = p;
			pi.push_back(p_i);
		}

		while((p = waitpid(-1, &status, 0))) {
			if(errno == ECHILD) {
				break;
			}

			ret_code = WEXITSTATUS(status);

			if(WIFEXITED(status)) {
#ifdef DEBUG
				fprintf(stdout, "[Mathilda Debug] Child %d exited normally with return code %d\n", p, ret_code);
#endif
				// Child exited normally, call finish()
				if(finish) {
					for(auto x : pi) {
						if(x.proc_pid == p) {
							finish(x.shm_ptr);
						}
					}
				}
			}

			if(WIFSIGNALED(status)) {
				signl = WTERMSIG(status);
#ifdef DEBUG
				fprintf(stdout, "[Mathilda Debug] Child %d was terminated by signal %d\n", p, signl);
#endif
				if(WCOREDUMP(status)) {
#ifdef DEBUG
					fprintf(stdout, "[Mathilda Debug] Child %d core dumped\n", p);
#endif
				}

				// This process probably timed out. Use
				// kill so we don't get a zombie process
				if(signl == SIGALRM) {
#ifdef DEBUG
					fprintf(stdout, "[Mathilda Debug] Child %d likely timed out\n", p);
#endif	
				}

				// Although the child was terminated by
				// a signal there is still the chance
				// there is data in shared memory so
				// we call finish() to retrieve
				if(finish) {
					for(auto x : pi) {
						if(x.proc_pid == p) {
							finish(x.shm_ptr);
						}
					}
				}
			}

			if(WIFSTOPPED(status)) {
				signl = WSTOPSIG(status);
#ifdef DEBUG
				fprintf(stdout, "[Mathilda Debug] Child %d was stopped by signal %d\n", p, signl);
#endif
			}
		}
	} else {
		// No children were forked
		mathilda_proc_init(0, 0, instructions.size(), instructions.size());

		if(finish) {
			finish(NULL);
		}
	}

	if(use_shm) {
		for(auto s : pi) {

			int ret;

			if(s.shm_id == 0) {
				continue;
			}

			ret = shmctl(s.shm_id, IPC_RMID, NULL);

			if(ret == ERR) {
				fprintf(stderr, "[Mathilda] Failed to free shared memory (%d). Aborting!\n(%s)\n", s.shm_id, strerror(errno));
				abort();
			}

			ret = shmdt(s.shm_ptr);

			if(ret == ERR) {
				fprintf(stderr, "[Mathilda] Failed to detach shared memory. Aborting!\n(%s)\n", strerror(errno));
				abort();
			}
		}

		pi.clear();
	}

	return OK;
}

uint32_t MathildaUtils::count_cores() {
	return std::thread::hardware_concurrency();
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
	m->timeout->data = m;

	uv_timer_start(m->timeout, (uv_timer_cb) on_timeout, timeout_ms, 0);
}

void curl_close_cb(uv_handle_t *handle) {
	Socket_Info *si = (Socket_Info *) handle->data;
	delete si;
	si = NULL;
}

int handle_socket(CURL *easy, curl_socket_t s, int action, void *userp, void *socketp) {
	Socket_Info *si = 0;

	Mathilda *m = (Mathilda *) userp;

	if(action == CURL_POLL_IN || action == CURL_POLL_OUT) {
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
		case CURL_POLL_REMOVE:
			if(socketp) {
	 	    	si = (Socket_Info *) socketp;
				uv_poll_stop(&si->poll_handle);
				uv_close((uv_handle_t *) &si->poll_handle, curl_close_cb);
				curl_multi_assign(m->multi_handle, s, NULL);
			}
		break;
		default:
			fprintf(stderr, "[LibMathilda (%d)] Unknown libcurl action (%d). Aborting!\n", m->proc_num, action);
			abort();
	}

	return 0;
}

void Mathilda::setup_uv() {
	if(loop == NULL) {
		loop = uv_default_loop();
		uv_timer_init(loop, timeout);
	}
}

void Mathilda::mathilda_proc_init(uint32_t proc_num, uint32_t start, uint32_t end, uint32_t sz_of_work) {
	if(instructions.empty())
		return;

	this->proc_num = proc_num;

	if(end > instructions.size()) {
		end = instructions.size();
	}

	setup_uv();

	if(multi_handle == NULL) {
		multi_handle = curl_multi_init();

		if(multi_handle == NULL) {
			fprintf(stdout, "[LibMathilda (%d)] Failed to initialize curl multi handle\n", proc_num);
			return;
		}
	}

	curl_multi_setopt(multi_handle, CURLMOPT_SOCKETFUNCTION, handle_socket);
	curl_multi_setopt(multi_handle, CURLMOPT_SOCKETDATA, this);
	curl_multi_setopt(multi_handle, CURLMOPT_TIMERFUNCTION, start_timeout);
	curl_multi_setopt(multi_handle, CURLMOPT_TIMERDATA, this);

	std::vector<Instruction *>::const_iterator it;

	for(it = instructions.begin()+start; it != instructions.begin()+end; ++it) {
		Instruction *i = *it;
		CURLMcode res;

		std::string uri = i->ssl ? "https://" : "http://";

		if(i->path[0] != '/') {
			i->path = "/" + i->path;
		}

		if(i->path.substr(0,2) == "//") {
			i->path = i->path.substr(1, i->path.size());
		}

#ifdef DEBUG
		fprintf(stdout, "[LibMathilda (%d)] uri=[%s] host=[%s] path=[%s]\n", proc_num, uri.c_str(), i->host.c_str(), i->path.c_str());
#endif

		std::string url = uri + i->host + i->path;

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
		fprintf(stdout, "[LibMathilda (%d)] Making HTTP %s request to %s\n", proc_num, i->http_method.c_str(), url.c_str());

		if(i->use_proxy == true) {
			fprintf(stdout, "[LibMathilda (%d)] Using proxy %s on port %d\n", proc_num, i->proxy.c_str(), i->proxy_port);
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

	uv_timer_stop(m->timeout);

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

	uv_timer_start(m->timeout, (uv_timer_cb) on_timeout, 0, 0);
}

// Linux only, won't build elsewhere, but
// libmathilda is only supported on Linux
void MathildaUtils::set_affinity(uint32_t c) {
	cpu_set_t cpus;
	CPU_ZERO(&cpus);
	CPU_SET(c, &cpus);
	int ret = sched_setaffinity(0, sizeof(cpus), &cpus);

#ifdef DEBUG
	if(ret == ERR) {
		fprintf(stdout, "[LibMathilda] Failed to bind to CPU %d. Cache invalidation may occur!\n", c);
	} else {
		fprintf(stdout, "[LibMathilda] Successfully	bound to CPU %d\n", c);
	}
#endif
}

bool MathildaUtils::is_subdomain(std::string const &l) {
	if((std::count(l.begin(), l.end(), '.')) < 3) {
		return true;
	}

	return false;
}

bool MathildaUtils::link_blacklist(std::string const &l) {
	if(l[0] == '#') {
		return true;
	}

	const char *blacklist [] = { "example.com" };

	for(int i=0; i < (sizeof(blacklist) / sizeof(char *)); i++) {
		if(l.find(blacklist[i]) != ERR) {
			return true;
		}
	}

	return false;
}

bool MathildaUtils::page_blacklist(std::string const &l) {
	if((l.find("Sorry, the page you requested was not found")) != string::npos
		|| (l.find("The requested URL was not found on this server")) != string::npos
		|| (l.find("<h2 align=\"center\">File does not exist.</h2>")) != string::npos) {
		return true;
	} else {
		return false;
	}
}

bool MathildaUtils::is_http_uri(std::string const &l) {
	//std::regex e("^(?:http://)?([^/]+)(?:/?.*/?)/(.*)$");
	//return std::regex_match(link, e);

	if(l.substr(0, 7) == "http://") {
		return true;
	} else {
		return false;
	}
}

bool MathildaUtils::is_https_uri(std::string const &l) {
	//std::regex e("^(?:http://)?([^/]+)(?:/?.*/?)/(.*)$");
	//return std::regex_match(link, e);

	if(l.substr(0, 8) == "https://") {
		return true;
	} else {
		return false;
	}
}

bool MathildaUtils::is_domain_host(std::string const &domain, std::string const &l) {
	//std::regex e(".*\\.example.com.*");
	//return std::regex_match(link, e);

	std::string d = MathildaUtils::extract_host_from_url(l);

	if(d.find(domain) != string::npos) {
		return true;
	} else {
		return false;
	}
}

std::string MathildaUtils::extract_host_from_url(std::string const &l) {
	if((MathildaUtils::is_http_uri(l)) == true) {
		std::string t = l.substr(strlen("http://"), l.size());
		int e = t.find('/');
		return t.substr(0, e);
	}

	if((MathildaUtils::is_https_uri(l)) == true) {
		std::string t = l.substr(strlen("https://"), l.size());
		int e = t.find('/');
		return t.substr(0, e);
	}

	int e = l.find("/");
	return l.substr(0, e);
}

std::string MathildaUtils::extract_path_from_url(std::string const &l) {
	std::string t(l);

	if((MathildaUtils::is_http_uri(l)) == true) {
		t = l.substr(strlen("http://"), l.size());
	}

	if((MathildaUtils::is_https_uri(l)) == true) {
		t = l.substr(strlen("https://"), l.size());
	}

	int e = t.find('/');

	if(e == ERR) {
		return "";
	}

	return t.substr(e, t.size());
}

int MathildaUtils::name_to_addr(std::string const &l, std::vector<std::string> &out, bool fast) {
	char buf[INET6_ADDRSTRLEN];
	struct addrinfo *result = NULL, *res = NULL;
    struct addrinfo hints;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	int ret;

	ret = getaddrinfo(l.c_str(), NULL, &hints, &result);

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
		out.push_back(buf);
	}

    freeaddrinfo(result);

    return OK;
}

std::string MathildaUtils::normalize_url(std::string const &l) {
	std::string prepend = "http://";
	std::string tmp;

	if((MathildaUtils::is_https_uri(l)) == true) {
		prepend = "https://";
	}

	tmp = l.substr(prepend.size(), l.size());

    if(tmp[0] == '/') {
        tmp.erase(tmp.begin());
    }

	while((tmp.find("//")) != ERR) {
		tmp.replace(tmp.find("//"), 2, "/");
	}

	tmp = prepend + tmp;

	return tmp;
}

void MathildaUtils::get_http_headers(const char *s, std::map<std::string, std::string> &e) {
	std::stringstream ss(s);
	std::string item;
	char *v;

	while (std::getline(ss, item, '\n')) {
		if(item[0] == '\r') {
			break;
		}

		if(item.find(":") != ERR) {
			e[item.substr(0, item.find(":"))] = item.substr(item.find(":")+2, item.size()-item.find(":")-3);
		}
	}
}

#ifdef MATHILDA_TESTING
void my_before(Instruction *i, CURL *c) {
	//curl_easy_setopt(c, CURLOPT_USERAGENT, "your user agent");
}

void my_after(Instruction *i, CURL *c, Response *r) {
	//printf("Response: %s\n%s", i->host.c_str(), r->text);
}

void my_finish(uint8_t *sm) {
	//printf("Shared memory @ %p", sm);
}

int main(int argc, char *argv[]) {
	bool ret = 0;
	int iret = 0;

	std::vector<std::string> out;
	iret = MathildaUtils::name_to_addr("www.yahoo.com", out, false);

	for(auto j : out) {
		printf("name_to_addr(www.yahoo.com) = %d %s\n", iret, j.c_str());
	}

	const char *HTTP = "200 OK\r\nX-Test: 1abc\r\nX-Hdr: abc2\r\nX-Server: 3xyz\r\n\r\ndatas";
	std::map<std::string, std::string> hdrs;
	MathildaUtils::get_http_headers(HTTP, hdrs);

	fprintf(stdout, "HTTP Headers from:\n%s\n", HTTP);

	for(auto const &h : hdrs) {
		fprintf(stdout, "[%s] -> [%s]\n", h.first.c_str(), h.second.c_str());
	}

	std::string url = MathildaUtils::normalize_url("http://www.yahoo.com/test//dir//a");
	fprintf(stdout, "normalize_url(http://www.yahoo.com/test//dir//a) = %s\n", url.c_str());

	iret = MathildaUtils::name_to_addr("aaaaaaa.yahoo.com", out, true);
	fprintf(stdout, "name_to_addr(aaaaaaa.yahoo.com) = %d\n", iret);

	std::string host = MathildaUtils::extract_host_from_url("http://example.test.example.com/something/");
	fprintf(stdout, "extract_host_from_url(http://example.test.example.com/something) = %s\n", host.c_str());

	std::string page = MathildaUtils::extract_path_from_url("http://example.test.example.com/something/test.php");
	fprintf(stdout, "extract_path_from_url(http://example.test.example.com/something/test.php) = %s\n", page.c_str());

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

	ret = MathildaUtils::is_http_uri("http://www.example.com");
	fprintf(stdout, "is_http_uri('http://www.example.com') | ret = %d\n", ret);

	ret = MathildaUtils::link_blacklist("http://www.example.com");
	fprintf(stdout, "link_blacklist('http://www.example.com') | ret = %d\n", ret);

	ret = MathildaUtils::link_blacklist("http://mail.example2.com");
	fprintf(stdout, "link_blacklist('http://mail.example2.com') | ret = %d\n", ret);

	unique_ptr<Mathilda> m(new Mathilda());

	m->finish = my_finish;

	/*for(int j = 0; j < 20000; j++) {
		Instruction *i = new Instruction((char *) "example.test.example.com", (char *) "/test.html");
		i->before = my_before;
		i->after = my_after;
		m->add_instruction(i);
	}

	m->safe_to_fork = true;
	m->use_shm = true;
	m->execute_instructions();
	m->clear_instructions();
*/
	return OK;
}
#endif
