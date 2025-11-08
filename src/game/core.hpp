#ifndef GAME_CORE_HPP
#define GAME_CORE_HPP

class Game {
public:
    bool pointerLocked = false;
    GameState gameState = GameState::MAIN_MENU;
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

    // Player model rendering
    PlayerModel* playerModel = nullptr;
    Shader* playerShader = nullptr;
    GLuint playerSkinTexture = 0;
    GLint playerMvpLoc;
    std::unordered_map<std::string, RemotePlayer> remotePlayers;

    // Network client
    NetworkClient netClient;
    std::string myClientId = "";  // Our client ID from server
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

    // Main menu state
    enum MenuOption {
        MENU_PLAY_ONLINE = 0,
        MENU_SETTINGS = 1,
        MENU_GITHUB = 2,
        MENU_MAX = 3
    };
    int selectedMenuOption = MENU_PLAY_ONLINE;
    bool lastUpKeyState = false;
    bool lastDownKeyState = false;
    bool lastEnterKeyState = false;
    float menuMouseX = 0.0f;
    float menuMouseY = 0.0f;
    GLuint menuBackgroundTexture = 0;
    Shader* menuShader = nullptr;
    GLuint menuVAO = 0, menuVBO = 0;
    GLint menuProjLoc;
    GLint menuTexLoc;
    GLint menuScrollLoc;
    float menuScrollOffset = 0.0f;

    Game();

    void init();
    void mainLoop();

    void handleKey(int keyCode, bool pressed);
    void handleMouseMove(float movementX, float movementY);
    void handleMouseClick(int button);
    void handleMenuMouseMove(float x, float y);
    void handleMenuClick(float x, float y);

    void updateChunks();
    void updateHighlightedBlock();

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

    // Menu
    void handleMenuSelection();
    void renderMainMenu(int width, int height);

    // Timing
    float calculateDeltaTime();

    // World / physics helpers
    bool isBlockSolid(int x, int y, int z) const;
    bool isBlockSelectable(int x, int y, int z) const;
    bool isWaterBlock(int x, int y, int z) const;
    bool isPlayerInWater() const;
    bool findSafeSpawn(float startX, float startZ, Vector3& outPos);
    void checkGround();
    bool isColliding(float x, float y, float z) const;

    // Physics & movement
    void applyPhysics(float dt);
    void processInput(float dt);

    // Edits
    void removeBlock(int x, int y, int z);
    void placeBlock(int x, int y, int z);

    // Raycast
    struct RaycastHit {
        bool hit;
        Vector3i blockPosition;
        Vector3i adjacentPosition;
    };
    RaycastHit raycast(float maxDistance);
    float intbound(float s, float ds);

    // Rendering
    void renderBlockOutline(int x, int y, int z, const mat4& mvp);
    void render();
    void renderRemotePlayers(const mat4& view);
    void renderUI();

    // Networking
    void sendHelloMessage();
    void sendInterestMessage(int centerX, int centerZ);
    void sendPoseUpdate();
    void handleServerMessage(const std::string& message);
    void handleHelloOk(const std::string& message);
    void handleChunkFull(const std::string& message);
    void handleChunkUnload(const std::string& message);
    void handleBlockUpdate(const std::string& message);
    void handlePlayerSnapshot(const std::string& message);

    // Math
    mat4 perspective(float fov, float aspect, float near, float far) const;
    mat4 multiply(const mat4& a, const mat4& b) const;
};

inline Game::Game() : shader(nullptr), outlineShader(nullptr), player(SPAWN_X, SPAWN_Y, SPAWN_Z) {
    std::cout << "Game Constructed - Player Spawn: (" << SPAWN_X << ", " << SPAWN_Y << ", " << SPAWN_Z << ")" << std::endl;
}

#endif // GAME_CORE_HPP