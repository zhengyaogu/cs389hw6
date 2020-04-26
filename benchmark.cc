#include <iostream>
#include <cassert>
#include "cache.hh"
#include "workload_generator.hh"
#include <cstring>
#include <vector>
#include <algorithm>
#include <chrono>

// It's Ok (at least after C++ 11) to return a vector.
std::vector<double> baseline_latencies(int nreq, double set_del_ratio, int key_pool_size)
{
    std::vector <double> req_time_cost;
    Cache my_cache("127.0.0.1", "10002");
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
            Cache::val_type val = my_cache.get(key, s);
            t2 = std::chrono::high_resolution_clock::now();
            delete[] val;
        }
        else if (request_type.compare("set") == 0)
        {
            key_type key = wkld.random_key();
            const Cache::val_type val = wkld.random_val();
            t1 = std::chrono::high_resolution_clock::now();
            my_cache.set(key, val, strlen(val) + 1);
            t2 = std::chrono::high_resolution_clock::now();
            delete[] val;
        }
        else if (request_type.compare("del") == 0)
        {
            key_type key = wkld.random_key();
            t1 = std::chrono::high_resolution_clock::now();
            my_cache.del(key);
            t2 = std::chrono::high_resolution_clock::now();
        }
        double duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
        req_time_cost.push_back(duration);
    }
    return req_time_cost;
}

std::pair<double, double> baseline_performance(int nreq, double set_del_ratio, int key_pool_size)
{
    double latency_95 = 0, mean_throughput = 0;
    std::vector <double> measurements = baseline_latencies(nreq, set_del_ratio, key_pool_size);
    assert(int(measurements.size()) == nreq);
    if(nreq != 0)
    {
        // from low to high
        std::sort(measurements.begin(), measurements.end());
        int latency_95_index = int(double(nreq) * 0.95);
        if(latency_95_index >= nreq)
            latency_95_index = nreq - 1;
        latency_95 = measurements[latency_95_index];
        double sum = 0;
        for(int i = 0; i < nreq; i ++)
            sum += measurements[i];
        assert(sum != 0);
        // convert request per millisecond to request per second
        mean_throughput = double(nreq) / sum * 1000.0;
        return std::make_pair (latency_95, mean_throughput);
    }
    return std::make_pair (0.0, 0.0);
}

int main()
{
    /*
    auto my_vector = baseline_latencies(100000, 10, 10000);
    std::sort(my_vector.begin(), my_vector.end());
    int j = 0;
    for(unsigned int i = 0; i < my_vector.size(); i ++)
    {
        if(my_vector[i] != my_vector[j])
        {
            std::cout << my_vector[j] / double(my_vector.size())  << " " << i << std::endl;
            j = i;
        }
        if(i == my_vector.size() - 1)
        {
            std::cout << my_vector[i] / double(my_vector.size())  << " " << i + 1 << std::endl;
        }
    }
    */
    auto result = baseline_performance(100000, 10, 100000);
    std::cout << result.first << " " << result.second << std::endl;
    
    return 0;
}