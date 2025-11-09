#ifndef GAME_RENDER_HPP
#define GAME_RENDER_HPP

#include <vector>

inline void Game::renderBlockOutline(int x, int y, int z, const mat4& mvp) {
    bool frontFace = !isBlockSolid(x, y, z - 1);
    bool backFace = !isBlockSolid(x, y, z + 1);
    bool leftFace = !isBlockSolid(x - 1, y, z);
    bool rightFace = !isBlockSolid(x + 1, y, z);
    bool bottomFace = !isBlockSolid(x, y - 1, z);
    bool topFace = !isBlockSolid(x, y + 1, z);

    float offset = 0.002f;
    float minX = x - offset, maxX = x + 1.0f + offset;
    float minY = y - offset, maxY = y + 1.0f + offset;
    float minZ = z - offset, maxZ = z + 1.0f + offset;
    std::vector<float> edges;

    if (bottomFace || frontFace)    edges.insert(edges.end(), { minX, minY, minZ,  maxX, minY, minZ });
    if (bottomFace || rightFace)    edges.insert(edges.end(), { maxX, minY, minZ,  maxX, minY, maxZ });
    if (bottomFace || backFace)     edges.insert(edges.end(), { maxX, minY, maxZ,  minX, minY, maxZ });
    if (bottomFace || leftFace)     edges.insert(edges.end(), { minX, minY, maxZ,  minX, minY, minZ });
    if (topFace || frontFace)       edges.insert(edges.end(), { minX, maxY, minZ,  maxX, maxY, minZ });
    if (topFace || rightFace)       edges.insert(edges.end(), { maxX, maxY, minZ,  maxX, maxY, maxZ });
    if (topFace || backFace)        edges.insert(edges.end(), { maxX, maxY, maxZ,  minX, maxY, maxZ });
    if (topFace || leftFace)        edges.insert(edges.end(), { minX, maxY, maxZ,  minX, maxY, minZ });
    if (frontFace || leftFace)      edges.insert(edges.end(), { minX, minY, minZ,  minX, maxY, minZ });
    if (frontFace || rightFace)     edges.insert(edges.end(), { maxX, minY, minZ,  maxX, maxY, minZ });
    if (backFace || rightFace)      edges.insert(edges.end(), { maxX, minY, maxZ,  maxX, maxY, maxZ });
    if (backFace || leftFace)       edges.insert(edges.end(), { minX, minY, maxZ,  minX, maxY, maxZ });
    if (edges.empty()) return;

    outlineShader->use();
    glUniformMatrix4fv(outlineMvpLoc, 1, GL_FALSE, mvp.data);
    glBindVertexArray(outlineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, outlineVBO);
    glBufferData(GL_ARRAY_BUFFER, edges.size() * sizeof(float), edges.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, edges.size() / 3);
    glBindVertexArray(0);
}

