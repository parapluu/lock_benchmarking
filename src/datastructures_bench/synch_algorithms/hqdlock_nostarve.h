#ifndef HQDLOCK_NOSTARVE_H
#define HQDLOCK_NOSTARVE_H

#include <stdbool.h>
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

//MCS  Lock
struct MCSNodeImpl;

typedef union CacheLinePaddedMCSNodePtrImpl {
    struct MCSNodeImpl * value;
    char padding[64];
} CacheLinePaddedMCSNodePtr;

typedef struct MCSNodeImpl {
    char pad1[64];
    CacheLinePaddedMCSNodePtr next;
    CacheLinePaddedBool locked;
} MCSNode;

typedef struct MCSLockImpl {
    char pad1[64];
    void (*writer)(int, int *);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    CacheLinePaddedMCSNodePtr endOfQueue;
} MCSLock;
static inline
bool mcslock_is_locked(MCSLock *lock){
    MCSNode * endOfQueue;
    load_acq(endOfQueue, lock->endOfQueue.value);
    return endOfQueue != NULL;
}

extern __thread MCSNode myMCSNode __attribute__((aligned(64)));

static inline
bool set_if_null_ptr(MCSNode ** pointerToOldValue, MCSNode * newValue){
    return __sync_bool_compare_and_swap(pointerToOldValue, NULL, newValue);
}

static inline
bool mcslock_try_write_read_lock(MCSLock *lock) {
    MCSNode * node = &myMCSNode;
    node->next.value = NULL;
    return set_if_null_ptr(&lock->endOfQueue.value, node);
}

__thread MCSNode myMCSNode __attribute__((aligned(64)));

static inline
MCSNode * get_and_set_node_ptr(MCSNode ** pointerToOldValue, MCSNode * newValue){
    MCSNode * x = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
        x = ACCESS_ONCE(*pointerToOldValue);
    }
}
 
void mcslock_initialize(MCSLock * lock, void (*writer)(int, int *)){
    lock->writer = writer;
    lock->endOfQueue.value = NULL;
    __sync_synchronize();
}
MCSLock * mcslock_create(void (*writer)(int, int*)){
    MCSLock * lock = malloc(sizeof(MCSLock));
    mcslock_initialize(lock, writer);
    return lock;
}


void mcslock_free(MCSLock * lock){
    free(lock);
}

void mcslock_register_this_thread(){
    MCSNode * node = &myMCSNode;
    node->locked.value = false;
    node->next.value = NULL;
}

//Returns true if it is taken over from another writer and false otherwise
static inline bool mcslock_write_read_lock(MCSLock *lock) {
    bool isNodeLocked;
    MCSNode * node = &myMCSNode;
    node->next.value = NULL;
    MCSNode * predecessor = get_and_set_node_ptr(&lock->endOfQueue.value, node);
    if (predecessor != NULL) {
        store_rel(node->locked.value, true);
        store_rel(predecessor->next.value, node);
        load_acq(isNodeLocked, node->locked.value);
        //Wait
        while (isNodeLocked) {
            __sync_synchronize();
            load_acq(isNodeLocked, node->locked.value);
        }
        return true;
    }else{
        return false;
    }
}

static inline
void mcslock_write_read_unlock(MCSLock * lock) {
    MCSNode * nextNode;
    MCSNode * node = &myMCSNode;
    load_acq(nextNode, node->next.value);
    if (nextNode == NULL) {
        if (__sync_bool_compare_and_swap(&lock->endOfQueue.value, node, NULL)){
            return;
        }
        //wait
        load_acq(nextNode, node->next.value);
        while (nextNode == NULL) {
             __sync_synchronize();
            load_acq(nextNode, node->next.value);
        }
    }
    store_rel(node->next.value->locked.value, false);
    __sync_synchronize();//Push change
}

void mcslock_read_lock(MCSLock *lock) {
    mcslock_write_read_lock(lock);
}

static inline
void mcslock_read_unlock(MCSLock *lock) {
    mcslock_write_read_unlock(lock);
}
void mcslock_write(MCSLock *lock, int writeInfo) {
    mcslock_write_read_lock(lock);
    lock->writer(writeInfo, NULL);
    mcslock_write_read_unlock(lock);
    
}

//Multi write queue

#define MWQ_CAPACITY MAX_NUM_OF_HELPED_OPS

