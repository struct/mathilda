# Mathilda

A C++ class for writing fast and scalable web exploits and scanners.

Disclaimer: I had to strip some of Mathilda's important functionality in order to open source it. I don't recommend you use Mathilda as-is. You will need to do some modifications and tweaking in order for it to be useful for you. Feel free to reach out to me with your ideas and pull requests.

## What

Mathilda is a multi-process libcurl/libuv wrapper written in C++11. Mathilda allows you to write fast web testing tools that can operate at scale.

Using these classes you can write tools that work quickly against a very large set of hosts. Some good examples are command injection crawlers, XXE exploits, SSRF, or HTTP header discovery tools.

## Why

The libcurl API is good, but I needed something that wrapped all the plumbing code you normally rewrite with each new command line web testing tool. Mathilda is a library that handles all of that for you. All you have to do is focus on writing the code that defines the requests and does something with the responses. All of this will be automatically distributed for you across a pool of worker processes that invoke your callbacks. I have benchmarked it at about 10k HTTP GET requests per second on a 24 core system.

## How

Mathilda takes a list of Instruction class instances. Each Instruction class represents a specific HTTP request to be made. These Instructions tell Mathilda how to make a request and defines pre and post function callback pointers to handle the request and response appropriately.

Depending on the number of cores your system has Mathilda manages a pool of worker processes for efficiently distributing work with an event model implemented by libuv. This design was chosen to minimize the impact that slower servers have on performance. More on that below.

It is important to note that Mathilda calls fork to create the child processes. This is normally a bad choice for library designs but suits our needs perfectly. However this means that you shouldn't call fork from your Mathilda tool and you shouldn't reuse file descriptors from your callbacks unless you really know what you're doing. All of this is covered below in the API documentation.

A bit of history for you. This code was originally a threadpool but I ran into limitations with using evented IO from threads because nothing is thread safe. You end up having to message a worker thread to safely update timers to avoid race conditions and dead locks. Which quickly becomes very inefficient and complex code. This is true of libuv, libevent, and libev in one form or another. The complexity began to outweigh the performance hit of forking. If you don't care about using and evented IO with libcurl then you can safely use a threadpool and its fast-enough. However you will need to use select with libcurls multi interface, which is older and slower than epoll, which libuv manages for us. In fact theres probably more asynchronous operations we could be using libuv for but aren't yet.

## API

If you don't see a function documented here then it isn't intended for tool/external use. Please see the tool example at the bottom of mathilda.cpp for copy/paste'able code to get you started.

### Mathilda class functions
	* add_instruction(Instruction *) - Adds an Instruction class to an internally managed vector
	* execute_instructions() - Instructs Mathilda to start scanning hosts
	* clear_instructions() - Clears the instructions vector Mathilda holds, rarely used

### Mathilda class members
	* safe_to_fork - A bool flag indicating whether it is OK to fork (default: false)
	* use_shm - A bool flag indicating whether shared memory segments should be setup (see docs for usage)
	* shm_id - An integer containing the shared memory ID for this child process
	* finish(uint8_t *) - Callback function pointer, executed when finished. Passed a pointer to shared memory

### Mathilda class misc
	* MATHILDA_FORK - Environment variable that when set tells Mathilda it is OK to fork

### MathildaUtils static class functions

In each of these calls d is a domain and l is a link. These utility functions need some work. They were stripped of a lot of their functionality in order to open source this code. You can quickly reimplement and/or modify them for your specific use case.

	* link_blacklist(const char *d, const char *l) - Compares a link against a known blacklist of links
	* page_blacklist(const char *d, const char *l) - Compares a page against a known blacklist of 404 pages
	* is_http_uri(const char *l) - Returns true if your link is an http/http URI
	* is_subdomain(const char *l) - Returns true if the link is to a subdomain
	* is_domain_host(const char *d, const char *l) - Returns true if the link is to domain host d
	* extract_host_from_url(const char *l) - Returns test.y.example.com from http://test.y.example.com/test/index.php
	* extract_page_from_url(const char *l) - Returns /test/index.php from http://test.y.example.com/test/index.php

### Instruction class members

