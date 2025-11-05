// Simple test server to debug networking
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <iostream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

int main() {
    try {
        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {tcp::v4(), 8080}};
        std::cout << "Listening on port 8080..." << std::endl;
        
        tcp::socket socket{ioc};
        acceptor.accept(socket);
        std::cout << "Connection accepted!" << std::endl;
        
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req);
        std::cout << "Request: " << req.method_string() << " " << req.target() << std::endl;
        
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "TestServer");
        res.set(http::field::content_type, "text/plain");
        res.body() = "Hello from test server!";
        res.prepare_payload();
        
        http::write(socket, res);
        std::cout << "Response sent" << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
