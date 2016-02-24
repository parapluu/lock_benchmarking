#include "flat_comb_rdx_lock.h"
#include <stdlib.h>
#include "utils/thread_identifier.h"

#define READ_PATIENCE_LIMIT 1000

#define STATICALLY_ALLOCATED_FLAT_COMB_NODES 128

FlatCombNode statically_allocated_fc_nodes[STATICALLY_ALLOCATED_FLAT_COMB_NODES];

__thread CacheLinePaddedFlatCombNodePtr myFCNode __attribute__((aligned(64)));
__thread FCMCSNode myFCMCSNode __attribute__((aligned(64)));

static inline
bool isWriteLocked(FlatCombRDXLock * lock){
    FCMCSNode * endOfMCSQueue;
    load_acq(endOfMCSQueue, lock->endOfMCSQueue.value);
    return endOfMCSQueue != NULL;
}

static inline
FCMCSNode * get_and_set_node_ptr(FCMCSNode ** pointerToOldValue, FCMCSNode * newValue){
    FCMCSNode * x = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
        x = ACCESS_ONCE(*pointerToOldValue);
    }
}

static inline
bool try_set_node_ptr(FCMCSNode ** pointerToOldValue, FCMCSNode * newValue){
    if (__sync_bool_compare_and_swap(pointerToOldValue, NULL, newValue)){
        return true;
    } else {
        return false;
    }     
}

FlatCombRDXLock * fcrdxlock_create(void (*writer)(void *, void **)){
    FlatCombRDXLock * lock = malloc(sizeof(FlatCombRDXLock));
    fcrdxlock_initialize(lock, writer);
    return lock;
}

void fcrdxlock_initialize(FlatCombRDXLock * lock, void (*writer)(void *, void **)){
    lock->writer = writer;
    lock->combine_count = 0;
    lock->combine_list.value = NULL;
    lock->endOfMCSQueue.value = NULL;
    NZI_INITIALIZE(&lock->nonZeroIndicator);
    lock->writeBarrier.value = 0;
    __sync_synchronize();
}

void fcrdxlock_free(FlatCombRDXLock * lock){
    free(lock);
}

void fcrdxlock_register_this_thread(){
    FlatCombNode * fcNode;
    FCMCSNode * fcMCSNode = &myFCMCSNode;
    assign_id_to_thread();
    if(myId.value < STATICALLY_ALLOCATED_FLAT_COMB_NODES){
        myFCNode.value = &statically_allocated_fc_nodes[myId.value];
    }else{
        myFCNode.value = malloc(sizeof(FlatCombNode));
    }
    fcNode = myFCNode.value;
    fcMCSNode->next.value = NULL;
    fcMCSNode->locked.value = false;
    fcNode->next = NULL;
    fcNode->request = NULL;
    fcNode->active.value = false;
    __sync_synchronize();        
}

static inline
void activateFCNode(FlatCombRDXLock *lock, FlatCombNode * fcNode){
    fcNode->active.value = true;
    FlatCombNode ** pointerToOldValue = &lock->combine_list.value;
    FlatCombNode * oldValue = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        fcNode->next = oldValue;
        if (__sync_bool_compare_and_swap(pointerToOldValue, oldValue, fcNode))
            return;
        oldValue = ACCESS_ONCE(*pointerToOldValue);
    }
}

//Multiple of 2
#define COMBINE_CLEAN_INTERVAL 131072

void clean_combine_list(FlatCombNode * combine_list, 
                        unsigned long combine_count){
    FlatCombNode * prev = combine_list;
    FlatCombNode * current = combine_list->next;
    while(current != NULL){
        if((combine_count - current->last_used) > COMBINE_CLEAN_INTERVAL){
            store_rel(prev->next, current->next);
            store_rel(current->active.value, false);
            current = prev->next;
        }else{
            prev = current;
            current = prev->next;
        }
    }
}

