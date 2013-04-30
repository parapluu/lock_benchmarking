#ifndef SUPPORT_MANY_LOCK_TYPES_H
#define SUPPORT_MANY_LOCK_TYPES_H

#ifdef LOCK_TYPE_SimpleDelayedWritesLock
//***********************************
//SimpleDelayedWritesLock
//***********************************
#include "simple_delayed_writers_lock.h"

#define LOCK_DATATYPE_NAME SimpleDelayedWritesLock
#define LOCK_FUN_PREFIX sdwlock

#elif defined (LOCK_TYPE_AllEqualRDXLock)
//***********************************
//AllEqualRDXLock
//***********************************
#include "all_equal_rdx_lock.h"

#define LOCK_DATATYPE_NAME AllEqualRDXLock
#define LOCK_FUN_PREFIX aerlock

#elif defined (LOCK_TYPE_MCSLock)
//***********************************
//MCSLock
//***********************************
#include "mcs_lock.h"

#define LOCK_DATATYPE_NAME MCSLock
#define LOCK_FUN_PREFIX mcslock

#else

#define LOCK_DATATYPE_NAME NoLockDatatypeSpecified
#define LOCK_FUN_PREFIX no_such_lock_type_prefix

#endif


#ifdef LOCK_TYPE_SimpleDelayedWritesLock

#elif defined (LOCK_TYPE_AllEqualRDXLock)

#endif


#ifdef LOCK_FUN_PREFIX

#define MY_CONCAT(a,b) a ## _ ## b                                                
#define MY_EVAL_CONCAT(a,b) MY_CONCAT(a,b)                                        
#define MY_FUN(name) MY_EVAL_CONCAT(LOCK_FUN_PREFIX, name)                              

#define LOCK_INITIALIZE(lock, writer) MY_FUN(initialize)(lock,writer)
#define LOCK_CREATE(writer) MY_FUN(create)(writer)
#define LOCK_FREE(lock) MY_FUN(free)(lock)
#define LOCK_REGISTER_THIS_THREAD() MY_FUN(register_this_thread)()
#define LOCK_WRITE(lock, writeInfo) MY_FUN(write)(lock, writeInfo)
#define LOCK_WRITE_READ_LOCK(lock) MY_FUN(write_read_lock)(lock)
#define LOCK_WRITE_READ_UNLOCK(lock) MY_FUN(write_read_unlock)(lock)
#define LOCK_READ_LOCK(lock) MY_FUN(read_lock)(lock)
#define LOCK_READ_UNLOCK(lock) MY_FUN(read_unlock)(lock)

#endif

#endif
