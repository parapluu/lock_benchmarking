#ifndef __BENCHMARK_FUNCTIONS_H__
#define __BENCHMARK_FUNCTIONS_H__

void run_scaling_benchmark(int numOfThreadsArraySize,
                           int* numOfThreadsArray,
                           double percentageReadParam,
                           long numberOfOperationsPerThreadParam,
                           int iterationsSpentInWriteCriticalSectionParam,
                           int iterationsSpentInReadCriticalSectionParam,
                           char benchmark_identifier[]);

#endif

