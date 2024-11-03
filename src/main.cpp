#define STB_IMAGE_IMPLEMENTATION

#include <GLES3/gl3.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <iostream>
#include <cmath>
#include <vector>
#include <chrono>
#include <algorithm>
#include "stb_image.h"

// World Dimensions
constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_HEIGHT = 16;

constexpr int WORLD_CHUNK_SIZE_X = 4;
constexpr int WORLD_CHUNK_SIZE_Y = 3;
constexpr int WORLD_CHUNK_SIZE_Z = 4;

constexpr int WORLD_SIZE_X = CHUNK_SIZE * WORLD_CHUNK_SIZE_X;
constexpr int WORLD_SIZE_Y = CHUNK_HEIGHT * WORLD_CHUNK_SIZE_Y;
constexpr int WORLD_SIZE_Z = CHUNK_SIZE * WORLD_CHUNK_SIZE_Z;

// The world spawn position is the calculated centre of the world.
constexpr float SPAWN_X = WORLD_SIZE_X / 2.0f;
constexpr float SPAWN_Y = WORLD_SIZE_Y + 1.6f;
constexpr float SPAWN_Z = WORLD_SIZE_Z / 2.0f;

// Input Handling
constexpr float BLOCK_SIZE = 1.0f;
constexpr float GRAVITY = -13.5f;
constexpr float JUMP_VELOCITY = 6.0f;
constexpr float PLAYER_SPEED = 6.0f;
constexpr float SENSITIVITY = 0.15f;
constexpr float CAM_FOV = 80.0f;
constexpr float epsilon = 0.001f;
constexpr float PLAYER_HEIGHT = 1.8f;

// Constants for bobbing effect
static constexpr float BOBBING_FREQUENCY = 18.0f;
static constexpr float BOBBING_AMPLITUDE = 0.2f;
static constexpr float BOBBING_HORIZONTAL_AMPLITUDE = 0.05f;
static constexpr float BOBBING_DAMPING_SPEED = 4.0f;

// Perlin Terrain Generation
constexpr unsigned int PERLIN_SEED = 42;
constexpr float PERLIN_FREQUENCY = 0.004f;
constexpr int PERLIN_OCTAVES = 6;
constexpr float PERLIN_PERSISTENCE = 0.5f;
constexpr float PERLIN_LACUNARITY = 1.8f;
constexpr float TERRAIN_HEIGHT_SCALE = 30.0f;

// Cave Generation Constants
constexpr int CAVE_START_DEPTH = 5;
constexpr int CAVE_END_DEPTH = 10;

// Cave Tunneling Parameters
constexpr int NUM_CAVES = 50;
constexpr int CAVE_LENGTH = 100;
constexpr float CAVE_RADIUS_MIN = 1.0f;
constexpr float CAVE_RADIUS_MAX = 4.0f;
constexpr float CAVE_DIRECTION_CHANGE = 0.2f;

// Ore Generation Constants
constexpr int COAL_ORE_MIN_Y = 5;
constexpr int COAL_ORE_MAX_Y = 50;
constexpr float COAL_ORE_CHANCE = 0.02f;

constexpr int IRON_ORE_MIN_Y = 5;
constexpr int IRON_ORE_MAX_Y = 40;
constexpr float IRON_ORE_CHANCE = 0.015f;

// Texture Atlas and Ambient Occlusion
constexpr int ATLAS_TILE_SIZE = 16;
constexpr int ATLAS_TILES_WIDTH = 160;
constexpr int ATLAS_TILES_HEIGHT = 16;
constexpr float AO_STRENGTH = 0.5f;

// Utility Matrix Structure
struct mat4 { float data[16] = {0}; };
struct Vector3 { float x, y, z; };
struct Vector3i { int x, y, z; };

// Player Class
class Player {
public:
    float x, y, z;
    float velocityY = 0.0f;
    bool onGround = false;

    Player(float startX, float startY, float startZ) : x(startX), y(startY), z(startZ) {}
};

#include "perlin_noise.hpp"
#include "shaders.hpp"
#include "camera.hpp"
#include "blocks_chunks_worlds.hpp"
#include "mesh.hpp"
#include "game.hpp"

// Global Game Instance
Game* gameInstance = nullptr;

// Extern Functions
extern "C" void setPointerLocked(bool locked) {
    if (gameInstance) {
        gameInstance->pointerLocked = locked;
    }
}

// Callback Functions
EM_BOOL key_callback(int eventType, const EmscriptenKeyboardEvent *e, void *userData) {
    if(eventType == EMSCRIPTEN_EVENT_KEYDOWN || eventType == EMSCRIPTEN_EVENT_KEYUP) {
        bool pressed = eventType == EMSCRIPTEN_EVENT_KEYDOWN;
        gameInstance->handleKey(e->keyCode, pressed);
    }
    return EM_TRUE;
}

EM_BOOL mouse_callback(int eventType, const EmscriptenMouseEvent *e, void *userData) {
    if(eventType == EMSCRIPTEN_EVENT_MOUSEMOVE) {
        gameInstance->handleMouseMove(static_cast<float>(e->movementX), static_cast<float>(e->movementY));
    }
    return EM_TRUE;
}

EM_BOOL mouse_button_callback(int eventType, const EmscriptenMouseEvent *e, void *userData) {
    if(eventType == EMSCRIPTEN_EVENT_MOUSEDOWN) {
        gameInstance->handleMouseClick(e->button);
    }
    return EM_TRUE;
}

// Main Loop Wrapper
void main_loop() {
    if(gameInstance)
        gameInstance->mainLoop();
}

int main() {
    // Initialise WebGL context attributes
    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.alpha = false;
    attr.depth = true;
    attr.stencil = false;
    attr.antialias = true;
    attr.majorVersion = 2;

    // Create WebGL 2.0 context
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("canvas", &attr);
    if(ctx <= 0) {
        std::cerr << "Failed to create WebGL context" << std::endl;
        return -1;
    }

    // Make the context current
    emscripten_webgl_make_context_current(ctx);

    // Initialise the Game instance
    Game game;
    game.init();
    gameInstance = &game;

    // Set up input event handlers
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, key_callback);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, key_callback);
    emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, mouse_callback);
    emscripten_set_mousedown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, mouse_button_callback);

    // Start the main loop
    emscripten_set_main_loop(main_loop, 0, 1);

    return 0;
}