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

#include "../shared/config.hpp"
#include "../shared/types.hpp"
#include "../shared/chunk.hpp"
#include "perlin_noise.hpp"

// Mountain Generation Constants
constexpr int TREE_LINE_HEIGHT = 60;           // No new trees above this height
constexpr int SNOW_TRANSITION_START = 61;      // Grass/dirt starts fading out, stone appears
constexpr int SNOW_FADE_START = 65;            // Snow begins to appear
constexpr int FULL_SNOW_HEIGHT = 77;           // Full snow coverage above this

// Fallback for std::lerp if not available
namespace {
    template<typename T>
    inline T lerp_fallback(T a, T b, T t) {
        return a + t * (b - a);
    }
}

class Hub; // Forward declaration for friend

class World {
    friend class Hub; // Allow Hub to access private generation methods
private:
    PerlinNoise perlin;
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>> chunks;
    std::unordered_set<ChunkCoord, std::hash<ChunkCoord>> loadedChunks;

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

    // Check if there's water within a certain radius
    bool isWaterNearby(int centerX, int centerY, int centerZ, int radius) const {
        for (int x = centerX - radius; x <= centerX + radius; ++x) {
            for (int y = centerY - radius; y <= centerY + radius; ++y) {
                for (int z = centerZ - radius; z <= centerZ + radius; ++z) {
                    if (x < 0 || x >= WORLD_SIZE_X || y < 0 || y >= WORLD_SIZE_Y || z < 0 || z >= WORLD_SIZE_Z) continue;
                    const Block* b = getBlockAt(x, y, z);
                    if (b && b->type == BLOCK_WATER) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // Find a safe spawn point at least minDistanceFromWater blocks away from any water
    bool findSafeSpawnPoint(float startX, float startZ, int minDistanceFromWater, float& outX, float& outY, float& outZ) {
        int sx = static_cast<int>(std::floor(startX));
        int sz = static_cast<int>(std::floor(startZ));
        const int maxRadius = 64; // Search within 64 blocks

        auto tryPosition = [&](int cx, int cz) -> bool {
            if (cx < 0 || cx >= WORLD_SIZE_X || cz < 0 || cz >= WORLD_SIZE_Z) return false;
            
            int surfaceY = findSurfaceY(cx, cz);
            if (surfaceY < 0 || surfaceY < WATER_LEVEL) return false;
            
            const Block* ground = getBlockAt(cx, surfaceY, cz);
            if (!ground || !ground->isSolid) return false;
            
            // Don't spawn on leaves, logs, or other non-ground blocks - only dirt, grass, stone, sand
            if (ground->type == BLOCK_LEAVES || ground->type == BLOCK_LOG || 
                ground->type == BLOCK_WATER || ground->type == BLOCK_TALL_GRASS ||
                ground->type == BLOCK_ORANGE_FLOWER || ground->type == BLOCK_BLUE_FLOWER) {
                return false;
            }
            
            const Block* above1 = getBlockAt(cx, surfaceY + 1, cz);
            if (above1 && above1->isSolid) return false;
            
            const Block* above2 = getBlockAt(cx, surfaceY + 2, cz);
            if (above2 && above2->isSolid) return false;
            
            // Check if water is nearby
            if (isWaterNearby(cx, surfaceY, cz, minDistanceFromWater)) {
                return false;
            }
            
            // Valid spawn point found
            outX = cx + 0.5f;
            outY = surfaceY + 1.6f;
            outZ = cz + 0.5f;
            return true;
        };

        // Try the starting position first
        if (tryPosition(sx, sz)) return true;

        // Search in expanding circles
        for (int r = 1; r <= maxRadius; ++r) {
            // Check perimeter of square at distance r
            for (int x = sx - r; x <= sx + r; ++x) {
                if (tryPosition(x, sz - r)) return true;
                if (tryPosition(x, sz + r)) return true;
            }
            for (int z = sz - r + 1; z <= sz + r - 1; ++z) {
                if (tryPosition(sx - r, z)) return true;
                if (tryPosition(sx + r, z)) return true;
            }
        }

        return false;
    }

    // Tree generation
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

    bool tryPlaceOakTreeAt(int x, int groundY, int z, std::mt19937& rng) {
        Block* ground = getBlockAt(x, groundY, z);
        if (!ground || !ground->isSolid || ground->type != BLOCK_GRASS) return false;
        if (!isAirAt(x, groundY + 1, z)) return false;
        std::uniform_int_distribution<int> trunkDist(4, 6);
        int trunkH = trunkDist(rng);
        if (groundY + trunkH + 3 >= WORLD_SIZE_Y) return false;

        for (int i = 1; i <= trunkH; ++i) {
            const Block* b = getBlockAt(x, groundY + i, z);
            if (b && b->isSolid && b->type != BLOCK_LEAVES) return false;
        }
        for (int i = 1; i <= trunkH; ++i) setBlockAndMarkDirty(x, groundY + i, z, BLOCK_LOG, true);

        int canopyY = groundY + trunkH;
        placeOakCanopy(x, canopyY, z, rng);
        return true;
    }

    void generateTreesForColumn(int cx, int cz) {
        unsigned int seed = static_cast<unsigned int>(PERLIN_SEED) ^ (cx * 0x9E3779B9u) ^ (cz * 0x85EBCA6Bu);
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> r01(0.0f, 1.0f);

        int baseX = cx * CHUNK_SIZE;
        int baseZ = cz * CHUNK_SIZE;

        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                int wx = baseX + lx;
                int wz = baseZ + lz;

                double densityNoise = perlin.noise(wx * 0.01, 0.0, wz * 0.01) * 0.5 + 0.5; // [0,1]
                float spawnChance = 0.015f + static_cast<float>(0.02 * densityNoise);
                if (r01(rng) >= spawnChance) continue;

                int y = findSurfaceY(wx, wz);
                if (y < 0) continue;

                // Don't spawn trees above the tree line
                if (y >= TREE_LINE_HEIGHT) continue;

                Block* g = getBlockAt(wx, y, wz);
                if (!g || g->type != BLOCK_GRASS) continue;

                bool nearWater = false;
                for (int dx = -1; dx <= 1 && !nearWater; ++dx) {
                    for (int dz = -1; dz <= 1 && !nearWater; ++dz) {
                        const Block* nb = getBlockAt(wx + dx, y, wz + dz);
                        if (nb && nb->isSolid && nb->type == BLOCK_WATER) nearWater = true;
                    }
                }
                if (nearWater) continue;

                tryPlaceOakTreeAt(wx, y, wz, rng);
            }
        }
    }

    // Grass + Flowers generation (abundant grass, small same-type flower clusters)
    void generateFoliageForColumn(int cx, int cz) {
        // Deterministic RNG per (cx,cz)
        unsigned int seed = static_cast<unsigned int>(PERLIN_SEED) ^ (cx * 0xB5297A4Du) ^ (cz * 0x68E31DA4u) ^ 0x9E3779B9u;
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> r01(0.0f, 1.0f);
        std::uniform_int_distribution<int> jitter(-2, 2);

        const int baseX = cx * CHUNK_SIZE;
        const int baseZ = cz * CHUNK_SIZE;

        auto placePlantIfValid = [&](int px, int pz, BlockType t) {
            if (px < 0 || px >= WORLD_SIZE_X || pz < 0 || pz >= WORLD_SIZE_Z) return false;
            int y = findSurfaceY(px, pz);
            if (y < 0) return false;

            // Must sit on grass and empty above
            Block* ground = getBlockAt(px, y, pz);
            if (!ground || ground->type != BLOCK_GRASS) return false;

            Block* above = getBlockAt(px, y + 1, pz);
            if (above && above->isSolid) return false;

            Block* above2 = getBlockAt(px, y + 2, pz);
            if (above2 && above2->isSolid && above2->type == BLOCK_WATER) return false;

            setBlockAndMarkDirty(px, y + 1, pz, t, /*solid=*/false);
            return true;
        };

        // GRASS PATCHES
        {
            // Coarse seeds to form clumps
            const int STEP = 4;
            for (int lx = 0; lx < CHUNK_SIZE; lx += STEP) {
                for (int lz = 0; lz < CHUNK_SIZE; lz += STEP) {
                    int wx = baseX + lx;
                    int wz = baseZ + lz;

                    // Clumpy mask for grass
                    double n = perlin.noise(wx * 0.06, 321.0, wz * 0.06) * 0.5 + 0.5; // [0,1]
                    if (n < 0.50) continue;
                    if (r01(rng) > 0.65f) continue;

                    int blades = 8 + (int)std::round(r01(rng) * 12.0f);
                    for (int i = 0; i < blades; ++i) {
                        int px = wx + jitter(rng);
                        int pz = wz + jitter(rng);
                        placePlantIfValid(px, pz, BLOCK_TALL_GRASS);
                    }
                }
            }

            // Sparse singles to break up gaps
            for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                    if (r01(rng) > 0.04f) continue;
                    int wx = baseX + lx;
                    int wz = baseZ + lz;
                    placePlantIfValid(wx, wz, BLOCK_TALL_GRASS);
                }
            }
        }

        // FLOWER CLUSTERS
        {
            // Cluster seeds more spaced out than grass
            const int STEP = 8;
            std::uniform_int_distribution<int> clusterRadiusDist(2, 3);
            std::uniform_int_distribution<int> clusterCountDist(3, 7);
            std::uniform_int_distribution<int> jitterSmall(-1, 1);

            for (int lx = 0; lx < CHUNK_SIZE; lx += STEP) {
                for (int lz = 0; lz < CHUNK_SIZE; lz += STEP) {
                    int wx = baseX + lx;
                    int wz = baseZ + lz;

                    double n = perlin.noise(wx * 0.03, 777.0, wz * 0.03) * 0.5 + 0.5; // [0,1]
                    if (!(n > 0.62 && r01(rng) < 0.35f)) continue;

                    // Choose flower type for this cluster (same type within cluster)
                    BlockType flowerType = (r01(rng) < 0.5f) ? BLOCK_ORANGE_FLOWER : BLOCK_BLUE_FLOWER;

                    int clusterCount  = clusterCountDist(rng);
                    int clusterRadius = clusterRadiusDist(rng);

                    for (int i = 0; i < clusterCount; ++i) {
                        int ox = wx + jitterSmall(rng);
                        int oz = wz + jitterSmall(rng);
                        float ang = r01(rng) * 6.2831853f;
                        float r   = (float)clusterRadius * std::sqrt(r01(rng));
                        int px = ox + (int)std::round(std::cos(ang) * r);
                        int pz = oz + (int)std::round(std::sin(ang) * r);

                        placePlantIfValid(px, pz, flowerType);
                    }
                }
            }

            // Very sparse single flowers
            for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                    if (r01(rng) > 0.0075f) continue;
                    int wx = baseX + lx;
                    int wz = baseZ + lz;
                    BlockType flowerType = (r01(rng) < 0.5f) ? BLOCK_ORANGE_FLOWER : BLOCK_BLUE_FLOWER;
                    placePlantIfValid(wx, wz, flowerType);
                }
            }
        }
    }

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

    void generateSandPatchesForColumn(int cx, int cz) {
        // Deterministic RNG per (cx,cz)
        unsigned int seed = static_cast<unsigned int>(PERLIN_SEED) ^ (cx * 0x7F4A7C15u) ^ (cz * 0x5BC7E75Du) ^ 0x6A09E667u;
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> r01(0.0f, 1.0f);

        const int baseX = cx * CHUNK_SIZE;
        const int baseZ = cz * CHUNK_SIZE;

        int sandPatchesGenerated = 0;
        int underwaterFloorsFound = 0;

        // Generate scattered sand patch centers
        const int STEP = 6; // Check every 6 blocks for potential patch center
        for (int lx = 0; lx < CHUNK_SIZE; lx += STEP) {
            for (int lz = 0; lz < CHUNK_SIZE; lz += STEP) {
                int wx = baseX + lx;
                int wz = baseZ + lz;

                // Use noise to create more natural distribution
                double noise = perlin.noise(wx * 0.05, 555.0, wz * 0.05) * 0.5 + 0.5; // [0,1]
                
                // Higher spawn chance for testing
                float spawnChance = 0.15f + static_cast<float>(0.25 * noise);
                if (r01(rng) > spawnChance) continue;

                // Find the underwater floor at this position
                int floorY = findUnderwaterFloorY(wx, 0, WATER_LEVEL - 1, wz);
                if (floorY < 0) continue;

                underwaterFloorsFound++;

                // Verify this is actually underwater
                const Block* above = getBlockAt(wx, floorY + 1, wz);
                if (!above || above->type != BLOCK_WATER) continue;

                // Random radius for this sand patch (2-5 blocks)
                std::uniform_real_distribution<float> radiusDist(2.0f, 5.0f);
                float radius = radiusDist(rng);
                float radiusSq = radius * radius;

                sandPatchesGenerated++;

                // Paint sand in a circular pattern
                int minX = static_cast<int>(std::floor(wx - radius));
                int maxX = static_cast<int>(std::floor(wx + radius));
                int minZ = static_cast<int>(std::floor(wz - radius));
                int maxZ = static_cast<int>(std::floor(wz + radius));

                for (int px = minX; px <= maxX; ++px) {
                    for (int pz = minZ; pz <= maxZ; ++pz) {
                        if (px < 0 || px >= WORLD_SIZE_X || pz < 0 || pz >= WORLD_SIZE_Z) continue;

                        // Check if within circle
                        float dx = px - wx;
                        float dz = pz - wz;
                        float distSq = dx * dx + dz * dz;
                        if (distSq > radiusSq) continue;

                        // Find the underwater floor at this position
                        int localFloorY = findUnderwaterFloorY(px, 0, WATER_LEVEL - 1, pz);
                        if (localFloorY < 0) continue;

                        // Verify this position is underwater
                        const Block* waterAbove = getBlockAt(px, localFloorY + 1, pz);
                        if (!waterAbove || waterAbove->type != BLOCK_WATER) continue;

                        // Get the floor block
                        Block* floorBlock = getBlockAt(px, localFloorY, pz);
                        if (!floorBlock || !floorBlock->isSolid) continue;

                        // Place sand (only on stone or dirt, not on existing sand or special blocks)
                        if (floorBlock->type == BLOCK_STONE || floorBlock->type == BLOCK_DIRT || floorBlock->type == BLOCK_GRASS) {
                            setBlockAndMarkDirty(px, localFloorY, pz, BLOCK_SAND, true);
                        }
                    }
                }
            }
        }
    }

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
                // Convert stone to dirt, but preserve sand
                if (b->type == BLOCK_STONE) {
                    setBlockAndMarkDirty(wx, floorY, wz, BLOCK_DIRT, true);
                }
            }
        }
    }

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

            Block* start = getBlockAt(sx, sy, sz);
            if (!start || !start->isSolid || start->type != BLOCK_STONE) continue;

            int steps = sizeDist(rng);
            float px = static_cast<float>(sx);
            float py = static_cast<float>(sy);
            float pz = static_cast<float>(sz);

            for (int i = 0; i < steps; ++i) {
                float radius = 0.9f + 0.9f * r01(rng);
                paintOreSphere(oreType, cx, cy, cz, static_cast<int>(std::round(px)), static_cast<int>(std::round(py)), static_cast<int>(std::round(pz)), radius);

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
    
    bool isChunkInBounds(int cx, int cy, int cz) const {
        return cx >= 0 && cx < WORLD_CHUNK_SIZE_X && cy >= 0 && cy < WORLD_CHUNK_SIZE_Y && cz >= 0 && cz < WORLD_CHUNK_SIZE_Z;
    }
    
    Chunk* getChunk(int cx, int cy, int cz) {
        if (!isChunkInBounds(cx, cy, cz)) return nullptr;
        ChunkCoord coord{cx, cy, cz};
        auto it = chunks.find(coord);
        if (it == chunks.end()) {
            auto chunk = std::make_unique<Chunk>(cx, cy, cz);
            Chunk* ptr = chunk.get();
            chunks[coord] = std::move(chunk);
            return ptr;
        }
        return it->second.get();
    }
    
    bool isChunkLoaded(int cx, int cy, int cz) const {
        ChunkCoord coord{cx, cy, cz};
        return chunks.find(coord) != chunks.end();
    }
    
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
    
    int getHeightAt(int x, int z) const {
        // --- Domain warp (unchanged) ---
        static const double WARP_FREQ = 0.0020;
        static const double WARP_AMPLITUDE = 80.0;

        double warpNoise1 = perlin.noise(x * WARP_FREQ, z * WARP_FREQ);
        double warpNoise2 = perlin.noise(x * WARP_FREQ + 1000.0, z * WARP_FREQ + 1000.0);
        double warpX = warpNoise1 * WARP_AMPLITUDE;
        double warpZ = warpNoise2 * WARP_AMPLITUDE;
        double warpedX = x + warpX;
        double warpedZ = z + warpZ;

        // rotate into 3D for fewer artifacts
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
            return total / maxAmp; // [0,1]
        };

        constexpr double HILL_BAND_SCALE = 1.0;
        const double continental = fbmNoise(0.005 * HILL_BAND_SCALE, 3); // large scale
        const double hills       = fbmNoise(0.012 * HILL_BAND_SCALE, 5); // medium
        const double detail      = fbmNoise(0.02,  2);                   // small

        // ---- More hills everywhere (but still masked later) ----
        // was: 0.55*hills + 0.40*continental
        double rawHeight = 0.65 * hills + 0.30 * continental + 0.05 * detail; // [0,1]

        auto smoothstep = [](double a, double b, double x) {
            double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
            return t * t * (3.0 - 2.0 * t);
        };

        auto softTerrace = [&](double v, double steps, double softness) {
            double u  = v * steps;
            double f  = u - std::floor(u);
            double u2 = std::floor(u) + smoothstep(0.3, 0.7, f);
            return lerp_fallback(v, u2 / steps, softness);
        };

        // Terracing for plateaus, based on the new rawHeight
        double heightFactor = softTerrace(rawHeight, 12.0, 0.10);

        // Valley stretching – slightly weaker than before so hills occupy more range
        double valleyStretch = 1.9; // was 2.2
        heightFactor = std::clamp(std::pow(heightFactor, valleyStretch), 0.0, 1.0);

        // -------- Deeper low areas (for lakes) – same logic as before --------
        double valleyMask = 1.0 - smoothstep(0.20, 0.50, rawHeight);
        double extraValleyDepth = 0.25; // keep your deep lakes
        heightFactor = std::clamp(heightFactor - valleyMask * extraValleyDepth, 0.0, 1.0);
        // ---------------------------------------------------------------------

        // --- More area counted as "hilly/mountainous" ---
        // lower thresholds => more hills & mountains
        double mountainSelector = 0.6 * hills + 0.4 * continental;
        double mountainMask = smoothstep(0.50, 0.80, mountainSelector); // was 0.58..0.82

        // hillBoost: 0 on plains, 1 on strong mountains
        double hillBoost = smoothstep(0.35, 1.0, mountainMask);

        // rollingMask: kicks in earlier for “hilly” zones, including mid areas
        double rollingMask = smoothstep(0.20, 0.60, mountainMask); // 0 on flats, ~1 on hills+

        // Base vertical scaling (your original idea)
        double baseMinScale = 4.0;
        double baseMaxScale = 18.0;
        double baseScale    = lerp_fallback(baseMinScale, baseMaxScale, mountainMask);

        // ---- Make hills more common and mountains taller ----
        //
        // plains (~0): rollingMask≈0, hillBoost≈0 -> multiplier ≈ 1.0
        // hills (mid): rollingMask>0, hillBoost small -> +40% height
        // big mountains: hillBoost→1 -> up to ~2.3x base scale
        double rollingContribution = 0.4 * rollingMask; // more “bumpy” midlands
        double mountainContribution = 1.2 * hillBoost;  // very tall peaks
        double verticalScaleMul = baseScale * (1.0 + rollingContribution + mountainContribution);
        // -----------------------------------------------------

        double scaledHeight = heightFactor * (TERRAIN_HEIGHT_SCALE * verticalScaleMul) + 10.0;

        int height = static_cast<int>(scaledHeight);
        if (height >= WORLD_SIZE_Y) height = WORLD_SIZE_Y - 1;
        if (height < 5) height = 5;
        return height;
    }



    void generateChunk(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || chunk->isGenerated) return;
        
        int baseX = cx * CHUNK_SIZE;
        int baseY = cy * CHUNK_HEIGHT;
        int baseZ = cz * CHUNK_SIZE;
        
        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                int worldX = baseX + lx;
                int worldZ = baseZ + lz;
                int columnHeight = getHeightAt(worldX, worldZ);
                
                for (int ly = 0; ly < CHUNK_HEIGHT; ++ly) {
                    int worldY = baseY + ly;
                    Block& block = chunk->blocks[lx][ly][lz];

                    if (worldY == 0) {
                        block.isSolid = true;
                        block.type = BLOCK_BEDROCK;
                        continue;
                    }

                    if (worldY <= columnHeight && worldY < WORLD_SIZE_Y) {
                        block.isSolid = true;
                        // Mountain peaks (above transition start) should be all stone
                        // Only generate dirt layers on lower terrain
                        if (columnHeight >= SNOW_TRANSITION_START) {
                            // High mountains - all stone
                            block.type = BLOCK_STONE;
                        } else if (worldY >= columnHeight - 3) {
                            // Normal terrain - dirt on top
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
                    if (worldY >= WATER_LEVEL) continue;
                    
                    Block& block = chunk->blocks[lx][ly][lz];
                    if (!block.isSolid) {
                        block.isSolid = true;
                        block.type = BLOCK_WATER;
                        waterAdded = true;
                    }
                }
            }
        }
        
        if (waterAdded) chunk->isDirty = true;
    }

    void generateCaves(int cx, int cy, int cz) {
        if (!isChunkInBounds(cx, cy, cz)) return;
        Chunk* baseChunk = getChunk(cx, cy, cz);
        if (!baseChunk || !baseChunk->isGenerated || baseChunk->isFullyProcessed) return;

        unsigned int caveSeed = static_cast<unsigned int>(PERLIN_SEED) ^ (cx * 0xA511E9B3u) ^ (cz * 0x63288691u);
        std::mt19937 caveRng(caveSeed);
        std::uniform_real_distribution<double> rand01(0.0, 1.0);

        int caveCount;
        double chance = rand01(caveRng);
        if (chance < 0.1) caveCount = 0;
        else if (chance < 0.6) caveCount = 1;
        else if (chance < 0.9) caveCount = 2;
        else caveCount = 3;
        if (caveCount <= 0) return;

        auto getSurfaceY = [&](int wx, int wz) { return getHeightAt(wx, wz); };
        
        // Helper function to check if there's water above a position (within a reasonable distance)
        auto hasWaterAbove = [&](int wx, int wy, int wz) -> bool {
            // Check up to 3 blocks above for water
            for (int checkY = wy + 1; checkY <= wy + 3 && checkY < WORLD_SIZE_Y; ++checkY) {
                const Block* checkBlock = getBlockAt(wx, checkY, wz);
                if (checkBlock && checkBlock->type == BLOCK_WATER) {
                    return true;
                }
                // If we hit a solid non-water block, stop checking (water can't be above it)
                if (checkBlock && checkBlock->isSolid && checkBlock->type != BLOCK_WATER) {
                    break;
                }
            }
            return false;
        };

        std::function<void(double,double,double,double,double,double,int,bool)> carveTunnel =
        [&](double x, double y, double z, double yaw, double pitch, double radius, int steps, bool allowBranch) {
            const double stepSize = 0.5;
            for (int i = 0; i < steps; ++i) {
                x += std::cos(yaw) * std::cos(pitch) * stepSize;
                y += std::sin(pitch) * stepSize;
                z += std::sin(yaw) * std::cos(pitch) * stepSize;

                if (x < 0 || x >= WORLD_SIZE_X || y < 0 || y >= WORLD_SIZE_Y || z < 0 || z >= WORLD_SIZE_Z) break;

                int minX = static_cast<int>(std::floor(x - radius));
                int maxX = static_cast<int>(std::floor(x + radius));
                int minY = static_cast<int>(std::floor(y - radius));
                int maxY = static_cast<int>(std::floor(y + radius));
                int minZ = static_cast<int>(std::floor(z - radius));
                int maxZ = static_cast<int>(std::floor(z + radius));
                double radiusSq = radius * radius;

                for (int xi = minX; xi <= maxX; ++xi) {
                    for (int yi = minY; yi <= maxY; ++yi) {
                        for (int zi = minZ; zi <= maxZ; ++zi) {
                            if (xi < 0 || xi >= WORLD_SIZE_X || yi < 0 || yi >= WORLD_SIZE_Y || zi < 0 || zi >= WORLD_SIZE_Z) continue;

                            double dx = xi + 0.5 - x;
                            double dy = yi + 0.5 - y;
                            double dz = zi + 0.5 - z;
                            if ((dx*dx + dy*dy + dz*dz) > radiusSq) continue;

                            int cx2 = xi / CHUNK_SIZE;
                            int cy2 = yi / CHUNK_HEIGHT;
                            int cz2 = zi / CHUNK_SIZE;
                            if (!isChunkLoaded(cx2, cy2, cz2)) continue;
                            Block* block = getBlockAt(xi, yi, zi);
                            if (!block) continue;

                            if (block->isSolid) {
                                if (block->type != BLOCK_WATER) {
                                    // Don't carve if there's water above (prevents caves exposed to water)
                                    if (!hasWaterAbove(xi, yi, zi)) {
                                        block->isSolid = false;
                                        markChunkDirty(cx2, cy2, cz2);
                                    }
                                }
                            }
                        }
                    }
                }

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

                yaw += std::uniform_real_distribution<double>(-0.1, 0.1)(caveRng);
                pitch += std::uniform_real_distribution<double>(-0.05, 0.05)(caveRng);
                if (pitch < -1.5) pitch = -1.5;
                if (pitch > 1.5)  pitch = 1.5;
            }
        };

        std::uniform_int_distribution<int> xLocalDist(0, CHUNK_SIZE - 1);
        std::uniform_int_distribution<int> zLocalDist(0, CHUNK_SIZE - 1);

        for (int c = 0; c < caveCount; ++c) {
            int lx = xLocalDist(caveRng);
            int lz = zLocalDist(caveRng);
            int wx = cx * CHUNK_SIZE + lx;
            int wz = cz * CHUNK_SIZE + lz;

            int surfaceY = getSurfaceY(wx, wz);
            if (surfaceY < 5) surfaceY = 5;
            std::uniform_int_distribution<int> yDist(5, surfaceY);
            int wy = yDist(caveRng);

            bool spaghetti = (rand01(caveRng) < 0.5);
            double caveRadius = spaghetti ? std::uniform_real_distribution<double>(1.5, 2.5)(caveRng) : std::uniform_real_distribution<double>(1.0, 1.4)(caveRng);

            int lengthSteps = spaghetti ? 2 * std::uniform_int_distribution<int>(40, 80)(caveRng)
                                        : 2 * std::uniform_int_distribution<int>(15, 40)(caveRng);

            double yawAngle   = std::uniform_real_distribution<double>(0.0, 2*M_PI)(caveRng);
            double pitchAngle = std::uniform_real_distribution<double>(-0.2, 0.2)(caveRng);

            carveTunnel(wx + 0.5, wy + 0.5, wz + 0.5, yawAngle, pitchAngle, caveRadius, lengthSteps, true);
        }

        updateCaveExposedSurfacesForColumn(cx, cz);
    }

    void updateCaveExposedSurfacesForColumn(int cx, int cz) {
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

    void generateOres(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || !chunk->isGenerated) return;

        generateOreVeinsForType(cx, cy, cz,
                                BLOCK_COAL_ORE,
                                COAL_ORE_MIN_Y, COAL_ORE_MAX_Y,
                                2, 5,
                                3, 14);

        generateOreVeinsForType(cx, cy, cz,
                                BLOCK_IRON_ORE,
                                IRON_ORE_MIN_Y, IRON_ORE_MAX_Y,
                                2, 4,
                                2, 12);
    }
    
    void updateSurfaceBlocks(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || !chunk->isGenerated) return;

        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
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

                if (topBlock.type == BLOCK_WATER ||
                    topBlock.type == BLOCK_LEAVES ||
                    topBlock.type == BLOCK_LOG) {
                    continue;
                }

                bool airAbove = (worldY + 1 >= WORLD_SIZE_Y) ? true : !isSolidAt(worldX, worldY + 1, worldZ);
                if (!airAbove) continue;

                // ===== MOUNTAIN PEAK SNOW LOGIC =====
                // Calculate the snow/stone transition based on height
                // We use multiple noise layers for more natural variation
                double transitionNoise = perlin.noise(worldX * 0.08, worldY * 0.12, worldZ * 0.08) * 0.5 + 0.5; // [0,1]
                double detailNoise = perlin.noise(worldX * 0.25, worldY * 0.3, worldZ * 0.25) * 0.5 + 0.5; // [0,1]
                double combinedNoise = (transitionNoise * 0.7 + detailNoise * 0.3);
                
                // Full snow zone (above FULL_SNOW_HEIGHT)
                if (worldY >= FULL_SNOW_HEIGHT) {
                    if (topBlock.type == BLOCK_STONE || topBlock.type == BLOCK_DIRT || topBlock.type == BLOCK_GRASS) {
                        topBlock.type = BLOCK_SNOW;
                    }
                }
                // Snow fade-in zone (SNOW_FADE_START to FULL_SNOW_HEIGHT)
                else if (worldY >= SNOW_FADE_START) {
                    // Calculate probability of snow vs stone based on height
                    double heightFactor = (worldY - SNOW_FADE_START) / (double)(FULL_SNOW_HEIGHT - SNOW_FADE_START);
                    heightFactor = std::clamp(heightFactor, 0.0, 1.0);
                    
                    // Combine height with noise for natural variation (heavy noise influence)
                    double snowProbability = heightFactor * 0.4 + combinedNoise * 0.6;
                    
                    // Add some vertical variation - offset the threshold based on noise
                    double noiseOffset = (combinedNoise - 0.5) * 6.0; // +/- 3 blocks variation
                    double adjustedHeight = worldY + noiseOffset;
                    
                    if (adjustedHeight >= SNOW_FADE_START + (FULL_SNOW_HEIGHT - SNOW_FADE_START) * (1.0 - combinedNoise * 0.5)) {
                        if (topBlock.type == BLOCK_STONE || topBlock.type == BLOCK_DIRT || topBlock.type == BLOCK_GRASS) {
                            topBlock.type = BLOCK_SNOW;
                        }
                    } else {
                        if (topBlock.type == BLOCK_DIRT || topBlock.type == BLOCK_GRASS) {
                            topBlock.type = BLOCK_STONE;
                        }
                    }
                }
                // Grass fade-out zone (SNOW_TRANSITION_START to SNOW_FADE_START)
                else if (worldY >= SNOW_TRANSITION_START) {
                    // Calculate probability of stone vs grass/dirt based on height
                    double heightFactor = (worldY - SNOW_TRANSITION_START) / (double)(SNOW_FADE_START - SNOW_TRANSITION_START);
                    heightFactor = std::clamp(heightFactor, 0.0, 1.0);
                    
                    // Heavy noise influence for natural patches
                    double stoneProbability = heightFactor * 0.5 + combinedNoise * 0.5;
                    
                    // Add vertical noise offset
                    double noiseOffset = (combinedNoise - 0.5) * 4.0; // +/- 2 blocks variation
                    double adjustedHeight = worldY + noiseOffset;
                    
                    if (adjustedHeight >= SNOW_TRANSITION_START + (SNOW_FADE_START - SNOW_TRANSITION_START) * (1.0 - combinedNoise * 0.4)) {
                        if (topBlock.type == BLOCK_DIRT || topBlock.type == BLOCK_GRASS) {
                            topBlock.type = BLOCK_STONE;
                        }
                    } else {
                        // Normal grass/dirt behavior
                        if (topBlock.type == BLOCK_DIRT) topBlock.type = BLOCK_GRASS;
                    }
                }
                // Normal zone (below SNOW_TRANSITION_START)
                else {
                    if (topBlock.type == BLOCK_DIRT) topBlock.type = BLOCK_GRASS;
                    if (topBlock.type != BLOCK_GRASS) continue;
                }

                // Convert layers below surface
                // For normal terrain (below mountain transition) - grass with dirt underneath
                if (worldY < SNOW_TRANSITION_START && topBlock.type == BLOCK_GRASS) {
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
                // For mountain peaks with snow - thin snow layer on solid stone
                else if (worldY >= SNOW_FADE_START && topBlock.type == BLOCK_SNOW) {
                    // Very thin snow layer (1-2 blocks), directly on stone
                    for (int i = 1; i <= 2; ++i) {
                        int yBelow = topYLocal - i;
                        if (yBelow < 0) break;
                        Block& b = chunk->blocks[x][yBelow][z];
                        if (b.isSolid &&
                            b.type != BLOCK_BEDROCK &&
                            b.type != BLOCK_WATER &&
                            b.type != BLOCK_LOG &&
                            b.type != BLOCK_LEAVES) {
                            // Very thin snow accumulation
                            double snowDepthNoise = perlin.noise(worldX * 0.2, (worldY - i) * 0.15, worldZ * 0.2) * 0.5 + 0.5;
                            if (i == 1 && snowDepthNoise > 0.4) {
                                b.type = BLOCK_SNOW;
                            } else {
                                b.type = BLOCK_STONE;
                            }
                        }
                    }
                    // Everything else is stone
                    for (int i = 3; i <= 12; ++i) {
                        int yDeep = topYLocal - i;
                        if (yDeep < 0) break;
                        Block& b = chunk->blocks[x][yDeep][z];
                        if (!b.isSolid) break;
                        if (b.type == BLOCK_LOG || b.type == BLOCK_LEAVES) break;
                        // Convert any dirt or grass to stone
                        if (b.type == BLOCK_DIRT || b.type == BLOCK_GRASS || b.type == BLOCK_SNOW) {
                            b.type = BLOCK_STONE;
                        }
                    }
                }
                // For mountain transition zones with exposed stone - also no dirt
                else if (worldY >= SNOW_TRANSITION_START && topBlock.type == BLOCK_STONE) {
                    // Mountain stone surfaces should be solid stone underneath
                    for (int i = 1; i <= 12; ++i) {
                        int yDeep = topYLocal - i;
                        if (yDeep < 0) break;
                        Block& b = chunk->blocks[x][yDeep][z];
                        if (!b.isSolid) break;
                        if (b.type == BLOCK_LOG || b.type == BLOCK_LEAVES) break;
                        // Convert any dirt or grass to stone
                        if (b.type == BLOCK_DIRT || b.type == BLOCK_GRASS) {
                            b.type = BLOCK_STONE;
                        }
                    }
                }
            }
        }
    }
    
