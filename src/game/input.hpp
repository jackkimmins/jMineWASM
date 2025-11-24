#ifndef GAME_INPUT_HPP
#define GAME_INPUT_HPP

inline void Game::handleKey(int keyCode, bool pressed) {
    keys[keyCode] = pressed;

    // Handle main menu input
    if (gameState == GameState::MAIN_MENU) {
        if (keyCode == 38) { // Up arrow
            if (pressed && !lastUpKeyState) {
                selectedMenuOption--;
                if (selectedMenuOption < 0) selectedMenuOption = MENU_MAX - 1;
                std::cout << "[MENU] Selected option: " << selectedMenuOption << std::endl;
            }
            lastUpKeyState = pressed;
        }
        else if (keyCode == 40) { // Down arrow
            if (pressed && !lastDownKeyState) {
                selectedMenuOption++;
                if (selectedMenuOption >= MENU_MAX) selectedMenuOption = 0;
                std::cout << "[MENU] Selected option: " << selectedMenuOption << std::endl;
            }
            lastDownKeyState = pressed;
        }
        else if (keyCode == 13) { // Enter
            if (pressed && !lastEnterKeyState) {
                handleMenuSelection();
            }
            lastEnterKeyState = pressed;
        }
        return; // Don't process other keys in menu
    }
    
    // Handle username input
    if (gameState == GameState::USERNAME_INPUT) {
        handleUsernameInputKey(keyCode, pressed);
        return; // Don't process other keys in username input
    }

    // Handle Q key to quit to main menu when paused (pointer not locked)
    if (keyCode == 81 && pressed && !pointerLocked && gameState == GameState::PLAYING) { // Q key
        returnToMainMenu();
        return;
    }
    
    // Handle chat input
    if (chatSystem.isChatOpen()) {
        if (keyCode == 13 && pressed) { // Enter - send message
            std::string message = chatSystem.submitInput();
            if (!message.empty()) {
                sendChatMessage(message);
            }
            return;
        }
        else if (keyCode == 27 && pressed) { // Escape - close chat
            chatSystem.closeChat();
            return;
        }
        else if (keyCode == 8 && pressed) { // Backspace
            chatSystem.backspaceInput();
            return;
        }
        // Don't process other game keys when chat is open
        return;
    }

    // T key to open chat
    if (keyCode == 84 && pressed) { // T key
        chatSystem.openChat();
        chatJustOpened = true;  // Mark that chat was just opened
        return;
    }

    // Fly mode
    if (keyCode == 32) { // Space
        if (pressed && !lastSpaceKeyState) {
            // Jump buffering - store jump input for a short time
            jumpBufferCounter = JUMP_BUFFER_TIME;
            
            float timeSinceLastPress = currentTime - lastSpaceKeyPressTime;
            if (timeSinceLastPress < DOUBLE_TAP_TIME && !isPlayerInWater()) {
                isFlying = !isFlying;
                if (isFlying) {
                    player.velocityY = 0.0f;
                    player.onGround = false;
                    isSprinting = false;
                }
            }
            lastSpaceKeyPressTime = currentTime;
        }

        lastSpaceKeyState = pressed;
    }

    // Sprint with Ctrl key (17 = Ctrl) OR double-tap W
    if (keyCode == 17) {
        isSprinting = pressed && keys[87] && !isPlayerInWater() && !isFlying;
        lastCtrlKeyState = pressed;
    }

    // Update sprint state when W is pressed/released
    if (keyCode == 87) { // W key
        // Double-tap detection for sprint
        if (pressed && !lastWKeyState) {
            float timeSinceLastPress = currentTime - lastWKeyPressTime;
            if (timeSinceLastPress < DOUBLE_TAP_TIME && !isPlayerInWater() && !isFlying) {
                isSprinting = true;
            }
            lastWKeyPressTime = currentTime;
        }
        
        // Keep sprint active if Ctrl is held
        if (pressed && lastCtrlKeyState && !isPlayerInWater() && !isFlying) {
            isSprinting = true;
        }
        if (!pressed) {
            isSprinting = false;
        }
        lastWKeyState = pressed;
    }

    // Cancel sprint on S key
    if (keyCode == 83 && pressed) {
        isSprinting = false;
    }
}

