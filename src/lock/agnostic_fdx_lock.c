#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include "agnostic_fdx_lock.h"
#include "utils/smp_utils.h"
#include "utils/thread_identifier.h"
#include <sched.h>
#define STATICALLY_ALLOCATED_FLAT_COMB_NODES 128

FlatCombNode statically_allocated_fc_nodes[STATICALLY_ALLOCATED_FLAT_COMB_NODES] __attribute__((aligned(128)));

__thread CacheLinePaddedFlatCombNodePtr myFCNode __attribute__((aligned(128)));

AgnosticFDXLock * afdxlock_create(void (*writer)(void *, void **)){
    AgnosticFDXLock * lock = malloc(sizeof(AgnosticFDXLock));
    afdxlock_initialize(lock, writer);
    return lock;
}

void afdxlock_initialize(AgnosticFDXLock * lock, void (*defaultWriter)(void *, void **)){
    lock->defaultWriter = defaultWriter;
    lock->combineCount = 0;
    lock->combineList.value = NULL;
    LOCK_INITIALIZE(&lock->lock, defaultWriter);
    __sync_synchronize();
}

void afdxlock_free(AgnosticFDXLock * lock){
    free(lock);
}

void afdxlock_register_this_thread(){
    FlatCombNode * fcNode;
    assign_id_to_thread();
    if(myId.value < STATICALLY_ALLOCATED_FLAT_COMB_NODES){
        myFCNode.value = &statically_allocated_fc_nodes[myId.value];
    }else{
        myFCNode.value = malloc(sizeof(FlatCombNode));
    }
    fcNode = myFCNode.value;
    fcNode->next = NULL;
    fcNode->request = NULL;
    fcNode->data = NULL;
    fcNode->responseLocation = NULL;
    fcNode->active.value = false;
    __sync_synchronize();
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

#define REPEAT_COMBINE_TIMES 1

void combine_requests(AgnosticFDXLock *lock){
    FlatCombNode * current_elm = ACCESS_ONCE(lock->combineList.value);
    void (*requestFun)(void *, void **);
    void * data;
    void ** responseLocation;
    unsigned long combine_count = lock->combineCount = lock->combineCount + 1;
    if(current_elm == NULL){
        return;
    }
    do{
        load_acq(requestFun, current_elm->request);
        if(requestFun != NULL){
            current_elm->last_used = combine_count;
            load_acq(data, current_elm->data);
            load_acq(responseLocation, current_elm->responseLocation);
            //By setting the request field to NULL we promise to perform the
            //request. This is thus the linearization point for delegate request.
            //The linerarization point for response requests is when a non-NULL
            //response is written to the response location.
            store_rel(current_elm->request, NULL);
            requestFun(data, responseLocation);
        }
        current_elm = current_elm->next;
    }while(current_elm != NULL);
    if((combine_count & (COMBINE_CLEAN_INTERVAL-1)) == 0){
        clean_combine_list(lock->combineList.value, combine_count);
    }
}



void afdxlock_write_with_response(AgnosticFDXLock *lock,
                                  void (*delgateFun)(void *, void **),
                                  void * data,
                                  void ** responseLocation){
    int i, u;
    bool isActive;
    void (*request)(void *, void **);
    FlatCombNode * fcNode = myFCNode.value;
    store_rel(fcNode->data, data);
    store_rel(fcNode->responseLocation, responseLocation);
    store_rel(fcNode->request, delgateFun);
    while(true){
        load_acq(isActive, fcNode->active.value);
        if(!isActive){
            activateFCNode(lock, fcNode);
        }
        for(u = 0; u < 4048; u++){
            if(LOCK_TRY_WRITE_READ_LOCK(&lock->lock)){
                for(int x = 0; x < REPEAT_COMBINE_TIMES; x++){
                    combine_requests(lock);
                }
                LOCK_WRITE_READ_UNLOCK(&lock->lock);
            }
            for(i = 0; i < 128; i++){
                for(int x = 0; x < 1; x++){
                    load_acq(request, fcNode->request);
                    if(request == NULL){
                        return;
                    }
                }
                pause_instruction();
                //__sync_synchronize();//TODO test if pause_instruction() is better
            }
            sched_yield();
        }
    }
}


void * afdxlock_write_with_response_block(AgnosticFDXLock *lock, 
                                          void  (*delgateFun)(void *, void **), 
                                          void * data){
    void * returnValue = NULL;
    void * currentValue = NULL;
    int i, u;
    bool isActive;
    FlatCombNode * fcNode = myFCNode.value;
    store_rel(fcNode->data, data);
    store_rel(fcNode->responseLocation, &returnValue);
    store_rel(fcNode->request, delgateFun);
    while(true){
        load_acq(isActive, fcNode->active.value);
        if(!isActive){
            activateFCNode(lock, fcNode);
        }
        for(u = 0; u < 4048; u++){
            if(LOCK_TRY_WRITE_READ_LOCK(&lock->lock)){
                for(int x = 0; x < REPEAT_COMBINE_TIMES; x++){
                    combine_requests(lock);
                }
                LOCK_WRITE_READ_UNLOCK(&lock->lock);
            }
            for(i = 0; i < 128; i++){
                for(int x = 0; x < 1; x++){
                    load_acq(currentValue, returnValue);
                    if(currentValue != NULL){
                        return currentValue;
                    }
                }
                //pause_instruction();
                __sync_synchronize();//TODO test if pause_instruction() is better
            }
            sched_yield();
        }
    }
    return currentValue;
} 

void afdxlock_write(AgnosticFDXLock *lock, void * writeInfo) {
    afdxlock_delegate(lock, lock->defaultWriter, writeInfo);
}

void afdxlock_write_read_lock(AgnosticFDXLock *lock) {
    LOCK_WRITE_READ_LOCK(&lock->lock);
}

void afdxlock_write_read_unlock(AgnosticFDXLock * lock) {
    LOCK_WRITE_READ_UNLOCK(&lock->lock);
}

void afdxlock_read_lock(AgnosticFDXLock *lock) {
    afdxlock_write_read_lock(lock);
}

void afdxlock_read_unlock(AgnosticFDXLock *lock) {
    afdxlock_write_read_unlock(lock);
}
