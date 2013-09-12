#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "test_framework.h"
#include "utils/smp_utils.h"
#include "utils/support_many_lock_types.h"


CacheLinePaddedPointer test_write_var __attribute__((aligned(128)))  = {.value = NULL};


__thread CacheLinePaddedInt numa_node;

void test_writer(void * pointer_increment, void **writeBackLocation){
    void * inital_test_write_var = test_write_var.value;
    test_write_var.value = test_write_var.value + (long)pointer_increment;
    assert(test_write_var.value == (inital_test_write_var+(long)pointer_increment));
}

int test_create(){
    LOCK_REGISTER_THIS_THREAD();
    LOCK_DATATYPE_NAME * lock = LOCK_CREATE(&test_writer);
    LOCK_FREE(lock);
    return 1;
}

int test_write_lock(){
    test_write_var.value = NULL;
    LOCK_DATATYPE_NAME * lock = LOCK_CREATE(&test_writer);
    LOCK_REGISTER_THIS_THREAD();
    LOCK_WRITE(lock, TO_VP(1));
    LOCK_FREE(lock);
    assert(test_write_var.value == TO_VP(1));
    return 1;
}


int test_write_lock_and_read_lock(){
    long incTo = 100;
    test_write_var.value = NULL;
    LOCK_DATATYPE_NAME * lock = LOCK_CREATE(&test_writer);
    LOCK_REGISTER_THIS_THREAD();
    for(long i = 1; i <= incTo; i++){
        LOCK_WRITE(lock, TO_VP(1));
        LOCK_READ_LOCK(lock);
        assert(test_write_var.value == TO_VP(i));
        LOCK_READ_UNLOCK(lock);
    }
    LOCK_FREE(lock);
    assert(test_write_var.value == TO_VP(incTo));
    return 1;
}

CacheLinePaddedBool read_lock_in_child __attribute__((aligned(128))) = {.value = false};

CacheLinePaddedBool blocking_thread_unblock __attribute__((aligned(128))) = {.value = false} ;

CacheLinePaddedBool blocking_thread_child_has_written __attribute__((aligned(128))) = {.value = false};

void *blocking_thread_child(void *x){
    
    LOCK_DATATYPE_NAME * lock = (LOCK_DATATYPE_NAME *) x;
    LOCK_REGISTER_THIS_THREAD();
    if(read_lock_in_child.value){
        LOCK_READ_LOCK(lock);
    }else{
        LOCK_WRITE_READ_LOCK(lock);
    }
    blocking_thread_child_has_written.value = true;
    __sync_synchronize();
    if(read_lock_in_child.value){
        LOCK_READ_UNLOCK(lock);
    }else{
        LOCK_WRITE_READ_UNLOCK(lock);
    }

    pthread_exit(0); 
}

void *blocking_thread(void *x){
    LOCK_DATATYPE_NAME * lock = (LOCK_DATATYPE_NAME *) x;
    LOCK_REGISTER_THIS_THREAD();
    LOCK_WRITE_READ_LOCK(lock);
    pthread_t thread;
    pthread_create(&thread,NULL,&blocking_thread_child,lock);

    __sync_synchronize();

    while(!ACCESS_ONCE(blocking_thread_unblock.value)){__sync_synchronize();}

    LOCK_WRITE_READ_UNLOCK(lock);

    pthread_join(thread,NULL);
    
    pthread_exit(0); 
}

int test_read_write_lock_is_blocking_other_lock(){
    LOCK_REGISTER_THIS_THREAD();
    for(int i = 0; i < 10; i ++){
        blocking_thread_unblock.value = false;
        blocking_thread_child_has_written.value = false;
        LOCK_DATATYPE_NAME * lock = LOCK_CREATE(&test_writer);
        pthread_t thread;
        pthread_create(&thread,NULL,&blocking_thread,lock);
        __sync_synchronize();
        assert(false==blocking_thread_child_has_written.value);
        blocking_thread_unblock.value = true;
        __sync_synchronize();
        pthread_join(thread,NULL);
        assert(blocking_thread_child_has_written.value);
        LOCK_FREE(lock);
    }
    return 1;
}

int test_read_write_lock_is_blocking_other_read_write_lock(){
    read_lock_in_child.value = false;
    return test_read_write_lock_is_blocking_other_lock();
}

int test_read_write_lock_is_blocking_other_read_lock(){
    read_lock_in_child.value = true;
    return test_read_write_lock_is_blocking_other_lock();
}



#define MICRO_SECONDS_TO_RUN_TEST 1000000

