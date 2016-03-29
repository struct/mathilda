// Copyright 2015 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf
// Mathilda module base class

#define RET_ERR_IF_PTR_INVALID(ptr)	\
	if(ptr == NULL || ptr == (void *) -1) { 	\
		return ERR;					\
	}

#define RET_IF_PTR_INVALID(ptr)	 	\
	if(ptr == NULL || ptr == (void *) -1) { 	\
		return;						\
	}

typedef struct StringEntry {
	size_t length;
} StringEntry;

/// MathildaUtils
/// Static utility methods for common routines
class MathildaUtils {
public:
	static int name_to_addr(std::string const &host, std::vector<std::string> &out, bool fast);
	static int shm_store_string(uint8_t *shm_head, const char *str, size_t sz);
	static void name_to_addr_a(std::vector<std::string> const &hostnames, std::vector<std::string> &out);
	static void get_http_headers(const char *text, std::map<std::string, std::string> &hdrs);
	static void read_file(char *fname, std::vector<std::string> &ret);
	static void unique_string_vector(vector<std::string> &strs);
	static void split(const std::string &str, char delim, std::vector<std::string> &elems);
	static void replaceAll(std::string &str, const std::string &from, const std::string &to);
	static bool link_blacklist(std::string const &uri);
	static bool page_blacklist(std::string const &text);
	static bool is_http_uri(std::string const &uri);
	static bool is_https_uri(std::string const &uri);
	static bool is_subdomain(std::string const &domain);
	static bool is_domain_host(std::string const &domain, std::string const &uri);
	static std::string extract_host_from_uri(std::string const &uri);
	static std::string extract_path_from_uri(std::string const &uri);
	static std::string normalize_uri(std::string const &uri);
};