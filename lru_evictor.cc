#include "lru_evictor.hh"
#include <cassert>

class LRU_Evictor::Node
{
    public:
    Node * prev, *next;
    key_type key;
    Node(key_type my_key, Node* my_prev = nullptr, Node* my_next = nullptr)
    {
        key = my_key;
        prev = my_prev;
        next = my_next;
    }
    ~Node() {}
};

LRU_Evictor::LRU_Evictor(std::function<std::size_t(key_type)> hasher)
{
    table = new std::unordered_map <key_type, Node*, std::function<std::size_t(key_type)> > (10, hasher);
    head = nullptr;
    tail = nullptr;
}

LRU_Evictor::~LRU_Evictor() 
{
    for(auto iter = table->begin(); iter != table->end(); iter ++)
        delete iter->second;
    delete table;
}

void LRU_Evictor::touch_key(const key_type& my_key)
{
    auto iter = table->find(my_key);
    // If this is an insertion.
    if(iter == table->end())
    {
        Node* new_node = new Node(my_key);
        table->insert(std::pair<key_type, Node *> (my_key, new_node));
        //If the evictor is empty yet.
        if(tail == nullptr)
        {
            assert(head == nullptr);
            head = new_node;
            tail = new_node;
            head->next = nullptr;
            tail->prev = nullptr;
        }
        // one element special case.
        else if (tail == head)
        {
            tail = new_node;
            tail->prev = head;
            head->next = new_node;
            head->prev = nullptr;
        }
        else
        {            
            assert(head != nullptr);
            assert(tail->next == nullptr);
            tail->next = new_node;
            new_node->prev = tail;
            tail = new_node;
        }
    }
    else
    {
        Node * address = iter->second;
        assert(address != nullptr);
        // If this is the head, i.e. the least recently used element.
        if(address -> prev == nullptr)
        {
            assert(address == head);
            // If this is the only element in the list
            if(address == tail)
                return;
            assert(address->next != nullptr);
            assert(address->next->prev == head);
            head = address->next;
            address->next->prev = nullptr;
            assert(tail != nullptr);
            tail->next = address;
            address->prev = tail;
            address->next = nullptr;
            tail = address;
        }
        else if (address == tail)
        {
            return;
        }
        // If address is in the middle
        else
        {
            assert(address->prev != nullptr);
            assert(address->next != nullptr);
            address->prev->next = address->next;
            address->next->prev = address->prev;
            assert(tail != nullptr);
            tail->next = address;
            address->prev = tail;
            address->next = nullptr;
            tail = address;
        }
        
        
    }
}

// returns empty string if the queue is empty.
const key_type LRU_Evictor::evict()
{
    if(head == nullptr)
        return "";
    key_type return_key = head->key;
    Node * head_that_would_be_deleted = head;
    head = head->next;
    if(head != nullptr)
        head->prev = nullptr;
    //If this is the only element in the list.
    if(tail == head_that_would_be_deleted)
    {
        tail = nullptr;
        head = nullptr;
    }
    table->erase(head_that_would_be_deleted->key);
    delete head_that_would_be_deleted;
    return return_key;
}