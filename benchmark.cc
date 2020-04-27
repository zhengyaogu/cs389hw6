#include <iostream>
#include <cassert>
#include "cache.hh"
#include "workload_generator.hh"
#include <cstring>
#include <vector>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

void baseline_latencies(int nreq, double set_del_ratio, int key_pool_size, std::vector <double> & req_time_cost, std::mutex & mutex)
{
    Cache* my_cache = new Cache("127.0.0.1", "10002");
    WorkloadGenerator wkld(set_del_ratio, key_pool_size);
    for(int i = 1; i <= nreq; i ++)
    {
        std::chrono::high_resolution_clock::time_point t1, t2;
        std::string request_type = wkld.request_type_dist();
        if (request_type.compare("get") == 0)
        {
            Cache::size_type s = 0;
            key_type key = wkld.random_key();
            t1 = std::chrono::high_resolution_clock::now();
            Cache::val_type val = my_cache->get(key, s);
            t2 = std::chrono::high_resolution_clock::now();
            delete[] val;
        }
        else if (request_type.compare("set") == 0)
        {
            key_type key = wkld.random_key();
            const Cache::val_type val = wkld.random_val();
            t1 = std::chrono::high_resolution_clock::now();
            my_cache->set(key, val, strlen(val) + 1);
            t2 = std::chrono::high_resolution_clock::now();
            delete[] val;
        }
        else if (request_type.compare("del") == 0)
        {
            key_type key = wkld.random_key();
            t1 = std::chrono::high_resolution_clock::now();
            my_cache->del(key);
            t2 = std::chrono::high_resolution_clock::now();
        }
        double duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();

        {
            std::lock_guard guard(mutex);
            req_time_cost.push_back(duration);
        }
    }
    delete my_cache;
}

std::pair<double, double> threaded_performance(int nthread, int nreq, double set_del_ratio, int key_pool_size)
{
    std::mutex mutex;
    double latency_95 = 0, mean_throughput = 0;
    std::vector <double> measurements;
    std::vector <std::thread> my_threads;
    for(int i = 0; i < nthread; i ++)
        my_threads.push_back(std::thread(baseline_latencies, nreq, set_del_ratio, key_pool_size, std::ref(measurements), std::ref(mutex)));
    for(int i = 0; i < nthread; i ++)
    {
        assert(my_threads[i].joinable());
        my_threads[i].join();
    }
    int req_size = nreq * nthread;
    assert(int(measurements.size()) == req_size);
    if(req_size != 0)
    {
        // from low to high
        std::sort(measurements.begin(), measurements.end());
        int latency_95_index = int(double(req_size) * 0.95);
        if(latency_95_index >= req_size)
            latency_95_index = req_size - 1;
        latency_95 = measurements[latency_95_index];
        double sum = 0;
        for(int i = 0; i < req_size; i ++)
            sum += measurements[i];
        assert(sum != 0);
        // convert request per millisecond to request per second
        mean_throughput = double(req_size) / sum * 1000000.0;
        return std::make_pair (latency_95, mean_throughput);
    }
    return std::make_pair (0.0, 0.0);
}

int main(int argc, char* argv[])
{
    assert(argc == 5);
    auto result = threaded_performance(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
    std::cout << result.first << " " << result.second << std::endl;
    
    return 0;
}