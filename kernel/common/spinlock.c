#include <stddef.h>
#include <stdbool.h>
#include "cpu/asm.h"
#include "cpu/smp.h"
#include "spinlock.h"
#include "printf.h"

/**
 * Checks if this CPU is holding the given lock
 */
static bool this_cpu_holding_lock(const struct spinlock *lock) {
    return lock->locked && lock->holding_cpu == get_processor_id();
}

/**
 * Lock the spinlock
 */
void spinlock_lock(struct spinlock *lock) {
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

/**
 * Unlock the spinlock
 */
void spinlock_unlock(struct spinlock *lock) {
    // Remember that we have disabled interrupts. So we should still have this lock
    if (!this_cpu_holding_lock(lock))
        panic("cpu not holding lock");
    lock->holding_cpu = 0;
    // Make a fence to prevent re-ordering
    __sync_synchronize();
    // Release the lock
    __sync_lock_release(&lock->locked);
}

/**
 * Returns true if the spinlock was locked
 */
bool spinlock_locked(struct spinlock *lock) {
    // Note: I don't think we need atomic instructions here.
    // We are just reading which is atomic in x86_64
    return lock->locked;
}