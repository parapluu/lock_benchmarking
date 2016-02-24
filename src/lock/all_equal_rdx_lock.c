#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include "all_equal_rdx_lock.h"
#include "utils/smp_utils.h"
#include "utils/thread_identifier.h"

#define NUMBER_OF_READ_SPIN_ATTEMPTS 10000

__thread Node myNode __attribute__((aligned(64)));

static inline
Node * get_and_set_node_ptr(Node ** pointerToOldValue, Node * newValue){
    Node * x = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
        x = ACCESS_ONCE(*pointerToOldValue);
    }
}

static inline
void disableReadSpinning(Node * node){
    store_rel(node->readSpinningEnabled.value, false);
    //We need a StoreLoad barrier since we don't want the store above
    //to be reorderd with the loads below
    __sync_synchronize();
    NZI_WAIT_UNIL_EMPTY(&node->nonZeroIndicator);
}

static inline
bool tryReadSpinningInQueue(AllEqualRDXLock * lock, Node * myNode){
    Node * node;
    int spinInNodeEnabled;
    for(int i = 0; i < NUMBER_OF_READ_SPIN_ATTEMPTS; i++){
        load_acq(node, lock->endOfQueue.value);
        if(node == NULL){
            NZI_ARRIVE(&lock->nonZeroIndicator);
            load_acq(node, lock->endOfQueue.value);
            if(node != NULL){
                NZI_DEPART(&lock->nonZeroIndicator);
            }else{
                return true;
            }
        }else{
            load_acq(spinInNodeEnabled, node->readSpinningEnabled.value);
            if(spinInNodeEnabled){
                NZI_ARRIVE(&node->nonZeroIndicator);
                load_acq(spinInNodeEnabled, node->readSpinningEnabled.value);
                if(spinInNodeEnabled){
                    load_acq(spinInNodeEnabled, node->readSpinningEnabled.value);  
                    while(spinInNodeEnabled){//the read spinning
                        __sync_synchronize();
                        load_acq(spinInNodeEnabled, node->readSpinningEnabled.value);
                    }
                    myNode->readLockIsSpinningOnNode = true;
                    myNode->readLockSpinningNode = node;
                    return true;
                }else{
                    NZI_DEPART(&node->nonZeroIndicator);
                }
            }
        }
        __sync_synchronize();
    }
    return false;
}
 
AllEqualRDXLock * aerlock_create(void (*writer)(void *, void **)){
    AllEqualRDXLock * lock = malloc(sizeof(AllEqualRDXLock));
    aerlock_initialize(lock, writer);
    return lock;
}

void aerlock_initialize(AllEqualRDXLock * lock, void (*writer)(void *, void **)){
    lock->writer = writer;
    lock->endOfQueue.value = NULL;
    NZI_INITIALIZE(&lock->nonZeroIndicator);
    __sync_synchronize();
}

void aerlock_free(AllEqualRDXLock * lock){
    free(lock);
}

void aerlock_register_this_thread(){
    Node * node = &myNode;
    assign_id_to_thread();
    node->locked.value = false;
    node->next.value = NULL;
    node->readLockIsWriteLock = false;
    node->readLockIsSpinningOnNode = false;
    NZI_INITIALIZE(&node->nonZeroIndicator);
    node->readSpinningEnabled.value = false;
    mwqueue_initialize(&node->writeQueue);
}

void aerlock_write(AllEqualRDXLock *lock, void * writeInfo) {
    Node * currentNode;
    load_acq(currentNode, lock->endOfQueue.value);
    if(currentNode == NULL || ! mwqueue_offer(&currentNode->writeQueue, writeInfo)){
        aerlock_write_read_lock(lock);
        lock->writer(writeInfo, NULL);
        aerlock_write_read_unlock(lock);
    }
}

void aerlock_write_read_lock(AllEqualRDXLock *lock) {
    bool isNodeLocked;
    Node * node = &myNode;
    node->next.value = NULL;
    Node * predecessor = get_and_set_node_ptr(&lock->endOfQueue.value, node);
    store_rel(node->readSpinningEnabled.value, true);
    mwqueue_reset_fully_read(&node->writeQueue);
    if (predecessor != NULL) {
        store_rel(node->locked.value, true);
        store_rel(predecessor->next.value, node);
        load_acq(isNodeLocked, node->locked.value);
        while (isNodeLocked){
            __sync_synchronize();
            load_acq(isNodeLocked, node->locked.value);
        }
    }else{
        //StoreLoad barrier
        //This is necessary because we don't want the stores above to be reorderd
        //with the following loads
        __sync_synchronize();
        NZI_WAIT_UNIL_EMPTY(&lock->nonZeroIndicator);
    }
}

void flushWriteQueue(AllEqualRDXLock * lock, MWQueue * writeQueue){
    void (*writer)(void *, void **) = lock->writer;
    void * element = mwqueue_take(writeQueue);
    while(element != NULL) {
        writer(element, NULL);
        element = mwqueue_take(writeQueue);
    }
}

void aerlock_write_read_unlock(AllEqualRDXLock * lock) {
    Node * nextNode;
    Node * node = &myNode;
    flushWriteQueue(lock, &node->writeQueue);
    disableReadSpinning(node);
    load_acq(nextNode, node->next.value);
    if (nextNode == NULL) {
        if (__sync_bool_compare_and_swap(&lock->endOfQueue.value, node, NULL)){
            return;
        }
        //wait
        load_acq(nextNode, node->next.value);
        while (nextNode == NULL){
            __sync_synchronize();
            load_acq(nextNode, node->next.value);
        }
    }
    store_rel(nextNode->locked.value, false);
    __sync_synchronize();//Push change
}

static inline
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
        NZI_DEPART(&node->readLockSpinningNode->nonZeroIndicator);
        node->readLockIsSpinningOnNode = false;
    } else {
        NZI_DEPART(&lock->nonZeroIndicator);
    }
}
