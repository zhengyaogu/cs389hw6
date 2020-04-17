#include <iostream>
#include <cassert>
#include "cache.hh"
#include "workload_generator.hh"
#include <cstring>

int main()
{
    Cache my_cache("127.0.0.1", "10002");
    WorkloadGenerator wkld(100, 1000);
    unsigned int warm_up_iters = 10000;
    unsigned int test_iters = 100000;
    unsigned int count = 0;
    unsigned int hits = 0;

    int set_count = 0;
    int del_count = 0;
    for (unsigned int i = 0; i < warm_up_iters + test_iters; i++)
    {
        std::string request_type = wkld.request_type_dist();
        if (request_type.compare("get") == 0)
            {
                Cache::size_type s = 0;
                key_type key = wkld.random_key();
                
                Cache::val_type val = my_cache.get(key, s);
                if (i >= warm_up_iters)
                {
                    count++;
                    if (val != nullptr) 
                    {
                        hits++;
                        delete[] val;
                    }
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
                my_cache.set(key, val, strlen(val) + 1);
                delete[] val;
//                std::cout << "set finished" << std::endl;
                set_count ++;
            }
       
        else if (request_type.compare("del") == 0)
            {
                key_type key = wkld.random_key();
                my_cache.del(key);
                del_count ++;
            }
            
    }
    std::cout << "Count: " << count << ", Hits: " << hits << std::endl;
    std::cout << "Set: " << set_count << ", Del: " << del_count << std::endl;
    std::cout << "The hit rate is: " << double(hits) / double(count) << std::endl;
}