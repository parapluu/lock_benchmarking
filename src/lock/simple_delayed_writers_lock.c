#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>


#include "simple_delayed_writers_lock.h"
#include "smp_utils.h"


inline
Node * get_and_set_node_ptr(Node ** pointerToOldValue, Node * newValue){
    Node * x = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
        x = ACCESS_ONCE(*pointerToOldValue);
    }
}


__thread Node myNode;
__thread int myId;

int myIdCounter = 0;

inline
void indicateReadEnter(SimpleDelayedWritesLock * lock){
    __sync_fetch_and_add(&lock->readLocks[myId % NUMBER_OF_READER_GROUPS].value, 1);
}

inline
void indicateReadExit(SimpleDelayedWritesLock * lock){
    __sync_fetch_and_sub(&lock->readLocks[myId % NUMBER_OF_READER_GROUPS].value, 1);
}

inline
void waitUntilAllReadersAreGone(SimpleDelayedWritesLock * lock){
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        while(ACCESS_ONCE(lock->readLocks[i].value) > 0){    
            __sync_synchronize();
        };
    }
}
 
SimpleDelayedWritesLock * sdwlock_create(void (*writer)(void *)){
    SimpleDelayedWritesLock * lock = malloc(sizeof(SimpleDelayedWritesLock));
    sdwlock_initialize(lock, writer);
    return lock;
}

void sdwlock_initialize(SimpleDelayedWritesLock * lock, void (*writer)(void *)){
    lock->writer = writer;
    lock->endOfQueue = NULL;
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        lock->readLocks[i].value = 0;
    }
    __sync_synchronize();
}

void sdwlock_free(SimpleDelayedWritesLock * lock){
    free(lock);
}


void sdwlock_register_this_thread(){
    Node * node = &myNode;
    myId = __sync_fetch_and_add(&myIdCounter, 1);
    node->locked.value = false;
    node->next = NULL;
    node->readLockIsWriteLock = false;
    mwqueue_initialize(&node->writeQueue);
}

void sdwlock_write(SimpleDelayedWritesLock *lock, void * writeInfo) {
    __sync_synchronize();
    Node * currentNode = ACCESS_ONCE(lock->endOfQueue);
    if(currentNode == NULL || ! mwqueue_offer(&currentNode->writeQueue, writeInfo)){
        sdwlock_write_read_lock(lock);
        lock->writer(writeInfo);
        sdwlock_write_read_unlock(lock);
    }
}

void sdwlock_write_read_lock(SimpleDelayedWritesLock *lock) {
    Node * node = &myNode;
    Node * predecessor = get_and_set_node_ptr(&lock->endOfQueue, node);
    mwqueue_reset_fully_read(&node->writeQueue);
    if (predecessor != NULL) {
        node->locked.value = true;
        predecessor->next = node;
        __sync_synchronize();
        //Wait
        while (ACCESS_ONCE(node->locked.value)) {
            __sync_synchronize();
        }
    }else{
        waitUntilAllReadersAreGone(lock);
    }
}

void flushWriteQueue(SimpleDelayedWritesLock * lock, MWQueue * writeQueue){
    void (*writer)(void *) = lock->writer;
    void * element = mwqueue_take(writeQueue);
    while(element != NULL) {
        writer(element);
        element = mwqueue_take(writeQueue);
    }
}

void sdwlock_write_read_unlock(SimpleDelayedWritesLock * lock) {
    Node * node = &myNode;
    flushWriteQueue(lock, &node->writeQueue);
    if (ACCESS_ONCE(node->next) == NULL) {
        if (__sync_bool_compare_and_swap(&lock->endOfQueue, node, NULL)){
            return;
        }
        //wait
        while (ACCESS_ONCE(node->next) == NULL) {
            __sync_synchronize();
        }
    }
    node->next->locked.value = false;
    node->next = NULL;
    __sync_synchronize();
}


void convertReadLockToWriteLock(SimpleDelayedWritesLock *lock, Node * node){
    node->readLockIsWriteLock = true;
    sdwlock_write_read_lock(lock);
}

void sdwlock_read_lock(SimpleDelayedWritesLock *lock) {
    Node * node = &myNode;
    __sync_synchronize();
    if(ACCESS_ONCE(lock->endOfQueue) == NULL){
        indicateReadEnter(lock);
        if(ACCESS_ONCE(lock->endOfQueue) != NULL){
            indicateReadExit(lock);
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
        indicateReadExit(lock);
    }
}
