#include <stdbool.h>
#include "smp_utils.h"
#include "common_lock_constants.h"
#include "ticket_lock.h"
#include "aticket_lock.h"


#ifndef COHORT_LOCK_H
#define COHORT_LOCK_H

#define MAXIMUM_NUMBER_OF_HAND_OVERS 64

typedef struct NodeLocalLockDataImpl {
    char pad1[64];
    TicketLock lock;
    CacheLinePaddedInt numberOfHandOvers;
    CacheLinePaddedBool needToTakeGlobalLock;
} NodeLocalLockData;

typedef struct CohortLockImpl {
    char pad1[64];
    void (*writer)(void *);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    ATicketLock globalLock;
    NodeLocalLockData localLockData[NUMBER_OF_NUMA_NODES];
} CohortLock;


CohortLock * cohortlock_create(void (*writer)(void *));
void cohortlock_free(CohortLock * lock);
void cohortlock_initialize(CohortLock * lock, void (*writer)(void *));
void cohortlock_register_this_thread();
void cohortlock_write(CohortLock *lock, void * writeInfo);
void cohortlock_write_read_lock(CohortLock *lock);
void cohortlock_write_read_unlock(CohortLock * lock);
void cohortlock_read_lock(CohortLock *lock);
void cohortlock_read_unlock(CohortLock *lock);

#endif
