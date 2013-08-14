#include <stdio.h>
#include <stdlib.h>

#include "datastructures/multi_writers_queue.h"
#include "test_framework.h"


int test_create(){

    MWQueue * test = mwqueue_create();
    mwqueue_free(test);
    return 1;

}

int test_offer(){
    {
        MWQueue * queue = mwqueue_create();
        mwqueue_reset_fully_read(queue);
        for(void * i = NULL; i < (void*)(MWQ_CAPACITY/2); i++){
            mwqueue_offer(queue, i);
        } 

        mwqueue_free(queue);
    }
    {
        MWQueue * queue = mwqueue_create();
        mwqueue_reset_fully_read(queue);
        for(void * i = NULL; i < (void*)(MWQ_CAPACITY*2); i++){
            mwqueue_offer(queue, i);
        }

        mwqueue_free(queue);
    }
    return 1;
}


int test_offer_and_take(){
    {
        MWQueue * queue = mwqueue_create();
        mwqueue_reset_fully_read(queue);
        for(void * i = (void*)1; i <= (void*)(MWQ_CAPACITY/2); i++){
            mwqueue_offer(queue, i);
        } 

        for(int i = 1; i <= (MWQ_CAPACITY/2); i++){
            assert(NULL != mwqueue_take(queue));
        }

        assert(NULL == mwqueue_take(queue));

        mwqueue_free(queue);
    }
    {
        MWQueue * queue = mwqueue_create();
        mwqueue_reset_fully_read(queue);
        for(void * i = (void*)1; i <= (void*)(MWQ_CAPACITY * 2); i++){
            mwqueue_offer(queue, i);
        }

        for(void * i = 0; i < TO_VP(MWQ_CAPACITY); i++){
            assert(NULL != mwqueue_take(queue));
        } 

        assert(NULL == mwqueue_take(queue));

        mwqueue_free(queue);
    }
    return 1;
}


int main(int argc, char **argv){
    
    printf("\n\n\n\033[32m ### STARTING MULTI WRITES QUEUE TESTS! -- \033[m\n\n\n");

    T(test_create(), "test_create()");

    T(test_offer(), "test_offer()");

    T(test_offer_and_take(), "test_offer_and_take()");

    printf("\n\n\n\033[32m ### MULTI WRITES QUEUE COMPLETED! -- \033[m\n\n\n");

    exit(0);

}

