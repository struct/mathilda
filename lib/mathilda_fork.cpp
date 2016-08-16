// Copyright 2015/2016 Yahoo Inc.
// Licensed under the BSD license, see LICENSE file for terms.
// Written by Chris Rohlf

#include "mathilda_fork.h"

/// Returns the PID for this child process
pid_t MathildaFork::get_pid() {
	return my_proc_info.pid;
}

/// Returns the shared memory ID for this child process
int MathildaFork::get_shm_id() {
	return my_proc_info.shm_id;
}

/// Returns the shared memory pointer for this child process
uint8_t *MathildaFork::get_shm_ptr() {
	return my_proc_info.shm_ptr;
}

/// Returns the size of the shared memory segment
size_t MathildaFork::get_shm_size() {
	return my_proc_info.shm_size;
}

/// Adds a string to an array of [size, string]
/// structures in shared memory
///
/// Inserts a string and its corresponding
/// length value into a list. Intended to
/// be used for parent<->child exchange of
/// data via shared memory
///
/// @param[in] str Pointer to a NULL terminated C string
int MathildaFork::shm_store_string(const char *str) {
	return shm_store_string(str, strlen(str));
}

/// Adds a string to an array of [size, string]
/// structures in shared memory
///
/// Inserts a string and its corresponding
/// length value into a list. Intended to
/// be used for parent<->child exchange of
/// data via shared memory
///
/// @param[in] str Pointer to a C string
/// @param[in] sz Size of the string
int MathildaFork::shm_store_string(const char *str, size_t sz) {
	RET_ERR_IF_PTR_INVALID(get_shm_ptr());

	if(sz == 0) {
		return 0;
	}

	if(sz > 16384) {
		sz = 16384;
	}

	StringEntry *tmp = (StringEntry *) get_shm_ptr();
	uint8_t *string = NULL;

	while(tmp->length != 0) {
		tmp = tmp + tmp->length;
	}

	if(tmp > (StringEntry *) get_shm_ptr() + get_shm_size()) {
		return ERR;
	}

	string = (uint8_t *) tmp + sizeof(StringEntry);
	tmp->length = sz;
	memcpy(string, str, sz);

	return OK;
}

/// Extracts strings from shared memory and puts
/// them into a std::vector
///
/// @param[in] proc_info A pointer to a ProcessInfo structure
/// @param[in] strings A reference to a vector of std::string
/// @return Returns the size of the vector
int MathildaFork::shm_retrieve_strings(ProcessInfo *proc_info, std::vector<std::string> &strings) {
	return shm_retrieve_strings(proc_info->shm_ptr, proc_info->shm_size, strings);
}

/// Extracts strings from shared memory and puts
/// them into a std::vector
///
/// @param[in] shm_ptr A pointer to the shared memory segment
/// @param[in] shm_size The size of the shared memory segment
/// @param[in] strings A reference to a vector of std::string
/// @return Returns the size of the vector
int MathildaFork::shm_retrieve_strings(uint8_t *shm_ptr, size_t shm_size, std::vector<std::string> &strings) {
	RET_ERR_IF_PTR_INVALID(shm_ptr);

	StringEntry *head = (StringEntry *) shm_ptr;
	char *string;

	while(head->length != 0) {
		string = (char *) head + sizeof(StringEntry);

		if(string >= (char *) (shm_ptr + shm_size)) {
			return strings.size();
		}

		if(strlen(string) == head->length) {
			strings.push_back(string);
		}

		head += head->length;
	}

	return strings.size();
}

// Sets the processor affinity
//
// Mathilda works by breaking up sets of Instructions to be
// executed by child processes. This function is called
// by child processes to bind them to a specific cpu
//
// @param[in] CPU number to bind to
// @return Returns OK or ERR from sched_setaffinity
int MathildaFork::set_affinity(uint32_t c) {
	int ret = 0;

#ifdef __linux__
	if(c >= cores) {
		c = 0;
	}

	cpu_set_t cpus;
	CPU_ZERO(&cpus);
	CPU_SET(c, &cpus);

	ret = sched_setaffinity(0, sizeof(cpus), &cpus);

	core = c;

#ifdef DEBUG
	if(ret == ERR) {
		fprintf(stdout, "[MathildaFork] Failed to bind process %d to CPU %d. Cache invalidation may occur!\n", getpid(), c);
	} else {
		fprintf(stdout, "[MathildaFork] Child (pid: %d) successfully bound to CPU %d\n", getpid(), c);
	}
#endif

#endif
	return ret;
}

