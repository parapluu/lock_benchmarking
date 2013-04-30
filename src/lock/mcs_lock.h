#include <stdbool.h>
#include "smp_utils.h"
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
    void (*writer)(void *);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    CacheLinePaddedMCSNodePtr endOfQueue;
} MCSLock;



MCSLock * mcslock_create(void (*writer)(void *));
void mcslock_free(MCSLock * lock);
void mcslock_initialize(MCSLock * lock, void (*writer)(void *));
void mcslock_register_this_thread();
void mcslock_write(MCSLock *lock, void * writeInfo);
void mcslock_write_read_lock(MCSLock *lock);
void mcslock_write_read_unlock(MCSLock * lock);
void mcslock_read_lock(MCSLock *lock);
void mcslock_read_unlock(MCSLock *lock);

#endif
