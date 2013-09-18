#!/usr/bin/python

import sys
import os
import subprocess

bin_dir_path = os.path.dirname(os.path.realpath(__file__))

command = [
    os.path.join(bin_dir_path, 'benchmark_lock.py'),
    #number of iterations
    '5',
    #Output dir (standard menas a dir in bench_result based on the git
    #commit id and the date)
    'standard',
    #benchmark prefixes (comma separated list)
    'rw_bench_clone',
    #locks to benchmark (comma separated list)
    'aer_rgnzi,drmcs_rgnzi,cohort,wprwcohort_rgnzi',
    #use pinning to NUMA nodes (comma separated list)
    'no,yes',
    #Benchmark number of threads (comma separated list)
    '1,2,4,8,12,16,24,32,48,62,64',
    #Procentage reads (comma separated list)
    '0.0,0.25,0.5,0.8,0.9,0.95,0.99,1.0',
    #Seconds to run the benchmark (comma separated list)
    '1',
    #Number of work items performed in write-critical section (comma
    #separated list)
    '4',
    #Number of work items performed in read-critical section (comma
    #separated list)
    '4',
    #Number of work items performed in non-critical section (comma
    #separated list)
    '0,64']

process = subprocess.Popen(command)
process.wait()
