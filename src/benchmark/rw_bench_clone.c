
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
#include <sched.h>

//Should be power of 2
#define NUMBER_OF_ELEMENTS_IN_ARRAYS 64
#define ELEMENT_POS_MASK (NUMBER_OF_ELEMENTS_IN_ARRAYS-1)
#define ADD_RAND_NUM_MASK (128-1)
#define FALSE_SHARING_SECURITY 128


#ifdef PINNING
__thread CacheLinePaddedInt numa_node __attribute__((aligned(128)));
//#define CORE_FIRST_POLICY_SANDY
//#define SPREAD_PINNING_POLICY
#define NODE_FIRST_POLICY
#endif

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


typedef struct LockWrapperImpl{
    char pad1[FALSE_SHARING_SECURITY];
    LOCK_DATATYPE_NAME value;
    char pad2[FALSE_SHARING_SECURITY];
} LockWrapper;

typedef struct LockThreadLocalSeedImpl{
    int thread_id;
    LOCK_DATATYPE_NAME * lock;
    unsigned short * seed;
} LockThreadLocalSeed;


//=======================
//Benchmark mutable state (Important that they are all on separate cache lines)
//=======================

LockWrapper lock_wrapper __attribute__((aligned(64)));
LOCK_DATATYPE_NAME * lock = &lock_wrapper.value;

SeedWrapper write_d_rand_seed_wrapper __attribute__((aligned(64)));
unsigned short * write_d_rand_seed = write_d_rand_seed_wrapper.value;

BoolWrapper benchmarkStopedWrapper __attribute__((aligned(64)));
bool * benchmarkStoped = &benchmarkStopedWrapper.value;

BoolWrapper benchmarkStartedWrapper __attribute__((aligned(64)));
bool * benchmarkStarted = &benchmarkStartedWrapper.value;

int sharedArrayWrapper[NUMBER_OF_ELEMENTS_IN_ARRAYS+2*FALSE_SHARING_SECURITY] __attribute__((aligned(64)));
int * sharedArray = &sharedArrayWrapper[FALSE_SHARING_SECURITY];

SeedWrapper threadLocalSeeds[128] __attribute__((aligned(64)));

//========================
//Benchmark imutable state
//========================

typedef struct ImutableStateWrapperImpl{
    char pad1[FALSE_SHARING_SECURITY];
    int iterationsSpentInWriteCriticalSection;
    int iterationsSpentInReadCriticalSection;
    int iterationsSpentInNonCriticalSection;
    double percentageRead;
    char pad2[FALSE_SHARING_SECURITY];
} ImutableStateWrapper;

ImutableStateWrapper imsw  __attribute__((aligned(64))) = 
{.iterationsSpentInWriteCriticalSection = 4,
 .iterationsSpentInReadCriticalSection = 4,
 .iterationsSpentInNonCriticalSection = 64,
 .percentageRead = 0.8};

__thread unsigned short * myXsubi;

//=============
//The Benchmark
//=============

#define WORK_IN_WRITE_CRITICAL_SECTION_1                                \
    do {                                                                \
        int writeToPos1 = (int)(jrand48(xsubi) & ELEMENT_POS_MASK);     \
        int randomNumber = (int)jrand48(xsubi) & ADD_RAND_NUM_MASK;     \
        sharedArray[writeToPos1] = sharedArray[writeToPos1] + randomNumber; \
        int writeToPos2 = (int)(jrand48(xsubi) & ELEMENT_POS_MASK);     \
        sharedArray[writeToPos2] = sharedArray[writeToPos2] - randomNumber; \
    } while (0)

#define WORK_IN_WRITE_CRITICAL_SECTION_2                                \
    do {                                                                \
        int randomNumber = (int)(long)x;                                    \
        int writeToPos1 = (int)(jrand48(xsubi) & ELEMENT_POS_MASK);         \
        sharedArray[writeToPos1] = sharedArray[writeToPos1] + randomNumber; \
        int writeToPos2 = (int)(jrand48(xsubi) & ELEMENT_POS_MASK);     \
        sharedArray[writeToPos2] = sharedArray[writeToPos2] - randomNumber; \
    } while (0)