public:
    void loadChunksAroundPosition(float worldX, float worldZ, float playerX = -9999.0f, float playerY = -9999.0f, float playerZ = -9999.0f) {
        int centerChunkX = static_cast<int>(std::floor(worldX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(worldZ / CHUNK_SIZE));
        int playerChunkX = static_cast<int>(std::floor(playerX / CHUNK_SIZE));
        int playerChunkZ = static_cast<int>(std::floor(playerZ / CHUNK_SIZE));
        bool hasPlayerPos = (playerX > -9000.0f);
        
        // Pass 1: base terrain
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
        
        // Pass 2: water
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
        
        // Pass 2.5: sand patches (after water, before underwater floor normalization)
        for (int cx = centerChunkX - CHUNK_LOAD_DISTANCE; cx <= centerChunkX + CHUNK_LOAD_DISTANCE; ++cx) {
            for (int cz = centerChunkZ - CHUNK_LOAD_DISTANCE; cz <= centerChunkZ + CHUNK_LOAD_DISTANCE; ++cz) {
                int dx = cx - centerChunkX;
                int dz = cz - centerChunkZ;
                if (dx * dx + dz * dz > CHUNK_LOAD_DISTANCE * CHUNK_LOAD_DISTANCE) continue;
                if (!isChunkInBounds(cx, 0, cz)) continue;

                // Generate sand patches for this column
                generateSandPatchesForColumn(cx, cz);
            }
        }
        
        // Pass 2.75: normalize underwater floor to dirt (after sand patches)
        for (int cx = centerChunkX - CHUNK_LOAD_DISTANCE; cx <= centerChunkX + CHUNK_LOAD_DISTANCE; ++cx) {
            for (int cz = centerChunkZ - CHUNK_LOAD_DISTANCE; cz <= centerChunkZ + CHUNK_LOAD_DISTANCE; ++cz) {
                int dx = cx - centerChunkX;
                int dz = cz - centerChunkZ;
                if (dx * dx + dz * dz > CHUNK_LOAD_DISTANCE * CHUNK_LOAD_DISTANCE) continue;
                if (!isChunkInBounds(cx, 0, cz)) continue;

                for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                    if (!isChunkInBounds(cx, cy, cz)) continue;
                    Chunk* chunk = getChunk(cx, cy, cz);
                    if (chunk && chunk->isGenerated && !chunk->isFullyProcessed) normaliseUnderwaterFloorToDirt(cx, cy, cz);
                }
            }
        }
        
        // Pass 3: surface blocks and trees/foliage (before caves so they can be carved properly)
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
                        if (cy == WORLD_CHUNK_SIZE_Y - 1) {
                            generateTreesForColumn(cx, cz);
                            generateFoliageForColumn(cx, cz);
                        }
                    }
                }
            }
        }
        
        // Pass 4: caves (after surface features so they carve through everything cleanly)
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
        
        // Pass 5: ores (after caves)
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
        
        // Pass 6: finalize all chunks
        for (int cx = centerChunkX - CHUNK_LOAD_DISTANCE; cx <= centerChunkX + CHUNK_LOAD_DISTANCE; ++cx) {
            for (int cz = centerChunkZ - CHUNK_LOAD_DISTANCE; cz <= centerChunkZ + CHUNK_LOAD_DISTANCE; ++cz) {
                int dx = cx - centerChunkX;
                int dz = cz - centerChunkZ;
                if (dx * dx + dz * dz > CHUNK_LOAD_DISTANCE * CHUNK_LOAD_DISTANCE) continue;
                if (!isChunkInBounds(cx, 0, cz)) continue;

                for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                    Chunk* chunk = getChunk(cx, cy, cz);
                    if (chunk && chunk->isGenerated && !chunk->isFullyProcessed) {
                        chunk->isFullyProcessed = true;
                    }
                }
            }
        }
    }
    
    void unloadDistantChunks(float worldX, float worldZ) {
        int centerChunkX = static_cast<int>(std::floor(worldX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(worldZ / CHUNK_SIZE));
        std::vector<ChunkCoord> toUnload;

        for (const auto& coord : loadedChunks) {
            int dx = coord.x - centerChunkX;
            int dz = coord.z - centerChunkZ;
            int distSq = dx * dx + dz * dz;
            if (distSq > (CHUNK_LOAD_DISTANCE + 2) * (CHUNK_LOAD_DISTANCE + 2)) toUnload.push_back(coord);
        }

        for (const auto& coord : toUnload) loadedChunks.erase(coord);
    }
    
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

        if (it == chunks.end() || !it->second->isGenerated) return false;
        return it->second->blocks[bx][by][bz].isSolid;
    }
    
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
    
    bool isOpaque(int x, int y, int z) const {
        if (x < 0 || x >= WORLD_SIZE_X || 
            y < 0 || y >= WORLD_SIZE_Y || 
            z < 0 || z >= WORLD_SIZE_Z) {
            return false;
        }
        const Block* block = getBlockAt(x, y, z);
        if (!block || !block->isSolid) return false;
        return block->type != BLOCK_WATER && block->type != BLOCK_LEAVES && block->type != BLOCK_GLASS;
    }
    
    void markChunkDirty(int cx, int cy, int cz) {
        if (Chunk* chunk = getChunk(cx, cy, cz)) {
            bool wasDirty = chunk->isDirty;
            chunk->isDirty = true;
        } else {
            std::cout << "[WORLD] WARNING: Chunk (" << cx << "," << cy << "," << cz << ") not found!" << std::endl;
        }
        if (Chunk* c = getChunk(cx - 1, cy, cz)) c->isDirty = true;
        if (Chunk* c = getChunk(cx + 1, cy, cz)) c->isDirty = true;
        if (Chunk* c = getChunk(cx, cy - 1, cz)) c->isDirty = true;
        if (Chunk* c = getChunk(cx, cy + 1, cz)) c->isDirty = true;
        if (Chunk* c = getChunk(cx, cy, cz - 1)) c->isDirty = true;
        if (Chunk* c = getChunk(cx, cy, cz + 1)) c->isDirty = true;
    }

    // Mark the chunk containing the block as dirty, and only mark bordering neighbours when the edit touches an edge.
    void markChunkDirtyForBlock(int wx, int wy, int wz) {
        if (wx < 0 || wx >= WORLD_SIZE_X || wy < 0 || wy >= WORLD_SIZE_Y || wz < 0 || wz >= WORLD_SIZE_Z) return;
        int cx = wx / CHUNK_SIZE;
        int cy = wy / CHUNK_HEIGHT;
        int cz = wz / CHUNK_SIZE;

        // Always mark the owning chunk.
        if (Chunk* chunk = getChunk(cx, cy, cz)) {
            chunk->isDirty = true;
        }

        const int localX = wx % CHUNK_SIZE;
        const int localY = wy % CHUNK_HEIGHT;
        const int localZ = wz % CHUNK_SIZE;

        // Touch neighbours only when on the border to avoid unnecessary rebuilds.
        if (localX == 0)        if (Chunk* c = getChunk(cx - 1, cy, cz)) c->isDirty = true;
        if (localX == CHUNK_SIZE - 1) if (Chunk* c = getChunk(cx + 1, cy, cz)) c->isDirty = true;
        if (localY == 0)        if (Chunk* c = getChunk(cx, cy - 1, cz)) c->isDirty = true;
        if (localY == CHUNK_HEIGHT - 1) if (Chunk* c = getChunk(cx, cy + 1, cz)) c->isDirty = true;
        if (localZ == 0)        if (Chunk* c = getChunk(cx, cy, cz - 1)) c->isDirty = true;
        if (localZ == CHUNK_SIZE - 1) if (Chunk* c = getChunk(cx, cy, cz + 1)) c->isDirty = true;
    }
    
    const std::unordered_set<ChunkCoord, std::hash<ChunkCoord>>& getLoadedChunks() const {
        return loadedChunks;
    }
    
    const std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>>& getChunks() const {
        return chunks;
    }

    // Networking methods for future client/server architecture
    // Populate a chunk from raw arrays (e.g., received from server)
    void setChunkData(int cx, int cy, int cz, const BlockType types[], const bool solids[]) {
        if (!isChunkInBounds(cx, cy, cz)) return;
        
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk) return;
        
        int idx = 0;
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    chunk->blocks[x][y][z].type = types[idx];
                    chunk->blocks[x][y][z].isSolid = solids[idx];
                    ++idx;
                }
            }
        }
        
        chunk->isGenerated = true;
        chunk->isDirty = true;
        chunk->isFullyProcessed = true;
        
        // Mark neighboring chunks as dirty so they regenerate their boundary faces
        markChunkDirty(cx, cy, cz);
        
        // Add to loaded chunks set so updateDirtyChunks can find it
        ChunkCoord coord{cx, cy, cz};
        loadedChunks.insert(coord);
    }
    
    // Erase a chunk and notify mesh manager (caller should call meshManager.removeChunkMesh)
    void eraseChunk(int cx, int cy, int cz) {
        ChunkCoord coord{cx, cy, cz};
        chunks.erase(coord);
        loadedChunks.erase(coord);
    }
};

#endif // WORLD_GENERATION_HPP
