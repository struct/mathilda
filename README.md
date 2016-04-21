# Mathilda

A C++ class for writing fast and scalable web exploits and scanners.

Disclaimer: I had to strip some of Mathilda's important functionality in order to open source it. You may need to do some modifications and tweaking in order for it to be useful for you. Feel free to reach out to me with your ideas and pull requests.

## What

Mathilda is a multi-process libcurl/libuv wrapper written (mostly) in C++11. Mathilda allows you to write fast web testing tools that can operate at scale. But what does scale mean in this context? It means horizontal scaling and distribution of work across worker processes wherever possible.

Using these classes you can write tools that work quickly against a very large set of hosts. Some good examples are command injection crawlers, XXE exploits, SSRF, or HTTP header discovery tools.

Mathilda is not a singleton, you can have multiple Mathilda class instances in your program. Each class instance manages its own internal state with regards to child processes, libuv, and libcurl. The point is to abstract a lot of that away but still give you access to the bits that you want to customize.

## Why

The libcurl API is good, but I needed something that wrapped all the plumbing code you normally rewrite with each new command line web testing tool. Mathilda is a library that handles all of that for you. All you have to do is focus on writing the code that defines the requests and does something with the responses. All of this will be automatically distributed for you across a pool of worker processes that invoke your callbacks. I have benchmarked it doing simple tasks at around ~10,000 HTTP GET requests per second on a 24 core system.

## How

Mathilda takes a list of Instruction class instances. Each Instruction class represents a specific HTTP request to be made. These Instruction class instances tell Mathilda how to make a request and defines pre and post function callback pointers to handle the request and response appropriately.

Depending on the number of cores your system has Mathilda manages a pool of worker processes for efficiently distributing work with an event model implemented by libuv. This design was chosen to minimize the impact that slower servers have on performance. More on that below.

It is important to note that Mathilda calls fork to create the child processes. This is normally a bad choice for library designs but suits our needs perfectly. Mathilda manages these child processes with a class named MathildaFork. You can use this class directly from your own code to manage child processes if you want but its purely optional.

A bit of history for you. This code was originally a threadpool but I ran into limitations with using evented IO from threads because nothing is thread safe. You end up having to message a worker thread to safely update timers to avoid race conditions and dead locks. This quickly becomes very inefficient and complex code that isn't worth the small performance gains (http://stackoverflow.com/a/809049). This is true of libuv, libevent, and libev in one form or another. The complexity began to outweigh the performance hit of forking. If you don't care about using and evented IO with libcurl then you can safely use a threadpool and its fast-enough. However you will need to use select with libcurls multi interface, which is older and slower than epoll, which libuv manages for us. In fact theres probably more asynchronous operations we could be using libuv for but aren't yet. If you're interested in how some of the Mathilda bits are reusable see Mathilda::name_to_addr_a and how it uses MathildaFork to distribute DNS lookups across worker processes.

## API

All source code is documented using Doxygen and the documentation automatically created in the project. Everything below is a quick start guide to familiarize you with how to use Mathilda and Instruction classes. I recommend starting here and referring to the Doxygen generated documentation when writing code.

### Mathilda class functions

	* add_instruction(Instruction *) - Adds an Instruction class to an internally managed vector
	* execute_instructions() - Instructs Mathilda to start scanning hosts
	* clear_instructions() - Clears the instructions vector Mathilda holds, rarely used

### Mathilda class members

	* safe_to_fork (bool) - A flag indicating whether it is OK to fork (default: true)
	* use_shm (bool) - A flag indicating whether shared memory segments should be allocated (default: false)
	* set_cpu (bool) - A flag that tells Mathilda to try and bind to a specific CPU with sched_setaffinity (default: true)
	* slow_parallel (bool) - Forks a child process for each Instruction if true (default: false)
	* timeout_seconds (uint32_t) - The number of seconds a child process should be given before a SIGALRM is sent
	* finish(ProcessInfo *) - Callback function pointer, executed after child exits. Passed a pointer to a ProcessInfo structure

