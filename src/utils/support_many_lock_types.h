#ifndef SUPPORT_MANY_LOCK_TYPES_H
#define SUPPORT_MANY_LOCK_TYPES_H

#include <assert.h>

#ifdef LOCK_TYPE_SimpleDelayedWritesLock
//***********************************
//SimpleDelayedWritesLock
//***********************************
#include "lock/simple_delayed_writers_lock.h"

#define LOCK_DATATYPE_NAME SimpleDelayedWritesLock
#define LOCK_FUN_PREFIX sdwlock

#elif defined (LOCK_TYPE_AllEqualRDXLock)
//***********************************
//AllEqualRDXLock
//***********************************
#include "lock/all_equal_rdx_lock.h"

#define LOCK_DATATYPE_NAME AllEqualRDXLock
#define LOCK_FUN_PREFIX aerlock

#elif defined (LOCK_TYPE_MCSLock)
//***********************************
//MCSLock
//***********************************
#include "lock/mcs_lock.h"

#define LOCK_DATATYPE_NAME MCSLock
#define LOCK_FUN_PREFIX mcslock

#elif defined (LOCK_TYPE_WPRWLock)
//***********************************
//WPRWLock
//***********************************
#include "lock/wprw_lock.h"

#define LOCK_DATATYPE_NAME WPRWLock
#define LOCK_FUN_PREFIX wprwlock


#elif defined (LOCK_TYPE_AgnosticRDXLock)
//***********************************
//AgnosticRDXLock
//***********************************
#include "lock/agnostic_rdx_lock.h"

#define HAS_LOCK_DELEGATE_FUN 1
#define LOCK_DATATYPE_NAME AgnosticRDXLock
#define LOCK_FUN_PREFIX ardxlock

#elif defined (LOCK_TYPE_TATASLock)
//***********************************
//TATASLock
//***********************************
#include "lock/tatas_lock.h"

#define LOCK_DATATYPE_NAME TATASLock
#define LOCK_FUN_PREFIX tataslock

#elif defined (LOCK_TYPE_TicketLock)
//***********************************
//TicketLock
//***********************************
#include "lock/ticket_lock.h"

#define LOCK_DATATYPE_NAME TicketLock
#define LOCK_FUN_PREFIX ticketlock

#elif defined (LOCK_TYPE_ATicketLock)
//***********************************
//Array TicketLock
//***********************************
#include "lock/aticket_lock.h"

#define LOCK_DATATYPE_NAME ATicketLock
#define LOCK_FUN_PREFIX aticketlock

#elif defined (LOCK_TYPE_CohortLock)
//***********************************
//CohortLock
//***********************************
#include "lock/cohort_lock.h"

#define LOCK_DATATYPE_NAME CohortLock
#define LOCK_FUN_PREFIX cohortlock

#elif defined (LOCK_TYPE_FlatCombRDXLock)

#include "lock/flat_comb_rdx_lock.h"

#define LOCK_DATATYPE_NAME FlatCombRDXLock
#define LOCK_FUN_PREFIX fcrdxlock

#elif defined (LOCK_TYPE_TTSRDXLock)

#include "lock/tts_rdx_lock.h"

#define LOCK_DATATYPE_NAME TTSRDXLock
#define LOCK_FUN_PREFIX ttsalock
#elif defined (LOCK_TYPE_CPPRDX)
//***********************************
//cpp RDX lock
//***********************************
#include "lock/cpprdx.h"

#define LOCK_DATATYPE_NAME CPPRDXLock
#define LOCK_FUN_PREFIX cpprdx

#elif defined (LOCK_TYPE_AgnosticDXLock)

#include "lock/agnostic_dx_lock.h"

#define HAS_LOCK_DELEGATE_FUN 1
#define HAS_LOCK_DELEGATE_RETURN_BLOCK_FUN 1
#define LOCK_DATATYPE_NAME AgnosticDXLock
#define LOCK_FUN_PREFIX adxlock

#elif defined (LOCK_TYPE_AgnosticFDXLock)

#include "lock/agnostic_fdx_lock.h"

#define HAS_LOCK_DELEGATE_FUN 1
#define HAS_LOCK_DELEGATE_RETURN_BLOCK_FUN 1
#define LOCK_DATATYPE_NAME AgnosticFDXLock
#define LOCK_FUN_PREFIX afdxlock

#elif defined (LOCK_TYPE_RHQDLock)

