#include <stdbool.h>
#include "utils/smp_utils.h"
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
    void (*writer)(void *, void **);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    ATicketLock globalLock;
    NodeLocalLockData localLockData[NUMBER_OF_NUMA_NODES];
} CohortLock;


CohortLock * cohortlock_create(void (*writer)(void *, void **));
void cohortlock_free(CohortLock * lock);
void cohortlock_initialize(CohortLock * lock, void (*writer)(void *, void **));
void cohortlock_register_this_thread();
void cohortlock_write(CohortLock *lock, void * writeInfo);
bool cohortlock_write_read_lock(CohortLock *lock);
void cohortlock_write_read_unlock(CohortLock * lock);
void cohortlock_read_lock(CohortLock *lock);
void cohortlock_read_unlock(CohortLock *lock);

static inline
bool cohortlock_is_locked(CohortLock *lock){
    int inCounter;
    int outCounter;
    load_acq(inCounter, lock->globalLock.inCounter.value);
    load_acq(outCounter, lock->globalLock.outCounter.value);
    return (inCounter != outCounter);
}

extern __thread CacheLinePaddedInt myLocalNode __attribute__((aligned(64)));

#ifdef PINNING
extern __thread CacheLinePaddedInt numa_node;
#endif

static inline
bool cohortlock_is_local_locked(CohortLock *lock){
    int inCounter;
    int outCounter;
#ifdef PINNING
    NodeLocalLockData * localData = &lock->localLockData[numa_node.value];
#else
    NodeLocalLockData * localData = &lock->localLockData[myLocalNode.value];
#endif 
    load_acq(inCounter, localData->lock.inCounter.value);
    load_acq(outCounter, localData->lock.outCounter.value);
    return (inCounter != outCounter);
}

static inline
bool cohortlock_try_write_read_lock(CohortLock *lock) {
    if(!cohortlock_is_locked(lock) && 
       !cohortlock_is_local_locked(lock)){
        cohortlock_write_read_lock(lock);
        return true;
    }else{
        return false;
    }
}

#endif