### Mathilda class misc

	* MATHILDA_FORK - Environment variable that when set tells Mathilda it is OK to fork

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
	* ssl (bool) - SSL support
	* include_headers (bool) - Include HTTP headers with the after callback (Response->text will include them)
	* use_proxy (bool) - Use the proxy configured with proxy/proxy_port
	* follow_redirects (bool) - Follow HTTP redirects
	* verbose (bool) - Enables verbose mode in Curl (this generates a lot of output)
	* curl_code (CURLCode) - The Curl response code returned after the request is finished
	* before(Instruction *, CURL *) - A callback function pointer, executed before curl_perform
	* after(Instruction *, CURL *, Response *) - Callback function pointer, executed after curl_perform

### MathildaUtils static class functions

These utility functions were written to make writing libmathilda tools easier. When working with HTTP and URIs you often need to work a collection of strings in some way. Sometimes its tokenizing them, or modifying them for fuzzing. These functions will help you do those kinds of operations using simple STL containers. The documentation for each function is auto generated using doxygen. Please see the 'docs' directory for more information.

	* name_to_addr(std::string const &, std::vector<std::string> &, bool) - Synchronous DNS lookups
	* name_to_addr_a(std::vector<std::string> const &, std::vector<std::string> &) - Asynchronous DNS lookups
	* shm_store_string(uint8_t *, const char *, size_t) - Stores a string in shared memory in [length,string] format
	* get_http_headers(const char *, std::map<std::string, std::string> &) - Returns a std::map of HTTP headers from a raw HTTP response
	* read_file(char *, std::vector<std::string> &) - Reads a file line by line into a std::vector
	* unique_string_vector(vector<std::string> &) - Removes duplicate entries from a std::vector of std::string
	* split(const std::string &, char, std::vector<std::string> &) - Splits a std::string according to a delimeter
	* replaceAll(std::string &, const std::string &, const std::string &) - Replaces all occurences of X within a std::string with Y
	* link_blacklist(std::string const &) - Checks a std::string against a blacklist of URI's
	* page_blacklist(std::string const &) - Checks a std::string against a blacklist of content
	* is_http_uri(std::string const &) - Returns true if a URI is an HTTP URI
	* is_https_uri(std::string const &) - Returns true if a URI is an HTTPS URI
	* is_subdomain(std::string const &) - Returns true if a URI is a subdomain
	* is_domain_host(std::string const &, std::string const &) - Returns true if a URI matches a domain
	* extract_host_from_uri(std::string const &) - Extracts the hostname from a URI
	* extract_path_from_uri(std::string const &) - Extracts the path from a URI
	* normalize_uri(std::string const &) - Attempts to normalize a URI

### Response Structure

This structure tracks the raw response from a web server. A pointer to the Response structure is passed to the Mathilda 'after' callback. Its members are:

	* text (char *) - A char pointer to whatever the server responded with
	* size (size_t) - The size of the data the server responded with

### WaitResult Structure

This structure is used by MathildaUtils::wait to return information about a child process to a caller. This function implements a basic wait loop for all child processes. A pointer to an instance of this structure is passed to the function and is filled out upon it returning. Its members are:

	* return_code (int) - An int representing the return code of an exited child
	* signal (int) - An int representing the signal received by the child
	* pid (pid_t) - The pid returned by waitpid

### ProcessInfo Structure

This structure is used by MathildaFork to track child processes. An instance of this structure exists for each child process the class is currently managing. A single instance of this structure also exists in the MathildaFork class instance itself but is only intended to be accessed by child processes. Its members are:

	* pid (pid_t) - The PID of the child process
	* timeout (int) - How long until the child process should receive SIGALRM
	* shm_id (int) - Shared memory key/id
	* shm_size (size_t) - Size of the shared memory segment
	* shm_ptr (uint8_t *) - Raw pointer to the shared memory segment

### Useful web classes

Mathilda ships with two very useful classes: Spider, Dirbuster. Both are self explanatory, here is how you use them:

```
#ifdef SPIDER
	cout << "Spidering..." << endl;
	std::vector<std::string> paths;
	paths.push_back("/index.php");
	std::string hh = "your-example-host.com";
	std::string d = "your-example-host.com";
	std::string cookie_file = "";
	Spider *s = new Spider(paths, hh, d, cookie_file, 80);
    s->run(3);

    for(auto x : s->links) {
        cout << "Discovered Link: " << x.c_str() << endl;
    }

    delete s;
#endif

#ifdef DIRBUSTER
    cout << "Dirbuster..." << endl;

	std::vector <std::string> pages;
	std::vector <std::string> dirs;
	const char *p = "pages.txt";
	const char *d = "dirs.txt";

	MathildaUtils::read_file((char *) p, pages);
	MathildaUtils::read_file((char *) d, dirs);

	std::vector<std::string> hh;
	hh.push_back("your-example-host.com");
	auto cookie_file = "";

    Dirbuster *dirb = new Dirbuster(hh, pages, dirs, cookie_file, 80);
    dirb->run();

	fprintf(stdout, "Dirbuster results:\n");

	for(auto pt : dirb->paths) {
		fprintf(stdout, "%s\n", pt.c_str());
	}

    delete dirb;
#endif
```

