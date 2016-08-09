/// Copyright 2015/2016 Yahoo Inc.
/// Licensed under the BSD license, see LICENSE file for terms.
/// Written by Chris Rohlf

// Mathilda C interface header. This interface is kind of ugly
// but its only meant for FFI/Ctypes bindings. If you can write
// and compile C then you can write C++ and don't need this.

#include "mathilda.h"

extern "C" {
	typedef void CMathilda;
	typedef void CInstruction;
	typedef void *(finish_fn)(ProcessInfo *);
	typedef void *(before_fn)(Instruction *, CURL *);
	typedef void *(after_fn)(Instruction *, CURL *, Response *);

	CMathilda *new_mathilda();
	void delete_mathilda(CMathilda *cm);
	void mathilda_add_instruction(CMathilda *cm, Instruction *i);
	void mathilda_clear_instructions(CMathilda *cm);
	void mathilda_wait_loop(CMathilda *cm);
	int mathilda_execute_instructions(CMathilda *cm);
	int mathilda_get_shm_id(CMathilda *cm);
	uint8_t *mathilda_get_shm_ptr(CMathilda *cm);
	void mathilda_set_use_shm(CMathilda *cm, int value);
	void mathilda_set_safe_to_fork(CMathilda *cm, int value);
	void mathilda_set_cpu(CMathilda *cm, int value);
	void mathilda_set_slow_parallel(CMathilda *cm, int value);
	void mathilda_set_process_timeout(CMathilda *cm, int value);
	void mathilda_set_shm_sz(CMathilda *cm, int value);

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
	Response *instruction_get_response(CInstruction *ci);
	CURLcode instruction_get_curl_code(CInstruction *ci);
	void instruction_set_before(CInstruction *ci, before_fn *bf);
	void instruction_set_after(CInstruction *ci, after_fn *af);
}