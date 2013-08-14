#include "utils/smp_utils.h"
#include <stdbool.h>
#include "utils/support_many_non_zero_indicator_types.h"

#ifndef ALL_EQUAL_RDX_LOCK_H
#define ALL_EQUAL_RDX_LOCK_H

struct FCMCSNodeImpl;

typedef union CacheLinePaddedFCMCSNodePtrImpl {
    struct FCMCSNodeImpl * value;
    char padding[64];
} CacheLinePaddedFCMCSNodePtr;

typedef struct FCMCSNodeImpl {
    char pad1[64];
    CacheLinePaddedFCMCSNodePtr next;
    CacheLinePaddedBool locked;
} FCMCSNode;

struct FlatCombNodeImpl;

typedef union CacheLinePaddedFlatCombNodePtrImpl {
    struct FlatCombNodeImpl * value;
    char padding[64];
} CacheLinePaddedFlatCombNodePtr;

typedef struct FlatCombNodeImpl {
    char pad1[64];
    struct FlatCombNodeImpl * next;
    void * request;
    unsigned long last_used;
    char pad2[64 - (2 * sizeof(void *) + sizeof(unsigned long)) % 64];
    CacheLinePaddedBool active;
    char pad3[64];
} FlatCombNode;

typedef struct FlatCombRDXLockImpl {
    char pad1[64];
    NZI_DATATYPE_NAME nonZeroIndicator;
    CacheLinePaddedInt writeBarrier;
    CacheLinePaddedFCMCSNodePtr endOfMCSQueue;
    CacheLinePaddedFlatCombNodePtr combine_list;
    void (*writer)(void *, void **);
    unsigned long combine_count;
} FlatCombRDXLock;

FlatCombRDXLock * fcrdxlock_create(void (*writer)(void *, void **));
void fcrdxlock_initialize(FlatCombRDXLock * lock, void (*writer)(void *, void **));
void fcrdxlock_free(FlatCombRDXLock * lock);

void fcrdxlock_register_this_thread();

void fcrdxlock_write(FlatCombRDXLock *lock, void * writeInfo);

void fcrdxlock_write_read_lock(FlatCombRDXLock *lock);
void fcrdxlock_write_read_unlock(FlatCombRDXLock * lock);

void fcrdxlock_read_lock(FlatCombRDXLock *lock);
void fcrdxlock_read_unlock(FlatCombRDXLock *lock);

#endif