inline void Game::handleCharInput(char c) {
    // Handle username input
    if (gameState == GameState::USERNAME_INPUT) {
        handleUsernameCharInput(c);
        return;
    }
    
    // Only handle character input when chat is open
    if (chatSystem.isChatOpen()) {
        // If chat was just opened, skip this character (to prevent 'T' from appearing)
        if (chatJustOpened) {
            chatJustOpened = false;
            return;
        }
        
        // Filter out control characters
        if (c >= 32 && c <= 126) {
            chatSystem.appendToInput(c);
        }
    }
}

inline void Game::handleMouseMove(float movementX, float movementY) {
    if (!pointerLocked) return;
    // Don't move camera when chat is open
    if (chatSystem.isChatOpen()) return;
    camera.yaw   += movementX * SENSITIVITY;
    camera.pitch = std::clamp(camera.pitch - movementY * SENSITIVITY, -89.0f, 89.0f);
}

inline void Game::handleMouseClick(int button) {
    // Don't handle clicks when chat is open or when not pointer locked
    if (chatSystem.isChatOpen()) return;
    if (!pointerLocked) return;
    
    float maxDistance = 4.5f;
    RaycastHit hit = raycast(maxDistance);
    if (!hit.hit) return;
    if (button == 0) {
        // Left-click: remove block (works for solids and tall grass)
        removeBlock(hit.blockPosition.x, hit.blockPosition.y, hit.blockPosition.z);
    } else if (button == 2) {
        // Right-click: place block
        // If looking at a plant (flower/tall grass), replace it instead of placing adjacent
        Block* hitBlock = world.getBlockAt(hit.blockPosition.x, hit.blockPosition.y, hit.blockPosition.z);
        bool isPlant = hitBlock && (hitBlock->type == BLOCK_TALL_GRASS || 
                                    hitBlock->type == BLOCK_ORANGE_FLOWER || 
                                    hitBlock->type == BLOCK_BLUE_FLOWER);
        
        if (isPlant) {
            // Replace the plant with the new block
            placeBlock(hit.blockPosition.x, hit.blockPosition.y, hit.blockPosition.z);
        } else {
            // Place block in the adjacent position as usual
            placeBlock(hit.adjacentPosition.x, hit.adjacentPosition.y, hit.adjacentPosition.z);
        }
    }
}

inline void Game::handleMouseWheel(double deltaY) {
    // Don't handle wheel when chat is open
    if (chatSystem.isChatOpen()) return;
    
    // Scroll down (positive deltaY) = next slot
    // Scroll up (negative deltaY) = previous slot
    if (deltaY > 0) {
        hotbarInventory.scrollNext();
    } else if (deltaY < 0) {
        hotbarInventory.scrollPrevious();
    }
}

inline void Game::handleMenuMouseMove(float x, float y) {
    // Store mouse position for menu rendering
    menuMouseX = x;
    menuMouseY = y;

    // Determine which menu option is being hovered
    // Menu buttons are centered at y=400, 480, 560 with height 60
    float canvasHeight = 600.0f; // Assuming standard canvas height
    int buttonWidthPixels = 300;
    int buttonHeightPixels = 60;
    int buttonSpacing = 20;
    int startY = 400;

    int canvasWidth = 800; // Assuming standard canvas width
    int buttonX = (canvasWidth - buttonWidthPixels) / 2;

    for (int i = 0; i < MENU_MAX; i++) {
        int buttonY = startY + i * (buttonHeightPixels + buttonSpacing);
        if (x >= buttonX && x <= buttonX + buttonWidthPixels &&
            y >= buttonY && y <= buttonY + buttonHeightPixels) {
            selectedMenuOption = i;
            return;
        }
    }
}

inline void Game::handleMenuClick(float x, float y) {
    // Check if click is on a menu button and handle selection
    float canvasHeight = 600.0f;
    int buttonWidthPixels = 300;
    int buttonHeightPixels = 60;
    int buttonSpacing = 20;
    int startY = 400;

    int canvasWidth = 800;
    int buttonX = (canvasWidth - buttonWidthPixels) / 2;

    for (int i = 0; i < MENU_MAX; i++) {
        int buttonY = startY + i * (buttonHeightPixels + buttonSpacing);
        if (x >= buttonX && x <= buttonX + buttonWidthPixels &&
            y >= buttonY && y <= buttonY + buttonHeightPixels) {
            selectedMenuOption = i;
            handleMenuSelection();
            return;
        }
    }
}

#endif // GAME_INPUT_HPP