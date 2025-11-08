// server/hub.cpp
// Hub implementation for client management and protocol handling
#include "hub.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>

void Hub::addClient(std::shared_ptr<ClientSession> client) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    std::string clientId = "client" + std::to_string(nextClientId++);
    clients[client] = clientId;
    std::cout << "[HUB] Added " << clientId << " (total: " << clients.size() << ")" << std::endl;
}

void Hub::calculateSafeSpawnPoint() {
    const int MIN_DISTANCE_FROM_WATER = 5;
    
    std::cout << "[HUB] Calculating safe spawn point (min " << MIN_DISTANCE_FROM_WATER << " blocks from water)..." << std::endl;
    
    // Load chunks around the default spawn area
    int spawnChunkX = static_cast<int>(SPAWN_X) / CHUNK_SIZE;
    int spawnChunkZ = static_cast<int>(SPAWN_Z) / CHUNK_SIZE;
    
    // First pass: Generate base terrain and water
    for (int dcx = -4; dcx <= 4; ++dcx) {
        for (int dcz = -4; dcz <= 4; ++dcz) {
            int cx = spawnChunkX + dcx;
            int cz = spawnChunkZ + dcz;
            
            if (cx < 0 || cx >= WORLD_CHUNK_SIZE_X || cz < 0 || cz >= WORLD_CHUNK_SIZE_Z) continue;
            
            // Generate all vertical chunks in this column
            for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                world.generateChunk(cx, cy, cz);
                world.generateWater(cx, cy, cz);
            }
        }
    }
    
    // Second pass: Caves, ores, and surface updates
    for (int dcx = -4; dcx <= 4; ++dcx) {
        for (int dcz = -4; dcz <= 4; ++dcz) {
            int cx = spawnChunkX + dcx;
            int cz = spawnChunkZ + dcz;
            
            if (cx < 0 || cx >= WORLD_CHUNK_SIZE_X || cz < 0 || cz >= WORLD_CHUNK_SIZE_Z) continue;
            
            for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                Chunk* chunk = world.getChunk(cx, cy, cz);
                if (chunk && chunk->isGenerated && !chunk->isFullyProcessed) {
                    world.generateCaves(cx, cy, cz);
                    world.generateOres(cx, cy, cz);
                    world.updateSurfaceBlocks(cx, cy, cz);
                }
            }
        }
    }
    
    // Third pass: Trees and foliage
    for (int dcx = -4; dcx <= 4; ++dcx) {
        for (int dcz = -4; dcz <= 4; ++dcz) {
            int cx = spawnChunkX + dcx;
            int cz = spawnChunkZ + dcz;
            
            if (cx < 0 || cx >= WORLD_CHUNK_SIZE_X || cz < 0 || cz >= WORLD_CHUNK_SIZE_Z) continue;
            
            world.generateTreesForColumn(cx, cz);
            world.generateFoliageForColumn(cx, cz);
            
            // Mark all chunks in this column as fully processed
            for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                Chunk* chunk = world.getChunk(cx, cy, cz);
                if (chunk) {
                    chunk->isFullyProcessed = true;
                }
            }
        }
    }
    
    // Try to find a safe spawn point
    if (world.findSafeSpawnPoint(SPAWN_X, SPAWN_Z, MIN_DISTANCE_FROM_WATER, spawnX, spawnY, spawnZ)) {
        std::cout << "[HUB] Safe spawn point found at (" << spawnX << ", " << spawnY << ", " << spawnZ << ")" << std::endl;
    } else {
        std::cout << "[HUB] Warning: Could not find safe spawn point away from water, using default" << std::endl;
        spawnX = SPAWN_X;
        spawnY = SPAWN_Y;
        spawnZ = SPAWN_Z;
    }
}

void Hub::removeClient(std::shared_ptr<ClientSession> client) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = clients.find(client);
    if (it != clients.end()) {
        std::cout << "[HUB] Removed " << it->second << std::endl;
        clients.erase(it);
    }
}

