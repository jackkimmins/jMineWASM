// game.hpp
#ifndef GAME_HPP
#define GAME_HPP

// Game Class
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
    GLint outlineMvpLoc;
    std::chrono::steady_clock::time_point lastFrame;
    bool keys[1024] = { false };
    GLuint textureAtlas;
    GLuint outlineVAO, outlineVBO;
    Vector3i highlightedBlock;
    bool hasHighlightedBlock = false;
    int lastPlayerChunkX = -999;
    int lastPlayerChunkZ = -999;

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
            void main() {
                gl_Position = uMVP * vec4(aPos, 1.0);
                TexCoord = aTexCoord;
                AO = aAO;
            })";

        const char* fragmentSrc = R"(#version 300 es
            precision mediump float;
            in vec2 TexCoord;
            in float AO;
            uniform sampler2D uTexture;
            uniform float uTime;
            out vec4 FragColor;
            void main() {
                vec2 texCoord = TexCoord;
                
                // Water animation: check if we're in water texture range (tiles 10-13)
                float tileWidth = 16.0 / 240.0;
                float tileX = floor(texCoord.x / tileWidth);
                
                if (tileX >= 10.0 && tileX < 14.0) {
                    // Animated water texture
                    float frame = mod(floor(uTime * 2.0), 4.0); // 2 FPS, 4 frames
                    float offsetX = frame * tileWidth;
                    texCoord.x = mod(texCoord.x, tileWidth) + 10.0 * tileWidth + offsetX;
                }
                
                vec4 texColor = texture(uTexture, texCoord);
                
                // Make water semi-transparent
                if (tileX >= 10.0 && tileX < 14.0) {
                    texColor.a *= 0.7; // 70% opacity for water
                }
                
                texColor.rgb *= 1.0 - AO; // Apply AO to darken the color
                FragColor = texColor;
            })";

        // Compile and link shaders
        shader = new Shader(vertexSrc, fragmentSrc);
        shader->use();
        mvpLoc = shader->getUniform("uMVP");
        timeLoc = shader->getUniform("uTime");

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

        // Set player's starting position based on terrain height
        int startX = WORLD_SIZE_X / 2;
        int startZ = WORLD_SIZE_Z / 2;

        // Initialise the world with the spawn position for the player
        int maxHeight = world.getHeightAt(static_cast<int>(SPAWN_X), static_cast<int>(SPAWN_Z));
        player.x = SPAWN_X;
        player.y = maxHeight + 1.6f;
        player.z = SPAWN_Z;

        // Update camera position to match spawn
        camera.x = SPAWN_X;
        camera.y = SPAWN_Y;
        camera.z = SPAWN_Z;
        
        // Track player chunk position
        lastPlayerChunkX = static_cast<int>(std::floor(player.x / CHUNK_SIZE));
        lastPlayerChunkZ = static_cast<int>(std::floor(player.z / CHUNK_SIZE));

        // Get initial canvas size
        int canvasWidth, canvasHeight;
        emscripten_get_canvas_element_size("canvas", &canvasWidth, &canvasHeight);

        // Initialise projection matrix with dynamic aspect ratio
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
        deltaTime = calculateDeltaTime();
        gameTime += deltaTime; // Accumulate time for animations
        processInput(deltaTime);
        applyPhysics(deltaTime);
        if (isMoving) bobbingTime += deltaTime;
        updateHighlightedBlock();
        updateChunks(); // Handle chunk loading/unloading
        meshManager.updateDirtyChunks(world); // Regenerate modified chunks
        render();
    }

    void handleKey(int keyCode, bool pressed) {
        keys[keyCode] = pressed;

        // Check for space key to jump
        if (keyCode == 32 && pressed && player.onGround) {
            player.velocityY = JUMP_VELOCITY;
            player.onGround = false;
        }
        
        // Check for double-tap on W key (forward) to toggle sprint
        if (keyCode == 87) { // W key
            if (pressed && !lastWKeyState) {
                // W key just pressed
                float timeSinceLastPress = currentTime - lastWKeyPressTime;
                if (timeSinceLastPress < DOUBLE_TAP_TIME) {
                    // Double tap detected - toggle sprint
                    isSprinting = true;
                }
                lastWKeyPressTime = currentTime;
            }
            lastWKeyState = pressed;
            
            // Exit sprint if W key is released
            if (!pressed && isSprinting) {
                isSprinting = false;
            }
        }
        
        // Exit sprint if moving backwards or sideways only
        if (keyCode == 83 && pressed) { // S key (backward)
            isSprinting = false;
        }
    }

    void handleMouseMove(float movementX, float movementY) {
        if (!pointerLocked) return;
        camera.yaw += movementX * SENSITIVITY;
        camera.pitch = std::clamp(camera.pitch - movementY * SENSITIVITY, -89.0f, 89.0f);
    }

    void handleMouseClick(int button) {
        float maxDistance = 4.5f;
        RaycastHit hit = raycast(maxDistance);
        if (hit.hit) {
            if (button == 0) {
                removeBlock(hit.blockPosition.x, hit.blockPosition.y, hit.blockPosition.z);
            }
            else if (button == 2) {
                placeBlock(hit.adjacentPosition.x, hit.adjacentPosition.y, hit.adjacentPosition.z);
            }
        }
    }

    void updateChunks() {
        // Check if player moved to a new chunk
        int currentChunkX = static_cast<int>(std::floor(player.x / CHUNK_SIZE));
        int currentChunkZ = static_cast<int>(std::floor(player.z / CHUNK_SIZE));
        
        if (currentChunkX != lastPlayerChunkX || currentChunkZ != lastPlayerChunkZ) {
            // Player moved to new chunk, update loaded chunks
            world.loadChunksAroundPosition(player.x, player.z);
            world.unloadDistantChunks(player.x, player.z);
            
            // Generate meshes for newly loaded chunks
            for (const auto& coord : world.getLoadedChunks()) {
                if (meshManager.chunkMeshes.find(coord) == meshManager.chunkMeshes.end()) {
                    meshManager.generateChunkMesh(world, coord.x, coord.y, coord.z);
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
    
    // Sprint state tracking
    bool isSprinting = false;
    bool lastWKeyState = false;
    float lastWKeyPressTime = 0.0f;
    float currentTime = 0.0f;

    float calculateDeltaTime() {
        auto now = std::chrono::steady_clock::now();
        float delta = std::chrono::duration<float>(now - lastFrame).count();
        lastFrame = now;
        return delta;
    }

    bool isBlockSolid(int x, int y, int z) const { return world.isSolidAt(x, y, z); }

    void checkGround() {
        float epsilon = 0.001f;
        // Check if there's a block directly beneath the player
        if (isColliding(player.x, player.y - epsilon, player.z)) player.onGround = true;
        else player.onGround = false;
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

        for (int bx = std::floor(minX); bx <= std::floor(maxX); ++bx)
            for (int by = std::floor(minY); by <= std::floor(maxY); ++by)
                for (int bz = std::floor(minZ); bz <= std::floor(maxZ); ++bz)
                    if (isBlockSolid(bx, by, bz)) return true;

        return false;
    }

    void applyPhysics(float dt) {
        player.velocityY += GRAVITY * dt;
        float newY = player.y + player.velocityY * dt;

        // This stops the player from falling through the world into oblivion
        if (player.y < -1.0f) {
            // Teleport player back to spawn
            player.x = SPAWN_X;
            player.y = SPAWN_Y;
            player.z = SPAWN_Z;
            player.velocityY = 0;
            player.onGround = true;
            
            // Update camera position to match respawn
            camera.x = SPAWN_X;
            camera.y = SPAWN_Y;
            camera.z = SPAWN_Z;
            
            // Update chunk tracking to force chunk reload around spawn
            lastPlayerChunkX = static_cast<int>(std::floor(player.x / CHUNK_SIZE));
            lastPlayerChunkZ = static_cast<int>(std::floor(player.z / CHUNK_SIZE));
            
            // Reload chunks around spawn position
            world.loadChunksAroundPosition(SPAWN_X, SPAWN_Z);
            world.unloadDistantChunks(SPAWN_X, SPAWN_Z);
            
            // Generate meshes for chunks around spawn
            for (const auto& coord : world.getLoadedChunks()) {
                if (meshManager.chunkMeshes.find(coord) == meshManager.chunkMeshes.end()) {
                    meshManager.generateChunkMesh(world, coord.x, coord.y, coord.z);
                } else {
                    // Regenerate existing chunk meshes to ensure they're up to date
                    meshManager.generateChunkMesh(world, coord.x, coord.y, coord.z);
                }
            }
            
            return;
        }

        if (player.velocityY > 0) {
            if (!isColliding(player.x, newY, player.z))  player.y = newY;
            else {
                // Collision above
                player.y = std::floor(newY);
                player.velocityY = 0;
            }
        }
        else { // Moving down or stationary
            if (!isColliding(player.x, newY, player.z)) {
                player.y = newY;
                player.onGround = false;
            }
            else {
                // Collision below
                player.y = std::floor(newY) + 1.0f;
                player.velocityY = 0;
                player.onGround = true;
            }
        }

        checkGround();
    }

    // Handle player input
    void processInput(float dt) {
        // Update current time for sprint tracking
        currentTime += dt;
        
        // Determine effective speed (normal or sprint)
        float speed = isSprinting ? SPRINT_SPEED : PLAYER_SPEED;
        float velocity = speed * dt;
        float radYaw = camera.yaw * M_PI / 180.0f;

        float frontX = cosf(radYaw);
        float frontZ = sinf(radYaw);
        float rightX = -sinf(radYaw);
        float rightZ = cosf(radYaw);

        float moveX = 0.0f, moveZ = 0.0f;

        if (keys[87]) { moveX += frontX * velocity; moveZ += frontZ * velocity; } // W
        if (keys[83]) { moveX -= frontX * velocity; moveZ -= frontZ * velocity; } // S
        if (keys[65]) { moveX -= rightX * velocity; moveZ -= rightZ * velocity; } // A
        if (keys[68]) { moveX += rightX * velocity; moveZ += rightZ * velocity; } // D

        // Exit sprint if not pressing forward
        if (!keys[87] && isSprinting) {
            isSprinting = false;
        }

        // Detect if the player is moving
        isMoving = (moveX != 0.0f || moveZ != 0.0f);

        float newX = player.x + moveX;
        if (!isColliding(newX, player.y, player.z)) player.x = newX;

        float newZ = player.z + moveZ;
        if (!isColliding(player.x, player.y, newZ)) player.z = newZ;

        // After processing input, make sure the player is still on the ground
        checkGround();
    }

    void removeBlock(int x, int y, int z) {
        Block* block = world.getBlockAt(x, y, z);
        if (!block) return;
        
        // Don't remove bedrock
        if (block->type == BLOCK_BEDROCK) return;
        
        block->isSolid = false;
        
        // Mark chunk as dirty
        int cx = x / CHUNK_SIZE;
        int cy = y / CHUNK_HEIGHT;
        int cz = z / CHUNK_SIZE;
        world.markChunkDirty(cx, cy, cz);
    }

    void placeBlock(int x, int y, int z) {
        Block* block = world.getBlockAt(x, y, z);
        if (!block) return;
        
        // Don't place a block if there's already a solid block there
        if (block->isSolid) return;
        
        // Prevent placing a block inside the player's bounding box
        float halfWidth = 0.3f;
        float halfDepth = 0.3f;
        
        // Player bounding box
        float playerMinX = player.x - halfWidth;
        float playerMaxX = player.x + halfWidth;
        float playerMinY = player.y;
        float playerMaxY = player.y + PLAYER_HEIGHT;
        float playerMinZ = player.z - halfDepth;
        float playerMaxZ = player.z + halfDepth;
        
        // Block bounding box
        float blockMinX = x;
        float blockMaxX = x + 1.0f;
        float blockMinY = y;
        float blockMaxY = y + 1.0f;
        float blockMinZ = z;
        float blockMaxZ = z + 1.0f;
        
        // Check for AABB overlap
        bool overlaps = (playerMinX < blockMaxX && playerMaxX > blockMinX) &&
                       (playerMinY < blockMaxY && playerMaxY > blockMinY) &&
                       (playerMinZ < blockMaxZ && playerMaxZ > blockMinZ);
        
        if (!overlaps) {
            block->isSolid = true;
            block->type = BLOCK_PLANKS;
            
            // Mark chunk as dirty
            int cx = x / CHUNK_SIZE;
            int cy = y / CHUNK_HEIGHT;
            int cz = z / CHUNK_SIZE;
            world.markChunkDirty(cx, cy, cz);
        }
    }

    struct RaycastHit {
        bool hit;
        Vector3i blockPosition;
        Vector3i adjacentPosition; // Pos to place a block (if right-clicked)
    };

    RaycastHit raycast(float maxDistance) {
        RaycastHit hitResult;
        hitResult.hit = false;
        Vector3 rayOrigin = { camera.x, camera.y, camera.z };
        Vector3 rayDirection = camera.getFrontVector();        // Normalise the direction
        float dirLength = sqrt(rayDirection.x * rayDirection.x + rayDirection.y * rayDirection.y + rayDirection.z * rayDirection.z);
        rayDirection.x /= dirLength;
        rayDirection.y /= dirLength;
        rayDirection.z /= dirLength;

        // Current block position
        int x = static_cast<int>(floor(rayOrigin.x));
        int y = static_cast<int>(floor(rayOrigin.y));
        int z = static_cast<int>(floor(rayOrigin.z));

        // Track the previous block position (for adjacent placement)
        int prevX = x;
        int prevY = y;
        int prevZ = z;

        // Direction of the ray (+1 or -1)
        int stepX = (rayDirection.x >= 0) ? 1 : -1;
        int stepY = (rayDirection.y >= 0) ? 1 : -1;
        int stepZ = (rayDirection.z >= 0) ? 1 : -1;

        // Compute tMaxX, tMaxY, tMaxZ
        // The distance along the ray to the next block boundary
        float tMaxX = intbound(rayOrigin.x, rayDirection.x);
        float tMaxY = intbound(rayOrigin.y, rayDirection.y);
        float tMaxZ = intbound(rayOrigin.z, rayDirection.z);

        // Compute tDeltaX, tDeltaY, tDeltaZ
        float tDeltaX = (rayDirection.x != 0) ? (stepX / rayDirection.x) : INFINITY;
        float tDeltaY = (rayDirection.y != 0) ? (stepY / rayDirection.y) : INFINITY;
        float tDeltaZ = (rayDirection.z != 0) ? (stepZ / rayDirection.z) : INFINITY;

        float distanceTravelled = 0.0f;

        while (distanceTravelled <= maxDistance) {
            // Check if the current block is solid
            if (isBlockSolid(x, y, z)) {
                hitResult.hit = true;
                hitResult.blockPosition = { x, y, z };
                // The adjacent position is where we came from (previous block)
                hitResult.adjacentPosition = { prevX, prevY, prevZ };
                return hitResult;
            }

            // Store current position before moving
            prevX = x;
            prevY = y;
            prevZ = z;

            // Move to next block boundary
            if (tMaxX < tMaxY) {
                if (tMaxX < tMaxZ) {
                    x += stepX;
                    distanceTravelled = tMaxX;
                    tMaxX += tDeltaX;
                }
                else {
                    z += stepZ;
                    distanceTravelled = tMaxZ;
                    tMaxZ += tDeltaZ;
                }
            }
            else {
                if (tMaxY < tMaxZ) {
                    y += stepY;
                    distanceTravelled = tMaxY;
                    tMaxY += tDeltaY;
                }
                else {
                    z += stepZ;
                    distanceTravelled = tMaxZ;
                    tMaxZ += tDeltaZ;
                }
            }
        }
        // No block hit within maxDistance
        return hitResult;
    }

    float intbound(float s, float ds) {
        // Find the distance from s to the next integer boundary
        if (ds == 0.0f) return INFINITY;
        else {
            float sInt = floor(s);
            if (ds > 0) return (sInt + 1.0f - s) / ds;
            else return (s - sInt) / -ds;
        }
    }

    void renderBlockOutline(int x, int y, int z, const mat4& mvp) {
        // Check which faces are exposed (not blocked by adjacent solid blocks)
        bool frontFace = !isBlockSolid(x, y, z - 1);  // -Z
        bool backFace = !isBlockSolid(x, y, z + 1);   // +Z
        bool leftFace = !isBlockSolid(x - 1, y, z);   // -X
        bool rightFace = !isBlockSolid(x + 1, y, z);  // +X
        bool bottomFace = !isBlockSolid(x, y - 1, z); // -Y
        bool topFace = !isBlockSolid(x, y + 1, z);    // +Y

        float offset = 0.002f; // Slight offset to prevent z-fighting but stay visible
        float minX = x - offset;
        float maxX = x + 1.0f + offset;
        float minY = y - offset;
        float maxY = y + 1.0f + offset;
        float minZ = z - offset;
        float maxZ = z + 1.0f + offset;

        // Build edges dynamically based on visible faces
        std::vector<float> edges;

        // Bottom face edges (only if bottom or adjacent vertical faces are visible)
        if (bottomFace || frontFace) {
            edges.insert(edges.end(), {minX, minY, minZ,  maxX, minY, minZ}); // Front bottom
        }
        if (bottomFace || rightFace) {
            edges.insert(edges.end(), {maxX, minY, minZ,  maxX, minY, maxZ}); // Right bottom
        }
        if (bottomFace || backFace) {
            edges.insert(edges.end(), {maxX, minY, maxZ,  minX, minY, maxZ}); // Back bottom
        }
        if (bottomFace || leftFace) {
            edges.insert(edges.end(), {minX, minY, maxZ,  minX, minY, minZ}); // Left bottom
        }

        // Top face edges (only if top or adjacent vertical faces are visible)
        if (topFace || frontFace) {
            edges.insert(edges.end(), {minX, maxY, minZ,  maxX, maxY, minZ}); // Front top
        }
        if (topFace || rightFace) {
            edges.insert(edges.end(), {maxX, maxY, minZ,  maxX, maxY, maxZ}); // Right top
        }
        if (topFace || backFace) {
            edges.insert(edges.end(), {maxX, maxY, maxZ,  minX, maxY, maxZ}); // Back top
        }
        if (topFace || leftFace) {
            edges.insert(edges.end(), {minX, maxY, maxZ,  minX, maxY, minZ}); // Left top
        }

        // Vertical edges (only if adjacent faces are visible)
        if (frontFace || leftFace) {
            edges.insert(edges.end(), {minX, minY, minZ,  minX, maxY, minZ}); // Front left
        }
        if (frontFace || rightFace) {
            edges.insert(edges.end(), {maxX, minY, minZ,  maxX, maxY, minZ}); // Front right
        }
        if (backFace || rightFace) {
            edges.insert(edges.end(), {maxX, minY, maxZ,  maxX, maxY, maxZ}); // Back right
        }
        if (backFace || leftFace) {
            edges.insert(edges.end(), {minX, minY, maxZ,  minX, maxY, maxZ}); // Back left
        }

        // Only render if there are visible edges
        if (edges.empty()) return;

        outlineShader->use();
        glUniformMatrix4fv(outlineMvpLoc, 1, GL_FALSE, mvp.data);

        glBindVertexArray(outlineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, outlineVBO);
        glBufferData(GL_ARRAY_BUFFER, edges.size() * sizeof(float), edges.data(), GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        glLineWidth(2.0f); // Make lines 2px wide
        glDrawArrays(GL_LINES, 0, edges.size() / 3);
        
        glBindVertexArray(0);
    }

    void render() {
        // Sync the camera pos with the player pos
        camera.x = player.x;
        camera.y = player.y + 1.6f;
        camera.z = player.z;

        // Compute target bobbing amounts
        float targetBobbingAmount = 0.0f;
        float targetHorizontalBobbingAmount = 0.0f;
        if (isMoving) {
            targetBobbingAmount = sin(bobbingTime * BOBBING_FREQUENCY) * BOBBING_AMPLITUDE;
            targetHorizontalBobbingAmount = sin(bobbingTime * BOBBING_FREQUENCY * 2.0f) * BOBBING_HORIZONTAL_AMPLITUDE;
        }

        // Smoothly interpolate bobbing offsets towards target values
        bobbingOffset += (targetBobbingAmount - bobbingOffset) * std::min(deltaTime * BOBBING_DAMPING_SPEED, 1.0f);
        bobbingHorizontalOffset += (targetHorizontalBobbingAmount - bobbingHorizontalOffset) * std::min(deltaTime * BOBBING_DAMPING_SPEED, 1.0f);

        // Apply vertical bobbing to the camera's Y position
        camera.y += bobbingOffset;

        // Apply horizontal bobbing to the camera's X and Z positions
        Vector3 right = camera.getRightVector();
        camera.x += right.x * bobbingHorizontalOffset;
        camera.z += right.z * bobbingHorizontalOffset; 

        // Get actual canvas size for responsive rendering
        int width, height;
        emscripten_get_canvas_element_size("canvas", &width, &height);
        glViewport(0, 0, width, height);

        // Update projection matrix if the aspect ratio has changed
        projection = perspective(CAM_FOV * M_PI / 180.0f, static_cast<float>(width) / static_cast<float>(height), 0.1f, 1000.0f);

        // Clear the screen - Sky Colour
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Use shader and set MVP matrix
        shader->use();
        mat4 view = camera.getViewMatrix();
        mat4 mvp = multiply(projection, view);
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.data);
        
        // Pass time to shader for water animation
        glUniform1f(timeLoc, gameTime);

        // Draw visible chunk meshes
        meshManager.drawVisibleChunks(player.x, player.z);

        // Draw block outline if a block is being looked at
        if (hasHighlightedBlock) {
            // Keep depth test enabled so the block occludes its own back edges
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
        return proj;
    }

    mat4 multiply(const mat4& a, const mat4& b) const {
        mat4 result;
        for(int row=0; row<4; ++row)
            for(int col=0; col<4; ++col)
                for(int k=0; k<4; ++k)
                    result.data[col * 4 + row] += a.data[k * 4 + row] * b.data[col * 4 + k];

        return result;
    }
};

#endif