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
