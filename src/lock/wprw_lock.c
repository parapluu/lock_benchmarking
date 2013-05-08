#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#include "wprw_lock.h"
#include "support_many_lock_types.h"
#include "smp_utils.h"

#define READ_PATIENCE_LIMIT 1000

__thread int myId __attribute__((aligned(64)));

int myIdCounter = 0;

inline
void indicateReadEnter(WPRWLock * lock){
    __sync_fetch_and_add(&lock->readLocks[myId % NUMBER_OF_READER_GROUPS].value, 1);
}

inline
void indicateReadExit(WPRWLock * lock){
    __sync_fetch_and_sub(&lock->readLocks[myId % NUMBER_OF_READER_GROUPS].value, 1);
}

inline
void waitUntilAllReadersAreGone(WPRWLock * lock){
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        while(ACCESS_ONCE(lock->readLocks[i].value) > 0){    
            __sync_synchronize();
        };
    }
}

inline
bool isWriteLocked(WPRWLock * lock){
#ifdef LOCK_TYPE_MCSLock
    return ACCESS_ONCE(lock->lock.endOfQueue.value) != NULL;
#elif defined (LOCK_TYPE_CohortLock)
    return (ACCESS_ONCE(lock->lock.globalLock.inCounter.value) != 
            ACCESS_ONCE(lock->lock.globalLock.outCounter.value));
#else
    printf("WPRW LOCK: Unsuported mutal exclusion lock\n");
    assert(false);
    return false;
#endif
}
 
WPRWLock * wprwlock_create(void (*writer)(void *)){
    WPRWLock * lock = malloc(sizeof(WPRWLock));
    wprwlock_initialize(lock, writer);
    return lock;
}

void wprwlock_initialize(WPRWLock * lock, void (*writer)(void *)){
    LOCK_INITIALIZE(&lock->lock, writer);
    lock->writeBarrier.value = 0;
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        lock->readLocks[i].value = 0;
    }
    __sync_synchronize();
}

void wprwlock_free(WPRWLock * lock){
    free(lock);
}


void wprwlock_register_this_thread(){
    LOCK_REGISTER_THIS_THREAD();
    myId = __sync_fetch_and_add(&myIdCounter, 1);
}

void wprwlock_write(WPRWLock *lock, void * writeInfo) {
    wprwlock_write_read_lock(lock);
    lock->lock.writer(writeInfo);
    wprwlock_write_read_unlock(lock);
}

void wprwlock_write_read_lock(WPRWLock *lock) {
    while(ACCESS_ONCE(lock->writeBarrier.value)){
        __sync_synchronize();
    }
    if(!LOCK_WRITE_READ_LOCK(&lock->lock)){
        waitUntilAllReadersAreGone(lock);
    }
}

void wprwlock_write_read_unlock(WPRWLock * lock) {
    LOCK_WRITE_READ_UNLOCK(&lock->lock);
}

void wprwlock_read_lock(WPRWLock *lock) {
    bool bRaised = false; 
    int readPatience = 0;
 start:
    indicateReadEnter(lock);
    if(isWriteLocked(lock)){
        indicateReadExit(lock);
        while(isWriteLocked(lock)){
            __sync_synchronize();
        }
        if((readPatience == READ_PATIENCE_LIMIT) && !bRaised){
            __sync_fetch_and_add(&lock->writeBarrier.value, 1);
            bRaised = true;
        }
        readPatience = readPatience + 1;
        goto start;
    }
    if(bRaised){
        __sync_fetch_and_sub(&lock->writeBarrier.value, 1);
    }
}

void wprwlock_read_unlock(WPRWLock *lock) {
    indicateReadExit(lock);
}
