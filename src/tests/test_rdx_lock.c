#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "test_framework.h"
#include "smp_utils.h"
#include "support_many_lock_types.h"


void * test_write_var = NULL;

void test_writer(void * pointer_increment){
    void * inital_test_write_var = test_write_var;
    test_write_var = test_write_var + (long)pointer_increment;
    assert(test_write_var == (inital_test_write_var+(long)pointer_increment));
}

int test_create(){
    LOCK_REGISTER_THIS_THREAD();
    LOCK_DATATYPE_NAME * lock = LOCK_CREATE(&test_writer);
    LOCK_FREE(lock);
    return 1;
}

int test_write_lock(){
    test_write_var = NULL;
    LOCK_REGISTER_THIS_THREAD();
    LOCK_DATATYPE_NAME * lock = LOCK_CREATE(&test_writer);
    LOCK_WRITE(lock, TO_VP(1));
    LOCK_FREE(lock);
    assert(test_write_var == TO_VP(1));
    return 1;
}


int test_write_lock_and_read_lock(){
    long incTo = 100;
    test_write_var = NULL;
    LOCK_DATATYPE_NAME * lock = LOCK_CREATE(&test_writer);
    LOCK_REGISTER_THIS_THREAD();
    for(long i = 1; i <= incTo; i++){
        LOCK_WRITE(lock, TO_VP(1));
        LOCK_READ_LOCK(lock);
        assert(test_write_var == TO_VP(i));
        LOCK_READ_UNLOCK(lock);
    }
    LOCK_FREE(lock);
    assert(test_write_var == TO_VP(incTo));
    return 1;
}

bool read_lock_in_child = false;

bool blocking_thread_unblock = false;

bool blocking_thread_child_has_written = false;

