/// Copyright 2015/2016 Yahoo Inc.
/// Licensed under the BSD license, see LICENSE file for terms.
/// Written by Chris Rohlf

// Mathilda C interface header. This interface is kind of ugly
// but its only meant for FFI/Ctypes bindings. If you can write
// and compile C then you can write C++ and don't need this.
// This file has been annotated with custom comments so I can
// easily parse it with Ruby and spit out an FFI interface

#include "mathilda.h"
#include "mathilda_utils.h"
#include "mathilda_dns.h"

extern "C" {
	typedef void CMathilda;
	typedef void CInstruction;
	typedef void CMathildaDNS;
	typedef void *(finish_fn)(ProcessInfo *);
	typedef void *(before_fn)(Instruction *, CURL *);
	typedef void *(after_fn)(Instruction *, CURL *, Response *);

	// Mathilda
	CMathilda *new_mathilda();
	void delete_mathilda(CMathilda *cm);
	void mathilda_add_instruction(CMathilda *cm, CInstruction *i);
	void mathilda_clear_instructions(CMathilda *cm);
	void mathilda_wait_loop(CMathilda *cm);
	void mathilda_set_use_shm(CMathilda *cm, int value);
	void mathilda_set_safe_to_fork(CMathilda *cm, int value);
	void mathilda_set_cpu(CMathilda *cm, int value);
	void mathilda_set_slow_parallel(CMathilda *cm, int value);
	void mathilda_set_process_timeout(CMathilda *cm, int value);
	void mathilda_set_shm_sz(CMathilda *cm, int value);
	int mathilda_execute_instructions(CMathilda *cm);
	int mathilda_get_shm_id(CMathilda *cm);
	uint8_t *mathilda_get_shm_ptr(CMathilda *cm);
	void mathilda_set_finish(CMathilda *cm, finish_fn *fn);

	// Instruction
	CInstruction *new_instruction(char *host, char *path);
	void delete_instruction(CInstruction *ci);
	int instruction_add_http_header(CInstruction *ci, char *header);
	int instruction_set_user_agent(CInstruction *ci, char *ua);
	void instruction_set_host(CInstruction *ci, char *host);
	void instruction_set_path(CInstruction *ci, char *path);
	void instruction_set_http_method(CInstruction *ci, char *http_method);
	void instruction_set_post_body(CInstruction *ci, char *post_body);
	void instruction_set_cookie_file(CInstruction *ci, char *cookie_file);
	void instruction_set_proxy(CInstruction *ci, char *proxy);
	void instruction_set_ssl(CInstruction *ci, int value);
	void instruction_set_include_headers(CInstruction *ci, int value);
	void instruction_set_follow_redirects(CInstruction *ci, int value);
	void instruction_set_use_proxy(CInstruction *ci, int value);
	void instruction_set_verbose(CInstruction *ci, int value);
	void instruction_set_port(CInstruction *ci, uint16_t value);
	void instruction_set_proxy_port(CInstruction *ci, uint16_t value);
	void instruction_set_response_code(CInstruction *ci, uint32_t value);
	void instruction_set_connect_timeout(CInstruction *ci, uint32_t value);
	void instruction_set_http_timeout(CInstruction *ci, uint32_t value);
	void instruction_set_before(CInstruction *ci, before_fn *bf);
	void instruction_set_after(CInstruction *ci, after_fn *af);
	Response *instruction_get_response(CInstruction *ci);
	CURLcode instruction_get_curl_code(CInstruction *ci);

	// MathildaUtils
	int util_link_blacklist(char *uri);
	int util_page_blacklist(char *text);
	int util_is_http_uri(char *uri);
	int util_is_https_uri(char *uri);
	int util_is_subdomain(char *domain);
	int util_is_domain_host(char *domain, char *uri);
	char *util_extract_host_from_uri(char *uri);
	char *util_extract_path_from_uri(char *uri);
	char *util_normalize_uri(char *uri);

	// MathildaDNS
	MathildaDNS *new_mathildadns();
	void delete_mathildadns(CMathildaDNS *mdns);
	void mathildadns_flush_cache(CMathildaDNS *mdns);
	void mathildadns_disable_cache(CMathildaDNS *mdns);
	void mathildadns_enable_cache(CMathildaDNS *mdns);
	char *mathildadns_name_to_addr(CMathildaDNS *mdns, char *host, int fast);
	char *mathildadns_addr_to_name(CMathildaDNS *mdns, char *ip);
	char *mathildadns_name_to_addr_a(CMathildaDNS *mdns, char *hostnames);
	char *mathildadns_addr_to_name_a(CMathildaDNS *mdns, char *ips);
}