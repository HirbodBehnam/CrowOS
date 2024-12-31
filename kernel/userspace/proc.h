#pragma once
#include "mem/vmm.h"
#include "fs/file.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Each process that this process can be in
 */
enum process_state { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

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
  struct fs_file open_files[MAX_OPEN_FILES];
};

struct process *my_process(void);
void scheduler_init(void);
void scheduler_switch_back(void);
void scheduler(void);