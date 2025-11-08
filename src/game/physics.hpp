#ifndef GAME_PHYSICS_HPP
#define GAME_PHYSICS_HPP

inline bool Game::isBlockSolid(int x, int y, int z) const {
    const Block* b = world.getBlockAt(x, y, z);
    if (!b) return true;
    if (b->type == BLOCK_WATER) return false;
    return b->isSolid;
}

inline bool Game::isBlockSelectable(int x, int y, int z) const {
    const Block* b = world.getBlockAt(x, y, z);
    if (!b) return false;
    // Water blocks should not be selectable (no outline, can't be destroyed)
    if (b->type == BLOCK_WATER) return false;
    if (b->isSolid) return true;
    return (b->type == BLOCK_TALL_GRASS || b->type == BLOCK_ORANGE_FLOWER || b->type == BLOCK_BLUE_FLOWER);
}

inline bool Game::isWaterBlock(int x, int y, int z) const {
    const Block* b = world.getBlockAt(x, y, z);
    return b && (b->type == BLOCK_WATER);
}

inline bool Game::isPlayerInWater() const {
    int px = static_cast<int>(std::floor(player.x));
    int pz = static_cast<int>(std::floor(player.z));
    int footY = static_cast<int>(std::floor(player.y + 0.01f));
    int headY = static_cast<int>(std::floor(player.y + PLAYER_HEIGHT - 0.01f));
    for (int by = footY; by <= headY; ++by) {
        if (isWaterBlock(px, by, pz)) {
            return true;
        }
    }
    return false;
}

inline void Game::checkGround() {
    float epsilon = 0.001f;
    player.onGround = isColliding(player.x, player.y - epsilon, player.z);
}

inline bool Game::isColliding(float x, float y, float z) const {
    float halfWidth = 0.3f;
    float halfDepth = 0.3f;
    float epsilon = 0.005f;
    float minX = x - halfWidth + epsilon;
    float maxX = x + halfWidth - epsilon;
    float minY = y;
    float maxY = y + PLAYER_HEIGHT - epsilon;
    float minZ = z - halfDepth + epsilon;
    float maxZ = z + halfDepth - epsilon;

    for (int bx = std::floor(minX); bx <= std::floor(maxX); ++bx) {
        for (int by = std::floor(minY); by <= std::floor(maxY); ++by) {
            for (int bz = std::floor(minZ); bz <= std::floor(maxZ); ++bz) {
                if (isBlockSolid(bx, by, bz)) {
                    return true;
                }
            }
        }
    }

    return false;
}

