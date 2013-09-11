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
void rhqdlock_write_with_response(RHQDLock *rhqdlock, 
                                  void (*delgateFun)(void *, void **), 
                                  void * data, 
                                  void ** responseLocation);
void * rhqdlock_write_with_response_block(RHQDLock *lock, 
                                          void (*delgateFun)(void *, void **), 
                                          void * data);
void rhqdlock_delegate(RHQDLock *lock, 
                       void (*delgateFun)(void *, void **), 
                       void * data);
void rhqdlock_write(RHQDLock *lock, void * writeInfo);
void rhqdlock_write_read_lock(RHQDLock *lock);
void rhqdlock_write_read_unlock(RHQDLock * lock);
void rhqdlock_read_lock(RHQDLock *lock);
void rhqdlock_read_unlock(RHQDLock *lock);



#endif
