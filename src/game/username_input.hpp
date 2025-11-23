#ifndef GAME_USERNAME_INPUT_HPP
#define GAME_USERNAME_INPUT_HPP

inline void Game::renderUsernameInput(int width, int height) {
    // Render darkened dirt texture background (same as menu)
    menuShader->use();

    float projMatrix[16] = {
        2.0f / width, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / height, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f
    };
    glUniformMatrix4fv(menuProjLoc, 1, GL_FALSE, projMatrix);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, menuBackgroundTexture);
    glUniform1i(menuTexLoc, 0);
    
    menuScrollOffset += deltaTime * 0.15f;
    if (menuScrollOffset > 1.0f) {
        menuScrollOffset -= 1.0f;
    }
    glUniform1f(menuScrollLoc, menuScrollOffset);

    float tilesX = width / 128.0f;
    float tilesY = height / 128.0f;

    float vertices[] = {
        0.0f, 0.0f,           0.0f, 0.0f,
        0.0f, (float)height,  0.0f, tilesY,
        (float)width, (float)height, tilesX, tilesY,
        0.0f, 0.0f,           0.0f, 0.0f,
        (float)width, (float)height, tilesX, tilesY,
        (float)width, 0.0f,   tilesX, 0.0f
    };

    glBindVertexArray(menuVAO);
    glBindBuffer(GL_ARRAY_BUFFER, menuVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Draw text overlay
    float centerX = width / 2.0f;

    // Title
    textRenderer.drawTextCentered("Enter Your Username", 100.0f, 6.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    // Instructions
    textRenderer.drawTextCentered("Username: 1-16 alphanumeric characters", 180.0f, 2.5f, 0.8f, 0.8f, 0.8f, 0.9f);

    // Input box background (semi-transparent dark rectangle)
    float inputBoxY = 280.0f;
    float inputBoxWidth = 600.0f;
    float inputBoxHeight = 80.0f;
    float inputBoxX = (width - inputBoxWidth) / 2.0f;

    // Draw input box border (highlighted)
    float borderThickness = 4.0f;
    
    // Draw the username text or cursor
    std::string displayText = usernameInput;
    if (displayText.empty()) {
        displayText = "_"; // Show cursor when empty
    } else {
        displayText += "_"; // Show cursor after text
    }
    
    float textScale = 4.0f;
    textRenderer.drawTextCentered(displayText, inputBoxY + 25.0f, textScale, 1.0f, 1.0f, 1.0f, 1.0f);

    // Error message (if any)
    if (!usernameError.empty()) {
        textRenderer.drawTextCentered(usernameError, inputBoxY + 100.0f, 2.5f, 1.0f, 0.3f, 0.3f, 1.0f);
    }

    // Connect button
    float buttonY = inputBoxY + 150.0f;
    bool canConnect = !usernameInput.empty() && usernameInput.length() <= 16;
    
    if (canConnect) {
        textRenderer.drawTextCentered("> Press ENTER to Connect <", buttonY, 3.5f, 0.0f, 1.0f, 0.0f, 1.0f);
    } else {
        textRenderer.drawTextCentered("Press ENTER to Connect", buttonY, 3.0f, 0.5f, 0.5f, 0.5f, 0.7f);
    }

    // Back instruction
    textRenderer.drawTextCentered("Press ESC to go back", buttonY + 80.0f, 2.0f, 0.6f, 0.6f, 0.6f, 0.8f);

    // Character count
    std::string charCount = std::to_string(usernameInput.length()) + " / 16";
    textRenderer.drawTextCentered(charCount, inputBoxY + 80.0f, 2.0f, 0.7f, 0.7f, 0.7f, 0.8f);
}

inline void Game::handleUsernameInputKey(int keyCode, bool pressed) {
    // Handle Enter key
    if (keyCode == 13) { // Enter
        if (pressed && !lastEnterKeyState) {
            submitUsername();
        }
        lastEnterKeyState = pressed;
    }
    // Handle Backspace
    else if (keyCode == 8) { // Backspace
        if (pressed && !lastBackspaceKeyState) {
            if (!usernameInput.empty()) {
                usernameInput.pop_back();
                usernameError = ""; // Clear error when typing
            }
        }
        lastBackspaceKeyState = pressed;
    }
    // Handle Escape - go back to menu
    else if (keyCode == 27) { // Escape
        if (pressed) {
            gameState = GameState::MAIN_MENU;
            usernameInput = "";
            usernameError = "";
        }
    }
}

inline void Game::handleUsernameCharInput(char c) {
    // Only accept alphanumeric characters
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
        if (usernameInput.length() < 16) {
            usernameInput += c;
            usernameError = ""; // Clear error when typing
        }
    }
}

inline void Game::submitUsername() {
    // Validate username
    if (usernameInput.empty()) {
        usernameError = "Username cannot be empty!";
        return;
    }
    
    if (usernameInput.length() > 16) {
        usernameError = "Username too long (max 16 characters)!";
        return;
    }
    
    // Check if only alphanumeric
    for (char c : usernameInput) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            usernameError = "Username must be alphanumeric only!";
            return;
        }
    }
    
    // Check if we can connect
    const char* wsUrl = std::getenv("GAME_WS_URL");
    if (!wsUrl || wsUrl[0] == '\0') {
        usernameError = "No server URL configured!";
        return;
    }
    
    // Clear error and transition to connecting state
    usernameError = "";
    gameState = GameState::CONNECTING;
    loadingStatus = "Connecting to server...";
    
    // Connect to server
    if (netClient.connect(wsUrl)) {
        std::cout << "[GAME] Connecting with username: " << usernameInput << std::endl;
    } else {
        std::cerr << "[GAME] Failed to connect" << std::endl;
        gameState = GameState::USERNAME_INPUT;
        usernameError = "Connection failed!";
    }
}

#endif // GAME_USERNAME_INPUT_HPP
