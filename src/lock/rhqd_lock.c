#define READ_PATIENCE_LIMIT 130000
#include "rhqd_lock.h"

QDLock * qdlock_create(void (*defaultWriter)(void *, void **)){
    QDLock * lock = (QDLock *)malloc(sizeof(QDLock));
    qdlock_initialize(lock, defaultWriter);
    return lock;
}

void qdlock_initialize(QDLock * lock, void (*defaultWriter)(void *, void **)){
    //TODO check if the following typecast is fine
    lock->defaultWriter = defaultWriter;
    tataslock_initialize(&lock->lock, defaultWriter);
    drmvqueue_initialize(&lock->writeQueue);
    __sync_synchronize();
}

void qdlock_free(QDLock * lock){
    free(lock);
}

//*******
//rhqdlock
//*******

void rhqdlock_initialize(RHQDLock * lock, void (*defaultWriter)(void *, void **));
RHQDLock * rhqdlock_create(void (*writer)(void *, void **)){
    RHQDLock * lock = (RHQDLock *)malloc(sizeof(RHQDLock));
    rhqdlock_initialize(lock, writer);
    return lock;
}

void rhqdlock_initialize(RHQDLock * lock, void (*defaultWriter)(void *, void **)){
    for(int n = 0; n < NUMBER_OF_NUMA_NODES; n++){
        qdlock_initialize(&lock->localLocks[n], defaultWriter);
    }
    mcslock_initialize(&lock->globalLock, defaultWriter);
    NZI_INITIALIZE(&lock->nonZeroIndicator);
    lock->writeBarrier.value = 0;
    __sync_synchronize();
}

void rhqdlock_free(RHQDLock * lock){
    free(lock);
}

void rhqdlock_register_this_thread(){
    assign_id_to_thread();
    mcslock_register_this_thread();
}



void rhqdlock_write(RHQDLock *lock, void * writeInfo) {
    rhqdlock_delegate(lock, lock->localLocks[0].defaultWriter, writeInfo);
}

void rhqdlock_write_read_lock(RHQDLock *lock) {
    waitUntilWriteBarrierOff(lock);
    mcslock_write_read_lock(&lock->globalLock);
    NZI_WAIT_UNIL_EMPTY(&lock->nonZeroIndicator);
}

void rhqdlock_write_read_unlock(RHQDLock * lock) {
    mcslock_write_read_unlock(&lock->globalLock);
}

void rhqdlock_read_lock(RHQDLock *lock) {
    bool bRaised = false; 
    int readPatience = 0;
 start:
    NZI_ARRIVE(&lock->nonZeroIndicator);
    if(mcslock_is_locked(&lock->globalLock)){
        NZI_DEPART(&lock->nonZeroIndicator);
        while(mcslock_is_locked(&lock->globalLock)){
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

void rhqdlock_read_unlock(RHQDLock *lock) {
    NZI_DEPART(&lock->nonZeroIndicator);
}
