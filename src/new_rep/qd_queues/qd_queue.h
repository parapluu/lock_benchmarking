#ifndef QD_QUEUE_H
#define QD_QUEUE_H

#include <stdbool.h>
#include <limits.h>

#include "misc/padded_types.h"
#include "misc/bsd_stdatomic.h"//Until c11 stdatomic.h is available
#include "misc/thread_includes.h"//Until c11 thread.h is available
#include "locks/tatas_lock.h"

/* Queue Delegation Queue */

#define QD_QUEUE_BUFFER_SIZE 4096

typedef struct QDQueueRequestIDImpl {
    volatile atomic_uint messageSize;
    void (*requestIdentifier)(unsigned int messageSize, void * message);
} QDRequestRequestId;

typedef struct QDQueueImpl {
    LLPaddedULong counter;
    LLPaddedBool closed;
    char buffer[QD_QUEUE_BUFFER_SIZE];
} QDQueue;

void qdq_initialize(QDQueue * q){
    for(int i = 0; i < QD_QUEUE_BUFFER_SIZE; i++){
        q->buffer[i] = UCHAR_MAX;
    }
    atomic_store_explicit( &q->counter.value,
                           QD_QUEUE_BUFFER_SIZE, 
                           memory_order_relaxed );
    atomic_store_explicit( &q->closed.value,
                           true,
                           memory_order_release );
}

void qdq_open(QDQueue* q) {
    atomic_store_explicit( &q->counter.value,
                           0, 
                           memory_order_relaxed );
    atomic_store_explicit( &q->closed.value,
                           false, 
                           memory_order_release );
}

bool qdq_enqueue(QDQueue* q,
             void (*funPtr)(unsigned int, void *), 
             unsigned int messageSize,
             void * messageAddress) {
    if(atomic_load_explicit( &q->closed.value, memory_order_acquire )){
        return false;
    }
    char * messageBuffer = (char *) messageAddress;
    unsigned int storeSize = sizeof(QDRequestRequestId) + messageSize;
    unsigned int pad = sizeof(unsigned int) - (storeSize & (sizeof(unsigned int) - 1));
    unsigned long bufferOffset = atomic_fetch_add(&q->counter.value,
                                                  storeSize + pad);
    unsigned int writeToOffset = (bufferOffset + storeSize);
    unsigned int nextReqOffset = writeToOffset + pad;
    QDRequestRequestId * reqId = 
        (QDRequestRequestId*)&q->buffer[bufferOffset];
    if(nextReqOffset <= QD_QUEUE_BUFFER_SIZE) {
        reqId->requestIdentifier = funPtr;
        unsigned long messageBodyStart = bufferOffset + sizeof(QDRequestRequestId);
        for(unsigned long i = messageBodyStart; 
            i < writeToOffset;
            i++){
            q->buffer[i] = messageBuffer[i - messageBodyStart];
        }
        atomic_store_explicit( &reqId->messageSize,
                               messageSize, 
                               memory_order_release );
        return true;
    } else if(bufferOffset < QD_QUEUE_BUFFER_SIZE){
        atomic_store_explicit( &reqId->messageSize,
                               QD_QUEUE_BUFFER_SIZE + 1, 
                               memory_order_release ); 
        return false;
    } else {
        return false;
    }
}

#include <stdlib.h>
#include <stdio.h>
void qdq_flush(QDQueue* q) {
    unsigned long todo = 0;
    bool open = true;
    while(open) {
        unsigned long done = todo;
        todo = atomic_load_explicit( &q->counter.value, memory_order_relaxed );
        if((todo == done) &&
           atomic_compare_exchange_strong( &q->counter.value,
                                           &todo,
                                           QD_QUEUE_BUFFER_SIZE)) { 
            open = false;
            atomic_store_explicit( &q->closed.value,
                                   true, 
                                   memory_order_relaxed );
        }
        if(todo >= QD_QUEUE_BUFFER_SIZE) { /* queue closed */
            todo = QD_QUEUE_BUFFER_SIZE;
            open = false;
            atomic_store_explicit( &q->closed.value,
                                   true, 
                                   memory_order_relaxed );
        }
        unsigned long index = done;
        while( index < todo ) {
            unsigned int messageSize;
            QDRequestRequestId * reqId = 
                (QDRequestRequestId*)&q->buffer[index];
            while(UINT_MAX == 
                  (messageSize = atomic_load_explicit( &reqId->messageSize,
                                                       memory_order_acquire ))){
                /* spin wait */
                atomic_thread_fence(memory_order_seq_cst);/*hw threads*/
            }
            if(messageSize == (QD_QUEUE_BUFFER_SIZE + 1)){
                return; /* Too big, we can return */
            }
            void (*funPtr)(unsigned int, void *) = reqId->requestIdentifier;
            unsigned int storeSize = sizeof(QDRequestRequestId) + messageSize;
            unsigned int messageEndOffset = index + storeSize;
            unsigned int pad = sizeof(unsigned int) - (storeSize & (sizeof(unsigned int) - 1));
            unsigned int nextReqOffset = messageEndOffset + pad;
            void * messageAddress = q->buffer + sizeof(QDRequestRequestId) + index;
            funPtr(messageSize, messageAddress);           
            for(unsigned int i = index; i < messageEndOffset; i++){
                q->buffer[i] = UCHAR_MAX;
            }
            index = nextReqOffset;
        }
    }
}

#endif
