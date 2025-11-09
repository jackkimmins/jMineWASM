// shared/config.hpp
#ifndef SHARED_CONFIG_HPP
#define SHARED_CONFIG_HPP

// World Dimensions
constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_HEIGHT = 16;

// World is 64x64 chunks (1024x1024 blocks)
constexpr int WORLD_CHUNK_SIZE_X = 64;
constexpr int WORLD_CHUNK_SIZE_Y = 8;
constexpr int WORLD_CHUNK_SIZE_Z = 64;

constexpr int WORLD_SIZE_X = CHUNK_SIZE * WORLD_CHUNK_SIZE_X;
constexpr int WORLD_SIZE_Y = CHUNK_HEIGHT * WORLD_CHUNK_SIZE_Y;
constexpr int WORLD_SIZE_Z = CHUNK_SIZE * WORLD_CHUNK_SIZE_Z;

// Render distance
constexpr int RENDER_DISTANCE = 8;
constexpr int CHUNK_LOAD_DISTANCE = RENDER_DISTANCE + 2;

// The world spawn position - centre of the world
constexpr float SPAWN_X = WORLD_SIZE_X / 2.0f;
constexpr float SPAWN_Y = WORLD_SIZE_Y + 100.6f;
constexpr float SPAWN_Z = WORLD_SIZE_Z / 2.0f;

// Input Handling
constexpr float BLOCK_SIZE = 1.0f;
constexpr float GRAVITY = -28.0f;
constexpr float JUMP_VELOCITY = 8.5f;
constexpr float PLAYER_SPEED = 4.317f;
constexpr float SPRINT_SPEED = 6.612f;
constexpr float DOUBLE_TAP_TIME = 0.3f;
constexpr float SENSITIVITY = 0.18f;
constexpr float CAM_FOV = 80.0f;
constexpr float epsilon = 0.001f;
constexpr float PLAYER_HEIGHT = 1.8f;

// Movement acceleration/deceleration
constexpr float GROUND_ACCELERATION = 25.0f;
constexpr float AIR_ACCELERATION = 8.0f;
constexpr float GROUND_FRICTION = 18.0f;
constexpr float AIR_FRICTION = 0.5f;
constexpr float FLY_FRICTION = 10.0f;

// Jump improvements
constexpr float COYOTE_TIME = 0.1f;              // Grace period after leaving ledge
constexpr float JUMP_BUFFER_TIME = 0.15f;        // Early jump input buffer
constexpr float STEP_HEIGHT = 0.6f;              // Auto step-up height (Minecraft: 0.6 blocks)

// Swimming
constexpr float SWIM_SPEED = 2.2f;               // Minecraft swimming speed
constexpr float SWIM_UP_SPEED = 4.0f;            // Upward swimming velocity
constexpr float WATER_GRAVITY = -4.0f;           // Slower sinking in water
constexpr float WATER_JUMP_VELOCITY = 4.5f;      // Jump out of water

constexpr float FLY_SPEED = 24.0f;
constexpr float FLY_VERTICAL_SPEED = 16.0f;

// Bobbing effect
static constexpr float BOBBING_FREQUENCY = 18.0f;
static constexpr float BOBBING_AMPLITUDE = 0.2f;
static constexpr float BOBBING_HORIZONTAL_AMPLITUDE = 0.05f;
static constexpr float BOBBING_DAMPING_SPEED = 4.0f;

// Perlin Terrain Generation
constexpr unsigned int PERLIN_SEED = 42;
constexpr float TERRAIN_HEIGHT_SCALE = 15.0f;

// Water Generation Constants
constexpr int WATER_LEVEL = 22;

// Ore Generation Constants
constexpr int COAL_ORE_MIN_Y = 5;
constexpr int COAL_ORE_MAX_Y = 50;
constexpr float COAL_ORE_CHANCE = 0.02f;

constexpr int IRON_ORE_MIN_Y = 5;
constexpr int IRON_ORE_MAX_Y = 40;
constexpr float IRON_ORE_CHANCE = 0.015f;

// Texture Atlas
constexpr int ATLAS_TILE_SIZE = 16;
constexpr int ATLAS_WIDTH = 128;
constexpr int ATLAS_HEIGHT = 128;
constexpr int ATLAS_TILES_WIDTH = ATLAS_WIDTH / ATLAS_TILE_SIZE;  // 8 tiles wide
constexpr int ATLAS_TILES_HEIGHT = ATLAS_HEIGHT / ATLAS_TILE_SIZE; // 8 tiles tall

constexpr float AO_STRENGTH = 0.5f;

// Water Animation
constexpr float WATER_ANIMATION_SPEED = 2.0f; // Frames per second

#endif // SHARED_CONFIG_HPP