void Hub::handleMessage(std::shared_ptr<ClientSession> client, const std::string& message) {
    // Simple JSON parsing - extract "op" field
    size_t opPos = message.find("\"op\":");
    if (opPos == std::string::npos) {
        std::cerr << "[HUB] Invalid message: no op field" << std::endl;
        return;
    }
    
    size_t opStart = message.find("\"", opPos + 5);
    size_t opEnd = message.find("\"", opStart + 1);
    if (opStart == std::string::npos || opEnd == std::string::npos) {
        std::cerr << "[HUB] Invalid message: malformed op" << std::endl;
        return;
    }
    
    std::string op = message.substr(opStart + 1, opEnd - opStart - 1);
    // std::cout << "[HUB] Handling op: " << op << std::endl;
    
    if (op == ClientOp::HELLO) {
        handleHello(client, message);
    } else if (op == ClientOp::SET_INTEREST) {
        handleSetInterest(client, message);
    } else if (op == ClientOp::POSE) {
        // Parse pose: {"op":"pose","x":10.5,"y":20.0,"z":30.5,"yaw":45.0,"pitch":10.0}
        size_t xPos = message.find("\"x\":");
        size_t yPos = message.find("\"y\":");
        size_t zPos = message.find("\"z\":");
        size_t yawPos = message.find("\"yaw\":");
        size_t pitchPos = message.find("\"pitch\":");
        
        if (xPos != std::string::npos && yPos != std::string::npos && zPos != std::string::npos) {
            client->lastPoseX = std::stof(message.substr(message.find(":", xPos) + 1));
            client->lastPoseY = std::stof(message.substr(message.find(":", yPos) + 1));
            client->lastPoseZ = std::stof(message.substr(message.find(":", zPos) + 1));
            
            if (yawPos != std::string::npos) {
                client->lastYaw = std::stof(message.substr(message.find(":", yawPos) + 1));
            }
            if (pitchPos != std::string::npos) {
                client->lastPitch = std::stof(message.substr(message.find(":", pitchPos) + 1));
            }
            
            client->lastPoseUpdate = std::chrono::steady_clock::now();
            
            // Broadcast updated player positions to all clients
            broadcastPlayerSnapshot();
        }
    } else if (op == ClientOp::EDIT) {
        handleEdit(client, message);
    } else {
        std::cerr << "[HUB] Unknown op: " << op << std::endl;
    }
}

void Hub::handleHello(std::shared_ptr<ClientSession> client, const std::string& message) {
    // Extract protocol version
    size_t protoPos = message.find("\"proto\":");
    int clientProto = 1;
    if (protoPos != std::string::npos) {
        size_t protoStart = protoPos + 8;
        size_t protoEnd = message.find_first_of(",}", protoStart);
        std::string protoStr = message.substr(protoStart, protoEnd - protoStart);
        clientProto = std::stoi(protoStr);
    }
    
    std::cout << "[HUB] Client hello (proto: " << clientProto << ")" << std::endl;
    
    // Protocol version gating
    if (clientProto != PROTOCOL_VERSION) {
        std::cerr << "[HUB] Protocol mismatch: client=" << clientProto 
                  << ", server=" << PROTOCOL_VERSION << " - rejecting" << std::endl;
        std::ostringstream error;
        error << "{\"op\":\"error\""
              << ",\"reason\":\"protocol_mismatch\""
              << ",\"client_proto\":" << clientProto
              << ",\"server_proto\":" << PROTOCOL_VERSION
              << ",\"message\":\"Client protocol " << clientProto 
              << " incompatible with server protocol " << PROTOCOL_VERSION << "\"}";
        client->send(error.str());
        // Close connection after error
        return;
    }
    
    // Get this client's ID
    std::string clientId;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = clients.find(client);
        if (it != clients.end()) {
            clientId = it->second;
        }
    }
    
    // Send hello_ok with client ID
    std::ostringstream response;
    response << "{\"op\":\"" << ServerOp::HELLO_OK << "\""
             << ",\"client_id\":\"" << clientId << "\""
             << ",\"server_version\":\"1.0.0\""
             << ",\"proto\":" << PROTOCOL_VERSION
             << ",\"seed\":" << PERLIN_SEED
             << ",\"world_size\":[" << WORLD_SIZE_X << "," << WORLD_SIZE_Y << "," << WORLD_SIZE_Z << "]"
             << ",\"chunk_size\":[" << CHUNK_SIZE << "," << CHUNK_HEIGHT << "," << CHUNK_SIZE << "]"
             << ",\"spawn\":[" << spawnX << "," << spawnY << "," << spawnZ << "]"
             << "}";
    
    std::string responseStr = response.str();
    std::cout << "[HUB] → hello_ok (client_id: " << clientId << ", spawn: [" << spawnX << "," << spawnY << "," << spawnZ << "])" << std::endl;
    client->send(responseStr);
}