// Returns the ProcessInfo structure for a PID
//
// This function iterates through all the ProcessInfo
// structures for each child it is managing and return
// a pointer to the structure for a PID
//
// @param[in] pid The PID you want a ProcessInfo structure for
// @return Returns a pointer to the ProcessInfo structure for PID
ProcessInfo *MathildaFork::process_info_pid(pid_t pid) {
	for(auto p : children) {
		if(pid == p->pid) {
			return p;
		}
	}

	return NULL;
}

// Removes a ProcessInfo structure for a given PID
//
// This function iterates through all internal ProcessInfo
// structures and finds a pointer to the one that represents
// the PID passed in. That structure is deleted
//
// @param[in] pid The PID of the process we no longer
//			  need to track
void MathildaFork::remove_child_pid(pid_t pid) {
	ProcessInfo *p = process_info_pid(pid);

	if(p != NULL) {
		delete_shm(p);
	}

	auto it = std::remove_if(children.begin(), children.end(), 
		[pid](ProcessInfo *p) {
			return p->pid == pid; 
		}
	);

	children.erase(it, children.end());
}

// Create a shared memory segment for a child process
//
// This function takes a pointer to a ProcessInfo structure
// creates a shared memory segment for it. A default size
// of SHM_SIZE is used
//
// @param[in,out] pi A pointer to a ProcessInfo structure
void MathildaFork::create_shm(ProcessInfo *pi) {
	create_shm(pi, SHM_SIZE);
}

