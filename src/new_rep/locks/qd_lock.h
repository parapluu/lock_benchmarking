#ifndef QD_LOCK_H
#define QD_LOCK_H

#include "misc/bsd_stdatomic.h"//Until c11 stdatomic.h is available
#include "misc/thread_includes.h"//Until c11 thread.h is available
#include <stdbool.h>

#include "misc/padded_types.h"
#include "locks/tatas_lock.h"
#include "qd_queues/qd_queue.h"

/* Queue Delegation Lock */

typedef struct QDLockImpl {
    TATASLock mutexLock;
    QDQueue queue;
} QDLock;

void qd_initialize(QDLock * lock){
    tatas_initialize(&lock->mutexLock);
    qdq_initialize(&lock->queue);
}

void qd_delegate(QDLock* l,
                 void (*funPtr)(unsigned int, void *), 
                 unsigned int messageSize,
                 void * messageAddress) {
    while(true) {
        if(tatas_try_lock(&l->mutexLock)) {
            qdq_open(&l->queue);
            funPtr(messageSize, messageAddress);
            qdq_flush(&l->queue);
            tatas_unlock(&l->mutexLock);
            return;
        } else if(qdq_enqueue(&l->queue,
                              funPtr,
                              messageSize,
                              messageAddress)){
            return;
        }
        thread_yield();
    }
}

#endif
