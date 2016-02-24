#include <stdbool.h>
#include "datastructures/dr_multi_writers_queue.h"
#include "common_lock_constants.h"
#include "utils/support_many_non_zero_indicator_types.h"
#include "utils/support_many_lock_types.h"

#ifndef AGNOSTIC_DX_LOCK_H
#define AGNOSTIC_DX_LOCK_H

#ifdef LOCK_TYPE_WPRW_MCSLock
//***********************************
//MCSLock
//***********************************
#include "mcs_lock.h"

#define LOCK_DATATYPE_NAME_WPRW MCSLock

#elif defined (LOCK_TYPE_WPRW_CohortLock)
//***********************************
//CohortLock
//***********************************
#include "cohort_lock.h"

#define LOCK_DATATYPE_NAME_WPRW CohortLock

#elif defined (LOCK_TYPE_WPRW_TATASLock)
//***********************************
//TATASLock
//***********************************
#include "tatas_lock.h"

#define LOCK_DATATYPE_NAME_WPRW TATASLock

#else

#define LOCK_DATATYPE_NAME_WPRW NoLockDatatypeSpecified

#endif


typedef struct AgnosticDXLockImpl {
    DRMWQueue writeQueue;
    char pad1[128];
    void (*defaultWriter)(void *, void **);
    char pad2[64 - sizeof(void * (*)(void*)) % 64];
    char pad3[128];
    LOCK_DATATYPE_NAME_WPRW lock;
    char pad4[64];
} AgnosticDXLock;



AgnosticDXLock * adxlock_create(void (*writer)(void *, void **));
void adxlock_free(AgnosticDXLock * lock);
void adxlock_initialize(AgnosticDXLock * lock, void (*writer)(void *, void **));
void adxlock_register_this_thread();
void adxlock_write(AgnosticDXLock *lock, void * writeInfo);
static void adxlock_write_with_response(AgnosticDXLock *lock, 
                                 void (*writer)(void *, void **),
                                 void * data,
                                 void ** responseLocation);
static void * adxlock_write_with_response_block(AgnosticDXLock *lock, 
                                         void (*delgateFun)(void *, void **), 
                                         void * data);
static void adxlock_delegate(AgnosticDXLock *lock, 
                      void (*delgateFun)(void *, void **), 
                      void * data);
void adxlock_write_read_lock(AgnosticDXLock *lock);
void adxlock_write_read_unlock(AgnosticDXLock * lock);
void adxlock_read_lock(AgnosticDXLock *lock);
void adxlock_read_unlock(AgnosticDXLock *lock);

static inline
void adxlock_write_with_response(AgnosticDXLock *lock, 
                                 void (*delgateFun)(void *, void **), 
                                 void * data, 
                                 void ** responseLocation){
    int counter = 0;
    DelegateRequestEntry e;
    e.request = delgateFun;
    e.data = data;
    e.responseLocation = responseLocation;
    do{
        if(!LOCK_IS_LOCKED(&lock->lock)){
            if(LOCK_TRY_WRITE_READ_LOCK(&lock->lock)){
                drmvqueue_reset_fully_read(&lock->writeQueue);
                delgateFun(data, responseLocation);
                drmvqueue_flush(&lock->writeQueue);
                LOCK_WRITE_READ_UNLOCK(&lock->lock);
                return;
            }
        }else{
            for(int i = 0; i < 7;i++){
                if(drmvqueue_offer(&lock->writeQueue, e)){
                    return;
                }else{
                    __sync_synchronize();
                }
            }
        }
        if((counter & 7) == 0){
            sched_yield();
        }
        counter = counter + 1;
    }while(true);
}

static inline
void * adxlock_write_with_response_block(AgnosticDXLock *lock, 
                                         void (*delgateFun)(void *, void **), 
                                         void * data){
    int counter = 0;
    void * returnValue = NULL;
    void * currentValue = NULL;
    adxlock_write_with_response(lock, delgateFun, data, &returnValue);
    load_acq(currentValue, returnValue);
    while(currentValue == NULL){
        if((counter & 7) == 0){
            sched_yield();
        }else{
            __sync_synchronize();
        }
        counter = counter + 1;
        load_acq(currentValue, returnValue);
    }
    return currentValue;
}

static inline
void adxlock_delegate(AgnosticDXLock *lock, 
                      void (*delgateFun)(void *, void**), 
                      void * data) {
    adxlock_write_with_response(lock, delgateFun, data, NULL);
}
#endif