#include "lock/rhqd_lock.h"

#define HAS_LOCK_DELEGATE_FUN 1
#define HAS_LOCK_DELEGATE_RETURN_BLOCK_FUN 1
#define LOCK_DATATYPE_NAME RHQDLock
#define LOCK_FUN_PREFIX rhqdlock

#elif defined (LOCK_TYPE_RCPPLock)

#include "lock/rcpp_lock.h"

#define HAS_LOCK_DELEGATE_FUN 1
#define HAS_LOCK_DELEGATE_RETURN_BLOCK_FUN 1
#define LOCK_DATATYPE_NAME RCPPLock
#define LOCK_FUN_PREFIX rcpplock

#else
#ifdef LOCK_DUMMY_FUNCTIONS

#define LOCK_DATATYPE_NAME NoLockDatatypeSpecified
#define LOCK_FUN_PREFIX no_such_lock_type_prefix

typedef struct {} NoLockDatatypeSpecified;
extern void no_such_lock_type_prefix_write_read_lock(LOCK_DATATYPE_NAME* lock);
extern void no_such_lock_type_prefix_write_read_unlock(LOCK_DATATYPE_NAME* lock);
extern void no_such_lock_type_prefix_read_lock(LOCK_DATATYPE_NAME* lock);
extern void no_such_lock_type_prefix_read_unlock(LOCK_DATATYPE_NAME* lock);
extern void* no_such_lock_type_prefix_write_with_response_block(LOCK_DATATYPE_NAME* lock, void (*delgateFun)(void *, void **), void * data); 
extern LOCK_DATATYPE_NAME* no_such_lock_type_prefix_create(void (*fun)(void*, void**));
extern void no_such_lock_type_prefix_free(LOCK_DATATYPE_NAME* lock);

#endif /* LOCK_DUMMY_FUNTIONS */

#endif

#ifdef LOCK_FUN_PREFIX

#define MY_CONCAT(a,b) a ## _ ## b                                                
#define MY_EVAL_CONCAT(a,b) MY_CONCAT(a,b)                                        
#define MY_FUN(name) MY_EVAL_CONCAT(LOCK_FUN_PREFIX, name)                              

#define LOCK_INITIALIZE(lock, writer) MY_FUN(initialize)(lock,writer)
#define LOCK_CREATE(writer) MY_FUN(create)(writer)
#define LOCK_FREE(lock) MY_FUN(free)(lock)
#define LOCK_REGISTER_THIS_THREAD() MY_FUN(register_this_thread)()
#define LOCK_DELEGATE(lock, delegateFun, data) MY_FUN(delegate)(lock, delegateFun, data)
#define LOCK_WRITE(lock, writeInfo) MY_FUN(write)(lock, writeInfo)
#define LOCK_WRITE_READ_LOCK(lock) MY_FUN(write_read_lock)(lock)
#define LOCK_TRY_WRITE_READ_LOCK(lock) MY_FUN(try_write_read_lock)(lock)
#define LOCK_WRITE_READ_UNLOCK(lock) MY_FUN(write_read_unlock)(lock)
#define LOCK_READ_LOCK(lock) MY_FUN(read_lock)(lock)
#define LOCK_READ_UNLOCK(lock) MY_FUN(read_unlock)(lock)
#define LOCK_IS_LOCKED(lock) MY_FUN(is_locked)(lock)


#ifndef HAS_LOCK_DELEGATE_RETURN_BLOCK_FUN
static inline
void * MY_FUN(write_with_response_block)(LOCK_DATATYPE_NAME *lock, 
                                         void (*delgateFun)(void *, void **), 
                                         void * data){
    void * responseValue = NULL;
    LOCK_WRITE_READ_LOCK(lock);
    delgateFun(data, &responseValue);
    LOCK_WRITE_READ_UNLOCK(lock);
    return responseValue;
}
#endif
#define LOCK_DELEGATE_RETURN_BLOCK(lock, delegateFun, data) MY_FUN(write_with_response_block)(lock, delegateFun, data)
#ifndef HAS_LOCK_DELEGATE_FUN
static inline
void MY_FUN(delegate)(LOCK_DATATYPE_NAME *lock, 
                      void (*delgateFun)(void *, void **), 
                      void * data){
    LOCK_WRITE_READ_LOCK(lock);
    delgateFun(data, NULL);
    LOCK_WRITE_READ_UNLOCK(lock);
}
#endif


#endif

#endif
