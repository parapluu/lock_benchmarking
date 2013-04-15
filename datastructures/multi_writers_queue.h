#include <stdbool.h>

#ifndef MULTI_WRITES_QUEUE_H
#define MULTI_WRITES_QUEUE_H

#define MWQ_CAPACITY 2048
typedef void * entry;
typedef struct MWQImpl {
    entry elements[MWQ_CAPACITY];
    int elementCount;
    int readCurrentElementIndex;
    int numOfElementsToRead;
    bool readStarted;
    bool closed;
} MWQueue;



MWQueue * mwqueue_create();
MWQueue * mwqueue_initialize(MWQueue * queue);
void mwqueue_free(MWQueue * queue);
bool mwqueue_offer(MWQueue * queue, entry e);
entry mwqueue_take(MWQueue * queue);
void mwqueue_reset_fully_read(MWQueue * queue);

#endif
