#ifndef TATAS_LOCK_H
#define TATAS_LOCK_H

#include "misc/padded_types.h"

#include "misc/bsd_stdatomic.h"//Until c11 stdatomic.h is available
#include "misc/thread_includes.h"//Until c11 thread.h is available
#include <stdbool.h>


typedef struct TATASLockImpl {
    LLPaddedFlag lockFlag;
} TATASLock;

void tatas_initialize(TATASLock * lock){
    atomic_init( &lock->lockFlag.value, false );
}

void tatas_lock(TATASLock *lock) {
    while(true){
        while(atomic_load_explicit(&lock->lockFlag.value, 
                                   memory_order_acquire)){
            thread_yield();
        }
        if( ! atomic_flag_test_and_set_explicit(&lock->lockFlag.value,
                                                memory_order_acquire)){
            return;
        }
   }
}

void tatas_unlock(TATASLock * lock) {
    atomic_flag_clear_explicit(&lock->lockFlag.value, memory_order_release);
}

bool tatas_is_locked(TATASLock *lock){
    return atomic_load_explicit(&lock->lockFlag.value, memory_order_acquire);
}

bool tatas_try_lock(TATASLock *lock) {
    if(!atomic_load_explicit(&lock->lockFlag.value, memory_order_acquire)){
        return !atomic_flag_test_and_set_explicit(&lock->lockFlag.value, memory_order_acquire);
    } else {
        return false;
    }
}

#endif
