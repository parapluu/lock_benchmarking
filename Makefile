NUMER_OF_READER_GROUPS=8

GIT_COMMIT = `date +'%y.%m.%d_%H.%M.%S'`_`git rev-parse HEAD`

BENCHMARK_NUM_OF_THREADS = 1 2 4 8
CC = gcc
CFLAGS = -I. -Isrc/lock -Isrc/datastructures -Isrc/tests -Isrc/utils -Isrc/benchmark/skiplist -O3 -std=gnu99 -Wall -g -pthread -DNUMBER_OF_READER_GROUPS=$(NUMER_OF_READER_GROUPS) 
TEST_MULTI_WRITERS_QUEUE_OBJECTS = bin/multi_writers_queue.o bin/test_multi_writers_queue.o

TEST_LOCK_OBJECTS = bin/multi_writers_queue.o

BENCHMARK_OBJECTS = bin/multi_writers_queue.o bin/simple_delayed_writers_lock.o bin/benchmark_functions.o
BENCHMARK_OBJECTS_ACCESS_SKIPLIST = bin/multi_writers_queue.o bin/simple_delayed_writers_lock.o bin/benchmark_functions_access_skiplist.o bin/skiplist.o

LOCK_SRC_DEPS = src/datastructures/multi_writers_queue.h \
	        src/utils/smp_utils.h \
	        src/lock/common_lock_constants.h

TEST_SRC_DEPS = src/tests/test_framework.h \
	        src/utils/smp_utils.h

RW_BENCH_SRC_DEPS = src/benchmark/rw_bench_clone.c \
	            src/utils/smp_utils.h

LIBS =

all: bin/test_multi_writers_queue bin/test_simple_delayed_writers_lock bin/mixed_ops_benchmark bin/mixed_ops_benchmark_access_skiplist bin/rw_bench_clone_sdw bin/rw_bench_clone_aer bin/test_all_equal_rdx_lock bin/test_mcs_lock bin/rw_bench_clone_mcs bin/test_drmcs_lock bin/rw_bench_clone_drmcs bin/test_ticket_lock bin/rw_bench_clone_ticket bin/test_aticket_lock bin/rw_bench_clone_aticket

#Executables

bin/test_multi_writers_queue: $(TEST_MULTI_WRITERS_QUEUE_OBJECTS)
	$(CC) -o bin/test_multi_writers_queue $(TEST_MULTI_WRITERS_QUEUE_OBJECTS) $(LIBS) $(CFLAGS)

bin/test_simple_delayed_writers_lock: $(TEST_LOCK_OBJECTS) bin/test_simple_delayed_writers_lock.o bin/simple_delayed_writers_lock.o
	$(CC) -o bin/test_simple_delayed_writers_lock $(TEST_LOCK_OBJECTS) bin/test_simple_delayed_writers_lock.o bin/simple_delayed_writers_lock.o $(LIBS) $(CFLAGS)

bin/test_all_equal_rdx_lock: $(TEST_LOCK_OBJECTS) bin/test_all_equal_rdx_lock.o bin/all_equal_rdx_lock.o
	$(CC) -o bin/test_all_equal_rdx_lock $(TEST_LOCK_OBJECTS) bin/test_all_equal_rdx_lock.o bin/all_equal_rdx_lock.o $(LIBS) $(CFLAGS)

bin/test_mcs_lock: $(TEST_LOCK_OBJECTS) bin/test_mcs_lock.o bin/mcs_lock.o
	$(CC) -o bin/test_mcs_lock $(TEST_LOCK_OBJECTS) bin/test_mcs_lock.o bin/mcs_lock.o $(LIBS) $(CFLAGS)

bin/test_drmcs_lock: $(TEST_LOCK_OBJECTS) bin/test_drmcs_lock.o bin/mcs_lock.o bin/drmcs_lock.o
	$(CC) -o bin/test_drmcs_lock $(TEST_LOCK_OBJECTS) bin/test_drmcs_lock.o bin/drmcs_lock.o bin/mcs_lock.o $(LIBS) $(CFLAGS)

