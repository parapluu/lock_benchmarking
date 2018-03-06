#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>
#include <limits.h>
#include <sched.h>
#include "smp_utils.h"
#include "skiplist.h"
#include "smp_utils.h"
#include <sched.h>


//#define RANDOM_LOCAL_WORK

//#define MIN_LOCAL_WORK_WHEN_RANDOM 32
//#define DEBUG_PRINT_IN_CS
//#define DEBUG_PRINT_OUTSIDE_CS
//#define SANITY_CHECK
//#define QUEUE_STATS
#define MAX_NUM_OF_HELPED_OPS 4096
#ifdef QUEUE_STATS
__thread CacheLinePaddedULong helpSeasonsPerformed __attribute__((aligned(128))) = {.value = 0};
__thread CacheLinePaddedULong numberOfDeques __attribute__((aligned(128))) = {.value = 0};
#endif

#define DEBUG_COUNT_OPS_PER_FLUSH


#ifdef SANITY_CHECK
CacheLinePaddedInt dequeues_executed = {.value = 0};
CacheLinePaddedInt enqueues_executed = {.value = 0};
CacheLinePaddedInt dequeues_issued = {.value = 0};
CacheLinePaddedInt enqueues_issued = {.value = 0};
#endif


#define PINNING
#ifdef PINNING
__thread CacheLinePaddedInt numa_node __attribute__((aligned(128)));
//#define CORE_FIRST_POLICY_SANDY
//#define SPREAD_PINNING_POLICY
#define NODE_FIRST_POLICY
#endif

//=======================
//Benchmark mutable state (Important that they are all on separate cache lines)
//=======================

//Should be power of 2
#define NUMBER_OF_ELEMENTS_IN_ARRAYS 64
#define ELEMENT_POS_MASK (NUMBER_OF_ELEMENTS_IN_ARRAYS-1)
#define ADD_RAND_NUM_MASK (1048576-1)
#define FALSE_SHARING_SECURITY 128


typedef struct SeedWrapperImpl{
    char pad1[FALSE_SHARING_SECURITY];
    unsigned short value[3];
    char pad2[FALSE_SHARING_SECURITY - (3*sizeof(short))];
} SeedWrapper;

typedef struct BoolWrapperImpl{
    char pad1[FALSE_SHARING_SECURITY];
    bool value;
    char pad2[FALSE_SHARING_SECURITY - sizeof(bool)];
} BoolWrapper;


typedef struct LockThreadLocalSeedImpl{
    char pad1[FALSE_SHARING_SECURITY];
    int thread_id;
    unsigned short * seed;
    //Write back
    long totalNumerOfIterationsExecuted;
#ifdef QUEUE_STATS
    unsigned long helpSeasonsPerformed;
    unsigned long numberOfDeques;
#endif
    char pad2[FALSE_SHARING_SECURITY];
} LockThreadLocalSeed;


//=======================
//Benchmark mutable state (Important that they are all on separate cache lines)
//=======================

SeedWrapper write_d_rand_seed_wrapper __attribute__((aligned(64)));
unsigned short * write_d_rand_seed = write_d_rand_seed_wrapper.value;

BoolWrapper benchmarkStopedWrapper __attribute__((aligned(64)));
bool * benchmarkStoped = &benchmarkStopedWrapper.value;

BoolWrapper benchmarkStartedWrapper __attribute__((aligned(64)));
bool * benchmarkStarted = &benchmarkStartedWrapper.value;

SeedWrapper threadLocalSeeds[512] __attribute__((aligned(64)));

//========================
//Benchmark imutable state
//========================

typedef struct ImutableStateWrapperImpl{
    char pad1[FALSE_SHARING_SECURITY];
    int iterationsSpentCriticalWork;
    int iterationsSpentNonCriticalWork;
    double percentageDequeue;
    char pad2[FALSE_SHARING_SECURITY];
} ImutableStateWrapper;

ImutableStateWrapper imsw  __attribute__((aligned(64))) = 
{.iterationsSpentCriticalWork = 1,
 .iterationsSpentNonCriticalWork = 0,
 .percentageDequeue = 0.5};

__thread unsigned short * myXsubi;


//=======================
//>>>>>>>>>>>>>>>>>>>>>>>
// Lock depended code
//>>>>>>>>>>>>>>>>>>>>>>>
//Necessary definations
//void lock_init()
//void lock_thread_init()

#ifdef USE_QDLOCK

//#define WAITS_BEFORE_CLOSE_QUEUE_ATTEMPT 1
//#define ACTIVATE_NO_CONTENTION_OPT
//#define CAS_FETCH_AND_ADD

#include "datastructures_bench/synch_algorithms/qdlock.h"

AgnosticDXLock lock __attribute__((aligned(64)));

void lock_init(){
    adxlock_initialize(&lock, NULL);
}

void lock_thread_init(){}


#elif defined (USE_NEWREP_QDLOCK)

#include "locks/locks.h"

QDLock lock __attribute__((aligned(64)));

void lock_init(){
    LL_initialize(&lock);
}

void lock_thread_init(){}

