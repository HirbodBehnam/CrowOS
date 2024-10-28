#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "asm.h"
#include "smt.h"
#include "printf.h"

struct spinlock {
    uint32_t locked;
    // Which CPU is holding this?
    uint32_t holding_cpu;
};

/**
 * We need to save the status of interrupt flag in a stack.
 * But we actually do not need a stack because when we store the value of
 * interrupt enable flag, we clear it. So as an optimization we only store the
 * first interrupt flag and the depth of stack.
 */
static struct {
    uint16_t depth;
    bool was_enabled;
} interrupt_enable_stack[MAX_CORES];

static void save_and_disable_interrupts() {
    cli();
    uint32_t processor_id = get_processor_id();
    if (interrupt_enable_stack[processor_id].depth == 0)
        interrupt_enable_stack[processor_id].was_enabled = (read_rflags() & FLAGS_IF) != 0;
    interrupt_enable_stack[processor_id].depth++;
}

static void restore_interrupts() {
    uint32_t processor_id = get_processor_id();
    interrupt_enable_stack[processor_id].depth--;
    if (interrupt_enable_stack[processor_id].depth == 0 && interrupt_enable_stack[processor_id].was_enabled)
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

void spinlock_lock(struct spinlock *lock) {
    // Disable interrupts
    save_and_disable_interrupts();
    // Deadlock checking
    if (this_cpu_holding_lock(lock))
        panic("deadlock");
    // Wait until we get the lock
    while(__sync_lock_test_and_set(&lock->locked, 1) != 0);
    // Make a fence to prevent re-ordering
    __sync_synchronize();
    // Prevent deadlocks by saving what CPU has this lock
    lock->holding_cpu = get_processor_id();
}

void spinlock_unlock(struct spinlock *lock) {
    // Remember that we have disabled interrupts. So we should still have this lock
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

