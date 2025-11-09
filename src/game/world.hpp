#ifndef GAME_WORLD_HPP
#define GAME_WORLD_HPP

inline void Game::updateChunks() {
    int currentChunkX = static_cast<int>(std::floor(player.x / CHUNK_SIZE));
    int currentChunkZ = static_cast<int>(std::floor(player.z / CHUNK_SIZE));

    if (currentChunkX != lastPlayerChunkX || currentChunkZ != lastPlayerChunkZ) {
        // Send interest update if connected and chunk changed
        if (netClient.isConnected()) {
            if (currentChunkX != lastInterestChunkX || currentChunkZ != lastInterestChunkZ) {
                sendInterestMessage(currentChunkX, currentChunkZ);
                lastInterestChunkX = currentChunkX;
                lastInterestChunkZ = currentChunkZ;
            }
        }

        lastPlayerChunkX = currentChunkX;
        lastPlayerChunkZ = currentChunkZ;
    }
}

inline void Game::updateHighlightedBlock() {
    float maxDistance = 4.5f;
    RaycastHit hit = raycast(maxDistance);
    if (hit.hit) {
        hasHighlightedBlock = true;
        highlightedBlock = hit.blockPosition;
    } else {
        hasHighlightedBlock = false;
    }
}

inline bool Game::findSafeSpawn(float startX, float startZ, Vector3& outPos) {
    int sx = static_cast<int>(std::floor(startX));
    int sz = static_cast<int>(std::floor(startZ));
    const int maxRadiusBlocks = (CHUNK_LOAD_DISTANCE - 1) * CHUNK_SIZE - 2;
    const int maxRadius = std::max(8, maxRadiusBlocks);

    auto tryColumn = [&](int cx, int cz, Vector3& pos) -> bool {
        // In online-only mode, we query existing world data
        // Do not attempt to generate chunks locally

        int h = world.getHeightAt(cx, cz);
        if (h < WATER_LEVEL) return false;

        if (!isBlockSolid(cx, h, cz)) return false;
        if (isWaterBlock(cx, h + 1, cz)) return false;

        float px = cx + 0.5f;
        float pz = cz + 0.5f;
        float py = static_cast<float>(h) + 1.6f;

        if (isColliding(px, py, pz)) return false;

        pos = { px, py, pz };
        return true;
    };

    if (tryColumn(sx, sz, outPos)) return true;

    for (int r = 1; r <= maxRadius; ++r) {
        for (int x = sx - r; x <= sx + r; ++x) {
            Vector3 pos;
            if (tryColumn(x, sz - r, pos)) { outPos = pos; return true; }
            if (tryColumn(x, sz + r, pos)) { outPos = pos; return true; }
        }
        for (int z = sz - r + 1; z <= sz + r - 1; ++z) {
            Vector3 pos;
            if (tryColumn(sx - r, z, pos)) { outPos = pos; return true; }
            if (tryColumn(sx + r, z, pos)) { outPos = pos; return true; }
        }
    }
    return false;
}

inline void Game::removeBlock(int x, int y, int z) {
    Block* block = world.getBlockAt(x, y, z);
    if (!block) return;
    if (block->type == BLOCK_BEDROCK) return;
    // Water blocks cannot be destroyed
    if (block->type == BLOCK_WATER) return;

    // Store block type for particles before removing
    BlockType brokenBlockType = block->type;

    // Send edit message to server (online-only game)
    if (netClient.isConnected()) {
        std::ostringstream msg;
        msg << "{\"op\":\"edit\",\"kind\":\"remove\",\"w\":[" << x << "," << y << "," << z << "]}";
        netClient.send(msg.str());
        std::cout << "[GAME] Sent remove edit: " << x << "," << y << "," << z << std::endl;
    }

    // Apply optimistically
    auto isPlant = [](BlockType t) {
        return t == BLOCK_TALL_GRASS || t == BLOCK_ORANGE_FLOWER || t == BLOCK_BLUE_FLOWER;
    };

    // If you directly broke a plant, just clear it (no particles for plants)
    if (isPlant(block->type)) {
        block->isSolid = false;
        block->type = BLOCK_DIRT;
        int cx = x / CHUNK_SIZE, cy = y / CHUNK_HEIGHT, cz = z / CHUNK_SIZE;
        world.markChunkDirty(cx, cy, cz);
        return;
    }

    // Spawn block breaking particles
    int textureIndex = meshManager.getTextureIndex(brokenBlockType, FACE_TOP);
    particleSystem.spawnBlockBreakParticles(x, y, z, brokenBlockType, textureIndex);

    // Break the supporting block
    block->isSolid = false;
    block->type = BLOCK_DIRT;
    int cx = x / CHUNK_SIZE, cy = y / CHUNK_HEIGHT, cz = z / CHUNK_SIZE;
    std::cout << "[GAME] removeBlock optimistic update at (" << x << "," << y << "," << z << ") chunk (" << cx << "," << cy << "," << cz << ")" << std::endl;
    world.markChunkDirty(cx, cy, cz);

    Block* above = world.getBlockAt(x, y + 1, z);
    if (above && isPlant(above->type)) {
        above->isSolid = false;
        above->type = BLOCK_DIRT;
        int acx = x / CHUNK_SIZE, acy = (y + 1) / CHUNK_HEIGHT, acz = z / CHUNK_SIZE;
        world.markChunkDirty(acx, acy, acz);
    }
}

