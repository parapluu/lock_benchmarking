#include <stdbool.h>
#include "utils/smp_utils.h"

#ifndef DR_MULTI_WRITES_QUEUE_H
#define DR_MULTI_WRITES_QUEUE_H

#define MWQ_CAPACITY 4048

typedef struct DelegateRequestEntryImpl {
    void (*request)(void *, void **);
    void * data;
    void ** responseLocation;
} DelegateRequestEntry;

typedef struct DRMWQImpl {
    char padd1[64];
    CacheLinePaddedBool closed;
    char padd2[64];
    CacheLinePaddedULong elementCount;
    DelegateRequestEntry elements[MWQ_CAPACITY];
    char padd3[64 - ((sizeof(DelegateRequestEntry)*MWQ_CAPACITY) % 64)];
} DRMWQueue;

DRMWQueue * drmvqueue_create();
DRMWQueue * drmvqueue_initialize(DRMWQueue * queue);
void drmvqueue_free(DRMWQueue * queue);
bool drmvqueue_offer(DRMWQueue * queue, DelegateRequestEntry e);
void drmvqueue_flush(DRMWQueue * queue);
void drmvqueue_reset_fully_read(DRMWQueue *  queue);

#endif