CacheLinePaddedBool test_phase_is_on __attribute__((aligned(128))) = {.value = false};
CacheLinePaddedInt count __attribute__((aligned(128))) = {.value = 0};
CacheLinePaddedDouble __attribute__((aligned(128))) percentageRead = {.value = 0.8}; 
CacheLinePaddedDouble __attribute__((aligned(128))) percentageExclusive = {.value = 0.0};

typedef struct LockCounterImpl {
    unsigned short xsubi[3];
    LOCK_DATATYPE_NAME * lock;
    int logicalWritesInFuture;
    char pad[128];
    int writesInFuture;
    bool pendingWrite;
} LockCounter;

LockCounter lock_counters[NUMBER_OF_HARDWARE_THREADS] __attribute__((aligned(128)));


void mixed_read_write_test_writer(void * lock_counter, void ** writeBackLocation){
    LockCounter * lc = (LockCounter *) lock_counter;
    lc->writesInFuture = ACCESS_ONCE(lc->writesInFuture) + 1;
    __sync_synchronize();
    count.value = ACCESS_ONCE(count.value) + 1;
    __sync_synchronize();
    int currentCount = ACCESS_ONCE(count.value);
    //usleep(100);
    __sync_synchronize();
    assert(currentCount == ACCESS_ONCE(count.value));
    lc->pendingWrite = false;
}


void *mixed_read_write_thread(void *x){
    double randomNumber;
    double delegateLimit = percentageRead.value + percentageExclusive.value;
    LOCK_REGISTER_THIS_THREAD();
    LockCounter * lc = (LockCounter *) x;
    LOCK_DATATYPE_NAME * lock = lc->lock;
    while(test_phase_is_on.value){
        randomNumber = erand48(lc->xsubi);
        if(randomNumber > delegateLimit){
            //printf("D");
            lc->pendingWrite = true;
            LOCK_WRITE(lock, lc);
            lc->logicalWritesInFuture = lc->logicalWritesInFuture + 1;
        }else if(randomNumber > percentageRead.value){
            //printf("X");
            lc->pendingWrite = true;
            LOCK_WRITE_READ_LOCK(lock);
            mixed_read_write_test_writer(lc, NULL);
            LOCK_WRITE_READ_UNLOCK(lock);
            lc->logicalWritesInFuture = lc->logicalWritesInFuture + 1;
        }else{
            //printf("R");
            LOCK_READ_LOCK(lock);
            assert(!ACCESS_ONCE(lc->pendingWrite));
            assert(lc->logicalWritesInFuture==ACCESS_ONCE(lc->writesInFuture));
            int currentCount = ACCESS_ONCE(count.value);
            //usleep(100);
            assert(currentCount == ACCESS_ONCE(count.value));
            LOCK_READ_UNLOCK(lock);
        }
    }
    LOCK_READ_LOCK(lock);
    LOCK_READ_UNLOCK(lock);
    assert(ACCESS_ONCE(lc->writesInFuture) == lc->logicalWritesInFuture);
    pthread_exit(0); 
}

int test_parallel_mixed_read_write(double percentageReadParam, double percentageExclusiveParam){
    percentageRead.value = percentageReadParam;
    percentageExclusive.value = percentageExclusiveParam;
    count.value = 0;
    pthread_t threads[NUMBER_OF_HARDWARE_THREADS];
    LOCK_DATATYPE_NAME * lock = LOCK_CREATE(&mixed_read_write_test_writer);
    store_rel(test_phase_is_on.value, true);
    __sync_synchronize();

    for(int i = 0; i < NUMBER_OF_HARDWARE_THREADS; i ++){
        LockCounter *lc = &lock_counters[i];
        lc->lock = lock;
        srand48(i);
        unsigned short* xsubi = seed48(lc->xsubi);
        lc->xsubi[0] = xsubi[0];
        lc->xsubi[1] = xsubi[1];
        lc->xsubi[2] = xsubi[2];
        lc->logicalWritesInFuture = 0;
        lc->writesInFuture = 0;
        lc->pendingWrite = false;

        pthread_create(&threads[i],NULL,&mixed_read_write_thread,lc);
    }

    usleep(MICRO_SECONDS_TO_RUN_TEST);

    store_rel(test_phase_is_on.value, false);
    __sync_synchronize();

    int totalNumOfWrites = 0;
    for(int i = 0; i < NUMBER_OF_HARDWARE_THREADS; i ++){
        pthread_join(threads[i],NULL);
        totalNumOfWrites = totalNumOfWrites + ACCESS_ONCE(lock_counters[i].writesInFuture);
    }

    assert(totalNumOfWrites == count.value);

    LOCK_FREE(lock);
    return 1;
}



