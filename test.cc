#include <iostream>
#include <cassert>
#include "cache.hh"
#define CATCH_CONFIG_MAIN 
#include "catch.hpp"
#include "workload_generator.hh"
 

using byte_type = char;
using val_type = const byte_type*; 
using size_type = uint32_t; 
using key_type = std::string;

TEST_CASE("FIFO", "[fifo][default]")
{
    Cache my_cache("127.0.0.1", "10002");

    // the key shouldn't be touched
    size_type get_size = 0;
    my_cache.get("fake_key", get_size);

    auto first_str = "first_val";
    val_type val_ptr = first_str;
    size_type first_size = 10;
    my_cache.set("first_key", val_ptr, first_size);

    get_size = 0;
    my_cache.get("first_key", get_size);

    get_size = 0;
    my_cache.get("third_key", get_size);

    auto second_str = "second_val";
    val_ptr = second_str;
    size_type second_size = 11;
    my_cache.set("second_key", val_ptr, second_size);

    auto third_str = "third_val";
    val_ptr = third_str;
    size_type third_size = 10;
    my_cache.set("third_key", val_ptr, third_size);
    SECTION("test that first_key is evicted")
    {
        get_size = 0;
        REQUIRE(my_cache.space_used() == third_size + second_size);
        REQUIRE(my_cache.get("first_key", get_size) == nullptr);
    }

    auto fourth_str = "fourth_str";
    val_ptr = fourth_str;
    size_type fourth_size = 11;
    my_cache.set("fourth_key", val_ptr, fourth_size);
    SECTION("test if third_key is evicted before second_key")
    {
        get_size = 0;
        REQUIRE(my_cache.space_used() == fourth_size + third_size); // third_size != second_size
        REQUIRE(my_cache.get("second_key", get_size) == nullptr);
        REQUIRE(my_cache.get("third_key", get_size) != nullptr);
    }

     my_cache.reset();
}

TEST_CASE ("set1", "[set][null]")
{
    Cache my_cache("127.0.0.1", "10002");    

    auto first_str = "first_val";
    val_type val_ptr = first_str;
    size_type first_size = 10;
    my_cache.set("first_key", val_ptr, first_size);

    auto second_str = "second_val";
    val_ptr = second_str;
    size_type second_size = 11;
    my_cache.set("second_key", val_ptr, second_size);

    REQUIRE(my_cache.space_used() == first_size + second_size);

    auto modified_first_str = "modified_first_val";
    val_ptr = modified_first_str;
    size_type modified_first_size = 19;
    my_cache.set("first_key", val_ptr, modified_first_size);

    REQUIRE(my_cache.space_used() == modified_first_size + second_size);

    auto third_str = "something_larger_than_30_characters";
    val_ptr = third_str;
    size_type third_size = 36;
    my_cache.set("third_key", val_ptr, third_size);

    auto fourth_str = "fourth_val";
    val_ptr = fourth_str;
    size_type fourth_size = 11;
    my_cache.set("fourth_key", val_ptr, fourth_size);

    //This should behave differently.
    REQUIRE(my_cache.space_used() == modified_first_size + second_size);
    my_cache.reset();
}

TEST_CASE ("set2", "[set][fifo]")
{
    Cache my_cache("127.0.0.1", "10002");    
    auto first_str = "first_val";
    val_type val_ptr = first_str;
    size_type first_size = 10;
    my_cache.set("first_key", val_ptr, first_size);

    auto second_str = "second_val";
    val_ptr = second_str;
    size_type second_size = 11;
    my_cache.set("second_key", val_ptr, second_size);

    REQUIRE(my_cache.space_used() == first_size + second_size);

    auto modified_first_str = "modified_first_val";
    val_ptr = modified_first_str;
    size_type modified_first_size = 19;
    my_cache.set("first_key", val_ptr, modified_first_size);

    REQUIRE(my_cache.space_used() == modified_first_size + second_size);

    SECTION("inserting a data bigger than maxmem: expected no insertion")
    {
        auto third_str = "something_larger_than_30_characters";
        val_ptr = third_str;
        size_type third_size = 36;
        my_cache.set("third_key", val_ptr, third_size);

        REQUIRE(my_cache.space_used() == modified_first_size + second_size);

        size_type get_size = 0;
        auto get_val_ptr = my_cache.get("third_key", get_size);
        REQUIRE(get_val_ptr == nullptr);
    }

    SECTION("inserting a third pair, expect to evict first pair")
    {
        auto third_str = "third_val";
        val_ptr = third_str;
        size_type third_size = 10;
        my_cache.set("third_key", val_ptr, third_size);

        REQUIRE(my_cache.space_used() == third_size + second_size);

        size_type get_size = 0;
        auto get_val_ptr = my_cache.get("first_key", get_size);
        REQUIRE(get_val_ptr == nullptr);
    }

    SECTION("Modified the second pair to a bigger size. The modification will induce the eviction of pair 1.")
    {
        auto modified_second_str = "bigger_than_second_val";
        val_ptr = modified_second_str;
        size_type modified_second_size = 23;
        my_cache.set("second_key", val_ptr, modified_second_size);

        REQUIRE(my_cache.space_used() == modified_second_size);

        size_type get_size = 0;
        auto get_val_ptr = my_cache.get("first_key", get_size);
        REQUIRE(get_val_ptr == nullptr);
    }
    my_cache.reset();

}

