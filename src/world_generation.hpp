#ifndef WORLD_GENERATION_HPP
#define WORLD_GENERATION_HPP

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <vector>
#include <random>
#include <functional>

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
    bool isFullyProcessed = false;
    ChunkCoord coord;
    Chunk() = default;
    Chunk(int cx, int cy, int cz) : coord{cx, cy, cz} {}
};

class World {
private:
    PerlinNoise perlin;
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>> chunks;
    std::unordered_set<ChunkCoord, std::hash<ChunkCoord>> loadedChunks;

    // Tree Generation Helpers
    inline void setBlockAndMarkDirty(int x, int y, int z, BlockType t, bool solid = true) {
        if (x < 0 || x >= WORLD_SIZE_X || y < 0 || y >= WORLD_SIZE_Y || z < 0 || z >= WORLD_SIZE_Z) return;
        int cx = x / CHUNK_SIZE;
        int cy = y / CHUNK_HEIGHT;
        int cz = z / CHUNK_SIZE;
        Block* b = getBlockAt(x, y, z);
        if (!b) return;
        b->isSolid = solid;
        b->type = t;
        markChunkDirty(cx, cy, cz);
    }

    inline bool isAirAt(int x, int y, int z) const {
        const Block* b = getBlockAt(x, y, z);
        return (!b || !b->isSolid);
    }

    // Find the current surface Y for (x,z) - topmost solid, non-water with air above
    int findSurfaceY(int x, int z) const {
        for (int y = WORLD_SIZE_Y - 2; y >= 1; --y) {
            const Block* b = getBlockAt(x, y, z);
            if (!b || !b->isSolid) continue;
            if (b->type == BLOCK_WATER) continue;
            const Block* above = getBlockAt(x, y + 1, z);
            if (!above || !above->isSolid) return y;
        }
        return -1;
    }

    // Simple tree canopy placement around (x, yTop, z)
    void placeOakCanopy(int x, int yTop, int z, std::mt19937& rng) {
        std::uniform_real_distribution<float> r01(0.0f, 1.0f);
        for (int dy = -2; dy <= 2; ++dy) {
            int layerY = yTop + dy;
            if (layerY < 0 || layerY >= WORLD_SIZE_Y) continue;

            int r = (std::abs(dy) >= 2) ? 1 : 2;
            for (int dx = -r; dx <= r; ++dx) {
                for (int dz = -r; dz <= r; ++dz) {
                    if (std::abs(dx) + std::abs(dz) > r + 1) continue;
                    int wx = x + dx;
                    int wz = z + dz;
                    if ((std::abs(dx) == r || std::abs(dz) == r) && r01(rng) < 0.25f) continue;

                    const Block* here = getBlockAt(wx, layerY, wz);

                    // Only place leaves into air
                    if (here && here->isSolid) continue;
                    setBlockAndMarkDirty(wx, layerY, wz, BLOCK_LEAVES, true);
                }
            }
        }

        // A little tuft on the very top
        if (yTop + 3 < WORLD_SIZE_Y && isAirAt(x, yTop + 3, z)) setBlockAndMarkDirty(x, yTop + 3, z, BLOCK_LEAVES, true);
    }

    // Attempt to place oak tree
    bool tryPlaceOakTreeAt(int x, int groundY, int z, std::mt19937& rng) {
        // Ground must be grass, air above, no water
        Block* ground = getBlockAt(x, groundY, z);
        if (!ground || !ground->isSolid || ground->type != BLOCK_GRASS) return false;
        if (!isAirAt(x, groundY + 1, z)) return false;
        std::uniform_int_distribution<int> trunkDist(4, 6);
        int trunkH = trunkDist(rng);
        if (groundY + trunkH + 3 >= WORLD_SIZE_Y) return false; // ensure canopy fits

        // Ensure trunk column is clear
        for (int i = 1; i <= trunkH; ++i) {
            const Block* b = getBlockAt(x, groundY + i, z);
            if (b && b->isSolid && b->type != BLOCK_LEAVES) return false;
        }

        // Place trunk
        for (int i = 1; i <= trunkH; ++i) {
            setBlockAndMarkDirty(x, groundY + i, z, BLOCK_LOG, true);
        }

        // Place canopy centered at trunk top
        int canopyY = groundY + trunkH;
        placeOakCanopy(x, canopyY, z, rng);

        return true;
    }

