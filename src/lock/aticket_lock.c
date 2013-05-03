#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

#include "aticket_lock.h"

 
ATicketLock * aticketlock_create(void (*writer)(void *)){
    ATicketLock * lock = malloc(sizeof(ATicketLock));
    aticketlock_initialize(lock, writer);
    return lock;
}

void aticketlock_initialize(ATicketLock * lock, void (*writer)(void *)){
    lock->writer = writer;
    lock->inCounter.value = 0;
    lock->outCounter.value = 0;
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
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
    lock->writer(writeInfo);
    aticketlock_write_read_unlock(lock);
}

void aticketlock_write_read_lock(ATicketLock *lock) {
    int myTicket = __sync_fetch_and_add(&lock->inCounter.value, 1);
    int spinPosition = myTicket % NUMBER_OF_READER_GROUPS;
    while(ACCESS_ONCE(lock->spinAreas[spinPosition].value) != myTicket){
        __sync_synchronize();
    }
}

void aticketlock_write_read_unlock(ATicketLock * lock) {
    lock->outCounter.value = lock->outCounter.value + 1;
    int nextPosition = lock->outCounter.value % NUMBER_OF_READER_GROUPS;
    lock->spinAreas[nextPosition].value = lock->outCounter.value;
    __sync_synchronize();
}

void aticketlock_read_lock(ATicketLock *lock) {
    aticketlock_write_read_lock(lock);
}

void aticketlock_read_unlock(ATicketLock *lock) {
    aticketlock_write_read_unlock(lock);
}
