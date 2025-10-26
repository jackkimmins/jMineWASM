#ifndef BLOCKS_CHUNKS_WORLDS_HPP
#define BLOCKS_CHUNKS_WORLDS_HPP

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <vector>
#include <random>

// BlockType Enum
enum BlockType {
    BLOCK_STONE,
    BLOCK_DIRT,
    BLOCK_PLANKS,
    BLOCK_GRASS,
    BLOCK_BEDROCK,
    BLOCK_COAL_ORE,
    BLOCK_IRON_ORE,
    BLOCK_LOG,
    BLOCK_LEAVES,
    BLOCK_WATER,
    BLOCK_SAND
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

struct Block {
    bool isSolid = false;
    BlockType type = BLOCK_STONE;
};

struct ChunkCoord {
    int x, y, z;
    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

namespace std {
    template <>
    struct hash<ChunkCoord> {
        size_t operator()(const ChunkCoord& c) const {
            return ((hash<int>()(c.x) ^ (hash<int>()(c.y) << 1)) >> 1) ^ (hash<int>()(c.z) << 1);
        }
    };
}

class Chunk {
public:
    Block blocks[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE]; // [x][y][z]
    bool isGenerated = false;
    bool isDirty = false;
    bool isFullyProcessed = false; // has ores and surface processed
    ChunkCoord coord;
    Chunk() = default;
    Chunk(int cx, int cy, int cz) : coord{cx, cy, cz} {}
};

class World {
private:
    PerlinNoise perlin;
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>> chunks;
    std::unordered_set<ChunkCoord> loadedChunks;
    
public:
    void initialise() {
        perlin = PerlinNoise(PERLIN_SEED);
    }
    
    // Check if chunk indices are within world bounds
    bool isChunkInBounds(int cx, int cy, int cz) const {
        return cx >= 0 && cx < WORLD_CHUNK_SIZE_X &&
               cy >= 0 && cy < WORLD_CHUNK_SIZE_Y &&
               cz >= 0 && cz < WORLD_CHUNK_SIZE_Z;
    }
    
    // Get or create a chunk at given chunk coordinates
    Chunk* getChunk(int cx, int cy, int cz) {
        if (!isChunkInBounds(cx, cy, cz)) return nullptr;
        ChunkCoord coord{cx, cy, cz};
        auto it = chunks.find(coord);
        if (it == chunks.end()) {
            // Create new chunk if not exists
            auto chunk = std::make_unique<Chunk>(cx, cy, cz);
            Chunk* ptr = chunk.get();
            chunks[coord] = std::move(chunk);
            return ptr;
        }
        return it->second.get();
    }
    
    // Check if chunk is already created in memory (exists in chunks map)
    bool isChunkLoaded(int cx, int cy, int cz) const {
        ChunkCoord coord{cx, cy, cz};
        return chunks.find(coord) != chunks.end();
    }
    
int getHeightAt(int x, int z) const {
        // Keep broad features
        static const double WARP_FREQ = 0.0020;
        static const double WARP_AMPLITUDE = 80.0;

        double warpNoise1 = perlin.noise(x * WARP_FREQ, z * WARP_FREQ);
        double warpNoise2 = perlin.noise(x * WARP_FREQ + 1000.0, z * WARP_FREQ + 1000.0);
        double warpX = warpNoise1 * WARP_AMPLITUDE;
        double warpZ = warpNoise2 * WARP_AMPLITUDE;

        double warpedX = x + warpX;
        double warpedZ = z + warpZ;

        // Domain rotation
        static const double g = 0.5773502691896258;
        static const double s = -0.2113248654051871;
        auto sampleNoise3D = [&](double freq) {
            double X = warpedX * freq;
            double Z = warpedZ * freq;
            double xr = X * (1.0 + s) + g * Z;
            double yr = s * X + g * Z;
            double zr = g * (Z - X);
            return perlin.noise(xr, yr, zr);
        };

        auto fbmNoise = [&](double baseFreq, int octaves) {
            double total = 0.0, amplitude = 1.0, maxAmp = 0.0, freq = baseFreq;
            for (int i = 0; i < octaves; ++i) {
                double n = sampleNoise3D(freq) * 0.5 + 0.5; // [0,1]
                total += n * amplitude;
                maxAmp += amplitude;
                amplitude *= 0.5;
                freq *= 2.0;
            }
            return total / maxAmp;
        };

        // Large-scale shapes
        constexpr double HILL_BAND_SCALE = 1.0;
        const double continental = fbmNoise(0.005 * HILL_BAND_SCALE, 3);
        const double hills = fbmNoise(0.012 * HILL_BAND_SCALE, 5);
        const double detail = fbmNoise(0.02,  2);

        // Base factor
        double heightFactor = 0.55 * hills + 0.40 * continental + 0.05 * detail;

        // Remove hard terracing
        auto smoothstep = [](double a, double b, double x) {
            double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
            return t * t * (3.0 - 2.0 * t);
        };

        auto softTerrace = [&](double v, double steps, double softness) {
            // Map v in [0,1] onto gentle steps without plateaus/edges
            double u = v * steps;
            double f = u - std::floor(u);
            double u2 = std::floor(u) + smoothstep(0.3, 0.7, f);
            return std::lerp(v, u2 / steps, softness);
        };

        heightFactor = softTerrace(heightFactor, 12.0, 0.10);

        double valleyStretch = 1.5;
        heightFactor = std::clamp(std::pow(heightFactor, valleyStretch), 0.0, 1.0);

        double mountainMask = smoothstep(0.58, 0.82, 0.6 * hills + 0.4 * continental);
        double verticalScaleMul = std::lerp(4.0, 18.0, mountainMask);

        double scaledHeight = heightFactor * (TERRAIN_HEIGHT_SCALE * verticalScaleMul) + 10.0;

        int height = static_cast<int>(scaledHeight);
        if (height >= WORLD_SIZE_Y) height = WORLD_SIZE_Y - 1;
        if (height < 5) height = 5;
        return height;
    }

    
    // Generate a single chunk's base terrain
    void generateChunk(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || chunk->isGenerated) return;
        