int main(int argc, char **argv){
    
    printf("\n\n\n\033[32m ### STARTING LOCK TESTS! -- \033[m\n\n\n");

    T(test_create(), "test_create()");

    T(test_write_lock(), "test_write_lock()");

    T(test_write_lock_and_read_lock(), "test_write_lock_and_read_lock()");

    T(test_read_write_lock_is_blocking_other_read_write_lock(), "test_read_write_lock_is_blocking_other_read_write_lock()");

    T(test_read_write_lock_is_blocking_other_read_lock(), "test_read_write_lock_is_blocking_other_read_lock()");

    //READ ONLY
    
    T(test_parallel_mixed_read_write(1.0, 0.0),"test_parallel_mixed_read_write(1.0, 0.0)");

    //DELEGATE ONLY

    T(test_parallel_mixed_read_write(0.0, 0.0),"test_parallel_mixed_read_write(0.0, 0.0)");

    //EXLUSIVE ONLY

    T(test_parallel_mixed_read_write(0.0, 1.0),"test_parallel_mixed_read_write(0.0, 1.0)");

    //DELEGATE AND READ

    T(test_parallel_mixed_read_write(0.95, 0.0),"test_parallel_mixed_read_write(0.95, 0.0)");

    T(test_parallel_mixed_read_write(0.9, 0.0),"test_parallel_mixed_read_write(0.9, 0.0)");

    T(test_parallel_mixed_read_write(0.8, 0.0),"test_parallel_mixed_read_write(0.8, 0.0)");

    T(test_parallel_mixed_read_write(0.5, 0.0),"test_parallel_mixed_read_write(0.5, 0.0)");

    T(test_parallel_mixed_read_write(0.3, 0.0),"test_parallel_mixed_read_write(0.3, 0.0)");

    T(test_parallel_mixed_read_write(0.1, 0.0),"test_parallel_mixed_read_write(0.1, 0.0)");

    //READ AND EXCLUSIVE

    T(test_parallel_mixed_read_write(0.95, 0.05),"test_parallel_mixed_read_write(0.95, 0.05)");

    T(test_parallel_mixed_read_write(0.9, 0.1),"test_parallel_mixed_read_write(0.9, 0.1)");

    T(test_parallel_mixed_read_write(0.8, 0.2),"test_parallel_mixed_read_write(0.8, 0.2)");

    T(test_parallel_mixed_read_write(0.5, 0.5),"test_parallel_mixed_read_write(0.5, 0.5)");

    T(test_parallel_mixed_read_write(0.3, 0.7),"test_parallel_mixed_read_write(0.3, 0.7)");

    T(test_parallel_mixed_read_write(0.1, 0.9),"test_parallel_mixed_read_write(0.1, 0.9)");

    T(test_parallel_mixed_read_write(0.01, 0.99),"test_parallel_mixed_read_write(0.01, 0.99)");

    //DELEGATE AND EXCLUSIVE

    T(test_parallel_mixed_read_write(0.0, 0.05),"test_parallel_mixed_read_write(0.0, 0.05)");

    T(test_parallel_mixed_read_write(0.0, 0.1),"test_parallel_mixed_read_write(0.0, 0.1)");

    T(test_parallel_mixed_read_write(0.0, 0.2),"test_parallel_mixed_read_write(0.0, 0.2)");

    T(test_parallel_mixed_read_write(0.0, 0.5),"test_parallel_mixed_read_write(0.0, 0.5)");

    T(test_parallel_mixed_read_write(0.0, 0.7),"test_parallel_mixed_read_write(0.0, 0.7)");

    T(test_parallel_mixed_read_write(0.0, 0.9),"test_parallel_mixed_read_write(0.0, 0.9)");

    T(test_parallel_mixed_read_write(0.0, 0.95),"test_parallel_mixed_read_write(0.0, 0.95)");

    T(test_parallel_mixed_read_write(0.0, 0.99),"test_parallel_mixed_read_write(0.0, 0.99)");

    //MIXED

    T(test_parallel_mixed_read_write(0.33, 0.33),"test_parallel_mixed_read_write(0.33, 0.33)");

    T(test_parallel_mixed_read_write(0.05, 0.8),"test_parallel_mixed_read_write(0.05, 0.8)");

    T(test_parallel_mixed_read_write(0.45, 0.5),"test_parallel_mixed_read_write(0.45, 0.5)");

    T(test_parallel_mixed_read_write(0.8, 0.1),"test_parallel_mixed_read_write(0.8, 0.1)");

    T(test_parallel_mixed_read_write(0.7, 0.05),"test_parallel_mixed_read_write(0.7, 0.05)");

    T(test_parallel_mixed_read_write(0.4, 0.2),"test_parallel_mixed_read_write(0.4, 0.2)");

    printf("\n\n\n\033[32m ### LOCK TESTS COMPLETED! -- \033[m\n\n\n");

    exit(0);

}