#define WORK_IN_READ_CRITICAL_SECTION_1                                 \
    do {                                                                \
        int readPos1 = (int)(jrand48(xsubi) & ELEMENT_POS_MASK);        \
        dummy = dummy + ACCESS_ONCE(sharedArray[readPos1]);             \
        int readPos2 = (int)(jrand48(xsubi) & ELEMENT_POS_MASK);        \
        dummy = dummy + ACCESS_ONCE(sharedArray[readPos2]);             \
    } while (0)

#define WORK_IN_NON_CRITICAL_SECTION_1                                  \
    do {                                                                \
        int writeToPos1 = (int)(jrand48(xsubi) & ELEMENT_POS_MASK);     \
        int randomNumber = (int)jrand48(xsubi) & ADD_RAND_NUM_MASK;     \
        privateArray[writeToPos1] = privateArray[writeToPos1] + randomNumber; \
        int writeToPos2 = (int)(jrand48(xsubi)  & ELEMENT_POS_MASK);    \
        privateArray[writeToPos2] = privateArray[writeToPos2] - randomNumber; \
    } while (0)

#ifdef RW_BENCH_CLONE
#define WORK_IN_WRITE_CRITICAL_SECTION WORK_IN_WRITE_CRITICAL_SECTION_1
#define WORK_IN_READ_CRITICAL_SECTION WORK_IN_READ_CRITICAL_SECTION_1
#define WORK_IN_NON_CRITICAL_SECTION WORK_IN_NON_CRITICAL_SECTION_1
#endif

#ifdef RW_BENCH_MEM_TRANSFER
#define WORK_IN_WRITE_CRITICAL_SECTION WORK_IN_WRITE_CRITICAL_SECTION_2
#define WORK_IN_READ_CRITICAL_SECTION WORK_IN_READ_CRITICAL_SECTION_1
#define WORK_IN_NON_CRITICAL_SECTION WORK_IN_NON_CRITICAL_SECTION_1
#endif

void mixed_read_write_benchmark_writer(void * x, void ** writeBackLocation){
    unsigned short * xsubi = myXsubi;
    for(int i = 0; i < imsw.iterationsSpentInWriteCriticalSection; i++){
        WORK_IN_WRITE_CRITICAL_SECTION;
    }
}


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



int globalDummy = 0; //To avoid that the compiler optimize away read 

