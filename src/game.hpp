// game.hpp
#ifndef GAME_HPP
#define GAME_HPP

#include "../client/net.hpp"
#include "../shared/protocol.hpp"
#include "../shared/serialization.hpp"
#include "text_renderer.hpp"
#include <sstream>

enum class GameState {
    LOADING,           // Initial loading
    CONNECTING,        // Connecting to server (online mode)
    WAITING_FOR_WORLD, // Waiting for world data from server
    PLAYING,           // Normal gameplay
    DISCONNECTED       // Disconnected from server
};

class Game {
public:
    bool pointerLocked = false;
    GameState gameState = GameState::LOADING;
    Shader* shader;
    Shader* outlineShader;
    TextRenderer textRenderer;
    MeshManager meshManager;
    World world;
    Camera camera;
    Player player;
    mat4 projection;
    GLint mvpLoc;
    GLint timeLoc;

    // Fog stuff
    GLint camPosLoc;
    GLint fogStartLoc;
    GLint fogEndLoc;
    GLint fogColorLoc;

    GLint outlineMvpLoc;
    std::chrono::steady_clock::time_point lastFrame;
    bool keys[1024] = { false };
    GLuint textureAtlas;
    GLuint outlineVAO, outlineVBO;
    Vector3i highlightedBlock;
    bool hasHighlightedBlock = false;
    int lastPlayerChunkX = -999;
    int lastPlayerChunkZ = -999;
    Vector3 lastSafePos { SPAWN_X, SPAWN_Y + 1.6f, SPAWN_Z };
    
    // Network client
    NetworkClient netClient;
    bool isOnlineMode = false;
    int lastInterestChunkX = -9999;
    int lastInterestChunkZ = -9999;
    std::unordered_map<ChunkCoord, int> chunkRevisions; // Track revisions for reconciliation
    float lastPoseSendTime = 0.0f;
    const float POSE_SEND_INTERVAL = 0.1f; // 10 Hz
    
    // Loading and connection status
    std::string loadingStatus = "Initializing...";
    bool hasReceivedFirstChunk = false;
    int chunksLoaded = 0;
    bool wasConnected = false; // Track if we were previously connected

    Game() : shader(nullptr), outlineShader(nullptr), player(SPAWN_X, SPAWN_Y, SPAWN_Z) { 
        std::cout << "Game Constructed - Player Spawn: (" << SPAWN_X << ", " << SPAWN_Y << ", " << SPAWN_Z << ")" << std::endl;
    }

    void init() {
        std::cout << "Game initialisation has started..." << std::endl;

        const char* vertexSrc = R"(#version 300 es
            precision mediump float;
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec2 aTexCoord;
            layout(location = 2) in float aAO;
            uniform mat4 uMVP;
            out vec2 TexCoord;
            out float AO;

            // Pass world position for fog computation
            out vec3 WorldPos;
            void main() {
                gl_Position = uMVP * vec4(aPos, 1.0);
                TexCoord = aTexCoord;
                AO = aAO;
                WorldPos = aPos;
            })";

        const char* fragmentSrc = R"(#version 300 es
            precision mediump float;

            in vec2 TexCoord;
            in float AO;
            in vec3 WorldPos;

            uniform sampler2D uTexture;
            uniform float uTime;

            // Fog uniforms
            uniform vec3  uCameraPos;
            uniform float uFogStart;
            uniform float uFogEnd;
            uniform vec3  uFogColor;

            out vec4 FragColor;

