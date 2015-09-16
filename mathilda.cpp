// Copyright 2015 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf
// Mathilda - Beta quality class for making web requests

#include "mathilda.h"

void Mathilda::add_instruction(Instruction *i) {
	instructions.push_back(i);
}

void Mathilda::clear_instructions() {
	instructions.clear();
}

int Mathilda::execute_instructions() {
	if((curl_global_init(CURL_GLOBAL_ALL)) != CURLE_OK)
		return ERR;

	create_worker_processes();

	return OK;
}

int Mathilda::create_worker_processes() {
	uint32_t num_cores = MathildaUtils::count_cores();

	if(instructions.size() < num_cores)
		num_cores = 1;

	int shmid = 0;
	int status = 0;
	int signl = 0;
	int ret_code = 0;
	vector<Process_shm> shm;

	pid_t p = 0;

	if(safe_to_fork == true || getenv("MATHILDA_FORK")) {
		for(uint32_t proc_num = 0; proc_num < num_cores+1; proc_num++) {
			if(use_shm) {
				shmid = shmget(IPC_PRIVATE, shm_sz, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
				shm_ptr = (uint8_t *) shmat(shmid, 0, 0);
			}

			p = fork();

			if(p == ERR) {
#ifdef DEBUG
				fprintf(stdout, "[Mathilda Debug] Failed to fork!\n");
#endif
			} else if(p == 0) {
				uint32_t start, end, sz_of_work;

				sz_of_work = instructions.size() / num_cores;
				start = sz_of_work * proc_num;

				end = start + sz_of_work;
				mathilda_proc_init(proc_num, start, end, sz_of_work);
				exit(OK);
			}

			shm.push_back({p, shm_ptr});
		}

		while((p = waitpid(-1, &status, 0))) {
			if(errno == ECHILD)
				break;

			ret_code = WEXITSTATUS(status);

			if(WIFEXITED(status)) {
#ifdef DEBUG
				fprintf(stdout, "[Mathilda Debug] Child %d exited normally with return code %d\n", p, ret_code);
#endif
				if(finish) {
					for(auto x : shm) {
						if(x.proc_pid == p)
							finish(x.shm_ptr);
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
			}

			if(WIFSTOPPED(status)) {
				signl = WSTOPSIG(status);
#ifdef DEBUG
				fprintf(stdout, "[Mathilda Debug] Child %d was stopped by signal %d\n", p, signl);
#endif
			}
		}
	} else {
		mathilda_proc_init(0, 0, instructions.size(), instructions.size());

		if(finish)
			finish(NULL);
	}

	if(use_shm) {
		for(auto s : shm) {
			shmdt(s.shm_ptr);
		}

		shm.clear();
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
	Instruction *i = NULL;

	while((msg = curl_multi_info_read(m->multi_handle, &msgs_left))) {
		if(msg->msg == CURLMSG_DONE) {
			curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &i);
			curl_easy_getinfo(i->easy, CURLINFO_RESPONSE_CODE, &response_code);

			i->curl_code = msg->data.result;

			if((response_code == i->response_code || i->response_code == 0) && i->after)
				i->after(i, i->easy, &i->response);

			if(i->response.text) {
				free(i->response.text);
				i->response.text = NULL;
			}

			curl_multi_remove_handle(m->multi_handle, i->easy);
			curl_easy_cleanup(i->easy);
			delete i;
			i = NULL;
		}
	}
}

void on_timeout(uv_timer_t *req, int status) {
	int running_handles;
	Mathilda *m = (Mathilda *) req->data;
	curl_multi_socket_action(m->multi_handle, CURL_SOCKET_TIMEOUT, 0, &running_handles);
	check_multi_info(m);
}

void start_timeout(CURLM *multi, uint64_t timeout_ms, void *userp) {
	if(timeout_ms <= 0)
		timeout_ms = 1;

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
		abort();
	}

	return 0;
}

void Mathilda::setup_uv() {
	loop = uv_default_loop();
	uv_timer_init(loop, &timeout);
}

void Mathilda::mathilda_proc_init(uint32_t proc_num, uint32_t start, uint32_t end, uint32_t sz_of_work) {
	if(instructions.empty())
		return;

	this->proc_num = proc_num;

	if(end > instructions.size())
		end = instructions.size();

	setup_uv();

	std::vector<Instruction *>::const_iterator it;

	multi_handle = curl_multi_init();
	curl_multi_setopt(multi_handle, CURLMOPT_SOCKETFUNCTION, handle_socket);
	curl_multi_setopt(multi_handle, CURLMOPT_SOCKETDATA, this);
	curl_multi_setopt(multi_handle, CURLMOPT_TIMERFUNCTION, start_timeout);
	curl_multi_setopt(multi_handle, CURLMOPT_TIMERDATA, this);

	for(it = instructions.begin()+start; it != instructions.begin()+end; ++it) {
		Instruction *i = *it;
		CURLMcode res;

		std::string uri = i->ssl ? "https://" : "http://";

		if(i->path[0] != '/')
			i->path = "/" + i->path;

		if(i->path.substr(0,2) == "//")
			i->path = i->path.substr(1, i->path.size());

#ifdef DEBUG
		fprintf(stdout, "[LibMathilda (%d)] uri=[%s] host=[%s] path=[%s]\n", proc_num, uri.c_str(), i->host.c_str(), i->path.c_str());
#endif

		std::string url = uri + i->host + i->path;

		curl_easy_setopt(i->easy, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);

		if(i->http_method == "POST") {
			curl_easy_setopt(i->easy, CURLOPT_POST, 1);
			curl_easy_setopt(i->easy, CURLOPT_POSTFIELDS, i->post_body.c_str());
		}

		if(i->follow_redirects)
			curl_easy_setopt(i->easy, CURLOPT_FOLLOWLOCATION, 1);

		if(i->cookie_file != "")
			curl_easy_setopt(i->easy, CURLOPT_COOKIEFILE, i->cookie_file.c_str());

		i->response.text = NULL;
		i->response.size = 0;

		if(i->use_proxy == true) {
			curl_easy_setopt(i->easy, CURLOPT_PROXY, i->proxy.c_str());
			curl_easy_setopt(i->easy, CURLOPT_PROXYPORT, i->proxy_port);
		}

		curl_easy_setopt(i->easy, CURLOPT_NOSIGNAL, 1); // do not remove
		curl_easy_setopt(i->easy, CURLOPT_DNS_CACHE_TIMEOUT, 1);
		curl_easy_setopt(i->easy, CURLOPT_WRITEFUNCTION, _curl_write_callback);
		curl_easy_setopt(i->easy, CURLOPT_WRITEDATA, &i->response);
		curl_easy_setopt(i->easy, CURLOPT_USERAGENT, default_ua);
		curl_easy_setopt(i->easy, CURLOPT_URL, url.c_str());
		curl_easy_setopt(i->easy, CURLOPT_PORT, i->port);
		curl_easy_setopt(i->easy, CURLOPT_SSL_VERIFYPEER, false);
		curl_easy_setopt(i->easy, CURLOPT_TIMEOUT, 0);
		curl_easy_setopt(i->easy, CURLOPT_PRIVATE, i);

	    if(i->before)
		    i->before(i, i->easy);

#ifdef DEBUG
		fprintf(stdout, "[LibMathilda (%d)] Making HTTP request: %s\n", proc_num, url.c_str());

		if(i->use_proxy == true) {
			fprintf(stdout, "[LibMathilda (%d)] Using proxy %s on port %d\n", i->proxy.c_str(), i->proxy_port);
		}
#endif
		res = curl_multi_add_handle(multi_handle, i->easy);

		if(res != CURLM_OK) {
			// TODO: handle other errors
		}
	}

	uv_run(loop, UV_RUN_DEFAULT);
	curl_multi_cleanup(multi_handle);
}

static size_t _curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	struct Response *mem = (struct Response *)userp;

	if(mem == NULL)
		return ERR;

	mem->text = (char *) realloc(mem->text, mem->size + realsize + 1);

	if(mem->text == NULL)
		return ERR;

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

	if(status < 0)
		flags = CURL_CSELECT_ERR;

	if(!status && events & UV_READABLE)
		flags |= CURL_CSELECT_IN;

	if(!status && events & UV_WRITABLE)
		flags |= CURL_CSELECT_OUT;

	curl_multi_socket_action(m->multi_handle, s->sock_fd, flags, &running_handles);
	check_multi_info(m);

	uv_timer_start(&m->timeout, (uv_timer_cb) on_timeout, 0, 0);
}

bool MathildaUtils::is_subdomain(const char *l) {
	std::string link(l);

	if((std::count(link.begin(), link.end(), '.')) < 3)
		return true;

	return false;
}

bool MathildaUtils::link_blacklist(const char *domain, const char *l) {
	std::string link(l);

	if(link[0] == '#')
		return true;

	if(link == domain)
		return true;

	return false;
}

bool MathildaUtils::page_blacklist(const char *l) {
	std::string link(l);

	if((link.find("Sorry, the page you requested was not found")) != string::npos
		|| (link.find("The requested URL was not found on this server")) != string::npos
		|| (link.find("<h2 align=\"center\">File does not exist.</h2>")) != string::npos)
		return true;
	else
		return false;
}

bool MathildaUtils::is_http_uri(const char *l) {
	std::string link(l);

	//std::regex e("^(?:http://)?([^/]+)(?:/?.*/?)/(.*)$");
	//return std::regex_match(link, e);

	if(link.substr(0, 7) == "http://")
		return true;
	else
		return false;
}

bool MathildaUtils::is_https_uri(const char *l) {
	std::string link(l);

	//std::regex e("^(?:http://)?([^/]+)(?:/?.*/?)/(.*)$");
	//return std::regex_match(link, e);

	if(link.substr(0, 8) == "https://")
		return true;
	else
		return false;
}

bool MathildaUtils::is_domain_host(const char *domain, const char *l) {
	std::string link(l);

	//std::regex e(".*\\.example.com.*");
	//return std::regex_match(link, e);

	if(link.find(domain) != string::npos)
		return true;
	else
		return false;
}

std::string MathildaUtils::extract_host_from_url(const char *l) {
	std::string link(l);

	if((MathildaUtils::is_http_uri(l)) == true) {
		std::string t = link.substr(strlen("http://"), link.size());
		int e = t.find('/');
		return t.substr(0, e);
	}

	if((MathildaUtils::is_https_uri(l)) == true) {
		std::string t = link.substr(strlen("https://"), link.size());
		int e = t.find('/');
		return t.substr(0, e);
	}

	int e = link.find("/");
	return link.substr(0, e);
}

std::string MathildaUtils::extract_page_from_url(const char *l) {
	std::string link(l);

	std::string t;

	if((MathildaUtils::is_http_uri(l)) == true) {
		t = link.substr(strlen("http://"), link.size());
	}

	if((MathildaUtils::is_https_uri(l)) == true) {
		t = link.substr(strlen("https://"), link.size());
	}

	int e = t.find('/');
	return t.substr(e, t.size());
}

#ifdef MATHILDA_TESTING
void my_before(Instruction *i, CURL *c) {
	//curl_easy_setopt(c, CURLOPT_USERAGENT, "some user agent");
}

void my_after(Instruction *i, CURL *c, Response *r) {
	//printf("Response: %s\n%s", i->host.c_str(), r->text);
}

void my_finish(uint8_t *sm) {
	//printf("Shared memory @ %p", sm);
}

int main(int argc, char *argv[]) {
	bool ret;

	std::string host = MathildaUtils::extract_host_from_url("http://example.test.example.com/something/");
	printf("extract_host_from_url(http://example.test.example.com/something) = %s\n", host.c_str());

	std::string page = MathildaUtils::extract_page_from_url("http://example.test.example.com/something/test.php");
	printf("extract_page_from_url(http://example.test.example.com/something/test.php) = %s\n", page.c_str());

	ret = MathildaUtils::is_domain_host(".example.com", "/example");
	printf("is_domain_host('/example') | ret = %d\n", ret);

	ret = MathildaUtils::is_domain_host(".example.com", "http://www.example.com");
	printf("is_domain_host('http://www.example.com') | ret = %d\n", ret);

	ret = MathildaUtils::is_domain_host(".example.com", "http://sports.exampleaaa.com");
	printf("is_domain_host('http://http://sports.exampleaaa.com') | ret = %d\n", ret);

	ret = MathildaUtils::is_domain_host(".example.com", "http://example.test.something.com/index.html");
	printf("is_domain_host('http://example.test.something.com/index.html') | ret = %d\n", ret);

	ret = MathildaUtils::is_subdomain("a.b.example.com");
	printf("is_subdomain(a.b.example.com) | ret = %d\n", ret);

	ret = MathildaUtils::is_subdomain("www.example.com");
	printf("is_subdomain(www.example.com) | ret = %d\n", ret);

	ret = MathildaUtils::is_http_uri("http://sports.example.com");
	printf("is_http_uri('http://sports.example.com') | ret = %d\n", ret);

	ret = MathildaUtils::is_http_uri("http://www.example.com");
	printf("is_http_uri('http://www.example.com') | ret = %d\n", ret);

	ret = MathildaUtils::link_blacklist("http://www.example.com", "http://www.example.com");
	printf("link_blacklist('http://www.example.com', 'http://www.example.com') | ret = %d\n", ret);

	ret = MathildaUtils::link_blacklist("http://www.example.com","http://mail.example.com");
	printf("link_blacklist('http://www.example.com', 'http://mail.example.com') | ret = %d\n", ret);

	ret = MathildaUtils::link_blacklist("http://example.com", "http://admin.example.com");
	printf("link_blacklist('http://www.example.com', 'http://admin.example.com') | ret = %d\n", ret);

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