void Hub::handleSetInterest(std::shared_ptr<ClientSession> client, const std::string& message) {
    // Parse center: [cx, cz]
    size_t centerPos = message.find("\"center\":");
    if (centerPos == std::string::npos) return;
    
    size_t bracketStart = message.find("[", centerPos);
    size_t comma = message.find(",", bracketStart);
    size_t bracketEnd = message.find("]", comma);
    
    if (bracketStart == std::string::npos || comma == std::string::npos || bracketEnd == std::string::npos) {
        std::cerr << "[HUB] Malformed center array" << std::endl;
        return;
    }
    
    int cx = std::stoi(message.substr(bracketStart + 1, comma - bracketStart - 1));
    int cz = std::stoi(message.substr(comma + 1, bracketEnd - comma - 1));
    
    // Parse radius
    size_t radiusPos = message.find("\"radius\":");
    int radius = RENDER_DISTANCE;
    if (radiusPos != std::string::npos) {
        size_t radiusStart = radiusPos + 9;
        size_t radiusEnd = message.find_first_of(",}", radiusStart);
        radius = std::stoi(message.substr(radiusStart, radiusEnd - radiusStart));
    }
    
    // std::cout << "[HUB] Set interest: center=(" << cx << "," << cz << "), radius=" << radius << std::endl;
    
    // Calculate new AOI
    std::unordered_set<ChunkCoord, std::hash<ChunkCoord>> newAOI;
    for (int dcx = -radius; dcx <= radius; ++dcx) {
        for (int dcz = -radius; dcz <= radius; ++dcz) {
            if (dcx * dcx + dcz * dcz > radius * radius) continue;
            
            int chunkX = cx + dcx;
            int chunkZ = cz + dcz;
            
            if (chunkX < 0 || chunkX >= WORLD_CHUNK_SIZE_X) continue;
            if (chunkZ < 0 || chunkZ >= WORLD_CHUNK_SIZE_Z) continue;
            
            // Add all vertical chunks
            for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                newAOI.insert({chunkX, cy, chunkZ});
            }
        }
    }
    
    auto& oldAOI = client->getAOI();
    
    // Find chunks to add (in new but not in old)
    std::vector<ChunkCoord> toAdd;
    for (const auto& coord : newAOI) {
        if (oldAOI.find(coord) == oldAOI.end()) {
            toAdd.push_back(coord);
        }
    }
    
    // Find chunks to remove (in old but not in new)
    std::vector<ChunkCoord> toRemove;
    for (const auto& coord : oldAOI) {
        if (newAOI.find(coord) == newAOI.end()) {
            toRemove.push_back(coord);
        }
    }
    
    // std::cout << "[HUB] AOI delta: +" << toAdd.size() << " -" << toRemove.size() << std::endl;
    
    // Group chunks by column (cx, cz) to ensure proper generation order
    std::map<std::pair<int, int>, std::vector<int>> columnChunks; // (cx,cz) -> [cy values]
    for (const auto& coord : toAdd) {
        columnChunks[{coord.x, coord.z}].push_back(coord.y);
    }
    
    // Generate columns completely before decorating
    for (auto& [column, yLayers] : columnChunks) {
        int cx = column.first;
        int cz = column.second;
        
        // Sort y layers to process bottom to top
        std::sort(yLayers.begin(), yLayers.end());
        
        bool columnNeedsGeneration = false;
        
        // First pass: generate terrain for all chunks in column
        for (int cy : yLayers) {
            Chunk* chunk = world.getChunk(cx, cy, cz);
            
            if (!chunk || !chunk->isGenerated) {
                columnNeedsGeneration = true;
                
                // Try to load saved chunk first
                bool loadedFromDisk = loadChunkIfSaved(cx, cy, cz);
                
                if (!loadedFromDisk) {
                    // Generate base terrain
                    world.generateChunk(cx, cy, cz);
                    world.generateWater(cx, cy, cz);
                }
            }
        }
        
        // Second pass: caves and ores (needs all terrain in place)
        if (columnNeedsGeneration) {
            for (int cy : yLayers) {
                Chunk* chunk = world.getChunk(cx, cy, cz);
                if (chunk && chunk->isGenerated && !chunk->isFullyProcessed) {
                    world.generateCaves(cx, cy, cz);
                    world.generateOres(cx, cy, cz);
                    world.updateSurfaceBlocks(cx, cy, cz);
                }
            }
            
            // Third pass: decorations (trees/foliage) - only if ENTIRE column is now loaded
            bool entireColumnLoaded = true;
            for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                Chunk* chunk = world.getChunk(cx, cy, cz);
                if (!chunk || !chunk->isGenerated) {
                    entireColumnLoaded = false;
                    break;
                }
            }
            
            if (entireColumnLoaded) {
                world.generateTreesForColumn(cx, cz);
                world.generateFoliageForColumn(cx, cz);
            }
            
            // Mark all chunks as fully processed
            for (int cy : yLayers) {
                Chunk* chunk = world.getChunk(cx, cy, cz);
                if (chunk) {
                    chunk->isFullyProcessed = true;
                }
            }
        }
    }
    
    // Send all chunks to client
    for (const auto& coord : toAdd) {
        sendChunkFull(client, coord.x, coord.y, coord.z);
    }
    
    // Send unload messages
    for (const auto& coord : toRemove) {
        sendChunkUnload(client, coord.x, coord.y, coord.z);
    }
    
    // Update client's AOI
    oldAOI = newAOI;
}

