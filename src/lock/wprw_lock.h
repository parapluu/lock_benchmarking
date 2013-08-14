#include <stdbool.h>
#include "utils/smp_utils.h"
#include "common_lock_constants.h"
#include "utils/support_many_lock_types.h"
#include "utils/support_many_non_zero_indicator_types.h"

#ifndef WPRW_LOCK_H
#define WPRW_LOCK_H

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

#else

#define LOCK_DATATYPE_NAME_WPRW NoLockDatatypeSpecified

#endif

typedef struct WPRWLockImpl {
    char pad1[64];
    LOCK_DATATYPE_NAME_WPRW lock;
    CacheLinePaddedInt writeBarrier;
    NZI_DATATYPE_NAME nonZeroIndicator;
    //    CacheLinePaddedInt readLocks[NUMBER_OF_READER_GROUPS];
} WPRWLock;

WPRWLock * wprwlock_create(void (*writer)(void *, void **));
void wprwlock_free(WPRWLock * lock);
void wprwlock_initialize(WPRWLock * lock, void (*writer)(void *, void **));
void wprwlock_register_this_thread();
void wprwlock_write(WPRWLock *lock, void * writeInfo);
void wprwlock_write_read_lock(WPRWLock *lock);
void wprwlock_write_read_unlock(WPRWLock * lock);
void wprwlock_read_lock(WPRWLock *lock);
void wprwlock_read_unlock(WPRWLock *lock);

#endif
