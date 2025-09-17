#include <iostream>
#include <set>
#include <memory>
#include <string>
#include <utility>
#include <queue>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <sqlite3.h>

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
    std::string user_login_;
    sqlite3* db_;
    std::queue<std::string> write_queue_;
    bool is_writing_ = false;

public:
    session(tcp::socket socket, sqlite3* db) : ws_(std::move(socket)), db_(db) {
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
            std::cout << "Received HTTP request: " << req.method_string() << " " << req.target() << std::endl;
            http::response<http::empty_body> res{ http::status::not_found, 11 };
            res.set(http::field::server, "Messenger-WebSocket-Server");
            res.prepare_payload();
            http::write(ws_.next_layer(), res);
        }
        catch (const beast::system_error& e) {
            std::cerr << "HTTP read/write error: " << e.what() << " (code: " << e.code().value() << ")" << std::endl;
        }
    }

    bool register_user(const std::string& login, const std::string& password) {
        std::string hashed_password = hash_password(password);
        std::string sql = "INSERT INTO users (login, password) VALUES (?, ?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "SQL prepare error: " << sqlite3_errmsg(db_) << std::endl;
            return false;
        }
        sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, hashed_password.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::string err_msg = sqlite3_errmsg(db_);
            std::cerr << "SQL insert error: " << err_msg << " (code: " << rc << ")" << std::endl;
            sqlite3_finalize(stmt);
            if (rc == SQLITE_CONSTRAINT) {
                write_message("System: Registration failed - login already exists");
                return false;
            }
            return false;
        }
        sqlite3_finalize(stmt);
        std::cout << "User registered: " << login << std::endl;
        return true;
    }

    bool authenticate_user(const std::string& login, const std::string& password) {
        std::string hashed_password = hash_password(password);
        std::string sql = "SELECT password FROM users WHERE login = ?;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "SQL prepare error: " << sqlite3_errmsg(db_) << std::endl;
            return false;
        }
        sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            std::string stored_password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            sqlite3_finalize(stmt);
            if (stored_password == hashed_password) {
                std::cout << "User authenticated: " << login << std::endl;
                return true;
            }
            else {
                std::cerr << "Authentication failed: incorrect password for " << login << std::endl;
                return false;
            }
        }
        sqlite3_finalize(stmt);
        std::cerr << "Authentication failed: user " << login << " not found" << std::endl;
        return false;
    }

    bool save_message(const std::string& user, const std::string& content) {
        std::string sql = "INSERT INTO messages (user, content, type) VALUES (?, ?, 'text');";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "SQL prepare error (messages): " << sqlite3_errmsg(db_) << std::endl;
            return false;
        }
        sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, content.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "SQL insert error (messages): " << sqlite3_errmsg(db_) << " (code: " << rc << ")" << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        std::cout << "Message saved: " << user << ": " << content << std::endl;
        return true;
    }

    std::string hash_password(const std::string& password) {
        // Заглушка для хэширования
        return password + "_hashed"; // В будущем замени на SHA-256 или bcrypt
    }

    void write_message(const std::string& message) {
        write_queue_.push(message);
        if (!is_writing_) {
            do_write();
        }
    }

    void do_write() {
        if (write_queue_.empty()) {
            is_writing_ = false;
            return;
        }
        is_writing_ = true;
        auto msg = std::make_shared<std::string>(write_queue_.front());
        ws_.async_write(net::buffer(*msg),
            [self = shared_from_this(), msg](beast::error_code ec, std::size_t bytes) {
                if (ec) {
                    std::cerr << "Write error: " << ec.message() << " (code: " << ec.value() << ")" << std::endl;
                }
                else {
                    std::cout << "Wrote " << bytes << " bytes for message: " << *msg << std::endl;
                }
                self->write_queue_.pop();
                self->do_write();
            });
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

                if (msg.find("register:") == 0) {
                    auto pos = msg.find(":", 9);
                    if (pos == std::string::npos) {
                        self->write_message("System: Invalid registration format");
                        self->read();
                        return;
                    }
                    std::string login = msg.substr(9, pos - 9);
                    std::string password = msg.substr(pos + 1);
                    if (self->register_user(login, password)) {
                        self->write_message("System: Registration successful");
                    }
                }
                else if (msg.find("login:") == 0) {
                    auto pos = msg.find(":", 6);
                    if (pos == std::string::npos) {
                        self->write_message("System: Invalid login format");
                        self->read();
                        return;
                    }
                    std::string login = msg.substr(6, pos - 6);
                    std::string password = msg.substr(pos + 1);
                    if (self->authenticate_user(login, password)) {
                        self->user_login_ = login;
                        clients.insert(self);
                        self->write_message("System: Login successful");
                        self->broadcast("System: " + login + " joined the chat");
                    }
                    else {
                        self->write_message("System: Login failed");
                    }
                }
                else if (msg.find("logout:") == 0) {
                    std::string login = msg.substr(7);
                    if (self->user_login_ == login) {
                        self->broadcast("System: " + login + " left the chat");
                        clients.erase(self);
                        self->user_login_.clear();
                        self->write_message("System: Logout successful");
                        auto timer = std::make_shared<net::steady_timer>(self->ws_.get_executor());
                        timer->expires_after(std::chrono::milliseconds(50));
                        timer->async_wait([self, timer](beast::error_code ec) {
                            if (!ec) {
                                self->ws_.async_close(websocket::close_code::normal, [self](beast::error_code ec) {
                                    if (ec) {
                                        std::cerr << "Close error: " << ec.message() << std::endl;
                                    }
                                    });
                            }
                            });
                    }
                    else {
                        self->write_message("System: Logout failed - invalid user");
                    }
                }
                else if (!self->user_login_.empty()) {
                    auto pos = msg.find(": ");
                    if (pos != std::string::npos) {
                        std::string user = msg.substr(0, pos);
                        std::string content = msg.substr(pos + 2);
                        self->save_message(user, content);
                    }
                    self->broadcast(msg);
                }
                else {
                    self->write_message("System: Please login first");
                }
                self->read();
            }
            else {
                std::cerr << "Read error: " << ec.message() << " (code: " << ec.value() << ")" << std::endl;
                clients.erase(self);
                self->ws_.async_close(websocket::close_code::normal, [](beast::error_code ec) {
                    if (ec) {
                        std::cerr << "Close error: " << ec.message() << std::endl;
                    }
                    });
            }
            });
    }

    void broadcast(const std::string& msg) {
        std::cout << "Broadcasting message: " << msg << " to " << clients.size() << " clients" << std::endl;
        for (const auto& client : clients) {
            client->write_message(msg);
        }
    }
};

