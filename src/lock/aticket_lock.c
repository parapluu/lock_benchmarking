#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

#include "aticket_lock.h"

 
ATicketLock * aticketlock_create(void (*writer)(void *, void **)){
    ATicketLock * lock = malloc(sizeof(ATicketLock));
    aticketlock_initialize(lock, writer);
    return lock;
}

void aticketlock_initialize(ATicketLock * lock, void (*writer)(void *, void **)){
    lock->writer = writer;
    lock->inCounter.value = 0;
    lock->outCounter.value = 0;
    for(int i = 0; i < ARRAY_SIZE; i++){
        lock->spinAreas[i].value = 0;
    }
    __sync_synchronize();
}

void aticketlock_free(ATicketLock * lock){
    free(lock);
}


void aticketlock_register_this_thread(){
}

void aticketlock_write(ATicketLock *lock, void * writeInfo) {
    aticketlock_write_read_lock(lock);
    lock->writer(writeInfo, NULL);
    aticketlock_write_read_unlock(lock);
}

void aticketlock_write_read_lock(ATicketLock *lock) {
    int waitTicket;
    int myTicket = __sync_fetch_and_add(&lock->inCounter.value, 1);
    int spinPosition = myTicket % ARRAY_SIZE;
    load_acq(waitTicket, lock->spinAreas[spinPosition].value);
    while(waitTicket != myTicket){
        __sync_synchronize();
        load_acq(waitTicket, lock->spinAreas[spinPosition].value);
    }
}

void aticketlock_write_read_unlock(ATicketLock * lock) {
    lock->outCounter.value = lock->outCounter.value + 1;
    int nextPosition = lock->outCounter.value % ARRAY_SIZE;
    store_rel(lock->spinAreas[nextPosition].value, lock->outCounter.value);
    __sync_synchronize();//Push change
}

void aticketlock_read_lock(ATicketLock *lock) {
    aticketlock_write_read_lock(lock);
}

void aticketlock_read_unlock(ATicketLock *lock) {
    aticketlock_write_read_unlock(lock);
}
