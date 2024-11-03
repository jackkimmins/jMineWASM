// blocks_chunks_worlds.hpp
#ifndef BLOCKS_CHUNKS_WORLDS_HPP
#define BLOCKS_CHUNKS_WORLDS_HPP

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

#endif