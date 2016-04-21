// Copyright 2015/2016 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <curl/curl.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sched.h>
#include <netdb.h>
#include <netinet/in.h>
#include <uv.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <regex>
#include <thread>

#include "mathilda_fork.h"

using namespace std;

#define MATHILDA_LIB_VERSION 1.5
#define OK 0
#define ERR -1
#define SHM_SIZE 1000000*16

static const char *default_ua = "Mozilla/5.0 (Windows NT 6.3; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/40.0.2049.0 Safari/537.36";

/// Response structure
/// The structure returned upon every
/// successful HTTP request
struct Response {
	char *text;
	size_t size;
};

class Mathilda;

/// Instruction class
/// Each Instruction class represents an HTTP
/// request to be made by the Mathilda engine
class Instruction {
public:
	Instruction(char *h, char *p) {
		easy = NULL;
		host = h;
		path = p;
		http_method = "GET";
		user_agent = default_ua;
		proxy = "";
		ssl = false;
		include_headers = true;
		follow_redirects = true;
		use_proxy = false;
		verbose = false;
		port = 80;
		proxy_port = 8080;
		response_code = 200;
		http_header_slist = NULL;
		before = NULL;
		after = NULL;
		response.text = NULL;
		response.size = 0;
		mathilda = NULL;
	}

	Instruction(std::string const &h, std::string const &p) {
		Instruction((char *) h.c_str(), (char *) p.c_str());
	}

	~Instruction() { 
		if(http_header_slist) {
			curl_slist_free_all(http_header_slist);
		}

		http_header_slist = NULL;
	}

	int add_http_header(const std::string &header);
	int set_user_agent(const std::string &ua);

	CURL *easy;
	std::string host;
	std::string path;
	std::string http_method;
	std::string post_body;
	std::string cookie_file;
	std::string user_agent;
	std::string proxy;
	std::map<std::string,std::string> post_parameters; // XXX TODO currently unused
	bool ssl;
	bool include_headers;
	bool follow_redirects;
	bool use_proxy;
	bool verbose;
	uint16_t port;
	uint16_t proxy_port;
	uint32_t response_code;
	struct curl_slist *http_header_slist;
	Response response;
	CURLcode curl_code;
	std::function<void (Instruction *, CURL *)> before;
	std::function<void (Instruction *, CURL *, Response *)> after;
	Mathilda *mathilda;
};

/// Mathilda class
/// The libmathilda engine is implemented here
class Mathilda {
public:
	Mathilda() {
		use_shm = false;
		safe_to_fork = true;
		set_cpu = true;
		slow_parallel = false;
		proc_num = 0;
		timeout_seconds = 30;
		shm_sz = SHM_SIZE;
		loop = NULL;
		multi_handle = NULL;
		shm_id = 0;
		shm_ptr = NULL;
		multi_handle = NULL;

		memset(&timeout, 0x0, sizeof(uv_timer_t));

		mf = new MathildaFork();
	}

	~Mathilda() {
		for(auto e : easy_handles) {
			curl_easy_cleanup(e);
		}

		if(multi_handle) {
			curl_multi_cleanup(multi_handle);
		}

		curl_global_cleanup();

		multi_handle = NULL;

		if(instructions.size() > 0) {
			instructions.erase(instructions.begin(), instructions.end());
		}

		delete mf;
		mf = NULL;
	};

	void add_instruction(Instruction *i);
	void clear_instructions();
	void wait_loop();
	int execute_instructions();
	int get_shm_id();
	uint8_t *get_shm_ptr();

	bool use_shm;
	bool safe_to_fork;
	bool set_cpu;
	bool slow_parallel;
	int proc_num;
	uint32_t timeout_seconds;
	uint32_t shm_sz;
	std::vector<Instruction *> instructions;
	std::vector<CURL *> easy_handles;
	uv_loop_t *loop;
	uv_timer_t timeout;
	CURLM *multi_handle;
	std::function<void (ProcessInfo *pi)> finish;
	MathildaFork *mf;

private:
	void mathilda_proc_init(uint32_t proc_num, uint32_t start, uint32_t end, uint32_t sz_of_work);
	void setup_uv();
	int create_worker_processes();
	int shm_id;
	uint8_t *shm_ptr;
};

/// Socket_Info Structure is used internally
/// by the Mathilda class
class Socket_Info {
  public:
	Socket_Info(curl_socket_t s, Mathilda *m) : sock_fd(s), m(m) {
		memset(&poll_handle, 0x0, sizeof(poll_handle));
		uv_poll_init_socket(m->loop, &poll_handle, sock_fd);
		poll_handle.data = this;
	}

	~Socket_Info() {}

	uv_poll_t poll_handle;
	curl_socket_t sock_fd;
	Mathilda *m;
};

void check_multi_info(Mathilda *m);
void curl_perform(uv_poll_t *si, int status, int events);
void curl_close_cb(uv_handle_t *handle);
void start_timeout(CURLM *multi, uint64_t timeout_ms, void *userp);
void on_timeout(uv_timer_t *req, int status);
int handle_socket(CURL *easy, curl_socket_t s, int action, void *userp, void *socketp);
static size_t _curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp);
