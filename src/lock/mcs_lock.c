#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

#include "mcs_lock.h"


inline
MCSNode * get_and_set_node_ptr(MCSNode ** pointerToOldValue, MCSNode * newValue){
    MCSNode * x = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
        x = ACCESS_ONCE(*pointerToOldValue);
    }
}

__thread MCSNode myMCSNode __attribute__((aligned(64)));
 
MCSLock * mcslock_create(void (*writer)(void *)){
    MCSLock * lock = malloc(sizeof(MCSLock));
    mcslock_initialize(lock, writer);
    return lock;
}

void mcslock_initialize(MCSLock * lock, void (*writer)(void *)){
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
    lock->writer(writeInfo);
    mcslock_write_read_unlock(lock);
    
}

void mcslock_write_read_lock(MCSLock *lock) {
    MCSNode * node = &myMCSNode;
    MCSNode * predecessor = get_and_set_node_ptr(&lock->endOfQueue.value, node);
    if (predecessor != NULL) {
        node->locked.value = true;
        predecessor->next.value = node;
        __sync_synchronize();
        //Wait
        while (ACCESS_ONCE(node->locked.value)) {
            __sync_synchronize();
        }
    }
}

void mcslock_write_read_unlock(MCSLock * lock) {
    MCSNode * node = &myMCSNode;
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
    node->next.value = NULL;
    __sync_synchronize();
}

void mcslock_read_lock(MCSLock *lock) {
    mcslock_write_read_lock(lock);
}

void mcslock_read_unlock(MCSLock *lock) {
    mcslock_write_read_unlock(lock);
}
