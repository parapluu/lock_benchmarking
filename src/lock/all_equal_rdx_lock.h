#include <stdbool.h>
#include "multi_writers_queue.h"
#include "common_lock_constants.h"

#ifndef ALL_EQUAL_RDX_LOCK_H
#define ALL_EQUAL_RDX_LOCK_H


typedef struct NodeImpl {
    MWQueue writeQueue __attribute__((aligned(64)));
    struct NodeImpl * next __attribute__((aligned(64)));
    char pad1[64];
    CacheLinePaddedBool locked __attribute__((aligned(64)));
    char pad2[64];
    CacheLinePaddedBool readSpinningEnabled __attribute__((aligned(64)));
    CacheLinePaddedInt readSpinnerFlags[NUMBER_OF_READER_GROUPS] __attribute__((aligned(64)));
    char pad3[64];
    bool readLockIsWriteLock;
    bool readLockIsSpinningOnNode;
    struct NodeImpl * readLockSpinningNode;
} Node;

typedef struct AllEqualRDXLockImpl {
    char pad1[64];
    void (*writer)(void *) __attribute__((aligned(64)));
    char pad2[64];
    Node * endOfQueue __attribute__((aligned(64)));
    CacheLinePaddedInt readLocks[NUMBER_OF_READER_GROUPS] __attribute__((aligned(64)));
    char pad3[64];
} AllEqualRDXLock;



AllEqualRDXLock * aerlock_create(void (*writer)(void *));
void aerlock_free(AllEqualRDXLock * lock);
void aerlock_initialize(AllEqualRDXLock * lock, void (*writer)(void *));
void aerlock_register_this_thread();
void aerlock_write(AllEqualRDXLock *lock, void * writeInfo);
void aerlock_write_read_lock(AllEqualRDXLock *lock);
void aerlock_write_read_unlock(AllEqualRDXLock * lock);
void aerlock_read_lock(AllEqualRDXLock *lock);
void aerlock_read_unlock(AllEqualRDXLock *lock);

#endif
