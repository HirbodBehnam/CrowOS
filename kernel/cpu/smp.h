#include <stdint.h>
#include "userspace/proc.h"

// Maximum number of cores we support
#define MAX_CORES 8

/**
 * Each CPU core has some kind of local storage which it should have access to.
 * This structure holds the variables which each cpu must use locally. For
 * example, the interrupt disable stack or the CPU ID or the running process of
 * each core is different from another.
 */
struct cpu_local_data {
  // Simply, the CPU ID of this CPU. The value is in range of [0, MAX_CORES)
  uint8_t cpuid;
  /**
   * We need to save the status of interrupt flag in a stack.
   * But we actually do not need a stack because when we store the value of
   * interrupt enable flag, we clear it. So as an optimization we only store the
   * first interrupt flag and the depth of stack.
   */
  struct {
    uint16_t depth;
    bool was_enabled;
  } interrupt_enable_stack;

  /**
   * The address of the local for this CPU core.
   *
   * Also, it's very important to note that this must be uint32_t pointer
   * and writes must be 32 bits wide or else, this won't work.
   */
  volatile uint32_t *lapic;

  // Current running process if any
  struct process *running_process;
};

/**
 * Gets a pointer to the local CPU structure that contains
 */
struct cpu_local_data *cpu_local(void);

/**
 * Gets the processor ID of the current process
 */
uint8_t get_processor_id(void);