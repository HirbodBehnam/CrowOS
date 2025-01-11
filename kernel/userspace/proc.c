#include "proc.h"
#include "common/printf.h"
#include "cpu/asm.h"
#include "cpu/smp.h"
#include "userspace/exec.h"

/**
 * The kernel stackpointer which we used just before we have switched to
 * userspace.
 */
static uint64_t kernel_stackpointer;

/**
 * Next PID to assign to a program
 */
static uint64_t next_pid = 1;

/**
 * Maximum number of processes we can have
 */
#define MAX_PROCESSES 64

/**
 * List of processes in the system
 */
static struct process processes[MAX_PROCESSES];

/**
 * Which process are we running?
 */
static struct process *running_process[MAX_CORES];

/**
 * Atomically get the next PID
 */
static inline uint64_t get_next_pid(void) {
  return __atomic_fetch_add(&next_pid, 1, __ATOMIC_RELAXED);
}

// defined in snippets.S
extern void context_switch(uint64_t to_rsp, uint64_t *from_rsp);

/**
 * Gets the current running process of this CPU core
 */
struct process *my_process(void) { return running_process[get_processor_id()]; }

/**
 * Allocate a new process. Will return NULL on error.
 */
struct process *proc_allocate(void) {
  // Find a free process slot
  struct process *p = NULL;
  for (size_t i = 0; i < MAX_PROCESSES; i++) {
    if (processes[i].state == UNUSED) {
      p = &processes[i];
      break;
    }
  }
  if (p == NULL) // no free slot
    return NULL;
  // Make it usable
  p->state = USED;
  p->pid = get_next_pid();
  p->pagetable = vmm_create_user_pagetable();
  if (p->pagetable == NULL) { // well shit
    p->state = UNUSED;
    return NULL;
  }
  return p;
}

/**
 * Allocates a file descriptor of the running process.
 * This function is not thread safe and a process shall not call this
 * function twice in two different threads.
 *
 * TODO: I can make this thread safe by adding a "USED" type for each
 * open file and change the type of each selected fd to USED. (like xv6)
 */
int proc_allocate_fd(void) {
  // This is OK to be not locked because each process is
  // currently only single threaded.
  struct process *p = my_process();
  if (p == NULL)
    panic("proc_allocate_fd: no process");
  int fd = -1;
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (p->open_files[i].type == FD_EMPTY) {
      fd = i;
      break;
    }
  }
  return fd; // may be -1
}

/**
 * Setup the scheduler by creating a process which runs as the very program
 */
void scheduler_init(void) {
  if (proc_exec("/init", NULL) == (uint64_t)-1)
    panic("cannot create /init process");
  kprintf("Initialized first userprog\n");
}

/**
 * Call this function from any interrupt or syscall in each user space
 * program in order to switch back to the scheduler and schedule any other
 * program. This is like the very bare bone of the yield function.
 */
void scheduler_switch_back(void) {
  context_switch(kernel_stackpointer,
                 &running_process[get_processor_id()]->resume_stack_pointer);
}

/**
 * Scheduler the scheduler of the operating system.
 */
void scheduler(void) {
  for (;;) {                                     // forever...
    for (size_t i = 0; i < MAX_PROCESSES; i++) { // look for processes...
      if (__sync_val_compare_and_swap(&processes[i].state, RUNNABLE,
                                      RUNNING)) { // which are runnable...
        // and make them running and when found
        running_process[get_processor_id()] = &processes[i];
        // switch to its memory space...
        install_pagetable(V2P(processes[i].pagetable));
        // and run it...
        context_switch(processes->resume_stack_pointer, &kernel_stackpointer);
        running_process[get_processor_id()] = NULL;
        // until we return and we do everything again!
      }
    }
  }
}