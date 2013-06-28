#!/usr/bin/python

import sys
from os import listdir
from os.path import join
from os.path import dirname
from os import mkdir
from subprocess import Popen
from subprocess import PIPE 
from shutil import copy
sys.argv.pop(0)

if len(sys.argv) < 2:
    print """Not enough parameters:

Optional flag -matplotlib (must be first)

The first parameter is the output dir or output file if -matplotlib is specified (where the graphs are placed).

The rest of the parameters are benchmark output dirs of benchmark that
shall be compared. The benchmark results are produced by the
bin/benchmark_lock.py script.

"""
    sys.exit()

use_matplotlib_out = False

if sys.argv[0] == '-matplotlib':
    use_matplotlib_out = True
    sys.argv.pop(0)

output_dir_or_file = sys.argv.pop(0)

compare_dirs = sys.argv

dat_files = [f for f in listdir(compare_dirs[0]) if f.endswith(".dat")]

if use_matplotlib_out:
    output_file = output_dir_or_file
    copy(join(dirname(__file__), '../src/benchmark/produce_graphs_template.py'), output_file)
    with open(output_file, "a") as f:
        for in_file_name in dat_files:
            f.write('set_up_figure("%s")\n' % in_file_name)
            for in_dir in compare_dirs:
                f.write('plot_file("%s", "%s")\n' % (join(in_dir,in_file_name), in_dir))
            f.write('complete_figure("%s")\n' % in_file_name)
            f.write("\n")
else:
    mkdir(output_dir_or_file)
    for file in dat_files:
        out_file_prefix = join(output_dir_or_file, file)
        gnuplot_pipe = Popen("gnuplot",stdin = PIPE).stdin
        gnuplot_pipe.write("set terminal png size 1024,768\n")
        gnuplot_pipe.write("set output \"%s.png\"\n" % out_file_prefix)
        gnuplot_pipe.write("set title \"%s\"\n" % file)
        gnuplot_pipe.write("set xlabel \"Number of threads\"\n")
        gnuplot_pipe.write("set ylabel \"Microseconds/operation\"\n")
        gnuplot_pipe.write("set yrange [0 : *]\n")
        dir_nr = 0
        gnuplot_pipe.write("plot ");
        for dir in compare_dirs:
            dir_nr = dir_nr + 1
            gnuplot_pipe.write("\"%s\" using 1:2 title \"%s\" with linespoints" % (join(dir,file),dir))
            if dir_nr == len(compare_dirs):
                gnuplot_pipe.write("\n")
            else:
                gnuplot_pipe.write(", \\\n");
        gnuplot_pipe.close()
        