#elif defined (USE_QDLOCKP)

#include "datastructures_bench/synch_algorithms/qdlockp.h"

AgnosticDXLock lock __attribute__((aligned(64)));

void lock_init(){
    adxlock_initialize(&lock, NULL);
}

void lock_thread_init(){}

#elif defined (USE_QDLOCK_NOSTARVE)

#include "datastructures_bench/synch_algorithms/qdlock_nostarve.h"

AgnosticDXLock lock __attribute__((aligned(64)));

void lock_init(){
    adxlock_initialize(&lock, NULL);
}

void lock_thread_init(){}

#elif defined (USE_QDLOCK_FUTEX)

#include "datastructures_bench/synch_algorithms/qdlock_futex.h"

AgnosticDXLock lock __attribute__((aligned(64)));

void lock_init(){
    adxlock_initialize(&lock, NULL);
}

void lock_thread_init(){}

#elif defined (USE_CPPLOCK)

#include "datastructures_bench/synch_algorithms/cpplock.h"

AgnosticDXLock lock __attribute__((aligned(64)));

void lock_init(){
    adxlock_initialize(&lock, NULL);
}

void lock_thread_init(){}

#elif defined (USE_OYAMA)

#include "datastructures_bench/synch_algorithms/oyama.h"

OyamaLock lock __attribute__((aligned(64)));

void lock_init(){
    oyamalock_initialize(&lock);
}

void lock_thread_init(){
    oyamalock_register_this_thread();
}


#elif defined (USE_HQDLOCK)

//#define WAITS_BEFORE_CLOSE_QUEUE_ATTEMPT 1
//#define ACTIVATE_NO_CONTENTION_OPT
//#define CAS_FETCH_AND_ADD

#include "datastructures_bench/synch_algorithms/hqdlock.h"

HQDLock lock __attribute__((aligned(64)));

void lock_init(){
    hqdlock_initialize(&lock, NULL);
}

void lock_thread_init(){
    clhThreadLocalInit();
}

#elif defined (USE_HQDLOCK_NOSTARVE)

//#define WAITS_BEFORE_CLOSE_QUEUE_ATTEMPT 1
//#define ACTIVATE_NO_CONTENTION_OPT
//#define CAS_FETCH_AND_ADD

#include "datastructures_bench/synch_algorithms/hqdlock_nostarve.h"

HQDLock lock __attribute__((aligned(64)));

void lock_init(){
    hqdlock_initialize(&lock, NULL);
}

void lock_thread_init(){
    clhThreadLocalInit();
}

#elif defined (USE_HSYNCH)

void enqueue_cs(int enqueueValue);
int dequeue_cs();
#include "datastructures_bench/synch_algorithms/hsynch.h"

HSynchStruct lock __attribute__((aligned(64)));
__thread HSynchThreadState thread_state __attribute__((aligned(64)));

void lock_init(){
    HSynchStructInit(&lock);
}

void lock_thread_init(){
    HSynchThreadStateInit(&thread_state);
}

#elif defined (USE_CCSYNCH)

void enqueue_cs(int enqueueValue);
int dequeue_cs();
#include "datastructures_bench/synch_algorithms/ccsynch.h"

LockStruct lock __attribute__((aligned(64)));
__thread ThreadState thread_state __attribute__((aligned(64)));

void lock_init(){
    ccsynch_lock_init(&lock);
}

void lock_thread_init(){
    threadStateInit(&thread_state);
}

#elif defined (USE_HSYNCH)

void enqueue_cs(int enqueueValue);
int dequeue_cs();
#include "datastructures_bench/synch_algorithms/hsynch.h"

HSynchStruct lock __attribute__((aligned(64)));
__thread HSynchThreadState thread_state __attribute__((aligned(64)));

void lock_init(){
    HSynchStructInit(&lock);
}

void lock_thread_init(){
    HSynchThreadStateInit(&thread_state);
}

#elif defined (USE_FLATCOMB)

void enqueue_cs(int enqueueValue);
int dequeue_cs();
#include "datastructures_bench/synch_algorithms/flat_comb.h"

FlatComb lock __attribute__((aligned(64)));
__thread SlotInfoPtr thread_state __attribute__((aligned(64)));

void lock_init(){
    init_flat_comb(&lock);
}

void lock_thread_init(){
    thread_state.value = NULL;
}

#elif defined (USE_CLH)

#include "datastructures_bench/synch_algorithms/clh.h"

CLHLockStruct lock __attribute__((aligned(64)));

void lock_init(){
    clhLockInitExisting(&lock);
}

void lock_thread_init(){
    clhThreadLocalInit();
}

#elif defined (USE_COHORTLOCK)
#define ARRAY_SIZE 64
#include "datastructures_bench/synch_algorithms/cohortlock.h"

CohortLock lock __attribute__((aligned(64)));

void lock_init(){
    cohortlock_initialize(&lock, NULL);
}

void lock_thread_init(){
    cohortlock_register_this_thread();
}

#elif defined (USE_PTHREADSLOCK)

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void lock_init(){
}

void lock_thread_init(){
}

