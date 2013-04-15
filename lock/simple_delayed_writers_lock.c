#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

#include "multi_writers_queue.h"
#include "simple_delayed_writers_lock.h"
#include "smp_utils.h"

#define GET(value_ptr)  __sync_fetch_and_add(value_ptr, 0)

#define NUMBER_OF_READER_GROUPS 64


typedef struct NodeImpl {
    bool locked;
    struct NodeImpl * next;
    bool readLockIsWriteLock;
    MWQueue writeQueue;
} Node;

inline
Node * get_and_set_node_ptr(Node ** pointerToOldValue, Node * newValue){
    while (true) {
        Node * x = GET(pointerToOldValue);
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
    }
}

typedef struct CacheLinePaddedIntImpl {
    int value;
    char padding[64 - sizeof(int)];
} CacheLinePaddedInt;

typedef struct SimpleDelayedWritesLockImpl {
    void (*writer)(void *);
    Node * endOfQueue;
    CacheLinePaddedInt readLocks[NUMBER_OF_READER_GROUPS] __attribute__((aligned(64)));
} SimpleDelayedWritesLock;

__thread Node myNode;
__thread int myId;

int myIdCounter = 0;

inline
void indicateReadEnter(SimpleDelayedWritesLock * lock){
    //    printf("%d %d\n", myId, myId % NUMBER_OF_READER_GROUPS);
    __sync_fetch_and_add(&lock->readLocks[myId % NUMBER_OF_READER_GROUPS].value, 1);
}

inline
void indicateReadExit(SimpleDelayedWritesLock * lock){
    __sync_fetch_and_sub(&lock->readLocks[myId % NUMBER_OF_READER_GROUPS].value, 1);
}

void waitUntilAllReadersAreGone(SimpleDelayedWritesLock * lock){
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        while(GET(&lock->readLocks[i].value) > 0){};
    }
}
 
SimpleDelayedWritesLock * sdwlock_create(void (*writer)(void *)){
    SimpleDelayedWritesLock * lock = malloc(sizeof(SimpleDelayedWritesLock));
    lock->writer = writer;
    lock->endOfQueue = NULL;
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        lock->readLocks[i].value = 0;
    }
    __sync_synchronize();
    return lock;
}

void sdwlock_free(SimpleDelayedWritesLock * lock){
    free(lock);
}


void sdwlock_register_this_thread(){
    Node * node = &myNode;
    myId = __sync_fetch_and_add(&myIdCounter, 1);
    node->locked = false;
    node->next = NULL;
    node->readLockIsWriteLock = false;
    mwqueue_initialize(&node->writeQueue);
}

void sdwlock_write(SimpleDelayedWritesLock *lock, void * writeInfo) {
    Node * currentNode = GET(&lock->endOfQueue);
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
        node->locked = true;
        predecessor->next = node;
        __sync_synchronize();
        //Wait
        while (GET(&node->locked)) {}
    }
    waitUntilAllReadersAreGone(lock);
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
    if (GET(&node->next) == NULL) {
        if (__sync_bool_compare_and_swap(&lock->endOfQueue, node, NULL)){
            return;
        }
        //wait
        while (GET(&node->next) == NULL) {}
    }
    node->next->locked = false;
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
