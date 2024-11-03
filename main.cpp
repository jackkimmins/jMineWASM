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

#include "perlin_noise.hpp"
#include "shaders.hpp"
#include "camera.hpp"

// Player Class
class Player {
public:
    float x, y, z;
    float velocityY = 0.0f;
    bool onGround = false;

    Player(float startX, float startY, float startZ) : x(startX), y(startY), z(startZ) {}
};

// BlockType Enum
enum BlockType {
    BLOCK_STONE,
    BLOCK_DIRT,
    BLOCK_PLANKS,
    BLOCK_GRASS,
    BLOCK_BEDROCK,
    BLOCK_COAL_ORE,
    BLOCK_IRON_ORE,
};

// FaceDirection Enum
enum FaceDirection {
    FACE_FRONT = 0,
    FACE_BACK = 1,
    FACE_RIGHT = 2,
    FACE_LEFT = 3,
    FACE_TOP = 4,
    FACE_BOTTOM = 5
};

// Block Structure
struct Block {
    bool isSolid = false;
    BlockType type = BLOCK_STONE;
};

// Chunks currently just contain a 3D array of blocks, might be expanded in the future to include things like biomes 😇
class Chunk {
    public: Block blocks[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE]; // [x][y][z]
};

// Modified World Class to include Y dimension
class World {
private:
    PerlinNoise perlin;
public:
    Chunk chunks[WORLD_CHUNK_SIZE_X][WORLD_CHUNK_SIZE_Y][WORLD_CHUNK_SIZE_Z]; // [cx][cy][cz]

    void initialise() {
        perlin = PerlinNoise(PERLIN_SEED);

        generateTerrain();
        generateOres();
        generateCaves();
        updateSurfaceBlocks();
    }

