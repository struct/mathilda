// Copyright 2015 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf
// mathilda.h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <uv.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <vector>
#include <map>
#include <regex>
#include <thread>

using namespace std;

#define MATHILDA_LIB_VERSION 1.0
#define NUM_PROCS 24
#define OK 0
#define ERR -1
#define ONE_MB 31250*32

static const char *default_ua = "Mozilla/5.0 (Windows NT 6.3; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/40.0.2049.0 Safari/537.36";

struct Response {
	char *text;
	size_t size;
};

class Instruction {
public:
	Instruction(char *h, char *p) {
		easy = curl_easy_init();
		host.assign(h);
		path.assign(p);
		http_method.assign("GET");
		user_agent.assign(default_ua);
		proxy.assign("");
		ssl = false;
		follow_redirects = true;
		use_proxy = false;
		port = 80;
		proxy_port = 8080;
		response_code = 200;
		before = NULL;
		after = NULL;
		response.text = NULL;
		response.size = 0;
	}

	Instruction(std::string h, std::string p) {
		Instruction(h.c_str(), p.c_str());
	}

	// There is no object destruction required
	~Instruction() { }

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
	bool follow_redirects;
	bool use_proxy;
	uint16_t port;
	uint16_t proxy_port;
	uint32_t response_code;
	Response response;
	CURLcode curl_code;
	std::function<void (Instruction *, CURL *)> before;
	std::function<void (Instruction *, CURL *, Response *)> after;
};

typedef struct Process_shm {
	pid_t proc_pid;
	uint8_t *shm_ptr;
} Process_shm;

// Static utility methods for common routines
class MathildaUtils {
public:
	static uint32_t count_cores();
	static bool link_blacklist(const char *domain, const char *l);
	static bool page_blacklist(const char *l);
	static bool is_http_uri(const char *l);
	static bool is_https_uri(const char *l);
	static bool is_subdomain(const char *l);
	static bool is_domain_host(const char *domain, const char *l);
	static std::string extract_host_from_url(const char *l);
	static std::string extract_page_from_url(const char *l);
};

class Mathilda {
public:
	Mathilda() : use_shm(false), safe_to_fork(false), shm_id(0), proc_num(0), shm_sz(ONE_MB), 
				shm_ptr(NULL), loop(NULL), multi_handle(NULL) {
		memset(&timeout, 0x0, sizeof(uv_timer_t));
	}

	~Mathilda() {
		curl_global_cleanup();
	};

	void add_instruction(Instruction *i);
	void clear_instructions();
	void mathilda_proc_init(uint32_t proc_num, uint32_t start, uint32_t end, uint32_t sz_of_work);
	void setup_uv();
	int execute_instructions();
	int create_worker_processes();

	bool use_shm;
	bool safe_to_fork;
	int shm_id;
	int proc_num;
	uint32_t shm_sz;
	uint8_t *shm_ptr;
	std::vector<Instruction *> instructions;
	uv_loop_t *loop;
	uv_timer_t timeout;
	CURLM *multi_handle;
	std::function<void (uint8_t *s)> finish;
};

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