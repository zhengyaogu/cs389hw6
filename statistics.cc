#include <iostream>
#include <cassert>
#include "cache.hh"
#include "workload_generator.hh"
#include <cstring>
#include <chrono>
#include <fstream>

std::vector<double> baseline_latencies(size_t nreq)
{
    Cache my_cache("127.0.0.1", "10002");
    WorkloadGenerator wkld(10, 100000);
    unsigned int warm_up_iters = 100000;
    unsigned int test_iters = static_cast<unsigned int>(nreq);
    unsigned int count = 0;
    unsigned int hits = 0;
    std::vector<double> measurements;
    measurements.reserve(test_iters);

    int set_count = 0;
    int del_count = 0;
    for (unsigned int i = 0; i < warm_up_iters + test_iters; i++)
    {
        std::string request_type = wkld.request_type_dist();
        if (request_type.compare("get") == 0)
            {
                Cache::size_type s = 0;
                key_type key = wkld.random_key();

                auto start = std::chrono::high_resolution_clock::now();
                Cache::val_type val = my_cache.get(key, s);
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> duration = (end - start);

                if (i >= warm_up_iters)
                {
                    count++;
                    if (val != nullptr) 
                    {
                        hits++;
                        delete[] val;
                    }
		    measurements.push_back(duration.count());
                }
//                std::cout << "get finished" << std::endl;

            }
        else if (request_type.compare("set") == 0)
            {
                key_type key = wkld.random_key();
                
                const Cache::val_type val = wkld.random_val();
//                std::cout << "key_size= " << key_size << ", key = " << key << std::endl;
//                std::cout << "val_size= " << val_size << ", val = " ;
//                for(int i = 0; i < val_size; i ++)
//                    std::cout << val[i];
//                std::cout << std::endl;
		auto start = std::chrono::high_resolution_clock::now();
                my_cache.set(key, val, strlen(val) + 1);
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> duration = (end - start);

                delete[] val;
//                std::cout << "set finished" << std::endl;
                set_count ++;
		if (i >= warm_up_iters)
		{
		    measurements.push_back(duration.count());
		}
            }
       
        else if (request_type.compare("del") == 0)
            {
                key_type key = wkld.random_key();
		auto start = std::chrono::high_resolution_clock::now();
                my_cache.del(key);
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> duration = (end - start);

                del_count ++;

		if (i >= warm_up_iters)
		{
		    measurements.push_back(duration.count());
		}
            }
            
    }
    std::cout << "Count: " << count << ", Hits: " << hits << std::endl;
    std::cout << "Set: " << set_count << ", Del: " << del_count << std::endl;
    std::cout << "The hit rate is: " << double(hits) / double(count) << std::endl;

    return measurements;
}

void export_measurements(size_t nreq)
{
    auto m = baseline_latencies(nreq);
    std::ofstream csv;
    csv.open("measurements.csv");
	csv << "latency" << std::endl;
    for (auto& it : m)
    {
    	csv << it << std::endl;
    }
    csv.close();
}

int main()
{
    /*
    auto m = baseline_latencies(100000);
    for (unsigned int i = 0; i < 30; i++)
    {
        std::cout << m.at(i) << ", ";
    }
    std::cout << std::endl;*/
    export_measurements(100);
}
