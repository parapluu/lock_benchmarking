#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

#include "mcs_lock.h"

__thread MCSNode myMCSNode __attribute__((aligned(64)));

static inline
MCSNode * get_and_set_node_ptr(MCSNode ** pointerToOldValue, MCSNode * newValue){
    MCSNode * x = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
        x = ACCESS_ONCE(*pointerToOldValue);
    }
}
 
MCSLock * mcslock_create(void (*writer)(void *, void **)){
    MCSLock * lock = malloc(sizeof(MCSLock));
    mcslock_initialize(lock, writer);
    return lock;
}

void mcslock_initialize(MCSLock * lock, void (*writer)(void *, void **)){
    lock->writer = writer;
    lock->endOfQueue.value = NULL;
    __sync_synchronize();
}

void mcslock_free(MCSLock * lock){
    free(lock);
}

void mcslock_register_this_thread(){
    MCSNode * node = &myMCSNode;
    node->locked.value = false;
    node->next.value = NULL;
}

void mcslock_write(MCSLock *lock, void * writeInfo) {
    mcslock_write_read_lock(lock);
    lock->writer(writeInfo, NULL);
    mcslock_write_read_unlock(lock);
    
}

//Returns true if it is taken over from another writer and false otherwise
bool mcslock_write_read_lock(MCSLock *lock) {
    bool isNodeLocked;
    MCSNode * node = &myMCSNode;
    node->next.value = NULL;
    MCSNode * predecessor = get_and_set_node_ptr(&lock->endOfQueue.value, node);
    if (predecessor != NULL) {
        store_rel(node->locked.value, true);
        store_rel(predecessor->next.value, node);
        load_acq(isNodeLocked, node->locked.value);
        //Wait
        while (isNodeLocked) {
            __sync_synchronize();
            load_acq(isNodeLocked, node->locked.value);
        }
        return true;
    }else{
        return false;
    }
}

void mcslock_write_read_unlock(MCSLock * lock) {
    MCSNode * nextNode;
    MCSNode * node = &myMCSNode;
    load_acq(nextNode, node->next.value);
    if (nextNode == NULL) {
        if (__sync_bool_compare_and_swap(&lock->endOfQueue.value, node, NULL)){
            return;
        }
        //wait
        load_acq(nextNode, node->next.value);
        while (nextNode == NULL) {
             __sync_synchronize();
            load_acq(nextNode, node->next.value);
        }
    }
    store_rel(node->next.value->locked.value, false);
    __sync_synchronize();//Push change
}

void mcslock_read_lock(MCSLock *lock) {
    mcslock_write_read_lock(lock);
}

void mcslock_read_unlock(MCSLock *lock) {
    mcslock_write_read_unlock(lock);
}