    // Generate trees once per (cx, cz) column
    void generateTreesForColumn(int cx, int cz) {
        // Deterministic RNG per column
        unsigned int seed = static_cast<unsigned int>(PERLIN_SEED) ^ (cx * 0x9E3779B9u) ^ (cz * 0x85EBCA6Bu);
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> r01(0.0f, 1.0f);

        int baseX = cx * CHUNK_SIZE;
        int baseZ = cz * CHUNK_SIZE;

        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                int wx = baseX + lx;
                int wz = baseZ + lz;

                // Light density noise to vary tree density
                double densityNoise = perlin.noise(wx * 0.01, 0.0, wz * 0.01) * 0.5 + 0.5; // [0,1]
                float spawnChance = 0.015f + static_cast<float>(0.02 * densityNoise);
                if (r01(rng) >= spawnChance) continue;

                // Find actual surface after caves/surface updates
                int y = findSurfaceY(wx, wz);
                if (y < 0) continue;

                // Must be grass
                Block* g = getBlockAt(wx, y, wz);
                if (!g || g->type != BLOCK_GRASS) continue;

                // Avoid shorelines
                bool nearWater = false;
                for (int dx = -1; dx <= 1 && !nearWater; ++dx) {
                    for (int dz = -1; dz <= 1 && !nearWater; ++dz) {
                        const Block* nb = getBlockAt(wx + dx, y, wz + dz);
                        if (nb && nb->isSolid && nb->type == BLOCK_WATER) nearWater = true;
                    }
                }
                if (nearWater) continue;

                // Place tree
                tryPlaceOakTreeAt(wx, y, wz, rng);
            }
        }
    }

    // Find the topmost seabed block
    int findUnderwaterFloorY(int wx, int wyMinInclusive, int wyMaxInclusive, int wz) const {
        int top = std::min(wyMaxInclusive, WATER_LEVEL - 1);
        for (int y = top; y >= wyMinInclusive; --y) {
            const Block* here = getBlockAt(wx, y, wz);
            const Block* above = getBlockAt(wx, y + 1, wz);
            if (!here) continue;
            if (!here->isSolid || here->type == BLOCK_WATER) continue;
            if (above && above->isSolid && above->type == BLOCK_WATER) {
                return y;
            }
        }
        return -1;
    }

    // Normalise water floor
    void normaliseUnderwaterFloorToDirt(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || !chunk->isGenerated) return;

        int baseX = cx * CHUNK_SIZE;
        int baseY = cy * CHUNK_HEIGHT;
        int baseZ = cz * CHUNK_SIZE;
        if (baseY >= WATER_LEVEL) return;

        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                int wx = baseX + lx;
                int wz = baseZ + lz;
                int floorY = findUnderwaterFloorY(wx, baseY, baseY + CHUNK_HEIGHT - 2, wz);
                if (floorY < 0) continue;
                Block* b = getBlockAt(wx, floorY, wz);
                if (!b || !b->isSolid) continue;
                if (b->type == BLOCK_STONE) {
                    setBlockAndMarkDirty(wx, floorY, wz, BLOCK_DIRT, true);
                }
            }
        }
    }

    // Paint a small sand "disk" at (centerX, centerZ) on the underwater floor
    void paintSandPatchAt(int centerX, int centerZ, int minY, int maxY, int radius, int depth, std::mt19937& rng) {
        int r2 = radius * radius;
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (dx*dx + dz*dz > r2) continue;
                int wx = centerX + dx;
                int wz = centerZ + dz;
                if (wx < 0 || wx >= WORLD_SIZE_X || wz < 0 || wz >= WORLD_SIZE_Z) continue;

                int floorY = findUnderwaterFloorY(wx, minY, maxY, wz);
                if (floorY < 0) continue;

                // Convert to sand
                for (int d = 0; d < depth; ++d) {
                    int y = floorY - d;
                    if (y <= 0) break;
                    Block* b = getBlockAt(wx, y, wz);
                    if (!b || !b->isSolid) break;
                    if (b->type == BLOCK_BEDROCK) break;
                    if (b->type == BLOCK_WATER) break;
                    setBlockAndMarkDirty(wx, y, wz, BLOCK_SAND, true);
                }
            }
        }
    }

    // Generate underwater sand patches within a chunk segment.
    void generateUnderwaterSandPatches(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || !chunk->isGenerated || chunk->isFullyProcessed) return;

        int baseX = cx * CHUNK_SIZE;
        int baseY = cy * CHUNK_HEIGHT;
        int baseZ = cz * CHUNK_SIZE;
        if (baseY >= WATER_LEVEL) return;

        normaliseUnderwaterFloorToDirt(cx, cy, cz);
        unsigned int seed = static_cast<unsigned int>(PERLIN_SEED) ^ (cx * 0xC2B2AE35u) ^ (cz * 0x27D4EB2Fu) ^ (cy * 0x85EBCA6Bu);
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> r01(0.0f, 1.0f);
        std::uniform_int_distribution<int> radiusDist(2, 4);
        std::uniform_int_distribution<int> groupCountDist(1, 2);

        // Place candidate centers on coarse grid
        const int STEP = 4;
        for (int lx = 0; lx < CHUNK_SIZE; lx += STEP) {
            for (int lz = 0; lz < CHUNK_SIZE; lz += STEP) {
                int wx = baseX + lx;
                int wz = baseZ + lz;

                // Must have a valid underwater floor within this vertical segment
                int floorY = findUnderwaterFloorY(wx, baseY, baseY + CHUNK_HEIGHT - 2, wz);
                if (floorY < 0) continue;

                // Low-frequency noise to clump patches
                double n = perlin.noise(wx * 0.02, 123.45, wz * 0.02) * 0.5 + 0.5; // [0,1]
                if (!(n > 0.65 && r01(rng) < 0.25f)) continue;
                int groups = groupCountDist(rng);
                int baseRadius = radiusDist(rng);
                int depth = (r01(rng) < 0.2f) ? 2 : 1;

                for (int g = 0; g < groups; ++g) {
                    // small jitter around the seed to create grouped patches
                    int jx = wx + static_cast<int>(std::round((r01(rng) - 0.5f) * baseRadius * 1.5f));
                    int jz = wz + static_cast<int>(std::round((r01(rng) - 0.5f) * baseRadius * 1.5f));
                    int r = std::max(1, baseRadius + (int)std::floor((r01(rng) - 0.5f) * 2.0f));
                    paintSandPatchAt(jx, jz, baseY, baseY + CHUNK_HEIGHT - 2, r, depth, rng);
                }
            }
        }
    }

    // Paint ore sphere
    void paintOreSphere(BlockType oreType, int cxHint, int cyHint, int czHint, int cxWorld, int cyWorld, int czWorld, float radius) {
        int minX = static_cast<int>(std::floor(cxWorld - radius));
        int maxX = static_cast<int>(std::floor(cxWorld + radius));
        int minY = static_cast<int>(std::floor(cyWorld - radius));
        int maxY = static_cast<int>(std::floor(cyWorld + radius));
        int minZ = static_cast<int>(std::floor(czWorld - radius));
        int maxZ = static_cast<int>(std::floor(czWorld + radius));

        float r2 = radius * radius;
        for (int x = minX; x <= maxX; ++x) {
            if (x < 0 || x >= WORLD_SIZE_X) continue;
            for (int y = minY; y <= maxY; ++y) {
                if (y < 1 || y >= WORLD_SIZE_Y) continue;
                for (int z = minZ; z <= maxZ; ++z) {
                    if (z < 0 || z >= WORLD_SIZE_Z) continue;

                    float dx = (x + 0.5f) - cxWorld;
                    float dy = (y + 0.5f) - cyWorld;
                    float dz = (z + 0.5f) - czWorld;
                    if (dx*dx + dy*dy + dz*dz > r2) continue;

                    Block* b = getBlockAt(x, y, z);
                    if (!b) continue;
                    if (!b->isSolid) continue;
                    if (b->type != BLOCK_STONE) continue;

                    setBlockAndMarkDirty(x, y, z, oreType, true);
                }
            }
        }
    }

    void generateOreVeinsForType(int cx, int cy, int cz, BlockType oreType, int minY, int maxY, int triesMin, int triesMax, int veinSizeMin, int veinSizeMax) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || !chunk->isGenerated) return;

        int baseX = cx * CHUNK_SIZE;
        int baseY = cy * CHUNK_HEIGHT;
        int baseZ = cz * CHUNK_SIZE;

        // Intersect Y-range with this segment
        int yMinLocal = std::max(minY, baseY + 1);
        int yMaxLocal = std::min(maxY, baseY + CHUNK_HEIGHT - 2);
        if (yMinLocal > yMaxLocal) return;

        unsigned int seed = static_cast<unsigned int>(PERLIN_SEED) ^ (cx * 0x51ED270Bu) ^ (cz * 0xA5CB924Fu) ^ (cy * 0x3255EC4Fu) ^ (static_cast<unsigned>(oreType) * 0x9E3779B9u);
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> triesDist(triesMin, triesMax);
        std::uniform_int_distribution<int> xDist(baseX, baseX + CHUNK_SIZE - 1);
        std::uniform_int_distribution<int> zDist(baseZ, baseZ + CHUNK_SIZE - 1);
        std::uniform_int_distribution<int> yDist(yMinLocal, yMaxLocal);
        std::uniform_int_distribution<int> sizeDist(veinSizeMin, veinSizeMax);
        std::uniform_real_distribution<float> r01(0.0f, 1.0f);

        int tries = triesDist(rng);
        for (int t = 0; t < tries; ++t) {
            int sx = xDist(rng);
            int sy = yDist(rng);
            int sz = zDist(rng);

            // Must start in stone
            Block* start = getBlockAt(sx, sy, sz);
            if (!start || !start->isSolid || start->type != BLOCK_STONE) continue;

            int steps = sizeDist(rng);
            float px = static_cast<float>(sx);
            float py = static_cast<float>(sy);
            float pz = static_cast<float>(sz);

            for (int i = 0; i < steps; ++i) {
                float radius = 0.9f + 0.9f * r01(rng);
                paintOreSphere(oreType, cx, cy, cz, static_cast<int>(std::round(px)), static_cast<int>(std::round(py)), static_cast<int>(std::round(pz)), radius);

                // Biased random drift
                float yaw = (r01(rng) * 2.0f - 1.0f) * 3.14159265f;
                float pitch = (r01(rng) * 2.0f - 1.0f) * 0.5f;
                float stepLen = 1.0f + 0.3f * r01(rng);
                px += std::cos(yaw) * std::cos(pitch) * stepLen;
                py += std::sin(pitch) * stepLen * 0.8f;
                pz += std::sin(yaw) * std::cos(pitch) * stepLen;
                px = std::clamp(px, 0.0f, static_cast<float>(WORLD_SIZE_X - 1));
                py = std::clamp(py, static_cast<float>(yMinLocal), static_cast<float>(yMaxLocal));
                pz = std::clamp(pz, 0.0f, static_cast<float>(WORLD_SIZE_Z - 1));
            }
        }

        chunk->isDirty = true;
    }