bin/test_ticket_lock: $(TEST_LOCK_OBJECTS) bin/test_ticket_lock.o bin/ticket_lock.o bin/ticket_lock.o
	$(CC) -o bin/test_ticket_lock $(TEST_LOCK_OBJECTS) bin/test_ticket_lock.o bin/ticket_lock.o$(LIBS) $(CFLAGS)

bin/test_aticket_lock: $(TEST_LOCK_OBJECTS) bin/test_aticket_lock.o bin/aticket_lock.o bin/aticket_lock.o
	$(CC) -o bin/test_aticket_lock $(TEST_LOCK_OBJECTS) bin/test_aticket_lock.o bin/aticket_lock.o$(LIBS) $(CFLAGS)

bin/mixed_ops_benchmark: $(BENCHMARK_OBJECTS)  bin/mixed_ops_benchmark.o
	$(CC) -o bin/mixed_ops_benchmark $(BENCHMARK_OBJECTS) bin/mixed_ops_benchmark.o $(LIBS) $(CFLAGS)

bin/rw_bench_clone_sdw: bin/multi_writers_queue.o bin/simple_delayed_writers_lock.o  bin/rw_bench_clone_sdw.o
	$(CC) -o bin/rw_bench_clone_sdw bin/multi_writers_queue.o bin/simple_delayed_writers_lock.o bin/rw_bench_clone_sdw.o $(LIBS) $(CFLAGS)

bin/rw_bench_clone_aer: bin/multi_writers_queue.o bin/all_equal_rdx_lock.o  bin/rw_bench_clone_aer.o
	$(CC) -o bin/rw_bench_clone_aer bin/multi_writers_queue.o bin/all_equal_rdx_lock.o bin/rw_bench_clone_aer.o $(LIBS) $(CFLAGS)

bin/rw_bench_clone_mcs: bin/mcs_lock.o  bin/rw_bench_clone_mcs.o
	$(CC) -o bin/rw_bench_clone_mcs bin/multi_writers_queue.o bin/mcs_lock.o bin/rw_bench_clone_mcs.o $(LIBS) $(CFLAGS)

bin/rw_bench_clone_drmcs: bin/drmcs_lock.o bin/mcs_lock.o  bin/rw_bench_clone_drmcs.o
	$(CC) -o bin/rw_bench_clone_drmcs  bin/mcs_lock.o bin/drmcs_lock.o bin/rw_bench_clone_drmcs.o $(LIBS) $(CFLAGS)

bin/rw_bench_clone_ticket: bin/ticket_lock.o bin/rw_bench_clone_ticket.o
	$(CC) -o bin/rw_bench_clone_ticket bin/ticket_lock.o bin/rw_bench_clone_ticket.o $(LIBS) $(CFLAGS)

bin/rw_bench_clone_aticket: bin/aticket_lock.o bin/rw_bench_clone_aticket.o
	$(CC) -o bin/rw_bench_clone_aticket bin/aticket_lock.o bin/rw_bench_clone_aticket.o $(LIBS) $(CFLAGS)

bin/mixed_ops_benchmark_access_skiplist: $(BENCHMARK_OBJECTS_ACCESS_SKIPLIST)  bin/mixed_ops_benchmark.o
	$(CC) -o bin/mixed_ops_benchmark_access_skiplist $(BENCHMARK_OBJECTS_ACCESS_SKIPLIST) bin/mixed_ops_benchmark.o $(LIBS) $(CFLAGS)

#Objects

bin/test_simple_delayed_writers_lock.o: $(TEST_SRC_DEPS) src/tests/test_rdx_lock.c
	$(CC) $(CFLAGS) -DLOCK_TYPE_SimpleDelayedWritesLock -c src/tests/test_rdx_lock.c ; \
	mv test_rdx_lock.o bin/test_simple_delayed_writers_lock.o

bin/test_all_equal_rdx_lock.o: $(TEST_SRC_DEPS) src/tests/test_rdx_lock.c
	$(CC) $(CFLAGS) -DLOCK_TYPE_AllEqualRDXLock -c src/tests/test_rdx_lock.c ; \
	mv test_rdx_lock.o bin/test_all_equal_rdx_lock.o

