#include "proc.h"
#include "common/printf.h"
#include "cpu/smp.h"
#include "cpu/asm.h"

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
extern void context_switch(uint64_t to_rsp, uint64_t *from_rsp);
// defined in ring3.S
extern void jump_to_ring3(void);

/**
 * Gets the current running process of this CPU core
 */
struct process *my_process(void) { return running_process[get_processor_id()]; }

/**
 * Setup the scheduler by creating a process which runs as the very program
 */
void scheduler_init(void) {
  // Allocate a process
  struct process *p = process_allocate();
  if (p == NULL)
    panic("scheduler_init OOM?");
  // Map the ring3 code to the code segment
  if (vmm_map_pages(
          p->pagetable, USER_CODE_START, PAGE_SIZE, V2P(vmm_ring3init_frame()),
          (pte_permissions){.writable = 0, .executable = 1, .userspace = 1}) !=
      0)
    panic("scheduler_init code");
  // Make the trap stack in a way that the switch context would jump
  // to jump_to_ring3(). Because we have 6 global registers, we can fill the
  // stack with 6 8byte zeros and then a return address. However, because we
  // zero the registers just before jumping to userspace, we can simply ignore
  // these registers and just put a return address at the very top of stack.
  uint64_t return_address = (uint64_t)jump_to_ring3;
  install_pagetable(V2P(p->pagetable));
  *(volatile uint64_t *)(INTSTACK_VIRTUAL_ADDRESS_TOP - sizeof(uint64_t)) =
      return_address;
  // Note to myself: We can keep the pagetable. In the scheduler we will install
  // this exact pagetable again.
  // 6 registers and return value
  p->resume_stack_pointer = INTSTACK_VIRTUAL_ADDRESS_TOP - sizeof(uint64_t) * 7;
  // Then we are good. The scheduler should be able to run this program
  p->state = RUNNABLE;
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