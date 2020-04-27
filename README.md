# CS389 HW5
author: Zhengyao Gu, Albert Ji

### multi-threaded benchmark
We updated `baseline_latencies` and made it thread-safe. It's now a void functiona and takes a vector reference and mutex as additional parameters.\
 It measures the latency and pushs the results in the vector, which is protected by the mutex. 
