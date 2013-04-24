#include "benchmark_functions.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include "simple_delayed_writers_lock.h"
#include "smp_utils.h"
#include "skiplist.h"


#define NUMBER_OF_ELEMENTS_IN_DATASTRUCTURE 256

#ifdef ACCESS_SKIPLIST
KVSet * skiplist;
unsigned short write_d_rand_seed_wrapper[512];
unsigned short * write_d_rand_seed = &write_d_rand_seed_wrapper[256];
#endif

long numberOfOperationsPerThread = 10000;
int numberOfThreads = 8;
int iterationsSpentInWriteCriticalSection = 0;
int iterationsSpentInReadCriticalSection = 0;
double percentageRead = 0.8;

bool benchmarkStarted = false;

SimpleDelayedWritesLock lock;

typedef struct LockInfoImpl {
    char pad1[128];
    unsigned short xsubi[64];
    char pad2[64];
    SimpleDelayedWritesLock *lock;
    char pad3[128];
} LockInfo;


void mixed_read_write_benchmark_writer(void * x){
    for(int i = 0; i < iterationsSpentInWriteCriticalSection; i++){
#ifdef ACCESS_SKIPLIST
        void * writeValue = (void*)(((long)erand48(write_d_rand_seed)) * NUMBER_OF_ELEMENTS_IN_DATASTRUCTURE + 1);

        skiplist->funs.put(skiplist, writeValue);
#else
        barrier();
#endif
    }
}


void *mixed_read_write_benchmark_thread(void *x){
    sdwlock_register_this_thread();
    LockInfo * li = (LockInfo *) x;
    SimpleDelayedWritesLock * lock = li->lock;
    unsigned short * xsubi = li->xsubi;
#ifdef ACCESS_SKIPLIST
    void * lookupSum = 0;
#endif
    while(!ACCESS_ONCE(benchmarkStarted)){
        __sync_synchronize();
    }
    for(long i = 0; i < numberOfOperationsPerThread; i++){
        if(erand48(xsubi) > percentageRead){
            sdwlock_write(lock, (void *)1);
        }else{
            sdwlock_read_lock(lock);
            for(int u = 0; u < iterationsSpentInReadCriticalSection; u++){
#ifdef ACCESS_SKIPLIST
                void * readValue = (void*)(((long)erand48(xsubi)) * NUMBER_OF_ELEMENTS_IN_DATASTRUCTURE + 1);
                lookupSum = lookupSum + (long)skiplist->funs.lookup(skiplist, readValue);
#else
                barrier();
#endif
            }
            sdwlock_read_unlock(lock);
        }
    }
    pthread_exit(0);
#ifdef ACCESS_SKIPLIST
    return lookupSum;
#endif
}

double benchmark_parallel_mixed_read_write(double percentageReadParam, 
                                           int numberOfThreadsParam,
                                           long numberOfOperationsPerThreadParam,
                                           int iterationsSpentInWriteCriticalSectionParam,
                                           int iterationsSpentInReadCriticalSectionParam){
#ifdef ACCESS_SKIPLIST
    skiplist = new_skiplist_default();
    write_d_rand_seed[0] = 1;
    write_d_rand_seed[1] = 2;
    write_d_rand_seed[2] = 3;
#endif
    percentageRead = percentageReadParam;
    numberOfThreads = numberOfThreadsParam;
    numberOfOperationsPerThread = numberOfOperationsPerThreadParam;
    iterationsSpentInWriteCriticalSection = iterationsSpentInWriteCriticalSectionParam;
    iterationsSpentInReadCriticalSection = iterationsSpentInReadCriticalSectionParam;
    benchmarkStarted = false;
    LockInfo lock_infos[numberOfThreads];
    pthread_t threads[numberOfThreads];
    struct timeval timeStart;
    struct timeval timeEnd;
    sdwlock_initialize(&lock, &mixed_read_write_benchmark_writer);
    for(int i = 0; i < numberOfThreads; i++){
        LockInfo *lc = &lock_infos[i];
        lc->lock = &lock;
        lc->xsubi[0] = i;
        lc->xsubi[2] = i;
        pthread_create(&threads[i],NULL,&mixed_read_write_benchmark_thread,lc);
    }
    usleep(100000);
    gettimeofday(&timeStart, NULL);
    benchmarkStarted = true;
    __sync_synchronize();

    for(int i = 0; i < numberOfThreads; i++){
        pthread_join(threads[i],NULL);
    }
    gettimeofday(&timeEnd, NULL);

    long benchmarkTime = (timeEnd.tv_sec-timeStart.tv_sec)*1000000 + timeEnd.tv_usec-timeStart.tv_usec;
    double timePerOp = ((double)benchmarkTime)/(((double)numberOfThreads*(double)numberOfOperationsPerThread));

#ifdef ACCESS_SKIPLIST
    skiplist->funs.delete_table(skiplist, NULL, NULL);
#endif

    return timePerOp;
}


void run_scaling_benchmark(int numOfThreadsArraySize,
                           int* numOfThreadsArray,
                           double percentageReadParam,
                           long numberOfOperationsPerThreadParam,
                           int iterationsSpentInWriteCriticalSectionParam,
                           int iterationsSpentInReadCriticalSectionParam,
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
                                                          numberOfOperationsPerThreadParam,
                                                          iterationsSpentInWriteCriticalSectionParam,
                                                          iterationsSpentInReadCriticalSectionParam);
        fprintf(benchmark_file, "%d %f\n", numOfThreads, time);
        printf("|| %f microseconds/operation (%d threads)\n", time, numOfThreads);
    }

    fclose(benchmark_file);

    printf("\n\n\033[32m -- BENCHMARKS FOR %s COMPLETED! -- \033[m\n\n", benchmark_identifier);



}


