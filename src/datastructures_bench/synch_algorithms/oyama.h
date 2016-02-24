#ifndef QDLOCK_H
#define QDLOCK_H

#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include <limits.h>


#ifndef SMP_UTILS_H
#define SMP_UTILS_H

//SMP Utils

//Make sure compiler does not optimize away memory access
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

//Atomic get
#define GET(value_ptr)  __sync_fetch_and_add(value_ptr, 0)

//Compiller barrier
#define barrier() __asm__ __volatile__("": : :"memory")

//See the following URL for explanation of acquire and release semantics:
//http://preshing.com/20120913/acquire-and-release-semantics

//Load with acquire barrier
#if __x86_64__
#define load_acq(assign_to,load_from) \
    assign_to = ACCESS_ONCE(load_from)
#else
#define load_acq(assign_to,load_from)           \
    do {                                        \
        barrier();                              \
        assign_to = ACCESS_ONCE(load_from);     \
        __sync_synchronize();                   \
    } while(0)
#endif


//Store with release barrier
#if __x86_64__
#define store_rel(store_to,store_value) \
    do{                                 \
        barrier();                      \
        store_to = store_value;        \
        barrier();                      \
    }while(0);
#else
#define store_rel(store_to,store_value) \
    do{                                 \
        __sync_synchronize();           \
        store_to = store_value;        \
        barrier();                      \
    }while(0);
#endif

//Intel pause instruction
#if __x86_64__
#define pause_instruction() \
  __asm volatile ("pause")
#else
#define pause_instruction() \
  __sync_synchronize()
#endif

static inline
int get_and_set_int(int * pointerToOldValue, int newValue){
    int x = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
        x = ACCESS_ONCE(*pointerToOldValue);
    }
}

static inline
unsigned long get_and_set_ulong(unsigned long * pointerToOldValue, unsigned long newValue){
    unsigned long x = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
        x = ACCESS_ONCE(*pointerToOldValue);
    }
}

typedef union CacheLinePaddedBoolImpl {
    bool value;
    char padding[64];
} CacheLinePaddedBool;

typedef union CacheLinePaddedIntImpl {
    int value;
    char padding[128];
} CacheLinePaddedInt;


typedef union CacheLinePaddedULongImpl {
    unsigned long value;
    char padding[128];
} CacheLinePaddedULong;

typedef union CacheLinePaddedDoubleImpl {
    double value;
    char padding[128];
} CacheLinePaddedDouble;

typedef union CacheLinePaddedPointerImpl {
    void * value;
    char padding[64];
} CacheLinePaddedPointer;

#endif

//Oyama Lock

typedef struct OyamaContextImpl {
#ifdef PRE_ALLOC_OPT
    bool free;
#endif
    struct OyamaContextImpl * next;
    void (*delgateFun)(int, int *);
    int data;
    int * responseLocation;
#ifdef PRE_ALLOC_OPT
    char pad[64 - (3*sizeof(void*) + sizeof(int) + sizeof(bool))];
#else
    char pad[64 - (3*sizeof(void*) + sizeof(int))];
#endif
} OyamaContext;

#define LOCK_IS_FREE_FLAG ((void*)((uintptr_t)1))
#define LOCK_IS_LOCKED_FLAG ((void *)((uintptr_t)2))

typedef struct OyamaLockImpl {
    CacheLinePaddedPointer lockWord;
} OyamaLock;

void oyamalock_initialize(OyamaLock * lock);
OyamaLock * oyamalock_create(){
    OyamaLock * lock = (OyamaLock *)malloc(sizeof(OyamaLock));
    oyamalock_initialize(lock);
    return lock;
}

void oyamalock_initialize(OyamaLock * lock){
    lock->lockWord.value = LOCK_IS_FREE_FLAG;
    __sync_synchronize();
}

void oyamalock_free(OyamaLock * lock){
    free(lock);
}

#ifdef PRE_ALLOC_OPT

__thread CacheLinePaddedInt currentOyamaContext  __attribute__((aligned(128)));

__thread OyamaContext oyamaContexts[MAX_NUM_OF_HELPED_OPS] __attribute__((aligned(128)));

void oyamalock_register_this_thread(){
    currentOyamaContext.value = 0;
    for(int i = 0; i < MAX_NUM_OF_HELPED_OPS; i++){
        oyamaContexts[i].free = true;
    }
}

#else
void oyamalock_register_this_thread(){}
#endif

#ifdef PRE_ALLOC_OPT

static inline OyamaContext * oyama_make_context(void (*delgateFun)(int, int *), int data, int * responseLocation){
    OyamaContext * context = &oyamaContexts[currentOyamaContext.value];
    bool isFree;
    load_acq(isFree, context->free);
    while(!isFree){
        sched_yield();
        load_acq(isFree, context->free);
    }
    context->delgateFun = delgateFun;
    context->data = data;
    context->responseLocation = responseLocation;
    store_rel(context->free, false);

    currentOyamaContext.value = currentOyamaContext.value + 1;
    if(currentOyamaContext.value == MAX_NUM_OF_HELPED_OPS){
        currentOyamaContext.value = 0;
    }
    return context;
}

