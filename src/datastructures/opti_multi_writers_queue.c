#include <stdlib.h>

#include "opti_multi_writers_queue.h"
#include "utils/smp_utils.h"


OptiMWQueue * omwqueue_create(){
    OptiMWQueue * queue = malloc(sizeof(OptiMWQueue));
    return omwqueue_initialize(queue);
}

OptiMWQueue * omwqueue_initialize(OptiMWQueue * queue){
    for(int i = 0; i < MWQ_CAPACITY; i++){
        queue->elements[i] = NULL;
    }
    queue->elementCount.value = MWQ_CAPACITY;
    queue->closed.value = true;
    __sync_synchronize();
    return queue;
}

void omwqueue_free(OptiMWQueue * queue){
    free(queue);
}

