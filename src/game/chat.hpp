// game/chat.hpp
#ifndef GAME_CHAT_HPP
#define GAME_CHAT_HPP

inline void Game::renderChat() {
    if (gameState != GameState::PLAYING) return;
    
    auto messages = chatSystem.getVisibleMessages(currentTime);
    if (messages.empty() && !chatSystem.isChatOpen()) return;
    
    // Disable depth testing for UI
    glDisable(GL_DEPTH_TEST);
    
    // Chat positioning (bottom-left corner, Minecraft style)
    const float chatX = 10.0f;
    const float chatBottomY = canvasHeight - 50.0f;  // Leave space at bottom
    const float lineHeight = 24.0f;  // Height of each line (increased for better readability)
    const float scale = 2.5f;  // Larger font size for better readability
    const float chatBgPadding = 8.0f;
    const float chatBgWidth = canvasWidth * 0.4f;  // 50% of screen width for more space
    
    // Calculate starting Y position (messages stack upward from bottom)
    int messageCount = messages.size();
    float startY = chatBottomY - (messageCount * lineHeight);
    
    // Add space for input box if chat is open
    if (chatSystem.isChatOpen()) {
        startY -= lineHeight * 1.5f;  // Extra space for input box
    }
    
    // Draw background for all messages
    if (!messages.empty() || chatSystem.isChatOpen()) {
        float bgHeight = (messageCount + (chatSystem.isChatOpen() ? 1.5f : 0)) * lineHeight + chatBgPadding * 2;
        float bgX = 0.0f;
        float bgY = startY - chatBgPadding;
        float bgW = chatBgWidth + chatBgPadding * 2;
        
        // Draw semi-transparent dark background
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        textRenderer.drawRect(bgX, bgY, bgW, bgHeight, 
                              0.0f, 0.0f, 0.0f, 
                              chatSystem.isChatOpen() ? 0.6f : 0.4f);
    }
    
    // Render messages
    float currentY = startY;
    for (const ChatMessage* msg : messages) {
        // Determine color based on message type
        float r = 1.0f, g = 1.0f, b = 1.0f;
        
        // Render sender name with color
        if (msg->type == MessageType::SYSTEM) {
            r = 1.0f; g = 1.0f; b = 0.0f;  // Yellow for system
        } else {
            r = 0.5f; g = 0.5f; b = 1.0f;  // Light blue for player names
        }
        
        std::string senderText = msg->sender + ": ";
        float senderWidth = textRenderer.measureLineWidth(senderText, scale);
        
        textRenderer.drawText(senderText, chatX + chatBgPadding, currentY, scale, r, g, b, 1.0f);
        
        // Render message text in white
        textRenderer.drawText(msg->message, chatX + chatBgPadding + senderWidth, currentY, scale, 1.0f, 1.0f, 1.0f, 1.0f);
        
        currentY += lineHeight;
    }
    
    // Render input box if chat is open
    if (chatSystem.isChatOpen()) {
        currentY += lineHeight * 0.5f;  // Add spacing
        
        std::string inputPrompt = "> ";
        std::string inputText = inputPrompt + chatSystem.getInputBuffer() + "_";  // Add cursor
        
        textRenderer.drawText(inputText, chatX + chatBgPadding, currentY, scale, 1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    // Restore OpenGL state
    glEnable(GL_DEPTH_TEST);
}

inline void Game::sendChatMessage(const std::string& message) {
    if (!netClient.isConnected()) {
        chatSystem.addSystemMessage("Cannot send message: Not connected to server");
        return;
    }
    
    std::ostringstream json;
    json << "{\"op\":\"" << ClientOp::CHAT << "\""
         << ",\"message\":\"" << message << "\""
         << "}";
    
    netClient.send(json.str());
}

#endif // GAME_CHAT_HPP
