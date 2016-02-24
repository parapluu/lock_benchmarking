#ifndef HQDLOCK_H
#define HQDLOCK_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include <limits.h>
#include "thread_identifier.h"
#include "mcs_lock.h"
#include "support_many_non_zero_indicator_types.h"
#include "datastructures/dr_multi_writers_queue.h"
#include "tatas_lock.h"

//QD Lock


typedef struct QDLockImpl {
    DRMWQueue writeQueue;
    char pad1[128];
    void (*defaultWriter)(void *, void **);
    char pad2[64 - sizeof(void * (*)(void*)) % 64];
    char pad3[128];
    TATASLock lock;
    char pad4[64];
} QDLock;

void qdlock_initialize(QDLock * lock, void (*defaultWriter)(void *, void **));
QDLock * qdlock_create(void (*defaultWriter)(void *, void **));
void qdlock_initialize(QDLock * lock, void (*defaultWriter)(void *, void **));
void qdlock_free(QDLock * lock);

//RHQDLock

typedef struct RHQDLockImpl {
    char pad2[64];
    QDLock localLocks[NUMBER_OF_NUMA_NODES];
    char pad3[64];
    MCSLock globalLock;
    char pad4[64];
    CacheLinePaddedInt writeBarrier;
    char pad5[64];
    NZI_DATATYPE_NAME nonZeroIndicator;
    char pad6[128];
} RHQDLock;

void rhqdlock_initialize(RHQDLock * lock, void (*defaultWriter)(void *, void **));
RHQDLock * rhqdlock_create(void (*writer)(void *, void **));
void rhqdlock_initialize(RHQDLock * lock, void (*defaultWriter)(void *, void **));
void rhqdlock_free(RHQDLock * lock);
void rhqdlock_register_this_thread();
static void rhqdlock_write_with_response(RHQDLock *rhqdlock, 
                                  void (*delgateFun)(void *, void **), 
                                  void * data, 
                                  void ** responseLocation);
static void * rhqdlock_write_with_response_block(RHQDLock *lock, 
                                          void (*delgateFun)(void *, void **), 
                                          void * data);
static void rhqdlock_delegate(RHQDLock *lock, 
                       void (*delgateFun)(void *, void **), 
                       void * data);
void rhqdlock_write(RHQDLock *lock, void * writeInfo);
void rhqdlock_write_read_lock(RHQDLock *lock);
void rhqdlock_write_read_unlock(RHQDLock * lock);
void rhqdlock_read_lock(RHQDLock *lock);
void rhqdlock_read_unlock(RHQDLock *lock);


static inline
void waitUntilWriteBarrierOff(RHQDLock *lock) {
    bool writeBarrierOn;
    load_acq(writeBarrierOn, lock->writeBarrier.value);    
    while(writeBarrierOn){
        __sync_synchronize();
        load_acq(writeBarrierOn, lock->writeBarrier.value);
    }
}

#ifdef PINNING
extern __thread CacheLinePaddedInt numa_node;
#endif

static inline
void rhqdlock_write_with_response(RHQDLock *rhqdlock, 
                                 void (*delgateFun)(void *, void **), 
                                 void * data, 
                                 void ** responseLocation){
#ifdef PINNING
    int my_numa_node = numa_node.value;
#else
    int my_numa_node = sched_getcpu() % NUMBER_OF_NUMA_NODES;
#endif
    QDLock * localLock = &rhqdlock->localLocks[my_numa_node];
    int counter = 0;
    DelegateRequestEntry e;
    e.request = delgateFun;
    e.data = data;
    e.responseLocation = responseLocation;
    do{
        if(!tataslock_is_locked(&localLock->lock)){
            if(tataslock_try_write_read_lock(&localLock->lock)){
                waitUntilWriteBarrierOff(rhqdlock);
                mcslock_write_read_lock(&rhqdlock->globalLock);
                drmvqueue_reset_fully_read(&localLock->writeQueue);
                NZI_WAIT_UNIL_EMPTY(&rhqdlock->nonZeroIndicator);
                delgateFun(data, responseLocation);
                drmvqueue_flush(&localLock->writeQueue);
                mcslock_write_read_unlock(&rhqdlock->globalLock);
                tataslock_write_read_unlock(&localLock->lock);
                return;
            }
        }else{
            while(tataslock_is_locked(&localLock->lock)){
                if(drmvqueue_offer(&localLock->writeQueue, e)){
                    return;
                }else{
                    __sync_synchronize();
                    __sync_synchronize();
                }
            }
        }
        if((counter & 7) == 0){
#ifdef USE_YIELD
            sched_yield();
#endif
        }
        counter = counter + 1;
    }while(true);
}

static inline
void * rhqdlock_write_with_response_block(RHQDLock *lock, 
                                      void (*delgateFun)(void *, void **), 
                                      void * data){
    int counter = 0;
    void * returnValue = NULL;
    void * currentValue;
    rhqdlock_write_with_response(lock, delgateFun, data, &returnValue);
    load_acq(currentValue, returnValue);
    while(currentValue == NULL){
        if((counter & 7) == 0){
#ifdef USE_YIELD
            sched_yield();
#endif
        }else{
            __sync_synchronize();
        }
        counter = counter + 1;
        load_acq(currentValue, returnValue);
    }
    return currentValue;
}

static inline
void rhqdlock_delegate(RHQDLock *lock, 
                      void (*delgateFun)(void *, void **), 
                      void * data) {
    rhqdlock_write_with_response(lock, delgateFun, data, NULL);
}
#endif