#elif defined (USE_JONATHAN_LOCKFREE)

void lock_init(){
}

void lock_thread_init(){
}

#endif
//<<<<<<<<<<<<<<<<<<<<<<<
// END Lock depended code
//<<<<<<<<<<<<<<<<<<<<<<<
//=======================

//=======================
//>>>>>>>>>>>>>>>>>>>>>>>
// Datastructure depended code
//>>>>>>>>>>>>>>>>>>>>>>>
//Necessary definitions
//void datastructure_init()
//void datastructure_thread_init()
//void enqueue(int)
//int dequeue()
//void datastructure_destroy();
#ifdef USE_PAIRING_HEAP
#include "datastructures_bench/datastructures/pairingheap/pairingheap.h"
DXPriorityQueue priority_queue __attribute__((aligned(64)));

void datastructure_init(){
    priority_queue.value = NULL;
}

void datastructure_thread_init(){}

void datastructure_destroy(){
    destroy_heap(priority_queue.value);
}

#if defined (USE_NEWREP_QDLOCK)

void enqueue_cs(unsigned int notUsed, void * messagePtr){
    int * typedMessagePtr = (int *) messagePtr;
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
#ifdef DEBUG_PRINT_IN_CS
    printf("ENQ CS %d\n", *typedMessagePtr);
#endif
   priority_queue.value = 
      insert(priority_queue.value, *typedMessagePtr);    
}

void dequeue_cs(unsigned int notUsed, void * messagePtr){
    int ** typedMessagePtr = (int **) messagePtr;
    int * resultLocation = *typedMessagePtr;
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    if(priority_queue.value != NULL){
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", top(priority_queue.value));
#endif
        *resultLocation = top(priority_queue.value);
        priority_queue.value = pop(priority_queue.value);
    }else{
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", -1);
#endif
        *resultLocation = -1;
    }
}

static inline void enqueue(int value){
    LL_delegate(&lock, &enqueue_cs, sizeof(int), &value);
}

