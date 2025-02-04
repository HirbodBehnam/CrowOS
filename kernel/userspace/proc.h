#pragma once
#include "common/condvar.h"
#include "fs/file.h"
#include "mem/vmm.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Each process that this process can be in
 */
enum process_state { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, EXITED };

/**
 * When we switch out or switch in the process, we shall save/load
 * the context of the program. This context is stored here. This is basically
 * a part of the stack of the program because we push our context values on
 * the stack.
 */
struct process_context {
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t rbp;
  uint64_t rbx;
  uint64_t return_address;
};

/**
 * Maximum number of files which a process can open
 */
#define MAX_OPEN_FILES 16

/**
 * Each process can be represented with this
 */
struct process {
  // The process ID
  uint64_t pid;
  // If we switch our stack pointer to this, we will resume the program
  uint64_t resume_stack_pointer;
  // The pagetable of this process
  pagetable_t pagetable;
  // Files open for this process. The index is the fd in the process.
  struct process_file open_files[MAX_OPEN_FILES];
  // Top of the initial data segment.
  uint64_t initial_data_segment;
  // The value returned by sbrk(0)
  uint64_t current_sbrk;
  // Current working directory dnode
  uint32_t working_directory;
  // The condvar which guards all variables below.
  // Programs might wait on this lock if they are using the wait
  // system call.
  struct condvar lock;
  // On what object are we waiting on if sleeping?
  void *waiting_channel;
  // The exit status of this application
  int exit_status;
  // What is going on in this process?
  enum process_state state;
};

struct process *my_process(void);
struct process *proc_allocate(void);
void proc_wakeup(void *waiting_channel, bool everyone);
int proc_allocate_fd(void);
void proc_exit(int exit_code) __attribute__((noreturn));
int proc_wait(uint64_t pid);
void *proc_sbrk(int64_t how_much);
void sys_sleep(uint64_t msec);
void scheduler_init(void);
void scheduler_switch_back(void);
void scheduler(void);