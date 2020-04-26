#include "cache.hh"
#include <unistd.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <nlohmann/json.hpp>
#include <typeinfo>

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>
using key_type = std::string;

class Cache::Impl
{
    private:
    const int version = 11;
    std::string host;
    std::string port;
    
    public:
    Impl(std::string h, std::string p)
    {
        host = h;
        port = p;

    }

    ~Impl()
    {
    }

    http::response<http::dynamic_body> connet(http::verb verb, std::string target) const
    {    
        // The io_context is required for all I/O
        net::io_context ioc;

        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);

        // Look up the domain name
        auto const results = resolver.resolve(host, port);

        // Make the connection on the IP address we get from a lookup
        stream.connect(results);

        // Set up an HTTP  request message

        http::request<http::string_body> req{verb, target, version};
        
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);


        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

//       std::cout << "1\n";

        // Declare a container to hold the response
        http::response<http::dynamic_body> res;

//        std::cout << "2\n";

        // Receive the HTTP response
        http::read(stream, buffer, res);

//        std::cout << "3\n";
        
        // Gracefully close the socket
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        return res;
    }

    void set(key_type key, Cache::val_type val, Cache::size_type size)
    {
        std::string s(val, size - 1);
        // Set up target string
        std::string target = "/" + key + "/" + s;
//        std::cout << target << "\n";
//        std::cout << target.compare("/1/2") << "\n";

//        std::cout << connet(http::verb::put, target) << std::endl;
        connet(http::verb::put, target);
    }

    Cache::val_type get(key_type key, Cache::size_type& val_size) const
    {        
//        std::cout << key << " " << key.size() << "\n";
        // Set up target string
        std::string target = "/" + key;
//        std::cout << target << " " << target.size() << "\n";
//        std::cout << target.compare("/1") << "\n";

        auto res = connet(http::verb::get, target);

        std::string ret = beast::buffers_to_string(res.body().data());

//        std::cout << ret << std::endl;
        if(ret[0] == '{')
        {
            int x = ret.find("value\":\"");
            int y = ret.find("\"}");
            auto value = ret.substr(x + 8, y - x - 8);
            val_size = y - x - 8 + 1;
//            std::cout << ret << std::endl;
//            std::cout << "x = " << x << ", y = " << y << ", val_size = " << val_size << std::endl;
//            std::cout << value << " " << val_size << "\n";
            char* val = new char[val_size];
            for(unsigned int i = 0; i < val_size - 1; i ++)
                val[i] = value[i];
            val[val_size - 1] = 0;
            return val;
        }
        val_size = 0;
        return nullptr;
    }

    bool del(key_type key)
    {
        std::string target = "/" + key;

        connet(http::verb::delete_, target);

        // Since the server doesn't pass any information about the deletion, we just assume false.
        return false;
    }

    Cache::size_type get_current_mem() const
    {
        auto res = connet(http::verb::head, "/");
        std::stringstream buffer;
        buffer << res.base() << std::endl;
        std::string str = buffer.str();
//       std::cout << str << "\n";
        int pos = str.find("Space-Used: ");
        std::string space_used = str.substr(pos + 12);
//        std::cout << space_used << "\n";
        int ret = stoi(space_used);
//        std::cout << ret << "\n";
        return ret;
    }

    // delete everything that is in Impl
    void reset()
    {
        // Set up target string
        std::string target = "/reset";

        // Write the message to standard out
        std::cout << connet(http::verb::post, target) << std::endl;
    }
};

Cache::Cache(size_type maxmem,
        float max_load_factor,
        Evictor* evictor,
        hash_func hasher)
{
    // This should never happen
    assert(false);
}

Cache::Cache(std::string host, std::string port)
{
    pImpl_ = std::unique_ptr<Impl>(new Impl(host, port));
}

Cache::~Cache()
{
    // delete this unique ptr
    pImpl_.reset();
}


void Cache::set(key_type key, Cache::val_type val, Cache::size_type size)
{
    pImpl_->set(key, val, size);
}

Cache::val_type Cache::get(key_type key, Cache::size_type& val_size) const
{
    return pImpl_->get(key, val_size);
}

bool Cache::del(key_type key)
{
    return pImpl_->del(key);
}

Cache::size_type Cache::space_used() const
{
    return pImpl_->get_current_mem();
}


void Cache::reset()
{
    pImpl_->reset();
}
/*
int main()
{
    Cache* cache = new Cache("127.0.0.1", "10002");
    
    std::cout << "set started" << std::endl;

    cache->set("4", "2", 2);

    std::cout << "set ended" << std::endl;

    std::cout << "set started" << std::endl;

    cache->set("2", "3", 2);

    std::cout << "set ended" << std::endl;
    
    Cache::size_type _;
    
    std::cout << "get started" << std::endl;

    assert(cache->get("1", _) == nullptr);

    std::cout << "get ended" << std::endl;
    
    std::cout << "get started" << std::endl;

    assert(cache->get("2", _)[0] == '3');

    std::cout << "get ended" << std::endl;

    std::cout << "delete started" << std::endl;

    cache->del("1");

    std::cout << "delete ended" << std::endl;
    
    std::cout << "space check started" << std::endl;

    std::cout << cache->space_used() << std::endl;

    std::cout << "space check ended" << std::endl;

    delete cache;
    return 0;
}
*/