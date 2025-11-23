#ifndef GAME_NETWORK_HPP
#define GAME_NETWORK_HPP

inline void Game::sendHelloMessage() {
    std::ostringstream json;
    json << "{\"op\":\"" << ClientOp::HELLO << "\""
         << ",\"proto\":" << PROTOCOL_VERSION
         << ",\"username\":\"" << usernameInput << "\"}";
    netClient.send(json.str());
}

inline void Game::sendInterestMessage(int centerX, int centerZ) {
    std::ostringstream json;
    json << "{\"op\":\"" << ClientOp::SET_INTEREST
         << "\",\"center\":[" << centerX << "," << centerZ
         << "],\"radius\":" << RENDER_DISTANCE << "}";
    netClient.send(json.str());
}

inline void Game::sendPoseUpdate() {
    std::ostringstream json;
    json << "{\"op\":\"" << ClientOp::POSE
         << "\",\"x\":" << player.x
         << ",\"y\":" << player.y
         << ",\"z\":" << player.z
         << ",\"yaw\":" << camera.yaw
         << ",\"pitch\":" << camera.pitch << "}";
    netClient.send(json.str());
}

inline void Game::handleServerMessage(const std::string& message) {
    // Simple JSON parsing - look for "op" field
    size_t opPos = message.find("\"op\":");
    if (opPos == std::string::npos) {
        std::cerr << "[GAME] Invalid message: no op field" << std::endl;
        return;
    }

    // Extract op value (very simple parser)
    size_t opStart = message.find("\"", opPos + 5);
    size_t opEnd = message.find("\"", opStart + 1);
    if (opStart == std::string::npos || opEnd == std::string::npos) {
        std::cerr << "[GAME] Invalid message: malformed op" << std::endl;
        return;
    }

    std::string op = message.substr(opStart + 1, opEnd - opStart - 1);
    // std::cout << "[GAME] Handling message op: " << op << std::endl;

    if (op == ServerOp::HELLO_OK) {
        handleHelloOk(message);
    } else if (op == ServerOp::AUTH_ERROR) {
        handleAuthError(message);
    } else if (op == ServerOp::CHUNK_FULL) {
        handleChunkFull(message);
    } else if (op == ServerOp::CHUNK_UNLOAD) {
        handleChunkUnload(message);
    } else if (op == ServerOp::PLAYER_SNAPSHOT) {
        handlePlayerSnapshot(message);
    } else if (op == ServerOp::BLOCK_UPDATE) {
        handleBlockUpdate(message);
    } else if (op == ServerOp::CHAT_MESSAGE) {
        handleChatMessage(message);
    } else if (op == ServerOp::SYSTEM_MESSAGE) {
        handleSystemMessage(message);
    } else {
        std::cerr << "[GAME] Unknown op: " << op << std::endl;
    }
}

inline void Game::handleHelloOk(const std::string& message) {
    // Parse: {"op":"hello_ok","username":"Player1", ... , "spawn":[x,y,z]}
    size_t usernamePos = message.find("\"username\":\"");
    if (usernamePos != std::string::npos) {
        size_t usernameStart = usernamePos + 12;  // Length of "username":""
        size_t usernameEnd = message.find("\"", usernameStart);
        if (usernameEnd != std::string::npos) {
            myUsername = message.substr(usernameStart, usernameEnd - usernameStart);
            std::cout << "[GAME] ✓ Server accepted authentication (username: " << myUsername << ")" << std::endl;
        }
    } else {
        std::cout << "[GAME] ✓ Server accepted hello" << std::endl;
    }

    size_t spawnPos = message.find("\"spawn\":[");
    if (spawnPos != std::string::npos) {
        size_t bracketStart = message.find("[", spawnPos);
        size_t bracketEnd = message.find("]", bracketStart);
        if (bracketStart != std::string::npos && bracketEnd != std::string::npos) {
            std::string coordsStr = message.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
            std::stringstream ss(coordsStr);
            std::string token;
            float spawnCoords[3];
            int index = 0;
            bool parseError = false;

            while (std::getline(ss, token, ',') && index < 3) {
                try {
                    spawnCoords[index++] = std::stof(token);
                } catch (const std::exception&) {
                    parseError = true;
                    break;
                }
            }

            if (!parseError && index == 3) {
                player.x = spawnCoords[0];
                player.y = spawnCoords[1];
                player.z = spawnCoords[2];
                player.velocityY = 0.0f;
                player.onGround = true;

                lastSafePos = { player.x, player.y, player.z };
                camera.x = player.x;
                camera.y = player.y + 1.6f;
                camera.z = player.z;

                int spawnChunkX = static_cast<int>(std::floor(player.x / CHUNK_SIZE));
                int spawnChunkZ = static_cast<int>(std::floor(player.z / CHUNK_SIZE));

                if (spawnChunkX != lastInterestChunkX || spawnChunkZ != lastInterestChunkZ) {
                    sendInterestMessage(spawnChunkX, spawnChunkZ);
                    lastInterestChunkX = spawnChunkX;
                    lastInterestChunkZ = spawnChunkZ;
                }

                lastPlayerChunkX = spawnChunkX;
                lastPlayerChunkZ = spawnChunkZ;

                std::cout << "[GAME] Applied server spawn at (" << player.x << ", " << player.y << ", " << player.z << ")" << std::endl;
            } else {
                std::cerr << "[GAME] Failed to parse spawn array from hello_ok" << std::endl;
            }
        }
    }
}

