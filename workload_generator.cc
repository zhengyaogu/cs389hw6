#include "workload_generator.hh"
#include <math.h>
#include <algorithm>
#include "cache.hh"
#include "evictor.hh"
#include <stdlib.h> 
#include <random>
#include <iostream>
#include <vector>
#include <math.h>

class WorkloadGenerator::Impl
{
    private:
    double mu_k = 30.7984;
    double sigma_k = 8.20449;
    double k_k = 0.078688;
    
    double theta_v = 0;
    double sigma_v = 214.476;
    double k_v = 0.078688;

    double get_rate = 0.67;
    double set_rate;
    double del_rate;

    std::default_random_engine generator;
    std::uniform_real_distribution<double> uni_dist;

    std::vector<key_type> key_pool;
    unsigned int key_num;
    std::array<double, 100> temp_weights;

    std::discrete_distribution<int> dd;

    Cache::size_type key_size_dist()
    {
        double u = uni_dist(generator);
        if (u == 0) {return 0;}
        double x = (sigma_k / k_k) * (pow(-log(u), -k_k) - 1) + mu_k;
        return static_cast<Cache::size_type>(round(x));
    }

    Cache::size_type val_size_dist()
    {
        double u = uni_dist(generator);
        double x = (sigma_v / k_v) * (pow(1 - u, -k_v) - 1) + theta_v;
        return static_cast<Cache::size_type>(round(x));
    }

    key_type random_key(Cache::size_type size)
    {
        auto randchar = []() -> char
        {
            const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "!@#$%^&*()_+[]|;',.<>?";
            const size_t max_index = sizeof(charset) - 1;
            return charset[ rand() % max_index ];
        };
        key_type key(size, 0);
        std::generate_n( key.begin(), size, randchar );
        return key;
    }

    const Cache::val_type random_val(Cache::size_type size)
    {
        auto ret = random_key(size);
//        std::cout << "val_ret = " << ret << std::endl;
        char* val = new char[size + 1];
        for(unsigned int i = 0; i < size; i ++)
            val[i] = ret[i];
        val[size] = 0;
        return val;
    }

    public:

    std::string request_type_dist()
    {
        double u = uni_dist(generator);
        if (u <= get_rate){return "get";}
        else if (u > get_rate && u <= get_rate + set_rate) {return "set";}
        else {return "del";}
    }

    Impl(double set_del_ratio, unsigned int key_num)
    : uni_dist(0.0, 1.0), key_num(key_num)
    {
	    key_pool.reserve(key_num);

        set_del_ratio = std::clamp(set_del_ratio, double(0.0), double(1010101.0));
        del_rate = (1 - get_rate) / (1 + set_del_ratio);
        set_rate = set_del_ratio * del_rate;

        for (unsigned int i = 0; i < key_num; i++)
        {
            auto key_size = key_size_dist();
            auto key = random_key(key_size);
            key_pool.push_back(key);
        }
	
        for (unsigned int i = 0; i < temp_weights.size(); i++)
        {
            temp_weights.at(i) = pow(10, temp_weights.size() - 1 - i);
        }

        dd = std::discrete_distribution(temp_weights.begin(), temp_weights.end());

    }
    
    ~Impl(){}

    key_type prompt_key()
    {
        auto segment = dd(generator);
        double u = uni_dist(generator);
        size_t i = static_cast<size_t>(round(double(key_num) / double(temp_weights.size()) * (double(segment) + double(u))));
        if (! (i < key_num)) 
            i = key_num - 1;
//        std::cout << "i = " << i << std::endl;
        return key_pool.at(i);
    }

    const Cache::val_type prompt_val()
    {
        auto s = val_size_dist();
        return random_val(s);
    }

};

WorkloadGenerator::WorkloadGenerator(double set_del_ratio, unsigned int key_num)
{
    pImpl_ = std::unique_ptr<Impl>(new Impl(set_del_ratio, key_num));
}

WorkloadGenerator::~WorkloadGenerator() {}

std::string WorkloadGenerator::request_type_dist()
{
    return pImpl_->request_type_dist();
}

key_type WorkloadGenerator::random_key()
{
    return pImpl_->prompt_key();
}

const Cache::val_type WorkloadGenerator::random_val()
{
    return pImpl_->prompt_val();
}