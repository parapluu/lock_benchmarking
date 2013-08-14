#include <stdbool.h>
#include "datastructures/multi_writers_queue.h"
#include "common_lock_constants.h"
#include "utils/support_many_non_zero_indicator_types.h"

#ifndef SIMPLE_DELAYED_WRITERS_LOCK_H
#define SIMPLE_DELAYED_WRITERS_LOCK_H

struct NodeImpl;

typedef union CacheLinePaddedNodePtrImpl {
    struct NodeImpl * value;
    char padding[64];
} CacheLinePaddedNodePtr;

typedef struct NodeImpl {
    MWQueue writeQueue;
    CacheLinePaddedNodePtr next;
    CacheLinePaddedBool locked;
    bool readLockIsWriteLock;
    char pad[64 - ((sizeof(bool)) % 64)];
} Node;

typedef struct SimpleDelayedWritesLockImpl {
    char pad1[64];
    void (*writer)(void *, void **);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    CacheLinePaddedNodePtr endOfQueue;
    NZI_DATATYPE_NAME nonZeroIndicator;
} SimpleDelayedWritesLock;



SimpleDelayedWritesLock * sdwlock_create(void (*writer)(void *, void **));
void sdwlock_free(SimpleDelayedWritesLock * lock);
void sdwlock_initialize(SimpleDelayedWritesLock * lock, void (*writer)(void *, void **));
void sdwlock_register_this_thread();
void sdwlock_write(SimpleDelayedWritesLock *lock, void * writeInfo);
void sdwlock_write_read_lock(SimpleDelayedWritesLock *lock);
void sdwlock_write_read_unlock(SimpleDelayedWritesLock * lock);
void sdwlock_read_lock(SimpleDelayedWritesLock *lock);
void sdwlock_read_unlock(SimpleDelayedWritesLock *lock);

#endif
