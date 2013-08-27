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

//#define DEBUG_PRINT_IN_CS
//#define DEBUG_PRINT_OUTSIDE_CS
#define SANITY_CHECK

#ifdef SANITY_CHECK
CacheLinePaddedInt dequeues_executed = {.value = 0};
CacheLinePaddedInt enqueues_executed = {.value = 0};
CacheLinePaddedInt dequeues_issued = {.value = 0};
CacheLinePaddedInt enqueues_issued = {.value = 0};
#endif


//=======================
//>>>>>>>>>>>>>>>>>>>>>>>
// Lock depended code
//>>>>>>>>>>>>>>>>>>>>>>>
//Necessary definations
//void lock_init()
//void lock_thread_init()

#ifdef USE_QDLOCK

#include "datastructures_bench/synch_algorithms/qdlock.h"

AgnosticDXLock lock __attribute__((aligned(64)));

void lock_init(){
    adxlock_initialize(&lock, NULL);
}

void lock_thread_init(){}

#elif defined (USE_CCSYNCH)

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

#ifdef USE_QDLOCK

void enqueue_cs(int enqueueValue, int * notUsed){
#ifdef DEBUG_PRINT_IN_CS
    printf("ENQ CS %d\n", enqueueValue);
#endif
   priority_queue.value = 
      insert(priority_queue.value, enqueueValue);    
}

void dequeue_cs(int notUsed, int * resultLocation){
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

inline void enqueue(int value){
    adxlock_delegate(&lock, &enqueue_cs, value); 
}
inline int dequeue(){
    return adxlock_write_with_response_block(&lock, &dequeue_cs, 0);
}

#elif defined (USE_CCSYNCH) || defined (USE_HSYNCH)

int enqueue_cs(int enqueueValue){
#ifdef DEBUG_PRINT_IN_CS
        printf("ENQ CS %d\n", enqueueValue);
#endif
   priority_queue.value = 
      insert(priority_queue.value, enqueueValue);    
   return 0;
}

int dequeue_cs(int notUsed){
#ifdef SANITY_CHECK
    dequeues_executed.value++;
#endif
    if(priority_queue.value != NULL){
        int returnValue = top(priority_queue.value);
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", returnValue);
#endif
        priority_queue.value = pop(priority_queue.value);
        return returnValue;
    }else{
#ifdef DEBUG_PRINT_IN_CS
        printf("DEQ CS %d\n", -1);
#endif
        return -1;
    }
}

#ifdef USE_CCSYNCH

inline static void enqueue(int value){
    applyOp(&lock, &thread_state, &enqueue_cs, value, 0/*Not used*/);
}
inline static int dequeue(){
    return applyOp(&lock, &thread_state, &dequeue_cs, 0/*Not used*/, 0/*Not used*/);
}

#elif defined (USE_HSYNCH)

inline static void enqueue(int value){
    applyOp(&lock, &thread_state, &enqueue_cs, value, sched_getcpu());
}
inline static int dequeue(){
    return applyOp(&lock, &thread_state, &dequeue_cs, 0/*Not used*/, sched_getcpu());
}

#endif

#elif defined (USE_FLATCOMB)

void enqueue_cs(int enqueueValue){
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

inline void enqueue(int value){
    do_op(&lock, &thread_state, value);
    //    adxlock_delegate(&lock, &enqueue_cs, value); 
}
inline int dequeue(){
    return do_op(&lock, &thread_state, _DEQ_VALUE);
    //adxlock_write_with_response_block(&lock, &dequeue_cs, 0);
}

#endif // lock spesific

#endif //USE Pairing heap
//<<<<<<<<<<<<<<<<<<<<<<<
// Datastructure depended code
//<<<<<<<<<<<<<<<<<<<<<<<
//=======================



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
    unsigned short * seed;
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

SeedWrapper threadLocalSeeds[128] __attribute__((aligned(64)));

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
        for(int u = 0; u < imsw.iterationsSpentNonCriticalWork; u++){
            int writeToPos1 = (int)(jrand48(xsubi) & ELEMENT_POS_MASK);
            int randomNumber = (int)jrand48(xsubi) & ADD_RAND_NUM_MASK;
            privateArray[writeToPos1] = privateArray[writeToPos1] + randomNumber;
            int writeToPos2 = (int)(jrand48(xsubi)  & ELEMENT_POS_MASK);
            privateArray[writeToPos2] = privateArray[writeToPos2] - randomNumber;
        }
        totalNumberOfCriticalSections++;
    }

    globalDummy = dummy + privateArray[0];

    return (void*)((long)totalNumberOfCriticalSections);
}

//================
//Benchmark Runner
//================


double benchmark_parallel_mixed_enqueue_dequeue(double percentageDequeueParam, 
                                                int numberOfThreads,
                                                int benchmarkTimeSeconds,
                                                int iterationsSpentCriticalWorkParam,
                                                int iterationsSpentInNonCriticalWorkParam){
    if(numberOfThreads>128){
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
    for(int i = 0; i < numberOfThreads; i ++){
        unsigned short * seed = threadLocalSeeds[i].value;
        srand48(i);
        unsigned short * seedResult = seed48(seed);
        seed[0] = seedResult[0];
        seed[1] = seedResult[1];
        seed[2] = seedResult[2];
        LockThreadLocalSeed * lockThreadLocalSeed = &lockThreadLocalSeeds[i];
        lockThreadLocalSeed->seed = seed;
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
    for(int i = 0; i < numberOfThreads; i++){
        long threadNumberOfOperations;
        pthread_join(threads[i],(void*)&threadNumberOfOperations);
        totalNumberOfOperations = totalNumberOfOperations + threadNumberOfOperations;
    }
    gettimeofday(&timeEnd, NULL);

    datastructure_destroy();

#ifdef SANITY_CHECK
    if(dequeues_executed.value != dequeues_issued.value){
        printf("\033[31m SANITY_CHECK FAIL:\033[m\n dequeues_issued == %d\n dequeues_executed == %d\n", dequeues_issued.value, dequeues_executed.value);
    }
#endif

    long benchmarRealTime = (timeEnd.tv_sec-timeStart.tv_sec)*1000000 + timeEnd.tv_usec-timeStart.tv_usec;

    double timePerOp = ((double)benchmarRealTime)/((double)totalNumberOfOperations);
    return timePerOp;
}


void run_scaling_benchmark(int numOfThreads,
                           double percentageDequeueParam,
                           int benchmarkTimeSeconds,
                           int iterationsSpentCriticalWorkParam,
                           int iterationsSpentInNonCriticalWorkParam){
            
    fprintf(stderr, "=> Benchmark %d threads\n", numOfThreads);
    double time = benchmark_parallel_mixed_enqueue_dequeue(percentageDequeueParam, 
                                                           numOfThreads,
                                                           benchmarkTimeSeconds,
                                                           iterationsSpentCriticalWorkParam,
                                                           iterationsSpentInNonCriticalWorkParam);
    printf("%d %f\n", numOfThreads, time);
    fprintf(stderr, "|| %f microseconds/operation (%d threads)\n", time, numOfThreads);

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
