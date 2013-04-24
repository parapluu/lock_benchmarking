#!/bin/sh

GIT_COMMIT=`date +'%y.%m.%d_%H.%M.%S'`_`git rev-parse HEAD`

THREADS=$@

for VAR in 1.0 0.99 0.95 0.9 0.8 0.0
do
    ID="benchmark_results/mixed_access_skiplist_benchmark_($VAR)_$GIT_COMMIT"
    echo $ID
    #valgrind --leak-check=yes --show-reachable=yes 
    ./src/benchmark/run_mixed_ops_bench_access_skiplist.sh $ID $VAR 10000000 1 1 $THREADS
done
