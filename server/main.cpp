// server/main.cpp
// HTTP file server + WebSocket upgrade handler
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <filesystem>
#include <optional>
#include "hub.hpp"
#include "server_config.hpp"
#include "logger.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace fs = std::filesystem;
using tcp = boost::asio::ip::tcp;

// Helper function to determine MIME type
std::string mime_type(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    if (ext == ".html") return "text/html";
    if (ext == ".js") return "application/javascript";
    if (ext == ".wasm") return "application/wasm";
    if (ext == ".png") return "image/png";
    if (ext == ".json") return "application/json";
    if (ext == ".data") return "application/octet-stream";
    return "application/octet-stream";
}

// HTTP session for serving static files
class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(tcp::socket socket, std::string root, Hub& hub)
        : socket_(std::move(socket)), root_(std::move(root)), hub_(hub) {}
    
    void run() {
        do_read();
    }
    
private:
    tcp::socket socket_;
    std::string root_;
    Hub& hub_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    
    // Store response to keep it alive during async operations
    std::optional<http::response<http::file_body>> file_response_;
    std::optional<http::response<http::string_body>> string_response_;
    
    void do_read() {
        req_ = {};
        http::async_read(socket_, buffer_, req_,
            beast::bind_front_handler(&HttpSession::on_read, shared_from_this()));
    }
    
    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        (void)bytes_transferred;
        
        if (ec == http::error::end_of_stream) {
            return do_close();
        }
        
        if (ec) {
            Logger::error("HTTP read error: " + ec.message());
            return;
        }
        
        // Check for WebSocket upgrade
        if (websocket::is_upgrade(req_)) {
            Logger::http("WebSocket upgrade request");
            
            // Create WebSocket session and pass the request for handshake
            auto ws = std::make_shared<ClientSession>(std::move(socket_), hub_);
            ws->run(std::move(req_));
            return;
        }
        
        // Handle HTTP request
        handle_request();
    }
    
    void handle_request() {
        std::string target = std::string(req_.target());
        if (target == "/") {
            target = "/index.html";
        }
        
        std::string path = root_ + target;
        Logger::http("GET " + std::string(req_.target()) + " -> " + path);
        
        // Check if file exists
        beast::error_code ec;
        http::file_body::value_type body;
        body.open(path.c_str(), beast::file_mode::scan, ec);
        
        if (ec == beast::errc::no_such_file_or_directory) {
            string_response_.emplace(http::status::not_found, req_.version());
            auto& res = *string_response_;
            res.set(http::field::server, "jMineWASM-Server");
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req_.keep_alive());
            res.body() = "404 Not Found";
            res.prepare_payload();
            http::async_write(socket_, res,
                beast::bind_front_handler(&HttpSession::on_write, shared_from_this(), res.need_eof()));
            return;
        }
        
        if (ec) {
            Logger::error("HTTP file error: " + ec.message());
            string_response_.emplace(http::status::internal_server_error, req_.version());
            auto& res = *string_response_;
            res.set(http::field::server, "jMineWASM-Server");
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req_.keep_alive());
            res.body() = "500 Internal Server Error: " + ec.message();
            res.prepare_payload();
            http::async_write(socket_, res,
                beast::bind_front_handler(&HttpSession::on_write, shared_from_this(), res.need_eof()));
            return;
        }
        
        // Send file
        auto const size = body.size();
        file_response_.emplace(
            std::piecewise_construct,
            std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, req_.version())
        );
        auto& res = *file_response_;
        res.set(http::field::server, "jMineWASM-Server");
        res.set(http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req_.keep_alive());
        http::async_write(socket_, res,
            beast::bind_front_handler(&HttpSession::on_write, shared_from_this(), res.need_eof()));
    }
    
    void on_write(bool close, beast::error_code ec, std::size_t bytes_transferred) {
        (void)bytes_transferred;
        
        if (ec) {
            Logger::error("HTTP write error: " + ec.message());
            return;
        }
        
        if (close) {
            Logger::http("Closing connection");
            return do_close();
        }
        
        Logger::http("Keeping connection alive, reading next request");
        do_read();
    }
    
    void do_close() {
        Logger::http("do_close() called");
        beast::error_code ec;
        socket_.shutdown(tcp::socket::shutdown_send, ec);
        if (ec) {
            Logger::error("HTTP shutdown error: " + ec.message());
        }
    }
};