public:
    void initialise() {
        perlin = PerlinNoise(PERLIN_SEED);
    }
    
    // Check if chunk indices are within world bounds
    bool isChunkInBounds(int cx, int cy, int cz) const {
        return cx >= 0 && cx < WORLD_CHUNK_SIZE_X && cy >= 0 && cy < WORLD_CHUNK_SIZE_Y && cz >= 0 && cz < WORLD_CHUNK_SIZE_Z;
    }
    
    // Get or create a chunk at given chunk coordinates
    Chunk* getChunk(int cx, int cy, int cz) {
        if (!isChunkInBounds(cx, cy, cz)) return nullptr;
        ChunkCoord coord{cx, cy, cz};
        auto it = chunks.find(coord);

        // Create new chunk if not exists
        if (it == chunks.end()) {
            auto chunk = std::make_unique<Chunk>(cx, cy, cz);
            Chunk* ptr = chunk.get();
            chunks[coord] = std::move(chunk);
            return ptr;
        }

        return it->second.get();
    }
    
    // Check if chunk is already created in memory
    bool isChunkLoaded(int cx, int cy, int cz) const {
        ChunkCoord coord{cx, cy, cz};
        return chunks.find(coord) != chunks.end();
    }
    
    // Check if chunk at player position is fully generated and safe for physics
    bool isChunkGeneratedAt(float worldX, float worldY, float worldZ) const {
        int cx = static_cast<int>(std::floor(worldX / CHUNK_SIZE));
        int cy = static_cast<int>(std::floor(worldY / CHUNK_HEIGHT));
        int cz = static_cast<int>(std::floor(worldZ / CHUNK_SIZE));
        
        if (!isChunkInBounds(cx, cy, cz)) return false;
        
        ChunkCoord coord{cx, cy, cz};
        auto it = chunks.find(coord);
        if (it == chunks.end()) return false;
        
        return it->second->isGenerated;
    }
    
    // Main function for determining terrain height using layered Perlin noise
    int getHeightAt(int x, int z) const {
        static const double WARP_FREQ = 0.0020;
        static const double WARP_AMPLITUDE = 80.0;

        double warpNoise1 = perlin.noise(x * WARP_FREQ, z * WARP_FREQ);
        double warpNoise2 = perlin.noise(x * WARP_FREQ + 1000.0, z * WARP_FREQ + 1000.0);
        double warpX = warpNoise1 * WARP_AMPLITUDE;
        double warpZ = warpNoise2 * WARP_AMPLITUDE;
        double warpedX = x + warpX;
        double warpedZ = z + warpZ;

        // Domain rotation - skewed cube coordinates
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
                double n = sampleNoise3D(freq) * 0.5 + 0.5;
                total += n * amplitude;
                maxAmp += amplitude;
                amplitude *= 0.5;
                freq *= 2.0;
            }
            return total / maxAmp;
        };

        constexpr double HILL_BAND_SCALE = 1.0;
        const double continental = fbmNoise(0.005 * HILL_BAND_SCALE, 3);
        const double hills = fbmNoise(0.012 * HILL_BAND_SCALE, 5);
        const double detail = fbmNoise(0.02,  2);
        double heightFactor = 0.55 * hills + 0.40 * continental + 0.05 * detail;

        // Remove terracing
        auto smoothstep = [](double a, double b, double x) {
            double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
            return t * t * (3.0 - 2.0 * t);
        };

        auto softTerrace = [&](double v, double steps, double softness) {
            double u = v * steps;
            double f = u - std::floor(u);
            double u2 = std::floor(u) + smoothstep(0.3, 0.7, f);
            return std::lerp(v, u2 / steps, softness);
        };

        heightFactor = softTerrace(heightFactor, 12.0, 0.10);
        double valleyStretch = 2.2;
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
        
        // Loop through every position in chunk column section
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

    // Fill air blocks below WATER_LEVEL
    void generateWater(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || !chunk->isGenerated) return;
        
        int baseX = cx * CHUNK_SIZE;
        int baseY = cy * CHUNK_HEIGHT;
        int baseZ = cz * CHUNK_SIZE;
        
        bool waterAdded = false;
        
        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                for (int ly = 0; ly < CHUNK_HEIGHT; ++ly) {
                    int worldY = baseY + ly;
                    
                    // Only process blocks below water level
                    if (worldY >= WATER_LEVEL) continue;
                    
                    Block& block = chunk->blocks[lx][ly][lz];
                    
                    // If it's air, convert to water
                    if (!block.isSolid) {
                        block.isSolid = true;
                        block.type = BLOCK_WATER;
                        waterAdded = true;
                    }
                }
            }
        }
        
        if (waterAdded) {
            chunk->isDirty = true;
        }
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

                            // Only carve if the chunk containing this block is generated
                            int cx2 = xi / CHUNK_SIZE;
                            int cy2 = yi / CHUNK_HEIGHT;
                            int cz2 = zi / CHUNK_SIZE;
                            if (!isChunkLoaded(cx2, cy2, cz2)) continue;
                            Block* block = getBlockAt(xi, yi, zi);
                            if (!block) continue;

                            // Don't carve through water blocks to prevent flooded caves
                            if (block->isSolid) {
                                if (block->type != BLOCK_WATER) {
                                    block->isSolid = false;
                                    markChunkDirty(cx2, cy2, cz2);
                                }
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
                if (pitch < -1.5) pitch = -1.5;
                if (pitch > 1.5)  pitch = 1.5;
            }
        };

        // Carve each cave in this chunk column
        std::uniform_int_distribution<int> xLocalDist(0, CHUNK_SIZE - 1);
        std::uniform_int_distribution<int> zLocalDist(0, CHUNK_SIZE - 1);

        for (int c = 0; c < caveCount; ++c) {
            // Random start position within this chunk column
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

            // Tunnel length based on type
            int lengthSteps;
            if (spaghetti) lengthSteps = 2 * std::uniform_int_distribution<int>(40, 80)(caveRng);
            else lengthSteps = 2 * std::uniform_int_distribution<int>(15, 40)(caveRng);

            // Initial direction angles
            double yawAngle   = std::uniform_real_distribution<double>(0.0, 2*M_PI)(caveRng);
            double pitchAngle = std::uniform_real_distribution<double>(-0.2, 0.2)(caveRng);

            // Carve out this cave tunnel
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
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                        Block& b = chunk->blocks[x][y][z];
                        if (!b.isSolid) continue;
                        if (b.type != BLOCK_DIRT) continue;

                        int worldX = cx * CHUNK_SIZE + x;
                        int worldY = cy * CHUNK_HEIGHT + y;
                        int worldZ = cz * CHUNK_SIZE + z;

                        // Check if the block above is air
                        bool hasAirAbove = false;
                        if (worldY + 1 >= WORLD_SIZE_Y) {
                            hasAirAbove = true;
                        } else {
                            Block* blockAbove = getBlockAt(worldX, worldY + 1, worldZ);
                            if (blockAbove && !blockAbove->isSolid) hasAirAbove = true;
                        }
                        
                        if (hasAirAbove) {
                            b.type = BLOCK_GRASS;
                            changed = true;
                        }
                    }
                }
            }

            if (changed) chunk->isDirty = true;
        }
    }

    // Generate ores as clustered veins
    void generateOres(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || !chunk->isGenerated) return;

        // Coal
        generateOreVeinsForType(cx, cy, cz,
                                BLOCK_COAL_ORE,
                                COAL_ORE_MIN_Y, COAL_ORE_MAX_Y,
                                /*triesMin*/ 2, /*triesMax*/ 5,
                                /*veinSizeMin*/ 3, /*veinSizeMax*/ 14);

        // Iron
        generateOreVeinsForType(cx, cy, cz,
                                BLOCK_IRON_ORE,
                                IRON_ORE_MIN_Y, IRON_ORE_MAX_Y,
                                /*triesMin*/ 2, /*triesMax*/ 4,
                                /*veinSizeMin*/ 2, /*veinSizeMax*/ 12);
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

                // Skip non-terrain tops
                if (topBlock.type == BLOCK_WATER ||
                    topBlock.type == BLOCK_LEAVES ||
                    topBlock.type == BLOCK_LOG) {
                    continue;
                }

                // Only consider it surface if the block above is air (or above world bounds)
                bool airAbove = (worldY + 1 >= WORLD_SIZE_Y) ? true : !isSolidAt(worldX, worldY + 1, worldZ);
                if (!airAbove) continue;

                // Turn dirt into grass at the surface
                if (topBlock.type == BLOCK_DIRT) topBlock.type = BLOCK_GRASS;
                if (topBlock.type != BLOCK_GRASS) continue;

                // Ensure exactly two dirt layers below the grass (if solid)
                for (int i = 1; i <= 2; ++i) {
                    int yBelow = topYLocal - i;
                    if (yBelow < 0) break;
                    Block& b = chunk->blocks[x][yBelow][z];
                    if (b.isSolid &&
                        b.type != BLOCK_BEDROCK &&
                        b.type != BLOCK_WATER &&
                        b.type != BLOCK_LOG &&
                        b.type != BLOCK_LEAVES) {
                        if (b.type != BLOCK_DIRT && b.type != BLOCK_GRASS) {
                            b.type = BLOCK_DIRT;
                        }
                    }
                }

                // Convert any deeper dirt (beyond 2 layers down) back to stone to avoid thick shelves
                for (int i = 3; i <= 8; ++i) {
                    int yDeep = topYLocal - i;
                    if (yDeep < 0) break;
                    Block& b = chunk->blocks[x][yDeep][z];
                    if (!b.isSolid) break;
                    if (b.type == BLOCK_LOG || b.type == BLOCK_LEAVES) break;
                    if (b.type != BLOCK_DIRT) break;
                    b.type = BLOCK_STONE;
                }
            }
        }
    }
    
    // Load all chunks within a given radius of a world position
    void loadChunksAroundPosition(float worldX, float worldZ, float playerX = -9999.0f, float playerY = -9999.0f, float playerZ = -9999.0f) {
        int centerChunkX = static_cast<int>(std::floor(worldX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(worldZ / CHUNK_SIZE));
        int playerChunkX = static_cast<int>(std::floor(playerX / CHUNK_SIZE));
        int playerChunkZ = static_cast<int>(std::floor(playerZ / CHUNK_SIZE));
        bool hasPlayerPos = (playerX > -9000.0f);
        
        // Pass 1: Ensure base terrain for all chunks in range is generated
        for (int cx = centerChunkX - CHUNK_LOAD_DISTANCE; cx <= centerChunkX + CHUNK_LOAD_DISTANCE; ++cx) {
            for (int cz = centerChunkZ - CHUNK_LOAD_DISTANCE; cz <= centerChunkZ + CHUNK_LOAD_DISTANCE; ++cz) {
                int dx = cx - centerChunkX;
                int dz = cz - centerChunkZ;
                if (dx * dx + dz * dz > CHUNK_LOAD_DISTANCE * CHUNK_LOAD_DISTANCE) continue;
                if (!isChunkInBounds(cx, 0, cz)) continue;
                
                for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                    if (!isChunkInBounds(cx, cy, cz)) continue;
                    ChunkCoord coord{cx, cy, cz};
                    Chunk* chunk = getChunk(cx, cy, cz);
                    if (!chunk) continue;
                    if (!chunk->isGenerated) generateChunk(cx, cy, cz);
                    loadedChunks.insert(coord);
                }
            }
        }
        
        // Pass 2: Generate water in all chunks below water level
        for (int cx = centerChunkX - CHUNK_LOAD_DISTANCE; cx <= centerChunkX + CHUNK_LOAD_DISTANCE; ++cx) {
            for (int cz = centerChunkZ - CHUNK_LOAD_DISTANCE; cz <= centerChunkZ + CHUNK_LOAD_DISTANCE; ++cz) {
                int dx = cx - centerChunkX;
                int dz = cz - centerChunkZ;
                if (dx * dx + dz * dz > CHUNK_LOAD_DISTANCE * CHUNK_LOAD_DISTANCE) continue;
                if (!isChunkInBounds(cx, 0, cz)) continue;

                for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                    if (!isChunkInBounds(cx, cy, cz)) continue;
                    Chunk* chunk = getChunk(cx, cy, cz);
                    if (chunk && chunk->isGenerated && !chunk->isFullyProcessed) generateWater(cx, cy, cz);
                }
            }
        }

        // Pass 2.5: Underwater sand patches
        for (int cx = centerChunkX - CHUNK_LOAD_DISTANCE; cx <= centerChunkX + CHUNK_LOAD_DISTANCE; ++cx) {
            for (int cz = centerChunkZ - CHUNK_LOAD_DISTANCE; cz <= centerChunkZ + CHUNK_LOAD_DISTANCE; ++cz) {
                int dx = cx - centerChunkX;
                int dz = cz - centerChunkZ;
                if (dx * dx + dz * dz > CHUNK_LOAD_DISTANCE * CHUNK_LOAD_DISTANCE) continue;
                if (!isChunkInBounds(cx, 0, cz)) continue;

                for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                    Chunk* chunk = getChunk(cx, cy, cz);
                    if (chunk && chunk->isGenerated && !chunk->isFullyProcessed) generateUnderwaterSandPatches(cx, cy, cz);
                }
            }
        }
        
        // Pass 3: Caves in all newly generated chunks
        for (int cx = centerChunkX - CHUNK_LOAD_DISTANCE; cx <= centerChunkX + CHUNK_LOAD_DISTANCE; ++cx) {
            for (int cz = centerChunkZ - CHUNK_LOAD_DISTANCE; cz <= centerChunkZ + CHUNK_LOAD_DISTANCE; ++cz) {
                int dx = cx - centerChunkX;
                int dz = cz - centerChunkZ;
                if (dx * dx + dz * dz > CHUNK_LOAD_DISTANCE * CHUNK_LOAD_DISTANCE) continue;
                if (!isChunkInBounds(cx, 0, cz)) continue;
                
                if (hasPlayerPos) {
                    if (std::abs(cx - playerChunkX) <= 1 && std::abs(cz - playerChunkZ) <= 1) continue;
                }
                
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
                    if (chunk && chunk->isGenerated && !chunk->isFullyProcessed) generateOres(cx, cy, cz);
                }
            }
        }
        
        // Pass 5: Update surface, then generate trees
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
                        if (cy == WORLD_CHUNK_SIZE_Y - 1) generateTreesForColumn(cx, cz);
                        chunk->isFullyProcessed = true;
                    }
                }
            }
        }
    }
    
    // Unload chunks far from the given position to free memory, still keeping them in the chunks map
    void unloadDistantChunks(float worldX, float worldZ) {
        int centerChunkX = static_cast<int>(std::floor(worldX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(worldZ / CHUNK_SIZE));
        std::vector<ChunkCoord> toUnload;

        for (const auto& coord : loadedChunks) {
            int dx = coord.x - centerChunkX;
            int dz = coord.z - centerChunkZ;
            int distSq = dx * dx + dz * dz;

            // Unload if beyond (CHUNK_LOAD_DISTANCE + 2) radius (extra buffer)
            if (distSq > (CHUNK_LOAD_DISTANCE + 2) * (CHUNK_LOAD_DISTANCE + 2)) toUnload.push_back(coord);
        }

        for (const auto& coord : toUnload) loadedChunks.erase(coord);
    }
    
    // Check if a world-space coordinate is solid
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

        // If chunk doesn't exist or isn't generated yet, treat as non-solid
        // This prevents false positives during chunk loading
        if (it == chunks.end() || !it->second->isGenerated) return false;
        return it->second->blocks[bx][by][bz].isSolid;
    }
    
    // Get a pointer to the Block at world-space coordinates
    Block* getBlockAt(int x, int y, int z) const {
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
        auto it = chunks.find({cx, cy, cz});
        if (it == chunks.end()) return nullptr;
        return &it->second->blocks[bx][by][bz];
    }
    
    // Check if block at world-space coordinates is opaque
    bool isOpaque(int x, int y, int z) const {
        if (x < 0 || x >= WORLD_SIZE_X || 
            y < 0 || y >= WORLD_SIZE_Y || 
            z < 0 || z >= WORLD_SIZE_Z) {
            return false;
        }
        const Block* block = getBlockAt(x, y, z);
        if (!block || !block->isSolid) return false;
        return block->type != BLOCK_WATER && block->type != BLOCK_LEAVES;
    }
    
    // Mark a chunk (and its neighbors) as needing a mesh update
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
    const std::unordered_set<ChunkCoord, std::hash<ChunkCoord>>& getLoadedChunks() const {
        return loadedChunks;
    }
    
    // Access the map of all chunk data (generated chunks in memory)
    const std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>>& getChunks() const {
        return chunks;
    }
};

#endif // WORLD_GENERATION_HPP
