Mutual Exclusion Locks and Benchmarks 
=====================================

This repository contains lock implementations, delegation lock
implementations and a set of benchmarks.

## Lock Implementations ##

### QD Lock ###

**Main file:** src/datastructures_bench/synch_algorithms/qdlock.h

**Lock Description:** See queue delegation locking paper 

### HQD Lock ###

**Main file:** src/datastructures_bench/synch_algorithms/hqdlock.h

**Lock Description:** See queue delegation locking paper 

### Flat Combining ###

**Main file:** src/datastructures_bench/synch_algorithms/flat_comb.h

**Lock Description:**: Flat combining is described in
  ["Flat combining and the synchronization-parallelism tradeoff"][FCPaper].
  The flat combining implementation is based on the C++ source
  [code provided by the authors of flat combining.][FCSource]

[FCPaper]: http://dl.acm.org/citation.cfm?id=1810479.1810540
[FCSource]: https://github.com/mit-carbon/Flat-Combining

### CC-Synch and H-Synch ###

**Main files:** src/datastructures_bench/synch_algorithms/hsynch.h and
  src/datastructures_bench/synch_algorithms/ccsynch.h

**Lock Description:** CC-Synch and H-Synch is described in
  ["Revisiting the combining synchronization technique"][SynchPaper].
  The CC-Synch and H-Synch implementations are taken from the source
  [code provided by the authors][SynchSource] of CC-Synch and
  H-Synch.

[SynchPaper]: http://dl.acm.org/citation.cfm?id=2145849
[SynchSource]: https://code.google.com/p/sim-universal-construction/

### Cohort Lock ###

**Main files:** src/lock/cohort_lock.{c, h }

**Lock Description:** The cohort lock is described in
  ["Lock cohorting: a general technique for designing NUMA locks"][CohortPaper].

[CohortPaper]: http://dl.acm.org/citation.cfm?id=2145848

### MR-QD and MR-HQD locks ###

**Main files:** src/lock/agnostic_rdx_lock.{c, h}, src/lock/rhqd_lock.{c, h}

**Lock Description:** Queue delegation locking paper

### Write-Preference Reader-Writer locks ###

**Main files:** src/lock/wprw_lock.{c, h} 

**Lock Description:**: Write-preference reader writer locks (DR-MCS
  and Cohort based) are both described in
  ["NUMA-aware reader-writer locks"][NumaRWPaper].

[NumaRWPaper]: http://dl.acm.org/citation.cfm?id=2442532

### Other Locks ###

Other locks that can be found in the repository are TATAS, MCS, CLH,
ticket-lock, partitioned ticket lock, etc.

## Requirements ##

* [GCC](http://gcc.gnu.org/) for compilation of the code (tested with gcc version 4.7.2)
* [SCons](http://www.scons.org/) for the build system (tested with scons version 2.1.0)

The code has so far been tested only on x86-64 systems running Linux.
It may not work on other platforms without modification.

* python and matplotlib for producing graphs from the gathered data

## How to Build ##

`scons`

Builds standard lock object files and benchmarks.

`scons --use_pinning`

Builds RWBench with thread pinning to hardware threads. The tests for
cohort lock (`test_cohort`), cohort based write-preference RW
(`test_wprwcohort_rgnzi`) and hierarchical multi-reader QD
(`test_rhqdlock_rgnzi`) should not be used when `--use_pinning` is
enabled. The reason for this is that when pinning is enabled, the NUMA
aware locks rely on the threads staying on the same node all the time,
but pinning is not enabled in the tests. To test these locks, first
compile without `--use_pinning`.

`scons --use_queue_stats`

With this option set, the data structure benchmark will produce
statistics about how many operations have been helped during
every help session.

`scons --use_print_thread_queue_stats`

This option together with the `--use_queue_stats` option will cause
the data structure benchmark to print per thread statistics. Using
this is not recommended for measurements, but it can be helpful for
debugging.

## Tests ##

For example `./bin/test_tatasdx` will run tests for the QD lock.

## Benchmarks ##

### Data Structure Benchmark ###

The data structure benchmark is described in the queue delegation
locking paper. It measures the throughput for a concurrent priority
queue implemented by protecting a pairing heap with QD locking,
CC-Synch, H-Synch, flat combining etc. The benchmark is implemented in
`src/benchmark/datastructures_bench.c`. It has several parameters that
can be used to change, for example, the amount of thread local
work. Note that the benchmark pins threads to hardware threads and if
the pinning does not work it might give unpredictable results,
especially for the NUMA-aware locks.

**An example:**

    $ ./bin/pairing_heap_bench_qdlock  8 0.5 10 1 0 0
    => Benchmark 8 threads
    8 10014853 26388210
    || 10014853 microseconds, 26388210 operations, (8 threads)

Run `./bin/pairing_heap_bench_qdlock` for a description of the
parameters.


### RWBench ###

The RWBench benchmark is described in "NUMA-aware reader-writer locks"
paper and in the queue delegation locking paper. It measure the
throughput of read and write critical sections performed by a number
of threads. The benchmark is implemented in the file
`src/datastructures_bench/rw_bench_clone.c`.  Note that the benchmark
pins threads to hardware threads when the compile option
`--use_pinning` has been specified, and if the pinning does not work it
might give unpredictable results, especially for the NUMA-aware locks.

**An example:**

    $ ./bin/rw_bench_clone_wprwcohort_rgnzi  8 0.8 10 4 4 32
    => Benchmark 8 threads
    8 10000104 64207442
    || 10000104 microseconds, 64207442 operations (8 threads)


Run `./bin/rw_bench_clone_wprwcohort_rgnzi` for a description of the
parameters.

### Run Benchmarks ###

The script `bin/benchmark_lock.py` can be used to run the benchmark
with different parameters. See the content of the file
`bin/run_benchmarks_on_intel_i7.py` for an explanation of the
parameters.  To run the pairing heap benchmarks on an 8-thread Intel
i7 CPU, you can run the command:

`bin/run_benchmarks_on_intel_i7.py`

The file `bin/benchmark_lock_XNonCW.py` is similar to
`bin/benchmark_lock.py` but is used to generate data for the graphs
where the x-axis shows the amount of thread-local work between the
operations.

### Produce Benchmark Graphs ###

The script `./bin/compare_benchmarks.py` can be used to produce
graphs. For example if you have run
`bin/run_benchmarks_on_intel_i7.py` without any modifications the
following can be used to produce graphs for the benchmark:

    cd bench_results
    ../bin/compare_benchmarks.py gen_graphs.py *

This will produce a couple a graphs in PNG and PDF formats and a file
called `gen_graphs.py` that can be changed to change the appearance of
the graphs.

