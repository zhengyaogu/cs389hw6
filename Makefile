all: Run_server

Run_server: compile_server execute_server
Run_client_test: compile_client execute_client

compile_server: cache_lib.cc lru_evictor.cc fifo_evictor.cc cache_server.cc
	g++ -I boost_1_72_0 -Wall -pthread -std=c++17 -g -O3 -o server.o cache_lib.cc lru_evictor.cc fifo_evictor.cc cache_server.cc

compile_client: cache_client.cc
	g++ -I boost_1_72_0 -Wall -pthread -std=c++17 -g -O3 -o test_client.o cache_client.cc

compile_test: test.cc cache_client.cc workload_generator.cc
	g++ -I boost_1_72_0 -Wall -pthread -std=c++17 -g -O3 -o test.o cache_client.cc test.cc workload_generator.cc

compile_calibrate: calibrate.cc cache_client.cc workload_generator.cc
	g++ -I boost_1_72_0 -Wall -pthread -std=c++17 -g -O3 -o calibrate.o cache_client.cc calibrate.cc workload_generator.cc

lru_test:
	./test.o [lru]
fifo_test:
	./test.o [fifo]
null_test:
	./test.o [null]
workload_test:
	./test.o [workload]

execute_server:
	./server.o -m 1024
	
execute_client:
	./test_client.o

clean:
	rm -f *.o