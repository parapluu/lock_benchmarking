#include <stdio.h>
#include <stdlib.h>
#include "benchmark_functions.h"

int main(int argc, char **argv){
    if(argc < 7){
        printf("The benchmark requires the following paramters:\n");
        printf("\n");
        printf("benchmarkIdentifier\n");
        printf("percentageRead\n");
        printf("numberOfOperationsPerThread\n");
        printf("iterationsSpentInWriteCriticalSection\n");
        printf("iterationsSpentInReadCriticalSection\n");
        printf("List of number of threads to test on\n");
        printf("\n");
        printf("Example\n");
        printf("./bin/mixed_ops_benchmark myBench 0.8 10000000 1000 1000 1 2 3 4\n");
        exit(0);
    }else{
        char * benchmarkIdentifier = argv[1];
        double percentageRead = atof(argv[2]);
        int numberOfOperationsPerThread = atoi(argv[3]);
        int iterationsSpentInWriteCriticalSection = atoi(argv[4]);
        int iterationsSpentInReadCriticalSection = atoi(argv[5]);

        int numOfThreadsArraySize = argc - 6;
        int numOfThreadsArray[numOfThreadsArraySize];
        for (int i = 2; i < argc; i++) {
            numOfThreadsArray[i-6] = atoi(argv[i]);
        }
        run_scaling_benchmark(numOfThreadsArraySize,
                              numOfThreadsArray,
                              percentageRead,
                              numberOfOperationsPerThread,
                              iterationsSpentInWriteCriticalSection,
                              iterationsSpentInReadCriticalSection,
                              benchmarkIdentifier);   

                          
        exit(0);
    }
}
