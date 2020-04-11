// FIFO evictor
#pragma once

#include "evictor.hh"
#include <string>
#include <queue>

// Data type to use as keys for Cache and Evictors:
using key_type = std::string;

// Abstract base class to define evictions policies.
// It allows touching a key (on a set or get event), and request for
// eviction, which also deletes a key. There is no explicit deletion
// mechanism other than eviction.
class FIFO_Evictor : public Evictor{
    private:
    std::queue<key_type> * my_que;
    public:
    FIFO_Evictor();
    ~FIFO_Evictor();

    // Inform evictor that a certain key has been set or get:
    void touch_key(const key_type&);

    // Request evictor for the next key to evict, and remove it from evictor.
    // If evictor doesn't know what to evict, return an empty key ("").
    const key_type evict();
};