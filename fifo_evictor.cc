#include "fifo_evictor.hh"

FIFO_Evictor::FIFO_Evictor()
{
    my_que = new std::queue<key_type>;
}

FIFO_Evictor::~FIFO_Evictor() 
{
    while(!my_que->empty()) my_que->pop();
    delete my_que;
}

void FIFO_Evictor::touch_key(const key_type& my_key)
{
    my_que->push(my_key);
}

// returns empty string if the queue is empty.
const key_type FIFO_Evictor::evict()
{
    if(!my_que->empty())
    {
        key_type my_key = my_que->front();
        my_que->pop();
        return my_key;
    }
    return "";
}