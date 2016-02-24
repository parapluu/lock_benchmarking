#include <stdbool.h>
#include "utils/smp_utils.h"

#ifndef DR_MULTI_WRITES_QUEUE_H
#define DR_MULTI_WRITES_QUEUE_H

#define MWQ_CAPACITY 4048

#ifdef CAS_FETCH_AND_ADD
#define FETCH_AND_ADD(valueAddress, incrementWith) CAS_fetch_and_add(valueAddress, incrementWith) 
#else
#define FETCH_AND_ADD(valueAddress, incrementWith) __sync_fetch_and_add(valueAddress, incrementWith) 
#endif 
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

static DRMWQueue * drmvqueue_create();
static DRMWQueue * drmvqueue_initialize(DRMWQueue * queue);
static void drmvqueue_free(DRMWQueue * queue);
static bool drmvqueue_offer(DRMWQueue * queue, DelegateRequestEntry e);
static void drmvqueue_flush(DRMWQueue * queue);
static void drmvqueue_reset_fully_read(DRMWQueue *  queue);

static inline
int CAS_fetch_and_add(int * valueAddress, int incrementWith){
    int oldValCAS;
    int oldVal = ACCESS_ONCE(*valueAddress);
    while(true){
        oldValCAS = __sync_val_compare_and_swap(valueAddress, oldVal, oldVal + incrementWith);
        if(oldVal == oldValCAS){
            return oldVal;
        }else{
            oldVal = oldValCAS;
        }
    }
}
static inline
unsigned long min(unsigned long i1, unsigned long i2){
    return i1 < i2 ? i1 : i2;
}

DRMWQueue * drmvqueue_create(){
    DRMWQueue * queue = malloc(sizeof(DRMWQueue));
    return drmvqueue_initialize(queue);
}

DRMWQueue * drmvqueue_initialize(DRMWQueue * queue){
    for(int i = 0; i < MWQ_CAPACITY; i++){
        queue->elements[i].request = NULL;
        queue->elements[i].data = NULL;
        queue->elements[i].responseLocation = NULL;
    }
    queue->elementCount.value = MWQ_CAPACITY;
    queue->closed.value = true;
    __sync_synchronize();
    return queue;
}

void drmvqueue_free(DRMWQueue * queue){
    free(queue);
}

static inline
bool drmvqueue_offer(DRMWQueue * queue, DelegateRequestEntry e){
    bool closed;
    load_acq(closed, queue->closed.value);
    if(!closed){
        int index = FETCH_AND_ADD(&queue->elementCount.value, 1);
        if(index < MWQ_CAPACITY){
            store_rel(queue->elements[index].responseLocation, e.responseLocation);
            store_rel(queue->elements[index].data, e.data);
            store_rel(queue->elements[index].request, e.request);
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
void drmvqueue_flush(DRMWQueue * queue){
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
            DelegateRequestEntry e;
            load_acq(e.request, queue->elements[currentElementIndex].request);
            while(e.request == NULL) {
                __sync_synchronize();
                load_acq(e.request, queue->elements[currentElementIndex].request);
            }
            load_acq(e.data, queue->elements[currentElementIndex].data);
            load_acq(e.responseLocation, queue->elements[currentElementIndex].responseLocation);
            e.request(e.data, e.responseLocation);
            store_rel(queue->elements[currentElementIndex].request, NULL);
            currentElementIndex = currentElementIndex + 1;
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
void drmvqueue_reset_fully_read(DRMWQueue * queue){
    store_rel(queue->elementCount.value, 0);
    store_rel(queue->closed.value, false);
}
#endif
