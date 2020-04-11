#include "evictor.hh"
#include "cache.hh"


class WorkloadGenerator
{
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
public:
    WorkloadGenerator(double set_del_ratio);
    ~WorkloadGenerator();

    Cache::size_type key_size_dist();

    Cache::size_type val_size_dist();

    std::string request_type_dist();

    key_type random_key(Cache::size_type size);

    const Cache::val_type random_val(Cache::size_type size);
};

