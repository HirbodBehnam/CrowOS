#pragma once
#include "common/spinlock.h"

/**
 * Condvar is the conventional condition variable structure found in
 * other programming languages such as Go and C++.
 * In most of these languages, condvars accept an external mutex/lock
 * to work with it. However, in CrowOS, we internally manage a spinlock
 * inside the condvar.
 * 
 * For an example of using condvar in other languages read this documents:
 * https://pkg.go.dev/sync#Cond
 * https://docs.rs/parking_lot/latest/parking_lot/struct.Condvar.html
 * https://en.cppreference.com/w/cpp/thread/condition_variable
 * https://linux.die.net/man/3/pthread_cond_wait
 */
struct condvar {
    // As said, we handle the spinlock internally.
    // You should hold the lock before using wait.
    // You can use however notify or notify_all if you are not
    // holding this lock.
    struct spinlock lock;
};

void condvar_lock(struct condvar *cond);
void condvar_unlock(struct condvar *cond);
void condvar_wait(struct condvar *cond);
void condvar_notify(struct condvar *cond);
void condvar_notify_all(struct condvar *cond);