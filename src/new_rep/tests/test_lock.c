#include "tests/test_framework.h"
#include "misc/random.h"

#include "misc/thread_includes.h"
#include "misc/bsd_stdatomic.h"//Until c11 stdatomic.h is available

#include "locks/locks.h"

typedef union {
    unsigned int value;
    char pad[CACHE_LINE_SIZE];
} LLPaddedSeed;

typedef union {
    unsigned long value;
    char pad[CACHE_LINE_SIZE];
} LLPaddedLocalCounter;

typedef struct {
    unsigned long * localInCSCounter;
    unsigned int * localSeed;
} ThreadLocalData;

int test_create(){
    LOCK_TYPE * lock = LL_create(LOCK_TYPE_NAME);
    LL_free(lock);
    return 1;
}

int test_lock(){
    LOCK_TYPE * lock = LL_create(LOCK_TYPE_NAME);
    for(int i = 0; i < 10; i++){
        LL_lock((lock));
        LL_unlock(lock);
    }
    LL_free(lock);
    return 1;
}

int test_is_locked(){
    LOCK_TYPE * lock = LL_create(LOCK_TYPE_NAME);
    for(int i = 0; i < 10; i++){
        assert(LL_is_locked(lock) == false);
        LL_lock(lock);
        assert(LL_is_locked(lock) == true);
        LL_unlock(lock);
        assert(LL_is_locked(lock) == false);
    }
    LL_free(lock);
    return 1;
}


LOCK_TYPE * lock;
LLPaddedULong counter = {.value = ATOMIC_VAR_INIT(0)};
LLPaddedBool stop = {.value = ATOMIC_FLAG_INIT};
LLPaddedDouble delegatePercentage;

void critical_section_code(unsigned long * localCounterPtr){
    unsigned long oldCounterValue = atomic_load(&counter.value);
    atomic_fetch_add(&counter.value, 1);
    *localCounterPtr = *localCounterPtr + 1;
    atomic_thread_fence(memory_order_seq_cst); 
    thread_yield();
    assert((oldCounterValue + 1) == atomic_load(&counter.value));
}

void delegate_function(unsigned int messageSize, void * messageAddress){
    assert(messageSize == sizeof(unsigned long *));
    unsigned long ** localCounterPtrPtr = (unsigned long **)messageAddress;
    critical_section_code(*localCounterPtrPtr);
}

void  * critical_section_thread(void * threadLocalDataVPtr){
    ThreadLocalData * threadLocalDataPtr = (ThreadLocalData*)threadLocalDataVPtr;
    unsigned long * localInCSCounter = threadLocalDataPtr->localInCSCounter;
    unsigned int * localSeed = threadLocalDataPtr->localSeed;
    while(!atomic_load_explicit(&stop.value, memory_order_acquire)){
        if(random_double(localSeed) > delegatePercentage.value){
            LL_lock(lock);
            critical_section_code(localInCSCounter);
            LL_unlock(lock);
        } else {
            LL_delegate(lock, delegate_function, sizeof(unsigned long *), &localInCSCounter);
        } 
    }
    return NULL;
}
int test_mutual_exclusion(double delegatePercentageParm){
    delegatePercentage.value = delegatePercentageParm;
    lock = LL_create(LOCK_TYPE_NAME);
    struct timespec testTime= {.tv_sec = 0, .tv_nsec = 100000000}; 
    for(int i = 1; i < 10; i++){
        atomic_store(&counter.value, 0);
        atomic_store(&stop.value, false);
        pthread_t threads[i];
        LLPaddedLocalCounter localInCSCounters[i];
        LLPaddedSeed localSeeds[i];
        ThreadLocalData * threadLocalData = aligned_alloc(CACHE_LINE_SIZE, sizeof(ThreadLocalData) * i);
        for(int n = 0; n < i; n++){
            localInCSCounters[n].value = 0;
            localSeeds[n].value = n;
            threadLocalData[n].localInCSCounter = &localInCSCounters[n].value;
            threadLocalData[n].localSeed = &localSeeds[n].value;
            pthread_create(&threads[n], NULL,
                           critical_section_thread,
                           &threadLocalData[n]);
        }
        nanosleep(&testTime, NULL);
        atomic_store(&stop.value, true);
        unsigned long localInCSCountersSum = 0;
        for(int n = 0; n < i; n++){
            void * resp = NULL;
            pthread_join(threads[n], &resp);
        }
        for(int n = 0; n < i; n++){
            localInCSCountersSum = localInCSCountersSum + 
                localInCSCounters[n].value;
        }
        free(threadLocalData);
        assert(localInCSCountersSum == atomic_load(&counter.value));
    }
    LL_free(lock);
    return 1;
}

int main(/*int argc, char **argv*/){
    
    printf("\n\n\n\033[32m ### STARTING LOCK TESTS! -- \033[m\n\n\n");

    T(test_create(), "test_create()");

    T(test_lock(), "test_lock()");

    T(test_mutual_exclusion(0.0), "test_mutual_exclusion LL_delegate = 0%");

    T(test_mutual_exclusion(0.5), "test_mutual_exclusion LL_delegate = 50%");

    T(test_mutual_exclusion(1.0), "test_mutual_exclusion LL_delegate = 100%");

    printf("\n\n\n\033[32m ### LOCK TESTS COMPLETED! -- \033[m\n\n\n");

    return 0;

}

