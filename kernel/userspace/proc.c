#include "proc.h"
#include "common/printf.h"
#include "cpu/asm.h"
#include "cpu/smp.h"
#include "fs/fs.h"
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
 * Unlocks the current running process's lock. This function
 * is intended to be called from assembly function "jump_to_ring3".
 */
void my_process_unlock(void) { spinlock_unlock(&my_process()->lock); };

/**
 * Allocate a new process. Will return NULL on error.
 */
struct process *proc_allocate(void) {
  // Find a free process slot
  struct process *p = NULL;
  for (size_t i = 0; i < MAX_PROCESSES; i++) {
    spinlock_lock(&processes[i].lock);
    if (processes[i].state == UNUSED) {
      p = &processes[i];
      break;
    }
    spinlock_unlock(&processes[i].lock);
  }
  if (p == NULL) // no free slot
    return NULL;
  // Make it usable
  p->state = USED;
  spinlock_unlock(&p->lock); // we can do an early unlock here probably
  p->pagetable = vmm_user_pagetable_new();
  if (p->pagetable == NULL) { // well shit
    p->state = UNUSED;
    return NULL;
  }
  p->pid = get_next_pid();
  p->exit_status = -1;
  return p;
}

/**
 * Wakes up one or all processes which are waiting on a waiting channel
 */
void proc_wakeup(void *waiting_channel, bool everyone) {
  bool done = false;
  for (size_t i = 0; i < MAX_PROCESSES; i++) {
    spinlock_lock(&processes[i].lock);
    if (processes[i].state == SLEEPING &&
        processes[i].waiting_channel == waiting_channel) {
      processes[i].state = RUNNABLE;
      // If we should wake up one thread only, then we are done
      done = !everyone;
    }
    spinlock_unlock(&processes[i].lock);
    if (done)
      break;
  }
}

/**
 * Allocates a file descriptor of the running process.
 * This function is not thread safe and a process shall not call this
 * function twice in two different threads.
 *
 * Note: Because there is only one thread per process, this function
 * does not need any locks or whatsoever.
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
 * Exits from the current process ans switches back to the scheduler
 */
void proc_exit(int exit_code) {
  struct process *proc = my_process();

  // Close all files
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (proc->open_files[i].type == FD_INODE) {
      fs_close(proc->open_files[i].structures.inode);
      proc->open_files[i].type = FD_EMPTY; // I don't think I need this
    }
  }

  // Lock the process to avoid race
  spinlock_lock(&proc->lock);

  // Set the exit status
  proc->exit_status = exit_code;
  // TODO: wake up the waiters

  // Set the status to the exited and return back
  proc->state = EXITED;
  scheduler_switch_back();
  panic("proc_exit: scheduler returned");
}

/**
 * Setup the scheduler by creating a process which runs as the very program
 */
void scheduler_init(void) {
  const char *args[] = {"/init", "hello", "userspace!", NULL};
  if (proc_exec("/init", args) == (uint64_t)-1)
    panic("cannot create /init process");
  kprintf("Initialized first userprog\n");
}

/**
 * Call this function from any interrupt or syscall in each user space
 * program in order to switch back to the scheduler and schedule any other
 * program. This is like the very bare bone of the yield function.
 *
 * Before calling this function, the caller should old the my_process()->lock
 */
void scheduler_switch_back(void) {
  struct process *proc = my_process();
  if (!spinlock_locked(&proc->lock))
    panic("scheduler_switch_back: not locked");
  context_switch(kernel_stackpointer, &proc->resume_stack_pointer);
}

/**
 * Scheduler the scheduler of the operating system.
 */
void scheduler(void) {
  for (;;) {                                     // forever...
    for (size_t i = 0; i < MAX_PROCESSES; i++) { // look for processes...
      spinlock_lock(&processes[i].lock);         // lock them to inspect them...
      switch (processes[i].state) {
      case RUNNABLE:
        processes[i].state = RUNNING; // which are runnable...
        // and make them running and when found
        running_process[get_processor_id()] = &processes[i];
        // switch to its memory space...
        install_pagetable(V2P(processes[i].pagetable));
        // and run it...
        context_switch(processes[i].resume_stack_pointer, &kernel_stackpointer);
        running_process[get_processor_id()] = NULL;
        // until we return and we do everything again!
        break;
      case EXITED:
        // If the pagetable of this process is installed, unload it
        if (get_installed_pagetable() == V2P(processes[i].pagetable))
          install_pagetable(V2P(kernel_pagetable));
        // Free the memory of the process
        vmm_user_pagetable_free(processes[i].pagetable);
        processes[i].state = UNUSED;
        break;
      default:
        break;
      }
      spinlock_unlock(&processes[i].lock);
    }
  }
}