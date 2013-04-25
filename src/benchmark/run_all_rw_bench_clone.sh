#!/bin/sh

GIT_COMMIT=`date +'%y.%m.%d_%H.%M.%S'`_`git rev-parse HEAD`

THREADS=$@

mkdir benchmark_results/$GIT_COMMIT

for VAR in 1.0 0.99 0.95 0.9 0.8 0.5 0.0
do
    ID="benchmark_results/$GIT_COMMIT/rw_bench_clone_benchmark_($VAR)"
    ./src/benchmark/rw_bench_clone_benchmark.sh $ID $VAR 5 4 4 64 $THREADS
done
