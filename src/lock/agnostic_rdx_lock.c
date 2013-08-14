#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include "agnostic_rdx_lock.h"
#include "utils/smp_utils.h"
#include "utils/thread_identifier.h"

#define READ_PATIENCE_LIMIT 130000
 
AgnosticRDXLock * ardxlock_create(void (*writer)(void *, void **)){
    AgnosticRDXLock * lock = malloc(sizeof(AgnosticRDXLock));
    ardxlock_initialize(lock, writer);
    return lock;
}

void ardxlock_initialize(AgnosticRDXLock * lock, void (*writer)(void *, void **)){
    lock->writer = writer;
    LOCK_INITIALIZE(&lock->lock, writer);
    NZI_INITIALIZE(&lock->nonZeroIndicator);
    omwqueue_initialize(&lock->writeQueue);
    __sync_synchronize();
}

void ardxlock_free(AgnosticRDXLock * lock){
    free(lock);
}

void ardxlock_register_this_thread(){
    assign_id_to_thread();
}

inline
void waitUntilWriteBarrierOff(AgnosticRDXLock *lock) {
    bool writeBarrierOn;
    load_acq(writeBarrierOn, lock->writeBarrier.value);    
    while(writeBarrierOn){
        __sync_synchronize();
        load_acq(writeBarrierOn, lock->writeBarrier.value);
    }
}

void ardxlock_write(AgnosticRDXLock *lock, void * writeInfo) {
    waitUntilWriteBarrierOff(lock);
    while(!omwqueue_offer(&lock->writeQueue, writeInfo)){
        if(!LOCK_IS_LOCKED(&lock->lock)){
            if(LOCK_TRY_WRITE_READ_LOCK(&lock->lock)){
                omwqueue_reset_fully_read(&lock->writeQueue);
                NZI_WAIT_UNIL_EMPTY(&lock->nonZeroIndicator);
                lock->writer(writeInfo, NULL);
                omwqueue_flush(&lock->writeQueue, lock->writer);
                LOCK_WRITE_READ_UNLOCK(&lock->lock);
                return;
            }
        }
        //__sync_synchronize(); or a pause instruction
        //is probably necessary here to make it perform on
        //sandy
    }
}

void ardxlock_write_read_lock(AgnosticRDXLock *lock) {
    waitUntilWriteBarrierOff(lock);
    LOCK_WRITE_READ_LOCK(&lock->lock);    
    omwqueue_reset_fully_read(&lock->writeQueue);
    __sync_synchronize();//Flush
    NZI_WAIT_UNIL_EMPTY(&lock->nonZeroIndicator);
}

void ardxlock_write_read_unlock(AgnosticRDXLock * lock) {
    omwqueue_flush(&lock->writeQueue, lock->writer);
    LOCK_WRITE_READ_UNLOCK(&lock->lock);
}

void ardxlock_read_lock(AgnosticRDXLock *lock) {
    bool bRaised = false; 
    int readPatience = 0;
 start:
    NZI_ARRIVE(&lock->nonZeroIndicator);
    if(LOCK_IS_LOCKED(&lock->lock)){
        NZI_DEPART(&lock->nonZeroIndicator);
        while(LOCK_IS_LOCKED(&lock->lock)){
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

void ardxlock_read_unlock(AgnosticRDXLock *lock) {
    NZI_DEPART(&lock->nonZeroIndicator);
}