typedef struct DelegateRequestEntryImpl {
    void (*request)(int, int*);
    int data;
    int * responseLocation;
#ifdef PAD_QUEUE_ELEMENTS_TO_TWO_CACHE_LINES
    char pad[128 - ((2*sizeof(void *)) + sizeof(int))];
#endif
} DelegateRequestEntry;

typedef struct DRMWQImpl {
    char padd1[64];
    CacheLinePaddedBool closed;
    char padd2[64];
    CacheLinePaddedULong elementCount;
    DelegateRequestEntry elements[MWQ_CAPACITY];
    char padd3[64 - ((sizeof(DelegateRequestEntry)*MWQ_CAPACITY) % 64)];
} DRMWQueue;

DRMWQueue * drmvqueue_create();
DRMWQueue * drmvqueue_initialize(DRMWQueue * queue);
static void drmvqueue_free(DRMWQueue * queue);
static bool drmvqueue_offer(DRMWQueue * queue, DelegateRequestEntry e);
static void drmvqueue_flush(DRMWQueue * queue);
static void drmvqueue_reset_fully_read(DRMWQueue *  queue);


static inline
int CAS_fetch_and_add(unsigned long * valueAddress, int incrementWith){
    unsigned long oldValCAS;
    unsigned long oldVal = ACCESS_ONCE(*valueAddress);
    while(true){
        oldValCAS = __sync_val_compare_and_swap(valueAddress, oldVal, oldVal + incrementWith);
        if(oldVal == oldValCAS){
            return oldVal;
        }else{
            oldVal = oldValCAS;
        }
    }
}

#ifdef CAS_FETCH_AND_ADD
#define FETCH_AND_ADD(valueAddress, incrementWith) CAS_fetch_and_add(valueAddress, incrementWith) 
#else
#define FETCH_AND_ADD(valueAddress, incrementWith) __sync_fetch_and_add(valueAddress, incrementWith) 
#endif 

static inline
unsigned long min(unsigned long i1, unsigned long i2){
    return i1 < i2 ? i1 : i2;
}

DRMWQueue * drmvqueue_create(){
    DRMWQueue * queue = (DRMWQueue *)malloc(sizeof(DRMWQueue));
    return drmvqueue_initialize(queue);
}

DRMWQueue * drmvqueue_initialize(DRMWQueue * queue){
    for(int i = 0; i < MWQ_CAPACITY; i++){
        queue->elements[i].request = NULL;
        queue->elements[i].data = 0;
        queue->elements[i].responseLocation = NULL;
    }
    queue->elementCount.value = MWQ_CAPACITY;
    queue->closed.value = true;
    __sync_synchronize();
    return queue;
}

void drmvqueue_free(DRMWQueue * queue){
    free(queue);
}

#define NEWOFFER
#ifdef NEWOFFER

static inline
bool drmvqueue_offer(DRMWQueue * queue, DelegateRequestEntry e){
    bool closed;
    load_acq(closed, queue->closed.value);
    if(!closed){
        int index = FETCH_AND_ADD(&queue->elementCount.value, 1);
        if(index < MWQ_CAPACITY){
            store_rel(queue->elements[index].responseLocation, e.responseLocation);
            store_rel(queue->elements[index].data, e.data);
            store_rel(queue->elements[index].request, e.request);
            __sync_synchronize();//Flush
            return true;
        }else{
            return false;
        }
    }else{
        return false;
    }
}

#else

static inline
bool drmvqueue_offer(DRMWQueue * queue, DelegateRequestEntry e){
    bool closed;
    load_acq(closed, queue->closed.value);
    if(!closed){
        int index = __sync_fetch_and_add(&queue->elementCount.value, 1);
        if(index < MWQ_CAPACITY){
            store_rel(queue->elements[index].responseLocation, e.responseLocation);
            store_rel(queue->elements[index].data, e.data);
            store_rel(queue->elements[index].request, e.request);
            __sync_synchronize();//Flush
            return true;
        }else{
            store_rel(queue->closed.value, true);
            __sync_synchronize();//Flush
            return false;
        }
    }else{
        return false;
    }
}

#endif