            void main() {
                vec2 texCoord = TexCoord;

                // ATLAS size for the frag, need to make this dynamic!
                float tileWidth = 16.0 / 288.0;
                float tileX = floor(texCoord.x / tileWidth);

                // Classify tiles
                bool isLeaves    = (tileX >= 9.0  && tileX < 10.0);
                bool isWater     = (tileX >= 10.0 && tileX < 14.0);
                bool isTallGrass = (tileX >= 15.0 && tileX < 16.0);
                bool isFlower    = (tileX >= 16.0 && tileX < 18.0);
                bool isCutout    = isLeaves || isTallGrass || isFlower;

                // Animate water by shifting within its 4-frame strip
                if (isWater) {
                    float frame = mod(floor(uTime * 2.0), 4.0);
                    float offsetX = frame * tileWidth;
                    texCoord.x = mod(texCoord.x, tileWidth) + 10.0 * tileWidth + offsetX;
                }

                vec4 texColor = texture(uTexture, texCoord);

                // Alpha-test cutout for foliage (leaves, tall grass etc.)
                if (isCutout) {
                    if (texColor.a < 0.5) discard;
                    texColor.a = 1.0;
                }

                // 70% opacity for water
                if (isWater) {
                    texColor.a *= 0.7;
                }

                // Apply ambient occlusion
                texColor.rgb *= (1.0 - AO);

                // Fog
                float dist = distance(WorldPos, uCameraPos);
                float fogFactor = smoothstep(uFogEnd, uFogStart, dist);
                texColor.rgb = mix(uFogColor, texColor.rgb, fogFactor);

                FragColor = texColor;
            })";


        // Compile and link shaders
        shader = new Shader(vertexSrc, fragmentSrc);
        shader->use();
        mvpLoc = shader->getUniform("uMVP");
        timeLoc = shader->getUniform("uTime");

        // Fog uniform locations
        camPosLoc   = shader->getUniform("uCameraPos");
        fogStartLoc = shader->getUniform("uFogStart");
        fogEndLoc   = shader->getUniform("uFogEnd");
        fogColorLoc = shader->getUniform("uFogColor");

        // Load Texture Atlas
        glGenTextures(1, &textureAtlas);
        glBindTexture(GL_TEXTURE_2D, textureAtlas);

        // Texture Parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        int width, height, nrChannels;
        unsigned char* data = stbi_load("/assets/texture_atlas.png", &width, &height, &nrChannels, 4);
        if (!data) {
            std::cerr << "Failed to load texture atlas: assets/texture_atlas.png" << std::endl;
            exit(1);
        }
        else {
            std::cout << "Loaded texture atlas: " << width << "x" << height << std::endl;

            // Transfer image data to GPU
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(data);
        }

        // Bind texture to texture unit 0 and set sampler uniform
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureAtlas);
        GLint textureLoc = shader->getUniform("uTexture");
        glUniform1i(textureLoc, 0);

        // Create outline shader for block highlighting
        const char* outlineVertexSrc = R"(#version 300 es
            precision mediump float;
            layout(location = 0) in vec3 aPos;
            uniform mat4 uMVP;
            void main() {
                gl_Position = uMVP * vec4(aPos, 1.0);
            })";

        const char* outlineFragmentSrc = R"(#version 300 es
            precision mediump float;
            out vec4 FragColor;
            void main() {
                FragColor = vec4(0.0, 0.0, 0.0, 1.0); // Black outline
            })";

        outlineShader = new Shader(outlineVertexSrc, outlineFragmentSrc);
        outlineMvpLoc = outlineShader->getUniform("uMVP");

        // Setup outline VAO and VBO
        glGenVertexArrays(1, &outlineVAO);
        glGenBuffers(1, &outlineVBO);

        // Initialise and generate the world
        loadingStatus = "Initializing world...";
        world.initialise();

        // Check for network mode
        const char* wsUrl = std::getenv("GAME_WS_URL");
        if (wsUrl && wsUrl[0] != '\0') {
            std::cout << "[GAME] Starting in ONLINE mode" << std::endl;
            isOnlineMode = true;
            gameState = GameState::CONNECTING;
            loadingStatus = "Connecting to server...";
            
            // Setup message handler
            netClient.setOnMessage([this](const std::string& msg) {
                this->handleServerMessage(msg);
            });
            
            // Setup connection handler - send hello and set_interest when connected
            netClient.setOnConnect([this]() {
                std::cout << "[GAME] Connection established, sending hello" << std::endl;
                loadingStatus = "Connected! Requesting world data...";
                gameState = GameState::WAITING_FOR_WORLD;
                wasConnected = true;
                
                int currentChunkX = static_cast<int>(std::floor(player.x / CHUNK_SIZE));
                int currentChunkZ = static_cast<int>(std::floor(player.z / CHUNK_SIZE));
                
                sendHelloMessage();
                sendInterestMessage(currentChunkX, currentChunkZ);
                
                // Update tracking to prevent resending
                lastPlayerChunkX = currentChunkX;
                lastPlayerChunkZ = currentChunkZ;
                lastInterestChunkX = currentChunkX;
                lastInterestChunkZ = currentChunkZ;
            });
            
            // Setup disconnection handler
            netClient.setOnDisconnect([this]() {
                std::cout << "[GAME] Disconnected from server" << std::endl;
                gameState = GameState::DISCONNECTED;
                loadingStatus = "Disconnected from server";
            });
            
            // Connect to server
            if (netClient.connect(wsUrl)) {
                std::cout << "[GAME] Waiting for connection..." << std::endl;
            } else {
                std::cerr << "[GAME] Failed to connect, falling back to offline mode" << std::endl;
                isOnlineMode = false;
                gameState = GameState::LOADING;
            }
        } else {
            std::cout << "[GAME] Starting in OFFLINE mode (no GAME_WS_URL set)" << std::endl;
            isOnlineMode = false;
        }

        // Only load chunks locally if in offline mode
        if (!isOnlineMode) {
            // Load initial chunks around spawn
            loadingStatus = "Generating terrain...";
            std::cout << "Loading initial chunks around spawn..." << std::endl;
            world.loadChunksAroundPosition(SPAWN_X, SPAWN_Z);
            
            // Generate initial chunk meshes
            loadingStatus = "Building meshes...";
            std::cout << "Generating initial chunk meshes..." << std::endl;
            for (const auto& coord : world.getLoadedChunks()) {
                meshManager.generateChunkMesh(world, coord.x, coord.y, coord.z);
                chunksLoaded++;
            }
            std::cout << "Loaded " << world.getLoadedChunks().size() << " chunks" << std::endl;
            gameState = GameState::PLAYING;
            loadingStatus = "Ready!";
        } else {
            std::cout << "[GAME] Skipping local chunk generation (online mode)" << std::endl;
        }

        // Make sure that the player spawns in a good spot
        Vector3 spawnPos;
        if (findSafeSpawn(SPAWN_X, SPAWN_Z, spawnPos)) {
            player.x = spawnPos.x;
            player.y = spawnPos.y;
            player.z = spawnPos.z;
        } else {
            int h = world.getHeightAt(static_cast<int>(SPAWN_X), static_cast<int>(SPAWN_Z));
            player.x = std::floor(SPAWN_X) + 0.5f;
            player.y = h + 1.6f;
            player.z = std::floor(SPAWN_Z) + 0.5f;
        }

        lastSafePos = { player.x, player.y, player.z };

        camera.x = player.x;
        camera.y = player.y + 1.6f;
        camera.z = player.z;
        
        // Don't initialize lastPlayerChunkX/Z here - keep them at -999/-999
        // so that hello message gets sent on first updateChunks() call

        int canvasWidth, canvasHeight;
        emscripten_get_canvas_element_size("canvas", &canvasWidth, &canvasHeight);

        projection = perspective(CAM_FOV * M_PI / 180.0f, static_cast<float>(canvasWidth) / static_cast<float>(canvasHeight), 0.1f, 1000.0f);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Initialize text renderer
        if (!textRenderer.init(canvasWidth, canvasHeight)) {
            std::cerr << "Failed to initialize text renderer" << std::endl;
        }

        lastFrame = std::chrono::steady_clock::now();
    }

    void mainLoop() {
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
            if (isOnlineMode && netClient.isConnected()) {
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

    void handleKey(int keyCode, bool pressed) {
        keys[keyCode] = pressed;

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

    void handleMouseMove(float movementX, float movementY) {
        if (!pointerLocked) return;
        camera.yaw   += movementX * SENSITIVITY;
        camera.pitch = std::clamp(camera.pitch - movementY * SENSITIVITY, -89.0f, 89.0f);
    }

    void handleMouseClick(int button) {
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

    void updateChunks() {
        int currentChunkX = static_cast<int>(std::floor(player.x / CHUNK_SIZE));
        int currentChunkZ = static_cast<int>(std::floor(player.z / CHUNK_SIZE));

        if (currentChunkX != lastPlayerChunkX || currentChunkZ != lastPlayerChunkZ) {
            // Send interest update if online and chunk changed
            if (isOnlineMode && netClient.isConnected()) {
                if (currentChunkX != lastInterestChunkX || currentChunkZ != lastInterestChunkZ) {
                    sendInterestMessage(currentChunkX, currentChunkZ);
                    lastInterestChunkX = currentChunkX;
                    lastInterestChunkZ = currentChunkZ;
                }
            }
            
            // Only do local generation if offline
            if (!isOnlineMode) {
                world.loadChunksAroundPosition(player.x, player.z);
                world.unloadDistantChunks(player.x, player.z);

                for (const auto& coord : world.getLoadedChunks()) {
                    if (meshManager.chunkMeshes.find(coord) == meshManager.chunkMeshes.end()) {
                        meshManager.generateChunkMesh(world, coord.x, coord.y, coord.z);
                    }
                }
            }

            lastPlayerChunkX = currentChunkX;
            lastPlayerChunkZ = currentChunkZ;
        }
    }

    void updateHighlightedBlock() {
        float maxDistance = 4.5f;
        RaycastHit hit = raycast(maxDistance);
        if (hit.hit) {
            hasHighlightedBlock = true;
            highlightedBlock = hit.blockPosition;
        } else {
            hasHighlightedBlock = false;
        }
    }

private:
    float bobbingTime = 0.0f;
    float bobbingOffset = 0.0f;
    float bobbingHorizontalOffset = 0.0f;
    bool isMoving = false;
    float gameTime = 0.0f;
    float deltaTime = 0.0f;
    bool isSprinting = false;
    bool lastWKeyState = false;
    float lastWKeyPressTime = 0.0f;
    float currentTime = 0.0f;
    bool isFlying = false;
    bool lastSpaceKeyState = false;
    float lastSpaceKeyPressTime = 0.0f;

    float calculateDeltaTime() {
        auto now = std::chrono::steady_clock::now();
        float delta = std::chrono::duration<float>(now - lastFrame).count();
        lastFrame = now;
        return delta;
    }

    // Solid for physics (does NOT include tall grass)
    bool isBlockSolid(int x, int y, int z) const {
        const Block* b = world.getBlockAt(x, y, z);
        if (!b) return true;
        if (b->type == BLOCK_WATER) return false;
        return b->isSolid;
    }

    // Selectable for raycast (includes tall grass even though non-solid)
    bool isBlockSelectable(int x, int y, int z) const {
        const Block* b = world.getBlockAt(x, y, z);
        if (!b) return false;
        if (b->isSolid) return true;
        return (b->type == BLOCK_TALL_GRASS || b->type == BLOCK_ORANGE_FLOWER || b->type == BLOCK_BLUE_FLOWER);
    }

    bool isWaterBlock(int x, int y, int z) const {
        const Block* b = world.getBlockAt(x, y, z);
        return b && (b->type == BLOCK_WATER);
    }

    // Check if the player is in water (any part of their bounding box)
    bool isPlayerInWater() const {
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

    bool findSafeSpawn(float startX, float startZ, Vector3& outPos) {
        int sx = static_cast<int>(std::floor(startX));
        int sz = static_cast<int>(std::floor(startZ));
        const int maxRadiusBlocks = (CHUNK_LOAD_DISTANCE - 1) * CHUNK_SIZE - 2;
        const int maxRadius = std::max(8, maxRadiusBlocks);

        auto tryColumn = [&](int cx, int cz, Vector3& pos) -> bool {
            // Only load chunks if in offline mode
            if (!isOnlineMode) {
                world.loadChunksAroundPosition(cx + 0.5f, cz + 0.5f);
            }

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

    void checkGround() {
        float epsilon = 0.001f;
        player.onGround = isColliding(player.x, player.y - epsilon, player.z);
    }

    bool isColliding(float x, float y, float z) const {
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

    void applyPhysics(float dt) {
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
            
            // Only generate chunks locally if in offline mode
            if (!isOnlineMode) {
                world.loadChunksAroundPosition(player.x, player.z);
                world.unloadDistantChunks(player.x, player.z);
                for (const auto& coord : world.getLoadedChunks()) {
                    if (meshManager.chunkMeshes.find(coord) == meshManager.chunkMeshes.end()) {
                        meshManager.generateChunkMesh(world, coord.x, coord.y, coord.z);
                    } else {
                        meshManager.generateChunkMesh(world, coord.x, coord.y, coord.z);
                    }
                }
            }
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

    void processInput(float dt) {
        currentTime += dt;

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

        // Only do local chunk generation if in offline mode
        if (!isOnlineMode && (deltaX != 0.0f || deltaZ != 0.0f)) {
            world.loadChunksAroundPosition(player.x + deltaX, player.z + deltaZ);
            world.unloadDistantChunks(player.x + deltaX, player.z + deltaZ);
            for (const auto& coord : world.getLoadedChunks()) {
                if (meshManager.chunkMeshes.find(coord) == meshManager.chunkMeshes.end()) {
                    meshManager.generateChunkMesh(world, coord.x, coord.y, coord.z);
                }
            }
        }

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

    void removeBlock(int x, int y, int z) {
        Block* block = world.getBlockAt(x, y, z);
        if (!block) return;
        if (block->type == BLOCK_BEDROCK) return;

        // Send edit message if online
        if (isOnlineMode && netClient.isConnected()) {
            std::ostringstream msg;
            msg << "{\"op\":\"edit\",\"kind\":\"remove\",\"w\":[" << x << "," << y << "," << z << "]}";
            netClient.send(msg.str());
            std::cout << "[GAME] Sent remove edit: " << x << "," << y << "," << z << std::endl;
        }

        // Apply optimistically
        auto isPlant = [](BlockType t) {
            return t == BLOCK_TALL_GRASS || t == BLOCK_ORANGE_FLOWER || t == BLOCK_BLUE_FLOWER;
        };

        // If you directly broke a plant, just clear it
        if (isPlant(block->type)) {
            block->isSolid = false;
            block->type = BLOCK_DIRT;
            int cx = x / CHUNK_SIZE, cy = y / CHUNK_HEIGHT, cz = z / CHUNK_SIZE;
            world.markChunkDirty(cx, cy, cz);
            return;
        }

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

    void placeBlock(int x, int y, int z) {
        Block* block = world.getBlockAt(x, y, z);
        if (!block) return;
        if (block->isSolid) return;

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

        // Send edit message if online
        if (isOnlineMode && netClient.isConnected()) {
            std::ostringstream msg;
            msg << "{\"op\":\"edit\",\"kind\":\"place\",\"w\":[" << x << "," << y << "," << z << "],\"type\":\"PLANKS\"}";
            netClient.send(msg.str());
            std::cout << "[GAME] Sent place edit: " << x << "," << y << "," << z << std::endl;
        }

        // Apply optimistically
        block->isSolid = true;
        block->type = BLOCK_PLANKS;

        int cx = x / CHUNK_SIZE;
        int cy = y / CHUNK_HEIGHT;
        int cz = z / CHUNK_SIZE;
        std::cout << "[GAME] placeBlock optimistic update at (" << x << "," << y << "," << z << ") chunk (" << cx << "," << cy << "," << cz << ")" << std::endl;
        world.markChunkDirty(cx, cy, cz);
    }

    struct RaycastHit {
        bool hit;
        Vector3i blockPosition;
        Vector3i adjacentPosition;
    };

    RaycastHit raycast(float maxDistance) {
        RaycastHit result;
        result.hit = false;
        Vector3 origin = { camera.x, camera.y, camera.z };
        Vector3 direction = camera.getFrontVector();

        float len = std::sqrt(direction.x*direction.x + direction.y*direction.y + direction.z*direction.z);
        if (len != 0) {
            direction.x /= len;
            direction.y /= len;
            direction.z /= len;
        }

        int bx = static_cast<int>(std::floor(origin.x));
        int by = static_cast<int>(std::floor(origin.y));
        int bz = static_cast<int>(std::floor(origin.z));
        int prevX = bx, prevY = by, prevZ = bz;

        int stepX = (direction.x >= 0 ? 1 : -1);
        int stepY = (direction.y >= 0 ? 1 : -1);
        int stepZ = (direction.z >= 0 ? 1 : -1);

        float tMaxX = intbound(origin.x, direction.x);
        float tMaxY = intbound(origin.y, direction.y);
        float tMaxZ = intbound(origin.z, direction.z);
        float tDeltaX = (direction.x != 0 ? stepX / direction.x : INFINITY);
        float tDeltaY = (direction.y != 0 ? stepY / direction.y : INFINITY);
        float tDeltaZ = (direction.z != 0 ? stepZ / direction.z : INFINITY);
        float traveled = 0.0f;

        while (traveled <= maxDistance) {
            // Selectable includes solids and tall grass (non-solid)
            if (isBlockSelectable(bx, by, bz)) {
                result.hit = true;
                result.blockPosition = { bx, by, bz };
                result.adjacentPosition = { prevX, prevY, prevZ };
                return result;
            }

            prevX = bx; prevY = by; prevZ = bz;
            if (tMaxX < tMaxY) {
                if (tMaxX < tMaxZ) {
                    bx += stepX;
                    traveled = tMaxX;
                    tMaxX += tDeltaX;
                } else {
                    bz += stepZ;
                    traveled = tMaxZ;
                    tMaxZ += tDeltaZ;
                }
            } else {
                if (tMaxY < tMaxZ) {
                    by += stepY;
                    traveled = tMaxY;
                    tMaxY += tDeltaY;
                } else {
                    bz += stepZ;
                    traveled = tMaxZ;
                    tMaxZ += tDeltaZ;
                }
            }
        }

        return result;
    }

    float intbound(float s, float ds) {
        if (ds == 0.0f) {
            return INFINITY;
        } else {
            float sFloor = std::floor(s);
            if (ds > 0) {
                return (sFloor + 1.0f - s) / ds;
            } else {
                return (s - sFloor) / -ds;
            }
        }
    }

    void renderBlockOutline(int x, int y, int z, const mat4& mvp) {
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

    void render() {
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

        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader->use();
        mat4 view = camera.getViewMatrix();
        mat4 mvp = multiply(projection, view);
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.data);
        glUniform1f(timeLoc, gameTime);

        float renderRadius = float(RENDER_DISTANCE * CHUNK_SIZE);
        float fogStart = renderRadius * 0.65f;
        float fogEnd   = renderRadius * 0.95f;
        glUniform3f(camPosLoc, camera.x, camera.y, camera.z);
        glUniform1f(fogStartLoc, fogStart);
        glUniform1f(fogEndLoc, fogEnd);
        glUniform3f(fogColorLoc, 0.53f, 0.81f, 0.92f);

        // FIRST PASS - Draw opaque & alpha-tested (solids + tall grass)
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);  // disable culling so billboard plants are double-sided
        meshManager.drawVisibleChunksSolid(player.x, player.z);
        
        // SECOND PASS - Draw transparent (water)
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
        glDepthMask(GL_FALSE);
        meshManager.drawVisibleChunksWater(player.x, player.z);
        glDepthMask(GL_TRUE);
        glDisable(GL_POLYGON_OFFSET_FILL);
        
        if (hasHighlightedBlock) renderBlockOutline(highlightedBlock.x, highlightedBlock.y, highlightedBlock.z, mvp);
        
        // Render UI overlay
        renderUI();
    }
    
    void renderUI() {
        // Disable depth testing for 2D UI
        glDisable(GL_DEPTH_TEST);
        
        // Update text renderer projection if window size changed
        int width, height;
        emscripten_get_canvas_element_size("canvas", &width, &height);
        if (width != textRenderer.getScreenWidth() || height != textRenderer.getScreenHeight()) {
            textRenderer.updateProjection(width, height);
        }
        
        // Handle different game states
        if (gameState == GameState::LOADING || gameState == GameState::CONNECTING || gameState == GameState::WAITING_FOR_WORLD) {
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
            
            // Render connection status in top-left corner (only in online mode)
            if (isOnlineMode && gameState == GameState::PLAYING) {
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
        }
        
        // Restore OpenGL state for 3D rendering
        glEnable(GL_DEPTH_TEST);
        
        // Restore the main shader and block texture atlas
        shader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureAtlas);
    }

    // Network methods
    void sendHelloMessage() {
        std::ostringstream json;
        json << "{\"op\":\"" << ClientOp::HELLO << "\",\"proto\":" << PROTOCOL_VERSION << "}";
        netClient.send(json.str());
    }
    
    void sendInterestMessage(int centerX, int centerZ) {
        std::ostringstream json;
        json << "{\"op\":\"" << ClientOp::SET_INTEREST 
             << "\",\"center\":[" << centerX << "," << centerZ 
             << "],\"radius\":" << RENDER_DISTANCE << "}";
        netClient.send(json.str());
    }
    
    void sendPoseUpdate() {
        std::ostringstream json;
        json << "{\"op\":\"" << ClientOp::POSE 
             << "\",\"x\":" << player.x 
             << ",\"y\":" << player.y
             << ",\"z\":" << player.z << "}";
        netClient.send(json.str());
    }
    
    void handleServerMessage(const std::string& message) {
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
        std::cout << "[GAME] Handling message op: " << op << std::endl;
        
        if (op == ServerOp::HELLO_OK) {
            handleHelloOk(message);
        } else if (op == ServerOp::CHUNK_FULL) {
            handleChunkFull(message);
        } else if (op == ServerOp::CHUNK_UNLOAD) {
            handleChunkUnload(message);
        } else if (op == ServerOp::PLAYER_SNAPSHOT) {
            handlePlayerSnapshot(message);
        } else if (op == ServerOp::BLOCK_UPDATE) {
            handleBlockUpdate(message);
        } else {
            std::cerr << "[GAME] Unknown op: " << op << std::endl;
        }
    }
    
    void handleHelloOk(const std::string& message) {
        std::cout << "[GAME] ✓ Server accepted hello" << std::endl;
    }
    
    void handleChunkFull(const std::string& message) {
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
                gameState = GameState::PLAYING;
                loadingStatus = "Ready!";
                std::cout << "[GAME] ✓ Enough chunks loaded, entering gameplay" << std::endl;
            }
        }
        
        std::cout << "[GAME] ✓ Loaded chunk (" << cx << "," << cy << "," << cz << ") from server" << std::endl;
    }
    
    void handleChunkUnload(const std::string& message) {
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
        
        std::cout << "[GAME] Unloading chunk (" << cx << "," << cy << "," << cz << ")" << std::endl;
        
        // Free chunk data and mesh
        world.eraseChunk(cx, cy, cz);
        meshManager.removeChunkMesh(cx, cy, cz);
        
        // Remove revision tracking
        ChunkCoord coord{cx, cy, cz};
        chunkRevisions.erase(coord);
    }
    
    void handleBlockUpdate(const std::string& message) {
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
    
    void handlePlayerSnapshot(const std::string& message) {
        std::cout << "[GAME] → Received player_snapshot (stub: future multiplayer)" << std::endl;
        // TODO: Parse other player positions for future multiplayer
        // Example: {"op":"player_snapshot","players":[{"id":"p1","x":10,"y":20,"z":30},...]}
    }

    mat4 perspective(float fov, float aspect, float near, float far) const {
        mat4 proj;
        float tanHalfFovy = tanf(fov / 2.0f);
        proj.data[0] = 1.0f / (aspect * tanHalfFovy);
        proj.data[5] = 1.0f / tanHalfFovy;
        proj.data[10] = -(far + near) / (far - near);
        proj.data[11] = -1.0f;
        proj.data[14] = -(2.0f * far * near) / (far - near);
        return proj;
    }

    mat4 multiply(const mat4& a, const mat4& b) const {
        mat4 result;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                result.data[col * 4 + row] = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    result.data[col * 4 + row] += a.data[k * 4 + row] * b.data[col * 4 + k];
                }
            }
        }
        return result;
    }
};

#endif // GAME_HPP
