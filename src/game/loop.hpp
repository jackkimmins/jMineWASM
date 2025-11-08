#ifndef GAME_LOOP_HPP
#define GAME_LOOP_HPP

inline void Game::mainLoop() {
    deltaTime = calculateDeltaTime();
    gameTime += deltaTime;

    // Only process game logic when in PLAYING state
    if (gameState == GameState::PLAYING) {
        processInput(deltaTime);
        updateChunks();
        applyPhysics(deltaTime);

        if (isMoving) bobbingTime += deltaTime;
        updateHighlightedBlock();

        // Send pose updates at 10 Hz
        if (netClient.isConnected()) {
            lastPoseSendTime += deltaTime;
            if (lastPoseSendTime >= POSE_SEND_INTERVAL) {
                sendPoseUpdate();
                lastPoseSendTime = 0.0f;
            }
        }
    }

    // Always update dirty chunks, even when not PLAYING
    // This ensures block_update messages from server are rendered immediately
    meshManager.updateDirtyChunks(world);

    render();
}

#endif // GAME_LOOP_HPP