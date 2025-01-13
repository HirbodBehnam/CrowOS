#pragma once
#include <stdbool.h>
#include <stdint.h>
/**
 * Spinlock is a very simple spinlock which locks the access to
 * a resource. It is important to note that the CrowOS kernel is
 * not preemptible so there is not need to save the interrupts
 * and such.
 */
struct spinlock {
    uint32_t locked;
    // Which CPU is holding this?
    uint32_t holding_cpu;
};

void spinlock_lock(struct spinlock *lock);
void spinlock_unlock(struct spinlock *lock);
bool spinlock_locked(struct spinlock *lock);