inline void Game::placeBlock(int x, int y, int z) {
    Block* block = world.getBlockAt(x, y, z);
    if (!block) return;
    // Allow placing blocks in water, plants (flowers/tall grass), or other non-solid blocks
    // Otherwise, the position must not be solid
    if (block->isSolid && block->type != BLOCK_WATER) return;

    float halfWidth = 0.3f;
    float halfDepth = 0.3f;
    float playerMinX = player.x - halfWidth;
    float playerMaxX = player.x + halfWidth;
    float playerMinY = player.y;
    float playerMaxY = player.y + PLAYER_HEIGHT;
    float playerMinZ = player.z - halfDepth;
    float playerMaxZ = player.z + halfDepth;
    float blockMinX = x;
    float blockMaxX = x + 1.0f;
    float blockMinY = y;
    float blockMaxY = y + 1.0f;
    float blockMinZ = z;
    float blockMaxZ = z + 1.0f;
    bool overlapsPlayer = (playerMinX < blockMaxX && playerMaxX > blockMinX) &&
                           (playerMinY < blockMaxY && playerMaxY > blockMinY) &&
                           (playerMinZ < blockMaxZ && playerMaxZ > blockMinZ);
    if (overlapsPlayer) return;

    // Get the selected block type from inventory
    BlockType selectedBlockType = hotbarInventory.getSelectedBlockType();

    // Convert BlockType to string for network message
    const char* blockTypeStr;
    switch (selectedBlockType) {
        case BLOCK_STONE:    blockTypeStr = "STONE"; break;
        case BLOCK_DIRT:     blockTypeStr = "DIRT"; break;
        case BLOCK_GRASS:    blockTypeStr = "GRASS"; break;
        case BLOCK_PLANKS:   blockTypeStr = "PLANKS"; break;
        case BLOCK_LOG:      blockTypeStr = "LOG"; break;
        case BLOCK_BEDROCK:  blockTypeStr = "BEDROCK"; break;
        case BLOCK_COAL_ORE: blockTypeStr = "COAL_ORE"; break;
        case BLOCK_IRON_ORE: blockTypeStr = "IRON_ORE"; break;
        case BLOCK_LEAVES:   blockTypeStr = "LEAVES"; break;
        case BLOCK_WATER:    blockTypeStr = "WATER"; break;
        case BLOCK_SAND:     blockTypeStr = "SAND"; break;
        case BLOCK_COBBLESTONE: blockTypeStr = "COBBLESTONE"; break;
        case BLOCK_GLASS:    blockTypeStr = "GLASS"; break;
        case BLOCK_CLAY:     blockTypeStr = "CLAY"; break;
        default:             blockTypeStr = "STONE"; break;
    }

    // Send edit message to server (online-only game)
    if (netClient.isConnected()) {
        std::ostringstream msg;
        msg << "{\"op\":\"edit\",\"kind\":\"place\",\"w\":[" << x << "," << y << "," << z << "],\"type\":\"" << blockTypeStr << "\"}";
        netClient.send(msg.str());
        std::cout << "[GAME] Sent place edit: " << x << "," << y << "," << z << " with type " << blockTypeStr << std::endl;
    }

    // Apply optimistically
    block->isSolid = true;
    block->type = selectedBlockType;

    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_HEIGHT;
    int cz = z / CHUNK_SIZE;
    std::cout << "[GAME] placeBlock optimistic update at (" << x << "," << y << "," << z << ") chunk (" << cx << "," << cy << "," << cz << ")" << std::endl;
    world.markChunkDirty(cx, cy, cz);
}

#endif // GAME_WORLD_HPP