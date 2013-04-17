#include <stdio.h>
#include <stdlib.h>
#include "benchmark_functions.h"

int main(int argc, char **argv){
    int numOfThreadsArraySize = argc - 1;
    int numOfThreadsArray[numOfThreadsArraySize];
    for (int i = 1; i < argc; i++) {
        numOfThreadsArray[i-1] = atoi(argv[i]);
    }
    run_scaling_benchmark(numOfThreadsArraySize /*int numOfThreadsArraySize*/,
                          numOfThreadsArray     /*int* numOfThreadsArray*/,
                          0.8                   /*double percentageRead*/,
                          1000000               /*int numberOfOperationsPerThread*/,
                          0                     /*int iterationsSpentInWriteCriticalSection*/,
                          0                     /*int iterationsSpentInReadCriticalSection*/,
                          "80_percent_reads_benchmark"     /*char benchmark_identifier[]*/);


    exit(0);

}
