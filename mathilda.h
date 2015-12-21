// Copyright 2015 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf
// mathilda.h

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
#include <netdb.h>
#include <netinet/in.h>
#include <uv.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <vector>
#include <map>
#include <regex>
#include <thread>

using namespace std;

#define MATHILDA_LIB_VERSION 1.3
#define OK 0
#define ERR -1
#define ONE_MB 31250*32

static const char *default_ua = "Mozilla/5.0 (Windows NT 6.3; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/40.0.2049.0 Safari/537.36";

struct Response {
	char *text;
	size_t size;
};

class Mathilda;

class Instruction {
public:
	Instruction(char *h, char *p) {
		easy = NULL;
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
		mathilda = NULL;
	}

	Instruction(std::string const &h, std::string const &p) {
		Instruction((char *) h.c_str(), (char *) p.c_str());
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
	Mathilda *mathilda;
};

typedef struct Process_Info {
	pid_t proc_pid;
	int shm_id;
	uint8_t *shm_ptr;
} Process_Info;

// Static utility methods for common routines
class MathildaUtils {
public:
	static uint32_t count_cores();
	static bool link_blacklist(std::string const &l);
	static bool page_blacklist(std::string const &l);
	static bool is_http_uri(std::string const &l);
	static bool is_https_uri(std::string const &l);
	static bool is_subdomain(std::string const &l);
	static bool is_domain_host(std::string const &domain, std::string const &l);
	static std::string extract_host_from_url(std::string const &l);
	static std::string extract_path_from_url(std::string const &l);
	static std::string normalize_url(std::string const &l);
	static int name_to_addr(std::string const &l, std::vector<std::string> &out, bool fast);
};

class Mathilda {
public:
	Mathilda() : use_shm(false), safe_to_fork(false), shm_id(0), proc_num(0), shm_sz(ONE_MB*4), 
				shm_ptr(NULL), loop(NULL), multi_handle(NULL), timeout_seconds(30) {
		memset(&timeout, 0x0, sizeof(uv_timer_t));
	}

	~Mathilda() {
		for(auto e : easy_handles) {
			curl_easy_cleanup(e);
		}

		curl_multi_cleanup(multi_handle);
		curl_global_cleanup();
	};

	void add_instruction(Instruction *i);
	void clear_instructions();
	void mathilda_proc_init(uint32_t proc_num, uint32_t start, uint32_t end, uint32_t sz_of_work);
	void setup_uv();
	int execute_instructions();
	int create_worker_processes();
	int get_shm_id();
	uint8_t *get_shm_ptr();

	bool use_shm;
	bool safe_to_fork;
	int proc_num;
	uint32_t timeout_seconds;
	uint32_t shm_sz;
	std::vector<Instruction *> instructions;
	std::vector<CURL *> easy_handles;
	uv_loop_t *loop;
	uv_timer_t timeout;
	CURLM *multi_handle;
	std::function<void (uint8_t *s)> finish;

private:
	int shm_id;
	uint8_t *shm_ptr;
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