This example code is built into mathilda.cpp and can be tested using the following commands:

```
make spider
./spider
```
or

```
make dirbuster
./dirbuster
```

## Writing Mathilda callbacks

Writing Mathilda callbacks is painless as long as you make use of C++11 std::function or lambdas. Heres an example in pseudocode:

```
void my_after(Instruction *i, CURL *c, Response *r) {
	printf("Response from %s:\n%s\n", i->host.c_str(), r->text);
	return;
}
...
Mathilda *m = new Mathilda();
Instruction *i = new Instruction((char *) "example.test.example.com", (char *) "/test.html");
i->after = my_after; 	// i->after is a std::function
m->add_instruction(i);
m->execute_instructions();
```

Alternatively, you can use C++11 lambdas to do the same thing:

```
Mathilda *m = new Mathilda();
Instruction *i = new Instruction((char *) "example.test.example.com", (char *) "/test.html");
i->before = [](Instruction *i, CURL *c) {
						i->add_http_header("Host: yourhost.com");
					};
m->add_instruction(i);
m->execute_instructions();
```

This tells Mathilda to invoke the my_after function after a request has been made. You can access the Instruction, Curl, and Response members passed to the callback like any other. Please see the API documentation above for the details on what they expose.

The Instruction::before callback executes before the Curl Easy handle is passed to Curl, giving you an opportunity to override any of the settings Mathilda has set on the handle.

The Instruction::after callback executes once the Curl call is completed and gives you the opportunity to inspect the response headers or content. This is only invoked if the HTTP response code matches Instruction::response_code or if Instruction::response_code is set to 0.

The Mathilda::finish callback executes after a child process has exited and gives you the opportunity to use the shared memory segment before it is destroyed. In most cases you want to share some results across parent and child process and these are often just strings. You can use MathildaUtils::shm_store_string to make this easier. See the section 'Using Shared Memory' below for more information.

Note: If you choose not to fork (i.e. set Mathilda::safe_to_fork to false) and your connection hangs forever then you need to implement the logic to catch and recover from that. All of the neccessary libuv and libcurl handles are available for doing this. The preferred method however is to fork and set a timeout.

### Accessing Curl Internals

Inside of every Instruction object is a pointer to a [Curl Easy object](http://curl.haxx.se/libcurl/c/libcurl-easy.html). You can access this handle inside of your callback and make libcurl API calls to retrieve HTTP headers, status code, and anything else the libcurl API supports.

### Using Shared Memory

The Shared Memory segment is perhaps the most complex part of Mathilda. It is %100 optional, but can be helpful when collecting results across multiple processes that would otherwise be lost. Before fork is called, each Mathilda shm_id and shm_ptr member variable is updated to point to a shared memory segment created just for that child process. In order to use this functionality the use_shm flag must be set to true when you create your Mathilda class instance. By default you get 4MB of shared memory allocated for each child process.

You can use this shared memory to store anything, and then access it later using the finish callback which is invoked after all child processes have exited.

## FAQ:

Q: What third party requirements are there?

A: libcurl, libuv, libstdc++.x86_64 libstdc++-devel.x86_64, a modern GCC/Clang (I recommend libgumbo for HTML parsing). If you compile these from source you will likely need to set your LD_LIBRARY_PATH environment variable. You can do this with the following command:

export LD_LIBRARY_PATH=/usr/local/lib

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

Q: I specified port 443 but my program keeps trying plain text HTTP over port 443...

A: You probably set Instruction::port as 443 but you probably didn't set Instruction::ssl to true. Try something like this:

```
	if(i->port == 443) {
		i->ssl = true;
	}
```

## Who

Mathilda was written in 2015/2016 by Chris Rohlf at Yahoo

## Copyright

Copyright 2015 Yahoo Inc.
Licensed under the BSD license, see LICENSE file for terms.
Written by Chris Rohlf