inline void Game::handleChunkFull(const std::string& message) {
    // Parse: {"op":"chunk_full","cx":32,"cy":1,"cz":36,"rev":0,"data":"base64..."}
    size_t cxPos = message.find("\"cx\":");
    size_t cyPos = message.find("\"cy\":");
    size_t czPos = message.find("\"cz\":");
    size_t dataPos = message.find("\"data\":\"");

    if (cxPos == std::string::npos || cyPos == std::string::npos ||
        czPos == std::string::npos || dataPos == std::string::npos) {
        std::cerr << "[GAME] Invalid chunk_full message format" << std::endl;
        return;
    }

    // Extract coordinates
    int cx = std::stoi(message.substr(message.find(":", cxPos) + 1));
    int cy = std::stoi(message.substr(message.find(":", cyPos) + 1));
    int cz = std::stoi(message.substr(message.find(":", czPos) + 1));

    // Extract base64 data
    size_t dataStart = dataPos + 8; // Skip past "data":"
    size_t dataEnd = message.find("\"", dataStart);
    if (dataEnd == std::string::npos) {
        std::cerr << "[GAME] Invalid chunk_full data field" << std::endl;
        return;
    }
    std::string base64Data = message.substr(dataStart, dataEnd - dataStart);

    // Decode base64 -> RLE bytes
    std::vector<uint8_t> encoded = Serialization::base64_decode(base64Data);

    // Decode RLE -> block arrays
    const int totalBlocks = CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE;
    std::vector<uint8_t> types(totalBlocks);
    std::vector<uint8_t> solids(totalBlocks);
    Serialization::decodeChunk(encoded, types.data(), solids.data());

    // Convert uint8_t to BlockType and bool for setChunkData
    BlockType* blockTypes = new BlockType[totalBlocks];
    bool* blockSolids = new bool[totalBlocks];
    for (int i = 0; i < totalBlocks; ++i) {
        blockTypes[i] = static_cast<BlockType>(types[i]);
        blockSolids[i] = (solids[i] != 0);
    }

    // Apply to world
    world.setChunkData(cx, cy, cz, blockTypes, blockSolids);

    // Clean up
    delete[] blockTypes;
    delete[] blockSolids;

    // Generate mesh
    meshManager.generateChunkMesh(world, cx, cy, cz);

    // Track chunk loading progress
    chunksLoaded++;
    if (!hasReceivedFirstChunk) {
        hasReceivedFirstChunk = true;
        std::cout << "[GAME] ✓ Received first chunk, starting gameplay..." << std::endl;
    }

    // Update loading status
    if (gameState == GameState::WAITING_FOR_WORLD) {
        loadingStatus = "Loading world... (" + std::to_string(chunksLoaded) + " chunks)";

        // Transition to PLAYING after we have some chunks
        if (chunksLoaded >= 5) {
            // Recalculate safe spawn position now that chunks are loaded
            // This prevents the player from falling through if chunks weren't ready earlier
            Vector3 spawnPos;
            if (findSafeSpawn(SPAWN_X, SPAWN_Z, spawnPos)) {
                player.x = spawnPos.x;
                player.y = spawnPos.y;
                player.z = spawnPos.z;
                std::cout << "[GAME] ✓ Spawn position set to (" << spawnPos.x << ", " << spawnPos.y << ", " << spawnPos.z << ")" << std::endl;
            } else {
                // Fallback to height-based spawn
                int h = world.getHeightAt(static_cast<int>(SPAWN_X), static_cast<int>(SPAWN_Z));
                player.x = std::floor(SPAWN_X) + 0.5f;
                player.y = h + 1.6f;
                player.z = std::floor(SPAWN_Z) + 0.5f;
                std::cout << "[GAME] ⚠ Using fallback spawn at height " << h << std::endl;
            }

            // Update camera and last safe position
            camera.x = player.x;
            camera.y = player.y + 1.8f;
            camera.z = player.z;
            lastSafePos = { player.x, player.y, player.z };

            // Reset velocity to prevent any falling
            player.velocityY = 0.0f;
            player.onGround = true;

            gameState = GameState::PLAYING;
            loadingStatus = "Ready!";
            std::cout << "[GAME] ✓ Enough chunks loaded, entering gameplay" << std::endl;
        }
    }
}

