#include "proc.h"
#include "printf.h"

/**
 * The kernel stackpointer which we used just before we have switched to userspace.
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
static struct process *running_process = NULL;

/**
 * Atomically get the next PID
 */
static inline uint64_t get_next_pid(void) {
    return __atomic_fetch_add(&next_pid, 1, __ATOMIC_RELAXED);
}

/**
 * Allocate a process. Will return NULL on error.
 */
static struct process *process_allocate(void) {
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

// defined in snippets.S
void context_switch(uint64_t to_rsp, uint64_t *from_rsp);

/**
 * Setup the scheduler by creating a process which runs as the very program
 */
void scheduler_init(void) {
    struct process *p = process_allocate();
    if (p == NULL)
        panic("scheduler_init OOM?");
}

/**
 * Call this function from any interrupt or syscall in each user space
 * program in order to switch back to the scheduler and schedule any other program.
 * This is like the very bare bone of the yield function.
 */
void scheduler_switch_back(void) {
    context_switch(kernel_stackpointer, &running_process->resume_stack_pointer);
}

/**
 * Scheduler the scheduler of the operating system.
 */
void scheduler(void) {
    for(;;) { // forever...
        for (size_t i = 0; i < MAX_PROCESSES; i++) { // look for processes...
            if (processes[i].state == RUNNABLE) { // which are runnable...
                // and run it...
                running_process = &processes[i];
                context_switch(processes->resume_stack_pointer, &kernel_stackpointer);
                running_process = NULL;
                // until we return and we do everything again!
            }
        }
    }
}