#!/usr/bin/python

import sys
import os
import subprocess

bin_dir_path = os.path.dirname(os.path.realpath(__file__))

command = [
    os.path.join(bin_dir_path, 'benchmark_lock.py'),
    #number of iterations
    '5',
    #Output dir (standard means a dir in bench_result based on the git
    #commit id and the date)
    'standard', 
    #benchmark prefixes (comma separated list)
    'rw_bench_clone',
    #locks to benchmark (comma separated list)
    'aer_rgnzi,drmcs_rgnzi',
    #use pinning to NUMA nodes (comma separated list)
    'no',
    #Benchmark number of threads (comma separated list)
    '1,2,3,4,5,6',
    #Procentage reads (comma separated list)
    '0.0,0.25,0.5,0.8,0.9,0.95,0.99,1.0',
    #Seconds to run the benchmark (comma separated list)
    '1',
    #Numper of work items performed in write-critical seciton (comma
    #separated list)
    '4',
    #Numper of work items performed in read-critical seciton (comma
    #separated list)
    '4',
    #Numper of work items performed in non-critical seciton (comma
    #separated list)
    '0,64']

process = subprocess.Popen(command)
process.wait()
