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

    // Fly mode
    if (keyCode == 32) { // Space
        if (pressed && !lastSpaceKeyState) {
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

        if (pressed && player.onGround && !isFlying && !isPlayerInWater()) {
            player.velocityY = JUMP_VELOCITY;
            player.onGround = false;
        }

        lastSpaceKeyState = pressed;
    }

    // Sprint toggle
    if (keyCode == 87) { // W key
        if (pressed && !lastWKeyState) {
            float timeSinceLastPress = currentTime - lastWKeyPressTime;
            if (timeSinceLastPress < DOUBLE_TAP_TIME) isSprinting = true;
            lastWKeyPressTime = currentTime;
        }

        lastWKeyState = pressed;
        if (!pressed && isSprinting) isSprinting = false;
    }

    if (keyCode == 83 && pressed) isSprinting = false;
}

inline void Game::handleMouseMove(float movementX, float movementY) {
    if (!pointerLocked) return;
    camera.yaw   += movementX * SENSITIVITY;
    camera.pitch = std::clamp(camera.pitch - movementY * SENSITIVITY, -89.0f, 89.0f);
}

inline void Game::handleMouseClick(int button) {
    float maxDistance = 4.5f;
    RaycastHit hit = raycast(maxDistance);
    if (!hit.hit) return;
    if (button == 0) {
        // Left-click: remove block (works for solids and tall grass)
        removeBlock(hit.blockPosition.x, hit.blockPosition.y, hit.blockPosition.z);
    } else if (button == 2) {
        // Right-click: place block
        placeBlock(hit.adjacentPosition.x, hit.adjacentPosition.y, hit.adjacentPosition.z);
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