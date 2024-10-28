/**
 * Spinlock is a very simple spinlock which locks the access to
 * a resource.
 */
struct spinlock;

/**
 * Lock the spinlock and disable interrupts
 */
void spinlock_lock(struct spinlock *lock);

/**
 * Unlock the spinlock and restores the interrupt flags
 */
void spinlock_unlock(struct spinlock *lock);