#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

#include "utils/thread_identifier.h"
#include "simple_delayed_writers_lock.h"
#include "utils/smp_utils.h"


static inline
Node * get_and_set_node_ptr(Node ** pointerToOldValue, Node * newValue){
    Node * x = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
        x = ACCESS_ONCE(*pointerToOldValue);
    }
}


__thread Node myNode __attribute__((aligned(64)));

 
SimpleDelayedWritesLock * sdwlock_create(void (*writer)(void *, void **)){
    SimpleDelayedWritesLock * lock = malloc(sizeof(SimpleDelayedWritesLock));
    sdwlock_initialize(lock, writer);
    return lock;
}

void sdwlock_initialize(SimpleDelayedWritesLock * lock, void (*writer)(void *, void **)){
    lock->writer = writer;
    lock->endOfQueue.value = NULL;
    NZI_INITIALIZE(&lock->nonZeroIndicator);
    __sync_synchronize();
}

void sdwlock_free(SimpleDelayedWritesLock * lock){
    free(lock);
}


void sdwlock_register_this_thread(){
    Node * node = &myNode;
    assign_id_to_thread();
    node->locked.value = false;
    node->next.value = NULL;
    node->readLockIsWriteLock = false;
    mwqueue_initialize(&node->writeQueue);
}

void sdwlock_write(SimpleDelayedWritesLock *lock, void * writeInfo) {
    __sync_synchronize();
    Node * currentNode = ACCESS_ONCE(lock->endOfQueue.value);
    if(currentNode == NULL || ! mwqueue_offer(&currentNode->writeQueue, writeInfo)){
        sdwlock_write_read_lock(lock);
        lock->writer(writeInfo, NULL);
        sdwlock_write_read_unlock(lock);
    }
}

void sdwlock_write_read_lock(SimpleDelayedWritesLock *lock) {
    Node * node = &myNode;
    node->next.value = NULL;
    Node * predecessor = get_and_set_node_ptr(&lock->endOfQueue.value, node);
    mwqueue_reset_fully_read(&node->writeQueue);
    if (predecessor != NULL) {
        node->locked.value = true;
        __sync_synchronize();
        predecessor->next.value = node;
        __sync_synchronize();
        //Wait
        while (ACCESS_ONCE(node->locked.value)) {
            __sync_synchronize();
        }
    }else{
        NZI_WAIT_UNIL_EMPTY(&lock->nonZeroIndicator);
    }
}

void flushWriteQueue(SimpleDelayedWritesLock * lock, MWQueue * writeQueue){
    void (*writer)(void *, void **) = lock->writer;
    void * element = mwqueue_take(writeQueue);
    while(element != NULL) {
        writer(element, NULL);
        element = mwqueue_take(writeQueue);
    }
}

void sdwlock_write_read_unlock(SimpleDelayedWritesLock * lock) {
    Node * node = &myNode;
    flushWriteQueue(lock, &node->writeQueue);
    if (ACCESS_ONCE(node->next.value) == NULL) {
        if (__sync_bool_compare_and_swap(&lock->endOfQueue.value, node, NULL)){
            return;
        }
        //wait
        while (ACCESS_ONCE(node->next.value) == NULL) {
            __sync_synchronize();
        }
    }
    node->next.value->locked.value = false;
    __sync_synchronize();
}


void convertReadLockToWriteLock(SimpleDelayedWritesLock *lock, Node * node){
    node->readLockIsWriteLock = true;
    sdwlock_write_read_lock(lock);
}

void sdwlock_read_lock(SimpleDelayedWritesLock *lock) {
    Node * node = &myNode;
    __sync_synchronize();
    if(ACCESS_ONCE(lock->endOfQueue.value) == NULL){
        NZI_ARRIVE(&lock->nonZeroIndicator);
        if(ACCESS_ONCE(lock->endOfQueue.value) != NULL){
            NZI_DEPART(&lock->nonZeroIndicator);
            convertReadLockToWriteLock(lock, node);
        }
    }else{
        convertReadLockToWriteLock(lock, node);
    }
}

void sdwlock_read_unlock(SimpleDelayedWritesLock *lock) {
    Node * node = &myNode;
    if(node->readLockIsWriteLock){
        sdwlock_write_read_unlock(lock);
        node->readLockIsWriteLock = false;
    } else {
        NZI_DEPART(&lock->nonZeroIndicator);
    }
}
