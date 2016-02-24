#include <stdbool.h>
#include "utils/smp_utils.h"
#include "common_lock_constants.h"

#ifndef MCS_LOCK_H
#define MCS_LOCK_H

struct MCSNodeImpl;

typedef union CacheLinePaddedMCSNodePtrImpl {
    struct MCSNodeImpl * value;
    char padding[64];
} CacheLinePaddedMCSNodePtr;

typedef struct MCSNodeImpl {
    char pad1[64];
    CacheLinePaddedMCSNodePtr next;
    CacheLinePaddedBool locked;
} MCSNode;

typedef struct MCSLockImpl {
    char pad1[64];
    void (*writer)(void *, void **);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    CacheLinePaddedMCSNodePtr endOfQueue;
} MCSLock;



MCSLock * mcslock_create(void (*writer)(void *, void **));
void mcslock_free(MCSLock * lock);
void mcslock_initialize(MCSLock * lock, void (*writer)(void *, void **));
void mcslock_register_this_thread();
void mcslock_write(MCSLock *lock, void * writeInfo);
bool mcslock_write_read_lock(MCSLock *lock);
void mcslock_write_read_unlock(MCSLock * lock);
void mcslock_read_lock(MCSLock *lock);
void mcslock_read_unlock(MCSLock *lock);

static inline
bool mcslock_is_locked(MCSLock *lock){
    MCSNode * endOfQueue;
    load_acq(endOfQueue, lock->endOfQueue.value);
    return endOfQueue != NULL;
}

extern __thread MCSNode myMCSNode __attribute__((aligned(64)));

static inline
bool set_if_null_ptr(MCSNode ** pointerToOldValue, MCSNode * newValue){
    return __sync_bool_compare_and_swap(pointerToOldValue, NULL, newValue);
}

static inline
bool mcslock_try_write_read_lock(MCSLock *lock) {
    MCSNode * node = &myMCSNode;
    if(ACCESS_ONCE(lock->endOfQueue.value) != NULL){
        return false;
    }else{
        node->next.value = NULL;
        return set_if_null_ptr(&lock->endOfQueue.value, node);
    }
}

#endif
