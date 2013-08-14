#include <stdbool.h>
#include "utils/smp_utils.h"

#ifndef MULTI_WRITES_QUEUE_H
#define MULTI_WRITES_QUEUE_H

#define MWQ_CAPACITY 4048
typedef void * entry;
typedef struct OptiMWQImpl {
    char padd1[64];
    CacheLinePaddedBool closed;
    char padd2[64];
    CacheLinePaddedULong elementCount;
    entry elements[MWQ_CAPACITY];
    char padd3[64 - ((sizeof(entry)*MWQ_CAPACITY) % 64)];
} OptiMWQueue;



OptiMWQueue * omwqueue_create();
OptiMWQueue * omwqueue_initialize(OptiMWQueue * queue);
void omwqueue_free(OptiMWQueue * queue);
bool omwqueue_offer(OptiMWQueue * queue, entry e);
void omwqueue_flush(OptiMWQueue * queue, void (*writer)(void *, void **));
void omwqueue_reset_fully_read(OptiMWQueue * queue);

#endif