void combine_requests(FlatCombRDXLock *lock){
    FlatCombNode * current_elm = ACCESS_ONCE(lock->combine_list.value);
    void * request_value;
    unsigned long combine_count = lock->combine_count = lock->combine_count + 1;
    if(current_elm == NULL){
        return;
    }
    do{
        request_value = ACCESS_ONCE(current_elm->request);
        if(request_value != NULL){
            lock->writer(current_elm->request, NULL);
            current_elm->request = NULL;
            current_elm->last_used = combine_count;
        }
        current_elm = current_elm->next;
    }while(current_elm != NULL);
    if((combine_count & (COMBINE_CLEAN_INTERVAL-1)) == 0){
        clean_combine_list(lock->combine_list.value, combine_count);
    }
}

static inline
bool try_write_read_lock(FlatCombRDXLock *lock) {
    bool writeBarrierOn;
    load_acq(writeBarrierOn, lock->writeBarrier.value);
    if(writeBarrierOn){
        return false;
    }
    FCMCSNode * node = &myFCMCSNode;
    node->next.value = NULL;
    if(ACCESS_ONCE(lock->endOfMCSQueue.value) == NULL &&
       try_set_node_ptr(&lock->endOfMCSQueue.value, node)){
        NZI_WAIT_UNIL_EMPTY(&lock->nonZeroIndicator);
        return true;
    }else{
        return false;
    }
}

void fcrdxlock_write(FlatCombRDXLock *lock, void * writeInfo) {
    FlatCombNode * fcNode = myFCNode.value;
    fcNode->request = writeInfo;
    int i, u;
    while(true){
        if(!ACCESS_ONCE(fcNode->active.value)){
            activateFCNode(lock, fcNode);
        }
        for(u = 0; u < 4048; u++){
            if(try_write_read_lock(lock)){
                combine_requests(lock);
                fcrdxlock_write_read_unlock(lock);
            }
            for(i = 0; i < 32; i++){
                if(ACCESS_ONCE(fcNode->request) == NULL){
                    return;
                }
                __sync_synchronize();//TODO test if pause_instruction() is better
            }
        }
    }
}

void fcrdxlock_write_read_lock(FlatCombRDXLock *lock) {
    bool isNodeLocked;
    FCMCSNode * node = &myFCMCSNode;
    bool writeBarrierOn;
    load_acq(writeBarrierOn, lock->writeBarrier.value);
    while(writeBarrierOn){
        __sync_synchronize();
        load_acq(writeBarrierOn, lock->writeBarrier.value);
    }
    node->next.value = NULL;
    FCMCSNode * predecessor = get_and_set_node_ptr(&lock->endOfMCSQueue.value, node);
    if (predecessor != NULL) {
        store_rel(node->locked.value, true);
        store_rel(predecessor->next.value, node);
        load_acq(isNodeLocked, node->locked.value);
        //Wait
        while (isNodeLocked) {
            __sync_synchronize();
            load_acq(isNodeLocked, node->locked.value);
        }
    }else{
        NZI_WAIT_UNIL_EMPTY(&lock->nonZeroIndicator);
    }
    combine_requests(lock);
}

void fcrdxlock_write_read_unlock(FlatCombRDXLock * lock) {
    FCMCSNode * nextNode;
    FCMCSNode * node = &myFCMCSNode;
    load_acq(nextNode, node->next.value);
    if (nextNode == NULL) {
        if (__sync_bool_compare_and_swap(&lock->endOfMCSQueue.value, node, NULL)){
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

void fcrdxlock_read_lock(FlatCombRDXLock *lock) {
    bool bRaised = false; 
    int readPatience = 0;
 start:
    NZI_ARRIVE(&lock->nonZeroIndicator);
    if(isWriteLocked(lock)){
        NZI_DEPART(&lock->nonZeroIndicator);
        while(isWriteLocked(lock)){
            __sync_synchronize();//Pause (pause instruction might be better)
            if((readPatience == READ_PATIENCE_LIMIT) && !bRaised){
                __sync_fetch_and_add(&lock->writeBarrier.value, 1);
                bRaised = true;
            }
            readPatience = readPatience + 1;
        }
        goto start;
    }
    if(bRaised){
        __sync_fetch_and_sub(&lock->writeBarrier.value, 1);
    }
}

void fcrdxlock_read_unlock(FlatCombRDXLock *lock) {
    NZI_DEPART(&lock->nonZeroIndicator);
}