inline void Game::handleChunkUnload(const std::string& message) {
    // Parse: {"op":"chunk_unload","cx":0,"cy":0,"cz":0}
    size_t cxPos = message.find("\"cx\":");
    size_t cyPos = message.find("\"cy\":");
    size_t czPos = message.find("\"cz\":");

    if (cxPos == std::string::npos || cyPos == std::string::npos || czPos == std::string::npos) {
        std::cerr << "[GAME] Invalid chunk_unload message" << std::endl;
        return;
    }

    int cx = std::stoi(message.substr(message.find(":", cxPos) + 1));
    int cy = std::stoi(message.substr(message.find(":", cyPos) + 1));
    int cz = std::stoi(message.substr(message.find(":", czPos) + 1));

    // Free chunk data and mesh
    world.eraseChunk(cx, cy, cz);
    meshManager.removeChunkMesh(cx, cy, cz);

    // Remove revision tracking
    ChunkCoord coord{cx, cy, cz};
    chunkRevisions.erase(coord);
}

inline void Game::handleBlockUpdate(const std::string& message) {
    // Parse: {"op":"block_update","w":[x,y,z],"type":1,"solid":true,"cx":0,"cy":0,"cz":0,"rev":5}
    size_t wPos = message.find("\"w\":");
    size_t typePos = message.find("\"type\":");
    size_t solidPos = message.find("\"solid\":");
    size_t cxPos = message.find("\"cx\":");
    size_t revPos = message.find("\"rev\":");

    if (wPos == std::string::npos || typePos == std::string::npos || solidPos == std::string::npos) {
        std::cerr << "[GAME] Invalid block_update message" << std::endl;
        return;
    }

    // Extract world coordinates
    size_t wArrayStart = message.find("[", wPos);
    size_t wArrayEnd = message.find("]", wArrayStart);
    std::string wArray = message.substr(wArrayStart + 1, wArrayEnd - wArrayStart - 1);

    int wx, wy, wz;
    if (sscanf(wArray.c_str(), "%d,%d,%d", &wx, &wy, &wz) != 3) {
        std::cerr << "[GAME] Invalid coordinates in block_update" << std::endl;
        return;
    }

    // Extract type
    size_t typeValStart = message.find(":", typePos) + 1;
    size_t typeValEnd = message.find_first_of(",}", typeValStart);
    int blockType = std::stoi(message.substr(typeValStart, typeValEnd - typeValStart));

    // Extract solid
    size_t solidValStart = message.find(":", solidPos) + 1;
    size_t solidValEnd = message.find_first_of(",}", solidValStart);
    std::string solidStr = message.substr(solidValStart, solidValEnd - solidValStart);
    bool isSolid = (solidStr.find("true") != std::string::npos);

    // Extract chunk coordinates
    int cx, cy, cz;
    if (cxPos != std::string::npos) {
        size_t cyPos = message.find("\"cy\":");
        size_t czPos = message.find("\"cz\":");
        cx = std::stoi(message.substr(message.find(":", cxPos) + 1));
        cy = std::stoi(message.substr(message.find(":", cyPos) + 1));
        cz = std::stoi(message.substr(message.find(":", czPos) + 1));
    } else {
        cx = wx / CHUNK_SIZE;
        cy = wy / CHUNK_HEIGHT;
        cz = wz / CHUNK_SIZE;
    }

    // Extract revision if present
    int rev = 0;
    if (revPos != std::string::npos) {
        size_t revValStart = message.find(":", revPos) + 1;
        size_t revValEnd = message.find_first_of(",}", revValStart);
        rev = std::stoi(message.substr(revValStart, revValEnd - revValStart));
    }

    // Check revision - only apply if newer or first time
    ChunkCoord coord{cx, cy, cz};
    if (chunkRevisions.count(coord) > 0 && chunkRevisions[coord] >= rev) {
        std::cout << "[GAME] Ignoring stale block_update (rev " << rev << " <= " << chunkRevisions[coord] << ")" << std::endl;
        return;
    }
    chunkRevisions[coord] = rev;

    // Apply update
    Block* block = world.getBlockAt(wx, wy, wz);
    if (!block) {
        std::cerr << "[GAME] Block not in loaded chunk" << std::endl;
        return;
    }

    block->type = static_cast<BlockType>(blockType);
    block->isSolid = isSolid;
    world.markChunkDirty(cx, cy, cz);

    std::cout << "[GAME] Applied block_update: (" << wx << "," << wy << "," << wz
              << ") type=" << blockType << " solid=" << isSolid << " rev=" << rev << std::endl;
}

