#set term x11
#set output
#set term post eps
#set output 'benchmark.eps'
set terminal png
set output output_filename
set title "Benchmark"
set xlabel "Number of threads"
set ylabel "Microseconds/operation"
set yrange [0 : *]
plot input_filename using 1:2 title the_title with linespoints