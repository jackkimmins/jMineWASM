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
            std::cerr << "[HTTP] Read error: " << ec.message() << std::endl;
            return;
        }
        
        // Check for WebSocket upgrade
        if (websocket::is_upgrade(req_)) {
            std::cout << "[HTTP] WebSocket upgrade request" << std::endl;
            
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
        std::cout << "[HTTP] GET " << req_.target() << " -> " << path << std::endl;
        
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
            std::cerr << "[HTTP] File error: " << ec.message() << std::endl;
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
            std::cerr << "[HTTP] Write error: " << ec.message() << std::endl;
            return;
        }
        
        if (close) {
            std::cout << "[HTTP] Closing connection" << std::endl;
            return do_close();
        }
        
        std::cout << "[HTTP] Keeping connection alive, reading next request" << std::endl;
        do_read();
    }
    
    void do_close() {
        std::cout << "[HTTP] do_close() called" << std::endl;
        beast::error_code ec;
        socket_.shutdown(tcp::socket::shutdown_send, ec);
        if (ec) {
            std::cerr << "[HTTP] Shutdown error: " << ec.message() << std::endl;
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
            std::cerr << "[LISTENER] Open error: " << ec.message() << std::endl;
            return;
        }
        
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            std::cerr << "[LISTENER] Set option error: " << ec.message() << std::endl;
            return;
        }
        
        acceptor_.bind(endpoint, ec);
        if (ec) {
            std::cerr << "[LISTENER] Bind error: " << ec.message() << std::endl;
            return;
        }
        
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            std::cerr << "[LISTENER] Listen error: " << ec.message() << std::endl;
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
            std::cerr << "[LISTENER] Accept error: " << ec.message() << std::endl;
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
    std::string root = "./www";
    unsigned short port = 8080;
    
    // Parse command line arguments
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
    std::cout << std::endl;
    
    // Check if root directory exists
    if (!fs::exists(root)) {
        std::cerr << "Error: Root directory does not exist: " << root << std::endl;
        return 1;
    }
    
    try {
        net::io_context ioc{1}; // Single threaded for simplicity
        Hub hub;
        
        // Create and launch listening port
        auto listener = std::make_shared<Listener>(ioc, tcp::endpoint{tcp::v4(), port}, root, hub);
        listener->run();
        
        std::cout << "[SERVER] Listening on http://localhost:" << port << std::endl;
        std::cout << "[SERVER] WebSocket endpoint: ws://localhost:" << port << "/ws" << std::endl;
        std::cout << "[SERVER] Press Ctrl+C to stop" << std::endl;
        std::cout << std::endl;
        
        // Setup signal handler for graceful shutdown
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](beast::error_code const&, int) {
            std::cout << "\n[SERVER] Shutting down..." << std::endl;
            std::cout << "[SERVER] Saving world..." << std::endl;
            hub.saveWorld();
            ioc.stop();
        });
        
        // Setup periodic auto-save timer (every 5 minutes)
        net::steady_timer saveTimer(ioc);
        std::function<void(beast::error_code)> scheduleSave;
        scheduleSave = [&](beast::error_code ec) {
            if (ec) return;
            std::cout << "[SERVER] Auto-save triggered" << std::endl;
            hub.saveWorld();
            saveTimer.expires_after(std::chrono::minutes(5));
            saveTimer.async_wait(scheduleSave);
        };
        saveTimer.expires_after(std::chrono::minutes(5));
        saveTimer.async_wait(scheduleSave);
        
        // Run the I/O service
        ioc.run();
        
        std::cout << "[SERVER] Stopped" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
