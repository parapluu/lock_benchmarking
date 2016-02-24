#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include "tts_rdx_lock.h"
#include "utils/smp_utils.h"
#include "utils/thread_identifier.h"

#define READ_PATIENCE_LIMIT 130000
 
TTSRDXLock * ttsalock_create(void (*writer)(void *, void **)){
    TTSRDXLock * lock = malloc(sizeof(TTSRDXLock));
    ttsalock_initialize(lock, writer);
    return lock;
}

void ttsalock_initialize(TTSRDXLock * lock, void (*writer)(void *, void **)){
    lock->writer = writer;
    lock->lockWord.value = 0;
    NZI_INITIALIZE(&lock->nonZeroIndicator);
    omwqueue_initialize(&lock->writeQueue);
    __sync_synchronize();
}

void ttsalock_free(TTSRDXLock * lock){
    free(lock);
}

void ttsalock_register_this_thread(){
    assign_id_to_thread();
}

static inline
void waitUntilWriteBarrierOff(TTSRDXLock *lock) {
    bool writeBarrierOn;
    load_acq(writeBarrierOn, lock->writeBarrier.value);    
    while(writeBarrierOn){
        __sync_synchronize();
        load_acq(writeBarrierOn, lock->writeBarrier.value);
    }
}

void ttsalock_write(TTSRDXLock *lock, void * writeInfo) {
    bool currentlylocked;
    waitUntilWriteBarrierOff(lock);
    while(!omwqueue_offer(&lock->writeQueue, writeInfo)){
        load_acq(currentlylocked, lock->lockWord.value);
        if(!currentlylocked){
            currentlylocked = __sync_lock_test_and_set(&lock->lockWord.value, true);
            if(!currentlylocked){
                //Was not locked before operation
                omwqueue_reset_fully_read(&lock->writeQueue);
                NZI_WAIT_UNIL_EMPTY(&lock->nonZeroIndicator);
                lock->writer(writeInfo, NULL);
                ttsalock_write_read_unlock(lock);
                return;
            }
        }
        //A __sync_synchronize(); or a pause instruction
        //is probably necessary here to make it perform on
        //sandy
    }
}

void ttsalock_write_read_lock(TTSRDXLock *lock) {
    bool currentlylocked;
    waitUntilWriteBarrierOff(lock);
    while(true){
        load_acq(currentlylocked, lock->lockWord.value);
        while(currentlylocked){
            load_acq(currentlylocked, lock->lockWord.value);
        }
        currentlylocked = __sync_lock_test_and_set(&lock->lockWord.value, true);
        if(!currentlylocked){
            //Was not locked before operation
            omwqueue_reset_fully_read(&lock->writeQueue);
            __sync_synchronize();//Flush
            NZI_WAIT_UNIL_EMPTY(&lock->nonZeroIndicator);
            return;
        }
    }
}

void ttsalock_write_read_unlock(TTSRDXLock * lock) {
    omwqueue_flush(&lock->writeQueue, lock->writer);
    __sync_lock_release(&lock->lockWord.value);
}

void ttsalock_read_lock(TTSRDXLock *lock) {
    bool bRaised = false; 
    int readPatience = 0;
 start:
    NZI_ARRIVE(&lock->nonZeroIndicator);
    if(lock->lockWord.value){
        NZI_DEPART(&lock->nonZeroIndicator);
        while(lock->lockWord.value){
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

void ttsalock_read_unlock(TTSRDXLock *lock) {
    NZI_DEPART(&lock->nonZeroIndicator);
}