inline void Game::render() {
    camera.x = player.x;
    camera.y = player.y + 1.6f;
    camera.z = player.z;

    float targetBob = 0.0f;
    float targetBobHorizontal = 0.0f;
    if (isMoving) {
        targetBob = sin(bobbingTime * BOBBING_FREQUENCY) * BOBBING_AMPLITUDE;
        targetBobHorizontal = sin(bobbingTime * BOBBING_FREQUENCY * 2.0f) * BOBBING_HORIZONTAL_AMPLITUDE;
    }

    bobbingOffset += (targetBob - bobbingOffset) * std::min(deltaTime * BOBBING_DAMPING_SPEED, 1.0f);
    bobbingHorizontalOffset += (targetBobHorizontal - bobbingHorizontalOffset) * std::min(deltaTime * BOBBING_DAMPING_SPEED, 1.0f);
    camera.y += bobbingOffset;
    Vector3 camRight = camera.getRightVector();
    camera.x += camRight.x * bobbingHorizontalOffset;
    camera.z += camRight.z * bobbingHorizontalOffset;

    int width, height;
    emscripten_get_canvas_element_size("canvas", &width, &height);
    glViewport(0, 0, width, height);
    projection = perspective(CAM_FOV * M_PI / 180.0f, (float)width / (float)height, 0.1f, 1000.0f);

    glClearColor(0.25f, 0.50f, 0.85f, 1.0f); // Clear to deep sky blue
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4 view = camera.getViewMatrix();

    // Render sky gradient
    glDepthFunc(GL_LEQUAL); // Change depth function so depth test passes when values are equal to depth buffer's content
    skyShader->use();
    glUniformMatrix4fv(skyViewLoc, 1, GL_FALSE, view.data);
    glUniformMatrix4fv(skyProjLoc, 1, GL_FALSE, projection.data);
    glBindVertexArray(skyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS); // Set depth function back to default

    shader->use();
    mat4 mvp = multiply(projection, view);
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.data);
    glUniform1f(timeLoc, gameTime);

    float renderRadius = float(RENDER_DISTANCE * CHUNK_SIZE);
    float fogStart = renderRadius * 0.45f;
    float fogEnd   = renderRadius * 0.95f;
    glUniform3f(camPosLoc, camera.x, camera.y, camera.z);
    glUniform1f(fogStartLoc, fogStart);
    glUniform1f(fogEndLoc, fogEnd);
    // Use a middle ground between horizon (0.53, 0.81, 0.92) and sky (0.25, 0.50, 0.85)
    glUniform3f(fogColorLoc, 0.42f, 0.68f, 0.89f);

    // Extract frustum planes from view-projection matrix for culling
    Frustum frustum;
    frustum.extractFromMatrix(mvp);

    // FIRST PASS - Draw opaque & alpha-tested (solids + tall grass)
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);  // disable culling so billboard plants are double-sided
    meshManager.drawVisibleChunksSolid(player.x, player.z, &frustum);

    // SECOND PASS - Draw transparent (water)
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);
    glDepthMask(GL_FALSE);
    meshManager.drawVisibleChunksWater(player.x, player.z, &frustum);
    glDisable(GL_POLYGON_OFFSET_FILL);
    
    // THIRD PASS - Draw glass (semi-transparent)
    // Enable back-face culling for glass to prevent seeing back faces through glass
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    meshManager.drawVisibleChunksGlass(player.x, player.z, &frustum);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);

    if (hasHighlightedBlock) renderBlockOutline(highlightedBlock.x, highlightedBlock.y, highlightedBlock.z, mvp);

    // Render remote players (always in online-only mode)
    renderRemotePlayers(view);

    // Render particles (after solid geometry but before UI)
    if (particleShader) {
        particleShader->use();
        glUniformMatrix4fv(particleMvpLoc, 1, GL_FALSE, mvp.data);
        
        // Enable blending for particles
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE); // Don't write to depth buffer
        
        Vector3 cameraPos = {camera.x, camera.y, camera.z};
        particleSystem.render(view, projection, textureAtlas, cameraPos);
        
        // Restore state
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        
        // Restore main shader
        shader->use();
    }

    // Render UI overlay
    renderUI();
}

inline void Game::renderRemotePlayers(const mat4& view) {
    if (remotePlayers.empty() || !playerModel || !playerShader) {
        return;
    }

    // Use player shader
    playerShader->use();

    // Bind player skin texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, playerSkinTexture);
    GLint skinLoc = playerShader->getUniform("uSkinTexture");
    glUniform1i(skinLoc, 0);

    // Enable culling for player models
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Render each remote player
    for (const auto& entry : remotePlayers) {
        const RemotePlayer& rp = entry.second;

        // Debug: log player position once every few seconds
        static float lastLogTime = 0.0f;
        if (gameTime - lastLogTime > 3.0f) {
            std::cout << "[RENDER] Remote player " << rp.id
                      << " at (" << rp.x << ", " << rp.y << ", " << rp.z << ")"
                      << " yaw=" << rp.yaw << std::endl;
            std::cout << "[RENDER] My position: (" << player.x << ", " << player.y << ", " << player.z << ")" << std::endl;
            float dx = rp.x - player.x;
            float dy = rp.y - player.y;
            float dz = rp.z - player.z;
            float distance = sqrtf(dx*dx + dy*dy + dz*dz);
            std::cout << "[RENDER] Distance to player: " << distance << " blocks" << std::endl;
            lastLogTime = gameTime;
        }

        // Create model matrix for this player (y is at feet)
        mat4 model = createPlayerModelMatrix(rp.x, rp.y, rp.z, rp.yaw);
        mat4 mvp = multiply(projection, multiply(view, model));

        // Set MVP uniform
        glUniformMatrix4fv(playerMvpLoc, 1, GL_FALSE, mvp.data);

        // Draw player model
        playerModel->draw();
    }

    // Restore state
    glDisable(GL_CULL_FACE);

    // Restore main shader and texture
    shader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureAtlas);
}