void oyama_free_context(OyamaContext * context){
    store_rel(context->free, true);
}

#else

static inline OyamaContext * oyama_make_context(void (*delgateFun)(int, int *), int data, int * responseLocation){
OyamaContext * context = memalign(64, (sizeof(OyamaContext)));
    context->delgateFun = delgateFun;
    context->data = data;
    context->responseLocation = responseLocation;
    return context;
}

void oyama_free_context(OyamaContext * context){
    free(context);
}

#endif


static inline void oyama_release_lock(OyamaLock *lock){
    while(true){
        if(__sync_bool_compare_and_swap(&lock->lockWord.value, LOCK_IS_LOCKED_FLAG, LOCK_IS_FREE_FLAG)){
            return;
        }
        barrier();
        OyamaContext * headContext = __sync_lock_test_and_set(&lock->lockWord.value, LOCK_IS_LOCKED_FLAG);
        barrier();
        //Reverse list
        OyamaContext * lastNode = headContext;
        OyamaContext * currentContext = headContext->next;
        while(currentContext != NULL){
            OyamaContext * nextCurrentContext = currentContext->next;
            currentContext->next = headContext;
            headContext = currentContext;
            currentContext = nextCurrentContext;
        }
        lastNode->next = NULL;
        //Execute contexts
        currentContext = headContext;
        while(currentContext != NULL){
            currentContext->delgateFun(currentContext->data, currentContext->responseLocation);
#ifdef QUEUE_STATS
            numberOfDeques.value = numberOfDeques.value + 1;
#endif
            OyamaContext * oldCurrentContext = currentContext;
            currentContext = currentContext->next;
            oyama_free_context(oldCurrentContext);
        }
    }
}

static inline bool oyama_insert(OyamaLock *lock, OyamaContext * context){
    OyamaContext * lockWordValue;
    while(true){
        load_acq(lockWordValue, lock->lockWord.value);
        if(lockWordValue == LOCK_IS_FREE_FLAG){
            if(__sync_bool_compare_and_swap(&lock->lockWord.value, LOCK_IS_FREE_FLAG, LOCK_IS_LOCKED_FLAG)){
                return false;
            }
        }else if(lockWordValue == LOCK_IS_LOCKED_FLAG){
            context->next = NULL;
            if(__sync_bool_compare_and_swap(&lock->lockWord.value, LOCK_IS_LOCKED_FLAG, context)){
                return true;
            }
        }else{
            context->next = lockWordValue;
            if(__sync_bool_compare_and_swap(&lock->lockWord.value, lockWordValue, context)){
                return true;
            }
        }
    }
}

static inline
void oyamalock_write_with_response(OyamaLock *lock, 
                                   void (*delgateFun)(int, int *), 
                                   int data, 
                                   int * responseLocation){
    if(__sync_bool_compare_and_swap(&lock->lockWord.value, LOCK_IS_FREE_FLAG, LOCK_IS_LOCKED_FLAG)){
        delgateFun(data, responseLocation);
#ifdef QUEUE_STATS
        helpSeasonsPerformed.value = helpSeasonsPerformed.value + 1;
#endif
        oyama_release_lock(lock);
    } else {
        OyamaContext * context = oyama_make_context(delgateFun, data, responseLocation);
        if(!oyama_insert(lock, context)){
#ifdef QUEUE_STATS
            helpSeasonsPerformed.value = helpSeasonsPerformed.value + 1;
#endif
            delgateFun(data, responseLocation);
            oyama_free_context(context);
            oyama_release_lock(lock);
        }
    }
}

static inline
int oyamalock_write_with_response_block(OyamaLock *lock, 
                                      void (*delgateFun)(int, int *), 
                                      int data){
    int counter = 0;
    int returnValue = INT_MIN;
    int currentValue;
    oyamalock_write_with_response(lock, delgateFun, data, &returnValue);
    load_acq(currentValue, returnValue);
    while(currentValue == INT_MIN){
        if((counter & 7) == 0){
#ifdef USE_YIELD
            sched_yield();
#endif
        }else{
            __sync_synchronize();
        }
        counter = counter + 1;
        load_acq(currentValue, returnValue);
    }
    return currentValue;
}

static inline
void oyamalock_delegate(OyamaLock *lock, 
                      void (*delgateFun)(int, int *), 
                      int data) {
    oyamalock_write_with_response(lock, delgateFun, data, NULL);
}


/* void oyamalock_write_read_lock(OyamaLock *lock) { */
/*     tataslock_write_read_lock(&lock->lock);     */
/*     drmvqueue_reset_fully_read(&lock->writeQueue); */
/*     __sync_synchronize();//Flush */
/* } */

/* void oyamalock_write_read_unlock(OyamaLock * lock) { */
/*     drmvqueue_flush(&lock->writeQueue); */
/*     tataslock_write_read_unlock(&lock->lock); */
/* } */

/* void oyamalock_read_lock(OyamaLock *lock) { */
/*     oyamalock_write_read_lock(lock); */
/* } */

/* void oyamalock_read_unlock(OyamaLock *lock) { */
/*     oyamalock_write_read_unlock(lock); */
/* } */

#endif
