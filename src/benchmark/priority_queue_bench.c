
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>
#include <limits.h>
#include "utils/smp_utils.h"
#include "benchmark/skiplist/skiplist.h"
#include "utils/smp_utils.h"
#include "utils/support_many_lock_types.h"
#include "benchmark/pairingheap/dxlocked_pairingheap.h"

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

#define PAIRING_HEAP_ENQUEUE_WORK                                       \
    do {                                                                \
        int randomNumber = 1 + ((int)jrand48(xsubi) & ADD_RAND_NUM_MASK); \
        dx_pq_ph_enqueue(randomNumber);                                 \
    } while (0)

#define PAIRING_HEAP_DEQUEUE_WORK                                       \
    do {                                                                \
        dummy = dummy + dx_pq_ph_dequeue();                             \
    } while (0)

#define WORK_IN_NON_CRITICAL_SECTION                                  \
    do {                                                                \
        int writeToPos1 = (int)(jrand48(xsubi) & ELEMENT_POS_MASK);     \
        int randomNumber = (int)jrand48(xsubi) & ADD_RAND_NUM_MASK;     \
        privateArray[writeToPos1] = privateArray[writeToPos1] + randomNumber; \
        int writeToPos2 = (int)(jrand48(xsubi)  & ELEMENT_POS_MASK);    \
        privateArray[writeToPos2] = privateArray[writeToPos2] - randomNumber; \
    } while (0)
#define PAIRING_HEAP 1
#ifdef PAIRING_HEAP
#define PRIORITY_QUEUE_ENQUEUE_WORK PAIRING_HEAP_ENQUEUE_WORK
#define PRIORITY_QUEUE_DEQUEUE_WORK PAIRING_HEAP_DEQUEUE_WORK
#endif


int globalDummy = 0; //To avoid that the compiler optimize away read 

void *mixed_read_write_benchmark_thread(void *lockThreadLocalSeedPointer){
    LOCK_REGISTER_THIS_THREAD();
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
            PRIORITY_QUEUE_ENQUEUE_WORK;
        }else{
            PRIORITY_QUEUE_DEQUEUE_WORK;
        }
        for(int u = 0; u < imsw.iterationsSpentNonCriticalWork; u++){
            WORK_IN_NON_CRITICAL_SECTION;
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
    dx_pq_ph_init();
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

    dx_pq_ph_destroy();

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
