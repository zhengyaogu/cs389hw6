#include "evictor.hh"
#include "cache.hh"


class WorkloadGenerator
{
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
public:
    WorkloadGenerator(double set_del_ratio, unsigned int key_num);
    ~WorkloadGenerator();

    std::string request_type_dist();

    key_type random_key();

    const Cache::val_type random_val();
};
