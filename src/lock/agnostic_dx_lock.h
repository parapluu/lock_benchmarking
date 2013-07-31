#include <stdbool.h>
#include "dr_multi_writers_queue.h"
#include "common_lock_constants.h"
#include "support_many_non_zero_indicator_types.h"
#include "support_many_lock_types.h"

#ifndef AGNOSTIC_DX_LOCK_H
#define AGNOSTIC_DX_LOCK_H

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


typedef struct AgnosticDXLockImpl {
    DRMWQueue writeQueue;
    char pad1[128];
    void (*defaultWriter)(void *, void **);
    char pad2[64 - sizeof(void * (*)(void*)) % 64];
    char pad3[128];
    LOCK_DATATYPE_NAME_WPRW lock;
    char pad4[64];
} AgnosticDXLock;



AgnosticDXLock * adxlock_create(void (*writer)(void *, void **));
void adxlock_free(AgnosticDXLock * lock);
void adxlock_initialize(AgnosticDXLock * lock, void (*writer)(void *, void **));
void adxlock_register_this_thread();
void adxlock_write(AgnosticDXLock *lock, void * writeInfo);
void adxlock_write_with_response(AgnosticDXLock *lock, 
                                 void (*writer)(void *, void **),
                                 void * data,
                                 void ** responseLocation);
void * adxlock_write_with_response_block(AgnosticDXLock *lock, 
                                         void (*delgateFun)(void *, void **), 
                                         void * data);
void adxlock_delegate(AgnosticDXLock *lock, 
                      void (*delgateFun)(void *, void **), 
                      void * data);
void adxlock_write_read_lock(AgnosticDXLock *lock);
void adxlock_write_read_unlock(AgnosticDXLock * lock);
void adxlock_read_lock(AgnosticDXLock *lock);
void adxlock_read_unlock(AgnosticDXLock *lock);

#endif
