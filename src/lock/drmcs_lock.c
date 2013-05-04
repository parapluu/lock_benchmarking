#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>


#include "drmcs_lock.h"
#include "mcs_lock.h"
#include "smp_utils.h"

#define READ_PATIENCE_LIMIT 1000

__thread int myId __attribute__((aligned(64)));

int myIdCounter = 0;

inline
void indicateReadEnter(DRMCSLock * lock){
    __sync_fetch_and_add(&lock->readLocks[myId % NUMBER_OF_READER_GROUPS].value, 1);
}

inline
void indicateReadExit(DRMCSLock * lock){
    __sync_fetch_and_sub(&lock->readLocks[myId % NUMBER_OF_READER_GROUPS].value, 1);
}

inline
void waitUntilAllReadersAreGone(DRMCSLock * lock){
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        while(ACCESS_ONCE(lock->readLocks[i].value) > 0){    
            __sync_synchronize();
        };
    }
}

inline
bool isWriteLocked(DRMCSLock * lock){
    return ACCESS_ONCE(lock->lock.endOfQueue.value) != NULL;
}
 
DRMCSLock * drmcslock_create(void (*writer)(void *)){
    DRMCSLock * lock = malloc(sizeof(DRMCSLock));
    drmcslock_initialize(lock, writer);
    return lock;
}

void drmcslock_initialize(DRMCSLock * lock, void (*writer)(void *)){
    mcslock_initialize(&lock->lock, writer);
    lock->writeBarrier.value = 0;
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        lock->readLocks[i].value = 0;
    }
    __sync_synchronize();
}

void drmcslock_free(DRMCSLock * lock){
    free(lock);
}


void drmcslock_register_this_thread(){
    mcslock_register_this_thread();
    myId = __sync_fetch_and_add(&myIdCounter, 1);
}

void drmcslock_write(DRMCSLock *lock, void * writeInfo) {
    drmcslock_write_read_lock(lock);
    lock->lock.writer(writeInfo);
    drmcslock_write_read_unlock(lock);
}

void drmcslock_write_read_lock(DRMCSLock *lock) {
    while(ACCESS_ONCE(lock->writeBarrier.value)){
        __sync_synchronize();
    }
    if(!mcslock_write_read_lock(&lock->lock)){
        waitUntilAllReadersAreGone(lock);
    }
}

void drmcslock_write_read_unlock(DRMCSLock * lock) {
    mcslock_write_read_unlock(&lock->lock);
}

void drmcslock_read_lock(DRMCSLock *lock) {
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

void drmcslock_read_unlock(DRMCSLock *lock) {
    indicateReadExit(lock);
}
