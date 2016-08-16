// Copyright 2015/2016 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#ifdef __APPLE__
	#include <string>
	#include <cstdlib>
#else
	#include <string.h>
#endif
#include <errno.h>
#include <vector>
#include <thread>
#include <algorithm>
#include <string>
#include <cstdlib>

using namespace std;
class MathildaFork;

#define OK 0
#define ERR -1

#define SHM_SIZE 1000000*8

#define DEFAULT_TIMEOUT 90

#define RET_ERR_IF_PTR_INVALID(ptr)	\
	if(ptr == NULL || ptr == (void *) -1) { 	\
		return ERR;					\
	}

#define RET_IF_PTR_INVALID(ptr)	 	\
	if(ptr == NULL || ptr == (void *) -1) { 	\
		return;						\
	}

// StringEntry
typedef struct StringEntry {
	size_t length;
} StringEntry;

typedef struct WaitResult {
	int return_code;
	int signal;
	pid_t pid;
} WaitResult;

typedef struct ProcessInfo {
	// Child process PID
	pid_t pid;
	// Shared memory ID
	int shm_id;
	// Timeout in seconds
	uint32_t timeout;
	// Shared memory size
	size_t shm_size;
	// Pointer to shared memory
	uint8_t *shm_ptr;
} ProcessInfo;

class MathildaFork {

public:
	MathildaFork() {
		parent_pid = getpid();
		parent = true;
		cores = std::thread::hardware_concurrency();
		core = 0;
		memset(&my_proc_info, 0x0, sizeof(ProcessInfo));
	}

	~MathildaFork() {
		for(auto p : children) {
			delete p;
		}
	}

	// Parent PID that created this class
	pid_t parent_pid;

	// A flag that indicates parent or child
	bool parent;

	// Store the number of processor cores
	uint32_t cores;

	// Tracks which core was bound last
	uint32_t core;

	// A vector of child PID's. Only the parent process
	// should be accessing this
	std::vector<ProcessInfo *> children;

	// This is to be populated partially by the parent
	// and partially by the child. Only the child
	// process should be accessing this
	ProcessInfo my_proc_info;

	void remove_child_pid(pid_t pid);
	int set_affinity(uint32_t c);
	int wait(WaitResult *wr);
	int get_shm_id();
	int shm_store_string(const char *str, size_t sz);
	int shm_store_string(const char *str);
	int shm_retrieve_strings(ProcessInfo *proc_info, std::vector<std::string> &strings);
	int shm_retrieve_strings(uint8_t *shm_ptr, size_t shm_size, std::vector<std::string> &strings);
	uint8_t *get_shm_ptr();
	size_t get_shm_size();
	pid_t fork_child(bool set_core);
	pid_t fork_child(bool set_core, bool use_shm, size_t sz, uint32_t timeout);
	pid_t get_pid();
	ProcessInfo *process_info_pid(pid_t pid);

private:
	void create_shm(ProcessInfo *pi);
	void create_shm(ProcessInfo *pi, size_t sz);
	void delete_shm(ProcessInfo *s);
	void connect_shm(ProcessInfo *pi);
};