TEST_CASE ("get", "[null][default]")
{
    Cache my_cache("127.0.0.1", "10002");
    auto first_str = "first_val";
    val_type val_ptr = first_str;
    size_type first_size = 10;
    my_cache.set("first_key", val_ptr, first_size);

    auto second_str = "second_val";
    val_ptr = second_str;
    size_type second_size = 11;
    my_cache.set("second_key", val_ptr, second_size);

    size_type get_size = 0;
    auto get_val_ptr = my_cache.get("second_key", get_size);
    REQUIRE(get_val_ptr != nullptr);
    if (get_val_ptr != nullptr)
    {
        for (unsigned int i = 0; i < second_size - 1; i++)
        {
            REQUIRE(val_ptr[i] == get_val_ptr[i]);
        }
        REQUIRE(get_size == second_size);
    }
    
    auto modified_first_str = "modified_first_val";
    val_ptr = modified_first_str;
    size_type modified_first_size = 19;
    my_cache.set("first_key", val_ptr, modified_first_size);

    get_size = 0;
    get_val_ptr = my_cache.get("first_key", get_size);
    REQUIRE(get_val_ptr != nullptr);
    for(unsigned int i = 0; i < get_size; i++)
        std::cout << get_val_ptr[i];
    std::cout << "\n";
    if (get_val_ptr != nullptr)
    {
        for (unsigned int i = 0; i < second_size; i++)
        {
            REQUIRE(val_ptr[i] == get_val_ptr[i]);
        }
        REQUIRE(get_size == modified_first_size);
    }
    
    my_cache.reset();
}

TEST_CASE ("del", "[null][default]")
{
    Cache my_cache("127.0.0.1", "10002");
    auto first_str = "first_val";
    val_type val_ptr = first_str;
    size_type first_size = 10;
    my_cache.set("first_key", val_ptr, first_size);

    auto second_str = "second_val";
    val_ptr = second_str;
    size_type second_size = 11;
    my_cache.set("second_key", val_ptr, second_size);

    my_cache.del("first_key");
    REQUIRE(my_cache.space_used() == second_size);

    my_cache.del("first_key");

    my_cache.del("second_key");
    REQUIRE(my_cache.space_used() == 0);
    
    my_cache.reset();
}

// used all the interface features at the same time and test Cache::reset
TEST_CASE("reset", "[fifo][reset]")
{
    Cache my_cache("127.0.0.1", "10002");    

    auto first_str = "first_val";
    val_type val_ptr = first_str;
    size_type first_size = 10;
    my_cache.set("first_key", val_ptr, first_size);

    auto second_str = "second_val";
    val_ptr = second_str;
    size_type second_size = 11;
    my_cache.set("second_key", val_ptr, second_size);

    auto modified_first_str = "modified_first_val";
    val_ptr = modified_first_str;
    size_type modified_first_size = 19;
    my_cache.set("first_key", val_ptr, modified_first_size);

    auto third_str = "something_larger_than_30_characters";
    val_ptr = third_str;
    size_type third_size = 36;
    my_cache.set("third_key", val_ptr, third_size);

    auto fourth_str = "fourth_val";
    val_ptr = fourth_str;
    size_type fourth_size = 11;
    my_cache.set("fourth_key", val_ptr, fourth_size);

    auto modified_second_str = "modified_second_val";
    val_ptr = modified_second_str;
    size_type modified_second_size = 20;
    my_cache.set("second_key", val_ptr, modified_second_size);

    my_cache.reset();
    REQUIRE(my_cache.get("first_key", first_size) == nullptr);
    REQUIRE(my_cache.get("second_key", first_size) == nullptr);
    REQUIRE(my_cache.get("third_key", first_size) == nullptr);
    REQUIRE(my_cache.get("fourth_key", first_size) == nullptr);
    REQUIRE(my_cache.space_used() == 0);

    
    my_cache.reset();
}