        // Chunk world-coordinate origin
        int baseX = cx * CHUNK_SIZE;
        int baseY = cy * CHUNK_HEIGHT;
        int baseZ = cz * CHUNK_SIZE;
        
        // Loop through every position in this chunk column section
        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                int worldX = baseX + lx;
                int worldZ = baseZ + lz;

                // Determine base terrain height for this column
                int columnHeight = getHeightAt(worldX, worldZ);
                
                for (int ly = 0; ly < CHUNK_HEIGHT; ++ly) {
                    int worldY = baseY + ly;
                    Block& block = chunk->blocks[lx][ly][lz];

                    // Bedrock at bottom of the world
                    if (worldY == 0) {
                        block.isSolid = true;
                        block.type = BLOCK_BEDROCK;
                        continue;
                    }
                    // Solid terrain up to columnHeight, air above
                    if (worldY <= columnHeight && worldY < WORLD_SIZE_Y) {
                        block.isSolid = true;
                        if (worldY >= columnHeight - 3) {
                            block.type = BLOCK_DIRT;
                        } else {
                            block.type = BLOCK_STONE;
                        }
                    } else {
                        block.isSolid = false;
                    }
                }
            }
        }
        
        chunk->isGenerated = true;
        chunk->isDirty = true;
    }

    void generateCaves(int cx, int cy, int cz) {
        if (!isChunkInBounds(cx, cy, cz)) return;
        Chunk* baseChunk = getChunk(cx, cy, cz);
        if (!baseChunk || !baseChunk->isGenerated || baseChunk->isFullyProcessed) return;

        // Random generator for this chunk column
        unsigned int caveSeed = static_cast<unsigned int>(PERLIN_SEED) ^ (cx * 0xA511E9B3u) ^ (cz * 0x63288691u);
        std::mt19937 caveRng(caveSeed);
        std::uniform_real_distribution<double> rand01(0.0, 1.0);

        // Decide how many caves to carve in this column
        int caveCount;
        double chance = rand01(caveRng);
        if (chance < 0.1) caveCount = 0;
        else if (chance < 0.6) caveCount = 1;
        else if (chance < 0.9) caveCount = 2;
        else caveCount = 3;
        if (caveCount <= 0) return;

        // Get surface height for a given (wx, wz)
        auto getSurfaceY = [&](int wx, int wz) { return getHeightAt(wx, wz); };

        // Lambda to recursively carve a cave tunnel
        std::function<void(double,double,double,double,double,double,int,bool)> carveTunnel = [&](double x, double y, double z, double yaw, double pitch, double radius, int steps, bool allowBranch) {
            const double stepSize = 0.5;
            for (int i = 0; i < steps; ++i) {
                // Move along the current direction vector
                x += std::cos(yaw) * std::cos(pitch) * stepSize;
                y += std::sin(pitch) * stepSize;
                z += std::sin(yaw) * std::cos(pitch) * stepSize;

                // Stop if out of world bounds
                if (x < 0 || x >= WORLD_SIZE_X || y < 0 || y >= WORLD_SIZE_Y || z < 0 || z >= WORLD_SIZE_Z) break;

                // Compute carving region
                int minX = static_cast<int>(std::floor(x - radius));
                int maxX = static_cast<int>(std::floor(x + radius));
                int minY = static_cast<int>(std::floor(y - radius));
                int maxY = static_cast<int>(std::floor(y + radius));
                int minZ = static_cast<int>(std::floor(z - radius));
                int maxZ = static_cast<int>(std::floor(z + radius));
                double radiusSq = radius * radius;

                // Remove blocks within the sphere
                for (int xi = minX; xi <= maxX; ++xi) {
                    for (int yi = minY; yi <= maxY; ++yi) {
                        for (int zi = minZ; zi <= maxZ; ++zi) {
                            if (xi < 0 || xi >= WORLD_SIZE_X || yi < 0 || yi >= WORLD_SIZE_Y || zi < 0 || zi >= WORLD_SIZE_Z) continue;

                            // Check distance from tunnel center
                            double dx = xi + 0.5 - x;
                            double dy = yi + 0.5 - y;
                            double dz = zi + 0.5 - z;
                            if ((dx*dx + dy*dy + dz*dz) > radiusSq) continue;

                            // Only carve if the chunk containing this block is generated (active in memory)
                            int cx2 = xi / CHUNK_SIZE;
                            int cy2 = yi / CHUNK_HEIGHT;
                            int cz2 = zi / CHUNK_SIZE;
                            if (!isChunkLoaded(cx2, cy2, cz2)) continue;
                            Block* block = getBlockAt(xi, yi, zi);
                            if (!block) continue;

                            if (block->isSolid) {
                                block->isSolid = false;
                                markChunkDirty(cx2, cy2, cz2);
                            }
                        }
                    }
                }

                // Possibly create one branch at halfway point
                if (allowBranch && i == steps / 2) {
                    if (rand01(caveRng) < 0.2) {
                        double branchYaw = yaw + ((rand01(caveRng) < 0.5) ? 1 : -1) * (0.78539816339 + 0.78539816339 * rand01(caveRng));
                        double branchPitch = pitch + std::uniform_real_distribution<double>(-0.2, 0.2)(caveRng);
                        double branchRadius = radius * (0.8 + 0.4 * rand01(caveRng));
                        int remainingSteps = steps - i;
                        int branchSteps = static_cast<int>(remainingSteps * (0.5 + 0.5 * rand01(caveRng)));
                        if (branchSteps > 0) carveTunnel(x, y, z, branchYaw, branchPitch, branchRadius, branchSteps, false);
                    }
                }

                // Gradually randomise direction for natural curves
                yaw += std::uniform_real_distribution<double>(-0.1, 0.1)(caveRng);
                pitch += std::uniform_real_distribution<double>(-0.05, 0.05)(caveRng);

                // Clamp pitch to avoid extreme vertical angles
                if (pitch < -1.5) pitch = -1.5;
                if (pitch > 1.5)  pitch = 1.5;
            }
        };

        // Carve each cave in this chunk column
        std::uniform_int_distribution<int> xLocalDist(0, CHUNK_SIZE - 1);
        std::uniform_int_distribution<int> zLocalDist(0, CHUNK_SIZE - 1);

        for (int c = 0; c < caveCount; ++c) {
            // Random start position within this chunk column (world coordinates)
            int lx = xLocalDist(caveRng);
            int lz = zLocalDist(caveRng);
            int wx = cx * CHUNK_SIZE + lx;
            int wz = cz * CHUNK_SIZE + lz;

            // Choose a random starting Y between bedrock+5 and surface height at (wx, wz)
            int surfaceY = getSurfaceY(wx, wz);
            if (surfaceY < 5) surfaceY = 5;
            std::uniform_int_distribution<int> yDist(5, surfaceY);
            int wy = yDist(caveRng);

            // Cave type
            bool spaghetti = (rand01(caveRng) < 0.5);
            double caveRadius = spaghetti ? std::uniform_real_distribution<double>(1.5, 2.5)(caveRng) : std::uniform_real_distribution<double>(1.0, 1.4)(caveRng);

            // Tunnel length (steps) based on type
            int lengthSteps;
            if (spaghetti) lengthSteps = 2 * std::uniform_int_distribution<int>(40, 80)(caveRng);
            else lengthSteps = 2 * std::uniform_int_distribution<int>(15, 40)(caveRng);

            // Initial direction angles
            double yawAngle   = std::uniform_real_distribution<double>(0.0, 2*M_PI)(caveRng);
            double pitchAngle = std::uniform_real_distribution<double>(-0.2, 0.2)(caveRng);

            // Carve out this cave tunnel (with branching allowed)
            carveTunnel(wx + 0.5, wy + 0.5, wz + 0.5, yawAngle, pitchAngle, caveRadius, lengthSteps, true);
        }

        updateCaveExposedSurfacesForColumn(cx, cz);
    }

    // Convert any dirt block that now has air immediately above it into grass.
    void updateCaveExposedSurfacesForColumn(int cx, int cz) {
        // Iterate through all vertical chunk segments in this column
        for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
            Chunk* chunk = getChunk(cx, cy, cz);
            if (!chunk || !chunk->isGenerated) continue;

            bool changed = false;
            // For every block in the chunk, if it's dirt and the block above is air -> turn to grass
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                        Block& b = chunk->blocks[x][y][z];
                        if (!b.isSolid) continue;
                        if (b.type != BLOCK_DIRT) continue;

                        int worldX = cx * CHUNK_SIZE + x;
                        int worldY = cy * CHUNK_HEIGHT + y;
                        int worldZ = cz * CHUNK_SIZE + z;

                        // If the voxel above is out of bounds or not solid, treat as exposed
                        bool airAbove = (worldY + 1 >= WORLD_SIZE_Y) ? true : !isSolidAt(worldX, worldY + 1, worldZ);
                        if (airAbove) {
                            b.type = BLOCK_GRASS;
                            changed = true;
                        }
                    }
                }
            }

            if (changed) {
                chunk->isDirty = true;
            }
        }
    }

    // Generate ores in an already generated chunk
    void generateOres(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || !chunk->isGenerated) return;
        
        // Reproducible RNG per chunk for ore distribution
        std::mt19937 rng(PERLIN_SEED + cx * 73856093 + cy * 19349663 + cz * 83492791);
        std::uniform_real_distribution<float> oreChance(0.0f, 1.0f);
        
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    int worldY = cy * CHUNK_HEIGHT + y;
                    Block& block = chunk->blocks[x][y][z];
                    if (block.isSolid && block.type == BLOCK_STONE) {
                        // Coal ore generation
                        if (worldY >= COAL_ORE_MIN_Y && worldY <= COAL_ORE_MAX_Y) {
                            if (oreChance(rng) < COAL_ORE_CHANCE) {
                                block.type = BLOCK_COAL_ORE;
                            }
                        }
                        // Iron ore generation
                        if (worldY >= IRON_ORE_MIN_Y && worldY <= IRON_ORE_MAX_Y) {
                            if (oreChance(rng) < IRON_ORE_CHANCE) {
                                block.type = BLOCK_IRON_ORE;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Update surface blocks
    void updateSurfaceBlocks(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || !chunk->isGenerated) return;

        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                // Find the topmost solid block in this column within the chunk
                int topYLocal = -1;
                for (int y = CHUNK_HEIGHT - 1; y >= 0; --y) {
                    if (chunk->blocks[x][y][z].isSolid) {
                        topYLocal = y;
                        break;
                    }
                }
                if (topYLocal < 0) continue;

                int worldX = cx * CHUNK_SIZE + x;
                int worldY = cy * CHUNK_HEIGHT + topYLocal;
                int worldZ = cz * CHUNK_SIZE + z;
                Block& topBlock = chunk->blocks[x][topYLocal][z];

                // Only consider it surface if the block above is air (or above world bounds)
                bool airAbove = (worldY + 1 >= WORLD_SIZE_Y) ? true : !isSolidAt(worldX, worldY + 1, worldZ);
                if (!airAbove) continue;

                // Turn dirt into grass at the surface
                if (topBlock.type == BLOCK_DIRT) {
                    topBlock.type = BLOCK_GRASS;
                }

                // Ensure exactly two dirt layers below the grass (if solid)
                for (int i = 1; i <= 2; ++i) {
                    int yBelow = topYLocal - i;
                    if (yBelow < 0) break;
                    Block& b = chunk->blocks[x][yBelow][z];
                    if (b.isSolid && b.type != BLOCK_BEDROCK) {
                        // Convert stone or ores near surface into dirt for consistency
                        if (b.type != BLOCK_DIRT && b.type != BLOCK_GRASS) {
                            b.type = BLOCK_DIRT;
                        }
                    }
                }

                // Convert any deeper dirt (beyond 2 layers down) back to stone 
                // to avoid thick dirt shelves
                for (int i = 3; i <= 8; ++i) {
                    int yDeep = topYLocal - i;
                    if (yDeep < 0) break;
                    Block& b = chunk->blocks[x][yDeep][z];
                    if (!b.isSolid) break;            // stop at caves/air
                    if (b.type != BLOCK_DIRT) break;  // stop if already stone/ore
                    b.type = BLOCK_STONE;
                }
            }
        }
    }
    
    // Load (generate or activate) all chunks within a given radius of a world position
    void loadChunksAroundPosition(float worldX, float worldZ) {
        int centerChunkX = static_cast<int>(std::floor(worldX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(worldZ / CHUNK_SIZE));
        
        // Pass 1: Ensure base terrain for all chunks in range is generated
        for (int cx = centerChunkX - CHUNK_LOAD_DISTANCE; cx <= centerChunkX + CHUNK_LOAD_DISTANCE; ++cx) {
            for (int cz = centerChunkZ - CHUNK_LOAD_DISTANCE; cz <= centerChunkZ + CHUNK_LOAD_DISTANCE; ++cz) {
                // Only load chunks within a circle (radius^2) area around the player
                int dx = cx - centerChunkX;
                int dz = cz - centerChunkZ;
                if (dx * dx + dz * dz > CHUNK_LOAD_DISTANCE * CHUNK_LOAD_DISTANCE) continue;
                if (!isChunkInBounds(cx, 0, cz)) continue;
                // Generate all vertical chunk segments for this column (cx, cz)
                for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                    if (!isChunkInBounds(cx, cy, cz)) continue;
                    ChunkCoord coord{cx, cy, cz};
                    Chunk* chunk = getChunk(cx, cy, cz);
                    if (!chunk) continue;
                    // Generate terrain if not already done for this chunk segment
                    if (!chunk->isGenerated) {
                        generateChunk(cx, cy, cz);
                    }
                    // Mark this chunk as active/loaded
                    loadedChunks.insert(coord);
                }
            }
        }
        
        // Pass 3: Carve caves in all newly generated chunks (after water generation)
        for (int cx = centerChunkX - CHUNK_LOAD_DISTANCE; cx <= centerChunkX + CHUNK_LOAD_DISTANCE; ++cx) {
            for (int cz = centerChunkZ - CHUNK_LOAD_DISTANCE; cz <= centerChunkZ + CHUNK_LOAD_DISTANCE; ++cz) {
                int dx = cx - centerChunkX;
                int dz = cz - centerChunkZ;
                if (dx * dx + dz * dz > CHUNK_LOAD_DISTANCE * CHUNK_LOAD_DISTANCE) continue;
                if (!isChunkInBounds(cx, 0, cz)) continue;
                // Only generates caves if chunk column isn't fully processed (skips if already done in a previous load)
                generateCaves(cx, 0, cz);
            }
        }
        
        // Pass 4: Populate ores in all generated chunks
        for (int cx = centerChunkX - CHUNK_LOAD_DISTANCE; cx <= centerChunkX + CHUNK_LOAD_DISTANCE; ++cx) {
            for (int cz = centerChunkZ - CHUNK_LOAD_DISTANCE; cz <= centerChunkZ + CHUNK_LOAD_DISTANCE; ++cz) {
                int dx = cx - centerChunkX;
                int dz = cz - centerChunkZ;
                if (dx * dx + dz * dz > CHUNK_LOAD_DISTANCE * CHUNK_LOAD_DISTANCE) continue;
                if (!isChunkInBounds(cx, 0, cz)) continue;
                for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                    Chunk* chunk = getChunk(cx, cy, cz);
                    if (chunk && chunk->isGenerated && !chunk->isFullyProcessed) {
                        generateOres(cx, cy, cz);
                    }
                }
            }
        }
        
        // Pass 5: Update surface blocks (add grass, adjust soil layers) and mark chunks as fully processed
        for (int cx = centerChunkX - CHUNK_LOAD_DISTANCE; cx <= centerChunkX + CHUNK_LOAD_DISTANCE; ++cx) {
            for (int cz = centerChunkZ - CHUNK_LOAD_DISTANCE; cz <= centerChunkZ + CHUNK_LOAD_DISTANCE; ++cz) {
                int dx = cx - centerChunkX;
                int dz = cz - centerChunkZ;
                if (dx * dx + dz * dz > CHUNK_LOAD_DISTANCE * CHUNK_LOAD_DISTANCE) continue;
                if (!isChunkInBounds(cx, 0, cz)) continue;
                for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                    Chunk* chunk = getChunk(cx, cy, cz);
                    if (chunk && chunk->isGenerated && !chunk->isFullyProcessed) {
                        updateSurfaceBlocks(cx, cy, cz);
                        chunk->isFullyProcessed = true;
                    }
                }
            }
        }
    }
    
    // Unload chunks far from the given position to free memory (retain data in map for reuse)
    void unloadDistantChunks(float worldX, float worldZ) {
        int centerChunkX = static_cast<int>(std::floor(worldX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(worldZ / CHUNK_SIZE));
        std::vector<ChunkCoord> toUnload;
        for (const auto& coord : loadedChunks) {
            int dx = coord.x - centerChunkX;
            int dz = coord.z - centerChunkZ;
            int distSq = dx * dx + dz * dz;
            // Unload if beyond (CHUNK_LOAD_DISTANCE + 2) radius (extra buffer)
            if (distSq > (CHUNK_LOAD_DISTANCE + 2) * (CHUNK_LOAD_DISTANCE + 2)) {
                toUnload.push_back(coord);
            }
        }
        // Remove far chunks from the loaded set (they remain in `chunks` map for persistence)
        for (const auto& coord : toUnload) {
            loadedChunks.erase(coord);
        }
    }
    
    // Check if a world-space coordinate is solid (for physics/collision)
    bool isSolidAt(int x, int y, int z) const {
        if (x < 0 || x >= WORLD_SIZE_X ||
            y < 0 || y >= WORLD_SIZE_Y ||
            z < 0 || z >= WORLD_SIZE_Z) {
            return false;
        }
        int cx = x / CHUNK_SIZE;
        int cy = y / CHUNK_HEIGHT;
        int cz = z / CHUNK_SIZE;
        int bx = x % CHUNK_SIZE;
        int by = y % CHUNK_HEIGHT;
        int bz = z % CHUNK_SIZE;
        auto it = chunks.find({cx, cy, cz});
        if (it == chunks.end()) return false;
        return it->second->blocks[bx][by][bz].isSolid;
    }
    
    // Get a pointer to the Block at world-space coordinates (or nullptr if out of bounds)
    Block* getBlockAt(int x, int y, int z) {
        if (x < 0 || x >= WORLD_SIZE_X ||
            y < 0 || y >= WORLD_SIZE_Y ||
            z < 0 || z >= WORLD_SIZE_Z) {
            return nullptr;
        }
        int cx = x / CHUNK_SIZE;
        int cy = y / CHUNK_HEIGHT;
        int cz = z / CHUNK_SIZE;
        int bx = x % CHUNK_SIZE;
        int by = y % CHUNK_HEIGHT;
        int bz = z % CHUNK_SIZE;
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk) return nullptr;
        return &chunk->blocks[bx][by][bz];
    }
    
    // Mark a chunk (and its neighbors) as needing a mesh update (after a block change)
    void markChunkDirty(int cx, int cy, int cz) {
        if (Chunk* chunk = getChunk(cx, cy, cz)) {
            chunk->isDirty = true;
        }
        // Mark adjacent chunks dirty so their mesh will update at boundaries
        if (Chunk* c = getChunk(cx - 1, cy, cz)) c->isDirty = true;
        if (Chunk* c = getChunk(cx + 1, cy, cz)) c->isDirty = true;
        if (Chunk* c = getChunk(cx, cy - 1, cz)) c->isDirty = true;
        if (Chunk* c = getChunk(cx, cy + 1, cz)) c->isDirty = true;
        if (Chunk* c = getChunk(cx, cy, cz - 1)) c->isDirty = true;
        if (Chunk* c = getChunk(cx, cy, cz + 1)) c->isDirty = true;
    }
    
    // Access the set of currently loaded (active) chunks
    const std::unordered_set<ChunkCoord>& getLoadedChunks() const {
        return loadedChunks;
    }
    
    // Access the map of all chunk data (generated chunks in memory)
    const std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>>& getChunks() const {
        return chunks;
    }
};

#endif // BLOCKS_CHUNKS_WORLDS_HPP