bin/test_mcs_lock.o: $(TEST_SRC_DEPS) src/tests/test_rdx_lock.c
	$(CC) $(CFLAGS) -DLOCK_TYPE_MCSLock -c src/tests/test_rdx_lock.c ; \
	mv test_rdx_lock.o bin/test_mcs_lock.o

bin/test_drmcs_lock.o: $(TEST_SRC_DEPS) src/tests/test_rdx_lock.c
	$(CC) $(CFLAGS) -DLOCK_TYPE_DRMCSLock -c src/tests/test_rdx_lock.c ; \
	mv test_rdx_lock.o bin/test_drmcs_lock.o

bin/test_ticket_lock.o: $(TEST_SRC_DEPS) src/tests/test_rdx_lock.c
	$(CC) $(CFLAGS) -DLOCK_TYPE_TicketLock -c src/tests/test_rdx_lock.c ; \
	mv test_rdx_lock.o bin/test_ticket_lock.o

bin/test_aticket_lock.o: $(TEST_SRC_DEPS) src/tests/test_rdx_lock.c
	$(CC) $(CFLAGS) -DLOCK_TYPE_ATicketLock -c src/tests/test_rdx_lock.c ; \
	mv test_rdx_lock.o bin/test_aticket_lock.o

bin/test_multi_writers_queue.o: $(TEST_SRC_DEPS) src/tests/test_multi_writers_queue.c
	$(CC) $(CFLAGS) -c src/tests/test_multi_writers_queue.c ; \
	mv *.o bin/

bin/benchmark_functions.o: src/benchmark/benchmark_functions.c src/benchmark/benchmark_functions.h src/lock/simple_delayed_writers_lock.h src/utils/smp_utils.h
	$(CC) $(CFLAGS) -c src/benchmark/benchmark_functions.c ; \
	mv *.o bin/

bin/rw_bench_clone_sdw.o: $(RW_BENCH_SRC_DEPS) src/lock/simple_delayed_writers_lock.h
	$(CC) $(CFLAGS) -DLOCK_TYPE_SimpleDelayedWritesLock -c src/benchmark/rw_bench_clone.c ; \
	mv rw_bench_clone.o bin/rw_bench_clone_sdw.o

bin/rw_bench_clone_aer.o: $(RW_BENCH_SRC_DEPS) src/lock/all_equal_rdx_lock.h
	$(CC) $(CFLAGS) -DLOCK_TYPE_AllEqualRDXLock -c src/benchmark/rw_bench_clone.c ; \
	mv rw_bench_clone.o bin/rw_bench_clone_aer.o

bin/rw_bench_clone_mcs.o: $(RW_BENCH_SRC_DEPS) src/lock/mcs_lock.h
	$(CC) $(CFLAGS) -DLOCK_TYPE_MCSLock -c src/benchmark/rw_bench_clone.c ; \
	mv rw_bench_clone.o bin/rw_bench_clone_mcs.o

bin/rw_bench_clone_drmcs.o: $(RW_BENCH_SRC_DEPS) src/lock/drmcs_lock.h
	$(CC) $(CFLAGS) -DLOCK_TYPE_DRMCSLock -c src/benchmark/rw_bench_clone.c ; \
	mv rw_bench_clone.o bin/rw_bench_clone_drmcs.o

bin/rw_bench_clone_ticket.o: $(RW_BENCH_SRC_DEPS) src/lock/ticket_lock.h
	$(CC) $(CFLAGS) -DLOCK_TYPE_TicketLock -c src/benchmark/rw_bench_clone.c ; \
	mv rw_bench_clone.o bin/rw_bench_clone_ticket.o

bin/rw_bench_clone_aticket.o: $(RW_BENCH_SRC_DEPS) src/lock/aticket_lock.h
	$(CC) $(CFLAGS) -DLOCK_TYPE_ATicketLock -c src/benchmark/rw_bench_clone.c ; \
	mv rw_bench_clone.o bin/rw_bench_clone_aticket.o

