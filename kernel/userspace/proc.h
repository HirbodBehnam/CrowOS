#pragma once
#include "fs/file.h"
#include "mem/vmm.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Each process that this process can be in
 */
enum process_state { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

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
  // What is going on in this process?
  enum process_state state;
  // The pagetable of this process
  pagetable_t pagetable;
  // Files open for this process. The index is the fd in the process.
  struct process_file open_files[MAX_OPEN_FILES];
};

struct process *my_process(void);
struct process *proc_allocate(void);
int proc_allocate_fd(void);
void scheduler_init(void);
void scheduler_switch_back(void);
void scheduler(void);