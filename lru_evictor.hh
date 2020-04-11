// LRU evictor
#pragma once

#include "evictor.hh"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

// Data type to use as keys for Cache and Evictors:
using key_type = std::string;

// Abstract base class to define evictions policies.
// It allows touching a key (on a set or get event), and request for
// eviction, which also deletes a key. There is no explicit deletion
// mechanism other than eviction.
class LRU_Evictor : public Evictor{
    private:
    // class to create doubly linked list.
    class Node;
    // Unordered map as a hash table to store the address of the nodes in doubly linked list.
    std::unordered_map < key_type, Node*, std::function<std::size_t(key_type)>> * table = nullptr;
    // The first and last elements in the doubly linked list.
    Node * head, * tail;
    public:
    LRU_Evictor(std::function<std::size_t(key_type)> hasher = std::hash<key_type>());
    ~LRU_Evictor();

    // Inform evictor that a certain key has been set or get:
    void touch_key(const key_type&);

    // Request evictor for the next key to evict, and remove it from evictor.
    // If evictor doesn't know what to evict, return an empty key ("").
    const key_type evict();
};