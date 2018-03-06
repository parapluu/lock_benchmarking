#define MAX_NUM_OF_HELPED_OPS 4096
#include "qdlock.h"


AgnosticDXLock * adxlock_create(void (*writer)(int, int *)){
    AgnosticDXLock * lock = (AgnosticDXLock *)malloc(sizeof(AgnosticDXLock));
    adxlock_initialize(lock, writer);
    return lock;
}
void adxlock_initialize(AgnosticDXLock * lock, void (*defaultWriter)(int, int *)){
    //TODO check if the following typecast is fine
    lock->defaultWriter = defaultWriter;
    tataslock_initialize(&lock->lock, defaultWriter);
    drmvqueue_initialize(&lock->writeQueue);
    __sync_synchronize();
}
void adxlock_write_with_response(AgnosticDXLock *lock, 
                                 void (*delgateFun)(int, int *), 
                                 int data, 
                                 int * responseLocation){
    int counter = 0;
    DelegateRequestEntry e;
    e.request = delgateFun;
    e.data = data;
    e.responseLocation = responseLocation;
    do{
        if(!tataslock_is_locked(&lock->lock)){
            if(tataslock_try_write_read_lock(&lock->lock)){
#ifdef ACTIVATE_NO_CONTENTION_OPT
	        if(counter > 0){
#endif
                    drmvqueue_reset_fully_read(&lock->writeQueue);
                    delgateFun(data, responseLocation);
                    drmvqueue_flush(&lock->writeQueue);
                    tataslock_write_read_unlock(&lock->lock);
                    return;
#ifdef ACTIVATE_NO_CONTENTION_OPT
	        }else{
                    delgateFun(data, responseLocation);
		    tataslock_write_read_unlock(&lock->lock);
		    return;
                }
#endif
            }
        }else{
            while(tataslock_is_locked(&lock->lock)){
                if(drmvqueue_offer(&lock->writeQueue, e)){
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
        sched_yield();
    }while(true);
}

int adxlock_write_with_response_block(AgnosticDXLock *lock, 
                                      void (*delgateFun)(int, int *), 
                                      int data){
    int counter = 0;
    int returnValue = INT_MIN;
    int currentValue;
    adxlock_write_with_response(lock, delgateFun, data, &returnValue);
    load_acq(currentValue, returnValue);
    while(currentValue == INT_MIN){
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
void adxlock_delegate(AgnosticDXLock *lock, 
                      void (*delgateFun)(int, int *), 
                      int data) {
    adxlock_write_with_response(lock, delgateFun, data, NULL);
}
void adxlock_write(AgnosticDXLock *lock, int writeInfo) {
    adxlock_delegate(lock, lock->defaultWriter, writeInfo);
}
void adxlock_write_read_unlock(AgnosticDXLock * lock) {
    drmvqueue_flush(&lock->writeQueue);
    tataslock_write_read_unlock(&lock->lock);
}
