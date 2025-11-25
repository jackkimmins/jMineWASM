#ifndef GAME_INIT_HPP
#define GAME_INIT_HPP

inline void Game::init() {
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
                vec2 atlasCoord = TexCoord;

                // Atlas is 128x128 with 16x16 tiles = 8x8 grid
                float atlasWidth = 128.0;
                float atlasHeight = 128.0;
                float tileSize = 16.0;
                float tilesPerRow = 8.0;  // 8 tiles per row
                
                // Determine which tile index based on UV coordinates
                // UVs are already calculated to point to specific tiles
                vec2 pixelCoord = atlasCoord * vec2(atlasWidth, atlasHeight);
                float tileX = floor(pixelCoord.x / tileSize);
                float tileY = floor(pixelCoord.y / tileSize);
                float tileIndex = tileY * tilesPerRow + tileX;

                // Classify tiles by their index (based on getTextureIndex in mesh.hpp)
                // Water: 9-12 (4 animation frames)
                // Leaves: 8
                // Tall grass: 14
                // Flowers: 15-16
                // Glass: 19
                bool isLeaves    = (tileIndex >= 8.0  && tileIndex < 9.0);
                bool isWater     = (tileIndex >= 9.0  && tileIndex < 13.0);
                bool isTallGrass = (tileIndex >= 14.0 && tileIndex < 15.0);
                bool isFlower    = (tileIndex >= 15.0 && tileIndex < 17.0);
                bool isGlass     = (tileIndex >= 19.0 && tileIndex < 20.0);
                bool isCutout    = isLeaves || isTallGrass || isFlower;
                bool isGrassSide = (tileIndex >= 2.0 && tileIndex < 3.0); // grass side texture

                // Greedy quads collapse UVs to a single point; rebuild per-block UVs using world position
                float uvGradient = max(length(dFdx(TexCoord)), length(dFdy(TexCoord)));
                if (uvGradient < 1e-6) {
                    vec3 dx = dFdx(WorldPos);
                    vec3 dy = dFdy(WorldPos);
                    vec3 n = normalize(cross(dx, dy));
                    vec3 an = abs(n);

                    vec2 withinTile;
                    if (an.x > an.y && an.x > an.z) {
                        withinTile = vec2(fract(WorldPos.z), fract(WorldPos.y)); // +X/-X faces -> use Z/Y
                    } else if (an.y > an.z) {
                        withinTile = vec2(fract(WorldPos.x), fract(WorldPos.z)); // +Y/-Y faces -> use X/Z
                    } else {
                        withinTile = vec2(fract(WorldPos.x), fract(WorldPos.y)); // +Z/-Z faces -> use X/Y
                    }

                    float inset = 0.001;
                    vec2 base = vec2(tileX, tileY) * tileSize + inset;
                    vec2 span = vec2(tileSize - 2.0 * inset);
                    atlasCoord = (base + withinTile * span) / vec2(atlasWidth, atlasHeight);
                    pixelCoord = atlasCoord * vec2(atlasWidth, atlasHeight);
                }

                // Rotate grass side texture 180 degrees so blades point up
                if (isGrassSide) {
                    vec2 tileOrigin = vec2(tileX * tileSize, tileY * tileSize);
                    vec2 withinTile = mod(pixelCoord - tileOrigin, tileSize);
                    withinTile = mod(vec2(tileSize) - withinTile, tileSize);
                    vec2 newPixelCoord = tileOrigin + withinTile;
                    atlasCoord = newPixelCoord / vec2(atlasWidth, atlasHeight);
                    pixelCoord = newPixelCoord;
                }

                // Animate water by cycling through frames 9,10,11,12
                if (isWater) {
                    float frame = mod(floor(uTime * 2.0), 4.0);
                    
                    // Get position within the current tile
                    vec2 withinTile = mod(pixelCoord, tileSize);
                    
                    // Calculate new UV for the animated frame (tile 9 + frame offset)
                    float waterTileIndex = 9.0 + frame;
                    float waterTileX = mod(waterTileIndex, tilesPerRow);
                    float waterTileY = floor(waterTileIndex / tilesPerRow);
                    
                    vec2 newPixelCoord = vec2(waterTileX * tileSize, waterTileY * tileSize) + withinTile;
                    atlasCoord = newPixelCoord / vec2(atlasWidth, atlasHeight);
                }

                vec4 texColor = texture(uTexture, atlasCoord);

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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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

    // Create player model shader
    const char* playerVertexSrc = R"(#version 300 es
            precision mediump float;
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec2 aTexCoord;
            uniform mat4 uMVP;
            out vec2 TexCoord;
            void main() {
                gl_Position = uMVP * vec4(aPos, 1.0);
                TexCoord = aTexCoord;
            })";

    const char* playerFragmentSrc = R"(#version 300 es
            precision mediump float;
            in vec2 TexCoord;
            uniform sampler2D uSkinTexture;
            out vec4 FragColor;
            void main() {
                vec4 texColor = texture(uSkinTexture, TexCoord);
                if (texColor.a < 0.1) discard;
                FragColor = texColor;
            })";

    playerShader = new Shader(playerVertexSrc, playerFragmentSrc);
    playerMvpLoc = playerShader->getUniform("uMVP");

    // Load player skin texture
    glGenTextures(1, &playerSkinTexture);
    glBindTexture(GL_TEXTURE_2D, playerSkinTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    unsigned char* skinData = stbi_load("/assets/skin.png", &width, &height, &nrChannels, 4);
    if (!skinData) {
        std::cerr << "Failed to load player skin: assets/skin.png" << std::endl;
    } else {
        std::cout << "Loaded player skin: " << width << "x" << height << std::endl;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, skinData);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(skinData);
    }

    // Create player model
    playerModel = new PlayerModel();
    std::cout << "Player model initialized" << std::endl;

    // Setup main menu shader and background
    const char* menuVertexSrc = R"(#version 300 es
            precision mediump float;
            layout(location = 0) in vec2 aPos;
            layout(location = 1) in vec2 aTexCoord;
            uniform mat4 uProjection;
            out vec2 TexCoord;
            void main() {
                gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
                TexCoord = aTexCoord;
            })";

    const char* menuFragmentSrc = R"(#version 300 es
            precision mediump float;
            in vec2 TexCoord;
            uniform sampler2D uTexture;
            uniform float uScrollOffset;
            out vec4 FragColor;
            void main() {
                // Atlas is 128x128 with 16x16 tiles = 8x8 grid
                float atlasWidth = 128.0;
                float atlasHeight = 128.0;
                float tileSize = 16.0;
                float tilesPerRow = atlasWidth / tileSize;  // 8 tiles per row
                
                // Dirt block is at tile index 1 (second tile, first row)
                float dirtTileIndex = 1.0;
                float dirtTileX = mod(dirtTileIndex, tilesPerRow);
                float dirtTileY = floor(dirtTileIndex / tilesPerRow);
                
                // Apply scroll offset to V coordinate for downward scrolling effect
                vec2 scrolledCoord = vec2(TexCoord.x, TexCoord.y + uScrollOffset);
                
                // Take only the fractional part to tile the texture
                vec2 tiled = fract(scrolledCoord);
                
                // Map tiled coordinates to dirt block region in atlas
                vec2 atlasCoord;
                atlasCoord.x = (dirtTileX + tiled.x) * tileSize / atlasWidth;
                atlasCoord.y = (dirtTileY + tiled.y) * tileSize / atlasHeight;
                
                vec4 texColor = texture(uTexture, atlasCoord);
                // Darken the texture for background effect
                texColor.rgb *= 0.3;
                FragColor = texColor;
            })";

    menuShader = new Shader(menuVertexSrc, menuFragmentSrc);
    menuProjLoc = menuShader->getUniform("uProjection");
    menuTexLoc = menuShader->getUniform("uTexture");
    menuScrollLoc = menuShader->getUniform("uScrollOffset");

    // Load dirt texture for menu background (reuse texture atlas)
    // We'll just use the already loaded textureAtlas
    menuBackgroundTexture = textureAtlas;

    // Setup menu VAO/VBO for full-screen textured quad
    glGenVertexArrays(1, &menuVAO);
    glGenBuffers(1, &menuVBO);

    std::cout << "Main menu initialized" << std::endl;

    // Initialise and generate the world
    loadingStatus = "Initializing world...";
    world.setOnChunkDirty([this](const ChunkCoord& coord) {
        meshManager.markChunkDirty(coord);
    });
    world.initialise();

    // Check for network mode
    const char* wsUrl = std::getenv("GAME_WS_URL");
    if (!wsUrl || wsUrl[0] == '\0') {
        std::cerr << "[GAME] ERROR: This game requires a server connection." << std::endl;
        std::cerr << "[GAME] ERROR: Please set GAME_WS_URL environment variable." << std::endl;
        std::cerr << "[GAME] ERROR: The game is now online-only and cannot be played without a server." << std::endl;
        loadingStatus = "Error: Server URL required (online-only game)";
        gameState = GameState::DISCONNECTED;
        return;
    }

    std::cout << "[GAME] Online-only mode - will connect when player starts game" << std::endl;

    // Setup message handler
    netClient.setOnMessage([this](const std::string& msg) {
        this->handleServerMessage(msg);
    });

    // Setup connection handler - send hello and set_interest when connected
    netClient.setOnConnect([this]() {
        std::cout << "[GAME] Connection established, sending hello" << std::endl;
        loadingStatus = "Requesting world data...";
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

    // Don't connect yet - wait for menu selection
    std::cout << "[GAME] Connection will be initiated from main menu" << std::endl;

    // Don't load chunks until the player starts the game from the menu
    std::cout << "[GAME] Skipping world generation - will load after menu" << std::endl;

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
    camera.y = player.y + 1.8f;
    camera.z = player.z;

    // Don't initialize lastPlayerChunkX/Z here - keep them at -999/-999
    // so that hello message gets sent on first updateChunks() call

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

    // Initialize hotbar inventory with blocks
    // Slot 0: Stone
    // Slot 1: Dirt
    // Slot 2: Grass
    // Slot 3: Planks
    // Slot 4: Log
    // Slot 5: Cobblestone
    // Slot 6: Glass
    // Slot 7: Clay
    // Slot 8: Snow
    hotbarInventory.setSlot(0, BLOCK_STONE);
    hotbarInventory.setSlot(1, BLOCK_DIRT);
    hotbarInventory.setSlot(2, BLOCK_GRASS);
    hotbarInventory.setSlot(3, BLOCK_PLANKS);
    hotbarInventory.setSlot(4, BLOCK_LOG);
    hotbarInventory.setSlot(5, BLOCK_COBBLESTONE);
    hotbarInventory.setSlot(6, BLOCK_GLASS);
    hotbarInventory.setSlot(7, BLOCK_CLAY);
    hotbarInventory.setSlot(8, BLOCK_SNOW);
    hotbarInventory.selectSlot(3);

    // Create hotbar shader for rendering 3D block previews
    const char* hotbarVertexSrc = R"(#version 300 es
            precision mediump float;
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec2 aTexCoord;
            uniform mat4 uMVP;
            out vec2 TexCoord;
            void main() {
                gl_Position = uMVP * vec4(aPos, 1.0);
                TexCoord = aTexCoord;
            })";

    const char* hotbarFragmentSrc = R"(#version 300 es
            precision mediump float;
            in vec2 TexCoord;
            uniform sampler2D uTexture;
            out vec4 FragColor;
            void main() {
                vec4 texColor = texture(uTexture, TexCoord);
                if (texColor.a < 0.1) discard;
                FragColor = texColor;
            })";

    hotbarShader = new Shader(hotbarVertexSrc, hotbarFragmentSrc);
    hotbarMvpLoc = hotbarShader->getUniform("uMVP");

    // Setup hotbar VAO/VBO/EBO for rendering block previews
    glGenVertexArrays(1, &hotbarVAO);
    glGenBuffers(1, &hotbarVBO);
    glGenBuffers(1, &hotbarEBO);

    // Create sky gradient shader
    const char* skyVertexSrc = R"(#version 300 es
            precision mediump float;
            layout(location = 0) in vec3 aPos;
            out vec3 worldDir;
            uniform mat4 uView;
            uniform mat4 uProjection;
            void main() {
                // Remove translation from view matrix for skybox effect
                mat4 rotView = mat4(mat3(uView));
                vec4 pos = uProjection * rotView * vec4(aPos, 1.0);
                gl_Position = pos.xyww; // Set z to w for max depth
                
                // Use world-space position for the gradient direction
                worldDir = aPos;
            })";

    const char* skyFragmentSrc = R"(#version 300 es
        precision mediump float;
        in vec3 worldDir;
        out vec4 FragColor;
        void main() {
            // Normalize the world direction
            vec3 dir = normalize(worldDir);
            
            // Use only the Y component (vertical) for gradient
            // dir.y ranges from -1 (down) to 1 (up)
            float t = clamp((dir.y + 1.0) * 0.5, 0.0, 1.0); // Remap to 0-1
            
            // Horizon color (light blue/cyan)
            vec3 horizonColor = vec3(0.53, 0.81, 0.92);
            
            // Sky color (deeper blue)
            vec3 skyColor = vec3(0.25, 0.50, 0.85);
            
            // Make horizon reach higher: sky only takes over near the top
            vec3 color = mix(horizonColor, skyColor, smoothstep(0.4, 1.0, t));
            
            FragColor = vec4(color, 1.0);
        })";

    skyShader = new Shader(skyVertexSrc, skyFragmentSrc);
    skyViewLoc = skyShader->getUniform("uView");
    skyProjLoc = skyShader->getUniform("uProjection");

    // Setup sky VAO/VBO - a cube for the skybox
    glGenVertexArrays(1, &skyVAO);
    glGenBuffers(1, &skyVBO);

    // Skybox cube vertices
    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    glBindVertexArray(skyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    std::cout << "Sky gradient initialized" << std::endl;

    // Create particle shader
    const char* particleVertexSrc = R"(#version 300 es
            precision mediump float;
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec2 aTexCoord;
            layout(location = 2) in float aAlpha;
            uniform mat4 uMVP;
            out vec2 TexCoord;
            out float Alpha;
            void main() {
                gl_Position = uMVP * vec4(aPos, 1.0);
                TexCoord = aTexCoord;
                Alpha = aAlpha;
            })";

    const char* particleFragmentSrc = R"(#version 300 es
            precision mediump float;
            in vec2 TexCoord;
            in float Alpha;
            uniform sampler2D uTexture;
            out vec4 FragColor;
            void main() {
                vec4 texColor = texture(uTexture, TexCoord);
                if (texColor.a < 0.1) discard;
                texColor.a *= Alpha;
                FragColor = texColor;
            })";

    particleShader = new Shader(particleVertexSrc, particleFragmentSrc);
    particleMvpLoc = particleShader->getUniform("uMVP");

    std::cout << "Particle system initialized" << std::endl;

    // Create crosshair shader (using inverted colors for visibility)
    const char* crosshairVertexSrc = R"(#version 300 es
            precision mediump float;
            layout(location = 0) in vec2 aPos;
            uniform mat4 uProjection;
            void main() {
                gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
            })";

    const char* crosshairFragmentSrc = R"(#version 300 es
            precision mediump float;
            out vec4 FragColor;
            void main() {
                // White color - we'll use blend mode for inversion
                FragColor = vec4(1.0, 1.0, 1.0, 1.0);
            })";

    crosshairShader = new Shader(crosshairVertexSrc, crosshairFragmentSrc);
    crosshairProjLoc = crosshairShader->getUniform("uProjection");

    // Setup crosshair VAO/VBO
    glGenVertexArrays(1, &crosshairVAO);
    glGenBuffers(1, &crosshairVBO);

    std::cout << "Crosshair initialized" << std::endl;

    lastFrame = std::chrono::steady_clock::now();
}

#endif // GAME_INIT_HPP