void Hub::sendChunkFull(std::shared_ptr<ClientSession> client, int cx, int cy, int cz) {
    Chunk* chunk = world.getChunk(cx, cy, cz);
    if (!chunk || !chunk->isGenerated) {
        std::cerr << "[HUB] Chunk not generated: " << cx << "," << cy << "," << cz << std::endl;
        return;
    }
    
    // Get current revision
    ChunkCoord coord{cx, cy, cz};
    int rev = chunkRevisions[coord]; // defaults to 0 if not set
    
    // Check cache
    ChunkCacheKey cacheKey{cx, cy, cz, rev};
    std::string base64Data;
    
    if (chunkCache.count(cacheKey) > 0) {
        // Cache hit!
        base64Data = chunkCache[cacheKey];
        // std::cout << "[HUB] → chunk_full(" << cx << "," << cy << "," << cz 
        //           << ") [CACHED, rev=" << rev << "]" << std::endl;
    } else {
        // Cache miss - serialize
        const int totalBlocks = CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE;
        std::vector<uint8_t> types(totalBlocks);
        std::vector<uint8_t> solids(totalBlocks);
        
        int idx = 0;
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    types[idx] = static_cast<uint8_t>(chunk->blocks[x][y][z].type);
                    solids[idx] = chunk->blocks[x][y][z].isSolid ? 1 : 0;
                    ++idx;
                }
            }
        }
        
        // RLE encode and base64
        std::vector<uint8_t> encoded = Serialization::encodeChunk(types.data(), solids.data());
        base64Data = Serialization::base64_encode(encoded);
        
        // Store in cache
        chunkCache[cacheKey] = base64Data;
        
        // std::cout << "[HUB] → chunk_full(" << cx << "," << cy << "," << cz 
        //           << ") [" << base64Data.length() << " chars, " << encoded.size() 
        //           << " bytes RLE, rev=" << rev << "]" << std::endl;
    }
    
    // Send chunk_full message
    std::ostringstream response;
    response << "{\"op\":\"" << ServerOp::CHUNK_FULL << "\""
             << ",\"cx\":" << cx
             << ",\"cy\":" << cy
             << ",\"cz\":" << cz
             << ",\"rev\":" << rev
             << ",\"data\":\"" << base64Data << "\""
             << "}";
    
    client->send(response.str());
}

