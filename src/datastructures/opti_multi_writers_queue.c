#include <stdlib.h>

#include "opti_multi_writers_queue.h"
#include "smp_utils.h"

inline
unsigned long min(unsigned long i1, unsigned long i2){
    return i1 < i2 ? i1 : i2;
}

OptiMWQueue * omwqueue_create(){
    OptiMWQueue * queue = malloc(sizeof(OptiMWQueue));
    return omwqueue_initialize(queue);
}

OptiMWQueue * omwqueue_initialize(OptiMWQueue * queue){
    for(int i = 0; i < MWQ_CAPACITY; i++){
        queue->elements[i] = NULL;
    }
    queue->elementCount.value = MWQ_CAPACITY;
    queue->closed.value = true;
    __sync_synchronize();
    return queue;
}

void omwqueue_free(OptiMWQueue * queue){
    free(queue);
}

inline
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

inline
void omwqueue_flush(OptiMWQueue * queue, void (*writer)(void *)){
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
            writer(theElement);
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

inline
void omwqueue_reset_fully_read(OptiMWQueue * queue){
    store_rel(queue->elementCount.value, 0);
    store_rel(queue->closed.value, false);
}
