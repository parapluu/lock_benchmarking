#!/usr/bin/python

import sys
from os import listdir
from os.path import join
from os import mkdir
from subprocess import Popen
from subprocess import PIPE 
output_dir = sys.argv[1]

sys.argv.pop(0)
sys.argv.pop(0)

compare_dirs = sys.argv

print 'Output dir:', output_dir

print 'Compare dirs:', compare_dirs

dat_files = [f for f in listdir(compare_dirs[0]) if f.endswith(".dat")]

mkdir(output_dir)

for file in dat_files:
    out_file_prefix = join(output_dir, file)
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
    