void Hub::sendChunkUnload(std::shared_ptr<ClientSession> client, int cx, int cy, int cz) {
    std::ostringstream response;
    response << "{\"op\":\"" << ServerOp::CHUNK_UNLOAD << "\""
             << ",\"cx\":" << cx
             << ",\"cy\":" << cy
             << ",\"cz\":" << cz
             << "}";
    
    std::string responseStr = response.str();
    // std::cout << "[HUB] → chunk_unload(" << cx << "," << cy << "," << cz << ")" << std::endl;
    client->send(responseStr);
}

void Hub::handleEdit(std::shared_ptr<ClientSession> client, const std::string& message) {
    // Rate limiting - refill tokens
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - client->lastEditTime).count();
    client->editTokens = std::min(60, client->editTokens + static_cast<int>(elapsed / 50)); // 20/sec
    client->lastEditTime = now;
    
    if (client->editTokens < 1) {
        std::cerr << "[HUB] Edit rate limit exceeded for client" << std::endl;
        return;
    }
    client->editTokens--;
    
    // Parse message: {"op":"edit","kind":"place|remove","w":[x,y,z],"type":"PLANKS"}
    size_t kindPos = message.find("\"kind\":");
    size_t wPos = message.find("\"w\":");
    size_t typePos = message.find("\"type\":");
    
    if (kindPos == std::string::npos || wPos == std::string::npos) {
        std::cerr << "[HUB] Invalid edit message" << std::endl;
        return;
    }
    
    // Extract kind
    size_t kindStart = message.find("\"", kindPos + 7);
    size_t kindEnd = message.find("\"", kindStart + 1);
    std::string kind = message.substr(kindStart + 1, kindEnd - kindStart - 1);
    
    // Extract world coordinates [x,y,z]
    size_t wArrayStart = message.find("[", wPos);
    size_t wArrayEnd = message.find("]", wArrayStart);
    std::string wArray = message.substr(wArrayStart + 1, wArrayEnd - wArrayStart - 1);
    
    int wx, wy, wz;
    if (sscanf(wArray.c_str(), "%d,%d,%d", &wx, &wy, &wz) != 3) {
        std::cerr << "[HUB] Invalid coordinates in edit" << std::endl;
        return;
    }
    
    // Validate bounds
    if (wx < 0 || wx >= WORLD_SIZE_X || wy < 0 || wy >= WORLD_SIZE_Y || wz < 0 || wz >= WORLD_SIZE_Z) {
        std::cerr << "[HUB] Edit out of bounds: " << wx << "," << wy << "," << wz << std::endl;
        return;
    }
    
    // Forbid breaking bedrock (y=0)
    if (kind == "remove" && wy == 0) {
        std::cerr << "[HUB] Cannot break bedrock" << std::endl;
        return;
    }
    
    // Clamp to nearby blocks (distance <= 6 from last pose)
    float dx = wx - client->lastPoseX;
    float dy = wy - client->lastPoseY;
    float dz = wz - client->lastPoseZ;
    float distSq = dx*dx + dy*dy + dz*dz;
    if (distSq > 36.0f) { // 6*6
        std::cerr << "[HUB] Edit too far from player: dist=" << std::sqrt(distSq) << std::endl;
        return;
    }
    
    // Apply edit
    uint8_t newType = 0;
    bool newSolid = false;
    
    if (kind == "remove") {
        newType = static_cast<uint8_t>(BLOCK_DIRT); // Use DIRT as "empty" (or could be STONE)
        newSolid = false;
    } else if (kind == "place") {
        // Extract block type
        if (typePos == std::string::npos) {
            std::cerr << "[HUB] Place edit missing type" << std::endl;
            return;
        }
        size_t typeStart = message.find("\"", typePos + 7);
        size_t typeEnd = message.find("\"", typeStart + 1);
        std::string typeStr = message.substr(typeStart + 1, typeEnd - typeStart - 1);
        
        // Map type string to BlockType
        if (typeStr == "DIRT") newType = static_cast<uint8_t>(BLOCK_DIRT);
        else if (typeStr == "GRASS") newType = static_cast<uint8_t>(BLOCK_GRASS);
        else if (typeStr == "STONE") newType = static_cast<uint8_t>(BLOCK_STONE);
        else if (typeStr == "SAND") newType = static_cast<uint8_t>(BLOCK_SAND);
        else if (typeStr == "PLANKS") newType = static_cast<uint8_t>(BLOCK_PLANKS);
        else if (typeStr == "LEAVES") newType = static_cast<uint8_t>(BLOCK_LEAVES);
        else if (typeStr == "WATER") newType = static_cast<uint8_t>(BLOCK_WATER);
        else if (typeStr == "COAL_ORE") newType = static_cast<uint8_t>(BLOCK_COAL_ORE);
        else if (typeStr == "IRON_ORE") newType = static_cast<uint8_t>(BLOCK_IRON_ORE);
        else {
            newType = static_cast<uint8_t>(BLOCK_DIRT); // Default
        }
        newSolid = true;
    } else {
        std::cerr << "[HUB] Unknown edit kind: " << kind << std::endl;
        return;
    }
    
    // Apply to world
    int cx = wx / CHUNK_SIZE;
    int cy = wy / CHUNK_HEIGHT;
    int cz = wz / CHUNK_SIZE;
    
    const Block* oldBlock = world.getBlockAt(wx, wy, wz);
    if (!oldBlock) {
        std::cerr << "[HUB] Block not in loaded chunk" << std::endl;
        return;
    }
    
    // Set block and mark chunk dirty
    world.setBlockAndMarkDirty(wx, wy, wz, static_cast<BlockType>(newType), newSolid);
    
    // If we removed a block, check if there's a plant above it that should also be removed
    if (kind == "remove") {
        int aboveY = wy + 1;
        if (aboveY < WORLD_SIZE_Y) {
            const Block* aboveBlock = world.getBlockAt(wx, aboveY, wz);
            if (aboveBlock) {
                // Check if the block above is a plant (tall grass or flower)
                bool isPlant = (aboveBlock->type == BLOCK_TALL_GRASS || 
                               aboveBlock->type == BLOCK_ORANGE_FLOWER || 
                               aboveBlock->type == BLOCK_BLUE_FLOWER);
                
                if (isPlant) {
                    // Remove the plant above
                    world.setBlockAndMarkDirty(wx, aboveY, wz, BLOCK_DIRT, false);
                    
                    // Increment revision for the chunk containing the plant
                    int acx = wx / CHUNK_SIZE;
                    int acy = aboveY / CHUNK_HEIGHT;
                    int acz = wz / CHUNK_SIZE;
                    ChunkCoord aboveCoord{acx, acy, acz};
                    chunkRevisions[aboveCoord]++;
                    
                    // Mark chunk as modified for saving
                    markChunkModified(acx, acy, acz);
                    
                    // Broadcast the plant removal to all interested clients
                    broadcastBlockUpdate(wx, aboveY, wz, static_cast<uint8_t>(BLOCK_DIRT), false);
                }
            }
        }
    }
    
    // Increment chunk revision
    ChunkCoord coord{cx, cy, cz};
    chunkRevisions[coord]++;
    
    // Mark chunk as modified for saving
    markChunkModified(cx, cy, cz);
    
    std::cout << "[HUB] Edit applied: " << kind << " at (" << wx << "," << wy << "," << wz 
              << ") type=" << (int)newType << " rev=" << chunkRevisions[coord] << std::endl;
    
    // Broadcast to all clients in AOI
    broadcastBlockUpdate(wx, wy, wz, newType, newSolid);
}

