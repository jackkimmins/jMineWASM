// game.hpp
#ifndef GAME_HPP
#define GAME_HPP

class Game {
public:
    bool pointerLocked = false;
    Shader* shader;
    Shader* outlineShader;
    MeshManager meshManager;
    World world;
    Camera camera;
    Player player;
    mat4 projection;
    GLint mvpLoc;
    GLint timeLoc;

    // Fog stuff:
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
                float tileWidth = 16.0 / 240.0;
                float tileX = floor(texCoord.x / tileWidth);

                // Classify tiles
                bool isLeaves = (tileX >= 9.0 && tileX < 10.0);
                bool isWater  = (tileX >= 10.0 && tileX < 14.0);

                // Animate water by shifting within its 4-frame strip
                if (isWater) {
                    float frame = mod(floor(uTime * 2.0), 4.0);
                    float offsetX = frame * tileWidth;
                    texCoord.x = mod(texCoord.x, tileWidth) + 10.0 * tileWidth + offsetX;
                }

                vec4 texColor = texture(uTexture, texCoord);

                if (isLeaves) {
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
        world.initialise();

        // Load initial chunks around spawn
        std::cout << "Loading initial chunks around spawn..." << std::endl;
        world.loadChunksAroundPosition(SPAWN_X, SPAWN_Z);
        
        // Generate initial chunk meshes
        std::cout << "Generating initial chunk meshes..." << std::endl;
        for (const auto& coord : world.getLoadedChunks()) {
            meshManager.generateChunkMesh(world, coord.x, coord.y, coord.z);
        }
        std::cout << "Loaded " << world.getLoadedChunks().size() << " chunks" << std::endl;

        // Make sure that the player spawns in a good spot
        Vector3 spawnPos;
        if (findSafeSpawn(SPAWN_X, SPAWN_Z, spawnPos)) {
            player.x = spawnPos.x;
            player.y = spawnPos.y;
            player.z = spawnPos.z;
        } else {
            // Fallback
            int h = world.getHeightAt(static_cast<int>(SPAWN_X), static_cast<int>(SPAWN_Z));
            player.x = std::floor(SPAWN_X) + 0.5f;
            player.y = h + 1.6f;
            player.z = std::floor(SPAWN_Z) + 0.5f;
        }

        // Initialise last safe position at spawn
        lastSafePos = { player.x, player.y, player.z };

        // Update camera position to match spawn
        camera.x = player.x;
        camera.y = player.y + 1.6f;
        camera.z = player.z;
        
        // Track player chunk position
        lastPlayerChunkX = static_cast<int>(std::floor(player.x / CHUNK_SIZE));
        lastPlayerChunkZ = static_cast<int>(std::floor(player.z / CHUNK_SIZE));

        // Get initial canvas size
        int canvasWidth, canvasHeight;
        emscripten_get_canvas_element_size("canvas", &canvasWidth, &canvasHeight);

        // Projection matrix with dyn aspect ratio
        projection = perspective(CAM_FOV * M_PI / 180.0f, static_cast<float>(canvasWidth) / static_cast<float>(canvasHeight), 0.1f, 1000.0f);

        // Enable depth testing and face culling
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        
        // Enable blending for transparency
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        lastFrame = std::chrono::steady_clock::now();
    }

    void mainLoop() {
        // Calculate time since last frame
        deltaTime = calculateDeltaTime();
        gameTime += deltaTime;

        // Handle player movement input (updates player.x and player.z)
        processInput(deltaTime);

        updateChunks();
        applyPhysics(deltaTime);

        if (isMoving) bobbingTime += deltaTime;

        // Highlight outline
        updateHighlightedBlock();

        meshManager.updateDirtyChunks(world);

        render();
    }

