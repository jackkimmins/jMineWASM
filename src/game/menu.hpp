#ifndef GAME_MENU_HPP
#define GAME_MENU_HPP

inline void Game::handleMenuSelection() {
    std::cout << "[MENU] Selected: " << selectedMenuOption << std::endl;

    if (selectedMenuOption == MENU_PLAY_ONLINE) {
        std::cout << "[MENU] Transitioning to username input..." << std::endl;

        // Transition to username input state
        gameState = GameState::USERNAME_INPUT;
        
        // Load previously saved username from localStorage
        usernameInput = loadUsernameFromStorage();
        usernameError = "";
        
        if (!usernameInput.empty()) {
            std::cout << "[MENU] Loaded saved username: " << usernameInput << std::endl;
        }
    }
    else if (selectedMenuOption == MENU_SETTINGS) {
        std::cout << "[MENU] Settings not yet implemented" << std::endl;
    }
    else if (selectedMenuOption == MENU_GITHUB) {
        std::cout << "[MENU] Opening GitHub..." << std::endl;
        // Open GitHub in new tab
        EM_ASM({
            window.open('https://github.com/jackkimmins/jMineWASM', '_blank');
        });
    }
}

inline void Game::renderMainMenu(int width, int height) {
    // Render darkened dirt texture background
    menuShader->use();

    // Setup orthographic projection for 2D rendering
    float projMatrix[16] = {
        2.0f / width, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / height, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f
    };
    glUniformMatrix4fv(menuProjLoc, 1, GL_FALSE, projMatrix);

    // Bind texture atlas (contains dirt texture)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, menuBackgroundTexture);
    glUniform1i(menuTexLoc, 0);
    
    // Update scroll offset for smooth scrolling effect
    menuScrollOffset += deltaTime * 0.15f; // Slow, smooth scroll
    if (menuScrollOffset > 1.0f) {
        menuScrollOffset -= 1.0f; // Keep it bounded
    }
    glUniform1f(menuScrollLoc, menuScrollOffset);

    // How many times to repeat the dirt texture across screen
    float tilesX = width / 128.0f;  // Each dirt block appears 128px wide
    float tilesY = height / 128.0f; // Each dirt block appears 128px tall

    // Full-screen quad with tiling UV coordinates
    // The fragment shader will map these to the dirt block portion of the atlas
    float vertices[] = {
        // x, y, u, v
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

    // Title: jMineWASM
    float titleY = 100.0f;
    textRenderer.drawTextCentered("jMineWASM", titleY, 8.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    // Subtitle: Online Multiplayer
    textRenderer.drawTextCentered("Multiplayer Edition", titleY + 60.0f, 2.5f, 0.8f, 0.8f, 1.0f, 0.9f);

    // Menu options
    float menuStartY = 300.0f;
    float menuSpacing = 60.0f;

    const char* menuOptions[] = {
        "Join Server",
        "Settings",
        "GitHub"
    };

    for (int i = 0; i < MENU_MAX; i++) {
        float optionY = menuStartY + i * menuSpacing;
        bool isSelected = (i == selectedMenuOption);

        // Highlight selected option
        if (isSelected) {
            textRenderer.drawTextCentered(std::string("> ") + menuOptions[i] + " <",
                                         optionY, 3.5f, 1.0f, 1.0f, 0.0f, 1.0f);
        } else {
            textRenderer.drawTextCentered(menuOptions[i],
                                         optionY, 3.0f, 0.7f, 0.7f, 0.7f, 0.9f);
        }
    }

    // Instructions
    float instructionsY = height - 150.0f;
    textRenderer.drawTextCentered("Use UP/DOWN arrows to navigate", instructionsY, 2.0f, 0.6f, 0.6f, 0.6f, 0.8f);
    textRenderer.drawTextCentered("Press ENTER to select", instructionsY + 30.0f, 2.0f, 0.6f, 0.6f, 0.6f, 0.8f);

    // Version in bottom left
    textRenderer.drawText("jMineWASM - Version 1.3", 20.0f, height - 40.0f, 3.0f, 1.0f, 1.0f, 1.0f, 0.8f);

    // Copyright in bottom right
    std::string copyright = "(c) 2025 Jack Kimmins";
    float copyrightWidth = copyright.length() * 9.0f * 2.0f; // Approximate width
    textRenderer.drawText(copyright, width - copyrightWidth - 20.0f, height - 40.0f, 3.0f, 1.0f, 1.0f, 1.0f, 0.8f);
}

#endif // GAME_MENU_HPP