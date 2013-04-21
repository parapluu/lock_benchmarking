
GIT_COMMIT = `date +'%y.%m.%d_%H.%M.%S'`_`git rev-parse HEAD`
BENCHMARK_NUM_OF_THREADS = 1 2
CC = gcc
CFLAGS = -I. -Isrc/lock -Isrc/datastructures -Isrc/tests -Isrc/utils -Isrc/benchmark/skiplist -O3 -std=gnu99 -Wall -g -pthread
TEST_MULTI_WRITERS_QUEUE_OBJECTS = bin/multi_writers_queue.o bin/test_multi_writers_queue.o
TEST_SIMPLE_DELAYED_WRITERS_OBJECTS = bin/multi_writers_queue.o bin/test_simple_delayed_writers_lock.o bin/simple_delayed_writers_lock.o
BENCHMARK_OBJECTS = bin/multi_writers_queue.o bin/simple_delayed_writers_lock.o bin/benchmark_functions.o
BENCHMARK_OBJECTS_ACCESS_SKIPLIST = bin/multi_writers_queue.o bin/simple_delayed_writers_lock.o bin/benchmark_functions_access_skiplist.o bin/skiplist.o

LIBS =

all: bin/test_multi_writers_queue bin/test_simple_delayed_writers_lock bin/mixed_ops_benchmark bin/mixed_ops_benchmark_access_skiplist

bin/test_multi_writers_queue: $(TEST_MULTI_WRITERS_QUEUE_OBJECTS)
	$(CC) -o bin/test_multi_writers_queue $(TEST_MULTI_WRITERS_QUEUE_OBJECTS) $(LIBS) $(CFLAGS)

bin/test_simple_delayed_writers_lock: $(TEST_SIMPLE_DELAYED_WRITERS_OBJECTS)
	$(CC) -o bin/test_simple_delayed_writers_lock $(TEST_SIMPLE_DELAYED_WRITERS_OBJECTS) $(LIBS) $(CFLAGS)

bin/mixed_ops_benchmark: $(BENCHMARK_OBJECTS)  bin/mixed_ops_benchmark.o
	$(CC) -o bin/mixed_ops_benchmark $(BENCHMARK_OBJECTS) bin/mixed_ops_benchmark.o $(LIBS) $(CFLAGS)

bin/mixed_ops_benchmark_access_skiplist: $(BENCHMARK_OBJECTS_ACCESS_SKIPLIST)  bin/mixed_ops_benchmark.o
	$(CC) -o bin/mixed_ops_benchmark_access_skiplist $(BENCHMARK_OBJECTS_ACCESS_SKIPLIST) bin/mixed_ops_benchmark.o $(LIBS) $(CFLAGS)

bin/test_simple_delayed_writers_lock.o: src/tests/test_simple_delayed_writers_lock.c src/tests/test_framework.h src/utils/smp_utils.h
	$(CC) $(CFLAGS) -c src/tests/test_simple_delayed_writers_lock.c ; \
	mv *.o bin/

bin/test_multi_writers_queue.o: src/tests/test_multi_writers_queue.c src/tests/test_framework.h
	$(CC) $(CFLAGS) -c src/tests/test_multi_writers_queue.c ; \
	mv *.o bin/

bin/benchmark_functions.o: src/benchmark/benchmark_functions.c src/benchmark/benchmark_functions.h src/lock/simple_delayed_writers_lock.h src/utils/smp_utils.h
	$(CC) $(CFLAGS) -c src/benchmark/benchmark_functions.c ; \
	mv *.o bin/


bin/benchmark_functions_access_skiplist.o: src/benchmark/benchmark_functions.c src/benchmark/benchmark_functions.h src/lock/simple_delayed_writers_lock.h src/utils/smp_utils.h
	$(CC) $(CFLAGS) -D ACCESS_SKIPLIST -c src/benchmark/benchmark_functions.c ; \
	mv benchmark_functions.o bin/benchmark_functions_access_skiplist.o

bin/mixed_ops_benchmark.o: src/benchmark/benchmark_functions.c src/benchmark/benchmark_functions.h src/benchmark/mixed_ops_benchmark.c
	$(CC) $(CFLAGS) -c src/benchmark/mixed_ops_benchmark.c ; \
	mv *.o bin/

bin/simple_delayed_writers_lock.o: src/lock/simple_delayed_writers_lock.c src/datastructures/multi_writers_queue.h src/utils/smp_utils.h
	$(CC) $(CFLAGS) -c src/lock/simple_delayed_writers_lock.c ; \
	mv *.o bin/

bin/multi_writers_queue.o: src/datastructures/multi_writers_queue.c src/datastructures/multi_writers_queue.h src/utils/smp_utils.h
	$(CC) $(CFLAGS) -c src/datastructures/multi_writers_queue.c ; \
	mv *.o bin/

bin/skiplist.o: src/benchmark/skiplist/skiplist.c src/benchmark/skiplist/skiplist.h src/benchmark/skiplist/kvset.h
	$(CC) $(CFLAGS) -c src/benchmark/skiplist/skiplist.c ; \
	mv *.o bin/


run_test_multi_writers_queue: bin/test_multi_writers_queue
	valgrind --leak-check=yes ./bin/test_multi_writers_queue

run_test_simple_delayed_writers_lock: bin/test_simple_delayed_writers_lock
	./bin/test_simple_delayed_writers_lock

run_test_simple_delayed_writers_lock_valgrind: bin/test_simple_delayed_writers_lock
	valgrind --leak-check=yes ./bin/test_simple_delayed_writers_lock

run_all_benchmarks: bin/mixed_ops_benchmark bin/mixed_ops_benchmark_access_skiplist
	./src/benchmark/run_all_benchmarks.sh $(BENCHMARK_NUM_OF_THREADS) 

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
	rm -f bin/test_multi_writers_queue bin/test_simple_delayed_writers_lock bin/mixed_ops_benchmark bin/mixed_ops_benchmark_access_skiplist bin/*.o

check-syntax: test_multi_writers_queue
