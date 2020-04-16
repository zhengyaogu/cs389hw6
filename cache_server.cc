#include "cache.hh"
#include "lru_evictor.hh"
#include "fifo_evictor.hh"
#include <unistd.h>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using json = nlohmann::json;
using size_type = uint64_t;
using port_type = unsigned short;


bool is_number(std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

bool is_ip(std::string& s)
{
    if (*s.begin() == '.' && *(s.end() - 1) == '.') {return false;}
    std::string::const_iterator it = s.begin();
    auto no_consec_dots = true;
    while (it != s.end() && (std::isdigit(*it) || (*it) == '.') && no_consec_dots) 
    {
        ++it;
        no_consec_dots = (*it == '.' && *(it - 1) != '.') || (*it) != '.';
    }
    return !s.empty() && it == s.end();
}

size_type to_number(std::string& s)
{
    return size_type(std::stoi(s));
}

bool set_params(size_type& maxmem, std::string& server, port_type& port, size_type& threads, int argc, char** argv)
{
    char opt;
    while ((opt = getopt(argc, argv, "m:p:s:t:")) != -1)
    {
        std::string param_str(optarg);

        switch(opt)
        {
            case 'm':
                if (is_number(param_str)) 
                {
                    auto param_uint = to_number(param_str);
                    maxmem = param_uint;
                }
                else
                {
                    std::cout << "Request failed: invalid parameter format for -m option" << std::endl;
                    return false;
                }
                break;
            
            case 'p':
                if (is_number(param_str)) 
                {
                    auto param_uint = to_number(param_str);
                    port = param_uint;
                }
                else
                {
                    std::cout << "Request failed: invalid parameter format for -p option" << std::endl;
                    return false;
                }
                break;

            case 's':
                if (is_ip(param_str))
                {
                    server = param_str;
                }
                else
                {
                    std::cout << "Request failed: invalid parameter format for -s option" << std::endl;
                    return false;
                }
                break;

            case 't':
                if (is_number(param_str)) 
                {
                    auto param_uint = to_number(param_str);
                    threads = param_uint;
                }
                else
                {
                    std::cout << "Request failed: invalid parameter format for -t option" << std::endl;
                    return false;
                }
                break;  
            
            case '?':
                std::cout << "Request failed: undefined option" << std::endl;
                return false;
        }

    }

    return true;
}


class http_connection : public std::enable_shared_from_this<http_connection>
{
public:
    http_connection(tcp::socket socket, Cache* cache)
        : socket_(std::move(socket))
    {
        cache_ = cache;
    }

    // Initiate the asynchronous operations associated with the connection.
    void
    start()
    {
        read_request();
        check_deadline();
    }

private:
    // The socket for the currently connected client.
    tcp::socket socket_;

    // The buffer for performing reads.
    beast::flat_buffer buffer_;

    // The request message.
    http::request<http::dynamic_body> request_;

    // The response message.
    http::response<http::dynamic_body> response_;

    // The timer for putting a deadline on connection processing.
    net::steady_timer deadline_{
        socket_.get_executor(), std::chrono::seconds(60)};

    Cache* cache_ = nullptr;

    void set_header(unsigned int code)
    {
        response_.version(11);
        response_.set(http::field::accept, "text/json");
        response_.set(http::field::content_type, "application/json");
        response_.set("Space-Used", cache_->space_used());
        response_.result(code);
    }

    // Asynchronously receive a complete request message.
    void
    read_request()
    {
        auto self = shared_from_this();

        http::async_read(
            socket_,
            buffer_,
            request_,
            [self](beast::error_code ec,
                std::size_t bytes_transferred)
            {
                boost::ignore_unused(bytes_transferred);
                if(!ec)
                    self->process_request();
            });
    }

    // Determine what needs to be done with the request message.
    void
    process_request()
    {
        switch(request_.method())
        {
        case http::verb::get:
            {
                auto key = static_cast<std::string>(request_.target());
                try
                {
                    if (key.at(0) != '/') 
                    {
                        response_.result(400);
                        break;
                    }
                }
                catch(int e)
                {
                    response_.result(400);
                    break;
                }
                key = key.substr(1);
//                std::cout << key << " " << key.size() << "\n";
                Cache::size_type size = 0;
                auto val = cache_->get(key, size);
                if (val == nullptr) 
                {
                    response_.result(404);
                    break;
                }
                set_header(200);
                create_response_get(key, val);
                break;
            }
        
        case http::verb::put:
            {auto key_value = static_cast<std::string>(request_.target());
            try
            {
                if (key_value.at(0) != '/') 
                {
                    set_header(400);
                    break;
                }
            }
            catch(int e)
            {
                set_header(400);
                break;
            }
            key_value = key_value.substr(1);

            auto delim_pos = key_value.find('/');
            if (delim_pos == std::string::npos) 
            {
                set_header(400);
                break;
            }
            auto key = key_value.substr(0, delim_pos);
//            std::cout << key << " " << key.size() << "\n";
            auto value = key_value.substr(delim_pos + 1);
//           std::cout << value << " " << value.size() << "\n";
            Cache::val_type val_ptr = value.c_str();

            cache_->set(key, val_ptr, value.size() + 1);

            set_header(200);

            break;}
        
        case http::verb::delete_:
            {auto key = static_cast<std::string>(request_.target());
            try
            {
                if (key.at(0) != '/') 
                {
                    set_header(400);
                    break;
                }
            }
            catch(int e)
            {
                set_header(400);
                break;
            }
            key = key.substr(1);
            cache_->del(key);
            set_header(200);
            break;}

        case http::verb::head:
            {
                set_header(200);
                break;
            }

        case http::verb::post:
            {auto target = static_cast<std::string>(request_.target());
            if (target.compare("/reset") != 0)
            {
                response_.result(400);
                break;
            }
            cache_->reset();
            set_header(200);
            break;}

        default:
            // We return responses indicating an error if
            // we do not recognize the request method.
            {
                response_.result(400);
                response_.set(http::field::content_type, "text/plain");
                beast::ostream(response_.body())
                                << std::string("Invalid request-method '") 
                                << std::string(request_.method_string()) 
                                << std::string("'");
                break;
            }
        }

        write_response();
    }

    void create_response_get(std::string key, Cache::val_type val)
    {
        json tuple;
        tuple["key"] = key;
        tuple["value"] = std::string(val);
        beast::ostream(response_.body())
            << (tuple.dump());
    }

    // Asynchronously transmit the response message.
    void
    write_response()
    {
        auto self = shared_from_this();

        http::async_write(
            socket_,
            response_,
            [self](beast::error_code ec, std::size_t)
            {
                self->socket_.shutdown(tcp::socket::shutdown_send, ec);
                self->deadline_.cancel();
            });
    }

    // Check whether we have spent enough time on this connection.
    void
    check_deadline()
    {
        auto self = shared_from_this();

        deadline_.async_wait(
            [self](beast::error_code ec)
            {
                if(!ec)
                {
                    // Close socket to cancel any outstanding operation.
                    self->socket_.close(ec);
                }
            });
    }

};

// "Loop" forever accepting new connections.
void
http_server(tcp::acceptor& acceptor, tcp::socket& socket, Cache* cache)
{
  acceptor.async_accept(socket,
      [&acceptor, &socket, cache](beast::error_code ec)
      {
          if(!ec)
              std::make_shared<http_connection>(std::move(socket), cache)->start();
          http_server(acceptor, socket, cache);
      });
}

int 
main(int argc, char* argv[])
{
    size_type maxmem = 300000;
    std::string address("127.0.0.1");
    port_type port = 10002;
    size_type threads = 1;

    auto request_success = set_params(maxmem, address, port, threads, argc, argv);
    if (!request_success) {return -1;}

    std::cout << std::endl;
    std::cout << "maxmem: " << maxmem << std::endl;
    std::cout << "server address: " << address << std::endl;
    std::cout << "port: " << port << std::endl;
    std::cout << "threads: " << threads << std::endl;

//    LRU_Evictor* evictor = new LRU_Evictor();
    LRU_Evictor* evictor = new LRU_Evictor();
//    Evictor* evictor = nullptr;
    Cache* cache = new Cache(maxmem, 0.75, evictor);

    
    try
    {
        std::cout << "cache address: " << cache << std::endl;
        auto const ip_address = net::ip::make_address(address);
        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {ip_address, port}};
        tcp::socket socket{ioc};
        http_server(acceptor, socket, cache);

        ioc.run();
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;

        delete evictor;
        delete cache;
    }
    
    delete evictor;
    delete cache;
}