static inline int dequeue(){
    int counter = 0;
    int returnValue = INT_MIN;
    int * returnValuePtr = &returnValue;
    int currentValue;
    LL_delegate(&lock, &dequeue_cs, sizeof(int*), &returnValuePtr);
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


#endif

#if defined (USE_QDLOCK) || defined (USE_HQDLOCK) || defined (USE_QDLOCKP) || defined (USE_OYAMA) || defined(USE_QDLOCK_NOSTARVE) || defined(USE_QDLOCK_FUTEX) || defined(USE_HQDLOCK_NOSTARVE) || defined(USE_CPPLOCK)

void enqueue_cs(int enqueueValue, int * notUsed){
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
#ifdef DEBUG_PRINT_IN_CS
    printf("ENQ CS %d\n", enqueueValue);
#endif
   priority_queue.value = 
      insert(priority_queue.value, enqueueValue);    
}

void enqueue_cs_write_back(int enqueueValue, int * resultLocation){
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
#ifdef DEBUG_PRINT_IN_CS
    printf("ENQ CS %d\n", enqueueValue);
#endif
   priority_queue.value = 
      insert(priority_queue.value, enqueueValue);
   *resultLocation = -1;
}

void dequeue_cs(int notUsed, int * resultLocation){
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    if(priority_queue.value != NULL){
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", top(priority_queue.value));
#endif
        //store_rel(*resultLocation, top(priority_queue.value));
	//priority_queue.value = pop(priority_queue.value);
        int local = top(priority_queue.value);
	priority_queue.value = pop(priority_queue.value);
#ifdef USE_CPPLOCK
	*resultLocation = local;
#else
	store_rel(*resultLocation, local);
#endif
    }else{
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", -1);
#endif
        *resultLocation = -1;
    }
}

static inline void enqueue(int value){
#ifdef NO_PURE_DELEGATE
#if defined (USE_QDLOCK) || defined (USE_QDLOCKP) || defined(USE_QDLOCK_NOSTARVE) || defined(USE_QDLOCK_FUTEX) || defined(USE_CPPLOCK)
    adxlock_write_with_response_block(&lock, &enqueue_cs_write_back, value); 
#elif defined (USE_OYAMA)
    oyamalock_write_with_response_block(&lock, &enqueue_cs_write_back, value); 
#else
    hqdlock_write_with_response_block(&lock, &enqueue_cs_write_back, value);
#endif
#else
#if defined (USE_QDLOCK) || defined (USE_QDLOCKP) || defined(USE_QDLOCK_NOSTARVE) || defined(USE_QDLOCK_FUTEX) || defined(USE_CPPLOCK)

    adxlock_delegate(&lock, &enqueue_cs, value);
#elif defined (USE_OYAMA)
    oyamalock_delegate(&lock, &enqueue_cs, value); 
#else
    hqdlock_delegate(&lock, &enqueue_cs, value);
#endif
#endif
}
static inline int dequeue(){
#if defined (USE_QDLOCK) || defined (USE_QDLOCKP) || defined(USE_QDLOCK_NOSTARVE) || defined(USE_QDLOCK_FUTEX) || defined(USE_CPPLOCK)

    return adxlock_write_with_response_block(&lock, &dequeue_cs, 0);
#elif defined (USE_OYAMA)
    return oyamalock_write_with_response_block(&lock, &dequeue_cs, 0);
#else
    return hqdlock_write_with_response_block(&lock, &dequeue_cs, 0);
#endif
}

#elif defined (USE_CCSYNCH) || defined (USE_HSYNCH)

void enqueue_cs(int enqueueValue){
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
#ifdef DEBUG_PRINT_IN_CS
    printf("ENQ CS %d\n", enqueueValue);
#endif
   priority_queue.value = 
      insert(priority_queue.value, enqueueValue + 2);    
}

int dequeue_cs(){
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    if(priority_queue.value != NULL){
        int returnValue = top(priority_queue.value);
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", returnValue - 2);
#endif
        priority_queue.value = pop(priority_queue.value);
        return returnValue;
    }else{
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", -1);
#endif
        return 1;
    }
}


#ifdef USE_CCSYNCH

inline static void enqueue(int value){
    applyOp(&lock, &thread_state, value, 0/*Not used*/);
}
inline static int dequeue(){
    return applyOp(&lock, &thread_state, DEQUEUE_ARG, 0/*Not used*/);
}

#elif defined (USE_HSYNCH)

inline static void enqueue(int value){
#ifdef PINNING
    int my_numa_node = numa_node.value;
#else
    int my_numa_node = sched_getcpu() % NUMBER_OF_NUMA_NODES;
#endif
    applyOp(&lock, &thread_state, value, my_numa_node);
}
inline static int dequeue(){
#ifdef PINNING
    int my_numa_node = numa_node.value;
#else
    int my_numa_node = sched_getcpu() % NUMBER_OF_NUMA_NODES;
#endif
    return applyOp(&lock, &thread_state, DEQUEUE_ARG, my_numa_node);
}

#endif

#elif defined (USE_FLATCOMB)

void enqueue_cs(int enqueueValue){
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
#ifdef DEBUG_PRINT_IN_CS
        printf("ENQ CS %d\n", enqueueValue);
#endif
   priority_queue.value = 
      insert(priority_queue.value, enqueueValue + 2);    
}

int dequeue_cs(){
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    if(priority_queue.value != NULL){
        int returnValue = top(priority_queue.value);
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", returnValue - 2);
#endif
        priority_queue.value = pop(priority_queue.value);
        return returnValue;
    }else{
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", -1);
#endif
        return 1;
    }
}

static inline void enqueue(int value){
    do_op(&lock, &thread_state, value);
    //    adxlock_delegate(&lock, &enqueue_cs, value); 
}
static inline int dequeue(){
    return do_op(&lock, &thread_state, _DEQ_VALUE);
    //adxlock_write_with_response_block(&lock, &dequeue_cs, 0);
}

#elif defined (USE_CLH)

static inline void enqueue(int value){
    clhLock(&lock, 0/*Not used*/);
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
#ifdef DEBUG_PRINT_IN_CS
    printf("ENQ CS %d\n", value);
#endif
    priority_queue.value = 
        insert(priority_queue.value, value);
    clhUnlock(&lock, 0/*Not used*/);
}
static inline int dequeue(){
    int result;
    clhLock(&lock, 0/*Not used*/);
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    if(priority_queue.value != NULL){
        result = top(priority_queue.value);
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", result);
#endif
        priority_queue.value = pop(priority_queue.value);
    }else{
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", -1);
#endif
        result = -1;
    }
    clhUnlock(&lock, 0/*Not used*/);
    return result;
}

#elif defined (USE_COHORTLOCK)

static inline void enqueue(int value){
    cohortlock_write_read_lock(&lock);
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
#ifdef DEBUG_PRINT_IN_CS
    printf("ENQ CS %d\n", value);
#endif
    priority_queue.value = 
        insert(priority_queue.value, value);
    cohortlock_write_read_unlock(&lock);
}
static inline int dequeue(){
    int result;
    cohortlock_write_read_lock(&lock);
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    if(priority_queue.value != NULL){
        result = top(priority_queue.value);
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", result);
#endif
        priority_queue.value = pop(priority_queue.value);
    }else{
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", -1);
#endif
        result = -1;
    }
    cohortlock_write_read_unlock(&lock);
    return result;
}

#elif defined (USE_PTHREADSLOCK)

static inline void enqueue(int value){
    pthread_mutex_lock(&lock);
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
#ifdef DEBUG_PRINT_IN_CS
    printf("ENQ CS %d\n", value);
#endif
    priority_queue.value = 
        insert(priority_queue.value, value);
    pthread_mutex_unlock(&lock);
}
static inline int dequeue(){
    int result;
    pthread_mutex_lock(&lock);
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    if(priority_queue.value != NULL){
        result = top(priority_queue.value);
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", result);
#endif
        priority_queue.value = pop(priority_queue.value);
    }else{
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", -1);
#endif
        result = -1;
    }
    pthread_mutex_unlock(&lock);
    return result;
}

#endif // lock spesific

#elif defined (USE_JONATHAN_LOCKFREE) //USE Pairing heap

#include "datastructures_bench/PR/prioq.h"
typedef struct {
char pad1[FALSE_SHARING_SECURITY];
pq_t * priority_queue;
char pad2[FALSE_SHARING_SECURITY];
} PaddedPqT;

PaddedPqT padded_priority_queue __attribute__((aligned(64)));

void datastructure_init(){
    _init_gc_subsystem();
    padded_priority_queue.priority_queue = pq_init(128);
    insertq(padded_priority_queue.priority_queue, 4, (void *)(long)4);
    deletemin(padded_priority_queue.priority_queue);
}

void datastructure_thread_init(){}

void datastructure_destroy(){
    pq_destroy(padded_priority_queue.priority_queue);
    _destroy_gc_subsystem();
}


static inline void enqueue(int value){

#ifdef DEBUG_PRINT_IN_CS
    printf("ENQ CS %d\n", value);
#endif
    insertq(padded_priority_queue.priority_queue, value, (void *)(long)value);

}
static inline int dequeue(){
    int result;
    void * resultPtr = deletemin(padded_priority_queue.priority_queue);
    if(resultPtr != NULL){
        result = (int)(long)resultPtr;
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", result);
#endif
    }else{
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", -1);
#endif
        result = -1;
    }
    return result;
}

#endif //USE Pairing heap

//###############
//micro bench
//###############
#ifdef USE_MICRO_BENCH

#define GLOBAL_ARRAY_SIZE 64

typedef struct CacheLinePaddedArrayImpl{
    char pad1[128];
    int value[GLOBAL_ARRAY_SIZE];
    char pad2[128];
} CacheLinePaddedArray;

CacheLinePaddedArray global_array  __attribute__((aligned(64)));

void datastructure_init(){
    for(int i = 0; i < GLOBAL_ARRAY_SIZE; i++){
        global_array.value[i] = 0;
    }
}

void datastructure_thread_init(){}

void datastructure_destroy(){}

static inline void cs_work(){
    unsigned short * xsubi = myXsubi;
    for(int i = 0; i < imsw.iterationsSpentCriticalWork; i++){
        int writeToPos1 = (int)(jrand48(xsubi) & ELEMENT_POS_MASK);
        int randomNumber = (int)jrand48(xsubi) & ADD_RAND_NUM_MASK;
        global_array.value[writeToPos1] = global_array.value[writeToPos1] + randomNumber;
        int writeToPos2 = (int)(jrand48(xsubi) & ELEMENT_POS_MASK);
        global_array.value[writeToPos2] = global_array.value[writeToPos2] - randomNumber;
    }
}

#if defined (USE_QDLOCK) || defined (USE_HQDLOCK) || defined (USE_QDLOCKP) || defined (USE_OYAMA) || defined(USE_QDLOCK_NOSTARVE) || defined(USE_QDLOCK_FUTEX) || defined(USE_HQDLOCK_NOSTARVE) || defined(USE_CPPLOCK)

void enqueue_cs(int enqueueValue, int * notUsed){
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
    cs_work();
}

void dequeue_cs(int notUsed, int * resultLocation){
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    cs_work();
    *resultLocation = 1;
}

static inline void enqueue(int value){
#if defined (USE_QDLOCK) || defined (USE_QDLOCKP) || defined(USE_QDLOCK_NOSTARVE) || defined(USE_QDLOCK_FUTEX) || defined(USE_CPPLOCK)
    adxlock_delegate(&lock, &enqueue_cs, value);
#elif defined (USE_OYAMA)
    oyamalock_delegate(&lock, &enqueue_cs, value);
#else
    hqdlock_delegate(&lock, &enqueue_cs, value);
#endif
}
static inline int dequeue(){
#if defined (USE_QDLOCK) || defined (USE_QDLOCKP) || defined(USE_QDLOCK_NOSTARVE) || defined(USE_QDLOCK_FUTEX) || defined(USE_CPPLOCK)
    return adxlock_write_with_response_block(&lock, &dequeue_cs, 0);
#elif defined (USE_OYAMA)
    return oyamalock_write_with_response_block(&lock, &dequeue_cs, 0);
#else
    return hqdlock_write_with_response_block(&lock, &dequeue_cs, 0);
#endif
}

#elif defined (USE_CCSYNCH) || defined (USE_HSYNCH)

void enqueue_cs(int enqueueValue){
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
    cs_work();
}

int dequeue_cs(){
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    cs_work();
    return -1;
}

#ifdef USE_CCSYNCH

inline static void enqueue(int value){
    applyOp(&lock, &thread_state, value, 0/*Not used*/);
}
inline static int dequeue(){
    return applyOp(&lock, &thread_state, DEQUEUE_ARG, 0/*Not used*/);
}

#elif defined (USE_HSYNCH)

inline static void enqueue(int value){
#ifdef PINNING
    int my_numa_node = numa_node.value;
#else
    int my_numa_node = sched_getcpu() % NUMBER_OF_NUMA_NODES;
#endif
    applyOp(&lock, &thread_state, value, my_numa_node);
}
inline static int dequeue(){
#ifdef PINNING
    int my_numa_node = numa_node.value;
#else
    int my_numa_node = sched_getcpu() % NUMBER_OF_NUMA_NODES;
#endif
    return applyOp(&lock, &thread_state, DEQUEUE_ARG, my_numa_node);
}

#endif

#elif defined (USE_FLATCOMB)

void enqueue_cs(int enqueueValue){
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
    cs_work();
}

int dequeue_cs(){
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    cs_work();
    return 1;
}

static inline void enqueue(int value){
    do_op(&lock, &thread_state, value);
}
static inline int dequeue(){
    return do_op(&lock, &thread_state, _DEQ_VALUE);
}

#elif defined (USE_CLH)

static inline void enqueue(int value){
    clhLock(&lock, 0/*Not used*/);
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
    cs_work();
    clhUnlock(&lock, 0/*Not used*/);
}
static inline int dequeue(){
    clhLock(&lock, 0/*Not used*/);
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    cs_work();
    clhUnlock(&lock, 0/*Not used*/);
    return 1;
}

#elif defined (USE_COHORTLOCK)

static inline void enqueue(int value){
    cohortlock_write_read_lock(&lock);
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
    cs_work();
    cohortlock_write_read_unlock(&lock);
}
static inline int dequeue(){
    cohortlock_write_read_lock(&lock);
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    cs_work();
    cohortlock_write_read_unlock(&lock);
    return 1;
}

#elif defined (USE_PTHREADSLOCK)

static inline void enqueue(int value){
    pthread_mutex_lock(&lock);
#ifdef SANITY_CHECK
    enqueues_executed.value++;
#endif
    cs_work();
    pthread_mutex_unlock(&lock);
}
static inline int dequeue(){
    pthread_mutex_lock(&lock);
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    cs_work();
    pthread_mutex_unlock(&lock);
    return 1;
}

#endif // lock spesific

#endif //USE Micro Bench

//######################
// shared fetch and add
//######################

#ifdef USE_SHARED_FETCH_AND_ADD

typedef struct CacheLinePaddedFaAWord{
    char pad1[128];
    int value;
    char pad2[128];
} CacheLinePaddedFaAWord;

CacheLinePaddedFaAWord fetchAndAddWord  __attribute__((aligned(64)));

void datastructure_init(){
    fetchAndAddWord.value = 0;
}

void lock_init(){}

void lock_thread_init(){}

void datastructure_thread_init(){}

void datastructure_destroy(){}

static inline void enqueue(int value){
    __sync_fetch_and_add(&fetchAndAddWord.value, 1);
}
static inline int dequeue(){
    return __sync_fetch_and_add(&fetchAndAddWord.value, 1);    
}
#endif

//<<<<<<<<<<<<<<<<<<<<<<<
// Datastructure depended code
//<<<<<<<<<<<<<<<<<<<<<<<
//=======================


#ifdef PINNING

int numa_structure[NUMBER_OF_NUMA_NODES][NUMBER_OF_CPUS_PER_NODE] = NUMA_STRUCTURE;
int core_in_node_counters[NUMBER_OF_NUMA_NODES];
int next_numa_node = 0;
bool first = true;
pthread_mutex_t thread_pin_mutex = PTHREAD_MUTEX_INITIALIZER;

void pin(int thread_id){
    pthread_mutex_lock(&thread_pin_mutex);
    if(first){
        first = false;
        for(int i = 0; i < NUMBER_OF_NUMA_NODES; i++){
            core_in_node_counters[i] = 0;
        }
    }


#ifdef NODE_FIRST_POLICY
    if((core_in_node_counters[next_numa_node] != 0) && (0 == (core_in_node_counters[next_numa_node] % NUMBER_OF_CPUS_PER_NODE))){
        next_numa_node++;
    }
#endif
#ifdef CORE_FIRST_POLICY_SANDY
    if((core_in_node_counters[next_numa_node] != 0) && (0 == (core_in_node_counters[next_numa_node] % NUMBER_OF_CPUS_PER_NODE))){
        next_numa_node++;
    }
#endif

    int node = next_numa_node % NUMBER_OF_NUMA_NODES;
    numa_node.value = node;   
    int core_in_node = core_in_node_counters[next_numa_node] % NUMBER_OF_CPUS_PER_NODE;
#ifdef CORE_FIRST_POLICY_SANDY
    if(((core_in_node+1) % 2) == 0){
        core_in_node = NUMBER_OF_CPUS_PER_NODE/2 + ((core_in_node+1)/2) - 1;
    }else if(core_in_node != 0){
        core_in_node = core_in_node / 2;
    }
#endif

    int core_to_pin_to = numa_structure[node][core_in_node]; 

#ifdef SPREAD_PINNING_POLICY
    next_numa_node++;
#endif
    core_in_node_counters[next_numa_node]++;

    int ret = 0;
    cpu_set_t mask;  
    unsigned int len = sizeof(mask);
    CPU_ZERO(&mask);
    CPU_SET(core_to_pin_to, &mask);
    ret = sched_setaffinity(0, len, &mask);
    if (ret == -1){
        printf("sched_setaffinity failed!!\n");
        exit(0);
    }
    pthread_mutex_unlock(&thread_pin_mutex);
}
#endif

//=============
//The Benchmark
//=============

int globalDummy = 0; //To avoid that the compiler optimize away read 

void *mixed_read_write_benchmark_thread(void *lockThreadLocalSeedPointer){
    lock_thread_init();
    datastructure_thread_init();
    LockThreadLocalSeed * lockThreadLocalSeed = (LockThreadLocalSeed *)lockThreadLocalSeedPointer; 
    int privateArray[NUMBER_OF_ELEMENTS_IN_ARRAYS];
    unsigned short * xsubi = lockThreadLocalSeed->seed;
#ifdef PINNING
    //pin(lockThreadLocalSeed->thread_id);
#endif
    myXsubi = xsubi;
    int dummy = 0;//To avoid that the compiler optimize away read
    int totalNumberOfCriticalSections = 0;
    //START LINE
    while(!ACCESS_ONCE(*benchmarkStarted)){
        __sync_synchronize();
    }
    while(!ACCESS_ONCE(*benchmarkStoped)){
        if(erand48(xsubi) > imsw.percentageDequeue){
            int randomNumber = 1 + ((int)jrand48(xsubi) & ADD_RAND_NUM_MASK);
#ifdef DEBUG_PRINT_OUTSIDE_CS
            printf("ENQ OUT %d\n", randomNumber);
#endif
            enqueue(randomNumber);
#ifdef SANITY_CHECK
            __sync_fetch_and_add(&enqueues_issued.value, 1);
#endif
        }else{
            int dequeueValue = dequeue();
#ifdef SANITY_CHECK
            __sync_fetch_and_add(&dequeues_issued.value, 1);
#endif
#ifdef DEBUG_PRINT_OUTSIDE_CS
            printf("DEQ OUT %d\n", dequeueValue);
#endif
            dummy = dummy + dequeueValue;        
        }
#ifdef RANDOM_LOCAL_WORK
        int workIterations = MIN_LOCAL_WORK_WHEN_RANDOM;
        if(imsw.iterationsSpentNonCriticalWork != 0){
            workIterations = MIN_LOCAL_WORK_WHEN_RANDOM + ((int)jrand48(xsubi)) % imsw.iterationsSpentNonCriticalWork;
        }       
        for(int u = 0; u < workIterations; u++){
#else
        for(int u = 0; u < imsw.iterationsSpentNonCriticalWork; u++){
#endif
            int writeToPos1 = (int)(jrand48(xsubi) & ELEMENT_POS_MASK);
            int randomNumber = (int)jrand48(xsubi) & ADD_RAND_NUM_MASK;
            privateArray[writeToPos1] = privateArray[writeToPos1] + randomNumber;
            int writeToPos2 = (int)(jrand48(xsubi)  & ELEMENT_POS_MASK);
            privateArray[writeToPos2] = privateArray[writeToPos2] - randomNumber;
        }
        totalNumberOfCriticalSections++;
    }

    globalDummy = dummy + privateArray[0];

#ifdef QUEUE_STATS
#ifdef PRINT_THREAD_QUEUE_STATS
    printf("NUMA NODE: %d\n", numa_node.value);
    printf("help sessions: %lu\n", helpSeasonsPerformed.value);
    printf("helped operat: %lu\n", numberOfDeques.value);
#endif
    lockThreadLocalSeed->helpSeasonsPerformed = helpSeasonsPerformed.value;
    lockThreadLocalSeed->numberOfDeques = numberOfDeques.value;
#endif
    lockThreadLocalSeed->totalNumerOfIterationsExecuted = (long)totalNumberOfCriticalSections;
    return lockThreadLocalSeed;
}

//================
//Benchmark Runner
//================

#ifdef QUEUE_STATS
double dequesPerFlush;
#endif

typedef struct {
	long ops;
	long time;
} result;
result benchmark_parallel_mixed_enqueue_dequeue(double percentageDequeueParam,
                                                int numberOfThreads,
                                                int benchmarkTimeSeconds,
                                                int iterationsSpentCriticalWorkParam,
                                                int iterationsSpentInNonCriticalWorkParam){
    if(numberOfThreads>512){
        printf("You need to increase the threadLocalSeeds variable!\n");
        assert(false);
    }

    imsw.percentageDequeue = percentageDequeueParam;
    imsw.iterationsSpentCriticalWork = iterationsSpentCriticalWorkParam;
    imsw.iterationsSpentNonCriticalWork = iterationsSpentInNonCriticalWorkParam;
    lock_init();
    datastructure_init();
    *benchmarkStarted = false;
    *benchmarkStoped = false;
    pthread_t threads[numberOfThreads];
    struct timeval timeStart;
    struct timeval timeEnd;
    LockThreadLocalSeed lockThreadLocalSeeds[numberOfThreads];
    for(int i = 0; i < numberOfThreads; i++){
        unsigned short * seed = threadLocalSeeds[i].value;
        srand48(i);
        unsigned short * seedResult = seed48(seed);
        seed[0] = seedResult[0];
        seed[1] = seedResult[1];
        seed[2] = seedResult[2];
        LockThreadLocalSeed * lockThreadLocalSeed = &lockThreadLocalSeeds[i];
        lockThreadLocalSeed->seed = seed;
        lockThreadLocalSeed->thread_id = i;
        pthread_create(&threads[i],NULL,&mixed_read_write_benchmark_thread,lockThreadLocalSeed);
    }
    usleep(100000);//To make sure all threads are set up
    gettimeofday(&timeStart, NULL);
    *benchmarkStarted = true;
    __sync_synchronize();

    usleep(1000000 * benchmarkTimeSeconds);


    *benchmarkStoped = true;
    __sync_synchronize();

    long totalNumberOfOperations = 0;
#ifdef QUEUE_STATS
    long totalNumberOfFlushs = 0;
    long totalNumberOfDeques = 0;
#endif
    for(int i = 0; i < numberOfThreads; i++){
        LockThreadLocalSeed * threadInfo;
        pthread_join(threads[i],(void*)&threadInfo);
        long threadNumberOfOperations = threadInfo->totalNumerOfIterationsExecuted;
        totalNumberOfOperations = totalNumberOfOperations + threadNumberOfOperations;
#ifdef QUEUE_STATS
        totalNumberOfFlushs = totalNumberOfFlushs + threadInfo->helpSeasonsPerformed;
        totalNumberOfDeques = totalNumberOfDeques + threadInfo->numberOfDeques;
#endif
    }

    gettimeofday(&timeEnd, NULL);

    datastructure_destroy();

#ifdef SANITY_CHECK
    if((dequeues_executed.value != dequeues_issued.value) ||
                (enqueues_executed.value != enqueues_issued.value)){
        printf("\033[31m SANITY_CHECK FAIL:\033[m\n");
        printf(" dequeues_issued == %d\n", dequeues_issued.value);
        printf(" dequeues_executed.value == %d\n", dequeues_executed.value);
        printf(" enqueues_issued == %d\n", enqueues_issued.value);
        printf(" enqueues_executed.value == %d\n", enqueues_executed.value);
    }
#endif

    long benchmarRealTime = (timeEnd.tv_sec-timeStart.tv_sec)*1000000 + timeEnd.tv_usec-timeStart.tv_usec;

#ifdef QUEUE_STATS
    if(totalNumberOfFlushs == 0){
        dequesPerFlush = 0.0;
    }else{
        dequesPerFlush = ((double)totalNumberOfDeques)/((double)totalNumberOfFlushs);
    }
#endif    

    result r;
    r.ops = totalNumberOfOperations;
    r.time = benchmarRealTime;

    return r;
}


void run_scaling_benchmark(int numOfThreads,
                           double percentageDequeueParam,
                           int benchmarkTimeSeconds,
                           int iterationsSpentCriticalWorkParam,
                           int iterationsSpentInNonCriticalWorkParam){
            
    fprintf(stderr, "=> Benchmark %d threads\n", numOfThreads);
    result r = benchmark_parallel_mixed_enqueue_dequeue(percentageDequeueParam,
                                                           numOfThreads,
                                                           benchmarkTimeSeconds,
                                                           iterationsSpentCriticalWorkParam,
                                                           iterationsSpentInNonCriticalWorkParam);
#ifdef QUEUE_STATS
    printf("%d %ld %ld %f\n", numOfThreads, r.time, r.ops, dequesPerFlush);
    fprintf(stderr, "|| %ld microseconds, %ld operations, %f deques/flush (%d threads)\n", r.time, r.ops, dequesPerFlush, numOfThreads);
#else
    printf("%d %ld %ld\n", numOfThreads, r.time, r.ops);
    fprintf(stderr, "|| %ld microseconds, %ld operations, (%d threads)\n", r.time, r.ops, numOfThreads);
#endif

}


int main(int argc, char **argv){
    int numberOfStandardArgs = 6;
    if((argc-1) < numberOfStandardArgs){
        printf("The benchmark requires the following paramters:\n");
        printf("\n");
        printf("* Number of threads\n");
        printf("* Percentage dequeue\n");
        printf("* Number of seconds to benchmark\n");
        printf("* Iterations critical work\n");
        printf("* Ignored!\n");
        printf("* Iterations non-critical work\n");
        printf("\n");
        printf("Example\n");
        printf("%s 8 0.5 10 1 0 0\n", argv[0]);
    }else{
        int numOfThreads = atoi(argv[1]);
        double percentageDequeue = atof(argv[2]);
        int numberOfSecondsToBenchmark = atoi(argv[3]);
        int iterationsSpentCriticalWork = atoi(argv[4]);
        int iterationsSpentInNonCriticalWork = atoi(argv[6]);

        run_scaling_benchmark(numOfThreads,
                              percentageDequeue,
                              numberOfSecondsToBenchmark,
                              iterationsSpentCriticalWork,
                              iterationsSpentInNonCriticalWork);   
    }
    exit(0);                          
}
