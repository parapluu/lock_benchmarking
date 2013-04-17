#include <stdbool.h>
#include "multi_writers_queue.h"

#ifndef SIMPLE_DELAYED_WRITERS_LOCK_H
#define SIMPLE_DELAYED_WRITERS_LOCK_H

#define NUMBER_OF_READER_GROUPS 8

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

typedef struct SimpleDelayedWritesLockImpl {
    char pad1[64];
    void (*writer)(void *) __attribute__((aligned(64)));
    char pad2[64];
    Node * endOfQueue __attribute__((aligned(64)));
    CacheLinePaddedInt readLocks[NUMBER_OF_READER_GROUPS] __attribute__((aligned(64)));
    char pad3[64];
} SimpleDelayedWritesLock;



SimpleDelayedWritesLock * sdwlock_create(void (*writer)(void *));
void sdwlock_free(SimpleDelayedWritesLock * lock);
void sdwlock_initialize(SimpleDelayedWritesLock * lock, void (*writer)(void *));
void sdwlock_register_this_thread();
void sdwlock_write(SimpleDelayedWritesLock *lock, void * writeInfo);
void sdwlock_write_read_lock(SimpleDelayedWritesLock *lock);
void sdwlock_write_read_unlock(SimpleDelayedWritesLock * lock);
void sdwlock_read_lock(SimpleDelayedWritesLock *lock);
void sdwlock_read_unlock(SimpleDelayedWritesLock *lock);

#endif