void Hub::broadcastBlockUpdate(int wx, int wy, int wz, uint8_t blockType, bool isSolid) {
    // Determine chunk
    int cx = wx / CHUNK_SIZE;
    int cy = wy / CHUNK_HEIGHT;
    int cz = wz / CHUNK_SIZE;
    ChunkCoord coord{cx, cy, cz};
    
    int rev = chunkRevisions[coord];
    
    // Build block_update message
    std::ostringstream msg;
    msg << "{\"op\":\"" << ServerOp::BLOCK_UPDATE << "\""
        << ",\"w\":[" << wx << "," << wy << "," << wz << "]"
        << ",\"type\":" << (int)blockType
        << ",\"solid\":" << (isSolid ? "true" : "false")
        << ",\"cx\":" << cx
        << ",\"cy\":" << cy
        << ",\"cz\":" << cz
        << ",\"rev\":" << rev
        << "}";
    
    std::string msgStr = msg.str();
    
    // Broadcast to all clients whose AOI includes this chunk
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto& [clientPtr, clientId] : clients) {
        if (clientPtr->getAOI().count(coord) > 0) {
            clientPtr->send(msgStr);
        }
    }
    
    std::cout << "[HUB] Broadcast block_update(" << wx << "," << wy << "," << wz 
              << ") to clients in AOI of chunk (" << cx << "," << cy << "," << cz << ")" << std::endl;
}