TEST_CASE("LRU", "[lru][extra]")
{
    Cache my_cache("127.0.0.1", "10002");    

    auto first_str = "first_val";
    val_type val_ptr = first_str;
    size_type first_size = 10;
    my_cache.set("first_key", val_ptr, first_size);

    auto second_str = "second_val";
    val_ptr = second_str;
    size_type second_size = 11;
    my_cache.set("second_key", val_ptr, second_size);

    REQUIRE(my_cache.space_used() == first_size + second_size);

    auto modified_first_str = "modified_first_val";
    val_ptr = modified_first_str;
    size_type modified_first_size = 19;
    my_cache.set("first_key", val_ptr, modified_first_size);

    REQUIRE(my_cache.space_used() == modified_first_size + second_size);

    auto third_str = "something_larger_than_30_characters";
    val_ptr = third_str;
    size_type third_size = 36;
    my_cache.set("third_key", val_ptr, third_size);

    auto fourth_str = "fourth_val";
    val_ptr = fourth_str;
    size_type fourth_size = 11;
    my_cache.set("fourth_key", val_ptr, fourth_size);

    //This should behave differently.
    REQUIRE(my_cache.space_used() == modified_first_size + fourth_size);

    auto another_modified_first_str = "1";
    val_ptr = another_modified_first_str;
    size_type another_modified_first_size = 2;
    my_cache.set("first_key", val_ptr, another_modified_first_size);
    REQUIRE(my_cache.space_used() == another_modified_first_size + fourth_size);

    auto fifth_str = "_the_fifth_val_";
    val_ptr = fifth_str;
    size_type fifth_size = 16;
    my_cache.set("fifth_key", val_ptr, fifth_size);
    REQUIRE(my_cache.space_used() == another_modified_first_size + fourth_size + fifth_size);

    // Now fourth should be the middle element in the evictor.
    size_type temp_size = 0;
    REQUIRE(my_cache.get("fourth_key", temp_size) != nullptr);
    REQUIRE(temp_size == fourth_size);
    // Now fourth should be the last element, another_modified_first_str 
    // should be the least recently used element.

    auto sixth_str = "12";
    val_ptr = sixth_str;
    size_type sixth_size = 3;
    my_cache.set("sixth_key", val_ptr, sixth_size);
    REQUIRE(my_cache.space_used() == fourth_size + fifth_size + sixth_size);


    std::cout << "\n";
}

TEST_CASE("WORKLOAD", "[workload]")
{
    Cache my_cache("127.0.0.1", "10002");
    WorkloadGenerator wkld(2);
    unsigned int warm_up_iters = 10000;
    unsigned int test_iters = 10000;
    unsigned int count = 0;
    unsigned int hits = 0;

    for (unsigned int i = 0; i < warm_up_iters + test_iters; i++)
    {
        std::string request_type = wkld.request_type_dist();
        if (request_type.compare("get") == 0)
            {
                Cache::size_type s = 0;
                Cache::size_type key_size = wkld.key_size_dist();
                key_type key = wkld.random_key(key_size);
                Cache::val_type val = my_cache.get(key, s);
                if (i >= warm_up_iters)
                {
                    count++;
                    if (val != nullptr) {hits++;}
                }
            }
        else if (request_type.compare("get") == 0)
            {
                Cache::size_type key_size = wkld.key_size_dist();
                key_type key = wkld.random_key(key_size);
                Cache::size_type val_size = wkld.val_size_dist();
                const Cache::val_type val = wkld.random_val(key_size);
                my_cache.set(key, val, val_size + 1);
            }
        else if (request_type.compare("del") == 0)
            {
                Cache::size_type key_size = wkld.key_size_dist();
                key_type key = wkld.random_key(key_size);
                my_cache.del(key);
            }
    }

    std::cout << "The hit rate is: " << double(hits) / double(count) << std::endl;
}
