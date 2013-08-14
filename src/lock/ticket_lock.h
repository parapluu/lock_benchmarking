#include <stdbool.h>
#include "utils/smp_utils.h"
#include "common_lock_constants.h"
#include "mcs_lock.h"


#ifndef TICKET_LOCK_H
#define TICKET_LOCK_H

typedef struct TicketLockImpl {
    char pad1[64];
    void (*writer)(void *, void **);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    CacheLinePaddedInt inCounter;
    CacheLinePaddedInt outCounter;
} TicketLock;


TicketLock * ticketlock_create(void (*writer)(void *, void **));
void ticketlock_free(TicketLock * lock);
void ticketlock_initialize(TicketLock * lock, void (*writer)(void *, void **));
void ticketlock_register_this_thread();
void ticketlock_write(TicketLock *lock, void * writeInfo);
void ticketlock_write_read_lock(TicketLock *lock);
void ticketlock_write_read_unlock(TicketLock * lock);
void ticketlock_read_lock(TicketLock *lock);
void ticketlock_read_unlock(TicketLock *lock);

#endif