void Hub::markChunkModified(int cx, int cy, int cz) {
    ChunkCoord coord{cx, cy, cz};
    modifiedChunks.insert(coord);
}

void Hub::saveWorld() {
    if (modifiedChunks.empty()) {
        std::cout << "[HUB] No modified chunks to save" << std::endl;
        return;
    }
    
    std::cout << "[HUB] Saving " << modifiedChunks.size() << " modified chunks..." << std::endl;
    
    // Create save directory if it doesn't exist
    std::filesystem::create_directories(worldSaveDir);
    
    int savedCount = 0;
    for (const auto& coord : modifiedChunks) {
        auto chunkIt = world.chunks.find(coord);
        if (chunkIt == world.chunks.end() || !chunkIt->second) {
            continue; // Chunk not loaded
        }
        
        const Chunk* chunk = chunkIt->second.get();
        
        // Build file path: world_save/chunk_X_Y_Z.dat
        std::ostringstream filename;
        filename << worldSaveDir << "/chunk_" << coord.x << "_" << coord.y << "_" << coord.z << ".dat";
        
        // Open file for binary writing
        std::ofstream file(filename.str(), std::ios::binary);
        if (!file) {
            std::cerr << "[HUB] Failed to open " << filename.str() << " for writing" << std::endl;
            continue;
        }
        
        // Write chunk data: simple format - just dump all blocks
        // Format: [CHUNK_SIZE^2 * CHUNK_HEIGHT] blocks, each 2 bytes (type + solid flag)
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    const Block& block = chunk->blocks[y][z][x];
                    uint8_t typeVal = static_cast<uint8_t>(block.type);
                    uint8_t solidVal = block.isSolid ? 1 : 0;
                    file.write(reinterpret_cast<const char*>(&typeVal), sizeof(typeVal));
                    file.write(reinterpret_cast<const char*>(&solidVal), sizeof(solidVal));
                }
            }
        }
        
        file.close();
        savedCount++;
    }
    
    std::cout << "[HUB] Saved " << savedCount << " chunks to " << worldSaveDir << std::endl;
}

bool Hub::loadChunkIfSaved(int cx, int cy, int cz) {
    // Build file path
    std::ostringstream filename;
    filename << worldSaveDir << "/chunk_" << cx << "_" << cy << "_" << cz << ".dat";
    
    std::string filepath = filename.str();
    if (!std::filesystem::exists(filepath)) {
        return false; // No saved data for this chunk
    }
    
    // Open file for binary reading
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "[HUB] Failed to open " << filepath << " for reading" << std::endl;
        return false;
    }
    
    // Ensure chunk exists (generate if needed)
    ChunkCoord coord{cx, cy, cz};
    if (world.chunks.find(coord) == world.chunks.end()) {
        world.generateChunk(cx, cy, cz);
    }
    
    Chunk* chunk = world.chunks[coord].get();
    if (!chunk) {
        file.close();
        return false;
    }
    
    // Read chunk data
    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                uint8_t typeVal, solidVal;
                file.read(reinterpret_cast<char*>(&typeVal), sizeof(typeVal));
                file.read(reinterpret_cast<char*>(&solidVal), sizeof(solidVal));
                
                if (!file) {
                    std::cerr << "[HUB] Incomplete chunk data in " << filepath << std::endl;
                    file.close();
                    return false;
                }
                
                chunk->blocks[y][z][x].type = static_cast<BlockType>(typeVal);
                chunk->blocks[y][z][x].isSolid = (solidVal != 0);
            }
        }
    }
    
    file.close();
    
    // Initialize chunk revision if not set
    if (chunkRevisions.find(coord) == chunkRevisions.end()) {
        chunkRevisions[coord] = 0;
    }
    
    std::cout << "[HUB] Loaded saved chunk (" << cx << "," << cy << "," << cz << ") from disk" << std::endl;
    return true;
}

