#include "condvar.h"
#include "common/printf.h"
#include "userspace/proc.h"

/**
 * Lock the lock inside this condvar.
 */
void condvar_lock(struct condvar *cond) { spinlock_lock(&cond->lock); }

/**
 * Unlock the lock inside this condvar.
 */
void condvar_unlock(struct condvar *cond) { spinlock_unlock(&cond->lock); }

/**
 * This function atomically unlocks cond.lock and suspends execution of the
 * calling kernel thread. After later resuming execution, Wait locks cond.lock
 * before returning. wait should return by notify or notify_all.
 *
 * Because cond.lock is not locked while wait is waiting, the caller typically
 * cannot assume that the condition is true when wait returns. Instead, the
 * caller should wait in a loop.
 */
void condvar_wait(struct condvar *cond) {
  // Get the current running process
  struct process *proc = my_process();
  if (proc == NULL)
    panic("condvar_wait: proc");
  /**
   * Lock the process lock and then unlock the condvar lock.
   *
   * Note: Keep the ordering this way. notify and notify_all at first lock the
   * process's lock.
   */
  spinlock_lock(&proc->lock);
  spinlock_unlock(&cond->lock);

  // Setup the sleep parameters
  proc->state = SLEEPING;
  proc->waiting_channel = &cond->lock;
  // Switch back to the kernel
  scheduler_switch_back();

  // Done sleeping!
  // NOTE: I'm not sure about this ordering. xv6 does it this way
  spinlock_unlock(&proc->lock);
  spinlock_lock(&cond->lock);
}

/**
 * Wake up one process (if there is any) which is waiting on this condvar.
 * Might be known as signal in other languages.
 *
 * It is allowed but not required for the caller to hold cond.lock during the
 * call.
 */
void condvar_notify(struct condvar *cond) { proc_wakeup(&cond->lock, false); }

/**
 * Wakes up all process (if there is any) which are waiting on this condvar.
 * Might be known as broadcast in other languages.
 *
 * It is allowed but not required for the caller to hold cond.lock during the
 * call.
 */
void condvar_notify_all(struct condvar *cond) {
  proc_wakeup(&cond->lock, true);
}