// Create a shared memory segment for a child process
//
// This function takes a pointer to a ProcessInfo structure
// creates a shared memory segment for it.
//
// @param[in,out] pi A pointer to a ProcessInfo structure
// @param[in] sz The size of the shared memory segment. This
//			  must be greater than SHM_SIZE
void MathildaFork::create_shm(ProcessInfo *pi, size_t sz) {
	if(sz < SHM_SIZE) {
		sz = SHM_SIZE;
	}
	sz = 10000;
	pi->shm_size = sz;
	pi->shm_id = shmget(IPC_PRIVATE, sz, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

	if(pi->shm_id == ERR) {
		fprintf(stderr, "[MathildaFork] Could not allocate (%lu bytes) of shared memory. Aborting! (%s)\n", pi->shm_size, strerror(errno));
		abort();
	}

	pi->shm_ptr = (uint8_t *) shmat(pi->shm_id, 0, 0);

	if(pi->shm_ptr == (void *) ERR) {
		fprintf(stderr, "[MathildaFork] Could not get handle to shared memory for id (%d). Aborting! (%s)\n", pi->shm_id, strerror(errno));
		abort();
	}
}

// Delete a shared memory segment
//
// This function takes a pointer to a ProcessInfo
// structure and destroys the shared memory used
// by it
//
// @param[in] s A pointer to a ProcessInfo structure
void MathildaFork::delete_shm(ProcessInfo *s) {
	if(s == NULL || s->shm_id == 0 || s->shm_ptr == NULL) {
		return;
	}

	int ret = shmctl(s->shm_id, IPC_RMID, NULL);

	if(ret == ERR) {
		fprintf(stderr, "[Mathilda] Failed to free shared memory (%d). Aborting!\n(%s)\n", s->shm_id, strerror(errno));
		abort();
	}

	ret = shmdt(s->shm_ptr);

	if(ret == ERR) {
		fprintf(stderr, "[Mathilda] Failed to detach shared memory. Aborting!\n(%s)\n", strerror(errno));
		abort();
	}

	s->shm_ptr = NULL;
	s->shm_id = 0;
	s->shm_size = 0;
}

// Connect a shared memory segment given a ProcessInfo structure
//
// This is just a wrapper around shmat() that uses the information
// stored in a ProcessInfo structure
//
// @param[in] pi A pointer to a ProcessInfo structure
void MathildaFork::connect_shm(ProcessInfo *pi) {
	pi->shm_ptr = (uint8_t *) shmat(pi->shm_id, 0, 0);
}

// Forks a child process with no support for shared memory
//
// Forks a child process and handles creating the ProcessInfo
// structure associated with it. The default timeout is 90
// seconds.
//
// @param[in] set_core True if set_affinity should be called
// @return Returns the PID of the child process or ERR
pid_t MathildaFork::fork_child(bool set_core) {
	return fork_child(set_core, false, false, DEFAULT_TIMEOUT);
}

// Forks a child process with support for CPU binding and
// shared memory setup
//
// Forks a child process and handles creating the ProcessInfo
// structure associated with it. Supports shared memory creation
// via flags.
//
// @param[in] set_core True if set_affinity should be called
// @param[in] use_shm True if shared memory should be created
// @param[in] sz Size of the shared memory segment
// @return Returns the PID of the child process or ERR
pid_t MathildaFork::fork_child(bool set_core, bool use_shm, size_t sz, uint32_t timeout) {
	ProcessInfo *pi = new ProcessInfo();

	if(pi == NULL) {
#ifdef DEBUG
		fprintf(stdout, "[MathildaFork] Failed to allocate ProcessInfo. Aborting!\n");
#endif
		abort();
	}

	memset(pi, 0x0, sizeof(ProcessInfo));

	if(use_shm == true) {
		MathildaFork::create_shm(pi, sz);
	}

	pi->timeout = timeout;

	pid_t p = fork();

	if(p == ERR) {
#ifdef DEBUG
		fprintf(stdout, "[MathildaFork] Failed to fork!\n");
#endif
	} else if(p != 0) { // Parent
#ifdef DEBUG
		fprintf(stdout, "[MathildaFork %d] Child process forked\n", p);
#endif
		pi->pid = p;

		if(use_shm == true) {
			connect_shm(pi);
		}

		children.push_back(pi);

		if(set_core == true) {
			core++;

			if(core >= cores) {
				core = 0;
			}
		}
	} else {  // Child
		parent = false;

		my_proc_info.pid = getpid();
		my_proc_info.shm_id = pi->shm_id;
		my_proc_info.shm_size = pi->shm_size;

		if(set_core == true) {
			set_affinity(core);
		}

		if(use_shm == true) {
			connect_shm(&my_proc_info);
		}

		alarm(timeout);
	}

	return p;
}

// A wait loop for child processes
//
// This function makes it easier for a parent process to
// wait on any child process for a basic set of signals
// and fills out the WaitResult structure passed to it
//
// @param[in,out] wr A pointer to a WaitResult structure that
//					 indicates whether the child process has
//					 exited or received another signal
// @return Returns an pid or ERR similar to waitpid()
int MathildaFork::wait(WaitResult *wr) {
	int status = 0;

	memset(wr, 0x0, sizeof(WaitResult));

	while((wr->pid = waitpid(-1, &status, 0))) {
		if(errno == ECHILD || wr->pid == ERR) {
			return ERR;
		}

		wr->return_code = WEXITSTATUS(status);

		if(WIFEXITED(status)) {
#ifdef DEBUG
			fprintf(stdout, "[MathildaFork] Child %d exited normally with return code %d\n", wr->pid, wr->return_code);
#endif
			return wr->pid;
		}

		if(WIFSIGNALED(status)) {
			wr->signal = WTERMSIG(status);
#ifdef DEBUG
			fprintf(stdout, "[MathildaFork] Child %d was terminated by signal %d\n", wr->pid, wr->signal);
#endif
			if(WCOREDUMP(status)) {
#ifdef DEBUG
				fprintf(stdout, "[MathildaFork] Child %d core dumped\n", wr->pid);
#endif
				return wr->pid;
			}

			// This process probably timed out. Use
			// kill so we don't get a zombie process
			if(wr->signal == SIGALRM) {
#ifdef DEBUG
				fprintf(stdout, "[MathildaFork] Child %d likely timed out\n", wr->pid);
#endif
				return wr->pid;
			}
		}

		if(WIFSTOPPED(status)) {
			wr->signal = WSTOPSIG(status);
#ifdef DEBUG
			fprintf(stdout, "[MathildaFork] Child %d was stopped by signal %d\n", wr->pid, wr->signal);
#endif
		}
	}

	return wr->pid;
}