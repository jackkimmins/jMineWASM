#ifndef GAME_HELPERS_HPP
#define GAME_HELPERS_HPP

inline float Game::calculateDeltaTime() {
    auto now = std::chrono::steady_clock::now();
    float delta = std::chrono::duration<float>(now - lastFrame).count();
    lastFrame = now;
    return delta;
}

inline std::string Game::loadUsernameFromStorage() {
#ifdef __EMSCRIPTEN__
    char* result = (char*)EM_ASM_INT({
        var username = localStorage.getItem('jmine_username');
        if (username && username.length > 0) {
            var lengthBytes = lengthBytesUTF8(username) + 1;
            var stringOnWasmHeap = _malloc(lengthBytes);
            stringToUTF8(username, stringOnWasmHeap, lengthBytes);
            return stringOnWasmHeap;
        }
        return 0;
    });
    
    if (result) {
        std::string username(result);
        free(result);
        return username;
    }
#endif
    return "";
}

inline void Game::saveUsernameToStorage(const std::string& username) {
#ifdef __EMSCRIPTEN__
    EM_ASM_({
        localStorage.setItem('jmine_username', UTF8ToString($0));
    }, username.c_str());
#endif
}

inline void Game::returnToMainMenu() {
    std::cout << "[GAME] Returning to main menu..." << std::endl;
    
    // Disconnect from server
    netClient.disconnect();
    
    // Clear world data and meshes
    // Get all chunk coordinates first to avoid iterator invalidation
    std::vector<ChunkCoord> chunksToRemove;
    for (const auto& chunk : world.getChunks()) {
        chunksToRemove.push_back(chunk.first);
    }
    
    // Remove all chunks and their meshes
    for (const ChunkCoord& coord : chunksToRemove) {
        world.eraseChunk(coord.x, coord.y, coord.z);
        meshManager.removeChunkMesh(coord.x, coord.y, coord.z);
    }
    
    // Clear remote players
    remotePlayers.clear();
    
    // Close chat if open
    if (chatSystem.isChatOpen()) {
        chatSystem.closeChat();
    }
    
    // Reset game state variables
    myUsername = "";
    serverAddress = "";
    chunksLoaded = 0;
    hasReceivedFirstChunk = false;
    lastInterestChunkX = -9999;
    lastInterestChunkZ = -9999;
    chunkRevisions.clear();
    
    // Reset player position
    player.x = SPAWN_X;
    player.y = SPAWN_Y;
    player.z = SPAWN_Z;
    player.velocityY = 0.0f;
    player.onGround = true;
    
    // Reset camera
    camera.x = SPAWN_X;
    camera.y = SPAWN_Y + 1.6f;
    camera.z = SPAWN_Z;
    camera.yaw = 0.0f;
    camera.pitch = 0.0f;
    
    // Reset movement state
    velocityX = 0.0f;
    velocityZ = 0.0f;
    bobbingTime = 0.0f;
    bobbingOffset = 0.0f;
    bobbingHorizontalOffset = 0.0f;
    isMoving = false;
    isSprinting = false;
    isFlying = false;
    
    // Reset keys
    for (int i = 0; i < 1024; i++) {
        keys[i] = false;
    }
    
    // Ensure pointer is unlocked
    pointerLocked = false;
    
    // Return to main menu state
    gameState = GameState::MAIN_MENU;
    selectedMenuOption = MENU_PLAY_ONLINE;
    
    std::cout << "[GAME] Returned to main menu" << std::endl;
}

#endif // GAME_HELPERS_HPP