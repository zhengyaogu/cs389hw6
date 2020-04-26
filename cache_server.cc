#include "cache.hh"
#include "lru_evictor.hh"
#include "fifo_evictor.hh"
#include <unistd.h>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>
#include <functional>
#include <vector>
#include <algorithm>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using json = nlohmann::json;
using size_type = uint64_t;
using port_type = unsigned short;

void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

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

bool set_params(size_type& maxmem, std::string& server, port_type& port, int& threads, int argc, char** argv)
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



// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<
    class Body, class Allocator,
    class Send>
void
handle_request(
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send,
    Cache* cache_)
{
    // Returns a bad request response
    auto const bad_request =
    [&req](beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found =
    [&req](beast::string_view target)
    {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;

    };

    auto const set_header = 
    [cache_] (http::response<http::string_body>& response_, unsigned int code)
    {
        response_.version(11);
        response_.set(http::field::accept, "text/json");
        response_.set(http::field::content_type, "application/json");
        response_.set("Space-Used", cache_->space_used());
        response_.result(code);
    };

    switch(req.method())
        {
        case http::verb::get:
            {
                http::response<http::string_body> res;
                auto key = static_cast<std::string>(req.target());
                try
                {
                    if (key.at(0) != '/') 
                    {
                        return send(bad_request("bad target delimitation"));
                    }
                }
                catch(int e)
                {
                    return send(bad_request("bad target delimitation"));
                }
                key = key.substr(1);
        //                std::cout << key << " " << key.size() << "\n";
                Cache::size_type size = 0;
                auto val = cache_->get(key, size);
                if (val == nullptr) 
                {
                    return send(not_found(key));
                }
                
                json tuple;
                tuple["key"] = key;
                tuple["value"] = std::string(val);
                res.body() = tuple.dump();
                return send(std::move(res));
                break;
            }

        case http::verb::put:
            {
                    http::response<http::string_body> res;
                    auto key_value = static_cast<std::string>(req.target());
                    try
                    {
                        if (key_value.at(0) != '/') 
                        {
                            return send(bad_request("bad target delimitation"));
                        }
                    }
                    catch(int e)
                    {
                        return send(bad_request("bad target delimitation"));
                    }
                    key_value = key_value.substr(1);

                    auto delim_pos = key_value.find('/');
                    if (delim_pos == std::string::npos) 
                    {
                        return send(bad_request("bad target delimitation"));
                    }
                    auto key = key_value.substr(0, delim_pos);
                //            std::cout << key << " " << key.size() << "\n";
                    auto value = key_value.substr(delim_pos + 1);
                //           std::cout << value << " " << value.size() << "\n";
                    Cache::val_type val_ptr = value.c_str();

                    cache_->set(key, val_ptr, value.size() + 1);

                    set_header(res, 200);
                    return send(std::move(res));

                    break;
            }

        case http::verb::delete_:
            {
            http::response<http::string_body> res;
            auto key = static_cast<std::string>(req.target());
            try
            {
                if (key.at(0) != '/') 
                {
                    return send(bad_request("bad target delimitation"));
                    break;
                }
            }
            catch(int e)
            {
                send(bad_request("bad target delimitation"));
                break;
            }
            key = key.substr(1);
            cache_->del(key);
            set_header(res, 200);
            return send(std::move(res));
            break;}

        case http::verb::head:
            {
                http::response<http::string_body> res;
                set_header(res, 200);
                return send(std::move(res));
                break;
            }

        case http::verb::post:
            {
            http::response<http::string_body> res;
            auto target = static_cast<std::string>(req.target());
            if (target.compare("/reset") != 0)
            {
                return send(bad_request("bad target delimitation"));
                break;
            }
            cache_->reset();
            set_header(res, 200);
            return send(std::move(res));
            break;}

        default:
            // We return responses indicating an error if
            // we do not recognize the request method.
            {
                    send(bad_request("request method undefined"));
            }
    }
}


// Handles an HTTP server connection
class session : public std::enable_shared_from_this<session>
{
    // This is the C++11 equivalent of a generic lambda.
    // The function object is used to send an HTTP message.
    struct send_lambda
    {
        session& self_;

        explicit
        send_lambda(session& self)
            : self_(self)
        {
        }

        template<bool isRequest, class Body, class Fields> void 
        operator()(http::message<isRequest, Body, Fields>&& msg) const
        {
            // The lifetime of the message has to extend
            // for the duration of the async operation so
            // we use a shared_ptr to manage it.
            auto sp = std::make_shared<
                http::message<isRequest, Body, Fields>>(std::move(msg));

            // Store a type-erased version of the shared
            // pointer in the class to keep it alive.
            self_.res_ = sp;

            // Write the response
            http::async_write(
                self_.stream_,
                *sp,
                beast::bind_front_handler(
                    &session::on_write,
                    self_.shared_from_this(),
                    sp->need_eof()));
        }
    };

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;
    send_lambda lambda_;
    Cache* cache_;

public:
    // Take ownership of the stream
    session(tcp::socket&& socket, Cache* cache)
        : stream_(std::move(socket))
        , lambda_(*this)
        , cache_(cache)
    {
    }

    // Start the asynchronous operation
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(stream_.get_executor(),
                      beast::bind_front_handler(
                          &session::do_read,
                          shared_from_this()));
    }

    void
    do_read()
    {
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(stream_, buffer_, req_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void
    on_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream)
            return do_close();

        if(ec)
            return fail(ec, "read");

        // Send the response
        handle_request(std::move(req_), lambda_, cache_);
    }

    void
    on_write(
        bool close,
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");

        if(close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return do_close();
        }

        // We're done with the response so delete it
        res_ = nullptr;

        // Read another request
        do_read();
    }

    void
    do_close()
    {
        // Send a TCP shutdown
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

/*
class http_connection : public std::enable_shared_from_this<http_connection>
{
private:
    // The socket for the currently connected client.
    tcp::socket socket_;

    beast::tcp_stream stream_;

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

    void run()
    {
        read_request();
        check_deadline();
    }

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

public:
    http_connection(tcp::socket socket, Cache* cache)
        : socket_(std::move(socket)),
        stream_(std::move(socket_))
    {
        cache_ = cache;
    }

    // Initiate the asynchronous operations associated with the connection.
    void
    start()
    {
        net::dispatch(stream_.get_executor(),
                      beast::bind_front_handler(
                          &http_connection::run,
                          shared_from_this()));
    }



};
*/
/*
// "Loop" forever accepting new connections.
void
http_server(tcp::acceptor& acceptor, tcp::socket& socket, Cache* cache)
{
  acceptor.async_accept(net::make_strand(socket.get_executor()),
      [&acceptor, cache](beast::error_code ec, tcp::socket socket)
      {
          if(!ec)
              std::make_shared<http_connection>(std::move(socket), cache)->start();
          http_server(acceptor, socket, cache);
      });
}
*/

class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    Cache* cache_;

public:
    listener(
        net::io_context& ioc,
        tcp::endpoint endpoint,
        Cache* cache)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , cache_(cache)
    {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if(ec)
        {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if(ec)
        {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if(ec)
        {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(
                    net::socket_base::max_listen_connections, ec);
                if(ec)
        {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void
    run()
    {
        do_accept();
    }

private:
    void
    do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &listener::on_accept,
                shared_from_this()));
    }

    void
    on_accept(beast::error_code ec, tcp::socket socket)
    {
        if(ec)
        {
            fail(ec, "accept");
        }
        else
        {
            // Create the session and run it
            std::make_shared<session>(
                std::move(socket),
                cache_)->run();
        }

        // Accept another connection
        do_accept();
    }
};


int 
main(int argc, char* argv[])
{
    size_type maxmem = 300000;
    std::string address("127.0.0.1");
    port_type port = 10002;
    int threads = 1;

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
        net::io_context ioc{threads};
        
        // Create and launch a listening port
        std::make_shared<listener>(
            ioc,
            tcp::endpoint{ip_address, port},
            cache)->run();
        // Run the I/O service on the requested number of threads
        std::vector<std::thread> v;
        v.reserve(threads - 1);
        for(auto i = threads - 1; i > 0; --i)
            v.emplace_back(
            [&ioc]
            {
                ioc.run();
            });
        ioc.run();

    
        delete evictor;
        delete cache;

        return EXIT_SUCCESS;
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;

        delete evictor;
        delete cache;

        return EXIT_FAILURE;
    }
}
