// client/net.hpp
// WebSocket client wrapper for Emscripten
#ifndef CLIENT_NET_HPP
#define CLIENT_NET_HPP

#ifdef __EMSCRIPTEN__
#include <emscripten/websocket.h>
#endif

#include <string>
#include <functional>
#include <iostream>
#include "../shared/protocol.hpp"

class NetworkClient {
public:
    using MessageCallback = std::function<void(const std::string&)>;
    
    NetworkClient() : socket(0), connected(false), onMessageCallback(nullptr) {}
    
    ~NetworkClient() {
#ifdef __EMSCRIPTEN__
        if (socket) {
            emscripten_websocket_close(socket, 1000, "Client shutdown");
            emscripten_websocket_delete(socket);
        }
#endif
    }
    
    // Connect to WebSocket server
    bool connect(const std::string& url) {
#ifdef __EMSCRIPTEN__
        if (socket) {
            std::cerr << "[NET] Already connected or connecting" << std::endl;
            return false;
        }
        
        std::cout << "[NET] Connecting to: " << url << std::endl;
        
        EmscriptenWebSocketCreateAttributes attrs;
        emscripten_websocket_init_create_attributes(&attrs);
        attrs.url = url.c_str();
        
        socket = emscripten_websocket_new(&attrs);
        if (socket <= 0) {
            std::cerr << "[NET] Failed to create WebSocket" << std::endl;
            return false;
        }
        
        // Set up callbacks (use main thread callbacks)
        emscripten_websocket_set_onopen_callback_on_thread(socket, this, onOpen, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
        emscripten_websocket_set_onerror_callback_on_thread(socket, this, onError, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
        emscripten_websocket_set_onclose_callback_on_thread(socket, this, onClose, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
        emscripten_websocket_set_onmessage_callback_on_thread(socket, this, onMessage, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
        
        return true;
#else
        std::cerr << "[NET] WebSocket only supported in Emscripten builds" << std::endl;
        return false;
#endif
    }
    
    // Send JSON message
    void send(const std::string& json) {
#ifdef __EMSCRIPTEN__
        if (!connected) {
            std::cerr << "[NET] Not connected, cannot send message" << std::endl;
            return;
        }
        
        std::cout << "[NET] → " << json << std::endl;
        emscripten_websocket_send_utf8_text(socket, json.c_str());
#endif
    }
    
    // Set callback for received messages
    void setOnMessage(MessageCallback callback) {
        onMessageCallback = callback;
    }
    
    // Set callback for connection opened
    void setOnConnect(std::function<void()> callback) {
        onConnectCallback = callback;
    }
    
    // Set callback for disconnection
    void setOnDisconnect(std::function<void()> callback) {
        onDisconnectCallback = callback;
    }
    
    bool isConnected() const {
        return connected;
    }
    
private:
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_WEBSOCKET_T socket;
#else
    int socket;
#endif
    bool connected;
    MessageCallback onMessageCallback;
    std::function<void()> onConnectCallback;
    std::function<void()> onDisconnectCallback;
    
#ifdef __EMSCRIPTEN__
    // WebSocket callbacks
    static EM_BOOL onOpen(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData) {
        NetworkClient* client = static_cast<NetworkClient*>(userData);
        client->connected = true;
        std::cout << "[NET] ✓ Connected" << std::endl;
        
        // Call connection callback if set
        if (client->onConnectCallback) {
            client->onConnectCallback();
        }
        
        return EM_TRUE;
    }
    
    static EM_BOOL onError(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData) {
        std::cerr << "[NET] ✗ Error occurred" << std::endl;
        return EM_TRUE;
    }
    
    static EM_BOOL onClose(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData) {
        NetworkClient* client = static_cast<NetworkClient*>(userData);
        client->connected = false;
        std::cout << "[NET] Connection closed (code: " << event->code 
                  << ", reason: " << event->reason << ")" << std::endl;
        
        // Call disconnection callback if set
        if (client->onDisconnectCallback) {
            client->onDisconnectCallback();
        }
        
        return EM_TRUE;
    }
    
    static EM_BOOL onMessage(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData) {
        NetworkClient* client = static_cast<NetworkClient*>(userData);
        
        if (event->isText) {
            std::string message(reinterpret_cast<const char*>(event->data), event->numBytes);
            std::cout << "[NET] ← " << message << std::endl;
            
            if (client->onMessageCallback) {
                client->onMessageCallback(message);
            }
        } else {
            std::cerr << "[NET] Received binary message (not supported)" << std::endl;
        }
        
        return EM_TRUE;
    }
#endif
};

#endif // CLIENT_NET_HPP
