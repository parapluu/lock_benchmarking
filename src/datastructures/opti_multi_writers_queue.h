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
static bool omwqueue_offer(OptiMWQueue * queue, entry e);
static void omwqueue_flush(OptiMWQueue * queue, void (*writer)(void *, void **));
static void omwqueue_reset_fully_read(OptiMWQueue * queue);

static inline
unsigned long min(unsigned long i1, unsigned long i2){
    return i1 < i2 ? i1 : i2;
}
static inline
bool omwqueue_offer(OptiMWQueue * queue, entry e){
    bool closed;
    load_acq(closed, queue->closed.value);
    if(!closed){
        int index = __sync_fetch_and_add(&queue->elementCount.value, 1);
        if(index < MWQ_CAPACITY){
            store_rel(queue->elements[index], e);
            __sync_synchronize();//Flush
            return true;
        }else{
            store_rel(queue->closed.value, true);
            __sync_synchronize();//Flush
            return false;
        }
    }else{
        return false;
    }
}

static inline
void omwqueue_flush(OptiMWQueue * queue, void (*writer)(void *, void **)){
    unsigned long numOfElementsToRead;
    unsigned long newNumOfElementsToRead;
    unsigned long currentElementIndex = 0;
    bool closed = false;
    load_acq(numOfElementsToRead, queue->elementCount.value);
    if(numOfElementsToRead >= MWQ_CAPACITY){
        closed = true;
        numOfElementsToRead = MWQ_CAPACITY;
    }

    while(true){
        if(currentElementIndex < numOfElementsToRead){
            //There is definitly an element that we should read
            entry theElement;
            load_acq(theElement, queue->elements[currentElementIndex]);
            while(theElement == NULL) {
                __sync_synchronize();
                load_acq(theElement, queue->elements[currentElementIndex]);
            }
            store_rel(queue->elements[currentElementIndex], NULL);
            currentElementIndex = currentElementIndex + 1;
            writer(theElement, NULL);
        }else if (closed){
            //The queue is closed and there is no more elements that need to be read:
            return;
        }else{
            //Seems like there are no elements that should be read and the queue is
            //not closed. Check again if there are still no more elements that should
            //be read before closing the queue
            load_acq(newNumOfElementsToRead, queue->elementCount.value);
            if(newNumOfElementsToRead == numOfElementsToRead){
                //numOfElementsToRead has not changed. Close the queue.
                numOfElementsToRead = 
                    min(get_and_set_ulong(&queue->elementCount.value, MWQ_CAPACITY + 1), 
                        MWQ_CAPACITY);
                closed = true;
            }else if(newNumOfElementsToRead < MWQ_CAPACITY){
                numOfElementsToRead = newNumOfElementsToRead;
            }else{
                closed = true;
                numOfElementsToRead = MWQ_CAPACITY;
            }
        }
    }
}

static inline
void omwqueue_reset_fully_read(OptiMWQueue * queue){
    store_rel(queue->elementCount.value, 0);
    store_rel(queue->closed.value, false);
}
#endif
