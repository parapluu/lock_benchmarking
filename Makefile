
GIT_COMMIT = `date +'%y.%m.%d_%H.%M.%S'`_`git rev-parse HEAD`
BENCHMARK_NUM_OF_THREADS = 1 2
CC = gcc
CFLAGS = -I. -Isrc/lock -Isrc/datastructures -Isrc/tests -Isrc/utils  -O3 -std=gnu99 -Wall -g -pthread
TEST_MULTI_WRITERS_QUEUE_OBJECTS = bin/multi_writers_queue.o bin/test_multi_writers_queue.o
TEST_SIMPLE_DELAYED_WRITERS_OBJECTS = bin/multi_writers_queue.o bin/test_simple_delayed_writers_lock.o bin/simple_delayed_writers_lock.o
BENCHMARK_OBJECTS = bin/multi_writers_queue.o bin/simple_delayed_writers_lock.o bin/benchmark_functions.o
LIBS =

all: bin/test_multi_writers_queue bin/test_simple_delayed_writers_lock bin/mixed_ops_benchmark

bin/test_multi_writers_queue: $(TEST_MULTI_WRITERS_QUEUE_OBJECTS)
	$(CC) -o bin/test_multi_writers_queue $(TEST_MULTI_WRITERS_QUEUE_OBJECTS) $(LIBS) $(CFLAGS)

bin/test_simple_delayed_writers_lock: $(TEST_SIMPLE_DELAYED_WRITERS_OBJECTS)
	$(CC) -o bin/test_simple_delayed_writers_lock $(TEST_SIMPLE_DELAYED_WRITERS_OBJECTS) $(LIBS) $(CFLAGS)

bin/mixed_ops_benchmark: $(BENCHMARK_OBJECTS)  bin/mixed_ops_benchmark.o
	$(CC) -o bin/mixed_ops_benchmark $(BENCHMARK_OBJECTS) bin/mixed_ops_benchmark.o $(LIBS) $(CFLAGS)

bin/test_simple_delayed_writers_lock.o: src/tests/test_simple_delayed_writers_lock.c src/tests/test_framework.h src/utils/smp_utils.h
	$(CC) $(CFLAGS) -c src/tests/test_simple_delayed_writers_lock.c ; \
	mv *.o bin/

bin/test_multi_writers_queue.o: src/tests/test_multi_writers_queue.c src/tests/test_framework.h
	$(CC) $(CFLAGS) -c src/tests/test_multi_writers_queue.c ; \
	mv *.o bin/

bin/benchmark_functions.o: src/benchmark/benchmark_functions.c src/benchmark/benchmark_functions.h src/lock/simple_delayed_writers_lock.h src/utils/smp_utils.h
	$(CC) $(CFLAGS) -c src/benchmark/benchmark_functions.c ; \
	mv *.o bin/

bin/mixed_ops_benchmark.o: src/benchmark/benchmark_functions.h src/benchmark/mixed_ops_benchmark.c
	$(CC) $(CFLAGS) -c src/benchmark/mixed_ops_benchmark.c ; \
	mv *.o bin/

bin/simple_delayed_writers_lock.o: src/lock/simple_delayed_writers_lock.c src/datastructures/multi_writers_queue.h src/utils/smp_utils.h
	$(CC) $(CFLAGS) -c src/lock/simple_delayed_writers_lock.c ; \
	mv *.o bin/

bin/multi_writers_queue.o: src/datastructures/multi_writers_queue.c src/datastructures/multi_writers_queue.h src/utils/smp_utils.h
	$(CC) $(CFLAGS) -c src/datastructures/multi_writers_queue.c ; \
	mv *.o bin/


run_test_multi_writers_queue: bin/test_multi_writers_queue
	valgrind --leak-check=yes ./bin/test_multi_writers_queue

run_test_simple_delayed_writers_lock: bin/test_simple_delayed_writers_lock
	./bin/test_simple_delayed_writers_lock

run_test_simple_delayed_writers_lock_valgrind: bin/test_simple_delayed_writers_lock
	valgrind --leak-check=yes ./bin/test_simple_delayed_writers_lock