inline void Game::handlePlayerSnapshot(const std::string& message) {
    // Parse: {"op":"player_snapshot","players":[{"id":"client1","x":10,"y":20,"z":30,"yaw":45,"pitch":10},...]}
    size_t playersPos = message.find("\"players\":");
    if (playersPos == std::string::npos) {
        return;
    }

    // Clear old players that haven't been updated (keep timeout simple for now)
    auto now = std::chrono::steady_clock::now();
    for (auto it = remotePlayers.begin(); it != remotePlayers.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.lastUpdate);
        if (elapsed.count() > 5) {  // Remove players not seen for 5 seconds
            std::cout << "[GAME] Removing stale player: " << it->first << std::endl;
            it = remotePlayers.erase(it);
        } else {
            ++it;
        }
    }

    // Simple parser for player array
    size_t arrayStart = message.find("[", playersPos);
    size_t arrayEnd = message.find("]", arrayStart);
    if (arrayStart == std::string::npos || arrayEnd == std::string::npos) {
        return;
    }

    // Extract each player object
    size_t pos = arrayStart + 1;
    while (pos < arrayEnd) {
        size_t objStart = message.find("{", pos);
        if (objStart == std::string::npos || objStart > arrayEnd) break;

        size_t objEnd = message.find("}", objStart);
        if (objEnd == std::string::npos || objEnd > arrayEnd) break;

        std::string playerObj = message.substr(objStart, objEnd - objStart + 1);

        // Parse player fields
        size_t idPos = playerObj.find("\"id\":\"");
        size_t xPos = playerObj.find("\"x\":");
        size_t yPos = playerObj.find("\"y\":");
        size_t zPos = playerObj.find("\"z\":");
        size_t yawPos = playerObj.find("\"yaw\":");
        size_t pitchPos = playerObj.find("\"pitch\":");

        if (idPos != std::string::npos && xPos != std::string::npos &&
            yPos != std::string::npos && zPos != std::string::npos) {

            // Extract ID
            size_t idStart = idPos + 6;
            size_t idEnd = playerObj.find("\"", idStart);
            std::string playerId = playerObj.substr(idStart, idEnd - idStart);

            // Extract coordinates
            float x = std::stof(playerObj.substr(playerObj.find(":", xPos) + 1));
            float y = std::stof(playerObj.substr(playerObj.find(":", yPos) + 1));
            float z = std::stof(playerObj.substr(playerObj.find(":", zPos) + 1));

            // Extract rotation (with defaults if not present)
            float yaw = 0.0f;
            float pitch = 0.0f;
            if (yawPos != std::string::npos) {
                yaw = std::stof(playerObj.substr(playerObj.find(":", yawPos) + 1));
            }
            if (pitchPos != std::string::npos) {
                pitch = std::stof(playerObj.substr(playerObj.find(":", pitchPos) + 1));
            }

            // Skip ourselves (safety check - server should already exclude us)
            if (!myUsername.empty() && playerId == myUsername) {
                pos = objEnd + 1;
                continue;
            }

            // Update or create remote player
            if (remotePlayers.count(playerId) > 0) {
                remotePlayers[playerId].x = x;
                remotePlayers[playerId].y = y;
                remotePlayers[playerId].z = z;
                remotePlayers[playerId].yaw = yaw;
                remotePlayers[playerId].pitch = pitch;
                remotePlayers[playerId].lastUpdate = now;
            } else {
                std::cout << "[GAME] New player connected: " << playerId << std::endl;
                remotePlayers[playerId] = RemotePlayer(playerId, x, y, z, yaw, pitch);
            }
        }

        pos = objEnd + 1;
    }

    // std::cout << "[GAME] Player snapshot: " << remotePlayers.size() << " remote players" << std::endl;
}