void Hub::loadWorld() {
    if (!std::filesystem::exists(worldSaveDir)) {
        std::cout << "[HUB] No saved world found at " << worldSaveDir << std::endl;
        return;
    }
    
    std::cout << "[HUB] Loading saved world from " << worldSaveDir << "..." << std::endl;
    
    int loadedCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(worldSaveDir)) {
        if (!entry.is_regular_file()) continue;
        
        std::string filename = entry.path().filename().string();
        
        // Parse filename: chunk_X_Y_Z.dat
        if (filename.find("chunk_") != 0 || filename.find(".dat") == std::string::npos) {
            continue;
        }
        
        // Extract coordinates
        size_t pos = 6; // After "chunk_"
        size_t next = filename.find('_', pos);
        if (next == std::string::npos) continue;
        int cx = std::stoi(filename.substr(pos, next - pos));
        
        pos = next + 1;
        next = filename.find('_', pos);
        if (next == std::string::npos) continue;
        int cy = std::stoi(filename.substr(pos, next - pos));
        
        pos = next + 1;
        next = filename.find('.', pos);
        if (next == std::string::npos) continue;
        int cz = std::stoi(filename.substr(pos, next - pos));
        
        // Open file for binary reading
        std::ifstream file(entry.path(), std::ios::binary);
        if (!file) {
            std::cerr << "[HUB] Failed to open " << entry.path() << " for reading" << std::endl;
            continue;
        }
        
        // Ensure chunk exists (generate if needed)
        ChunkCoord coord{cx, cy, cz};
        if (world.chunks.find(coord) == world.chunks.end()) {
            world.generateChunk(cx, cy, cz);
        }
        
        Chunk* chunk = world.chunks[coord].get();
        if (!chunk) continue;
        
        // Read chunk data
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    uint8_t typeVal, solidVal;
                    file.read(reinterpret_cast<char*>(&typeVal), sizeof(typeVal));
                    file.read(reinterpret_cast<char*>(&solidVal), sizeof(solidVal));
                    
                    if (!file) {
                        std::cerr << "[HUB] Incomplete chunk data in " << filename << std::endl;
                        goto next_file;
                    }
                    
                    chunk->blocks[y][z][x].type = static_cast<BlockType>(typeVal);
                    chunk->blocks[y][z][x].isSolid = (solidVal != 0);
                }
            }
        }
        
        // Mark chunk as loaded and initialize revision
        chunkRevisions[coord] = 0;
        loadedCount++;
        
        next_file:
        file.close();
    }
    
    std::cout << "[HUB] Loaded " << loadedCount << " chunks from disk" << std::endl;
}

void Hub::broadcastPlayerSnapshot() {
    std::lock_guard<std::mutex> lock(clientsMutex);
    
    if (clients.empty()) {
        return;
    }
    
    // Send personalized snapshot to each client (excluding themselves)
    for (const auto& receiverEntry : clients) {
        auto receiverClient = receiverEntry.first;
        const std::string& receiverClientId = receiverEntry.second;
        
        // Build player snapshot message for this specific client
        std::ostringstream json;
        json << "{\"op\":\"" << ServerOp::PLAYER_SNAPSHOT << "\",\"players\":[";
        
        bool first = true;
        for (const auto& playerEntry : clients) {
            auto playerClient = playerEntry.first;
            const std::string& playerClientId = playerEntry.second;
            
            // Skip the receiving client (don't send themselves)
            if (playerClient == receiverClient) {
                continue;
            }
            
            if (!first) json << ",";
            first = false;
            
            json << "{\"id\":\"" << playerClientId << "\""
                 << ",\"x\":" << playerClient->lastPoseX
                 << ",\"y\":" << playerClient->lastPoseY
                 << ",\"z\":" << playerClient->lastPoseZ
                 << ",\"yaw\":" << playerClient->lastYaw
                 << ",\"pitch\":" << playerClient->lastPitch
                 << "}";
        }
        
        json << "]}";
        
        // Send to this specific client
        receiverClient->send(json.str());
    }
}