class listener : public std::enable_shared_from_this<listener> {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    sqlite3* db_;
public:
    listener(net::io_context& ioc, tcp::endpoint endpoint, sqlite3* db)
        : ioc_(ioc), acceptor_(ioc, endpoint), db_(db) {
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
                auto sess = std::make_shared<session>(std::move(socket), self->db_);
                sess->start();
            }
            else {
                std::cerr << "Accept error: " << ec.message() << " (code: " << ec.value() << ")" << std::endl;
            }
            self->accept();
            });
    }
};

void do_listen(net::io_context& ioc, tcp::endpoint endpoint, sqlite3* db) {
    std::cout << "Listening for connections on " << endpoint << "..." << std::endl;
    auto l = std::make_shared<listener>(ioc, endpoint, db);
    l->start();
}

int main() {
    try {
        std::cout << "Server starting on port 8080..." << std::endl;
        sqlite3* db;
        int rc = sqlite3_open("F:\\Projects\\Messenger\\messenger.db", &db);
        if (rc) {
            std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
            return 1;
        }
        std::cout << "Database opened successfully!" << std::endl;

        const char* sql = "CREATE TABLE IF NOT EXISTS users ("
            "login TEXT PRIMARY KEY NOT NULL, "
            "password TEXT NOT NULL);";
        char* errMsg = 0;
        rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL error: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            sqlite3_close(db);
            return 1;
        }
        std::cout << "Table 'users' created successfully!" << std::endl;

        const char* sql_messages = "CREATE TABLE IF NOT EXISTS messages ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "user TEXT NOT NULL, "
            "content TEXT, "
            "type TEXT NOT NULL, "
            "file_path TEXT, "
            "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
        rc = sqlite3_exec(db, sql_messages, 0, 0, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL error (messages): " << errMsg << std::endl;
            sqlite3_free(errMsg);
            sqlite3_close(db);
            return 1;
        }
        std::cout << "Table 'messages' created successfully!" << std::endl;

        net::io_context ioc{ 1 };
        tcp::endpoint endpoint{ net::ip::make_address("0.0.0.0"), 8080 };
        do_listen(ioc, endpoint, db);
        std::cout << "Running io_context..." << std::endl;
        ioc.run();
        sqlite3_close(db);
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