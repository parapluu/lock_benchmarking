#include <stdbool.h>
#include "common_lock_constants.h"
#include "utils/smp_utils.h"

#ifndef TATAS_LOCK_H
#define TATAS_LOCK_H


typedef struct TATASLockImpl {
    char pad1[64];
    void (*writer)(void *, void **);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    char pad3[64];
    CacheLinePaddedBool lockWord;
    char pad4[64];
} TATASLock;



TATASLock * tataslock_create(void (*writer)(void *, void **));
void tataslock_free(TATASLock * lock);
void tataslock_initialize(TATASLock * lock, void (*writer)(void *, void **));
void tataslock_register_this_thread();
void tataslock_write(TATASLock *lock, void * writeInfo);
void tataslock_write_read_lock(TATASLock *lock);
void tataslock_write_read_unlock(TATASLock * lock);
void tataslock_read_lock(TATASLock *lock);
void tataslock_read_unlock(TATASLock *lock);

static inline
bool tataslock_is_locked(TATASLock *lock){
    bool locked;
    load_acq(locked, lock->lockWord.value);
    return locked;
}

static inline
bool tataslock_try_write_read_lock(TATASLock *lock) {
    return !__sync_lock_test_and_set(&lock->lockWord.value, true);
}

#endif