inline void Game::applyPhysics(float dt) {
    if (isFlying) {
        float vert = 0.0f;
        if (keys[32]) vert += FLY_VERTICAL_SPEED * dt; // Up
        if (keys[16]) vert -= FLY_VERTICAL_SPEED * dt; // Down

        float newY = player.y + vert;

        if (!isColliding(player.x, newY, player.z)) {
            player.y = newY;
            player.onGround = false;
        } else {
            if (vert > 0.0f) {
                player.y = std::floor(newY + PLAYER_HEIGHT) - PLAYER_HEIGHT;
            } else if (vert < 0.0f) {
                player.y = std::floor(newY) + 1.0f;
                player.onGround = true;
                isFlying = false;
                player.velocityY = 0.0f;
            }
        }

        checkGround();
        if (player.onGround) isFlying = false;

        if (isColliding(player.x, player.y, player.z)) {
            player.x = lastSafePos.x;
            player.y = lastSafePos.y;
            player.z = lastSafePos.z;
            player.velocityY = 0.0f;
            player.onGround = true;
            isFlying = false;
        } else {
            lastSafePos = { player.x, player.y, player.z };
        }

        return;
    }

    // Check for water physics
    bool inWater = isPlayerInWater();
    if (inWater) {
        // Swimming physics: slow gravity and upward swim on space
        if (keys[32]) {
            // Swim up
            player.velocityY = 5.0f; // Upward velocity when holding space
        } else {
            // Slowly sink
            player.velocityY += (GRAVITY * 0.2f) * dt;
        }

        float newY = player.y + player.velocityY * dt;
        if (player.velocityY > 0) {
            if (!isColliding(player.x, newY, player.z)) {
                player.y = newY;
            } else {
                player.y = std::floor(newY + PLAYER_HEIGHT) - PLAYER_HEIGHT;
                player.velocityY = 0.0f;
            }
        } else {
            if (!isColliding(player.x, newY, player.z)) {
                player.y = newY;
                player.onGround = false;
            } else {
                player.y = std::floor(newY) + 1.0f;
                player.velocityY = 0.0f;
                player.onGround = true;
            }
        }

        checkGround();
        if (isColliding(player.x, player.y, player.z)) {
            player.x = lastSafePos.x;
            player.y = lastSafePos.y;
            player.z = lastSafePos.z;
            player.velocityY = 0.0f;
            player.onGround = true;
        } else {
            lastSafePos = { player.x, player.y, player.z };
        }
        return;
    }

    // Normal physics
    player.velocityY += GRAVITY * dt;
    float newY = player.y + player.velocityY * dt;

    if (player.y < -1.0f) {
        Vector3 spawnPos;
        if (!findSafeSpawn(SPAWN_X, SPAWN_Z, spawnPos)) {
            int h = world.getHeightAt(static_cast<int>(SPAWN_X), static_cast<int>(SPAWN_Z));
            spawnPos = { std::floor(SPAWN_X) + 0.5f, h + 1.6f, std::floor(SPAWN_Z) + 0.5f };
        }

        player.x = spawnPos.x;
        player.y = spawnPos.y;
        player.z = spawnPos.z;
        player.velocityY = 0.0f;
        player.onGround = true;

        camera.x = player.x;
        camera.y = player.y + 1.6f;
        camera.z = player.z;

        lastPlayerChunkX = static_cast<int>(std::floor(player.x / CHUNK_SIZE));
        lastPlayerChunkZ = static_cast<int>(std::floor(player.z / CHUNK_SIZE));

        // In online-only mode, we do NOT generate chunks locally
        // The server will send chunk data when we reconnect

        lastSafePos = { player.x, player.y, player.z };
        return;
    }

    if (player.velocityY > 0) {
        if (!isColliding(player.x, newY, player.z)) {
            player.y = newY;
        } else {
            player.y = std::floor(newY + PLAYER_HEIGHT) - PLAYER_HEIGHT;
            player.velocityY = 0.0f;
        }
    } else {
        if (!isColliding(player.x, newY, player.z)) {
            player.y = newY;
            player.onGround = false;
        } else {
            player.y = std::floor(newY) + 1.0f;
            player.velocityY = 0.0f;
            player.onGround = true;
        }
    }

    checkGround();

    if (isColliding(player.x, player.y, player.z)) {
        player.x = lastSafePos.x;
        player.y = lastSafePos.y;
        player.z = lastSafePos.z;
        player.velocityY = 0.0f;
        player.onGround = true;
    } else {
        lastSafePos = { player.x, player.y, player.z };
    }
}

inline void Game::processInput(float dt) {
    currentTime += dt;

    // Don't process player movement when chat is open
    if (chatSystem.isChatOpen()) {
        return;
    }

    float baseSpeed = isSprinting ? SPRINT_SPEED : PLAYER_SPEED;
    if (isFlying) {
        baseSpeed = FLY_SPEED;
        isSprinting = false;
    }
    // Reduce horizontal speed if in water
    if (!isFlying && isPlayerInWater()) {
        isSprinting = false;
        baseSpeed *= 0.5f;
    }
    float distance = baseSpeed * dt;

    float radYaw = camera.yaw * M_PI / 180.0f;
    float frontX = cosf(radYaw);
    float frontZ = sinf(radYaw);
    float rightX = -sinf(radYaw);
    float rightZ = cosf(radYaw);

    float deltaX = 0.0f;
    float deltaZ = 0.0f;

    if (keys[87]) { // W
        deltaX += frontX * distance;
        deltaZ += frontZ * distance;
    }
    if (keys[83]) { // S
        deltaX -= frontX * distance;
        deltaZ -= frontZ * distance;
        isSprinting = false;
    }
    if (keys[65]) { // A
        deltaX -= rightX * distance;
        deltaZ -= rightZ * distance;
    }
    if (keys[68]) { // D
        deltaX += rightX * distance;
        deltaZ += rightZ * distance;
    }

    isMoving = (deltaX != 0.0f || deltaZ != 0.0f) && !isFlying;

    // In online-only mode, we do NOT generate chunks locally
    // All chunk data comes from the server

    if (!isColliding(player.x + deltaX, player.y, player.z)) player.x += deltaX;
    if (!isColliding(player.x, player.y, player.z + deltaZ)) player.z += deltaZ;

    if (!keys[87] && isSprinting) isSprinting = false;

    if (isFlying) {
        checkGround();
        if (player.onGround) isFlying = false;
    } else {
        checkGround();
    }
}

#endif // GAME_PHYSICS_HPP