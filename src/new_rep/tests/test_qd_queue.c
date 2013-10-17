
#include <stdio.h>
#include <stdlib.h>
#include "misc/bsd_stdatomic.h"//Until c11 stdatomic.h is available

#include "qd_queues/qd_queue.h"
#include "tests/test_framework.h"
#include "misc/random.h"

int test_initialize(){

    QDQueue test;
    qdq_initialize(&test);
    return 1;

}
volatile atomic_ulong counter = ATOMIC_VAR_INIT(0);
void critical_section(unsigned int messageSize, void * message){
    if(messageSize == 0 && message != NULL){ /* Prevent warning */
        atomic_fetch_add(&counter, 1);
    }else{
        assert(false);
    }
}
int test_enqueue(int nrOfEnqueues){   
    QDQueue queue;
    qdq_initialize(&queue);
    qdq_open(&queue);
    for(int i = 0; i < nrOfEnqueues; i++){
        qdq_enqueue(&queue, critical_section, 0, NULL);
    }
    return 1;
}

int test_enqueue_and_flush(int nrOfEnqueues){
    atomic_store(&counter, 0);
    QDQueue queue;
    qdq_initialize(&queue);
    qdq_open(&queue);
    unsigned long enqueueCounter = 0;
    for(int i = 0; i < nrOfEnqueues; i++){
        if(qdq_enqueue(&queue, critical_section, 0, NULL)){
            enqueueCounter = enqueueCounter + 1;
        }
    }
    qdq_flush(&queue);
    assert(atomic_load(&counter) == enqueueCounter);
    return 1;
}

void variable_message_size_cs(unsigned int messageSize, void * message){
    unsigned char * messageBytes = (unsigned char *)message;
    for(unsigned int i = 0; i < messageSize; i++){
        assert(((unsigned int)messageBytes[i]) == messageSize);
    }
    atomic_fetch_add(&counter, 1);
}
int test_variable_message_sizes(int nrOfEnqueues){
    atomic_store(&counter, 0);
    QDQueue queue;
    qdq_initialize(&queue);
    qdq_open(&queue);
    unsigned int seed = 0;
    unsigned long enqueueCounter = 0;
    for(int i = 0; i < nrOfEnqueues; i++){
        unsigned int messageSize = (unsigned int)(15.0*random_double(&seed));
        char messageBuffer[messageSize];
        for(unsigned int i = 0; i < messageSize; i++){
            messageBuffer[i] = (unsigned char)messageSize;
        }
        if(qdq_enqueue(&queue, variable_message_size_cs, messageSize, messageBuffer)){
            enqueueCounter = enqueueCounter + 1;
        }
    }
    qdq_flush(&queue);
    assert(atomic_load(&counter) == enqueueCounter);
    return 1;
}

int main(/*int argc, char **argv*/){
    
    printf("\n\n\n\033[32m ### STARTING QD QUEUE TESTS! -- \033[m\n\n\n");

    T(test_initialize(), "test_initialize()");

    T(test_enqueue(1), "test_enqueue(nrOfEnqueues = 1)");
    T(test_enqueue(15), "test_enqueue(nrOfEnqueues = 2)");
    T(test_enqueue(15), "test_enqueue(nrOfEnqueues = 15)");
    T(test_enqueue(QD_QUEUE_BUFFER_SIZE*2), "test_enqueue(nrOfEnqueues = QD_QUEUE_BUFFER_SIZE*2)");

    T(test_enqueue_and_flush(1), "test_enqueue_and_flush(nrOfEnqueues = 1)");
    T(test_enqueue_and_flush(2), "test_enqueue_and_flush(nrOfEnqueues = 2)");
    T(test_enqueue_and_flush(15), "test_enqueue_and_flush(nrOfEnqueues = 15)");
    T(test_enqueue_and_flush(QD_QUEUE_BUFFER_SIZE*2), "test_enqueue_and_flush(nrOfEnqueues = QD_QUEUE_BUFFER_SIZE*2)");

    T(test_variable_message_sizes(1), "test_variable_message_sizes(nrOfEnqueues = 1)");
    T(test_variable_message_sizes(2), "test_variable_message_sizes(nrOfEnqueues = 2)");
    T(test_variable_message_sizes(15), "test_variable_message_sizes(nrOfEnqueues = 15)");
    T(test_variable_message_sizes(QD_QUEUE_BUFFER_SIZE*2), "test_variable_message_sizes(nrOfEnqueues = QD_QUEUE_BUFFER_SIZE*2)");

    printf("\n\n\n\033[32m ### QD QUEUE COMPLETED! -- \033[m\n\n\n");

    exit(0);

}
