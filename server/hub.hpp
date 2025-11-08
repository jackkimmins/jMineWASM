// server/hub.hpp
// WebSocket client manager and message dispatcher
#ifndef SERVER_HUB_HPP
#define SERVER_HUB_HPP

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <iostream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <cmath>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/strand.hpp>

#include "../shared/config.hpp"
#include "../shared/types.hpp"
#include "../shared/chunk.hpp"
#include "../shared/protocol.hpp"
#include "../shared/serialization.hpp"
#include "../shared/perlin_noise.hpp"
#include "../shared/world_generation.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class ClientSession;

class Hub {
public:
    Hub(const std::string& saveDir = "./world_save") : worldSaveDir(saveDir) {
        std::cout << "[HUB] Initializing world..." << std::endl;
        world.initialise();
        std::cout << "[HUB] World initialized" << std::endl;
        
        // Try to load existing world data
        loadWorld();
        
        // Calculate safe spawn point
        calculateSafeSpawnPoint();
    }
    
    ~Hub() {
        // Auto-save on shutdown
        saveWorld();
    }
    
    void addClient(std::shared_ptr<ClientSession> client);
    void removeClient(std::shared_ptr<ClientSession> client);
    void handleMessage(std::shared_ptr<ClientSession> client, const std::string& message);
    void broadcastPlayerSnapshot();
    
    World& getWorld() { return world; }
    
    // World persistence
    void saveWorld();
    void loadWorld();
    void markChunkModified(int cx, int cy, int cz);
    bool loadChunkIfSaved(int cx, int cy, int cz); // Returns true if loaded from disk
    
    // Spawn point management
    void calculateSafeSpawnPoint();
    float getSpawnX() const { return spawnX; }
    float getSpawnY() const { return spawnY; }
    float getSpawnZ() const { return spawnZ; }
    
private:
    World world;
    std::mutex clientsMutex;
    std::unordered_map<std::shared_ptr<ClientSession>, std::string> clients; // client -> id
    int nextClientId = 1;
    std::string worldSaveDir;
    
    // Spawn point coordinates
    float spawnX = SPAWN_X;
    float spawnY = SPAWN_Y;
    float spawnZ = SPAWN_Z;
    
    // Track which chunks have been modified and need saving
    std::unordered_set<ChunkCoord, std::hash<ChunkCoord>> modifiedChunks;
    
    // Chunk revision tracking and cache (per chunk coordinate)
    std::unordered_map<ChunkCoord, int> chunkRevisions;
    
    // Cached serialized chunks: key = (cx,cy,cz,rev), value = base64 RLE data
    struct ChunkCacheKey {
        int cx, cy, cz, rev;
        bool operator==(const ChunkCacheKey& o) const {
            return cx==o.cx && cy==o.cy && cz==o.cz && rev==o.rev;
        }
    };
    struct ChunkCacheKeyHash {
        std::size_t operator()(const ChunkCacheKey& k) const {
            return std::hash<int>()(k.cx) ^ (std::hash<int>()(k.cy) << 1) ^ 
                   (std::hash<int>()(k.cz) << 2) ^ (std::hash<int>()(k.rev) << 3);
        }
    };
    std::unordered_map<ChunkCacheKey, std::string, ChunkCacheKeyHash> chunkCache;
    
    void handleHello(std::shared_ptr<ClientSession> client, const std::string& message);
    void handleSetInterest(std::shared_ptr<ClientSession> client, const std::string& message);
    void handleEdit(std::shared_ptr<ClientSession> client, const std::string& message);
    void handleChat(std::shared_ptr<ClientSession> client, const std::string& message);
    void sendChunkFull(std::shared_ptr<ClientSession> client, int cx, int cy, int cz);
    void sendChunkUnload(std::shared_ptr<ClientSession> client, int cx, int cy, int cz);
    void broadcastBlockUpdate(int wx, int wy, int wz, uint8_t blockType, bool isSolid);
    void broadcastChatMessage(const std::string& sender, const std::string& message);
    void broadcastSystemMessage(const std::string& message);
    void sendSystemMessage(std::shared_ptr<ClientSession> client, const std::string& message);
};

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(tcp::socket socket, Hub& hub)
        : ws_(std::move(socket)), hub_(hub), strand_(ws_.get_executor()) {}
    
    template<class Body, class Allocator>
    void run(http::request<Body, http::basic_fields<Allocator>> req) {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.set_option(websocket::stream_base::decorator([](websocket::response_type& res) {
            res.set(http::field::server, "jMineWASM-Server");
        }));
        
        // Accept the WebSocket upgrade with the HTTP request
        ws_.async_accept(req, beast::bind_front_handler(&ClientSession::on_accept, shared_from_this()));
    }
    
    void send(const std::string& message) {
        net::post(strand_, [self = shared_from_this(), message]() {
            self->queue_.push_back(message);
            if (self->queue_.size() == 1) {
                self->do_write();
            }
        });
    }
    
    std::unordered_set<ChunkCoord, std::hash<ChunkCoord>>& getAOI() { return currentAOI; }
    
    // Rate limiting and pose tracking
    float lastPoseX = 0.0f, lastPoseY = 0.0f, lastPoseZ = 0.0f;
    float lastYaw = 0.0f, lastPitch = 0.0f;
    std::chrono::steady_clock::time_point lastEditTime;
    std::chrono::steady_clock::time_point lastPoseUpdate;
    int editTokens = 60; // Token bucket: 60 initial, refills at 20/sec
    
private:
    websocket::stream<tcp::socket> ws_;
    Hub& hub_;
    net::strand<net::any_io_executor> strand_;
    beast::flat_buffer buffer_;
    std::vector<std::string> queue_;
    std::unordered_set<ChunkCoord, std::hash<ChunkCoord>> currentAOI;
    
    void on_accept(beast::error_code ec) {
        if (ec) {
            std::cerr << "[SESSION] Accept error: " << ec.message() << std::endl;
            return;
        }
        std::cout << "[SESSION] Client connected" << std::endl;
        hub_.addClient(shared_from_this());
        do_read();
    }
    
    void do_read() {
        ws_.async_read(buffer_, beast::bind_front_handler(&ClientSession::on_read, shared_from_this()));
    }
    
    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        if (ec == websocket::error::closed) {
            std::cout << "[SESSION] Client disconnected" << std::endl;
            hub_.removeClient(shared_from_this());
            return;
        }
        
        if (ec) {
            std::cerr << "[SESSION] Read error: " << ec.message() << std::endl;
            hub_.removeClient(shared_from_this());
            return;
        }
        
        std::string message = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        
        // std::cout << "[SESSION] ← " << message << std::endl;
        hub_.handleMessage(shared_from_this(), message);
        
        do_read();
    }
    
    void do_write() {
        if (queue_.empty()) return;
        
        ws_.async_write(net::buffer(queue_.front()),
            beast::bind_front_handler(&ClientSession::on_write, shared_from_this()));
    }
    
    void on_write(beast::error_code ec, std::size_t bytes_transferred) {
        if (ec) {
            std::cerr << "[SESSION] Write error: " << ec.message() << std::endl;
            return;
        }
        
        queue_.erase(queue_.begin());
        if (!queue_.empty()) {
            do_write();
        }
    }
};

#endif // SERVER_HUB_HPP
