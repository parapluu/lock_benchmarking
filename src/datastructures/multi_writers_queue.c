#include <stdlib.h>

#include "multi_writers_queue.h"
#include "utils/smp_utils.h"

inline
int min(int i1, int i2){
    return i1 < i2 ? i1 : i2;
}

MWQueue * mwqueue_create(){
    MWQueue * queue = malloc(sizeof(MWQueue));
    return mwqueue_initialize(queue);
}

MWQueue * mwqueue_initialize(MWQueue * queue){
    for(int i = 0; i < MWQ_CAPACITY; i++){
        queue->elements[i] = NULL;
    }
    queue->elementCount.value = MWQ_CAPACITY;
    queue->readCurrentElementIndex = 0;
    queue->numOfElementsToRead = 0;
    queue->readStarted = true;
    queue->closed = true;
    __sync_synchronize();
    return queue;
}

void mwqueue_free(MWQueue * queue){
    free(queue);
}

inline
bool mwqueue_offer(MWQueue * queue, entry e){
    int index = __sync_fetch_and_add(&queue->elementCount.value, 1);
    if(index < MWQ_CAPACITY){
        store_rel(queue->elements[index], e);
        __sync_synchronize();
        return true;
    }else{
        return false;
    }
}

inline
void init_read(MWQueue * queue){
    load_acq(queue->numOfElementsToRead, queue->elementCount.value);
    if(queue->numOfElementsToRead >= MWQ_CAPACITY){
        queue->closed = true;
        queue->numOfElementsToRead = MWQ_CAPACITY;
    }
    queue->readStarted = true;
}

inline
entry mwqueue_take(MWQueue * queue){
    if(queue->readStarted == false){
        init_read(queue);
    }
    do{
        if(queue->readCurrentElementIndex < queue->numOfElementsToRead){
            //There is definitly an element that we should read
            entry theElement;
            load_acq(theElement, queue->elements[queue->readCurrentElementIndex]);
            while(theElement == NULL) {
                __sync_synchronize();
                load_acq(theElement, queue->elements[queue->readCurrentElementIndex]);
            }
            queue->elements[queue->readCurrentElementIndex] = NULL;
            queue->readCurrentElementIndex = queue->readCurrentElementIndex + 1;
            return theElement;
        }else if (queue->closed){
            //The queue is closed and there is no more elements that need to be read:
            return NULL;
        }else{
            //Seems like there are no elements that should be read and the queue is
            //not closed. Check again if there are still no more elements that should
            //be read before closing the queue
            int newNumOfElementsToRead;
            load_acq(newNumOfElementsToRead, queue->elementCount.value);
            if(newNumOfElementsToRead < MWQ_CAPACITY){  
                //The capacity is not reached
                //Check if the number of elements to read has changed before closing
                //the queue.
                if(newNumOfElementsToRead == queue->numOfElementsToRead){
                    //numOfElementsToRead has not changed. Close the queue.
                    queue->numOfElementsToRead = 
                        min(get_and_set_int(&queue->elementCount.value, MWQ_CAPACITY + 1), 
                            MWQ_CAPACITY);
                    queue->closed = true;
                }else{
                    queue->numOfElementsToRead = newNumOfElementsToRead;
                }
            }else {
                queue->closed = true;
                queue->numOfElementsToRead = MWQ_CAPACITY;
            } 
        }
    }while(true);
    return NULL;
}

inline
void mwqueue_reset_fully_read(MWQueue * queue){
    store_rel(queue->readCurrentElementIndex, 0);
    store_rel(queue->readStarted, false);
    store_rel(queue->closed, false);
    store_rel(queue->elementCount.value, 0);
}
