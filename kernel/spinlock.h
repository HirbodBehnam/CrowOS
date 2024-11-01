#pragma once
/**
 * Spinlock is a very simple spinlock which locks the access to
 * a resource.
 */
struct spinlock {
    uint32_t locked;
    // Which CPU is holding this?
    uint32_t holding_cpu;
};

void spinlock_lock(struct spinlock *lock);
void spinlock_unlock(struct spinlock *lock);