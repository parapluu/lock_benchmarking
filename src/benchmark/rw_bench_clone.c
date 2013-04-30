
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>
#include <limits.h>
#include "smp_utils.h"
#include "skiplist.h"
#include "smp_utils.h"
#include "support_many_lock_types.h"

//Should be power of 2
#define NUMBER_OF_ELEMENTS_IN_ARRAYS 64
#define ELEMENT_POS_MASK (NUMBER_OF_ELEMENTS_IN_ARRAYS-1)
#define ADD_RAND_NUM_MASK (128-1)
#define FALSE_SHARING_SECURITY 128


typedef struct SeedWrapperImpl{
    char pad1[FALSE_SHARING_SECURITY];
    unsigned short value[3];
    char pad2[FALSE_SHARING_SECURITY - sizeof(short)];
} SeedWrapper;

typedef struct BoolWrapperImpl{
    char pad1[FALSE_SHARING_SECURITY];
    bool value;
    char pad2[FALSE_SHARING_SECURITY - sizeof(bool)];
} BoolWrapper;


typedef struct LockWrapperImpl{
    char pad1[FALSE_SHARING_SECURITY];
    LOCK_DATATYPE_NAME value;
    char pad2[sizeof(FALSE_SHARING_SECURITY)];
} LockWrapper;

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


void mixed_read_write_benchmark_writer(void * x){
    unsigned short * xsubi = myXsubi;
    for(int i = 0; i < imsw.iterationsSpentInWriteCriticalSection; i++){
        WORK_IN_WRITE_CRITICAL_SECTION_1;
    }
}

int globalDummy = 0; //To avoid that the compiler optimize away read 

void *mixed_read_write_benchmark_thread(void *lock_pointer){
    LOCK_REGISTER_THIS_THREAD();
    LOCK_DATATYPE_NAME * lock = lock_pointer;
    int privateArray[NUMBER_OF_ELEMENTS_IN_ARRAYS];
    unsigned short xsubi[3];
    myXsubi = xsubi;
    xsubi[0] = pthread_self();
    xsubi[2] = pthread_self();
    int dummy = 0;//To avoid that the compiler optimize away read
    int totalNumberOfCriticalSections = 0;
    //START LINE
    while(!ACCESS_ONCE(*benchmarkStarted)){
        __sync_synchronize();
    }
    while(!ACCESS_ONCE(*benchmarkStoped)){
        if(erand48(xsubi) > imsw.percentageRead){
            LOCK_WRITE(lock, (void *)1);
        }else{
            LOCK_READ_LOCK(lock);
            for(int u = 0; u < imsw.iterationsSpentInReadCriticalSection; u++){
                WORK_IN_READ_CRITICAL_SECTION_1;
            }
            LOCK_READ_UNLOCK(lock);
        }
        for(int u = 0; u < imsw.iterationsSpentInNonCriticalSection; u++){
            WORK_IN_NON_CRITICAL_SECTION_1;
        }
        totalNumberOfCriticalSections++;
    }

    globalDummy = dummy + privateArray[0];

    return (void*)((long)totalNumberOfCriticalSections);
}

//================
//Benchmark Runner
//================


double benchmark_parallel_mixed_read_write(double percentageReadParam, 
                                           int numberOfThreads,
                                           int benchmarkTimeSeconds,
                                           int iterationsSpentInWriteCriticalSectionParam,
                                           int iterationsSpentInReadCriticalSectionParam,
                                           int iterationsSpentInNonCriticalSectionParam){
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
    for(int i = 0; i < numberOfThreads; i ++){
        pthread_create(&threads[i],NULL,&mixed_read_write_benchmark_thread,lock);
    }
    usleep(1000000);//To make sure all threads are set up
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
        //printf("THREAD NUMBER OF OPS: %ld\n", threadNumberOfOperations);
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
    //printf("BENCHMARK REAL TIME: %f\n", ((double)benchmarRealTime)/1000000.0);
    //printf("BENCHMARK TOTAL NUMBER OF OPS: %ld\n", totalNumberOfOperations);

    double timePerOp = ((double)benchmarRealTime)/((double)totalNumberOfOperations);
    return timePerOp;
}


void run_scaling_benchmark(int numOfThreadsArraySize,
                           int* numOfThreadsArray,
                           double percentageReadParam,
                           int benchmarkTimeSeconds,
                           int iterationsSpentInWriteCriticalSectionParam,
                           int iterationsSpentInReadCriticalSectionParam,
                           int iterationsSpentInNonCriticalSectionParam,
                           char benchmark_identifier[]){

    char * file_name_buffer = malloc(4096);
    char file_name_format [] = "%s.dat";

    sprintf(file_name_buffer, file_name_format, benchmark_identifier);
    FILE *benchmark_file = fopen(file_name_buffer, "w");

    free(file_name_buffer);

    fprintf(stderr, "\n\n\033[32m -- STARTING BENCHMARKS FOR %s! -- \033[m\n\n", benchmark_identifier);
            
    for(int i = 0 ; i < numOfThreadsArraySize; i++){
        int numOfThreads = numOfThreadsArray[i];
        printf("=> Benchmark %d threads\n", numOfThreads);
        double time = benchmark_parallel_mixed_read_write(percentageReadParam, 
                                                          numOfThreads,
                                                          benchmarkTimeSeconds,
                                                          iterationsSpentInWriteCriticalSectionParam,
                                                          iterationsSpentInReadCriticalSectionParam,
                                                          iterationsSpentInNonCriticalSectionParam);
        fprintf(benchmark_file, "%d %f\n", numOfThreads, time);
        printf("|| %f microseconds/operation (%d threads)\n", time, numOfThreads);
    }

    fclose(benchmark_file);

    printf("\n\n\033[32m -- BENCHMARKS FOR %s COMPLETED! -- \033[m\n\n", benchmark_identifier);



}


int main(int argc, char **argv){
    int numberOfStandardArgs = 7;
    if(argc < (numberOfStandardArgs + 1)){
        printf("The benchmark requires the following paramters:\n");
        printf("\n");
        printf("benchmarkIdentifier\n");
        printf("percentageRead\n");
        printf("numberOfSecondsToBenchmark\n");
        printf("iterationsSpentInWriteCriticalSection\n");
        printf("iterationsSpentInReadCriticalSection\n");
        printf("iterationsSpentInNonCriticalSection\n");
        printf("List of number of threads to test on\n");
        printf("\n");
        printf("Example\n");
        printf("./bin/rw_bench_clone myBench 0.8 10 4 4 64 1 2 3 4\n");
    }else{
        char * benchmarkIdentifier = argv[1];
        double percentageRead = atof(argv[2]);
        int numberOfSecondsToBenchmark = atoi(argv[3]);
        int iterationsSpentInWriteCriticalSection = atoi(argv[4]);
        int iterationsSpentInReadCriticalSection = atoi(argv[5]);
        int iterationsSpentInNonCriticalSection = atoi(argv[6]);

        int numOfThreadsArraySize = argc - numberOfStandardArgs;
        int numOfThreadsArray[numOfThreadsArraySize];
        for (int i = numberOfStandardArgs; i < argc; i++) {
            numOfThreadsArray[i-numberOfStandardArgs] = atoi(argv[i]);
        }
        run_scaling_benchmark(numOfThreadsArraySize,
                              numOfThreadsArray,
                              percentageRead,
                              numberOfSecondsToBenchmark,
                              iterationsSpentInWriteCriticalSection,
                              iterationsSpentInReadCriticalSection,
                              iterationsSpentInNonCriticalSection,
                              benchmarkIdentifier);   
    }
    exit(0);                          
}
