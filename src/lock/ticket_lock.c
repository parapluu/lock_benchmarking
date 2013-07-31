#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

#include "ticket_lock.h"

 
TicketLock * ticketlock_create(void (*writer)(void *, void **)){
    TicketLock * lock = malloc(sizeof(TicketLock));
    ticketlock_initialize(lock, writer);
    return lock;
}

void ticketlock_initialize(TicketLock * lock, void (*writer)(void *, void **)){
    lock->writer = writer;
    lock->inCounter.value = 0;
    lock->outCounter.value = 0;
    __sync_synchronize();
}

void ticketlock_free(TicketLock * lock){
    free(lock);
}


void ticketlock_register_this_thread(){
}

void ticketlock_write(TicketLock *lock, void * writeInfo) {
    ticketlock_write_read_lock(lock);
    lock->writer(writeInfo, NULL);
    ticketlock_write_read_unlock(lock);
}

void ticketlock_write_read_lock(TicketLock *lock) {
    int outCounter;
    int myTicket = __sync_fetch_and_add(&lock->inCounter.value, 1);
    load_acq(outCounter, lock->outCounter.value);
    while(outCounter != myTicket){
        load_acq(outCounter, lock->outCounter.value);
        __sync_synchronize();
    }
}

void ticketlock_write_read_unlock(TicketLock * lock) {
    __sync_fetch_and_add(&lock->outCounter.value, 1);
}

void ticketlock_read_lock(TicketLock *lock) {
    ticketlock_write_read_lock(lock);
}

void ticketlock_read_unlock(TicketLock *lock) {
    ticketlock_write_read_unlock(lock);
}
