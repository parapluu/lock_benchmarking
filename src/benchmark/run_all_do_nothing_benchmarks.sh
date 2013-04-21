#!/bin/sh

GIT_COMMIT=`date +'%y.%m.%d_%H.%M.%S'`_`git rev-parse HEAD`

THREADS=$@

for READS in 1.0 0.99 0.95 0.9 0.8 0.0
do
    for ITERATIONS in 0 100 1000 10000
    do
        ID="benchmark_results/mixed_benchmark_($READS)_($ITERATIONS)_$GIT_COMMIT"
        echo $ID
        ./src/benchmark/run_mixed_ops_bench.sh $ID $READS 10000000 $ITERATIONS $ITERATIONS $THREADS
    done
done
