#include "workload_generator.hh"
#include <math.h>
#include <algorithm>
#include "cache.hh"
#include "evictor.hh"
#include <stdlib.h> 
#include <random>
#include <iostream>

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

    public:
    
    Impl(double set_del_ratio)
    : uni_dist(0.0, 1.0)
    {
        set_del_ratio = std::clamp(set_del_ratio, double(0.0), double(1.0));
        del_rate = (1 - get_rate) / (1 + set_del_ratio);
        set_rate = set_del_ratio * del_rate;
    }
    
    ~Impl(){}

    Cache::size_type key_size_dist()
    {
        double u = uni_dist(generator);
        while (u == 0) {u = uni_dist(generator);}
        double x = (sigma_k / k_k) * (pow(-log(u), -k_k) - 1) + mu_k;
        return static_cast<Cache::size_type>(round(x));
    }

    Cache::size_type val_size_dist()
    {
        double u = uni_dist(generator);
        double x = (sigma_v / k_v) * (pow(1 - u, -k_v) - 1) + theta_v;
        return static_cast<Cache::size_type>(round(x));
    }

    std::string request_type_dist()
    {
        double u = uni_dist(generator);
        if (u <= get_rate){return "get";}
        else if (u > get_rate && u <= get_rate + set_rate) {return "set";}
        else {return "del";}
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

};

WorkloadGenerator::WorkloadGenerator(double set_del_ratio)
{
    pImpl_ = std::unique_ptr<Impl>(new Impl(set_del_ratio));
}

WorkloadGenerator::~WorkloadGenerator() {}

Cache::size_type WorkloadGenerator::key_size_dist()
{
    return pImpl_->key_size_dist();
}

Cache::size_type WorkloadGenerator::val_size_dist()
{
    return pImpl_->val_size_dist();
}

std::string WorkloadGenerator::request_type_dist()
{
    return pImpl_->request_type_dist();
}

key_type WorkloadGenerator::random_key(unsigned int size)
{
    return pImpl_->random_key(size);
}

const Cache::val_type WorkloadGenerator::random_val(unsigned int size)
{
    return pImpl_->random_val(size);
}

