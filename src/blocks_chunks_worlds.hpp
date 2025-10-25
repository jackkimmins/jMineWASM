// blocks_chunks_worlds.hpp
#ifndef BLOCKS_CHUNKS_WORLDS_HPP
#define BLOCKS_CHUNKS_WORLDS_HPP

#include <memory>
#include <unordered_map>
#include <unordered_set>

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

// Chunk coordinate hash function for unordered_map
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
    bool isFullyProcessed = false; // Has ores, caves, and surface blocks updated
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
    
    // Check if chunk exists in the world bounds
    bool isChunkInBounds(int cx, int cy, int cz) const {
        return cx >= 0 && cx < WORLD_CHUNK_SIZE_X &&
               cy >= 0 && cy < WORLD_CHUNK_SIZE_Y &&
               cz >= 0 && cz < WORLD_CHUNK_SIZE_Z;
    }
    
    // Get or create chunk
    Chunk* getChunk(int cx, int cy, int cz) {
        if (!isChunkInBounds(cx, cy, cz)) return nullptr;
        
        ChunkCoord coord{cx, cy, cz};
        auto it = chunks.find(coord);
        
        if (it == chunks.end()) {
            // Chunk doesn't exist, create it
            auto chunk = std::make_unique<Chunk>(cx, cy, cz);
            Chunk* ptr = chunk.get();
            chunks[coord] = std::move(chunk);
            return ptr;
        }
        
        return it->second.get();
    }
    
    // Check if chunk is loaded
    bool isChunkLoaded(int cx, int cy, int cz) const {
        ChunkCoord coord{cx, cy, cz};
        return chunks.find(coord) != chunks.end();
    }
    
    // Generate a single chunk
    void generateChunk(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || chunk->isGenerated) return;
        
        // Generate terrain for this chunk
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                int worldX = cx * CHUNK_SIZE + x;
                int worldZ = cz * CHUNK_SIZE + z;
                int maxHeight = getHeightAt(worldX, worldZ);
                
                for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                    int worldY = cy * CHUNK_HEIGHT + y;
                    
                    if (worldY <= maxHeight && worldY < WORLD_SIZE_Y) {
                        chunk->blocks[x][y][z].isSolid = true;
                        
                        // Assign block types based on height
                        // Don't assign grass yet - will be done in updateSurfaceBlocks
                        if (worldY >= maxHeight - 3) {
                            chunk->blocks[x][y][z].type = BLOCK_DIRT;
                        } else if (worldY == 0) {
                            chunk->blocks[x][y][z].type = BLOCK_BEDROCK;
                        } else {
                            chunk->blocks[x][y][z].type = BLOCK_STONE;
                        }
                    }
                }
            }
        }
        
        chunk->isGenerated = true;
        chunk->isDirty = true;
    }
    
    // Generate ores for a chunk
    void generateOres(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || !chunk->isGenerated) return;
        
        std::mt19937 rng(PERLIN_SEED + cx * 73856093 + cy * 19349663 + cz * 83492791);
        std::uniform_real_distribution<float> oreChanceDist(0.0f, 1.0f);
        
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    int worldY = cy * CHUNK_HEIGHT + y;
                    Block& block = chunk->blocks[x][y][z];
                    
                    if (block.isSolid && block.type == BLOCK_STONE) {
                        // Coal Ore Generation
                        if (worldY >= COAL_ORE_MIN_Y && worldY <= COAL_ORE_MAX_Y) {
                            if (oreChanceDist(rng) < COAL_ORE_CHANCE) {
                                block.type = BLOCK_COAL_ORE;
                            }
                        }
                        
                        // Iron Ore Generation
                        if (worldY >= IRON_ORE_MIN_Y && worldY <= IRON_ORE_MAX_Y) {
                            if (oreChanceDist(rng) < IRON_ORE_CHANCE) {
                                block.type = BLOCK_IRON_ORE;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Generate caves for a chunk (carve based on 3D noise)
    void generateCaves(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || !chunk->isGenerated) return;
        
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    int worldX = cx * CHUNK_SIZE + x;
                    int worldY = cy * CHUNK_HEIGHT + y;
                    int worldZ = cz * CHUNK_SIZE + z;
                    
                    // Only generate caves in the bottom chunk layer and skip bedrock
                    if (cy != 0 || worldY <= 2 || worldY >= 14) continue;
                    
                    // Use 3D Perlin noise for cave generation with much stricter threshold
                    double caveNoise1 = perlin.noise(worldX * 0.04, worldY * 0.04, worldZ * 0.04);
                    double caveNoise2 = perlin.noise(worldX * 0.04 + 1000, worldY * 0.04 + 1000, worldZ * 0.04 + 1000);
                    
                    // Only carve caves where BOTH noise values are high (creates more sparse caves)
                    if (caveNoise1 > 0.65 && caveNoise2 > 0.65) {
                        chunk->blocks[x][y][z].isSolid = false;
                    }
                }
            }
        }
    }
    
    // Update surface blocks for a chunk
    void updateSurfaceBlocks(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (!chunk || !chunk->isGenerated) return;
        
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                int worldX = cx * CHUNK_SIZE + x;
                int worldZ = cz * CHUNK_SIZE + z;
                
                // Check each block in this chunk column
                for (int y = CHUNK_HEIGHT - 1; y >= 0; --y) {
                    int worldY = cy * CHUNK_HEIGHT + y;
                    
                    if (chunk->blocks[x][y][z].isSolid && chunk->blocks[x][y][z].type == BLOCK_DIRT) {
                        // Check if block above is air
                        bool hasAirAbove = false;
                        
                        if (y == CHUNK_HEIGHT - 1) {
                            // Check chunk above
                            int worldYAbove = worldY + 1;
                            if (worldYAbove >= WORLD_SIZE_Y) {
                                hasAirAbove = true;
                            } else {
                                hasAirAbove = !isSolidAt(worldX, worldYAbove, worldZ);
                            }
                        } else {
                            hasAirAbove = !chunk->blocks[x][y + 1][z].isSolid;
                        }
                        
                        if (hasAirAbove) {
                            chunk->blocks[x][y][z].type = BLOCK_GRASS;
                        }
                    }
                }
            }
        }
    }
    
    // Load chunks around a position
    void loadChunksAroundPosition(float worldX, float worldZ) {
        int centerChunkX = static_cast<int>(std::floor(worldX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(worldZ / CHUNK_SIZE));
        
        // First pass: Generate terrain for all chunks
        for (int cx = centerChunkX - CHUNK_LOAD_DISTANCE; cx <= centerChunkX + CHUNK_LOAD_DISTANCE; ++cx) {
            for (int cz = centerChunkZ - CHUNK_LOAD_DISTANCE; cz <= centerChunkZ + CHUNK_LOAD_DISTANCE; ++cz) {
                // Check if within cylindrical distance
                int dx = cx - centerChunkX;
                int dz = cz - centerChunkZ;
                if (dx * dx + dz * dz > CHUNK_LOAD_DISTANCE * CHUNK_LOAD_DISTANCE) continue;
                
                if (!isChunkInBounds(cx, 0, cz)) continue;
                
                // Load all Y levels for this X,Z chunk column
                for (int cy = 0; cy < WORLD_CHUNK_SIZE_Y; ++cy) {
                    if (!isChunkLoaded(cx, cy, cz)) {
                        generateChunk(cx, cy, cz);
                        ChunkCoord coord{cx, cy, cz};
                        loadedChunks.insert(coord);
                    }
                }
            }
        }
        
        // Second pass: Generate ores and caves (needs all terrain generated first)
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
                        generateCaves(cx, cy, cz);
                    }
                }
            }
        }
        
        // Third pass: Update surface blocks (needs ores and caves done)
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
    
    // Unload chunks far from position
    void unloadDistantChunks(float worldX, float worldZ) {
        int centerChunkX = static_cast<int>(std::floor(worldX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(worldZ / CHUNK_SIZE));
        
        std::vector<ChunkCoord> toUnload;
        
        for (const auto& coord : loadedChunks) {
            int dx = coord.x - centerChunkX;
            int dz = coord.z - centerChunkZ;
            int distSq = dx * dx + dz * dz;
            
            // Unload if beyond load distance + buffer
            if (distSq > (CHUNK_LOAD_DISTANCE + 2) * (CHUNK_LOAD_DISTANCE + 2)) {
                toUnload.push_back(coord);
            }
        }
        
        // This preserves player changes
        for (const auto& coord : toUnload) {
            loadedChunks.erase(coord);
            // Chunk data remains in chunks map for persistence
        }
    }
    
    // Get height at world position using Perlin noise
    int getHeightAt(int x, int z) const {
        double noiseHeight = 0.0;
        double amplitude = 1.0;
        double frequency = PERLIN_FREQUENCY;
        double maxAmplitude = 0.0;
        
        for (int i = 0; i < PERLIN_OCTAVES; ++i) {
            noiseHeight += perlin.noise(x * frequency, z * frequency) * amplitude;
            maxAmplitude += amplitude;
            amplitude *= PERLIN_PERSISTENCE;
            frequency *= PERLIN_LACUNARITY;
        }
        
        // Normalise and scale the noise
        noiseHeight = (noiseHeight / maxAmplitude);
        
        // Apply a smoother curve to the height
        // This creates more gentle hills instead of extreme peaks and valleys
        noiseHeight = noiseHeight * 0.5 + 0.5; // Map from [-1,1] to [0,1]
        
        int height = static_cast<int>(noiseHeight * TERRAIN_HEIGHT_SCALE + 20.0);
        
        if (height >= WORLD_SIZE_Y) height = WORLD_SIZE_Y - 1;
        else if (height < 5) height = 5; // Minimum height to avoid too flat areas
        
        return height;
    }
    
    // Check if block is solid at world coordinates
    bool isSolidAt(int x, int y, int z) const {
        if (x < 0 || x >= WORLD_SIZE_X || y < 0 || y >= WORLD_SIZE_Y || z < 0 || z >= WORLD_SIZE_Z) {
            return false;
        }
        
        int cx = x / CHUNK_SIZE;
        int cy = y / CHUNK_HEIGHT;
        int cz = z / CHUNK_SIZE;
        int bx = x % CHUNK_SIZE;
        int by = y % CHUNK_HEIGHT;
        int bz = z % CHUNK_SIZE;
        
        ChunkCoord coord{cx, cy, cz};
        auto it = chunks.find(coord);
        if (it == chunks.end()) return false;
        
        return it->second->blocks[bx][by][bz].isSolid;
    }
    
    // Get block at world coordinates
    Block* getBlockAt(int x, int y, int z) {
        if (x < 0 || x >= WORLD_SIZE_X || y < 0 || y >= WORLD_SIZE_Y || z < 0 || z >= WORLD_SIZE_Z) {
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
    
    // Set block dirty flag (needs mesh regeneration)
    void markChunkDirty(int cx, int cy, int cz) {
        Chunk* chunk = getChunk(cx, cy, cz);
        if (chunk) chunk->isDirty = true;
        
        // Also mark adjacent chunks dirty (for proper face culling at boundaries)
        if (Chunk* c = getChunk(cx - 1, cy, cz)) c->isDirty = true;
        if (Chunk* c = getChunk(cx + 1, cy, cz)) c->isDirty = true;
        if (Chunk* c = getChunk(cx, cy - 1, cz)) c->isDirty = true;
        if (Chunk* c = getChunk(cx, cy + 1, cz)) c->isDirty = true;
        if (Chunk* c = getChunk(cx, cy, cz - 1)) c->isDirty = true;
        if (Chunk* c = getChunk(cx, cy, cz + 1)) c->isDirty = true;
    }
    
    // Get all loaded chunks
    const std::unordered_set<ChunkCoord>& getLoadedChunks() const {
        return loadedChunks;
    }
    
    // Get chunks map
    const std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>>& getChunks() const {
        return chunks;
    }
};

#endif
