#include <stdbool.h>
#include "smp_utils.h"
#include "common_lock_constants.h"
#include "mcs_lock.h"


#ifndef DRMCS_LOCK_H
#define DRMCS_LOCK_H

typedef struct DRMCSLockImpl {
    char pad1[64];
    MCSLock lock;
    CacheLinePaddedInt writeBarrier;
    CacheLinePaddedInt readLocks[NUMBER_OF_READER_GROUPS];
} DRMCSLock;


DRMCSLock * drmcslock_create(void (*writer)(void *));
void drmcslock_free(DRMCSLock * lock);
void drmcslock_initialize(DRMCSLock * lock, void (*writer)(void *));
void drmcslock_register_this_thread();
void drmcslock_write(DRMCSLock *lock, void * writeInfo);
void drmcslock_write_read_lock(DRMCSLock *lock);
void drmcslock_write_read_unlock(DRMCSLock * lock);
void drmcslock_read_lock(DRMCSLock *lock);
void drmcslock_read_unlock(DRMCSLock *lock);

#endif