    void handleKey(int keyCode, bool pressed) {
        keys[keyCode] = pressed;

        // Fly mode
        if (keyCode == 32) { // Space
            if (pressed && !lastSpaceKeyState) {
                float timeSinceLastPress = currentTime - lastSpaceKeyPressTime;
                if (timeSinceLastPress < DOUBLE_TAP_TIME) {
                    isFlying = !isFlying;
                    if (isFlying) {
                        player.velocityY = 0.0f;
                        player.onGround = false;
                        isSprinting = false;
                    }
                }
                lastSpaceKeyPressTime = currentTime;
            }

            if (pressed && player.onGround && !isFlying) {
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

        // Exiting sprint if moving backward
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
            // Left-click: remove block
            removeBlock(hit.blockPosition.x, hit.blockPosition.y, hit.blockPosition.z);
        } else if (button == 2) {
            // Right-click: place block
            placeBlock(hit.adjacentPosition.x, hit.adjacentPosition.y, hit.adjacentPosition.z);
        }
    }

    void updateChunks() {
        // Check if player entered a new chunk
        int currentChunkX = static_cast<int>(std::floor(player.x / CHUNK_SIZE));
        int currentChunkZ = static_cast<int>(std::floor(player.z / CHUNK_SIZE));

        if (currentChunkX != lastPlayerChunkX || currentChunkZ != lastPlayerChunkZ) {
            // Load chunks around the new position
            world.loadChunksAroundPosition(player.x, player.z);

            // Unload chunks far away to free memory
            world.unloadDistantChunks(player.x, player.z);

            // Generate meshes for any newly loaded chunks
            for (const auto& coord : world.getLoadedChunks()) {
                if (meshManager.chunkMeshes.find(coord) == meshManager.chunkMeshes.end()) {
                    meshManager.generateChunkMesh(world, coord.x, coord.y, coord.z);
                }
            }

            // Update the last known chunk coordinates
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

    bool isBlockSolid(int x, int y, int z) const {
        const Block* b = world.getBlockAt(x, y, z);
        return b ? b->isSolid : true;
    }

    bool isWaterBlock(int x, int y, int z) const {
        const Block* b = world.getBlockAt(x, y, z);
        return b && (b->type == BLOCK_WATER);
    }

    bool findSafeSpawn(float startX, float startZ, Vector3& outPos) {
        // Convert to integer column
        int sx = static_cast<int>(std::floor(startX));
        int sz = static_cast<int>(std::floor(startZ));
        const int maxRadiusBlocks = (CHUNK_LOAD_DISTANCE - 1) * CHUNK_SIZE - 2;
        const int maxRadius = std::max(8, maxRadiusBlocks);

        auto tryColumn = [&](int cx, int cz, Vector3& pos) -> bool {
            // Ensure chunks around this candidate are available for queries
            world.loadChunksAroundPosition(cx + 0.5f, cz + 0.5f);

            int h = world.getHeightAt(cx, cz);
            // Reject ocean/lake positions
            if (h < WATER_LEVEL) return false;

            // Ground must be solid and not water
            if (!isBlockSolid(cx, h, cz)) return false;
            if (isWaterBlock(cx, h + 1, cz)) return false;

            // Candidate feet position (center of block for safety)
            float px = cx + 0.5f;
            float pz = cz + 0.5f;
            float py = static_cast<float>(h) + 1.6f;

            // Ensure the player is not intersecting anything
            if (isColliding(px, py, pz)) return false;

            pos = { px, py, pz };
            return true;
        };

        // Check the center first
        if (tryColumn(sx, sz, outPos)) return true;

        // Spiral search outward
        for (int r = 1; r <= maxRadius; ++r) {
            // top and bottom rows of the square ring
            for (int x = sx - r; x <= sx + r; ++x) {
                Vector3 pos;
                if (tryColumn(x, sz - r, pos)) { outPos = pos; return true; }
                if (tryColumn(x, sz + r, pos)) { outPos = pos; return true; }
            }
            // left and right columns of the square ring
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
        // Axis-Aligned Bounding Box (AABB) collision check for player
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
                // Collisions while flying
                if (vert > 0.0f) {
                    // Moving up into a ceiling
                    player.y = std::floor(newY + PLAYER_HEIGHT) - PLAYER_HEIGHT;
                } else if (vert < 0.0f) {
                    // landing stops flying
                    player.y = std::floor(newY) + 1.0f;
                    player.onGround = true;
                    isFlying = false;
                    player.velocityY = 0.0f;
                }
            }

            // Check for landing if user wasn't holding down
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

        // Normal physics with gravity
        player.velocityY += GRAVITY * dt;
        float newY = player.y + player.velocityY * dt;

        // If player falls below world, reset position
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

            // Reload chunks around the spawn to ensure safe landing area
            lastPlayerChunkX = static_cast<int>(std::floor(player.x / CHUNK_SIZE));
            lastPlayerChunkZ = static_cast<int>(std::floor(player.z / CHUNK_SIZE));
            world.loadChunksAroundPosition(player.x, player.z);
            world.unloadDistantChunks(player.x, player.z);
            for (const auto& coord : world.getLoadedChunks()) {
                if (meshManager.chunkMeshes.find(coord) == meshManager.chunkMeshes.end()) {
                    meshManager.generateChunkMesh(world, coord.x, coord.y, coord.z);
                } else {
                    meshManager.generateChunkMesh(world, coord.x, coord.y, coord.z);
                }
            }
            // Reset last safe position too
            lastSafePos = { player.x, player.y, player.z };
            return;
        }

        if (player.velocityY > 0) {
            // Moving upward
            if (!isColliding(player.x, newY, player.z)) {
                player.y = newY;
            } else {
                // Snap so the player's head rests just below the ceiling
                player.y = std::floor(newY + PLAYER_HEIGHT) - PLAYER_HEIGHT;
                player.velocityY = 0.0f;
            }
        } else {
            // Moving downward or standing
            if (!isColliding(player.x, newY, player.z)) {
                player.y = newY;
                player.onGround = false;
            } else {
                // Landed on a block below
                player.y = std::floor(newY) + 1.0f;  // snap to top of the block
                player.velocityY = 0.0f;
                player.onGround = true;
            }
        }

        // Continuously check if player is standing on ground after physics update
        checkGround();

        // Fail-safe: if we are intersecting (e.g., chunk popped in), revert to last known good position
        if (isColliding(player.x, player.y, player.z)) {
            player.x = lastSafePos.x;
            player.y = lastSafePos.y;
            player.z = lastSafePos.z;
            player.velocityY = 0.0f;
            player.onGround = true;
        } else {
            // Update last safe position when clean
            lastSafePos = { player.x, player.y, player.z };
        }
    }

    void processInput(float dt) {
        // Track time for double-tap logic
        currentTime += dt;

        // Determine movement speed (consider sprint or fly)
        float baseSpeed = isSprinting ? SPRINT_SPEED : PLAYER_SPEED;
        if (isFlying) {
            baseSpeed = FLY_SPEED;
            isSprinting = false; // sprint irrelevant while flying
        }
        float distance = baseSpeed * dt;

        // Calculate forward and right movement vectors based on camera yaw
        float radYaw = camera.yaw * M_PI / 180.0f;
        float frontX = cosf(radYaw);
        float frontZ = sinf(radYaw);
        float rightX = -sinf(radYaw);
        float rightZ = cosf(radYaw);

        // Movement deltas (horizontal only; vertical handled in physics, esp. fly)
        float deltaX = 0.0f;
        float deltaZ = 0.0f;

        if (keys[87]) { // W - forward
            deltaX += frontX * distance;
            deltaZ += frontZ * distance;
        }
        if (keys[83]) { // S - backward
            deltaX -= frontX * distance;
            deltaZ -= frontZ * distance;
            // Exiting sprint if moving backward
            isSprinting = false;
        }
        if (keys[65]) { // A - strafe left
            deltaX -= rightX * distance;
            deltaZ -= rightZ * distance;
        }
        if (keys[68]) { // D - strafe right
            deltaX += rightX * distance;
            deltaZ += rightZ * distance;
        }

        // Determine if the player is trying to move (for bobbing effect)
        isMoving = (deltaX != 0.0f || deltaZ != 0.0f) && !isFlying; // no bobbing in fly

        // *** Preload chunks for the intended destination to avoid stepping into unknown space ***
        if (deltaX != 0.0f || deltaZ != 0.0f) {
            world.loadChunksAroundPosition(player.x + deltaX, player.z + deltaZ);
            world.unloadDistantChunks(player.x + deltaX, player.z + deltaZ);
            for (const auto& coord : world.getLoadedChunks()) {
                if (meshManager.chunkMeshes.find(coord) == meshManager.chunkMeshes.end()) {
                    meshManager.generateChunkMesh(world, coord.x, coord.y, coord.z);
                }
            }
        }

        // Attempt horizontal movement with collision checks
        if (!isColliding(player.x + deltaX, player.y, player.z)) {
            player.x += deltaX;
        }
        if (!isColliding(player.x, player.y, player.z + deltaZ)) {
            player.z += deltaZ;
        }

        // If the player stopped pressing forward, stop sprinting
        if (!keys[87] && isSprinting) {
            isSprinting = false;
        }

        // If flying and we happen to be on ground (e.g., brushed a slope), stop flying
        if (isFlying) {
            checkGround();
            if (player.onGround) {
                isFlying = false;
            }
        } else {
            // After moving, verify if still on ground (e.g., stepped off a ledge)
            checkGround();
        }
    }

    void removeBlock(int x, int y, int z) {
        Block* block = world.getBlockAt(x, y, z);
        if (!block) return;
        if (block->type == BLOCK_BEDROCK) return;  // cannot remove bedrock
        block->isSolid = false;
        // Mark the chunk containing this block as dirty to update its mesh
        int cx = x / CHUNK_SIZE;
        int cy = y / CHUNK_HEIGHT;
        int cz = z / CHUNK_SIZE;
        world.markChunkDirty(cx, cy, cz);
    }

    void placeBlock(int x, int y, int z) {
        Block* block = world.getBlockAt(x, y, z);
        if (!block) return;
        if (block->isSolid) return;  // cannot place if space is already occupied
        // Prevent placing blocks inside the player's own bounding box
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
        if (overlapsPlayer) {
            return; // don't place a block where the player is standing
        }
        // Place the new block
        block->isSolid = true;
        block->type = BLOCK_PLANKS;
        // Mark the chunk as dirty to refresh the mesh (including neighbors for faces)
        int cx = x / CHUNK_SIZE;
        int cy = y / CHUNK_HEIGHT;
        int cz = z / CHUNK_SIZE;
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
        // Normalize direction vector
        float len = std::sqrt(direction.x*direction.x + direction.y*direction.y + direction.z*direction.z);
        if (len != 0) {
            direction.x /= len;
            direction.y /= len;
            direction.z /= len;
        }
        // Starting block coordinates
        int bx = static_cast<int>(std::floor(origin.x));
        int by = static_cast<int>(std::floor(origin.y));
        int bz = static_cast<int>(std::floor(origin.z));
        int prevX = bx, prevY = by, prevZ = bz;
        // Determine step direction for the ray
        int stepX = (direction.x >= 0 ? 1 : -1);
        int stepY = (direction.y >= 0 ? 1 : -1);
        int stepZ = (direction.z >= 0 ? 1 : -1);
        // Calculate initial tMax and tDelta for ray-grid intersection
        float tMaxX = intbound(origin.x, direction.x);
        float tMaxY = intbound(origin.y, direction.y);
        float tMaxZ = intbound(origin.z, direction.z);
        float tDeltaX = (direction.x != 0 ? stepX / direction.x : INFINITY);
        float tDeltaY = (direction.y != 0 ? stepY / direction.y : INFINITY);
        float tDeltaZ = (direction.z != 0 ? stepZ / direction.z : INFINITY);
        float traveled = 0.0f;
        // Traverse the grid
        while (traveled <= maxDistance) {
            if (isBlockSolid(bx, by, bz)) {
                // We hit a solid block
                result.hit = true;
                result.blockPosition = { bx, by, bz };
                result.adjacentPosition = { prevX, prevY, prevZ };
                return result;
            }
            // Move to next grid cell
            prevX = bx;
            prevY = by;
            prevZ = bz;
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
        return result; // no block hit within range
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
        // Determine which block faces are exposed (not adjacent to another solid block)
        bool frontFace  = !isBlockSolid(x,     y,     z - 1); // -Z face visible?
        bool backFace   = !isBlockSolid(x,     y,     z + 1); // +Z face visible?
        bool leftFace   = !isBlockSolid(x - 1, y,     z    ); // -X face visible?
        bool rightFace  = !isBlockSolid(x + 1, y,     z    ); // +X face visible?
        bool bottomFace = !isBlockSolid(x,     y - 1, z    ); // -Y face visible?
        bool topFace    = !isBlockSolid(x,     y + 1, z    ); // +Y face visible?
        float offset = 0.002f; // slight offset to avoid z-fighting
        float minX = x - offset, maxX = x + 1.0f + offset;
        float minY = y - offset, maxY = y + 1.0f + offset;
        float minZ = z - offset, maxZ = z + 1.0f + offset;
        std::vector<float> edges;
        // Construct outline edges based on visible faces
        if (bottomFace || frontFace) {
            edges.insert(edges.end(), { minX, minY, minZ,  maxX, minY, minZ }); // front-bottom edge
        }
        if (bottomFace || rightFace) {
            edges.insert(edges.end(), { maxX, minY, minZ,  maxX, minY, maxZ }); // right-bottom edge
        }
        if (bottomFace || backFace) {
            edges.insert(edges.end(), { maxX, minY, maxZ,  minX, minY, maxZ }); // back-bottom edge
        }
        if (bottomFace || leftFace) {
            edges.insert(edges.end(), { minX, minY, maxZ,  minX, minY, minZ }); // left-bottom edge
        }
        if (topFace || frontFace) {
            edges.insert(edges.end(), { minX, maxY, minZ,  maxX, maxY, minZ }); // front-top edge
        }
        if (topFace || rightFace) {
            edges.insert(edges.end(), { maxX, maxY, minZ,  maxX, maxY, maxZ }); // right-top edge
        }
        if (topFace || backFace) {
            edges.insert(edges.end(), { maxX, maxY, maxZ,  minX, maxY, maxZ }); // back-top edge
        }
        if (topFace || leftFace) {
            edges.insert(edges.end(), { minX, maxY, maxZ,  minX, maxY, minZ }); // left-top edge
        }
        if (frontFace || leftFace) {
            edges.insert(edges.end(), { minX, minY, minZ,  minX, maxY, minZ }); // front-left vertical
        }
        if (frontFace || rightFace) {
            edges.insert(edges.end(), { maxX, minY, minZ,  maxX, maxY, minZ }); // front-right vertical
        }
        if (backFace || rightFace) {
            edges.insert(edges.end(), { maxX, minY, maxZ,  maxX, maxY, maxZ }); // back-right vertical
        }
        if (backFace || leftFace) {
            edges.insert(edges.end(), { minX, minY, maxZ,  minX, maxY, maxZ }); // back-left vertical
        }
        if (edges.empty()) {
            return; // no outline needed if block is completely surrounded
        }
        // Render the outline using the outline shader
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
        // Sync camera position with player (with bobbing offsets applied)
        camera.x = player.x;
        camera.y = player.y + 1.6f;
        camera.z = player.z;

        // Smoothly apply head bobbing effect to camera position
        float targetBob = 0.0f;
        float targetBobHorizontal = 0.0f;
        if (isMoving) {
            targetBob = sin(bobbingTime * BOBBING_FREQUENCY) * BOBBING_AMPLITUDE;
            targetBobHorizontal = sin(bobbingTime * BOBBING_FREQUENCY * 2.0f) * BOBBING_HORIZONTAL_AMPLITUDE;
        }
        // Interpolate bobbing for smooth movement
        bobbingOffset += (targetBob - bobbingOffset) * std::min(deltaTime * BOBBING_DAMPING_SPEED, 1.0f);
        bobbingHorizontalOffset += (targetBobHorizontal - bobbingHorizontalOffset) * std::min(deltaTime * BOBBING_DAMPING_SPEED, 1.0f);
        // Apply bobbing to camera coordinates
        camera.y += bobbingOffset;
        Vector3 camRight = camera.getRightVector();
        camera.x += camRight.x * bobbingHorizontalOffset;
        camera.z += camRight.z * bobbingHorizontalOffset;

        // Update viewport dimensions (in case of canvas resize)
        int width, height;
        emscripten_get_canvas_element_size("canvas", &width, &height);
        glViewport(0, 0, width, height);

        // Update projection matrix for current aspect ratio
        projection = perspective(CAM_FOV * M_PI / 180.0f, (float)width / (float)height, 0.1f, 1000.0f);

        // Clear screen with sky color (same as fog color)
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Set up main shader and pass MVP matrix and time uniform
        shader->use();
        mat4 view = camera.getViewMatrix();
        mat4 mvp = multiply(projection, view);
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.data);
        glUniform1f(timeLoc, gameTime);

        // ---- Fog params (derived from render distance in blocks) ----
        float renderRadius = float(RENDER_DISTANCE * CHUNK_SIZE);   // blocks
        float fogStart = renderRadius * 0.65f;
        float fogEnd   = renderRadius * 0.95f;
        glUniform3f(camPosLoc, camera.x, camera.y, camera.z);
        glUniform1f(fogStartLoc, fogStart);
        glUniform1f(fogEndLoc, fogEnd);
        glUniform3f(fogColorLoc, 0.53f, 0.81f, 0.92f); // sky blue

        // FIRST PASS: Draw solid (opaque) blocks with depth writes enabled
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);  // Disable face culling for all blocks
        meshManager.drawVisibleChunksSolid(player.x, player.z);
        
        // SECOND PASS: Draw water (transparent) blocks with polygon offset
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);  // Push water slightly away in depth
        glDepthMask(GL_FALSE);  // Don't write to depth buffer for transparency
        meshManager.drawVisibleChunksWater(player.x, player.z);
        glDepthMask(GL_TRUE);  // Re-enable depth writes for future rendering
        glDisable(GL_POLYGON_OFFSET_FILL);
        
        // Draw outline around targeted block, if any (with depth testing still enabled)
        if (hasHighlightedBlock) {
            renderBlockOutline(highlightedBlock.x, highlightedBlock.y, highlightedBlock.z, mvp);
        }
    }

    mat4 perspective(float fov, float aspect, float near, float far) const {
        mat4 proj;
        float tanHalfFovy = tanf(fov / 2.0f);
        proj.data[0] = 1.0f / (aspect * tanHalfFovy);
        proj.data[5] = 1.0f / tanHalfFovy;
        proj.data[10] = -(far + near) / (far - near);
        proj.data[11] = -1.0f;
        proj.data[14] = -(2.0f * far * near) / (far - near);
        // proj.data[1], [2], etc., default to 0 via mat4 constructor
        return proj;
    }

    mat4 multiply(const mat4& a, const mat4& b) const {
        mat4 result;
        // 4x4 matrix multiplication
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
