all: Run_server

Run_benchmark: compile_benchmark execute_benchmark
Run_server: compile_server execute_server

compile_server: cache_lib.cc lru_evictor.cc fifo_evictor.cc cache_server.cc
	g++ -I ../HW4/boost_1_72_0 -Wall -std=c++17 -pthread -g -O3 -o server.o cache_lib.cc cache_server.cc


compile_calibrate: calibrate.cc cache_client.cc workload_generator.cc
	g++ -I ../HW4/boost_1_72_0 -Wall -pthread -std=c++17 -g -O3 -o calibrate.o cache_client.cc calibrate.cc workload_generator.cc

compile_benchmark: benchmark.cc cache_client.cc workload_generator.cc
	g++ -I ../HW4/boost_1_72_0 -Wall -pthread -std=c++17 -g -O3 -o benchmark.o cache_client.cc benchmark.cc workload_generator.cc



execute_server:
	./server.o -m 1800000 -t 10
	

execute_benchmark:
	./benchmark.o 1 10 5 10000

clean:
	rm -f *.o