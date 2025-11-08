// chat_system.hpp
#ifndef CHAT_SYSTEM_HPP
#define CHAT_SYSTEM_HPP

#include <string>
#include <vector>
#include <chrono>

enum class MessageType {
    PLAYER,
    SYSTEM
};

struct ChatMessage {
    std::string sender;
    std::string message;
    MessageType type;
    std::chrono::steady_clock::time_point timestamp;
    
    ChatMessage(const std::string& s, const std::string& m, MessageType t)
        : sender(s), message(m), type(t), timestamp(std::chrono::steady_clock::now()) {}
};

class ChatSystem {
public:
    ChatSystem() : chatOpen(false), maxVisibleMessages(10), messageTimeout(10.0f) {}
    
    void addMessage(const std::string& sender, const std::string& message, MessageType type) {
        messages.push_back(ChatMessage(sender, message, type));
        
        // Keep only the last 100 messages in memory
        if (messages.size() > 100) {
            messages.erase(messages.begin());
        }
    }
    
    void addPlayerMessage(const std::string& playerName, const std::string& message) {
        addMessage(playerName, message, MessageType::PLAYER);
    }
    
    void addSystemMessage(const std::string& message) {
        addMessage("System", message, MessageType::SYSTEM);
    }
    
    void toggleChat() {
        chatOpen = !chatOpen;
        if (chatOpen) {
            inputBuffer.clear();
        }
    }
    
    void openChat() {
        chatOpen = true;
        inputBuffer.clear();
    }
    
    void closeChat() {
        chatOpen = false;
        inputBuffer.clear();
    }
    
    bool isChatOpen() const {
        return chatOpen;
    }
    
    void appendToInput(char c) {
        if (inputBuffer.length() < 256) {  // Max message length
            inputBuffer += c;
        }
    }
    
    void backspaceInput() {
        if (!inputBuffer.empty()) {
            inputBuffer.pop_back();
        }
    }
    
    std::string getInputBuffer() const {
        return inputBuffer;
    }
    
    std::string submitInput() {
        std::string result = inputBuffer;
        inputBuffer.clear();
        chatOpen = false;
        return result;
    }
    
    // Get messages to display (either visible ones when chat closed, or all recent when open)
    std::vector<const ChatMessage*> getVisibleMessages(float currentTime) const {
        std::vector<const ChatMessage*> visible;
        
        if (chatOpen) {
            // When chat is open, show more messages (up to 20)
            int startIdx = std::max(0, static_cast<int>(messages.size()) - 20);
            for (int i = startIdx; i < static_cast<int>(messages.size()); i++) {
                visible.push_back(&messages[i]);
            }
        } else {
            // When chat is closed, show only recent messages that haven't timed out
            for (auto it = messages.rbegin(); it != messages.rend() && visible.size() < static_cast<size_t>(maxVisibleMessages); ++it) {
                auto elapsed = std::chrono::duration<float>(
                    std::chrono::steady_clock::now() - it->timestamp
                ).count();
                
                if (elapsed < messageTimeout) {
                    visible.insert(visible.begin(), &(*it));
                }
            }
        }
        
        return visible;
    }
    
    int getMaxVisibleMessages() const {
        return maxVisibleMessages;
    }
    
    void setMaxVisibleMessages(int max) {
        maxVisibleMessages = max;
    }
    
private:
    bool chatOpen;
    std::string inputBuffer;
    std::vector<ChatMessage> messages;
    int maxVisibleMessages;
    float messageTimeout;  // Seconds before message fades when chat is closed
};

#endif // CHAT_SYSTEM_HPP