inline void Game::renderUI() {
    // Disable depth testing for 2D UI
    glDisable(GL_DEPTH_TEST);

    // Update text renderer projection if window size changed
    int width, height;
    emscripten_get_canvas_element_size("canvas", &width, &height);
    if (width != textRenderer.getScreenWidth() || height != textRenderer.getScreenHeight()) {
        textRenderer.updateProjection(width, height);
    }

    // Handle different game states
    if (gameState == GameState::MAIN_MENU) {
        renderMainMenu(width, height);
    } else if (gameState == GameState::LOADING || gameState == GameState::CONNECTING || gameState == GameState::WAITING_FOR_WORLD) {
        // Full screen black background for loading
        textRenderer.drawOverlay(0.0f, 0.0f, 0.0f, 1.0f);

        // Show loading status in center
        float centerY = height / 2.0f - 30.0f;
        textRenderer.drawTextCentered(loadingStatus, centerY, 3.0f, 1.0f, 1.0f, 1.0f, 1.0f);

        // Show additional info based on state
        if (gameState == GameState::CONNECTING) {
            textRenderer.drawTextCentered("Establishing connection...", centerY + 50.0f, 2.0f, 0.7f, 0.7f, 0.7f, 0.8f);
        } else if (gameState == GameState::WAITING_FOR_WORLD) {
            std::string chunksInfo = std::to_string(chunksLoaded) + " chunks loaded";
            textRenderer.drawTextCentered(chunksInfo, centerY + 50.0f, 2.0f, 0.7f, 0.7f, 0.7f, 0.8f);
        }
    } else if (gameState == GameState::DISCONNECTED) {
        // Full screen black background for disconnect
        textRenderer.drawOverlay(0.0f, 0.0f, 0.0f, 1.0f);

        // Show disconnect message
        float centerY = height / 2.0f - 60.0f;
        textRenderer.drawTextCentered("DISCONNECTED", centerY, 4.0f, 1.0f, 0.3f, 0.3f, 1.0f);

        // Show reason/instructions
        textRenderer.drawTextCentered("Connection to server lost", centerY + 70.0f, 2.0f, 0.8f, 0.8f, 0.8f, 0.9f);
        textRenderer.drawTextCentered("Please refresh the page to reconnect", centerY + 110.0f, 2.0f, 0.6f, 0.6f, 0.6f, 0.8f);
    } else if (gameState == GameState::PLAYING) {
        // Normal gameplay UI

        // Render connection status in top-left corner
        if (gameState == GameState::PLAYING) {
            std::string statusText;
            float r = 1.0f, g = 1.0f, b = 1.0f;

            if (netClient.isConnected()) {
                statusText = "CONNECTED";
                r = 0.3f; g = 1.0f; b = 0.3f; // Green
            } else {
                statusText = "DISCONNECTED";
                r = 1.0f; g = 0.3f; b = 0.3f; // Red
            }

            textRenderer.drawText(statusText, 10.0f, 10.0f, 2.0f, r, g, b, 0.9f);
        }

        // Render pause overlay when not pointer locked
        if (!pointerLocked) {
            // Darken the screen
            textRenderer.drawOverlay(0.0f, 0.0f, 0.0f, 0.5f);

            // Display "PAUSED" in center
            float centerY = height / 2.0f - 30.0f;
            textRenderer.drawTextCentered("PAUSED", centerY, 5.0f, 1.0f, 1.0f, 1.0f, 1.0f);

            // Display instructions
            float instructionY = centerY + 60.0f;
            textRenderer.drawTextCentered("Click to resume", instructionY, 2.0f, 0.8f, 0.8f, 0.8f, 0.9f);
        }
        
        // Render chat (always render when in playing state)
        renderChat();
        
        // Render hotbar
        renderHotbar();
    }

    // Restore OpenGL state for 3D rendering
    glEnable(GL_DEPTH_TEST);

    // Restore the main shader and block texture atlas
    shader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureAtlas);
}

#endif // GAME_RENDER_HPP