void *mixed_read_write_benchmark_thread(void *lockThreadLocalSeedPointer){
    LOCK_REGISTER_THIS_THREAD();
    LockThreadLocalSeed * lockThreadLocalSeed = (LockThreadLocalSeed *)lockThreadLocalSeedPointer; 
    LOCK_DATATYPE_NAME * lock = lockThreadLocalSeed->lock;
    int privateArray[NUMBER_OF_ELEMENTS_IN_ARRAYS];
    unsigned short * xsubi = lockThreadLocalSeed->seed;
    myXsubi = xsubi;
    int dummy = 0;//To avoid that the compiler optimize away read
    int totalNumberOfCriticalSections = 0;

#ifdef PINNING
    pin(lockThreadLocalSeed->thread_id);
#endif

    //START LINE
    while(!ACCESS_ONCE(*benchmarkStarted)){
        __sync_synchronize();
    }
    while(!ACCESS_ONCE(*benchmarkStoped)){
        if(erand48(xsubi) > imsw.percentageRead){
#ifdef RW_BENCH_CLONE
            LOCK_WRITE(lock, (void *)1);
#else
            int randomNumber = (int)(jrand48(xsubi) & ADD_RAND_NUM_MASK) + 1;
            LOCK_WRITE(lock, (void *)(long)randomNumber);
#endif
        }else{
            LOCK_READ_LOCK(lock);
            for(int u = 0; u < imsw.iterationsSpentInReadCriticalSection; u++){
                WORK_IN_READ_CRITICAL_SECTION;
            }
            LOCK_READ_UNLOCK(lock);
        }
        for(int u = 0; u < imsw.iterationsSpentInNonCriticalSection; u++){
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


typedef struct {
	long ops;
	long time;
} result;
result benchmark_parallel_mixed_read_write(double percentageReadParam, 
                                           int numberOfThreads,
                                           int benchmarkTimeSeconds,
                                           int iterationsSpentInWriteCriticalSectionParam,
                                           int iterationsSpentInReadCriticalSectionParam,
                                           int iterationsSpentInNonCriticalSectionParam){
    if(numberOfThreads>128){
        printf("You need to increase the threadLocalSeeds variable!\n");
        assert(false);
    }

    imsw.percentageRead = percentageReadParam;
    imsw.iterationsSpentInWriteCriticalSection = iterationsSpentInWriteCriticalSectionParam;
    imsw.iterationsSpentInReadCriticalSection = iterationsSpentInReadCriticalSectionParam;
    imsw.iterationsSpentInNonCriticalSection = iterationsSpentInNonCriticalSectionParam;
    *benchmarkStarted = false;
    *benchmarkStoped = false;
    LOCK_INITIALIZE(lock, &mixed_read_write_benchmark_writer);
    pthread_t threads[numberOfThreads];
    struct timeval timeStart;
    struct timeval timeEnd;
    for(int i = 0; i < NUMBER_OF_ELEMENTS_IN_ARRAYS; i++){
        sharedArray[i] = 0;
    }
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
        lockThreadLocalSeed->lock = lock;
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
    for(int i = 0; i < numberOfThreads; i++){
        long threadNumberOfOperations;
        pthread_join(threads[i],(void*)&threadNumberOfOperations);
        totalNumberOfOperations = totalNumberOfOperations + threadNumberOfOperations;
    }
    gettimeofday(&timeEnd, NULL);

    //Assert that sum of shared array is zero
    long sharedArraySum = 0;
    for(int i = 0; i < NUMBER_OF_ELEMENTS_IN_ARRAYS; i++){
        sharedArraySum = sharedArraySum + sharedArray[i];
    }
    assert(sharedArraySum == 0);

    long benchmarRealTime = (timeEnd.tv_sec-timeStart.tv_sec)*1000000 + timeEnd.tv_usec-timeStart.tv_usec;

    result r;
    r.ops = totalNumberOfOperations;
    r.time = benchmarRealTime;
    return r;
}


void run_scaling_benchmark(int numOfThreads,
                           double percentageReadParam,
                           int benchmarkTimeSeconds,
                           int iterationsSpentInWriteCriticalSectionParam,
                           int iterationsSpentInReadCriticalSectionParam,
                           int iterationsSpentInNonCriticalSectionParam){
            
    fprintf(stderr, "=> Benchmark %d threads\n", numOfThreads);
    result r = benchmark_parallel_mixed_read_write(percentageReadParam, 
                                                      numOfThreads,
                                                      benchmarkTimeSeconds,
                                                      iterationsSpentInWriteCriticalSectionParam,
                                                      iterationsSpentInReadCriticalSectionParam,
                                                      iterationsSpentInNonCriticalSectionParam);
    printf("%d %ld %ld\n", numOfThreads, r.time, r.ops);
    fprintf(stderr, "|| %ld microseconds, %ld operations (%d threads)\n", r.time, r.ops, numOfThreads);

}


int main(int argc, char **argv){
    int numberOfStandardArgs = 6;
    if((argc-1) < numberOfStandardArgs){
        printf("The benchmark requires the following paramters:\n");
        printf("\n");
        printf("* Number of threads\n");
        printf("* Percentage read\n");
        printf("* Number of seconds to benchmark\n");
        printf("* Iterations spent in write critical section\n");
        printf("* Iterations spent in read critical section\n");
        printf("* Iterations spent in non critical section\n");
        printf("\n");
        printf("Example\n");
        printf("%s 8 0.8 10 4 4 64\n", argv[0]);
    }else{
        int numOfThreads = atoi(argv[1]);
        double percentageRead = atof(argv[2]);
        int numberOfSecondsToBenchmark = atoi(argv[3]);
        int iterationsSpentInWriteCriticalSection = atoi(argv[4]);
        int iterationsSpentInReadCriticalSection = atoi(argv[5]);
        int iterationsSpentInNonCriticalSection = atoi(argv[6]);

        run_scaling_benchmark(numOfThreads,
                              percentageRead,
                              numberOfSecondsToBenchmark,
                              iterationsSpentInWriteCriticalSection,
                              iterationsSpentInReadCriticalSection,
                              iterationsSpentInNonCriticalSection);   
    }
    exit(0);                          
}