// Listener accepts incoming connections
class Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(net::io_context& ioc, tcp::endpoint endpoint, std::string root, Hub& hub)
        : ioc_(ioc), acceptor_(ioc), root_(std::move(root)), hub_(hub) {
        beast::error_code ec;
        
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            Logger::error("Listener open error: " + ec.message());
            return;
        }
        
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            Logger::error("Listener set option error: " + ec.message());
            return;
        }
        
        acceptor_.bind(endpoint, ec);
        if (ec) {
            Logger::error("Listener bind error: " + ec.message());
            return;
        }
        
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            Logger::error("Listener listen error: " + ec.message());
            return;
        }
    }
    
    void run() {
        do_accept();
    }
    
private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::string root_;
    Hub& hub_;
    
    void do_accept() {
        acceptor_.async_accept(
            beast::bind_front_handler(&Listener::on_accept, shared_from_this()));
    }
    
    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (ec) {
            Logger::error("Listener accept error: " + ec.message());
            // Don't keep trying if we have a fatal error
            if (ec == net::error::operation_aborted) {
                return;
            }
        } else {
            std::make_shared<HttpSession>(std::move(socket), root_, hub_)->run();
        }
        
        // Accept next connection
        do_accept();
    }
};

int main(int argc, char* argv[]) {
    // Load server configuration
    ServerConfig config;
    config.loadFromFile("server.config");
    
    std::string root = "./www";
    unsigned short port = config.port;
    
    // Parse command line arguments (override config file)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) {
            root = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<unsigned short>(std::atoi(argv[++i]));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --root <path>   Root directory for static files (default: ./www)\n"
                      << "  --port <port>   Port to listen on (default: 8080)\n"
                      << "  --help, -h      Show this help\n";
            return 0;
        }
    }
    
    std::cout << "╔════════════════════════════════════╗" << std::endl;
    std::cout << "║   jMineWASM Authoritative Server  ║" << std::endl;
    std::cout << "╚════════════════════════════════════╝" << std::endl;
    std::cout << "Root: " << root << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "MOTD: " << config.motd << std::endl;
    std::cout << std::endl;
    
    // Check if root directory exists
    if (!fs::exists(root)) {
        Logger::error("Root directory does not exist: " + root);
        return 1;
    }
    
    try {
        net::io_context ioc{1}; // Single threaded for simplicity
        Hub hub("./world_save", config.motd);
        
        // Create and launch listening port
        auto listener = std::make_shared<Listener>(ioc, tcp::endpoint{tcp::v4(), port}, root, hub);
        listener->run();
        
        Logger::server("Listening on http://localhost:" + std::to_string(port));
        Logger::server("WebSocket endpoint: ws://localhost:" + std::to_string(port) + "/ws");
        Logger::server("Press Ctrl+C to stop");
        std::cout << std::endl;
        
        // Setup signal handler for graceful shutdown
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](beast::error_code const&, int) {
            std::cout << std::endl;
            Logger::server("Shutting down...");
            Logger::server("Saving world...");
            hub.saveWorld();
            ioc.stop();
        });
        
        // Setup periodic auto-save timer (every 5 minutes)
        net::steady_timer saveTimer(ioc);
        std::function<void(beast::error_code)> scheduleSave;
        scheduleSave = [&](beast::error_code ec) {
            if (ec) return;
            Logger::server("Auto-save triggered");
            hub.saveWorld();
            saveTimer.expires_after(std::chrono::minutes(5));
            saveTimer.async_wait(scheduleSave);
        };
        saveTimer.expires_after(std::chrono::minutes(5));
        saveTimer.async_wait(scheduleSave);
        
        // Run the I/O service
        ioc.run();
        
        Logger::server("Stopped");
    }
    catch (const std::exception& e) {
        Logger::error("Server error: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}