This class needs to be created with the new operator, filled in, and passed to the Mathilda::add_instruction function

	* host (std::string) - The hostname to scan
	* path (std::string) - The URI to request
	* http_method (std::string) - The HTTP method to use (GET/POST)
	* post_body (std::string) - The POST body to send to the server
	* cookie_file (std::string) - Location on disk of a Curl cookie file
	* user_agent (std::string) - User agent string to use (default is Chrome)
	* proxy (std::string) - Hostname of the proxy you want to use
	* port (short) - Server port to connect to
	* proxy_port (short) - Proxy port to connect to
	* response_code (int) - Invoke the after callback only if HTTP response matches this (0 for always invoke)
	* ssl (boolean) - SSL support
	* use_proxy (boolean) - Use the proxy configured with proxy/proxy_port
	* follow_redirects (boolean) - Follow HTTP redirects
	* curl_code (CURLCode) - The Curl response code returned after the request is finished
	* before(Instruction *, CURL *) - A callback function pointer, executed before curl_perform
	* after(Instruction *, CURL *, Response *) - Callback function pointer, executed after curl_perform

### Response structure members

A pointer to this structure is passed to the Mathilda 'after' callbackg

	* text - A char pointer to whatever the server responded with
	* size - The size of the data the server responded with

## Writing your callback

Writing Mathilda callbacks is painless as long as you make use of C++11 std::function. Heres an example in pseudocode:

```
int my_after(Instruction *i, CURL *c, Response *r) {
	printf("Response from %s:\n%s\n", i->host.c_str(), r->text);
	return OK;
}
...
Mathilda *m = new Mathilda();
Instruction *i = new Instruction((char *) "example.test.example.com", (char *) "/test.html");
i->after = my_after;
m->add_instruction(i);
m->execute_instructions();
```

This tells Mathilda to invoke the my_after function after a request has been made. You can access the Instruction, Curl, and Response members passed to the callback like any other. Please see the API documentation above for the details on what they expose.

The before callback executes before the Curl Easy handle is passed to Curl, giving you an opportunity to override any of the settings Mathilda has set on the handle.

The after callback executes once the Curl call is completed and gives you the opportunity to inspect the response headers or content.

The finish callback executes after all child processes have exited and gives you the opportunity to use the shared memory segment before it is destroyed.

### Accessing the Curl object

Inside of every Instruction object is a pointer to a [Curl Easy object](http://curl.haxx.se/libcurl/c/libcurl-easy.html). You can access this handle inside of your callback and make libcurl API calls to retrieve HTTP headers, status code, and anything else the libcurl API supports.

### Using Shared Memory

The Shared Memory segment is perhaps the most complex part of Mathilda. It is %100 OPTIONAL, but can be helpful when collecting results across multiple processes that would otherwise be lost. Before fork is called, each Mathilda shm_id and shm_ptr member variable is updated to point to a shared memory segment created just for that child process. In order to use this functionality the use_shm flag must be set to true when you create your Mathilda class instance.

You can use this shared memory to store anything, and then access it later using the finish callback which is invoked after all child processes have exited.

## FAQ:

Q: What third party requirements are there?

A: libcurl, libuv, libstdc++.x86_64 libstdc++-devel.x86_64, a modern GCC/Clang (I recommend libgumbo for HTML parsing)

Q: Why not just write a better libcurl?

A: Because theres nothing wrong with libcurl. In fact it works perfectly for both HTTP and webapp testing. But you end up writing the same code over and over everytime you want to write a small testing tool. Mathilda solves that problem by implementing the plumbing in such a way that scales your small tools up with little to no additional work.

Q: What has Mathilda been used for?

A: Web application security testing, web server and caching proxy load testing, HTTP fuzzing, and many other things.

Q: How do I compile/use Mathilda and these tools?

A: Heres the short answer:

Compile Mathilda as a library:

g++ -o libmathilda.so mathilda.cpp -lcurl -std=c++11 -shared -fPIC -rdynamic -ggdb -luv

Compile Mathilda unit tests (found at the bottom of Mathilda.cpp)

g++ -o mathilda_test mathilda.cpp -lcurl -std=c++11 -ggdb -luv -DMATHILDA_TESTING

You can delete the unit tests at the bottom of mathilda.cpp and replace it with your own code and use the command above.

## Who

Mathilda was written in 2015 by Chris Rohlf at Yahoo

## Copyright

Copyright 2015 Yahoo Inc.
Licensed under the BSD license, see LICENSE file for terms.
Written by Chris Rohlf

## TODO

* An authentication hook for when you have custom SSO requirements
* post_parameters map is currently unused
* HTTP2 support (requires libcurl changes)
* Blacklists could and should be configurable and populated via the API
