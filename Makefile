
CC = gcc
CFLAGS = -I. -Ilock -Idatastructures -Itests -Iutils  -O3 -std=gnu99 -Wall -g -pthread
TEST_MULTI_WRITERS_QUEUE_OBJECTS = multi_writers_queue.o test_multi_writers_queue.o
TEST_SIMPLE_DELAYED_WRITERS_OBJECTS = multi_writers_queue.o test_simple_delayed_writers_lock.o simple_delayed_writers_lock.o
BENCHMARK_OBJECTS = multi_writers_queue.o simple_delayed_writers_lock.o benchmark_functions.o
LIBS =

all: test_multi_writers_queue test_simple_delayed_writers_lock benchmark_writes benchmark_reads benchmark_80_percent_reads

test_multi_writers_queue: $(TEST_MULTI_WRITERS_QUEUE_OBJECTS)
	$(CC) -o test_multi_writers_queue $(TEST_MULTI_WRITERS_QUEUE_OBJECTS) $(LIBS) $(CFLAGS)

test_simple_delayed_writers_lock: $(TEST_SIMPLE_DELAYED_WRITERS_OBJECTS)
	$(CC) -o test_simple_delayed_writers_lock $(TEST_SIMPLE_DELAYED_WRITERS_OBJECTS) $(LIBS) $(CFLAGS)

benchmark_writes: $(BENCHMARK_OBJECTS) benchmark_writes.o
	$(CC) -o benchmark_writes $(BENCHMARK_OBJECTS) benchmark_writes.o $(LIBS) $(CFLAGS)

benchmark_reads: $(BENCHMARK_OBJECTS)  benchmark_reads.o
	$(CC) -o benchmark_reads $(BENCHMARK_OBJECTS) benchmark_reads.o $(LIBS) $(CFLAGS)

benchmark_80_percent_reads: $(BENCHMARK_OBJECTS)  benchmark_80_percent_reads.o
	$(CC) -o benchmark_80_percent_reads $(BENCHMARK_OBJECTS) benchmark_80_percent_reads.o $(LIBS) $(CFLAGS)

run_test_multi_writers_queue: test_multi_writers_queue
	valgrind --leak-check=yes ./test_multi_writers_queue

run_test_simple_delayed_writers_lock: test_simple_delayed_writers_lock
	./test_simple_delayed_writers_lock

run_test_simple_delayed_writers_lock_valgrind: test_simple_delayed_writers_lock
	valgrind --leak-check=yes ./test_simple_delayed_writers_lock

run_writes_benchmark: benchmark_writes simple_delayed_writers_lock.o
	./benchmark_writes 1 2 3 4 5 6 7 8 ; \
	gnuplot -e "output_filename='writes_benchmark.png'" -e "input_filename='writes_benchmark.dat'" -e "the_title='Writes Benchmark'" benchmark/plot_bench.gp

run_reads_benchmark: benchmark_reads simple_delayed_writers_lock.o
	./benchmark_reads 1 2 3 4 5 6 7 8 ; \
	gnuplot -e "output_filename='reads_benchmark.png'" -e "input_filename='reads_benchmark.dat'" -e "the_title='Reads Benchmark'" benchmark/plot_bench.gp

run_80_percent_reads_benchmark: benchmark_80_percent_reads simple_delayed_writers_lock.o
	./benchmark_80_percent_reads 1 2 3 4 5 6 7 8 ; \
	gnuplot -e "output_filename='80_percent_reads_benchmark.png'" -e "input_filename='80_percent_reads_benchmark.dat'" -e "the_title='80 Percent Reads Benchmark'" benchmark/plot_bench.gp

test_simple_delayed_writers_lock.o: tests/test_simple_delayed_writers_lock.c tests/test_framework.h utils/smp_utils.h
	$(CC) $(CFLAGS) -c tests/test_simple_delayed_writers_lock.c

test_multi_writers_queue.o: tests/test_multi_writers_queue.c tests/test_framework.h
	$(CC) $(CFLAGS) -c tests/test_multi_writers_queue.c

benchmark_functions.o: benchmark/benchmark_functions.c benchmark/benchmark_functions.h lock/simple_delayed_writers_lock.h utils/smp_utils.h
	$(CC) $(CFLAGS) -c benchmark/benchmark_functions.c

benchmark_writes.o: benchmark/benchmark_functions.h benchmark/benchmark_writes.c
	$(CC) $(CFLAGS) -c benchmark/benchmark_writes.c

benchmark_reads.o: benchmark/benchmark_functions.h benchmark/benchmark_reads.c
	$(CC) $(CFLAGS) -c benchmark/benchmark_reads.c

benchmark_80_percent_reads.o: benchmark/benchmark_functions.h benchmark/benchmark_80_percent_reads.c
	$(CC) $(CFLAGS) -c benchmark/benchmark_80_percent_reads.c

simple_delayed_writers_lock.o: lock/simple_delayed_writers_lock.c datastructures/multi_writers_queue.h utils/smp_utils.h
	$(CC) $(CFLAGS) -c lock/simple_delayed_writers_lock.c

multi_writers_queue.o: datastructures/multi_writers_queue.c datastructures/multi_writers_queue.h utils/smp_utils.h
	$(CC) $(CFLAGS) -c datastructures/multi_writers_queue.c

clean:
	rm -f test_multi_writers_queue test_simple_delayed_writers_lock benchmark_writes benchmark_80_percent_reads benchmark_reads *.o

check-syntax: test_multi_writers_queue
