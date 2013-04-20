#include <stdlib.h>

#include "multi_writers_queue.h"
#include "smp_utils.h"

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

bool mwqueue_offer(MWQueue * queue, entry e){
    int index = __sync_fetch_and_add(&queue->elementCount.value, 1);
    if(index < MWQ_CAPACITY){
        queue->elements[index] = e;
        __sync_synchronize();
        return true;
    }else{
        return false;
    }
}

entry mwqueue_take(MWQueue * queue){
    if(queue->readStarted == false){
        //Read number of elements and lock the queue for inserts
        __sync_synchronize();
        queue->numOfElementsToRead = ACCESS_ONCE(queue->elementCount.value);
        if(queue->numOfElementsToRead >= MWQ_CAPACITY){
            queue->closed = true;
            queue->numOfElementsToRead = MWQ_CAPACITY;
        }
        queue->readStarted = true;
    }
    bool repeat = false;
    do{
        if(queue->readCurrentElementIndex < queue->numOfElementsToRead){
            entry theElement = ACCESS_ONCE(queue->elements[queue->readCurrentElementIndex]);
            while(theElement == NULL) {
                __sync_synchronize();
                theElement = ACCESS_ONCE(queue->elements[queue->readCurrentElementIndex]);
            }
            queue->elements[queue->readCurrentElementIndex] = NULL;
            queue->readCurrentElementIndex = queue->readCurrentElementIndex + 1;
            return theElement;
        }else if (queue->closed){
            return NULL;
        }else{
            __sync_synchronize();
            int newNumOfElementsToRead = ACCESS_ONCE(queue->elementCount.value);
            if(newNumOfElementsToRead < MWQ_CAPACITY){  
                if(newNumOfElementsToRead == queue->numOfElementsToRead){
                    queue->numOfElementsToRead = 
                        min(get_and_set_int(&queue->elementCount.value, MWQ_CAPACITY + 1), MWQ_CAPACITY);
                    queue->closed = true;
                    repeat = true;
                }else{
                    queue->numOfElementsToRead = newNumOfElementsToRead;
                    repeat = true;
                }
            }else {
                queue->closed = true;
                queue->numOfElementsToRead = MWQ_CAPACITY;
                repeat = true;
            } 
        }
    }while(repeat);
    return NULL;
}

    
void mwqueue_reset_fully_read(MWQueue * queue){
    queue->readCurrentElementIndex = 0;
    queue->readStarted = false;
    queue->closed = false;
    queue->elementCount.value = 0;
    __sync_synchronize();
}