bin/benchmark_functions_access_skiplist.o: src/benchmark/benchmark_functions.c src/benchmark/benchmark_functions.h src/lock/simple_delayed_writers_lock.h src/utils/smp_utils.h
	$(CC) $(CFLAGS) -DACCESS_SKIPLIST -c src/benchmark/benchmark_functions.c ; \
	mv benchmark_functions.o bin/benchmark_functions_access_skiplist.o

bin/mixed_ops_benchmark.o: src/benchmark/benchmark_functions.c src/benchmark/benchmark_functions.h src/benchmark/mixed_ops_benchmark.c
	$(CC) $(CFLAGS) -c src/benchmark/mixed_ops_benchmark.c ; \
	mv *.o bin/

bin/simple_delayed_writers_lock.o: $(LOCK_SRC_DEPS) src/lock/simple_delayed_writers_lock.c src/lock/simple_delayed_writers_lock.h 
	$(CC) $(CFLAGS) -c src/lock/simple_delayed_writers_lock.c ; \
	mv *.o bin/

bin/all_equal_rdx_lock.o: $(LOCK_SRC_DEPS) src/lock/all_equal_rdx_lock.c src/lock/all_equal_rdx_lock.h
	$(CC) $(CFLAGS) -c src/lock/all_equal_rdx_lock.c ; \
	mv *.o bin/

bin/mcs_lock.o: $(LOCK_SRC_DEPS) src/lock/mcs_lock.c src/lock/mcs_lock.h
	$(CC) $(CFLAGS) -c src/lock/mcs_lock.c ; \
	mv *.o bin/

bin/drmcs_lock.o: $(LOCK_SRC_DEPS) src/lock/drmcs_lock.c src/lock/drmcs_lock.h
	$(CC) $(CFLAGS) -c src/lock/drmcs_lock.c ; \
	mv *.o bin/

bin/ticket_lock.o: $(LOCK_SRC_DEPS) src/lock/ticket_lock.c src/lock/ticket_lock.h
	$(CC) $(CFLAGS) -c src/lock/ticket_lock.c ; \
	mv *.o bin/

bin/aticket_lock.o: $(LOCK_SRC_DEPS) src/lock/aticket_lock.c src/lock/aticket_lock.h
	$(CC) $(CFLAGS) -c src/lock/aticket_lock.c ; \
	mv *.o bin/

bin/multi_writers_queue.o: src/datastructures/multi_writers_queue.c src/datastructures/multi_writers_queue.h src/utils/smp_utils.h
	$(CC) $(CFLAGS) -c src/datastructures/multi_writers_queue.c ; \
	mv *.o bin/

bin/skiplist.o: src/benchmark/skiplist/skiplist.c src/benchmark/skiplist/skiplist.h src/benchmark/skiplist/kvset.h
	$(CC) $(CFLAGS) -c src/benchmark/skiplist/skiplist.c ; \
	mv *.o bin/

##Test tasks

run_test_multi_writers_queue: bin/test_multi_writers_queue
	valgrind --leak-check=yes ./bin/test_multi_writers_queue

run_test_simple_delayed_writers_lock: bin/test_simple_delayed_writers_lock
	./bin/test_simple_delayed_writers_lock

run_test_simple_delayed_writers_lock_valgrind: bin/test_simple_delayed_writers_lock
	valgrind --leak-check=yes ./bin/test_simple_delayed_writers_lock

run_test_all_equal_rdx_lock: bin/test_all_equal_rdx_lock
	./bin/test_all_equal_rdx_lock

run_test_all_equal_rdx_lock_valgrind: bin/test_all_equal_rdx_lock
	valgrind --leak-check=yes ./bin/test_all_equal_rdx_lock


#Benchmark tasks

run_all_benchmarks: bin/mixed_ops_benchmark bin/mixed_ops_benchmark_access_skiplist
	./src/benchmark/run_all_benchmarks.sh $(BENCHMARK_NUM_OF_THREADS) 

