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

class session; // Предварительное объявление
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
                std::cout << "Sending WebSocket response headers..." << std::endl;
            }));
        ws_.set_option(websocket::stream_base::timeout{
            std::chrono::seconds(5), // handshake timeout
            std::chrono::seconds(30), // idle timeout
            false // keep-alive pings
            });
        try {
            ws_.accept();
            std::cout << "Client connected via WebSocket!" << std::endl;
            clients.insert(shared_from_this());
            read();
        }
        catch (const beast::system_error& e) {
            std::cerr << "Accept error: " << e.what() << " (code: " << e.code().value() << ")" << std::endl;
            read_http_request();
        }
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
        std::cout << "Starting read..." << std::endl;
        try {
            // Пробуем синхронный read для теста
            std::size_t bytes = ws_.read(buffer_);
            std::cout << "Read completed, bytes: " << bytes << std::endl;
            std::string msg = beast::buffers_to_string(buffer_.data());
            std::cout << "Received message: " << msg << " (" << bytes << " bytes)" << std::endl;
            buffer_.consume(buffer_.size());
            broadcast(msg);
            read(); // Продолжаем читать
        }
        catch (const beast::system_error& e) {
            std::cerr << "Read error: " << e.what() << " (code: " << e.code().value() << ")" << std::endl;
            clients.erase(shared_from_this());
            ws_.close(websocket::close_code::normal);
        }
    }
    void broadcast(const std::string& msg) {
        std::cout << "Broadcasting message: " << msg << " to " << clients.size() << " clients" << std::endl;
        for (const auto& client : clients) {
            try {
                client->ws_.write(net::buffer(msg));
                std::cout << "Wrote " << msg.size() << " bytes to client" << std::endl;
            }
            catch (const beast::system_error& e) {
                std::cerr << "Write error: " << e.what() << " (code: " << e.code().value() << ")" << std::endl;
            }
        }
    }
};

void do_listen(net::io_context& ioc, tcp::endpoint endpoint) {
    std::cout << "Listening for connections on " << endpoint << "..." << std::endl;
    tcp::acceptor acceptor{ ioc, endpoint };
    while (true) {
        tcp::socket socket{ ioc };
        std::cout << "Waiting for new client..." << std::endl;
        acceptor.accept(socket);
        std::cout << "New client accepted" << std::endl;
        auto sess = std::make_shared<session>(std::move(socket));
        sess->start();
    }
}

int main() {
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