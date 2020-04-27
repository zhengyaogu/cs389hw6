#include "cache.hh"
#include <unordered_map>
#include <utility>
#include <cassert>
#include <iostream>
#include <mutex>

std::mutex mutex;

class Cache::Impl
{
    using pair_type = std::pair<Cache::val_type, Cache::size_type>;

    private:
    Cache::size_type my_maxmem, current_mem;
    std::unordered_map < key_type, pair_type, std::function<std::size_t(key_type)>> * table = nullptr;
    float my_max_load_factor = 0.0;
    Evictor * my_evictor = nullptr;

    public:
    Impl(Cache::size_type maxmem,
        float max_load_factor,
        Evictor* evictor,
        hash_func hasher)
    {
        table = new std::unordered_map < key_type, pair_type, std::function<std::size_t(key_type)> > (10, hasher);
        table->max_load_factor(max_load_factor);
        my_maxmem = maxmem;
        current_mem = 0;
        my_max_load_factor = max_load_factor;
        my_evictor = evictor;
    }

    ~Impl()
    {
        for (auto iter = table->begin(); iter != table->end(); iter ++)
            delete[] iter->second.first;
        table->clear();
        delete table;
    }

    //free up the memory iter.val points to and get rid of iter in the map
    void free(std::unordered_map<key_type, pair_type>::const_iterator iter)
    {
        assert(current_mem >= iter->second.second);
        current_mem -= iter->second.second;
        delete[] iter->second.first;
        table->erase(iter);
    }

    void set(key_type key, Cache::val_type val, Cache::size_type size)
    {
        // first copy the val, doesn't need locking
        Cache::byte_type* copied_val = new byte_type[size];
        for (unsigned int i = 0; i < size; ++i) 
        {
            copied_val[i] = val[i];
        }

            int size_change = 0;
            // Find if element exists first
            auto iter = table->find(key);
            // If doesn't exist
            if(iter == table->end())
                size_change = size;
            else
                size_change = int(size) - int(iter->second.second);
            assert(current_mem <= my_maxmem);
            if(size_change > int(my_maxmem))
            {
                std::cout << "Insertion <" << key << ", " << val << ", " << size << "> failed: not enough space\n";
                return;
            }
            // if there is no evictor and the insertion will exceed maxmem, return
            if (!my_evictor && int(current_mem) + size_change > int(my_maxmem))
            {
                std::cout << "Insertion <" << key << ", " << val << ", " << size << "> failed: no evictor to make space\n";
                return;
            }
            while(int(current_mem) + size_change > int(my_maxmem))
            {
                key_type eviction_key = my_evictor->evict();
                assert(eviction_key != "");
                auto eviction_address = table->find(eviction_key);
                if(eviction_address == table->end())
                    continue;
                if(eviction_address->first == key)
                    size_change = size;
                free(eviction_address);
            }
            // If already exist, delete first, then insert.
            iter = table->find(key);
            if(iter != table->end())
                free(iter);
            // Insert.
            table->insert(std::pair<key_type, pair_type> (key, pair_type (copied_val, size)));
            current_mem += size;
            assert(table->max_load_factor() <= my_max_load_factor);
            assert(current_mem <= my_maxmem);

            if (my_evictor) {my_evictor->touch_key(key);}
        
        
    }

    Cache::val_type get(key_type key, Cache::size_type& val_size) const
    {
        auto iter = table->find(key);
        // If doesn't exist
        if(iter == table->end())
        {
            val_size = 0;
            return nullptr;
        }
        val_size = iter->second.second;

        if (my_evictor) {my_evictor->touch_key(key);}
        return iter->second.first;
    }

    bool del(key_type key)
    {
        auto iter = table->find(key);
        if(iter == table->end())
            return false;
        free(iter);
        assert(table->max_load_factor() <= my_max_load_factor);
        return true;
    }

    Cache::size_type get_current_mem() const
    {
        return current_mem;
    }

    // delete everything that is in Impl
    void reset()
    {
        while(!table->empty())
            free(table->cbegin());
        table->clear();
        current_mem = 0;
        std::string evicted = "1";
        while(evicted != "" && my_evictor != nullptr)
            evicted = my_evictor->evict();
    }
};

Cache::Cache(size_type maxmem,
        float max_load_factor,
        Evictor* evictor,
        hash_func hasher)
{
    pImpl_ = std::unique_ptr<Impl>(new Impl(maxmem, max_load_factor, evictor, hasher));
}

Cache::Cache(std::string host, std::string port)
{ }


Cache::~Cache()
{
    {
    std::lock_guard guard(mutex);
    pImpl_->reset();
    }
}


void Cache::set(key_type key, Cache::val_type val, Cache::size_type size)
{
    {
    std::lock_guard guard(mutex);
    pImpl_->set(key, val, size);
    }
}

Cache::val_type Cache::get(key_type key, Cache::size_type& val_size) const
{
    {
    std::lock_guard guard(mutex);
    return pImpl_->get(key, val_size);
    }
}

bool Cache::del(key_type key)
{
    {
    std::lock_guard guard(mutex);
    return pImpl_->del(key);
    }
}

Cache::size_type Cache::space_used() const
{
    {
    std::lock_guard guard(mutex);
    return pImpl_->get_current_mem();
    }
}


void Cache::reset()
{
    {
    std::lock_guard guard(mutex);
    pImpl_->reset();
    }
}