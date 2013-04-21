#!/bin/sh

./src/benchmark/run_all_access_skiplist_benchmarks.sh $@

./src/benchmark/run_all_do_nothing_benchmarks.sh $@
