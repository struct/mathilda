/// Copyright 2015/2016 Yahoo Inc.
/// Licensed under the BSD license, see LICENSE file for terms.
/// Written by Chris Rohlf

#include "mathilda.h"
#include "mathilda_utils.h"

/// Checks if a domain is a subdomain
///
/// This works by first calling extract_host_from_uri
/// and then checking the host to see if it would be
/// a subdomain or a tld
///
/// @param[in] uri A url, hostname or other string
/// @return true if uri is a subdomain, false if its not
bool MathildaUtils::is_subdomain(std::string const &uri) {
	std::string domain = MathildaUtils::extract_host_from_uri(uri);

	if((std::count(domain.begin(), domain.end(), '.')) < 3) {
		return true;
	}

	return false;
}

/// Compares a link against a known blacklist of links
///
/// Takes a link and compares it against a hardcoded blacklist
/// This is useful for when you don't want to make requests
/// to pages with endless redirects or bad URIs
///
/// @param[in] link A url, hostname or other string
/// @return true if blacklisted, false if not
bool MathildaUtils::link_blacklist(std::string const &link) {
	if(link[0] == '#') {
		return true;
	}

	const char *blacklist [] = { "example.com", "javascript:" };

	for(size_t i=0; i < (sizeof(blacklist) / sizeof(char *)); i++) {
		if(link.find(blacklist[i]) != string::npos) {
			return true;
		}
	}

	return false;
}

/// Compares a page against a known blacklist of page contents
///
/// Takes a blob of text from a webpage and checks for known
/// blacklisted contents. Useful for 404 pages.
///
/// @param[in] text A blob of text, usually from an HTML page
/// @return true if blacklisted, false if not
bool MathildaUtils::page_blacklist(std::string const &text) {
	if((text.find("Sorry, the page you requested was not found")) != string::npos
		|| (text.find("The requested URL was not found on this server")) != string::npos
		|| (text.find("<h2 align=\"center\">File does not exist.</h2>")) != string::npos) {
		return true;
	} else {
		return false;
	}
}

/// Returns true if your string is an http URI
///
/// Takes a string and determines if it starts with
/// the http:// URI. This is a simple helper function
/// for quickly determining whether to ignore a link
///
/// @param[in] uri A string, usually a URI
/// @return true if uri starts with http://, false if it doesn't
bool MathildaUtils::is_http_uri(std::string const &uri) {
	if(uri.substr(0, 7) == "http://") {
		return true;
	} else {
		return false;
	}
}

/// Returns true if your string is an https URI
///
/// Takes a string and determines if it starts with
/// the https:/// URI. This is a simple helper function
/// for quickly determining whether to ignore a link
///
/// @param[in] uri A string, usually a link from an HTML page
/// @return true if uri starts with https:///, false if it doesn't
bool MathildaUtils::is_https_uri(std::string const &uri) {
	if(uri.substr(0, 8) == "https://") {
		return true;
	} else {
		return false;
	}
}

/// Checks if a link will make request to domain
///
/// This function is useful for when you have a link
/// to a potentially new endpoint but want to constrain
/// all operations to a specific TLD
///
/// @param[in] domain A TLD you want to check for
/// @param[in] uri A string or a link usually from an HTML page
/// @return true if the uri goes to that domain, false if it doesn't
bool MathildaUtils::is_domain_host(std::string const &domain, std::string const &uri) {
	std::string d = MathildaUtils::extract_host_from_uri(uri);

	if(d.find(domain) != string::npos) {
		return true;
	} else {
		return false;
	}
}

/// Extracts the hostname from a URI string
///
/// This function extracts a hostname from a URL. Its used
/// heavily internally to the library to perform functions
/// like is_domain_host. Its helpful in situations where
/// you want to build new URI's from existing ones
///
/// @param[in] uri A string or a link usually from an HTML page
/// @return Returns a std::string which holds the extracted host
std::string MathildaUtils::extract_host_from_uri(std::string const &uri) {
	std::string host;

	if((MathildaUtils::is_http_uri(uri)) == true) {
		std::string t = uri.substr(strlen("http://"), uri.size());
		size_t e = t.find('/');

		if(e == string::npos) {
			e = t.size();
		}

		host = t.substr(0, e);
	} else if((MathildaUtils::is_https_uri(uri)) == true) {
		std::string t = uri.substr(strlen("https://"), uri.size());
		size_t e = t.find('/');

		if(e == string::npos) {
			e = t.size();
		}

		host = t.substr(0, e);
	} else {
		host = uri;
	}

	if(host.find("/") != string::npos) {
		size_t e = host.find('/');

		if(e == string::npos) {
			e = host.size();
		}

		return host.substr(0, e);
	} else {
		return host;
	}
}

