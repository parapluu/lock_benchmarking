#!/bin/sh

LOCK_EXEC_SUFIX=$1

#Remove first argument from list of arguments
shift

echo "./bin/rw_bench_clone_$LOCK_EXEC_SUFIX"

FILE_NAME_PREFIX=$1
exec "./bin/rw_bench_clone_$LOCK_EXEC_SUFIX" $@
gnuplot -e "output_filename='$FILE_NAME_PREFIX.png'" -e "input_filename='$FILE_NAME_PREFIX.dat'" -e "the_title='$FILE_NAME_PREFIX'" src/benchmark/plot_bench.gp
