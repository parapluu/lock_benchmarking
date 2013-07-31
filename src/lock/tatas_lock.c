#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include "tatas_lock.h"


TATASLock * tataslock_create(void (*writer)(void *, void **)){
    TATASLock * lock = malloc(sizeof(TATASLock));
    tataslock_initialize(lock, writer);
    return lock;
}

void tataslock_initialize(TATASLock * lock, void (*writer)(void *, void **)){
    lock->writer = writer;
    lock->lockWord.value = 0;
    __sync_synchronize();
}

void tataslock_free(TATASLock * lock){
    free(lock);
}

void tataslock_register_this_thread(){
}

void tataslock_write(TATASLock *lock, void * writeInfo) {
    tataslock_write_read_lock(lock);
    lock->writer(writeInfo, NULL);
    tataslock_write_read_unlock(lock);
}

void tataslock_write_read_lock(TATASLock *lock) {
    bool currentlylocked;
    while(true){
        load_acq(currentlylocked, lock->lockWord.value);
        while(currentlylocked){
            load_acq(currentlylocked, lock->lockWord.value);
        }
        currentlylocked = __sync_lock_test_and_set(&lock->lockWord.value, true);
        if(!currentlylocked){
            //Was not locked before operation
            return;
        }
        __sync_synchronize();//Pause instruction?
    }
}

void tataslock_write_read_unlock(TATASLock * lock) {
    __sync_lock_release(&lock->lockWord.value);
}

void tataslock_read_lock(TATASLock *lock) {
    tataslock_write_read_lock(lock);
}

void tataslock_read_unlock(TATASLock *lock) {
    tataslock_write_read_unlock(lock);
}
