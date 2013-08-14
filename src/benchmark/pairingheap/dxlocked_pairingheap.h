#include "pairingheap.h"
#include "utils/support_many_lock_types.h"

#ifndef DXLOCKED_PAIRINGHEAP_H
#define DXLOCKED_PAIRINGHEAP_H

typedef struct DXPriorityQueueImpl{
    char pad1[128];
    struct node* value;
    char pad2[128 - (sizeof(struct node*))];
} DXPriorityQueue;

DXPriorityQueue dx_pq_ph_datastructure __attribute__((aligned(64)));

LOCK_DATATYPE_NAME dx_pq_ph_lock __attribute__((aligned(64)));


void dx_pq_ph_init(){
    LOCK_INITIALIZE(&dx_pq_ph_lock, NULL);//Default write function not used
    dx_pq_ph_datastructure.value = NULL;
}

void dx_pq_ph_destroy(){
    destroy_heap(dx_pq_ph_datastructure.value);
}

void dx_pq_ph_enqueue_critical_section(void * enqueueValue, void ** notUsed){
    dx_pq_ph_datastructure.value = 
        insert(dx_pq_ph_datastructure.value, (int)(long)enqueueValue);    
}

void dx_pq_ph_enqueue(int value){
    LOCK_DELEGATE(&dx_pq_ph_lock, &dx_pq_ph_enqueue_critical_section, (void*)(long)value); 
}

void dx_pq_ph_dequeue_critical_section(void * notUsed, void ** resultLocationPtr){
    int * resultLocation = (int*)resultLocationPtr;
    if(dx_pq_ph_datastructure.value != NULL){
        *resultLocation = top(dx_pq_ph_datastructure.value);
        dx_pq_ph_datastructure.value = pop(dx_pq_ph_datastructure.value);
    }else{
        *resultLocation = -1;
    }
}

int dx_pq_ph_dequeue(){
   return (int)(long)LOCK_DELEGATE_RETURN_BLOCK(&dx_pq_ph_lock, 
                                                &dx_pq_ph_dequeue_critical_section, 
                                                NULL);
}

#endif
