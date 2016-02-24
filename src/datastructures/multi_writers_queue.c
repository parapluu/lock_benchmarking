#include <stdlib.h>

#include "multi_writers_queue.h"
#include "utils/smp_utils.h"

MWQueue * mwqueue_create(){
    MWQueue * queue = malloc(sizeof(MWQueue));
    return mwqueue_initialize(queue);
}


void mwqueue_free(MWQueue * queue){
    free(queue);
}