run_all_rw_bench_sdw_benchmarks: bin/rw_bench_clone_sdw
	./src/benchmark/run_all_rw_bench_clone.sh sdw $(BENCHMARK_NUM_OF_THREADS)

run_all_rw_bench_aer_benchmarks: bin/rw_bench_clone_aer
	./src/benchmark/run_all_rw_bench_clone.sh aer $(BENCHMARK_NUM_OF_THREADS)

run_all_rw_bench_mcs_benchmarks: bin/rw_bench_clone_mcs
	./src/benchmark/run_all_rw_bench_clone.sh mcs $(BENCHMARK_NUM_OF_THREADS)

run_all_rw_bench_drmcs_benchmarks: bin/rw_bench_clone_drmcs
	./src/benchmark/run_all_rw_bench_clone.sh drmcs $(BENCHMARK_NUM_OF_THREADS)

run_all_rw_bench_ticket_benchmarks: bin/rw_bench_clone_ticket
	./src/benchmark/run_all_rw_bench_clone.sh ticket $(BENCHMARK_NUM_OF_THREADS)

run_all_rw_bench_aticket_benchmarks: bin/rw_bench_clone_aticket
	./src/benchmark/run_all_rw_bench_clone.sh aticket $(BENCHMARK_NUM_OF_THREADS)

run_writes_benchmark: bin/mixed_ops_benchmark bin/simple_delayed_writers_lock.o
	FILE_NAME_PREFIX=benchmark_results/writes_benchmark_$(GIT_COMMIT) ; \
	./src/benchmark/run_mixed_ops_bench.sh $$FILE_NAME_PREFIX 0.0 1000000 1000 1000 $(BENCHMARK_NUM_OF_THREADS)

run_reads_benchmark: bin/mixed_ops_benchmark bin/simple_delayed_writers_lock.o
	FILE_NAME_PREFIX=benchmark_results/reads_benchmark_$(GIT_COMMIT) ; \
	./src/benchmark/run_mixed_ops_bench.sh $$FILE_NAME_PREFIX 1.0 1000000 1000 1000 $(BENCHMARK_NUM_OF_THREADS)

run_80_percent_reads_benchmark: bin/mixed_ops_benchmark bin/simple_delayed_writers_lock.o
	FILE_NAME_PREFIX=benchmark_results/80_percent_reads_benchmark_$(GIT_COMMIT) ; \
	./src/benchmark/run_mixed_ops_bench.sh $$FILE_NAME_PREFIX 0.8 1000000 1000 1000 $(BENCHMARK_NUM_OF_THREADS)


run_writes_access_skiplist_benchmark: bin/mixed_ops_benchmark_access_skiplist bin/simple_delayed_writers_lock.o
	FILE_NAME_PREFIX=benchmark_results/writes_benchmark_access_skiplist_$(GIT_COMMIT) ; \
	./src/benchmark/run_mixed_ops_bench_access_skiplist.sh $$FILE_NAME_PREFIX 0.0 1000000 1 1 $(BENCHMARK_NUM_OF_THREADS)

run_reads_access_skiplist_benchmark: bin/mixed_ops_benchmark_access_skiplist bin/simple_delayed_writers_lock.o
	FILE_NAME_PREFIX=benchmark_results/reads_benchmark_access_skiplist_$(GIT_COMMIT) ; \
	./src/benchmark/run_mixed_ops_bench_access_skiplist.sh $$FILE_NAME_PREFIX 1.0 1000000 1 1 $(BENCHMARK_NUM_OF_THREADS)

run_80_access_skiplist_percent_reads_benchmark: bin/mixed_ops_benchmark_access_skiplist bin/simple_delayed_writers_lock.o
	FILE_NAME_PREFIX=benchmark_results/80_percent_reads_access_skiplist_benchmark_$(GIT_COMMIT) ; \
	./src/benchmark/run_mixed_ops_bench_access_skiplist.sh $$FILE_NAME_PREFIX 0.0 1000000 1 1 $(BENCHMARK_NUM_OF_THREADS)

clean:
	rm -f  bin/*

check-syntax: test_multi_writers_queue