static inline
void drmvqueue_flush(DRMWQueue * queue){
    unsigned long numOfElementsToRead;
    unsigned long newNumOfElementsToRead;
    unsigned long currentElementIndex = 0;
    bool closed = false;
    load_acq(numOfElementsToRead, queue->elementCount.value);
    if(numOfElementsToRead >= MWQ_CAPACITY){
#ifdef NEWOFFER
        store_rel(queue->closed.value, true);
#endif
        closed = true;
        numOfElementsToRead = MWQ_CAPACITY;
    }

    while(true){
        if(currentElementIndex < numOfElementsToRead){
            //There is definitly an element that we should read
            DelegateRequestEntry e;
            load_acq(e.request, queue->elements[currentElementIndex].request);
            load_acq(e.data, queue->elements[currentElementIndex].data);
            load_acq(e.responseLocation, queue->elements[currentElementIndex].responseLocation);
            while(e.request == NULL) {
                __sync_synchronize();
                load_acq(e.request, queue->elements[currentElementIndex].request);
                load_acq(e.data, queue->elements[currentElementIndex].data);
                load_acq(e.responseLocation, queue->elements[currentElementIndex].responseLocation);
            }
            e.request(e.data, e.responseLocation);
            store_rel(queue->elements[currentElementIndex].request, NULL);
            currentElementIndex = currentElementIndex + 1;
        }else if (closed){
#ifdef QUEUE_STATS
            helpSeasonsPerformed.value++;
            numberOfDeques.value = numberOfDeques.value + currentElementIndex;
#endif
            //The queue is closed and there is no more elements that need to be read:
            return;
        }else{
            //Seems like there are no elements that should be read and the queue is
            //not closed. Check again if there are still no more elements that should
            //be read before closing the queue
#ifdef WAITS_BEFORE_CLOSE_QUEUE_ATTEMPT
            for(int i = 0; i < WAITS_BEFORE_CLOSE_QUEUE_ATTEMPT; i++){
                __sync_synchronize();                
            }
#endif
            load_acq(newNumOfElementsToRead, queue->elementCount.value);
            if(newNumOfElementsToRead == numOfElementsToRead){
                //numOfElementsToRead has not changed. Close the queue.
                numOfElementsToRead = 
                    min(get_and_set_ulong(&queue->elementCount.value, MWQ_CAPACITY + 1), 
                        MWQ_CAPACITY);
#ifdef NEWOFFER
                store_rel(queue->closed.value, true);
#endif
		closed = true;
            }else if(newNumOfElementsToRead < MWQ_CAPACITY){
                numOfElementsToRead = newNumOfElementsToRead;
            }else{
#ifdef NEWOFFER
                store_rel(queue->closed.value, true);
#endif
                closed = true;
                numOfElementsToRead = MWQ_CAPACITY;
            }
        }
    }
}

static inline
void drmvqueue_reset_fully_read(DRMWQueue * queue){
    store_rel(queue->elementCount.value, 0);
    store_rel(queue->closed.value, false);
}



//QD Lock


typedef struct AgnosticDXLockImpl {
    DRMWQueue writeQueue;
    char pad1[128];
    void (*defaultWriter)(int, int *);
    char pad2[64 - sizeof(void * (*)(void*)) % 64];
    char pad3[128];
    MCSLock lock;
    char pad4[64];
} AgnosticDXLock;

void adxlock_initialize(AgnosticDXLock * lock, void (*defaultWriter)(int, int *));
AgnosticDXLock * adxlock_create(void (*writer)(int, int *)){
    AgnosticDXLock * lock = (AgnosticDXLock *)malloc(sizeof(AgnosticDXLock));
    adxlock_initialize(lock, writer);
    return lock;
}

void adxlock_initialize(AgnosticDXLock * lock, void (*defaultWriter)(int, int *)){
    //TODO check if the following typecast is fine
    lock->defaultWriter = defaultWriter;
    mcslock_initialize(&lock->lock, defaultWriter);
    drmvqueue_initialize(&lock->writeQueue);
    __sync_synchronize();
}

void adxlock_free(AgnosticDXLock * lock){
    free(lock);
}


//*******
//hqdlock
//*******
#include "synch_algs_config.h"
#include "synch_algs_primitives.h"

#include "clh.h"

typedef struct HQDLockImpl {
    char pad1[64];
    AgnosticDXLock qdlocks[NUMBER_OF_NUMA_NODES];
    char pad2[64];
    CLHLockStruct lock;
    char pad3[128];
} HQDLock;

