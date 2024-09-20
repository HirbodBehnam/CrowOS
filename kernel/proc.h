#pragma once
#include "vmm.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Each process that this process can be in
 */
enum process_state { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

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
};

struct process *my_process(void);
void scheduler_init(void);
void scheduler_switch_back(void);
void scheduler(void);