    void generateTerrain() {
        for (int cx = 0; cx < WORLD_CHUNK_SIZE_X; ++cx) {
            for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                for (int cz = 0; cz < WORLD_CHUNK_SIZE_Z; ++cz) {
                    for (int x = 0; x < CHUNK_SIZE; ++x) {
                        for (int z = 0; z < CHUNK_SIZE; ++z) {
                            int worldX = cx * CHUNK_SIZE + x;
                            int worldZ = cz * CHUNK_SIZE + z;
                            int maxHeight = getHeightAt(worldX, worldZ);

                            for (int y = 0; y <= maxHeight && y < WORLD_SIZE_Y; ++y) {
                                int by = y % CHUNK_HEIGHT;
                                int cy = y / CHUNK_HEIGHT;

                                chunks[cx][cy][cz].blocks[x][by][z].isSolid = true;

                                // Assign textures based on height
                                if (y == maxHeight) {
                                    chunks[cx][cy][cz].blocks[x][by][z].type = BLOCK_GRASS;
                                } else if (y >= maxHeight - 3) {
                                    chunks[cx][cy][cz].blocks[x][by][z].type = BLOCK_DIRT;
                                } else if (y == 0) {
                                    chunks[cx][cy][cz].blocks[x][by][z].type = BLOCK_BEDROCK;
                                } else {
                                    chunks[cx][cy][cz].blocks[x][by][z].type = BLOCK_STONE;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void generateOres() {
        std::mt19937 rng(PERLIN_SEED);
        std::uniform_real_distribution<float> oreChanceDist(0.0f, 1.0f);

        for (int cx = 0; cx < WORLD_CHUNK_SIZE_X; ++cx) {
            for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                for (int cz = 0; cz < WORLD_CHUNK_SIZE_Z; ++cz) {
                    for (int x = 0; x < CHUNK_SIZE; ++x) {
                        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                            for (int z = 0; z < CHUNK_SIZE; ++z) {
                                int worldY = cy * CHUNK_HEIGHT + y;

                                // Get the block reference
                                Block& block = chunks[cx][cy][cz].blocks[x][y][z];

                                // Only consider stone blocks
                                if (block.isSolid && block.type == BLOCK_STONE) {
                                    // Coal Ore Generation
                                    if (worldY >= COAL_ORE_MIN_Y && worldY <= COAL_ORE_MAX_Y) {
                                        if (oreChanceDist(rng) < COAL_ORE_CHANCE) block.type = BLOCK_COAL_ORE;
                                    }

                                    // Iron Ore Generation
                                    if (worldY >= IRON_ORE_MIN_Y && worldY <= IRON_ORE_MAX_Y) {
                                        if (oreChanceDist(rng) < IRON_ORE_CHANCE) block.type = BLOCK_IRON_ORE;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void updateSurfaceBlocks() {
        for (int x = 0; x < WORLD_SIZE_X; ++x) {
            for (int z = 0; z < WORLD_SIZE_Z; ++z) {
                for (int y = WORLD_SIZE_Y - 1; y >= 0; --y) {
                    if (isSolidAt(x, y, z)) {
                        int cx = x / CHUNK_SIZE;
                        int cy = y / CHUNK_HEIGHT;
                        int cz = z / CHUNK_SIZE;
                        int bx = x % CHUNK_SIZE;
                        int by = y % CHUNK_HEIGHT;
                        int bz = z % CHUNK_SIZE;

                        Block& block = chunks[cx][cy][cz].blocks[bx][by][bz];
                        if (block.type == BLOCK_DIRT) block.type = BLOCK_GRASS;
                        break;
                    }
                }
            }
        }
    }


    void generateCaves() {
        std::mt19937 rng(PERLIN_SEED);
        std::uniform_real_distribution<float> distX(0, WORLD_SIZE_X);
        std::uniform_real_distribution<float> distY(CAVE_END_DEPTH, WORLD_SIZE_Y - CAVE_START_DEPTH);
        std::uniform_real_distribution<float> distZ(0, WORLD_SIZE_Z);
        std::uniform_real_distribution<float> angleDist(-CAVE_DIRECTION_CHANGE, CAVE_DIRECTION_CHANGE);
        std::uniform_real_distribution<float> radiusDist(CAVE_RADIUS_MIN, CAVE_RADIUS_MAX);

        for (int i = 0; i < NUM_CAVES; ++i) {
            // Starting position
            float x = distX(rng);
            float y = distY(rng);
            float z = distZ(rng);

            // Initial direction (random unit vector)
            float theta = distX(rng) * 2.0f * M_PI / WORLD_SIZE_X;
            float phi = distY(rng) * M_PI / WORLD_SIZE_Y;
            float dirX = sin(phi) * cos(theta);
            float dirY = cos(phi);
            float dirZ = sin(phi) * sin(theta);

            // Random starting and ending radius for the cave
            float startRadius = radiusDist(rng);
            float endRadius = radiusDist(rng);

            // Carve the cave
            for (int j = 0; j < CAVE_LENGTH; ++j) {
                // Interpolate radius along the length of the cave
                float t = static_cast<float>(j) / CAVE_LENGTH;
                float radius = startRadius + t * (endRadius - startRadius);

                carveSphere(x, y, z, radius);

                // Slightly change direction
                dirX += angleDist(rng);
                dirY += angleDist(rng);
                dirZ += angleDist(rng);

                // Normalise direction
                float dirLength = sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
                dirX /= dirLength;
                dirY /= dirLength;
                dirZ /= dirLength;

                // Move to next position
                x += dirX;
                y += dirY;
                z += dirZ;

                // Ensure we stay within world bounds
                if (x < 0 || x >= WORLD_SIZE_X || y < CAVE_END_DEPTH || y >= WORLD_SIZE_Y - CAVE_START_DEPTH || z < 0 || z >= WORLD_SIZE_Z) break;
            }
        }
    }

    void carveSphere(float centerX, float centerY, float centerZ, float radius) {
        int minX = static_cast<int>(std::floor(centerX - radius));
        int maxX = static_cast<int>(std::ceil(centerX + radius));
        int minY = static_cast<int>(std::floor(centerY - radius));
        int maxY = static_cast<int>(std::ceil(centerY + radius));
        int minZ = static_cast<int>(std::floor(centerZ - radius));
        int maxZ = static_cast<int>(std::ceil(centerZ + radius));

        for (int x = minX; x <= maxX; ++x) {
            for (int y = minY; y <= maxY; ++y) {
                for (int z = minZ; z <= maxZ; ++z) {
                    // Check bounds
                    if (x < 0 || x >= WORLD_SIZE_X || y < 0 || y >= WORLD_SIZE_Y || z < 0 || z >= WORLD_SIZE_Z) continue;

                    float dx = x + 0.5f - centerX;
                    float dy = y + 0.5f - centerY;
                    float dz = z + 0.5f - centerZ;
                    float distanceSquared = dx * dx + dy * dy + dz * dz;

                    if (distanceSquared <= radius * radius) {
                        // Carve out the block
                        int cx = x / CHUNK_SIZE;
                        int cy = y / CHUNK_HEIGHT;
                        int cz = z / CHUNK_SIZE;
                        int bx = x % CHUNK_SIZE;
                        int by = y % CHUNK_HEIGHT;
                        int bz = z % CHUNK_SIZE;

                        chunks[cx][cy][cz].blocks[bx][by][bz].isSolid = false;
                    }
                }
            }
        }
    }

    int getHeightAt(int x, int z) {
        double noiseHeight = 0.0;
        double amplitude = 1.0;
        double frequency = PERLIN_FREQUENCY;
        double maxAmplitude = 0.0;

        // I am using multiple octaves of Perlin noise to create more detailed terrain
        for (int i = 0; i < PERLIN_OCTAVES; ++i) {
            noiseHeight += perlin.noise(x * frequency, z * frequency) * amplitude;
            maxAmplitude += amplitude;

            amplitude *= PERLIN_PERSISTENCE;
            frequency *= PERLIN_LACUNARITY;
        }

        // Normalise the noiseHeight and scale it to match the desired terrain height
        noiseHeight = (noiseHeight / maxAmplitude) * TERRAIN_HEIGHT_SCALE;

        // Offset to ensure heights remain positive
        int height = static_cast<int>(noiseHeight + 20.0);
        if (height >= WORLD_SIZE_Y) height = WORLD_SIZE_Y - 1;
        else if (height < 0) height = 0;

        return height;
    }

    bool isSolidAt(int x, int y, int z) const {
        if (x >= 0 && x < WORLD_SIZE_X && y >= 0 && y < WORLD_SIZE_Y && z >= 0 && z < WORLD_SIZE_Z) {
            int cx = x / CHUNK_SIZE;
            int cy = y / CHUNK_HEIGHT;
            int cz = z / CHUNK_SIZE;
            int blockX = x % CHUNK_SIZE;
            int blockY = y % CHUNK_HEIGHT;
            int blockZ = z % CHUNK_SIZE;

            return chunks[cx][cy][cz].blocks[blockX][blockY][blockZ].isSolid;
        } else {
            return false;
        }
    }
};

// Mesh Class
class Mesh {
public:
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    GLuint VAO, VBO, EBO;

    Mesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
    }

    void generate(const World& world) {
        worldPtr = &world;
        unsigned int currentIndex = 0;
        for (int cx = 0; cx < WORLD_CHUNK_SIZE_X; ++cx) {
            for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                for (int cz = 0; cz < WORLD_CHUNK_SIZE_Z; ++cz) {
                    for (int x = 0; x < CHUNK_SIZE; ++x) {
                        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                            for (int z = 0; z < CHUNK_SIZE; ++z) {
                                const Block& block = world.chunks[cx][cy][cz].blocks[x][y][z];
                                if (!block.isSolid) continue;

                                float worldX = (cx * CHUNK_SIZE) + x;
                                float worldY = (cy * CHUNK_HEIGHT) + y;
                                float worldZ = (cz * CHUNK_SIZE) + z;

                                if (!isSolid(world, cx, cy, cz, x + 1, y, z)) addFace(worldX, worldY, worldZ, currentIndex, FACE_RIGHT, block.type);
                                if (!isSolid(world, cx, cy, cz, x - 1, y, z)) addFace(worldX, worldY, worldZ, currentIndex, FACE_LEFT, block.type);
                                if (!isSolid(world, cx, cy, cz, x, y + 1, z)) addFace(worldX, worldY, worldZ, currentIndex, FACE_TOP, block.type);
                                if (!isSolid(world, cx, cy, cz, x, y - 1, z) && !(block.type == BLOCK_BEDROCK && y == 0)) addFace(worldX, worldY, worldZ, currentIndex, FACE_BOTTOM, block.type);
                                if (!isSolid(world, cx, cy, cz, x, y, z + 1)) addFace(worldX, worldY, worldZ, currentIndex, FACE_FRONT, block.type);
                                if (!isSolid(world, cx, cy, cz, x, y, z - 1)) addFace(worldX, worldY, worldZ, currentIndex, FACE_BACK, block.type);
                            }
                        }
                    }
                }
            }
        }
    }

    // Helper function to check if a neighboring block is solid
    bool isSolid(const World& world, int cx, int cy, int cz, int x, int y, int z) {
        if (x < 0) {
            if (cx == 0) return false;
            x += CHUNK_SIZE;
            cx -= 1;
        } else if (x >= CHUNK_SIZE) {
            if (cx == WORLD_CHUNK_SIZE_X - 1) return false;
            x -= CHUNK_SIZE;
            cx += 1;
        }
        if (y < 0) {
            if (cy == 0) return false;
            y += CHUNK_HEIGHT;
            cy -= 1;
        } else if (y >= CHUNK_HEIGHT) {
            if (cy == WORLD_CHUNK_SIZE_Y - 1) return false;
            y -= CHUNK_HEIGHT;
            cy += 1;
        }
        if (z < 0) {
            if (cz == 0) return false;
            z += CHUNK_SIZE;
            cz -= 1;
        } else if (z >= CHUNK_SIZE) {
            if (cz == WORLD_CHUNK_SIZE_Z - 1) return false;
            z -= CHUNK_SIZE;
            cz += 1;
        }
        return world.chunks[cx][cy][cz].blocks[x][y][z].isSolid;
    }

    int getTextureIndex(BlockType blockType, FaceDirection face) {
        switch (blockType) {
            case BLOCK_GRASS:
                switch (face) {
                    case FACE_TOP:
                        return 3;
                    case FACE_BOTTOM:
                        return 1;
                    default:
                        return 4;
                }
            case BLOCK_STONE:
                return 0;
            case BLOCK_DIRT:
                return 1;
            case BLOCK_PLANKS:
                return 2;
            case BLOCK_BEDROCK:
                return 5;
            case BLOCK_COAL_ORE:
                return 6;
            case BLOCK_IRON_ORE:
                return 7;
            default:
                // Future Me Problem: Make a custom missing/invalid texture
                return 0;
        }
    }

    void addFace(float x, float y, float z, unsigned int& indexOffset, FaceDirection face, BlockType blockType) {
        int faceIndex = static_cast<int>(face);
        int textureIndex = getTextureIndex(blockType, face);

        // Compute texture coordinates based on textureIndex
        int tileX = textureIndex % ATLAS_TILES_WIDTH;
        int tileY = textureIndex / ATLAS_TILES_WIDTH;

        float u0 = (tileX * ATLAS_TILE_SIZE) / static_cast<float>(ATLAS_TILES_WIDTH);
        float v0 = (tileY * ATLAS_TILE_SIZE) / static_cast<float>(ATLAS_TILES_HEIGHT);
        float u1 = ((tileX + 1) * ATLAS_TILE_SIZE) / static_cast<float>(ATLAS_TILES_WIDTH);
        float v1 = ((tileY + 1) * ATLAS_TILE_SIZE) / static_cast<float>(ATLAS_TILES_HEIGHT);

        // Texture coordinates for the face
        float texCoords[4][2] = {
            {u0, v1}, // bottom-left
            {u1, v1}, // bottom-right
            {u1, v0}, // top-right
            {u0, v0}  // top-left
        };

        // Ambient Occlusion values for the four vertices
        float aoValues[4];

        // Calculate AO values for each vertex
        for (int i = 0; i < 4; ++i) {
            // Determine the positions of the corner and side blocks
            int dx = static_cast<int>(faceVertices[faceIndex][i][0]);
            int dy = static_cast<int>(faceVertices[faceIndex][i][1]);
            int dz = static_cast<int>(faceVertices[faceIndex][i][2]);

            // Positions of adjacent blocks
            int px = static_cast<int>(x + dx);
            int py = static_cast<int>(y + dy);
            int pz = static_cast<int>(z + dz);

            // Side blocks
            bool side1 = isSolidAt(px + faceNormals[faceIndex][0], py + faceNormals[faceIndex][1], pz + faceNormals[faceIndex][2]);
            bool side2 = isSolidAt(px + faceTangents[faceIndex][0], py + faceTangents[faceIndex][1], pz + faceTangents[faceIndex][2]);

            // Corner block
            bool corner = isSolidAt(px + faceNormals[faceIndex][0] + faceTangents[faceIndex][0],
                                    py + faceNormals[faceIndex][1] + faceTangents[faceIndex][1],
                                    pz + faceNormals[faceIndex][2] + faceTangents[faceIndex][2]);

            // Calculate AO based on neighboring blocks
            aoValues[i] = calculateAO(side1, side2, corner);
        }

        // Add vertices with positions, texture coordinates, and AO values
        for (int i = 0; i < 4; ++i) {
            vertices.push_back(faceVertices[faceIndex][i][0] + x);
            vertices.push_back(faceVertices[faceIndex][i][1] + y);
            vertices.push_back(faceVertices[faceIndex][i][2] + z);
            vertices.push_back(texCoords[i][0]);
            vertices.push_back(texCoords[i][1]);
            vertices.push_back(aoValues[i]);
        }

        // Add indices for the face
        for (int i = 0; i < 6; ++i) indices.push_back(indexOffset + faceIndices[faceIndex][i]);
        indexOffset += 4;
    }

    void setup() {
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // Position attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

        // Texture coordinate attribute
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

        // Ambient Occlusion attribute
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));

        glBindVertexArray(0);
    }

    void updateBuffer() {
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
    }

    void draw() const {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    ~Mesh() {
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glDeleteVertexArrays(1, &VAO);
    }

private:
    // Face vertices and indices
    static const float faceVertices[6][4][3];
    static const unsigned int faceIndices[6][6];

    // Face normals and tangents for AO calculations
    static const int faceNormals[6][3];
    static const int faceTangents[6][3];

    // Calculate AO value based on neighboring blocks
    float calculateAO(bool side1, bool side2, bool corner) {
        if (side1 && side2) return AO_STRENGTH;

        int occlusion = side1 + side2 + corner;
        switch (occlusion) {
            case 0: return 0.0f * AO_STRENGTH;
            case 1: return 0.4f * AO_STRENGTH;
            case 2: return 0.6f * AO_STRENGTH;
            case 3: return 0.7f * AO_STRENGTH;
            default: return 0.0f;
        }
    }

    // Check if a block is solid at world coordinates
    bool isSolidAt(int x, int y, int z) const { return worldPtr->isSolidAt(x, y, z); }
    const World* worldPtr; // Pointer to world data
};

// Update the faceVertices array in the Mesh class
const float Mesh::faceVertices[6][4][3] = {
    // Front face
    {{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}},
    // Back face
    {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}},
    // Right face
    {{1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    // Left face
    {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    // Top face
    {{0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    // Bottom face
    {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
};

const unsigned int Mesh::faceIndices[6][6] = {
    {0, 1, 2, 2, 3, 0},  // Front
    {0, 1, 2, 2, 3, 0},  // Back
    {0, 1, 2, 2, 3, 0},  // Right
    {0, 1, 2, 2, 3, 0},  // Left
    {0, 1, 2, 2, 3, 0},  // Top
    {0, 1, 2, 2, 3, 0}   // Bottom
};

// Face normals and tangents for AO calculations
const int Mesh::faceNormals[6][3] = {
    { 0,  0,  1}, // Front
    { 0,  0, -1}, // Back
    { 1,  0,  0}, // Right
    {-1,  0,  0}, // Left
    { 0,  1,  0}, // Top
    { 0, -1,  0}  // Bottom
};

const int Mesh::faceTangents[6][3] = {
    { 1,  0,  0}, // Front
    {-1,  0,  0}, // Back
    { 0,  0, -1}, // Right
    { 0,  0,  1}, // Left
    { 1,  0,  0}, // Top
    { 1,  0,  0}  // Bottom
};

// Game Class
class Game {
public:
    bool pointerLocked = false;
    Shader* shader;
    Mesh mesh;
    World world;
    Camera camera;
    Player player;
    mat4 projection;
    GLint mvpLoc;
    std::chrono::steady_clock::time_point lastFrame;
    bool keys[1024] = { false };
    GLuint textureAtlas;

    Game() : shader(nullptr), player(SPAWN_X, SPAWN_Y, SPAWN_Z) { std::cout << "Game Constructed - Player Spawn: (" << SPAWN_X << ", " << SPAWN_Y << ", " << SPAWN_Z << ")" << std::endl; }

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
            out vec4 FragColor;
            void main() {
                vec4 texColor = texture(uTexture, TexCoord);
                texColor.rgb *= 1.0 - AO; // Apply AO to darken the color
                FragColor = texColor;
            })";

        // Compile and link shaders
        shader = new Shader(vertexSrc, fragmentSrc);
        shader->use();
        mvpLoc = shader->getUniform("uMVP");

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

        // Initialise and generate the world
        world.initialise();

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

        // Get initial canvas size
        int canvasWidth, canvasHeight;
        emscripten_get_canvas_element_size("canvas", &canvasWidth, &canvasHeight);

        // Initialise projection matrix with dynamic aspect ratio
        projection = perspective(CAM_FOV * M_PI / 180.0f, static_cast<float>(canvasWidth) / static_cast<float>(canvasHeight), 0.1f, 1000.0f);

        // Generate the mesh based on the world data
        mesh.generate(world);

        // Setup mesh buffers
        mesh.setup();

        // Enable depth testing and face culling
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        lastFrame = std::chrono::steady_clock::now();
    }

    void mainLoop() {
        deltaTime = calculateDeltaTime();
        processInput(deltaTime);
        applyPhysics(deltaTime);
        if (isMoving) bobbingTime += deltaTime;
        render();
    }

    void handleKey(int keyCode, bool pressed) {
        keys[keyCode] = pressed;

        // Check for space key to jump
        if (keyCode == 32 && pressed && player.onGround) {
            player.velocityY = JUMP_VELOCITY;
            player.onGround = false;
        }
    }

    void handleMouseMove(float movementX, float movementY) {
        if (!pointerLocked) return;
        camera.yaw += movementX * SENSITIVITY;
        camera.pitch = std::clamp(camera.pitch - movementY * SENSITIVITY, -89.0f, 89.0f);
    }

    void handleMouseClick(int button) {
        float maxDistance = 4.0f;
        RaycastHit hit = raycast(maxDistance);
        if (hit.hit) {
            if (button == 0) removeBlock(hit.blockPosition.x, hit.blockPosition.y, hit.blockPosition.z);
            else if (button == 2) placeBlock(hit.adjacentPosition.x, hit.adjacentPosition.y, hit.adjacentPosition.z);

            if (button == 0 || button == 2) {
                // Regen the mesh
                mesh.vertices.clear();
                mesh.indices.clear();
                mesh.generate(world);
                mesh.setup();
            }
        }
    }

private:
    float bobbingTime = 0.0f;
    float bobbingOffset = 0.0f;
    float bobbingHorizontalOffset = 0.0f;
    bool isMoving = false;
    float deltaTime = 0.0f;

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
        float velocity = PLAYER_SPEED * dt;
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
        if (x >= 0 && x < WORLD_SIZE_X && y >= 0 && y < WORLD_SIZE_Y && z >= 0 && z < WORLD_SIZE_Z) {
            int cx = x / CHUNK_SIZE;
            int cy = y / CHUNK_HEIGHT;
            int cz = z / CHUNK_SIZE;
            int blockX = x % CHUNK_SIZE;
            int blockY = y % CHUNK_HEIGHT;
            int blockZ = z % CHUNK_SIZE;

            if (world.chunks[cx][cy][cz].blocks[blockX][blockY][blockZ].type == BLOCK_BEDROCK) return;

            world.chunks[cx][cy][cz].blocks[blockX][blockY][blockZ].isSolid = false;
        }
    }

    void placeBlock(int x, int y, int z) {
        if (x >= 0 && x < WORLD_SIZE_X && y >= 0 && y < WORLD_SIZE_Y && z >= 0 && z < WORLD_SIZE_Z) {
            int cx = x / CHUNK_SIZE;
            int cy = y / CHUNK_HEIGHT;
            int cz = z / CHUNK_SIZE;
            int blockX = x % CHUNK_SIZE;
            int blockY = y % CHUNK_HEIGHT;
            int blockZ = z % CHUNK_SIZE;

            // Prevent placing a block inside the player
            if (!isColliding(x + 0.5f, y + 0.5f, z + 0.5f)) {
                world.chunks[cx][cy][cz].blocks[blockX][blockY][blockZ].isSolid = true;
                world.chunks[cx][cy][cz].blocks[blockX][blockY][blockZ].type = BLOCK_PLANKS; // Set to desired block type
            }
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
        Vector3 rayDirection = camera.getFrontVector();

        // Normalise the direction
        float dirLength = sqrt(rayDirection.x * rayDirection.x + rayDirection.y * rayDirection.y + rayDirection.z * rayDirection.z);
        rayDirection.x /= dirLength;
        rayDirection.y /= dirLength;
        rayDirection.z /= dirLength;

        // Current block position
        int x = static_cast<int>(floor(rayOrigin.x));
        int y = static_cast<int>(floor(rayOrigin.y));
        int z = static_cast<int>(floor(rayOrigin.z));

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

                // For adjacent position, need to know which face we entered from
                if (tMaxX < tMaxY && tMaxX < tMaxZ) hitResult.adjacentPosition = { x - stepX, y, z };
                else if (tMaxY < tMaxZ) hitResult.adjacentPosition = { x, y - stepY, z };
                else hitResult.adjacentPosition = { x, y, z - stepZ };
                return hitResult;
            }

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

        // Draw the mesh
        mesh.draw();
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