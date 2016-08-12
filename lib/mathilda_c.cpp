/// Copyright 2015/2016 Yahoo Inc.
/// Licensed under the BSD license, see LICENSE file for terms.
/// Written by Chris Rohlf

// This is a C interface to the C++ Mathilda library
// The C interface allows us to write FFI or CTypes
// much easier than trying to wrap the C++ classes

#include "mathilda_c.h"

// Mathilda class wrapping

extern "C" {

CMathilda *new_mathilda() {
	return reinterpret_cast<void*>(new Mathilda());
}

void delete_mathilda(CMathilda *cm) {
	delete reinterpret_cast<Mathilda *>(cm);
}

void mathilda_add_instruction(CMathilda *cm, CInstruction *i) {
	reinterpret_cast<Instruction *>(i)->mathilda = reinterpret_cast<Mathilda *>(cm);
	reinterpret_cast<Instruction *>(i)->mathilda->instructions.push_back(reinterpret_cast<Instruction *>(i));
}

void mathilda_clear_instructions(CMathilda *cm) {
	reinterpret_cast<Mathilda *>(cm)->instructions.clear();
}

void mathilda_wait_loop(CMathilda *cm) {
	reinterpret_cast<Mathilda *>(cm)->wait_loop();
}

int mathilda_execute_instructions(CMathilda *cm) {
	return reinterpret_cast<Mathilda *>(cm)->execute_instructions();
}

int mathilda_get_shm_id(CMathilda *cm) {
	return reinterpret_cast<Mathilda *>(cm)->shm_id;
}

uint8_t *mathilda_get_shm_ptr(CMathilda *cm) {
	return (uint8_t *) reinterpret_cast<Mathilda *>(cm)->shm_ptr;
}

void mathilda_set_use_shm(CMathilda *cm, int value) {
	reinterpret_cast<Mathilda *>(cm)->use_shm = (value == 1) ? true : false;
}

void mathilda_set_safe_to_fork(CMathilda *cm, int value) {
	reinterpret_cast<Mathilda *>(cm)->safe_to_fork = (value == 1) ? true : false;
}

void mathilda_set_cpu(CMathilda *cm, int value) {
	reinterpret_cast<Mathilda *>(cm)->set_cpu = (value == 1) ? true : false;
}

void mathilda_set_slow_parallel(CMathilda *cm, int value) {
	reinterpret_cast<Mathilda *>(cm)->slow_parallel = (value == 1) ? true : false;
}

void mathilda_set_process_timeout(CMathilda *cm, int value) {
	reinterpret_cast<Mathilda *>(cm)->process_timeout = value;
}

void mathilda_set_shm_sz(CMathilda *cm, int value) {
	reinterpret_cast<Mathilda *>(cm)->shm_sz = value;
}

void mathilda_set_finish(CMathilda *cm, finish_fn *fn) {
	reinterpret_cast<Mathilda *>(cm)->finish = fn;
}

// Instruction class wrapping

CInstruction *new_instruction(char *host, char *path) {
	return reinterpret_cast<void*>(new Instruction(host, path));
}

void delete_instruction(CInstruction *ci) {
	delete reinterpret_cast<Instruction *>(ci);
}

int instruction_add_http_header(CInstruction *ci, char *header) {
	reinterpret_cast<Instruction *>(ci)->http_header_slist = curl_slist_append(reinterpret_cast<Instruction *>(ci)->http_header_slist, header);

	if(reinterpret_cast<Instruction *>(ci)->http_header_slist == NULL) {
		return ERR;
	}

	curl_easy_setopt(reinterpret_cast<Instruction *>(ci)->easy, CURLOPT_HTTPHEADER, reinterpret_cast<Instruction *>(ci)->http_header_slist);

	return OK;
}

int instruction_set_user_agent(CInstruction *ci, char *ua) {
	if(curl_easy_setopt(reinterpret_cast<Instruction *>(ci)->easy, CURLOPT_USERAGENT, ua) == CURLE_OK) {
		return OK;
	} else {
		return ERR;
	}
}

void instruction_set_host(CInstruction *ci, char *host) {
	reinterpret_cast<Instruction *>(ci)->host = host;
}

void instruction_set_path(CInstruction *ci, char *path) {
	reinterpret_cast<Instruction *>(ci)->path = path;
}

void instruction_set_http_method(CInstruction *ci, char *http_method) {
	reinterpret_cast<Instruction *>(ci)->http_method = http_method;
}

void instruction_set_post_body(CInstruction *ci, char *post_body) {
	reinterpret_cast<Instruction *>(ci)->post_body = post_body;
}

void instruction_set_cookie_file(CInstruction *ci, char *cookie_file) {
	reinterpret_cast<Instruction *>(ci)->cookie_file = cookie_file;
}

void instruction_set_proxy(CInstruction *ci, char *proxy) {
	reinterpret_cast<Instruction *>(ci)->proxy = proxy;
}

void instruction_set_ssl(CInstruction *ci, int value) {
	reinterpret_cast<Instruction *>(ci)->ssl = (value == 1) ? true : false;
}

void instruction_set_include_headers(CInstruction *ci, int value) {
	reinterpret_cast<Instruction *>(ci)->include_headers = (value == 1) ? true : false;
}

void instruction_set_follow_redirects(CInstruction *ci, int value) {
	reinterpret_cast<Instruction *>(ci)->follow_redirects = (value == 1) ? true : false;
}

void instruction_set_use_proxy(CInstruction *ci, int value) {
	reinterpret_cast<Instruction *>(ci)->use_proxy = (value == 1) ? true : false;
}

void instruction_set_verbose(CInstruction *ci, int value) {
	reinterpret_cast<Instruction *>(ci)->verbose = (value == 1) ? true : false;
}

void instruction_set_port(CInstruction *ci, uint16_t value) {
	reinterpret_cast<Instruction *>(ci)->port = value;
}

void instruction_set_proxy_port(CInstruction *ci, uint16_t value) {
	reinterpret_cast<Instruction *>(ci)->proxy_port = value;
}

void instruction_set_response_code(CInstruction *ci, uint32_t value) {
	reinterpret_cast<Instruction *>(ci)->response_code = value;
}

void instruction_set_connect_timeout(CInstruction *ci, uint32_t value) {
	reinterpret_cast<Instruction *>(ci)->connect_timeout = value;
}

void instruction_set_http_timeout(CInstruction *ci, uint32_t value) {
	reinterpret_cast<Instruction *>(ci)->http_timeout = value;
}

Response *instruction_get_response(CInstruction *ci) {
	return &reinterpret_cast<Instruction *>(ci)->response;
}

CURLcode instruction_get_curl_code(CInstruction *ci) {
	return reinterpret_cast<Instruction *>(ci)->curl_code;
}

void instruction_set_before(CInstruction *ci, before_fn *bf) {
	reinterpret_cast<Instruction *>(ci)->before = bf;
}

void instruction_set_after(CInstruction *ci, after_fn *af) {
	reinterpret_cast<Instruction *>(ci)->after = af;
}

int util_link_blacklist(char *uri) {
	return MathildaUtils::link_blacklist(uri);
}

int util_page_blacklist(char *text) {
	return MathildaUtils::page_blacklist(text);
}

int util_is_http_uri(char *uri) {
	return MathildaUtils::is_http_uri(uri);
}

int util_is_https_uri(char *uri) {
	return MathildaUtils::is_https_uri(uri);
}

int util_is_subdomain(char *domain) {
	return MathildaUtils::is_subdomain(domain);
}

int util_is_domain_host(char *domain, char *uri) {
	return MathildaUtils::is_domain_host(domain, uri);
}

char *util_extract_host_from_uri(char *uri) {
	std::string s = MathildaUtils::extract_host_from_uri(uri);
	char *ts = (char *) malloc(s.size()+1);
	memset(ts, 0x0, s.size()+1);
	strncpy(ts, s.c_str(), s.size());
	return ts;
}

char *util_extract_path_from_uri(char *uri) {
	std::string s = MathildaUtils::extract_path_from_uri(uri);
	char *ts = (char *) malloc(s.size()+1);
	memset(ts, 0x0, s.size()+1);
	strncpy(ts, s.c_str(), s.size());
	return ts;
}

char *util_normalize_uri(char *uri) {
	std::string s = MathildaUtils::normalize_uri(uri);
	char *ts = (char *) malloc(s.size()+1);
	memset(ts, 0x0, s.size()+1);
	strncpy(ts, s.c_str(), s.size());
	return ts;
}

MathildaDNS *new_mathildadns() {
	return reinterpret_cast<MathildaDNS *>(new MathildaDNS());
}

void delete_mathildadns(CMathildaDNS *mdns) {
	delete reinterpret_cast<MathildaDNS *>(mdns);
}

void mathildadns_flush_cache(CMathildaDNS *mdns) {
	reinterpret_cast<MathildaDNS *>(mdns)->flush_cache();
}

void mathildadns_disable_cache(CMathildaDNS *mdns) {
	reinterpret_cast<MathildaDNS *>(mdns)->disable_cache();
}

void mathildadns_enable_cache(CMathildaDNS *mdns) {
	reinterpret_cast<MathildaDNS *>(mdns)->enable_cache();
}

char *mathildadns_name_to_addr(CMathildaDNS *mdns, char *host, int fast) {
	std::vector<std::string> r;
	char *results = NULL;
	int ret = reinterpret_cast<MathildaDNS *>(mdns)->name_to_addr(host, r, fast);

	if(ret == OK) {
		std::string z;

		for(auto &n : r) {
			z += n + ",";
		}

		results = (char *) malloc(z.size()+1);
		memset(results, 0x0, z.size()+1);
		strncpy(results, z.c_str(), z.size());
	}

	return results;
}

char *mathildadns_addr_to_name(CMathildaDNS *mdns, char *ip) {
	std::string r;
	char *result = NULL;
	int ret = reinterpret_cast<MathildaDNS *>(mdns)->addr_to_name(ip, r);

	if(ret == OK) {
		result = (char *) malloc(r.size()+1);
		memset(result, 0x0, r.size()+1);
		strncpy(result, r.c_str(), r.size());
	}

	return result;
}

char *mathildadns_name_to_addr_a(CMathildaDNS *mdns, char *hostnames) {
	std::vector<std::string> h;
	std::vector<std::string> r;
	char *results = NULL;
	MathildaUtils::split(hostnames, ',', h);
	reinterpret_cast<MathildaDNS *>(mdns)->name_to_addr_a(h, r);

	std::string y;
	for(auto &z : r) {
		y += z + ",";
	}

	results = (char *) malloc(y.size()+1);
	memset(results, 0x0, y.size()+1);
	strncpy(results, y.c_str(), y.size());
	return results;
}

char *mathildadns_addr_to_name_a(CMathildaDNS *mdns, char *ips) {
	std::vector<std::string> i;
	std::vector<std::string> r;
	char *results = NULL;
	MathildaUtils::split(ips, ',', i);
	reinterpret_cast<MathildaDNS *>(mdns)->addr_to_name_a(i, r);

	std::string y;

	for(auto &z : r) {
		y += z + ",";
	}

	results = (char *) malloc(y.size()+1);
	memset(results, 0x0, y.size()+1);
	strncpy(results, y.c_str(), y.size());
	return results;
}

} // extern C