inline void Game::handleChatMessage(const std::string& message) {
    // Parse: {"op":"chat_message","sender":"Player","message":"Hello!"}
    size_t senderPos = message.find("\"sender\":\"");
    size_t messagePos = message.find("\"message\":\"");
    
    if (senderPos == std::string::npos || messagePos == std::string::npos) {
        std::cerr << "[GAME] Invalid chat_message format" << std::endl;
        return;
    }
    
    // Extract sender
    size_t senderStart = senderPos + 10;  // Length of "sender":"
    size_t senderEnd = message.find("\"", senderStart);
    if (senderEnd == std::string::npos) return;
    std::string sender = message.substr(senderStart, senderEnd - senderStart);
    
    // Extract message text
    size_t msgStart = messagePos + 11;  // Length of "message":"
    size_t msgEnd = message.find("\"", msgStart);
    if (msgEnd == std::string::npos) return;
    std::string msgText = message.substr(msgStart, msgEnd - msgStart);
    
    std::cout << "[CHAT] " << sender << ": " << msgText << std::endl;
    chatSystem.addPlayerMessage(sender, msgText);
}

inline void Game::handleSystemMessage(const std::string& message) {
    // Parse: {"op":"system_message","message":"Player joined the server"}
    size_t messagePos = message.find("\"message\":\"");
    
    if (messagePos == std::string::npos) {
        std::cerr << "[GAME] Invalid system_message format" << std::endl;
        return;
    }
    
    // Extract message text
    size_t msgStart = messagePos + 11;  // Length of "message":"
    size_t msgEnd = message.find("\"", msgStart);
    if (msgEnd == std::string::npos) return;
    std::string msgText = message.substr(msgStart, msgEnd - msgStart);
    
    std::cout << "[SYSTEM] " << msgText << std::endl;
    chatSystem.addSystemMessage(msgText);
}

inline void Game::handleAuthError(const std::string& message) {
    // Parse: {"op":"auth_error","reason":"username_taken","message":"Username 'Player1' is already in use"}
    size_t messagePos = message.find("\"message\":\"");
    std::string errorMsg = "Authentication failed";
    
    if (messagePos != std::string::npos) {
        size_t msgStart = messagePos + 11;  // Length of "message":"
        size_t msgEnd = message.find("\"", msgStart);
        if (msgEnd != std::string::npos) {
            errorMsg = message.substr(msgStart, msgEnd - msgStart);
        }
    }
    
    std::cerr << "[GAME] Authentication error: " << errorMsg << std::endl;
    
    // Disconnect from server to allow reconnection
    netClient.disconnect();
    
    // Return to username input screen with error
    gameState = GameState::USERNAME_INPUT;
    usernameError = errorMsg;
    loadingStatus = "Authentication failed";
}

#endif // GAME_NETWORK_HPP