void hqdlock_initialize(HQDLock * lock, void (*defaultWriter)(int, int *));
HQDLock * hqdlock_create(void (*writer)(int, int *)){
    HQDLock * lock = (HQDLock *)malloc(sizeof(HQDLock));
    hqdlock_initialize(lock, writer);
    return lock;
}

void hqdlock_initialize(HQDLock * lock, void (*defaultWriter)(int, int *)){
    for(int n = 0; n < NUMBER_OF_NUMA_NODES; n++){
        adxlock_initialize(&lock->qdlocks[n], NULL);
    }
    clhLockInitExisting(&lock->lock);
}

void hqdlock_free(HQDLock * lock){
    free(lock);
}

void hqdlock_register_this_thread(){
    clhThreadLocalInit();
}

static inline
void hqdlock_write_with_response(HQDLock *hqdlock, 
                                 void (*delgateFun)(int, int *), 
                                 int data, 
                                 int * responseLocation){
#ifdef PINNING
    int my_numa_node = numa_node.value;
#else
    int my_numa_node = sched_getcpu() % NUMBER_OF_NUMA_NODES;
#endif
    AgnosticDXLock * lock = &hqdlock->qdlocks[my_numa_node];

    int counter = 0;
    DelegateRequestEntry e;
    e.request = delgateFun;
    e.data = data;
    e.responseLocation = responseLocation;
    do{
        if(!mcslock_is_locked(&lock->lock)){
            if(mcslock_try_write_read_lock(&lock->lock)){
                clhLock(&hqdlock->lock, 0 /*NOT USED*/);
#ifdef ACTIVATE_NO_CONTENTION_OPT
	        if(counter > 0){
#endif
                    drmvqueue_reset_fully_read(&lock->writeQueue);
                    delgateFun(data, responseLocation);
                    drmvqueue_flush(&lock->writeQueue);
                    clhUnlock(&hqdlock->lock, 0 /*NOT USED*/);
                    mcslock_write_read_unlock(&lock->lock);
                    return;
#ifdef ACTIVATE_NO_CONTENTION_OPT
                }else{
                    delgateFun(data, responseLocation);
#endif
                    clhUnlock(&hqdlock->lock, 0 /*NOT USED*/);
		    mcslock_write_read_unlock(&lock->lock);
		    return;
#ifdef ACTIVATE_NO_CONTENTION_OPT
                }
#endif
            }
        }else{
            while(mcslock_is_locked(&lock->lock)){
                if(drmvqueue_offer(&lock->writeQueue, e)){
                    return;
                }else{
                    __sync_synchronize();
                    __sync_synchronize();
                }
            }
        }
        if((counter & 7) == 0){
#ifdef USE_YIELD
            sched_yield();
#endif
        }
        counter = counter + 1;
    }while(counter < 32);
    /* limit reached: force locking */
    mcslock_write_read_lock(lock);
    drmvqueue_reset_fully_read(&lock->writeQueue);
    delgateFun(data, responseLocation);
    drmvqueue_flush(&lock->writeQueue);
    mcslock_write_read_unlock(&lock->lock);
}

static inline
int hqdlock_write_with_response_block(HQDLock *lock, 
                                      void (*delgateFun)(int, int *), 
                                      int data){
    int counter = 0;
    int returnValue = INT_MIN;
    int currentValue;
    hqdlock_write_with_response(lock, delgateFun, data, &returnValue);
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
void hqdlock_delegate(HQDLock *lock, 
                      void (*delgateFun)(int, int *), 
                      int data) {
    hqdlock_write_with_response(lock, delgateFun, data, NULL);
}

void hqdlock_write(HQDLock *lock, int writeInfo) {
    hqdlock_delegate(lock, lock->qdlocks[0].defaultWriter, writeInfo);
}

void hqdlock_write_read_lock(HQDLock *lock) {
    clhLock(&lock->lock, 0 /*NOT USED*/);
}

void hqdlock_write_read_unlock(HQDLock * lock) {
    clhUnlock(&lock->lock, 0 /*NOT USED*/);
}

void hqdlock_read_lock(HQDLock *lock) {
    hqdlock_write_read_lock(lock);
}

void hqdlock_read_unlock(HQDLock *lock) {
    hqdlock_write_read_unlock(lock);
}

#endif
