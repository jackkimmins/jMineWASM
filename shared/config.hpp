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
constexpr float PLAYER_SPEED = 4.5f;
constexpr float SPRINT_SPEED = 6.5f;
constexpr float DOUBLE_TAP_TIME = 0.3f;
constexpr float SENSITIVITY = 0.18f;
constexpr float CAM_FOV = 80.0f;
constexpr float epsilon = 0.001f;
constexpr float PLAYER_HEIGHT = 1.8f;

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

// Texture Atlas and Ambient Occlusion
// Atlas is 128x128 pixels, containing 16x16 pixel tiles
// This gives us 8x8 = 64 possible tile slots
constexpr int ATLAS_TILE_SIZE = 16;
constexpr int ATLAS_WIDTH = 128;
constexpr int ATLAS_HEIGHT = 128;
constexpr int ATLAS_TILES_WIDTH = ATLAS_WIDTH / ATLAS_TILE_SIZE;  // 8 tiles wide
constexpr int ATLAS_TILES_HEIGHT = ATLAS_HEIGHT / ATLAS_TILE_SIZE; // 8 tiles tall
constexpr float AO_STRENGTH = 0.5f;

// Water Animation
constexpr float WATER_ANIMATION_SPEED = 2.0f; // Frames per second

#endif // SHARED_CONFIG_HPP
