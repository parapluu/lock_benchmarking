#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include "agnostic_dx_lock.h"
#include "utils/smp_utils.h"

#define READ_PATIENCE_LIMIT 130000
 
AgnosticDXLock * adxlock_create(void (*writer)(void *, void **)){
    AgnosticDXLock * lock = malloc(sizeof(AgnosticDXLock));
    adxlock_initialize(lock, writer);
    return lock;
}

void adxlock_initialize(AgnosticDXLock * lock, void (*defaultWriter)(void *, void **)){
    //TODO check if the following typecast is fine
    lock->defaultWriter = defaultWriter;
    LOCK_INITIALIZE(&lock->lock, defaultWriter);
    drmvqueue_initialize(&lock->writeQueue);
    __sync_synchronize();
}

void adxlock_free(AgnosticDXLock * lock){
    free(lock);
}

void adxlock_register_this_thread(){
}

inline
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

inline
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

inline
void adxlock_delegate(AgnosticDXLock *lock, 
                      void (*delgateFun)(void *, void**), 
                      void * data) {
    adxlock_write_with_response(lock, delgateFun, data, NULL);
}

void adxlock_write(AgnosticDXLock *lock, void * writeInfo) {
    adxlock_delegate(lock, lock->defaultWriter, writeInfo);
}

void adxlock_write_read_lock(AgnosticDXLock *lock) {
    LOCK_WRITE_READ_LOCK(&lock->lock);    
    drmvqueue_reset_fully_read(&lock->writeQueue);
    __sync_synchronize();//Flush
}

void adxlock_write_read_unlock(AgnosticDXLock * lock) {
    drmvqueue_flush(&lock->writeQueue);
    LOCK_WRITE_READ_UNLOCK(&lock->lock);
}

void adxlock_read_lock(AgnosticDXLock *lock) {
    adxlock_write_read_lock(lock);
}

void adxlock_read_unlock(AgnosticDXLock *lock) {
    adxlock_write_read_unlock(lock);
}
