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

output_dir_or_file = sys.argv.pop(0)

compare_dirs = sys.argv

dat_files = [f for f in listdir(compare_dirs[0]) if f.endswith(".dat")]

output_file = output_dir_or_file
copy(join(dirname(__file__), '../src/benchmark/produce_graphs_template.py'), output_file)
with open(output_file, "a") as f:
	for in_file_name in dat_files:
	    f.write('set_up_figure("%s")\n' % in_file_name)
	    for in_dir in compare_dirs:
		f.write('plot_file("%s", "%s")\n' % (join(in_dir,in_file_name), in_dir.split('#')[2]))
	    f.write('complete_figure("%s")\n' % in_file_name)
	    f.write("\n")


FORMAT='png'
execfile(output_file)
