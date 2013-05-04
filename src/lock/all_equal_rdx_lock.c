#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include "all_equal_rdx_lock.h"
#include "smp_utils.h"

#define NUMBER_OF_READ_SPIN_ATTEMPTS 128



void debugPrintCacheLinePaddedInts(CacheLinePaddedInt * ints, int numOfInts){
    for(int i = 0; i < numOfInts; i++){
        printf("%d ", ints[i].value);
    }
    printf("\n");
}

inline
Node * get_and_set_node_ptr(Node ** pointerToOldValue, Node * newValue){
    Node * x = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
        x = ACCESS_ONCE(*pointerToOldValue);
    }
}

__thread Node myNode __attribute__((aligned(64)));
__thread CacheLinePaddedInt myId __attribute__((aligned(64)));

int myIdCounter = 0;

inline
void indicateReadEnter(AllEqualRDXLock * lock){
    __sync_fetch_and_add(&lock->readLocks[myId.value % NUMBER_OF_READER_GROUPS].value, 1);
}

inline
void indicateReadExit(AllEqualRDXLock * lock){
    __sync_fetch_and_sub(&lock->readLocks[myId.value % NUMBER_OF_READER_GROUPS].value, 1);
}

inline
void waitUntilAllReadersAreGone(AllEqualRDXLock * lock){
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        while(ACCESS_ONCE(lock->readLocks[i].value) > 0){    
            __sync_synchronize();
        };
    }
}

inline
void disableReadSpinning(Node * node){
    node->readSpinningEnabled.value = false;
    __sync_synchronize();

    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        while(ACCESS_ONCE(node->readSpinnerFlags[i].value) > 0){    
            __sync_synchronize();
        };
    }
}

inline
bool tryReadSpinningInQueue(AllEqualRDXLock * lock, Node * myNode){
    __sync_synchronize();
    Node * node = ACCESS_ONCE(lock->endOfQueue.value);
    for(int i = 0; i < NUMBER_OF_READ_SPIN_ATTEMPTS; i++){
        if(node == NULL){
            indicateReadEnter(lock);
            if(ACCESS_ONCE(lock->endOfQueue.value) != NULL){
                indicateReadExit(lock);
            }else{
                return true;
            }
        }else if(ACCESS_ONCE(node->readSpinningEnabled.value)){
            __sync_fetch_and_add(&node->readSpinnerFlags[myId.value % NUMBER_OF_READER_GROUPS].value, 1);
            if(ACCESS_ONCE(node->readSpinningEnabled.value)){
                do{//the read spinning
                    __sync_synchronize();
                }while(ACCESS_ONCE(node->readSpinningEnabled.value));
                myNode->readLockIsSpinningOnNode = true;
                myNode->readLockSpinningNode = node;
                return true;
            }else{
                __sync_fetch_and_sub(&node->readSpinnerFlags[myId.value % NUMBER_OF_READER_GROUPS].value, 1);
            }
        }else{
            __sync_synchronize();
        }
        node = ACCESS_ONCE(lock->endOfQueue.value);
    }
    return false;
}

inline
void indicateReadExitFromQueueNode(Node * node){
    __sync_fetch_and_sub(&node->readSpinnerFlags[myId.value % NUMBER_OF_READER_GROUPS].value, 1);
}
 
AllEqualRDXLock * aerlock_create(void (*writer)(void *)){
    AllEqualRDXLock * lock = malloc(sizeof(AllEqualRDXLock));
    aerlock_initialize(lock, writer);
    return lock;
}

void aerlock_initialize(AllEqualRDXLock * lock, void (*writer)(void *)){
    lock->writer = writer;
    lock->endOfQueue.value = NULL;
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        lock->readLocks[i].value = 0;
    }
    __sync_synchronize();
}

void aerlock_free(AllEqualRDXLock * lock){
    free(lock);
}


void aerlock_register_this_thread(){
    Node * node = &myNode;
    myId.value = __sync_fetch_and_add(&myIdCounter, 1);
    node->locked.value = false;
    node->next.value = NULL;
    node->readLockIsWriteLock = false;
    node->readLockIsSpinningOnNode = false;
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        node->readSpinnerFlags[i].value = 0;
    }
    node->readSpinningEnabled.value = false;
    mwqueue_initialize(&node->writeQueue);
}

void aerlock_write(AllEqualRDXLock *lock, void * writeInfo) {
    __sync_synchronize();
    Node * currentNode = ACCESS_ONCE(lock->endOfQueue.value);
    if(currentNode == NULL || ! mwqueue_offer(&currentNode->writeQueue, writeInfo)){
        aerlock_write_read_lock(lock);
        lock->writer(writeInfo);
        aerlock_write_read_unlock(lock);
    }
}

void aerlock_write_read_lock(AllEqualRDXLock *lock) {
    Node * node = &myNode;
    node->next.value = NULL;
    Node * predecessor = get_and_set_node_ptr(&lock->endOfQueue.value, node);
    node->readSpinningEnabled.value = true;//mb in next statement
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
        waitUntilAllReadersAreGone(lock);
    }
}

void flushWriteQueue(AllEqualRDXLock * lock, MWQueue * writeQueue){
    void (*writer)(void *) = lock->writer;
    void * element = mwqueue_take(writeQueue);
    while(element != NULL) {
        writer(element);
        element = mwqueue_take(writeQueue);
    }
}

void aerlock_write_read_unlock(AllEqualRDXLock * lock) {
    Node * node = &myNode;
    flushWriteQueue(lock, &node->writeQueue);
    disableReadSpinning(node);
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

inline
void convertReadLockToWriteLock(AllEqualRDXLock *lock, Node * node){
    node->readLockIsWriteLock = true;
    aerlock_write_read_lock(lock);
}

void aerlock_read_lock(AllEqualRDXLock *lock) {
    Node * MYNode = &myNode;
    if(!tryReadSpinningInQueue(lock, MYNode)){
        convertReadLockToWriteLock(lock, MYNode);
    }
}

void aerlock_read_unlock(AllEqualRDXLock *lock) {
    Node * node = &myNode;
    if(node->readLockIsWriteLock){
        aerlock_write_read_unlock(lock);
        node->readLockIsWriteLock = false;
    }else if(node->readLockIsSpinningOnNode){
        indicateReadExitFromQueueNode(node->readLockSpinningNode);
        node->readLockIsSpinningOnNode = false;
    } else {
        indicateReadExit(lock);
    }
}
