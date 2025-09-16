#include <iostream>
#include <set>
#include <memory>
#include <string>
#include <utility>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class session; 
std::set<std::shared_ptr<session>> clients;



class session : public std::enable_shared_from_this<session> {
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
public:
    session(tcp::socket socket) : ws_(std::move(socket)) {
        std::cout << "Session created" << std::endl;
    }
    void start() {
        std::cout << "Starting WebSocket handshake..." << std::endl;
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                res.set(http::field::server, "Messenger-WebSocket-Server");
                std::cout << "Sending WebSocket response headers: " << res << std::endl;
            }));
        ws_.set_option(websocket::stream_base::timeout{
            std::chrono::seconds(10), 
            std::chrono::seconds(60), 
            true 
            });
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            if (!ec) {
                std::cout << "Client connected via WebSocket!" << std::endl;
                clients.insert(self);
                self->read();
            }
            else {
                std::cerr << "Async accept error: " << ec.message() << " (code: " << ec.value() << ")" << std::endl;
                self->read_http_request();
            }
            });
    }
private:
    void read_http_request() {
        try {
            http::request<http::string_body> req;
            http::read(ws_.next_layer(), buffer_, req);
            std::cout << "Received HTTP request: " << req.method_string() << " " << req.target() << " HTTP/" << req.version() / 10 << "." << req.version() % 10 << std::endl;
            for (const auto& field : req) {
                std::cout << field.name_string() << ": " << field.value() << std::endl;
            }
            http::response<http::empty_body> res{ http::status::not_found, 11 };
            res.set(http::field::server, "Messenger-WebSocket-Server");
            res.prepare_payload();
            http::write(ws_.next_layer(), res);
        }
        catch (const beast::system_error& e) {
            std::cerr << "HTTP read/write error: " << e.what() << " (code: " << e.code().value() << ")" << std::endl;
        }
    }
    void read() {
        std::cout << "Starting async_read..." << std::endl;
        ws_.async_read(buffer_, [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
            std::cout << "Async read callback invoked" << std::endl;
            if (!ec) {
                std::cout << "Read completed, bytes: " << bytes << std::endl;
                std::string msg = beast::buffers_to_string(self->buffer_.data());
                std::cout << "Received message: " << msg << " (" << bytes << " bytes)" << std::endl;
                self->buffer_.consume(self->buffer_.size());
                self->broadcast(msg);
                self->read();
            }
            else {
                std::cerr << "Read error: " << ec.message() << " (code: " << ec.value() << ")" << std::endl;
                clients.erase(self);
                self->ws_.async_close(websocket::close_code::normal, [](beast::error_code ec) {
                    if (ec) {
                        std::cerr << "Close error: " << ec.message() << " (code: " << ec.value() << ")" << std::endl;
                    }
                    });
            }
            });
    }
    void broadcast(const std::string& msg) {
        std::cout << "Broadcasting message: " << msg << " to " << clients.size() << " clients" << std::endl;
        for (const auto& client : clients) {
            auto msg_copy = std::make_shared<std::string>(msg);
            client->ws_.async_write(
                net::buffer(*msg_copy),
                [msg_copy](beast::error_code ec, std::size_t bytes) {
                    if (ec) {
                        std::cerr << "Write error: " << ec.message() << " (code: " << ec.value() << ")" << std::endl;
                    }
                    else {
                        std::cout << "Wrote " << bytes << " bytes for message: " << *msg_copy << std::endl;
                    }
                }
            );
        }
    }
};

class listener : public std::enable_shared_from_this<listener> {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
public:
    listener(net::io_context& ioc, tcp::endpoint endpoint) : ioc_(ioc), acceptor_(ioc, endpoint) {
        std::cout << "Listener created for endpoint " << endpoint << std::endl;
    }
    void start() {
        accept();
    }
private:
    void accept() {
        acceptor_.async_accept(ioc_, [self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::cout << "New client accepted" << std::endl;
                auto sess = std::make_shared<session>(std::move(socket));
                sess->start();
            }
            else {
                std::cerr << "Accept error: " << ec.message() << " (code: " << ec.value() << ")" << std::endl;
            }
            self->accept(); 
            });
    }
};

void do_listen(net::io_context& ioc, tcp::endpoint endpoint) {
    std::cout << "Listening for connections on " << endpoint << "..." << std::endl;
    auto l = std::make_shared<listener>(ioc, endpoint);
    l->start();
}

int main() {
	setlocale(LC_ALL, "ru_RU.UTF-8");
    try {
        std::cout << "Server starting on port 8080..." << std::endl;
        net::io_context ioc{ 1 };
        tcp::endpoint endpoint{ net::ip::make_address("0.0.0.0"), 8080 };
        do_listen(ioc, endpoint);
        std::cout << "Running io_context..." << std::endl;
        ioc.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
        return 1;
    }
    return 0;
}