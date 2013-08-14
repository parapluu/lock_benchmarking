#include <stdbool.h>
#include "utils/smp_utils.h"
#include "common_lock_constants.h"
#include "mcs_lock.h"


#ifndef ATICKET_LOCK_H
#define ATICKET_LOCK_H

typedef struct ATicketLockImpl {
    char pad1[64];
    void (*writer)(void *, void **);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    CacheLinePaddedInt inCounter;
    CacheLinePaddedInt outCounter;
    CacheLinePaddedInt spinAreas[ARRAY_SIZE];
} ATicketLock;


ATicketLock * aticketlock_create(void (*writer)(void *, void **));
void aticketlock_free(ATicketLock * lock);
void aticketlock_initialize(ATicketLock * lock, void (*writer)(void *, void **));
void aticketlock_register_this_thread();
void aticketlock_write(ATicketLock *lock, void * writeInfo);
void aticketlock_write_read_lock(ATicketLock *lock);
void aticketlock_write_read_unlock(ATicketLock * lock);
void aticketlock_read_lock(ATicketLock *lock);
void aticketlock_read_unlock(ATicketLock *lock);

#endif
