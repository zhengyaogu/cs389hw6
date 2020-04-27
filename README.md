# CS389 HW6
author: Zhengyao Gu, Albert Ji

### multi-threaded benchmark
We updated `baseline_latencies` and made it thread-safe. It's now a void functiona and takes a vector reference and mutex as additional parameters.\
 It measures the latency and pushs the results in the vector, which is protected by the mutex so only one thread could write to it at the same time. Note that the measurement is done in microseconds because our latency is very low, usually below 1 millosecond.\
 The function `threaded_performance` would create `nthread` number of `baseline_latencies` threads and run them concurrently. It then calculates the 95th-latency and mean throughout and returns them.
 
 ### multithreaded server
We use `std::lock_guard` to handle the concurrency of the cache. We create two global mutex, one in `cache_lib.cc` and the other in the `lru_evictor.cc`. When we're modifying the context inside them, we lock them up to prevent issues like race conditions. 

### Issues currently known
There is a strange bug that keeps preventing us measuring the performance. Our client would always be terminated (aborted) during the second request become of an "end of stream" error. \
This error also occured in hw5, but we managed to avoid it by creating a new connection for each request of the client. However, this method doesn't work in hw6. 
 - When the server is single-thread and the client is multi-thread, we would quickly receive a "Cannot allocate memory" error and the client would get aborted. Even if we don't receive any error, the client would freeze if nreq and nthread are large, making measuring the performance extremely difficult.
 - When the server is multi-thread, we would get "connect: Invalid argument" error nearly for each request, which means the server is not processing the request correctly. Thus measuring under this circumstance is meaningless.
