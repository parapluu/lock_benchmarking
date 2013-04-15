Read-Delegate-Exclusive (RDX) Lock 
==================================

This repository contains a C implementation of the RDX lock as well as
tests and benchmarks.

## Description ##

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
* gnu make

(The code has only been tested on Linux but should work with other
operating systems)


## Build ##

`make`

## Tests ##

* `make run_test_multi_writers_queue`
* `make run_test_simple_delayed_writers_lock`

## Benchmarks ##

* `make run_writes_benchmark`
* `make run_reads_benchmark`
* `make run_80_percent_reads_benchmark`

## Usage ##

See tests under the directory `tests/` and the API in
`lock/simple_delayed_writers_lock.h`.
