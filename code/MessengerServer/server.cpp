#include <iostream>
#include <set>
#include <memory>
#include <string>
#include <utility>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

std::set<std::shared_ptr<websocket::stream<tcp::socket>>> clients;

class session : public std::enable_shared_from_this<session> {
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
public:
    session(tcp::socket socket) : ws_(std::move(socket)) {}
    void start() {
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            if (!ec) {
                std::cout << "Client connected!" << std::endl;
                clients.insert(std::shared_ptr<websocket::stream<tcp::socket>>(self, &self->ws_));
                self->read();
            }
            else {
                std::cerr << "Accept error: " << ec.message() << std::endl;
            }
            });
    }
private:
    void read() {
        ws_.async_read(buffer_, [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
            if (!ec) {
                std::string msg = beast::buffers_to_string(self -> buffer_.data());
                self -> buffer_.consume(self -> buffer_.size());
                self->broadcast(msg);
                self->read();
            }
            else {
                clients.erase(std::shared_ptr<websocket::stream<tcp::socket>>(self, &self->ws_));
                std::cerr << "Read error: " << ec.message() << std::endl;
            }
            });
    }
    void broadcast(const std::string& msg) {
        for (auto& client : clients) {
            client->async_write(
                net::buffer(msg),
                [](beast::error_code ec, std::size_t /*bytes*/) {
                    if (ec) {
                        std::cerr << "Write error: " << ec.message() << std::endl;
                    }
                }
            );
        }
    }
};

void do_listen(net::io_context& ioc, tcp::endpoint endpoint) {
    tcp::acceptor acceptor{ ioc, endpoint };
    while (true) {
        tcp::socket socket{ ioc };
        acceptor.accept(socket);
        auto sess = std::make_shared<session>(std::move(socket));
        sess->start();
    }
}

int main() {
    try {
        net::io_context ioc{ 1 };
        tcp::endpoint endpoint{ net::ip::make_address("0.0.0.0"), 8080 };
        do_listen(ioc, endpoint);
        ioc.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}