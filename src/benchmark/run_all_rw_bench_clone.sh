#!/bin/sh

LOCK_EXEC_SUFIX=$1

GIT_COMMIT="$LOCK_EXEC_SUFIX"_`date +'%y.%m.%d_%H.%M.%S'`_`git rev-parse HEAD`

#Remove first argument from list of arguments
shift

THREADS=$@

mkdir benchmark_results/$GIT_COMMIT

for NON_CRITICAL in 0 32 64
do
    for VAR in 1.0 0.99 0.95 0.9 0.8 0.5 0.0
    do
        ID="benchmark_results/$GIT_COMMIT/rw_bench_clone_benchmark_($VAR)_($NON_CRITICAL)"
        ./src/benchmark/rw_bench_clone_benchmark.sh $LOCK_EXEC_SUFIX $ID $VAR 5 4 4 $NON_CRITICAL $THREADS
    done
done
