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
    'pairing_heap_bench',
    #locks to benchmark (comma separated list)
    'qdlock,hqdlock,ccsynch,flatcomb,clh',
    #use pinning to NUMA nodes (comma separated list)
    'no',
    #Benchmark number of threads (comma separated list)
    '1,2,3,4,5,6,7,8',
    #Percentage dequeue (comma separated list)
    '0.5',
    #Seconds to run the benchmark (comma separated list)
    '1',
    #Number of work items performed in write-critical section (comma
    #separated list)
    '2',
    #Number of work items performed in read-critical section (comma
    #separated list)
    '0',
    #Number of work items performed in non-critical section (comma
    #separated list)
    '0,32,64']

process = subprocess.Popen(command)
process.wait()
