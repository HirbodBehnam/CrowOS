#include "spinlock.h"
#include "cpu/asm.h"
#include "cpu/smp.h"
#include "printf.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Saves if the interrupts where enabled before and disables them
 */
static void save_and_disable_interrupts() {
  bool interrupts_were_enabled = is_interrupts_enabled();
  cli();
  struct cpu_local_data *cpu = cpu_local();
  if (cpu->interrupt_enable_stack.depth == 0)
    cpu->interrupt_enable_stack.was_enabled = interrupts_were_enabled;
  cpu->interrupt_enable_stack.depth++;
}

/**
 * Restores the interrupts enabled register which was saved with
 * save_and_disable_interrupts function.
 */
static void restore_interrupts() {
  struct cpu_local_data *cpu = cpu_local();
  cpu->interrupt_enable_stack.depth--;
  if (cpu->interrupt_enable_stack.depth == 0 &&
      cpu->interrupt_enable_stack.was_enabled)
    sti();
}

/**
 * Checks if this CPU is holding the given lock
 */
static bool this_cpu_holding_lock(const struct spinlock *lock) {
  bool result;
  save_and_disable_interrupts(); // do not interrupt while we are checking
  result = lock->locked && lock->holding_cpu == get_processor_id();
  restore_interrupts();
  return result;
}

/**
 * Lock the spinlock and disable interrupts
 */
void spinlock_lock(struct spinlock *lock) {
  // Disable interrupts
  save_and_disable_interrupts();
  // Deadlock checking
  if (this_cpu_holding_lock(lock))
    panic("deadlock");
  // Wait until we get the lock
  while (__sync_lock_test_and_set(&lock->locked, 1) != 0)
    ;
  // Make a fence to prevent re-ordering
  __sync_synchronize();
  // Prevent deadlocks by saving what CPU has this lock
  lock->holding_cpu = get_processor_id();
}

/**
 * Unlock the spinlock and restores the interrupt flags
 */
void spinlock_unlock(struct spinlock *lock) {
  // Remember that we have disabled interrupts. So we should still have this
  // lock
  if (!this_cpu_holding_lock(lock))
    panic("cpu not holding lock");
  lock->holding_cpu = 0;
  // Make a fence to prevent re-ordering
  __sync_synchronize();
  // Release the lock
  __sync_lock_release(&lock->locked);
  // Restore the interrupts
  restore_interrupts();
}
/**
 * Returns true if the spinlock was locked
 */
bool spinlock_locked(struct spinlock *lock) {
  // Note: I don't think we need atomic instructions here.
  // We are just reading which is atomic in x86_64
  return lock->locked;
}