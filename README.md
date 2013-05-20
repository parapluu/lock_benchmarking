Read-Delegate-Exclusive (RDX) Lock 
==================================

This repository contains C implementations of RDX locks,
[cohort lock](http://dl.acm.org/citation.cfm?id=2145848),
[NUMA-aware reader-writer lock](http://dl.acm.org/citation.cfm?id=2442532),
and MCS lock etc.

## RDX Lock Description ##

The RDX lock can be acquired for three different types of access:

1. ***R*ead**
2. **Write only (*D*elegated)**
3. **Write and read (E*X*lusive)**

Acquiring an RDX lock for reading **R** has the same semantics as
acquiring a traditional
[readers-writer lock](http://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock)
(RW Lock) for reading. Likewise, acquiring the RDX lock for writing
and reading **X** has the same semantics as acquiring an RW lock for
writing. The difference between an RDX lock and a traditional RW lock
is the second type of access listed above: the write only access
**D**. A write or delegate acquisition in an RDX lock will give
information about a write operation that shall be protected by the
lock to the lock. If all the data protected by the lock is read and
written by first acquiring appropriate type of lock access, the user
can think as if the write operation is executed instantly and
atomically. However, the lock might delegate the operation to another
thread as long as the semantics is the same as if the an exclusive
lock was acquired for the operation.


## Requirements ##

* GCC (Tested with gcc version 4.7.2)
* [SCons](http://www.scons.org/) (build system)

(The code has only been tested on Linux but should work with other
operating systems)


## Build ##

`scons`

## Tests ##

`./bin/test_aer`

Where aer can be replaced with another lock id. 

## Benchmarks ##

`./bin/benchmark_lock.py`

The command will give you information about the parameters

## Compare Benchmark Results ##

`./bin/compare_benchmarks.py`

The first parameter is the output dir (where the graphs are placed).

The rest of the parameters are benchmark output dirs of benchmark that
shall be compared. The benchmark results are produced by the
`./bin/benchmark_lock.py` script.

## Usage ##

See tests under the directory `src/tests`, the benchmarks under
directory `src/benchmark` and the API's for the locks in
`lock/*_lock.h`.