/// Extracts the query path from a URI
///
/// This function makes extracting query paths from
/// arbitrary URI's much easier. Its useful when you
/// are scraping an HTML page for links.
///
/// @param[in] uri A string or a link usually from an HTML page
/// @returns Returns a std::string which holds the extracted query path
std::string MathildaUtils::extract_path_from_uri(std::string const &uri) {
	std::string t(uri);

	if((MathildaUtils::is_http_uri(uri)) == true) {
		t = uri.substr(strlen("http://"), uri.size());
	}

	if((MathildaUtils::is_https_uri(uri)) == true) {
		t = uri.substr(strlen("https://"), uri.size());
	}

	size_t e = t.find('/');

	if(e == string::npos) {
		return "";
	}

	return t.substr(e, t.size());
}

/// Performs some basic normalization of a URI
///
/// This function performs some basic normalization
/// of a URI string. Its needed because theres a lot
/// of broken HTML out there and our API's generally
/// require URI's in a specific format
///
/// @param[in,out] uri A string or a link usually from an HTML page
/// @return Returns a std::string uri thats been 'normalized'
std::string MathildaUtils::normalize_uri(std::string const &uri) {
	std::string prepend = "http://";
	std::string tmp;

	if((MathildaUtils::is_https_uri(uri)) == true) {
		prepend = "https://";
	}

	tmp = uri.substr(prepend.size(), uri.size());

    if(tmp[0] == '/') {
        tmp.erase(tmp.begin());
    }

	while((tmp.find("//")) != string::npos) {
		tmp.replace(tmp.find("//"), 2, "/");
	}

	tmp = prepend + tmp;

	return tmp;
}

/// Tokenizes and returns the headers from an HTTP response
///
/// Takes a blob of text containing HTTP headers and
/// a std::map of std::string and returns the map filled
/// out containing all the key/value header pairs
///
/// @param[in] text A char * pointer to an HTTP response payload
/// @param[in,out] hdrs A std::map of std::string that will hold the
///	           HTTP header key value pairs
void MathildaUtils::get_http_headers(const char *text, std::map<std::string, std::string> &hdrs) {
	std::stringstream ss(text);
	std::string item;

	while (std::getline(ss, item, '\n')) {
		if(item[0] == '\r') {
			break;
		}

		if(item.find(":") != string::npos) {
			hdrs[item.substr(0, item.find(":"))] = item.substr(item.find(":")+2, item.size()-item.find(":")-3);
		}
	}
}

/// Reads a file and returns its contents
///
/// Reads a file by filename and fills out a std::vector
/// of std::string with each string delimeted by newline
///
/// @param[in] fname The filename
/// @param[in,out] ret A std::vector of std::string containing
///				   the file contents
void MathildaUtils::read_file(char *fname, std::vector<std::string>& ret) {
	std::ifstream file(fname);
	std::string line;

	while(std::getline(file, line)) {
		ret.push_back(line);
	}
}

/// Removes the duplicates from a vector of strings
///
/// Takes a std::vector of std::string and removes
/// any duplicates from it in place
///
/// @param[in,out] strs A std::vector of std::string
void MathildaUtils::unique_string_vector(vector<std::string> &strs) {
	sort(strs.begin(), strs.end());
	strs.erase(unique(strs.begin(), strs.end()), strs.end());
}

/// Splits a string using a delimeter
///
/// Takes a string and a delimeter and splits it
/// up into separate strings and stores them in
/// the elems vector
///
/// @param[in] str A std::string to tokenize
/// @param[in] delim A character delimeter
/// @param[in,out] elems A std::vector of strings
void MathildaUtils::split(const std::string &str, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(str);
	std::string item;

	while (std::getline(ss, item, delim)) {
		if(!item.empty()) {
			elems.push_back(std::move(item));
		}
	}
}

/// String replacement function
///
/// Replaces all occurences of a string A within
/// string B with string C
///
/// @param[in,out] str A std::string to operate on
/// @param[in] from A std::string we want to replace in str with to
/// @param[in] to A std::string we want want to replace from with
void MathildaUtils::replaceAll(std::string& str, const std::string& from, const std::string& to) {
	size_t s;

	if(from.size() == 0) {
		return;
	}

	while((s = str.find(from, s)) != string::npos) {
		str.replace(s, from.length(), to);
		s += to.length();
	}
}