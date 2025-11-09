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
            // Swim up with constant velocity
            player.velocityY = SWIM_UP_SPEED;
        } else {
            // Apply reduced gravity when in water
            player.velocityY += WATER_GRAVITY * dt;
            // Clamp sink speed
            if (player.velocityY < -2.0f) {
                player.velocityY = -2.0f;
            }
        }

        // Jump out of water if on surface and space pressed
        if (keys[32] && !lastSpaceKeyState) {
            // Check if head is near water surface
            int headY = static_cast<int>(std::floor(player.y + PLAYER_HEIGHT));
            int px = static_cast<int>(std::floor(player.x));
            int pz = static_cast<int>(std::floor(player.z));
            
            if (!isWaterBlock(px, headY + 1, pz)) {
                player.velocityY = WATER_JUMP_VELOCITY;
            }
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

    // Update coyote time
    if (player.onGround) {
        coyoteTimeCounter = COYOTE_TIME;
    } else if (wasOnGroundLastFrame && !player.onGround) {
        coyoteTimeCounter = COYOTE_TIME;
    } else if (coyoteTimeCounter > 0.0f) {
        coyoteTimeCounter -= dt;
    }

    // Handle jump with buffering and coyote time
    // Allow continuous jumping when holding space (but not in water)
    bool canJump = coyoteTimeCounter > 0.0f;
    
    if (jumpBufferCounter > 0.0f) {
        jumpBufferCounter -= dt;
        
        // Can jump if either on ground or within coyote time
        if (canJump) {
            player.velocityY = JUMP_VELOCITY;
            player.onGround = false;
            jumpBufferCounter = 0.0f;
            coyoteTimeCounter = 0.0f;
        }
    }
    
    // Continuous jumping - if space is still held and player just landed
    // But NOT when in water (to prevent bouncing on water surface)
    if (keys[32] && player.onGround && wasOnGroundLastFrame == false && !inWater) {
        player.velocityY = JUMP_VELOCITY;
        player.onGround = false;
        coyoteTimeCounter = 0.0f;
    }

    wasOnGroundLastFrame = player.onGround;

    // Normal physics
    player.velocityY += GRAVITY * dt;
    
    // Terminal velocity
    if (player.velocityY < -55.0f) {
        player.velocityY = -55.0f;
    }
    
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

    // Determine target speed based on state
    float targetSpeed = PLAYER_SPEED;
    if (isFlying) {
        targetSpeed = FLY_SPEED;
        isSprinting = false;
    } else if (isSprinting && keys[87]) {
        targetSpeed = SPRINT_SPEED;
    }
    
    // Reduce speed in water
    if (!isFlying && isPlayerInWater()) {
        targetSpeed = SWIM_SPEED;
        isSprinting = false;
    }

    // Calculate camera direction vectors
    float radYaw = camera.yaw * M_PI / 180.0f;
    float frontX = cosf(radYaw);
    float frontZ = sinf(radYaw);
    float rightX = -sinf(radYaw);
    float rightZ = cosf(radYaw);

    // Get input direction (normalized)
    float inputX = 0.0f;
    float inputZ = 0.0f;
    
    if (keys[87]) { // W
        inputX += frontX;
        inputZ += frontZ;
    }
    if (keys[83]) { // S
        inputX -= frontX;
        inputZ -= frontZ;
    }
    if (keys[65]) { // A
        inputX -= rightX;
        inputZ -= rightZ;
    }
    if (keys[68]) { // D
        inputX += rightX;
        inputZ += rightZ;
    }

    // Normalize input vector
    float inputLength = sqrtf(inputX * inputX + inputZ * inputZ);
    if (inputLength > 0.001f) {
        inputX /= inputLength;
        inputZ /= inputLength;
    }

    // Calculate target velocity
    float targetVelX = inputX * targetSpeed;
    float targetVelZ = inputZ * targetSpeed;

    // Choose acceleration and friction based on state
    float acceleration = player.onGround ? GROUND_ACCELERATION : AIR_ACCELERATION;
    float friction = player.onGround ? GROUND_FRICTION : (isFlying ? FLY_FRICTION : AIR_FRICTION);

    // Apply acceleration toward target velocity
    if (inputLength > 0.001f) {
        // Moving - accelerate toward target
        velocityX += (targetVelX - velocityX) * acceleration * dt;
        velocityZ += (targetVelZ - velocityZ) * acceleration * dt;
    } else {
        // Not moving - apply friction
        velocityX -= velocityX * friction * dt;
        velocityZ -= velocityZ * friction * dt;
        
        // Stop completely when very slow (prevents tiny movements/bounces)
        if (fabsf(velocityX) < 0.1f) velocityX = 0.0f;
        if (fabsf(velocityZ) < 0.1f) velocityZ = 0.0f;
    }

    // Calculate movement this frame
    float deltaX = velocityX * dt;
    float deltaZ = velocityZ * dt;

    isMoving = (fabsf(velocityX) > 0.1f || fabsf(velocityZ) > 0.1f) && !isFlying;

    // Try horizontal movement
    float newX = player.x + deltaX;
    float newZ = player.z + deltaZ;

    // Try moving in both directions
    bool canMoveX = !isColliding(newX, player.y, player.z);
    bool canMoveZ = !isColliding(player.x, player.y, newZ);
    bool canMoveBoth = !isColliding(newX, player.y, newZ);

    if (canMoveBoth) {
        player.x = newX;
        player.z = newZ;
    } else {
        // Try step-up assistance (Minecraft-style)
        bool stepped = false;
        if (!isFlying && player.onGround && (fabsf(deltaX) > 0.001f || fabsf(deltaZ) > 0.001f)) {
            // Try stepping up to STEP_HEIGHT
            for (float stepY = 0.1f; stepY <= STEP_HEIGHT; stepY += 0.1f) {
                if (!isColliding(newX, player.y + stepY, newZ)) {
                    // Can step up here
                    player.x = newX;
                    player.z = newZ;
                    player.y += stepY;
                    stepped = true;
                    break;
                }
            }
        }
        
        if (!stepped) {
            // Try sliding along walls
            if (canMoveX) {
                player.x = newX;
                velocityZ *= 0.5f; // Dampen perpendicular velocity
            }
            if (canMoveZ) {
                player.z = newZ;
                velocityX *= 0.5f; // Dampen perpendicular velocity
            }
            
            // If hit a wall, reduce velocity
            if (!canMoveX && !canMoveZ) {
                velocityX *= 0.5f;
                velocityZ *= 0.5f;
            }
        }
    }

    // Cancel sprint if not moving forward
    if (!keys[87] && isSprinting) {
        isSprinting = false;
    }

    if (isFlying) {
        checkGround();
        if (player.onGround) isFlying = false;
    } else {
        checkGround();
    }
}

#endif // GAME_PHYSICS_HPP