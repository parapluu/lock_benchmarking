#include <stdbool.h>
#include "smp_utils.h"

#ifndef MULTI_WRITES_QUEUE_H
#define MULTI_WRITES_QUEUE_H

#define MWQ_CAPACITY 2048
typedef void * entry;
typedef struct MWQImpl {
    char padd1[64];
    CacheLinePaddedInt elementCount;
    entry elements[MWQ_CAPACITY];
    char padd2[64];
    int readCurrentElementIndex;
    int numOfElementsToRead;
    bool readStarted;
    bool closed;
    char padd3[64];
} MWQueue;



MWQueue * mwqueue_create();
MWQueue * mwqueue_initialize(MWQueue * queue);
void mwqueue_free(MWQueue * queue);
bool mwqueue_offer(MWQueue * queue, entry e);
entry mwqueue_take(MWQueue * queue);
void mwqueue_reset_fully_read(MWQueue * queue);

#endif