void *blocking_thread_child(void *x){
    
    LOCK_DATATYPE_NAME * lock = (LOCK_DATATYPE_NAME *) x;
    LOCK_REGISTER_THIS_THREAD();
    if(read_lock_in_child){
        LOCK_READ_LOCK(lock);
    }else{
        LOCK_WRITE_READ_LOCK(lock);
    }
    blocking_thread_child_has_written = true;
    __sync_synchronize();
    if(read_lock_in_child){
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

    while(!ACCESS_ONCE(blocking_thread_unblock)){__sync_synchronize();}

    LOCK_WRITE_READ_UNLOCK(lock);

    pthread_join(thread,NULL);
    
    pthread_exit(0); 
}

int test_read_write_lock_is_blocking_other_lock(){
    LOCK_REGISTER_THIS_THREAD();
    for(int i = 0; i < 10; i ++){
        blocking_thread_unblock = false;
        blocking_thread_child_has_written = false;
        LOCK_DATATYPE_NAME * lock = LOCK_CREATE(&test_writer);
        pthread_t thread;
        pthread_create(&thread,NULL,&blocking_thread,lock);
        __sync_synchronize();
        assert(false==blocking_thread_child_has_written);
        blocking_thread_unblock = true;
        __sync_synchronize();
        pthread_join(thread,NULL);
        assert(blocking_thread_child_has_written);
        LOCK_FREE(lock);
    }
    return 1;
}

int test_read_write_lock_is_blocking_other_read_write_lock(){
    read_lock_in_child = false;
    return test_read_write_lock_is_blocking_other_lock();
}

int test_read_write_lock_is_blocking_other_read_lock(){
    read_lock_in_child = true;
    return test_read_write_lock_is_blocking_other_lock();
}


int numberOfOperationsPerThread = 1000000;
#define NUMBER_OF_THREADS 2
int count = 0;
double percentageRead = 0.8;


typedef struct LockCounterImpl {
    unsigned short xsubi[3];
    LOCK_DATATYPE_NAME * lock;
    int logicalWritesInFuture;
    char pad[128];
    int writesInFuture;
    bool pendingWrite;
} LockCounter;

LockCounter lock_counters[NUMBER_OF_THREADS] __attribute__((aligned(128)));


void mixed_read_write_test_writer(void * lock_counter){
    LockCounter * lc = (LockCounter *) lock_counter;
    lc->writesInFuture = ACCESS_ONCE(lc->writesInFuture) + 1;
    __sync_synchronize();
    count = ACCESS_ONCE(count) + 1;
    __sync_synchronize();
    int currentCount = ACCESS_ONCE(count);
    //usleep(100);
    __sync_synchronize();
    assert(currentCount == ACCESS_ONCE(count));
    lc->pendingWrite = false;
}


void *mixed_read_write_thread(void *x){
    LOCK_REGISTER_THIS_THREAD();
    LockCounter * lc = (LockCounter *) x;
    LOCK_DATATYPE_NAME * lock = lc->lock;
    for(int i = 0; i < numberOfOperationsPerThread; i++){
        if(erand48(lc->xsubi) > percentageRead){
            lc->pendingWrite = true;
            LOCK_WRITE(lock, lc);
            lc->logicalWritesInFuture = lc->logicalWritesInFuture + 1;
        }else{
            LOCK_READ_LOCK(lock);
            assert(!ACCESS_ONCE(lc->pendingWrite));
            assert(lc->logicalWritesInFuture==ACCESS_ONCE(lc->writesInFuture));
            int currentCount = ACCESS_ONCE(count);
            //usleep(100);
            assert(currentCount == ACCESS_ONCE(count));
            LOCK_READ_UNLOCK(lock);
        }
    }
    LOCK_READ_LOCK(lock);
    LOCK_READ_UNLOCK(lock);
    assert(ACCESS_ONCE(lc->writesInFuture) == lc->logicalWritesInFuture);
    
    pthread_exit(0); 
}

int test_parallel_mixed_read_write(double percentageReadParam){
    percentageRead = percentageReadParam;
    count = 0;
    pthread_t threads[NUMBER_OF_THREADS];
    LOCK_DATATYPE_NAME * lock = LOCK_CREATE(&mixed_read_write_test_writer);
    for(int i = 0; i < NUMBER_OF_THREADS; i ++){
        LockCounter *lc = &lock_counters[i];
        lc->lock = lock;
        lc->xsubi[0] = i;
        lc->xsubi[2] = i;
        lc->logicalWritesInFuture = 0;
        lc->writesInFuture = 0;
        lc->pendingWrite = false;

        pthread_create(&threads[i],NULL,&mixed_read_write_thread,lc);
    }
    int totalNumOfWrites = 0;
    for(int i = 0; i < NUMBER_OF_THREADS; i ++){
        pthread_join(threads[i],NULL);
        totalNumOfWrites = totalNumOfWrites + ACCESS_ONCE(lock_counters[i].writesInFuture);
    }

    assert(totalNumOfWrites == count);

    LOCK_FREE(lock);
    return 1;
}



int main(int argc, char **argv){
    
    printf("\n\n\n\033[32m ### STARTING SIMPLE DELAYED WRITERS LOCK TESTS! -- \033[m\n\n\n");

    T(test_create(), "test_create()");

    T(test_write_lock(), "test_write_lock()");

    T(test_write_lock_and_read_lock(), "test_write_lock_and_read_lock()");

    T(test_read_write_lock_is_blocking_other_read_write_lock(), "test_read_write_lock_is_blocking_other_read_write_lock()");

    T(test_read_write_lock_is_blocking_other_read_lock(), "test_read_write_lock_is_blocking_other_read_lock()");

    T(test_parallel_mixed_read_write(1.0),"test_parallel_mixed_read_write(1.0)");

    T(test_parallel_mixed_read_write(0.95),"test_parallel_mixed_read_write(0.95)");

    T(test_parallel_mixed_read_write(0.9),"test_parallel_mixed_read_write(0.9)");

    T(test_parallel_mixed_read_write(0.8),"test_parallel_mixed_read_write(0.8)");

    T(test_parallel_mixed_read_write(0.5),"test_parallel_mixed_read_write(0.5)");

    T(test_parallel_mixed_read_write(0.3),"test_parallel_mixed_read_write(0.3)");

    T(test_parallel_mixed_read_write(0.1),"test_parallel_mixed_read_write(0.1)");

    T(test_parallel_mixed_read_write(0.0),"test_parallel_mixed_read_write(0.0)");

    printf("\n\n\n\033[32m ### SIMPLE DELAYED WRITERS LOCK COMPLETED! -- \033[m\n\n\n");

    exit(0);

}

