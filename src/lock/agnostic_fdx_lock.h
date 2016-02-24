#include <stdbool.h>
#include "datastructures/dr_multi_writers_queue.h"
#include "common_lock_constants.h"
#include "utils/support_many_non_zero_indicator_types.h"
#include "utils/support_many_lock_types.h"

#ifndef AGNOSTIC_FDX_LOCK_H
#define AGNOSTIC_FDX_LOCK_H

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

struct FlatCombNodeImpl;

typedef union CacheLinePaddedFlatCombNodePtrImpl {
    struct FlatCombNodeImpl * value;
    char padding[64];
} CacheLinePaddedFlatCombNodePtr;

typedef struct FlatCombNodeImpl {
    char pad1[128];
    struct FlatCombNodeImpl * next;
    void * data;
    void ** responseLocation;
    unsigned long last_used;
    char pad2[128 - (3 * sizeof(void *) + sizeof(unsigned long)) % 64];
    void (*request)(void *, void **);
    char pad3[128 - (sizeof(void *)) % 64];
    CacheLinePaddedBool active;
    char pad4[128];
} FlatCombNode;


typedef struct AgnosticFDXLockImpl {
    CacheLinePaddedFlatCombNodePtr combineList;
    unsigned long combineCount;
    char pad1[64 - sizeof(unsigned long) % 64];
    char pad2[64];
    void (*defaultWriter)(void *, void**);
    char pad3[64 - sizeof(void * (*)(void*)) % 64];
    char pad4[128];
    LOCK_DATATYPE_NAME_WPRW lock;
    char pad5[64];
} AgnosticFDXLock;



AgnosticFDXLock * afdxlock_create(void (*writer)(void *, void **));
void afdxlock_free(AgnosticFDXLock * lock);
void afdxlock_initialize(AgnosticFDXLock * lock, void (*writer)(void *, void **));
void afdxlock_register_this_thread();
void afdxlock_write(AgnosticFDXLock *lock, void * writeInfo);
void afdxlock_write_with_response(AgnosticFDXLock *lock, 
                                  void (*writer)(void *, void **),
                                  void * data,
                                  void ** responseLocation);
void * afdxlock_write_with_response_block(AgnosticFDXLock *lock, 
                                          void (*delgateFun)(void *, void **), 
                                          void * data);
static void afdxlock_delegate(AgnosticFDXLock *lock, 
                       void (*delgateFun)(void *, void **), 
                       void * data);
void afdxlock_write_read_lock(AgnosticFDXLock *lock);
void afdxlock_write_read_unlock(AgnosticFDXLock * lock);
void afdxlock_read_lock(AgnosticFDXLock *lock);
void afdxlock_read_unlock(AgnosticFDXLock *lock);

static inline
    void afdxlock_delegate(AgnosticFDXLock *lock, void (*delgateFun)(void *, void **), void * data) {
    afdxlock_write_with_response(lock, delgateFun, data, NULL);
}
static inline
void activateFCNode(AgnosticFDXLock *lock, FlatCombNode * fcNode){
    fcNode->active.value = true;
    FlatCombNode ** pointerToOldValue = &lock->combineList.value;
    FlatCombNode * oldValue = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        fcNode->next = oldValue;
        if (__sync_bool_compare_and_swap(pointerToOldValue, oldValue, fcNode))
            return;
        oldValue = ACCESS_ONCE(*pointerToOldValue);
    }
}

#endif
