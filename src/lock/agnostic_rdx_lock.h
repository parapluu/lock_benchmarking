#include <stdbool.h>
#include "datastructures/dr_multi_writers_queue.h"
#include "common_lock_constants.h"
#include "utils/support_many_non_zero_indicator_types.h"
#include "utils/support_many_lock_types.h"

#ifndef AGNOSTIC_RDX_LOCK_H
#define AGNOSTIC_RDX_LOCK_H

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

#elif defined (LOCK_TYPE_WPRW_TATASLock)
//***********************************
//TATASLock
//***********************************
#include "tatas_lock.h"

#define LOCK_DATATYPE_NAME_WPRW TATASLock

#else

#define LOCK_DATATYPE_NAME_WPRW NoLockDatatypeSpecified

#endif


typedef struct AgnosticRDXLockImpl {
    DRMWQueue writeQueue;
    char pad1[64];
    void (*writer)(void *, void **);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    char pad3[64];
    CacheLinePaddedInt writeBarrier;
    LOCK_DATATYPE_NAME_WPRW lock;
    char pad4[64];
    NZI_DATATYPE_NAME nonZeroIndicator;
} AgnosticRDXLock;



AgnosticRDXLock * ardxlock_create(void (*writer)(void *, void **));
void ardxlock_free(AgnosticRDXLock * lock);
void ardxlock_initialize(AgnosticRDXLock * lock, void (*writer)(void *, void **));
void ardxlock_register_this_thread();
void ardxlock_write_with_response(AgnosticRDXLock *lock, void (*delgateFun)(void *, void **), void * data, void ** responseLocation);
void ardxlock_delegate(AgnosticRDXLock *lock, void (*delgateFun)(void *, void**), void * data);
void ardxlock_write(AgnosticRDXLock *lock, void * writeInfo);
void ardxlock_write_read_lock(AgnosticRDXLock *lock);
void ardxlock_write_read_unlock(AgnosticRDXLock * lock);
void ardxlock_read_lock(AgnosticRDXLock *lock);
void ardxlock_read_unlock(AgnosticRDXLock *lock);

#endif
