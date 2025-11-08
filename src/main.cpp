// main.cpp
#define STB_IMAGE_IMPLEMENTATION

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include <iostream>
#include <cmath>
#include <vector>
#include <chrono>
#include <algorithm>
#include "stb_image.h"

#include "../shared/config.hpp"
#include "../shared/types.hpp"
#include "../shared/chunk.hpp"

// Player Class
class Player {
public:
    float x, y, z;
    float velocityY = 0.0f;
    bool onGround = false;

    Player(float startX, float startY, float startZ) : x(startX), y(startY), z(startZ) {}
};

#include "../shared/perlin_noise.hpp"
#include "shaders.hpp"
#include "camera.hpp"
#include "../shared/world_generation.hpp"
#include "mesh.hpp"
#include "game.hpp"

// Global Game Instance 
Game* gameInstance = nullptr;

// Extern Functions
extern "C" void setPointerLocked(bool locked) {
    if (gameInstance) gameInstance->pointerLocked = locked;
}


// Callback Functions
EM_BOOL key_callback(int eventType, const EmscriptenKeyboardEvent *e, void *userData) {
    if (eventType == EMSCRIPTEN_EVENT_KEYDOWN || eventType == EMSCRIPTEN_EVENT_KEYUP) {
        bool pressed = eventType == EMSCRIPTEN_EVENT_KEYDOWN;
        gameInstance->handleKey(e->keyCode, pressed);
    }
    return EM_TRUE;
}

EM_BOOL mouse_callback(int eventType, const EmscriptenMouseEvent *e, void *userData) {
    // Only handle mouse movement during gameplay, not in menu
    if (eventType == EMSCRIPTEN_EVENT_MOUSEMOVE && gameInstance->gameState == GameState::PLAYING) {
        gameInstance->handleMouseMove(static_cast<float>(e->movementX), static_cast<float>(e->movementY));
    }
    return EM_TRUE;
}

EM_BOOL mouse_button_callback(int eventType, const EmscriptenMouseEvent *e, void *userData) {
    // Only handle mouse clicks during gameplay, not in menu
    if (eventType == EMSCRIPTEN_EVENT_MOUSEDOWN && gameInstance->gameState == GameState::PLAYING) {
        gameInstance->handleMouseClick(e->button);
    }
    return EM_TRUE;
}

// Main Loop Wrapper
void main_loop() {
    if (gameInstance) gameInstance->mainLoop();
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

    // Make context current
    emscripten_webgl_make_context_current(ctx);

    // Initialise Game instance
    Game game;
    game.init();
    gameInstance = &game;

    // Input event handlers
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, key_callback);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, key_callback);
    emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, mouse_callback);
    emscripten_set_mousedown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, mouse_button_callback);

    // Start main loop
    emscripten_set_main_loop(main_loop, 0, 1);

    return 0;
}