run_writes_benchmark: bin/mixed_ops_benchmark bin/simple_delayed_writers_lock.o
	FILE_NAME_PREFIX=benchmark_results/writes_benchmark_$(GIT_COMMIT) ; \
	./bin/mixed_ops_benchmark $$FILE_NAME_PREFIX 0.0 1000000 10000 10000 $(BENCHMARK_NUM_OF_THREADS) ; \
	gnuplot -e "output_filename='$$FILE_NAME_PREFIX.png'" -e "input_filename='$$FILE_NAME_PREFIX.dat'" -e "the_title='80 Percent Reads Benchmark'" src/benchmark/plot_bench.gp

run_reads_benchmark: bin/mixed_ops_benchmark bin/simple_delayed_writers_lock.o
	FILE_NAME_PREFIX=benchmark_results/reads_benchmark_$(GIT_COMMIT) ; \
	./bin/mixed_ops_benchmark $$FILE_NAME_PREFIX 1.0 1000000 10000 10000 $(BENCHMARK_NUM_OF_THREADS) ; \
	gnuplot -e "output_filename='$$FILE_NAME_PREFIX.png'" -e "input_filename='$$FILE_NAME_PREFIX.dat'" -e "the_title='80 Percent Reads Benchmark'" src/benchmark/plot_bench.gp


run_80_percent_reads_benchmark: bin/mixed_ops_benchmark bin/simple_delayed_writers_lock.o
	FILE_NAME_PREFIX=benchmark_results/80_percent_reads_benchmark_$(GIT_COMMIT) ; \
	./bin/mixed_ops_benchmark $$FILE_NAME_PREFIX 0.8 1000000 10000 10000 $(BENCHMARK_NUM_OF_THREADS) ; \
	gnuplot -e "output_filename='$$FILE_NAME_PREFIX.png'" -e "input_filename='$$FILE_NAME_PREFIX.dat'" -e "the_title='80 Percent Reads Benchmark'" src/benchmark/plot_bench.gp

run_90_percent_reads_benchmark: bin/mixed_ops_benchmark bin/simple_delayed_writers_lock.o
	FILE_NAME_PREFIX=benchmark_results/90_percent_reads_benchmark_$(GIT_COMMIT) ; \
	./bin/mixed_ops_benchmark $$FILE_NAME_PREFIX 0.9 1000000 10000 10000 $(BENCHMARK_NUM_OF_THREADS) ; \
	gnuplot -e "output_filename='$$FILE_NAME_PREFIX.png'" -e "input_filename='$$FILE_NAME_PREFIX.dat'" -e "the_title='80 Percent Reads Benchmark'" src/benchmark/plot_bench.gp


run_95_percent_reads_benchmark: bin/mixed_ops_benchmark bin/simple_delayed_writers_lock.o
	FILE_NAME_PREFIX=benchmark_results/95_percent_reads_benchmark_$(GIT_COMMIT) ; \
	./bin/mixed_ops_benchmark $$FILE_NAME_PREFIX 0.95 1000000 10000 10000 $(BENCHMARK_NUM_OF_THREADS) ; \
	gnuplot -e "output_filename='$$FILE_NAME_PREFIX.png'" -e "input_filename='$$FILE_NAME_PREFIX.dat'" -e "the_title='80 Percent Reads Benchmark'" src/benchmark/plot_bench.gp

run_99_percent_reads_benchmark: bin/mixed_ops_benchmark bin/simple_delayed_writers_lock.o
	FILE_NAME_PREFIX=benchmark_results/99_percent_reads_benchmark_$(GIT_COMMIT) ; \
	./bin/mixed_ops_benchmark $$FILE_NAME_PREFIX 0.99 1000000 10000 10000 $(BENCHMARK_NUM_OF_THREADS) ; \
	gnuplot -e "output_filename='$$FILE_NAME_PREFIX.png'" -e "input_filename='$$FILE_NAME_PREFIX.dat'" -e "the_title='80 Percent Reads Benchmark'" src/benchmark/plot_bench.gp


clean:
	rm -f bin/test_multi_writers_queue bin/test_simple_delayed_writers_lock bin/mixed_ops_benchmark bin/*.o

check-syntax: test_multi_writers_queue
