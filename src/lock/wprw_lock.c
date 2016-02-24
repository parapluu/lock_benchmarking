#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#include "wprw_lock.h"
#include "utils/support_many_lock_types.h"
#include "utils/smp_utils.h"
#include "utils/thread_identifier.h"


#define READ_PATIENCE_LIMIT 130000

static inline
bool isWriteLocked(WPRWLock * lock){
#ifdef LOCK_TYPE_MCSLock
    MCSNode * endOfQueue;
    load_acq(endOfQueue, lock->lock.endOfQueue.value);
    return endOfQueue != NULL;
#elif defined (LOCK_TYPE_CohortLock)
    int inCounter;
    int outCounter;
    load_acq(inCounter, lock->lock.globalLock.inCounter.value);
    load_acq(outCounter, lock->lock.globalLock.outCounter.value);
    return (inCounter != outCounter);
#else
    printf("WPRW LOCK: Unsuported mutal exclusion lock\n");
    assert(false);
    return false;
#endif
}
 
WPRWLock * wprwlock_create(void (*writer)(void *, void **)){
    WPRWLock * lock = malloc(sizeof(WPRWLock));
    wprwlock_initialize(lock, writer);
    return lock;
}

void wprwlock_initialize(WPRWLock * lock, void (*writer)(void *, void **)){
    LOCK_INITIALIZE(&lock->lock, writer);
    lock->writeBarrier.value = 0;
    NZI_INITIALIZE(&lock->nonZeroIndicator);
    __sync_synchronize();
}

void wprwlock_free(WPRWLock * lock){
    free(lock);
}


void wprwlock_register_this_thread(){
    LOCK_REGISTER_THIS_THREAD();
    assign_id_to_thread();
}

void wprwlock_write(WPRWLock *lock, void * writeInfo) {
    wprwlock_write_read_lock(lock);
    lock->lock.writer(writeInfo, NULL);
    wprwlock_write_read_unlock(lock);
}

void wprwlock_write_read_lock(WPRWLock *lock) {
    bool writeBarrierOn;
    load_acq(writeBarrierOn, lock->writeBarrier.value);
    while(writeBarrierOn){
        __sync_synchronize();
        load_acq(writeBarrierOn, lock->writeBarrier.value);
    }
    if(!LOCK_WRITE_READ_LOCK(&lock->lock)){
        NZI_WAIT_UNIL_EMPTY(&lock->nonZeroIndicator);
    }
}

void wprwlock_write_read_unlock(WPRWLock * lock) {
    LOCK_WRITE_READ_UNLOCK(&lock->lock);
}

void wprwlock_read_lock(WPRWLock *lock) {
    bool bRaised = false; 
    int readPatience = 0;
 start:
    NZI_ARRIVE(&lock->nonZeroIndicator);
    if(isWriteLocked(lock)){
        NZI_DEPART(&lock->nonZeroIndicator);
        while(isWriteLocked(lock)){
            __sync_synchronize();//Pause (pause instruction might be better)
            if((readPatience == READ_PATIENCE_LIMIT) && !bRaised){
                __sync_fetch_and_add(&lock->writeBarrier.value, 1);
                bRaised = true;
            }
            readPatience = readPatience + 1;
        }
        goto start;
    }
    if(bRaised){
        __sync_fetch_and_sub(&lock->writeBarrier.value, 1);
    }
}

void wprwlock_read_unlock(WPRWLock *lock) {
    NZI_DEPART(&lock->nonZeroIndicator);
}
