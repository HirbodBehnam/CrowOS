#include "proc.h"
#include "common/lib.h"
#include "common/printf.h"
#include "cpu/asm.h"
#include "cpu/smp.h"
#include "device/rtc.h"
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
struct process *my_process(void) { return cpu_local()->running_process; }

/**
 * Unlocks the current running process's lock. This function
 * is intended to be called from assembly function "jump_to_ring3".
 */
void my_process_unlock(void) { condvar_unlock(&my_process()->lock); };

/**
 * Allocate a new process. Will return NULL on error.
 */
struct process *proc_allocate(void) {
  // Find a free process slot
  struct process *p = NULL;
  for (size_t i = 0; i < MAX_PROCESSES; i++) {
    condvar_lock(&processes[i].lock);
    if (processes[i].state == UNUSED) {
      p = &processes[i];
      break;
    }
    condvar_unlock(&processes[i].lock);
  }
  if (p == NULL) // no free slot
    return NULL;
  // Make it usable
  p->state = USED;
  p->pid = get_next_pid();
  p->exit_status = -1;
  condvar_unlock(&p->lock); // we can do an early unlock here probably
  p->pagetable = vmm_user_pagetable_new();
  if (p->pagetable == NULL) { // well shit
    p->state = UNUSED;
    p->pid = 0;
    return NULL;
  }
  p->current_sbrk = 0;
  p->initial_data_segment = 0;
  memset(&p->additional_data, 0, sizeof(p->additional_data));
  return p;
}

/**
 * Wakes up one or all processes which are waiting on a waiting channel
 */
void proc_wakeup(void *waiting_channel, bool everyone) {
  struct process *p = my_process();
  bool done = false;
  for (size_t i = 0; i < MAX_PROCESSES; i++) {
    if (p == &processes[i]) // avoid deadlock
      continue;
    condvar_lock(&processes[i].lock);
    if (processes[i].state == SLEEPING &&
        processes[i].waiting_channel == waiting_channel) {
      processes[i].state = RUNNABLE;
      // If we should wake up one thread only, then we are done
      done = !everyone;
    }
    condvar_unlock(&processes[i].lock);
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
  condvar_lock(&proc->lock);

  // Set the exit status
  proc->exit_status = exit_code;
  condvar_notify_all(&proc->lock);

  // Set the status to the exited and return back
  proc->state = EXITED;
  scheduler_switch_back();
  panic("proc_exit: scheduler returned");
}

/**
 * Waits until a process is finished and returns its exit value.
 * Will return -1 if the pid does not exist.
 *
 * Note: Because we use locks in scheduler there is no way we can
 * face a race here. However, the is the possibility that the exit
 * code gets lost. On the other hand, we give this process one round
 * in the round robin process in order for another process to wait
 * for it; If no process has waited on this process, the result will
 * be lost.
 */
int proc_wait(uint64_t target_pid) {
  struct process *target_process = NULL;

  // Look for the process with the given pid
  for (size_t i = 0; i < MAX_PROCESSES; i++) {
    condvar_lock(&processes[i].lock);
    if (processes[i].pid == target_pid) {
      target_process = &processes[i];
      break;
    }
    condvar_unlock(&processes[i].lock);
  }

  // Did we find the given process?
  if (target_process == NULL)
    return -1;

  // Wait until the status is exited
  while (target_process->state != EXITED)
    condvar_wait(&target_process->lock);

  // Copy the value to a register to prevent races
  int exit_status = target_process->exit_status;
  condvar_unlock(&target_process->lock);
  return exit_status;
}

/**
 * Allocates are deallocates memory by increasing or decreasing the top of the
 * data segment.
 */
void *proc_sbrk(int64_t how_much) {
  struct process *p = my_process();
  void *before = (void *)p->current_sbrk;

  if (how_much > 0) { // allocating memory
    p->current_sbrk =
        vmm_user_sbrk_allocate(p->pagetable, p->current_sbrk, how_much);
  } else if (how_much < 0) { // deallocating memory
    if (p->initial_data_segment <= p->current_sbrk + how_much) {
      // Do not deallocate memory which is not allocated with sbrk
      how_much = p->initial_data_segment - p->current_sbrk;
    }
    p->current_sbrk =
        vmm_user_sbrk_deallocate(p->pagetable, p->current_sbrk, -how_much);
  }
  return before;
}

/**
 * Sleep the current process for at least the number of milliseconds given as
 * the argument.
 */
void sys_sleep(uint64_t msec) {
  struct process *proc = my_process();
  const uint64_t target_epoch_wakeup = rtc_now() + msec;
  /**
   * For now, we simply use busy waiting. Just wait until we have
   * reached the desired sleep duration. In each step, we switch back
   * to the scheduler to allow other processes to run.
   */

  condvar_lock(&my_process()->lock); // lock before switch back
  while (target_epoch_wakeup > rtc_now()) {
    proc->state = RUNNABLE;  // we can run this again
    scheduler_switch_back(); // switch back to allow other processes to run
  }
  condvar_unlock(&my_process()->lock);
}

/**
 * Setup the scheduler by creating a process which runs as the very program
 */
void scheduler_init(void) {
  const char *args[] = {"/init", NULL};
  if (proc_exec("/init", args, fs_get_root()) == (uint64_t)-1)
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
  if (!spinlock_locked(&proc->lock.lock))
    panic("scheduler_switch_back: not locked");
  if (proc->state == RUNNING)
    panic("scheduler_switch_back: RUNNING");
  context_switch(kernel_stackpointer, &proc->resume_stack_pointer);
}

/**
 * Scheduler the scheduler of the operating system.
 */
void scheduler(void) {
  for (;;) { // forever...
    // The most recent process to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // processes are waiting.
    sti();
    for (size_t i = 0; i < MAX_PROCESSES; i++) { // look for processes...
      condvar_lock(&processes[i].lock);          // lock them to inspect them...
      switch (processes[i].state) {
      case RUNNABLE:
        processes[i].state = RUNNING; // which are runnable...
        // and make them running and when found
        cpu_local()->running_process = &processes[i];
        // switch to its memory space...
        install_pagetable(V2P(processes[i].pagetable));
        // load program values...
        // It is important to load the gs base in the kernel gs base
        // in order for it to be swapped with swapgs and be stored in
        // the main gs base.
        wrmsr(MSR_KERNEL_GS_BASE, processes[i].additional_data.gs_base);
        // and run it...
        context_switch(processes[i].resume_stack_pointer, &kernel_stackpointer);
        cpu_local()->running_process = NULL;
        // until we return and we do everything again!
        break;
      case EXITED:
        // If the pagetable of this process is installed, unload it
        if (get_installed_pagetable() == V2P(processes[i].pagetable))
          install_pagetable(V2P(kernel_pagetable));
        // Free the memory of the process
        vmm_user_pagetable_free(processes[i].pagetable);
        processes[i].state = UNUSED;
        processes[i].pid = 0; // do not give false positive in wait
        processes[i].resume_stack_pointer = 0;
        processes[i].current_sbrk = 0;
        processes[i].initial_data_segment = 0;
        processes[i].pagetable = NULL;
        memset(&processes[i].additional_data, 0,
               sizeof(processes[i].additional_data));
        break;
      default:
        break;
      }
      condvar_unlock(&processes[i].lock);
    }
  }
}