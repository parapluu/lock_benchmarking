#!/bin/sh

FILE_NAME_PREFIX=$1
./bin/mixed_ops_benchmark_access_skiplist $@
gnuplot -e "output_filename='$FILE_NAME_PREFIX.png'" -e "input_filename='$FILE_NAME_PREFIX.dat'" -e "the_title='$FILE_NAME_PREFIX'" src/benchmark/plot_bench.gp
