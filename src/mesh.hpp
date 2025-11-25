// mesh.hpp
#ifndef MESH_HPP
#define MESH_HPP

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <array>
#include <cmath>

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#endif

#include "../shared/config.hpp"
#include "../shared/types.hpp"
#include "../shared/chunk.hpp"
#include "frustum.hpp"

class ChunkMesh {
public:
    std::vector<float> solidVertices;
    std::vector<unsigned int> solidIndices;
    std::vector<float> plantVertices;
    std::vector<unsigned int> plantIndices;
    std::vector<float> waterVertices;
    std::vector<unsigned int> waterIndices;
    std::vector<float> glassVertices;
    std::vector<unsigned int> glassIndices;
    GLuint solidVAO, solidVBO, solidEBO;
    GLuint plantVAO, plantVBO, plantEBO;
    GLuint waterVAO, waterVBO, waterEBO;
    GLuint glassVAO, glassVBO, glassEBO;
    bool isSetup = false;
    size_t solidVertexCapacity = 0;
    size_t solidIndexCapacity = 0;
    size_t plantVertexCapacity = 0;
    size_t plantIndexCapacity = 0;
    size_t waterVertexCapacity = 0;
    size_t waterIndexCapacity = 0;
    size_t glassVertexCapacity = 0;
    size_t glassIndexCapacity = 0;
    
    ChunkMesh() {
        glGenVertexArrays(1, &solidVAO);
        glGenBuffers(1, &solidVBO);
        glGenBuffers(1, &solidEBO);
        glGenVertexArrays(1, &plantVAO);
        glGenBuffers(1, &plantVBO);
        glGenBuffers(1, &plantEBO);
        glGenVertexArrays(1, &waterVAO);
        glGenBuffers(1, &waterVBO);
        glGenBuffers(1, &waterEBO);
        glGenVertexArrays(1, &glassVAO);
        glGenBuffers(1, &glassVBO);
        glGenBuffers(1, &glassEBO);
    }
    
    void setup() {
        auto upload = [](GLuint vao, GLuint vbo, GLuint ebo, const std::vector<float>& vertices, const std::vector<unsigned int>& indices, size_t& vCap, size_t& iCap) {
            if (vertices.empty()) return;
            glBindVertexArray(vao);

            const size_t vBytes = vertices.size() * sizeof(float);
            const size_t iBytes = indices.size() * sizeof(unsigned int);

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            if (vBytes > vCap) {
                glBufferData(GL_ARRAY_BUFFER, vBytes, vertices.data(), GL_DYNAMIC_DRAW);
                vCap = vBytes;
            } else {
                glBufferSubData(GL_ARRAY_BUFFER, 0, vBytes, vertices.data());
            }

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            if (iBytes > iCap) {
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, iBytes, indices.data(), GL_DYNAMIC_DRAW);
                iCap = iBytes;
            } else {
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, iBytes, indices.data());
            }
            
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
            glBindVertexArray(0);
        };

        upload(solidVAO, solidVBO, solidEBO, solidVertices, solidIndices, solidVertexCapacity, solidIndexCapacity);
        upload(plantVAO, plantVBO, plantEBO, plantVertices, plantIndices, plantVertexCapacity, plantIndexCapacity);
        upload(waterVAO, waterVBO, waterEBO, waterVertices, waterIndices, waterVertexCapacity, waterIndexCapacity);
        upload(glassVAO, glassVBO, glassEBO, glassVertices, glassIndices, glassVertexCapacity, glassIndexCapacity);
        
        isSetup = true;
    }
    
    void drawSolid() const {
        if (!isSetup || solidIndices.empty()) return;
        glBindVertexArray(solidVAO);
        glDrawElements(GL_TRIANGLES, solidIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    
    void drawPlants() const {
        if (!isSetup || plantIndices.empty()) return;
        glBindVertexArray(plantVAO);
        glDrawElements(GL_TRIANGLES, plantIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    
    void drawWater() const {
        if (!isSetup || waterIndices.empty()) return;
        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, waterIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    
    void drawGlass() const {
        if (!isSetup || glassIndices.empty()) return;
        glBindVertexArray(glassVAO);
        glDrawElements(GL_TRIANGLES, glassIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    
    ~ChunkMesh() {
        glDeleteBuffers(1, &solidVBO);
        glDeleteBuffers(1, &solidEBO);
        glDeleteVertexArrays(1, &solidVAO);
        glDeleteBuffers(1, &plantVBO);
        glDeleteBuffers(1, &plantEBO);
        glDeleteVertexArrays(1, &plantVAO);
        glDeleteBuffers(1, &waterVBO);
        glDeleteBuffers(1, &waterEBO);
        glDeleteVertexArrays(1, &waterVAO);
        glDeleteBuffers(1, &glassVBO);
        glDeleteBuffers(1, &glassEBO);
        glDeleteVertexArrays(1, &glassVAO);
    }
};

struct CachedBlock {
    Block block;
    bool hasData = false; // false when neighbour chunk is missing/unloaded
};

struct FaceDesc {
    bool present = false;
    BlockType type = BLOCK_STONE;
    int textureIndex = 0;
    bool isWater = false;
    bool isFoliage = false;
    bool isGlass = false;
    bool lowerWaterTop = false;
    std::array<float, 4> ao = {0.f, 0.f, 0.f, 0.f};
};

// Small, cache-friendly sampler with a 1-block halo around the target chunk to remove unordered_map lookups.
struct BlockSampler {
    static constexpr int PAD = 1;
    static constexpr int SX = CHUNK_SIZE + PAD * 2;      // 16 + 2
    static constexpr int SY = CHUNK_HEIGHT + PAD * 2;    // 16 + 2
    static constexpr int SZ = CHUNK_SIZE + PAD * 2;      // 16 + 2

    std::array<CachedBlock, SX * SY * SZ> cache{};
    int baseWorldX = 0;
    int baseWorldY = 0;
    int baseWorldZ = 0;

    BlockSampler(const World& world, int cx, int cy, int cz) {
        baseWorldX = cx * CHUNK_SIZE;
        baseWorldY = cy * CHUNK_HEIGHT;
        baseWorldZ = cz * CHUNK_SIZE;

        for (int lx = -1; lx <= CHUNK_SIZE; ++lx) {
            for (int ly = -1; ly <= CHUNK_HEIGHT; ++ly) {
                for (int lz = -1; lz <= CHUNK_SIZE; ++lz) {
                    const int wx = baseWorldX + lx;
                    const int wy = baseWorldY + ly;
                    const int wz = baseWorldZ + lz;
                    CachedBlock cb{};
                    if (const Block* b = world.getBlockAt(wx, wy, wz)) {
                        cb.block = *b;
                        cb.hasData = true;
                    }
                    cache[index(lx, ly, lz)] = cb;
                }
            }
        }
    }

    bool isSolidWithFallback(int wx, int wy, int wz, bool missingSolid) const {
        const CachedBlock& cb = lookup(wx, wy, wz);
        if (!cb.hasData) return missingSolid;
        return cb.block.isSolid;
    }

    bool isSolid(int wx, int wy, int wz) const {
        return isSolidWithFallback(wx, wy, wz, false);
    }

    const Block* tryGet(int wx, int wy, int wz) const {
        const CachedBlock& cb = lookup(wx, wy, wz);
        return cb.hasData ? &cb.block : nullptr;
    }

private:
    size_t index(int lx, int ly, int lz) const {
        const int ix = lx + PAD;
        const int iy = ly + PAD;
        const int iz = lz + PAD;
        return (static_cast<size_t>(ix) * SY + static_cast<size_t>(iy)) * SZ + static_cast<size_t>(iz);
    }

    const CachedBlock& lookup(int wx, int wy, int wz) const {
        const int lx = wx - baseWorldX;
        const int ly = wy - baseWorldY;
        const int lz = wz - baseWorldZ;
        if (lx < -1 || lx > CHUNK_SIZE || ly < -1 || ly > CHUNK_HEIGHT || lz < -1 || lz > CHUNK_SIZE) {
            static CachedBlock solidBoundary{Block{true, BLOCK_STONE}, false};
            return solidBoundary;
        }
        return cache[index(lx, ly, lz)];
    }
};

class MeshManager {
public:
    std::unordered_map<ChunkCoord, std::unique_ptr<ChunkMesh>> chunkMeshes;
    std::vector<ChunkCoord> dirtyQueue;
    std::unordered_set<ChunkCoord> dirtySet;
    
    void generateChunkMesh(const World& world, int cx, int cy, int cz) {
        ChunkCoord coord{cx, cy, cz};
        auto it = world.getChunks().find(coord);
        if (it == world.getChunks().end()) return;
        
        const Chunk* chunk = it->second.get();
        if (!chunk || !chunk->isGenerated) return;
        
        auto& mesh = chunkMeshes[coord];
        if (!mesh) {
            mesh = std::make_unique<ChunkMesh>();
        }
        
        mesh->solidVertices.clear();
        mesh->solidIndices.clear();
        mesh->plantVertices.clear();
        mesh->plantIndices.clear();
        mesh->waterVertices.clear();
        mesh->waterIndices.clear();
        mesh->glassVertices.clear();
        mesh->glassIndices.clear();
        unsigned int solidIndex = 0;
        unsigned int plantIndex = 0;
        unsigned int waterIndex = 0;
        unsigned int glassIndex = 0;

        const int baseX = cx * CHUNK_SIZE;
        const int baseY = cy * CHUNK_HEIGHT;
        const int baseZ = cz * CHUNK_SIZE;

        BlockSampler sampler(world, cx, cy, cz);

        // First pass: plants (billboarded, kept separate from greedy quads).
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    const Block& block = chunk->blocks[x][y][z];
                    if (block.type == BLOCK_TALL_GRASS || block.type == BLOCK_ORANGE_FLOWER || block.type == BLOCK_BLUE_FLOWER) {
                        float wx = baseX + x;
                        float wy = baseY + y;
                        float wz = baseZ + z;
                        addBillboardPlant(*mesh, wx, wy, wz, plantIndex, block.type);
                    }
                }
            }
        }

        auto runGreedy = [&](int uCount, int vCount, auto fillMask, auto emitRect) {
            std::vector<FaceDesc> mask(uCount * vCount);
            fillMask(mask);

            for (int v = 0; v < vCount; ++v) {
                for (int u = 0; u < uCount; ) {
                    FaceDesc& fd = mask[u + v * uCount];
                    if (!fd.present) {
                        ++u;
                        continue;
                    }

                    int width = 1;
                    while (u + width < uCount && facesEqual(fd, mask[u + width + v * uCount])) {
                        ++width;
                    }

                    int height = 1;
                    bool canExpand = true;
                    while (v + height < vCount && canExpand) {
                        for (int k = 0; k < width; ++k) {
                            if (!facesEqual(fd, mask[u + k + (v + height) * uCount])) {
                                canExpand = false;
                                break;
                            }
                        }
                        if (canExpand) ++height;
                    }

                    emitRect(u, v, width, height, fd);

                    for (int dv = 0; dv < height; ++dv) {
                        for (int du = 0; du < width; ++du) {
                            mask[u + du + (v + dv) * uCount].present = false;
                        }
                    }

                    u += width;
                }
            }
        };

        // +X (right) faces
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            runGreedy(CHUNK_SIZE, CHUNK_HEIGHT, [&](std::vector<FaceDesc>& mask) {
                for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                    for (int z = 0; z < CHUNK_SIZE; ++z) {
                        int idx = z + y * CHUNK_SIZE;
                        const Block& block = chunk->blocks[x][y][z];
                        int wx = baseX + x;
                        int wy = baseY + y;
                        int wz = baseZ + z;
                        mask[idx] = buildFaceDesc(block, wx, wy, wz, FACE_RIGHT, sampler);
                    }
                }
            }, [&](int u, int v, int width, int height, const FaceDesc& fd) {
                float wx = baseX + x;
                float wy = baseY + v;
                float wz = baseZ + u;
                addGreedyQuad(*mesh, fd, FACE_RIGHT, wx, wy, wz, width, height, solidIndex, waterIndex, glassIndex);
            });
        }

        // -X (left) faces
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            runGreedy(CHUNK_SIZE, CHUNK_HEIGHT, [&](std::vector<FaceDesc>& mask) {
                for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                    for (int z = 0; z < CHUNK_SIZE; ++z) {
                        int idx = z + y * CHUNK_SIZE;
                        const Block& block = chunk->blocks[x][y][z];
                        int wx = baseX + x;
                        int wy = baseY + y;
                        int wz = baseZ + z;
                        mask[idx] = buildFaceDesc(block, wx, wy, wz, FACE_LEFT, sampler);
                    }
                }
            }, [&](int u, int v, int width, int height, const FaceDesc& fd) {
                float wx = baseX + x;
                float wy = baseY + v;
                float wz = baseZ + u;
                addGreedyQuad(*mesh, fd, FACE_LEFT, wx, wy, wz, width, height, solidIndex, waterIndex, glassIndex);
            });
        }

        // +Y (top) faces
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            runGreedy(CHUNK_SIZE, CHUNK_SIZE, [&](std::vector<FaceDesc>& mask) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    for (int x = 0; x < CHUNK_SIZE; ++x) {
                        int idx = x + z * CHUNK_SIZE;
                        const Block& block = chunk->blocks[x][y][z];
                        int wx = baseX + x;
                        int wy = baseY + y;
                        int wz = baseZ + z;
                        mask[idx] = buildFaceDesc(block, wx, wy, wz, FACE_TOP, sampler);
                    }
                }
            }, [&](int u, int v, int width, int height, const FaceDesc& fd) {
                float wx = baseX + u;
                float wy = baseY + y;
                float wz = baseZ + v;
                addGreedyQuad(*mesh, fd, FACE_TOP, wx, wy, wz, width, height, solidIndex, waterIndex, glassIndex);
            });
        }

        // -Y (bottom) faces
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            runGreedy(CHUNK_SIZE, CHUNK_SIZE, [&](std::vector<FaceDesc>& mask) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    for (int x = 0; x < CHUNK_SIZE; ++x) {
                        int idx = x + z * CHUNK_SIZE;
                        const Block& block = chunk->blocks[x][y][z];
                        int wx = baseX + x;
                        int wy = baseY + y;
                        int wz = baseZ + z;
                        mask[idx] = buildFaceDesc(block, wx, wy, wz, FACE_BOTTOM, sampler);
                    }
                }
            }, [&](int u, int v, int width, int height, const FaceDesc& fd) {
                float wx = baseX + u;
                float wy = baseY + y;
                float wz = baseZ + v;
                addGreedyQuad(*mesh, fd, FACE_BOTTOM, wx, wy, wz, width, height, solidIndex, waterIndex, glassIndex);
            });
        }

        // +Z (front) faces
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            runGreedy(CHUNK_SIZE, CHUNK_HEIGHT, [&](std::vector<FaceDesc>& mask) {
                for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                    for (int x = 0; x < CHUNK_SIZE; ++x) {
                        int idx = x + y * CHUNK_SIZE;
                        const Block& block = chunk->blocks[x][y][z];
                        int wx = baseX + x;
                        int wy = baseY + y;
                        int wz = baseZ + z;
                        mask[idx] = buildFaceDesc(block, wx, wy, wz, FACE_FRONT, sampler);
                    }
                }
            }, [&](int u, int v, int width, int height, const FaceDesc& fd) {
                float wx = baseX + u;
                float wy = baseY + v;
                float wz = baseZ + z;
                addGreedyQuad(*mesh, fd, FACE_FRONT, wx, wy, wz, width, height, solidIndex, waterIndex, glassIndex);
            });
        }

        // -Z (back) faces
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            runGreedy(CHUNK_SIZE, CHUNK_HEIGHT, [&](std::vector<FaceDesc>& mask) {
                for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                    for (int x = 0; x < CHUNK_SIZE; ++x) {
                        int idx = x + y * CHUNK_SIZE;
                        const Block& block = chunk->blocks[x][y][z];
                        int wx = baseX + x;
                        int wy = baseY + y;
                        int wz = baseZ + z;
                        mask[idx] = buildFaceDesc(block, wx, wy, wz, FACE_BACK, sampler);
                    }
                }
            }, [&](int u, int v, int width, int height, const FaceDesc& fd) {
                float wx = baseX + u;
                float wy = baseY + v;
                float wz = baseZ + z;
                addGreedyQuad(*mesh, fd, FACE_BACK, wx, wy, wz, width, height, solidIndex, waterIndex, glassIndex);
            });
        }

        mesh->setup();
    }
    
    void markChunkDirty(const ChunkCoord& coord) {
        if (dirtySet.insert(coord).second) {
            dirtyQueue.push_back(coord);
        }
    }
    
    void updateDirtyChunks(World& world) {     
        static constexpr int MAX_DIRTY_PER_FRAME = 4;
        int processed = 0;
        // Process queued dirty chunks first to avoid scanning all loaded chunks.
        while (!dirtyQueue.empty() && processed < MAX_DIRTY_PER_FRAME) {
            ChunkCoord coord = dirtyQueue.back();
            dirtyQueue.pop_back();
            dirtySet.erase(coord);
            Chunk* chunk = world.getChunk(coord.x, coord.y, coord.z);
            if (!chunk || !chunk->isDirty) continue;
            generateChunkMesh(world, coord.x, coord.y, coord.z);
            chunk->isDirty = false;
            ++processed;
        }
        // Fallback: scan for any remaining dirty chunks if queue missed them (bounded by MAX_DIRTY_PER_FRAME).
        if (processed < MAX_DIRTY_PER_FRAME) {
            for (const auto& coord : world.getLoadedChunks()) {
                if (processed >= MAX_DIRTY_PER_FRAME) break;
                Chunk* chunk = world.getChunk(coord.x, coord.y, coord.z);
                if (chunk && chunk->isDirty) {
                    generateChunkMesh(world, coord.x, coord.y, coord.z);
                    chunk->isDirty = false;
                    ++processed;
                }
            }
        }
    }
    
    void drawVisibleChunksSolid(float playerX, float playerZ, const Frustum* frustum = nullptr) {
        int centerChunkX = static_cast<int>(std::floor(playerX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(playerZ / CHUNK_SIZE));
        
        std::vector<std::pair<float, ChunkMesh*>> drawList;
        drawList.reserve(chunkMeshes.size());
        
        for (auto& pair : chunkMeshes) {
            const ChunkCoord& coord = pair.first;
            int dx = coord.x - centerChunkX;
            int dz = coord.z - centerChunkZ;
            
            // Distance culling (2D horizontal distance)
            if (dx * dx + dz * dz > RENDER_DISTANCE * RENDER_DISTANCE) {
                continue;
            }
            
            // Frustum culling (if frustum is provided)
            if (frustum) {
                if (!frustum->isChunkVisible(coord.x, coord.y, coord.z, CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE)) {
                    continue;
                }
            }
            
            float dist2 = static_cast<float>(dx * dx + dz * dz);
            drawList.push_back({dist2, pair.second.get()});
        }
        
        std::sort(drawList.begin(), drawList.end(), [](const auto& a, const auto& b) {
            return a.first < b.first; // front-to-back for early-Z
        });
        
        for (const auto& item : drawList) {
            item.second->drawSolid();
        }
    }
    
    void drawVisibleChunksPlants(float playerX, float playerZ, const Frustum* frustum = nullptr) {
        int centerChunkX = static_cast<int>(std::floor(playerX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(playerZ / CHUNK_SIZE));
        
        std::vector<std::pair<float, ChunkMesh*>> drawList;
        drawList.reserve(chunkMeshes.size());
        
        for (auto& pair : chunkMeshes) {
            const ChunkCoord& coord = pair.first;
            int dx = coord.x - centerChunkX;
            int dz = coord.z - centerChunkZ;
            
            if (dx * dx + dz * dz > RENDER_DISTANCE * RENDER_DISTANCE) {
                continue;
            }
            
            if (frustum) {
                if (!frustum->isChunkVisible(coord.x, coord.y, coord.z, CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE)) {
                    continue;
                }
            }
            
            float dist2 = static_cast<float>(dx * dx + dz * dz);
            drawList.push_back({dist2, pair.second.get()});
        }
        
        std::sort(drawList.begin(), drawList.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        
        for (const auto& item : drawList) {
            item.second->drawPlants();
        }
    }
    
    void drawVisibleChunksWater(float playerX, float playerZ, const Frustum* frustum = nullptr) {
        int centerChunkX = static_cast<int>(std::floor(playerX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(playerZ / CHUNK_SIZE));
        
        std::vector<std::pair<float, ChunkMesh*>> drawList;
        drawList.reserve(chunkMeshes.size());
        
        for (auto& pair : chunkMeshes) {
            const ChunkCoord& coord = pair.first;
            int dx = coord.x - centerChunkX;
            int dz = coord.z - centerChunkZ;
            
            // Distance culling (2D horizontal distance)
            if (dx * dx + dz * dz > RENDER_DISTANCE * RENDER_DISTANCE) {
                continue;
            }
            
            // Frustum culling (if frustum is provided)
            if (frustum) {
                if (!frustum->isChunkVisible(coord.x, coord.y, coord.z, CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE)) {
                    continue;
                }
            }
            
            float dist2 = static_cast<float>(dx * dx + dz * dz);
            drawList.push_back({dist2, pair.second.get()});
        }
        
        std::sort(drawList.begin(), drawList.end(), [](const auto& a, const auto& b) {
            return a.first > b.first; // back-to-front for blending
        });
        
        for (const auto& item : drawList) {
            item.second->drawWater();
        }
    }
    
    void drawVisibleChunksGlass(float playerX, float playerZ, const Frustum* frustum = nullptr) {
        int centerChunkX = static_cast<int>(std::floor(playerX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(playerZ / CHUNK_SIZE));
        
        std::vector<std::pair<float, ChunkMesh*>> drawList;
        drawList.reserve(chunkMeshes.size());
        
        for (auto& pair : chunkMeshes) {
            const ChunkCoord& coord = pair.first;
            int dx = coord.x - centerChunkX;
            int dz = coord.z - centerChunkZ;
            
            // Distance culling (2D horizontal distance)
            if (dx * dx + dz * dz > RENDER_DISTANCE * RENDER_DISTANCE) {
                continue;
            }
            
            // Frustum culling (if frustum is provided)
            if (frustum) {
                if (!frustum->isChunkVisible(coord.x, coord.y, coord.z, CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE)) {
                    continue;
                }
            }
            
            float dist2 = static_cast<float>(dx * dx + dz * dz);
            drawList.push_back({dist2, pair.second.get()});
        }
        
        std::sort(drawList.begin(), drawList.end(), [](const auto& a, const auto& b) {
            return a.first > b.first;
        });
        
        for (const auto& item : drawList) {
            item.second->drawGlass();
        }
    }
    
    void removeChunkMesh(int cx, int cy, int cz) {
        ChunkCoord coord{cx, cy, cz};
        chunkMeshes.erase(coord);
    }
    
    // Make texture index accessible for hotbar rendering
    int getTextureIndex(BlockType blockType, FaceDirection face) const {
        switch (blockType) {
            case BLOCK_GRASS:
                switch (face) {
                    case FACE_TOP:    return 3;
                    case FACE_BOTTOM: return 1;
                    default:          return 2;
                }
            case BLOCK_STONE:    return 0;
            case BLOCK_DIRT:     return 1;
            case BLOCK_BEDROCK:  return 4;
            case BLOCK_COAL_ORE: return 5;
            case BLOCK_IRON_ORE: return 6;
            case BLOCK_LOG:      return 7;
            case BLOCK_LEAVES:   return 8;
            case BLOCK_WATER:    return 9;
            case BLOCK_SAND:     return 13;
            case BLOCK_TALL_GRASS:return 14;
            case BLOCK_ORANGE_FLOWER: return 15;
            case BLOCK_BLUE_FLOWER: return 16;
            case BLOCK_PLANKS:   return 17;
            case BLOCK_COBBLESTONE: return 18;
            case BLOCK_GLASS:    return 19;
            case BLOCK_CLAY:     return 20;
            case BLOCK_SNOW:     return 21;
            default:             return 0;
        }
    }
    
private:

    // Cross-billboard (two vertical quads) for plants like tall grass, rotated to block diagonals.
    void addBillboardPlant(ChunkMesh& mesh, float x, float y, float z, unsigned int& indexOffset, BlockType blockType) {
        int textureIndex = getTextureIndex(blockType, FACE_FRONT);
        int tileX = textureIndex % ATLAS_TILES_WIDTH;
        int tileY = textureIndex / ATLAS_TILES_WIDTH;

        // Add small UV inset to prevent texture bleeding
        const float uvInset = 0.004f;
        float u0 = (tileX * ATLAS_TILE_SIZE) / float(ATLAS_WIDTH) + uvInset;
        float v0 = (tileY * ATLAS_TILE_SIZE) / float(ATLAS_HEIGHT) + uvInset;
        float u1 = ((tileX + 1) * ATLAS_TILE_SIZE) / float(ATLAS_WIDTH) - uvInset;
        float v1 = ((tileY + 1) * ATLAS_TILE_SIZE) / float(ATLAS_HEIGHT) - uvInset;
        float uv[4][2] = { {u0,v1}, {u1,v1}, {u1,v0}, {u0,v0} };

        std::vector<float>& vertices = mesh.plantVertices;
        std::vector<unsigned int>& indices = mesh.plantIndices;

        const float y0 = y;
        const float y1 = y + 1.0f;
        const float eps = 0.0008f;

        float quadA[4][3] = {
            { x + 0.0f + eps, y0, z + 0.0f + eps },   // bottom-left
            { x + 1.0f - eps, y0, z + 1.0f - eps },   // bottom-right
            { x + 1.0f - eps, y1, z + 1.0f - eps },   // top-right
            { x + 0.0f + eps, y1, z + 0.0f + eps }    // top-left
        };

        float quadB[4][3] = {
            { x + 1.0f - eps, y0, z + 0.0f + eps },   // bottom-left
            { x + 0.0f + eps, y0, z + 1.0f - eps },   // bottom-right
            { x + 0.0f + eps, y1, z + 1.0f - eps },   // top-right
            { x + 1.0f - eps, y1, z + 0.0f + eps }    // top-left
        };

        auto pushQuad = [&](float q[4][3]) {
            for (int i = 0; i < 4; ++i) {
                vertices.push_back(q[i][0]);
                vertices.push_back(q[i][1]);
                vertices.push_back(q[i][2]);
                vertices.push_back(uv[i][0]);
                vertices.push_back(uv[i][1]);
                vertices.push_back(0.0f);
            }
            indices.push_back(indexOffset + 0);
            indices.push_back(indexOffset + 1);
            indices.push_back(indexOffset + 2);
            indices.push_back(indexOffset + 2);
            indices.push_back(indexOffset + 3);
            indices.push_back(indexOffset + 0);
            indexOffset += 4;
        };

        pushQuad(quadA);
        pushQuad(quadB);
    }

    void addFace(ChunkMesh& mesh, float x, float y, float z, unsigned int& indexOffset, 
                 FaceDirection face, BlockType blockType, const BlockSampler& sampler, bool isWater, bool isFoliage, bool isGlass = false) {
        int faceIndex = static_cast<int>(face);
        int textureIndex = getTextureIndex(blockType, face);
        
        int tileX = textureIndex % ATLAS_TILES_WIDTH;
        int tileY = textureIndex / ATLAS_TILES_WIDTH;
        
        // Add small UV inset to prevent texture bleeding between atlas tiles
        const float uvInset = 0.001f;
        float u0 = (tileX * ATLAS_TILE_SIZE) / float(ATLAS_WIDTH) + uvInset;
        float v0 = (tileY * ATLAS_TILE_SIZE) / float(ATLAS_HEIGHT) + uvInset;
        float u1 = ((tileX + 1) * ATLAS_TILE_SIZE) / float(ATLAS_WIDTH) - uvInset;
        float v1 = ((tileY + 1) * ATLAS_TILE_SIZE) / float(ATLAS_HEIGHT) - uvInset;
        float texCoords[4][2] = { {u0,v1}, {u1,v1}, {u1,v0}, {u0,v0} };
        
        float aoValues[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        if (!isWater && !isFoliage && !isGlass) { // no AO on water, leaves, or glass
            for (int i = 0; i < 4; ++i) {
                int dx = (int)faceVertices[faceIndex][i][0];
                int dy = (int)faceVertices[faceIndex][i][1];
                int dz = (int)faceVertices[faceIndex][i][2];
                int px = (int)(x + dx);
                int py = (int)(y + dy);
                int pz = (int)(z + dz);
                bool missingSolid = (py < WATER_LEVEL); // treat missing as solid underwater, air above
                bool side1  = sampler.isSolidWithFallback(px + faceNormals[faceIndex][0], py + faceNormals[faceIndex][1], pz + faceNormals[faceIndex][2], missingSolid);
                bool side2  = sampler.isSolidWithFallback(px + faceTangents[faceIndex][0], py + faceTangents[faceIndex][1], pz + faceTangents[faceIndex][2], missingSolid);
                bool corner = sampler.isSolidWithFallback(px + faceNormals[faceIndex][0] + faceTangents[faceIndex][0],
                                                          py + faceNormals[faceIndex][1] + faceTangents[faceIndex][1],
                                                          pz + faceNormals[faceIndex][2] + faceTangents[faceIndex][2],
                                                          missingSolid);
                aoValues[i] = calculateAO(side1, side2, corner);
            }
        }
        
        bool lowerWaterTop = false;
        if (isWater && face == FACE_TOP) {
            const Block* above = sampler.tryGet((int)x, (int)y + 1, (int)z);
            if (!above || above->type != BLOCK_WATER) {
                lowerWaterTop = true;
            }
        }
        
        std::vector<float>& vertices = isGlass ? mesh.glassVertices : (isWater ? mesh.waterVertices : mesh.solidVertices);
        std::vector<unsigned int>& indices = isGlass ? mesh.glassIndices : (isWater ? mesh.waterIndices : mesh.solidIndices);

        // small outward push for foliage to avoid z-fighting with adjacent solids
        const float foliageOffset = 0.0015f;
        float nx = (float)faceNormals[faceIndex][0];
        float ny = (float)faceNormals[faceIndex][1];
        float nz = (float)faceNormals[faceIndex][2];
        
        for (int i = 0; i < 4; ++i) {
            float vx = faceVertices[faceIndex][i][0] + x;
            float vy = faceVertices[faceIndex][i][1];
            float vz = faceVertices[faceIndex][i][2] + z;
            if (isWater) {
                const float waterOffset = 0.001f;
                if (faceIndex == 4) {
                    if (lowerWaterTop) vy = 0.8f;
                } else if (faceIndex == 0) {
                    vz += waterOffset;
                } else if (faceIndex == 1) {
                    vz -= waterOffset;
                } else if (faceIndex == 2) {
                    vx += waterOffset;
                } else if (faceIndex == 3) {
                    vx -= waterOffset;
                } else if (faceIndex == 5) {
                    vy -= waterOffset;
                }
                if (faceIndex < 4 && lowerWaterTop && vy > 0.99f) vy = 0.8f;
            }
            if (isFoliage) {
                // push along face normal slightly
                vx += nx * foliageOffset;
                vy += ny * foliageOffset;
                vz += nz * foliageOffset;
            }
            vy += y;
            vertices.push_back(vx);
            vertices.push_back(vy);
            vertices.push_back(vz);
            vertices.push_back(texCoords[i][0]);
            vertices.push_back(texCoords[i][1]);
            vertices.push_back(aoValues[i]);
        }
        
        for (int i = 0; i < 6; ++i) {
            indices.push_back(indexOffset + faceIndices[faceIndex][i]);
        }
        indexOffset += 4;
    }
    
    FaceDesc buildFaceDesc(const Block& block, int wx, int wy, int wz, FaceDirection face, const BlockSampler& sampler) const {
        FaceDesc fd{};
        if (!block.isSolid) return fd;
        if (block.type == BLOCK_TALL_GRASS || block.type == BLOCK_ORANGE_FLOWER || block.type == BLOCK_BLUE_FLOWER) return fd;
        if (block.type == BLOCK_BEDROCK && face == FACE_BOTTOM && (wy % CHUNK_HEIGHT) == 0) return fd;

        bool isWater = (block.type == BLOCK_WATER);
        bool isFoliage = (block.type == BLOCK_LEAVES);
        bool isGlass = (block.type == BLOCK_GLASS);

        auto shouldRenderFaceAgainst = [&](const Block* neighbor, BlockType selfType) -> bool {
            if (!neighbor) return false;
            if (!neighbor->isSolid) return true;
            if (selfType == BLOCK_LEAVES && neighbor->type == BLOCK_LEAVES) return false;
            if (selfType == BLOCK_LEAVES) return true;
            if (selfType == BLOCK_GLASS) {
                if (neighbor->type == BLOCK_GLASS) return false;
                if (neighbor->type == BLOCK_WATER || neighbor->type == BLOCK_LEAVES) return true;
                return false;
            }
            if (neighbor->type == BLOCK_WATER || neighbor->type == BLOCK_LEAVES || neighbor->type == BLOCK_GLASS) return true;
            return false;
        };

        int nx = wx + faceNormals[static_cast<int>(face)][0];
        int ny = wy + faceNormals[static_cast<int>(face)][1];
        int nz = wz + faceNormals[static_cast<int>(face)][2];
        const Block* neighbor = sampler.tryGet(nx, ny, nz);

        bool render = false;
        if (isWater) {
            render = neighbor && neighbor->type != BLOCK_WATER && (!neighbor->isSolid || neighbor->type == BLOCK_LEAVES);
        } else if (isGlass) {
            render = neighbor && (!neighbor->isSolid || neighbor->type == BLOCK_WATER || neighbor->type == BLOCK_LEAVES);
        } else {
            render = shouldRenderFaceAgainst(neighbor, block.type);
        }

        if (!render) return fd;

        fd.present = true;
        fd.type = block.type;
        fd.textureIndex = getTextureIndex(block.type, face);
        fd.isWater = isWater;
        fd.isFoliage = isFoliage;
        fd.isGlass = isGlass;
        fd.lowerWaterTop = false;

        if (isWater && face == FACE_TOP) {
            const Block* above = sampler.tryGet(wx, wy + 1, wz);
            fd.lowerWaterTop = (!above || above->type != BLOCK_WATER);
        }

        if (!isWater && !isFoliage && !isGlass) {
            int faceIndex = static_cast<int>(face);
            for (int i = 0; i < 4; ++i) {
                int dx = (int)faceVertices[faceIndex][i][0];
                int dy = (int)faceVertices[faceIndex][i][1];
                int dz = (int)faceVertices[faceIndex][i][2];
                int px = wx + dx;
                int py = wy + dy;
                int pz = wz + dz;
                bool missingSolid = (py < WATER_LEVEL);
                bool side1  = sampler.isSolidWithFallback(px + faceNormals[faceIndex][0], py + faceNormals[faceIndex][1], pz + faceNormals[faceIndex][2], missingSolid);
                bool side2  = sampler.isSolidWithFallback(px + faceTangents[faceIndex][0], py + faceTangents[faceIndex][1], pz + faceTangents[faceIndex][2], missingSolid);
                bool corner = sampler.isSolidWithFallback(px + faceNormals[faceIndex][0] + faceTangents[faceIndex][0],
                                                          py + faceNormals[faceIndex][1] + faceTangents[faceIndex][1],
                                                          pz + faceNormals[faceIndex][2] + faceTangents[faceIndex][2],
                                                          missingSolid);
                fd.ao[i] = calculateAO(side1, side2, corner);
            }
        }

        return fd;
    }

    bool facesEqual(const FaceDesc& a, const FaceDesc& b) const {
        if (!a.present || !b.present) return false;
        if (a.type != b.type || a.textureIndex != b.textureIndex) return false;
        if (a.isWater != b.isWater || a.isFoliage != b.isFoliage || a.isGlass != b.isGlass) return false;
        if (a.lowerWaterTop != b.lowerWaterTop) return false;
        return a.ao == b.ao;
    }

    void addGreedyQuad(ChunkMesh& mesh, const FaceDesc& fd, FaceDirection face,
                       float baseX, float baseY, float baseZ, int width, int height,
                       unsigned int& solidIndex, unsigned int& waterIndex, unsigned int& glassIndex) const {
        int faceIndex = static_cast<int>(face);
        int textureIndex = fd.textureIndex;

        int tileX = textureIndex % ATLAS_TILES_WIDTH;
        int tileY = textureIndex / ATLAS_TILES_WIDTH;

        const float uvInset = 0.001f;
        float u0 = (tileX * ATLAS_TILE_SIZE) / float(ATLAS_WIDTH) + uvInset;
        float v0 = (tileY * ATLAS_TILE_SIZE) / float(ATLAS_HEIGHT) + uvInset;
        float u1 = ((tileX + 1) * ATLAS_TILE_SIZE) / float(ATLAS_WIDTH) - uvInset;
        float v1 = ((tileY + 1) * ATLAS_TILE_SIZE) / float(ATLAS_HEIGHT) - uvInset;

        float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
        switch (face) {
            case FACE_FRONT:
            case FACE_BACK:   scaleX = (float)width; scaleY = (float)height; break;
            case FACE_RIGHT:
            case FACE_LEFT:   scaleZ = (float)width; scaleY = (float)height; break;
            case FACE_TOP:
            case FACE_BOTTOM: scaleX = (float)width; scaleZ = (float)height; break;
        }

        std::vector<float>& vertices = fd.isGlass ? mesh.glassVertices : (fd.isWater ? mesh.waterVertices : mesh.solidVertices);
        std::vector<unsigned int>& indices = fd.isGlass ? mesh.glassIndices : (fd.isWater ? mesh.waterIndices : mesh.solidIndices);
        unsigned int& indexOffset = fd.isGlass ? glassIndex : (fd.isWater ? waterIndex : solidIndex);

        for (int i = 0; i < 4; ++i) {
            float vx = baseX + faceVertices[faceIndex][i][0] * scaleX;
            float vy = baseY + faceVertices[faceIndex][i][1] * scaleY;
            float vz = baseZ + faceVertices[faceIndex][i][2] * scaleZ;

            if (fd.isWater) {
                const float waterOffset = 0.001f;
                if (faceIndex == 4) {
                    if (fd.lowerWaterTop) vy = baseY + 0.8f;
                } else if (faceIndex == 0) {
                    vz += waterOffset;
                } else if (faceIndex == 1) {
                    vz -= waterOffset;
                } else if (faceIndex == 2) {
                    vx += waterOffset;
                } else if (faceIndex == 3) {
                    vx -= waterOffset;
                } else if (faceIndex == 5) {
                    vy -= waterOffset;
                }
                if (faceIndex < 4 && fd.lowerWaterTop && vy > baseY + 0.99f) vy = baseY + 0.8f;
            }

            if (fd.isFoliage) {
                const float foliageOffset = 0.0015f;
                vx += (float)faceNormals[faceIndex][0] * foliageOffset;
                vy += (float)faceNormals[faceIndex][1] * foliageOffset;
                vz += (float)faceNormals[faceIndex][2] * foliageOffset;
            }

            // Repeat UV every block within the same atlas tile so merged quads don't bleed into neighbors.
            float uAxis = 0.0f, vAxis = 0.0f;
            switch (face) {
                case FACE_FRONT:
                case FACE_BACK:   uAxis = vx; vAxis = vy; break;
                case FACE_RIGHT:
                case FACE_LEFT:   uAxis = vz; vAxis = vy; break;
                case FACE_TOP:
                case FACE_BOTTOM: uAxis = vx; vAxis = vz; break;
            }
            float uTile = uAxis - std::floor(uAxis);
            float vTile = vAxis - std::floor(vAxis);
            float uCoord = u0 + uTile * (u1 - u0);
            float vCoord = v0 + vTile * (v1 - v0);

            vertices.push_back(vx);
            vertices.push_back(vy);
            vertices.push_back(vz);
            vertices.push_back(uCoord);
            vertices.push_back(vCoord);
            vertices.push_back(fd.ao[i]);
        }

        indices.push_back(indexOffset + 0);
        indices.push_back(indexOffset + 1);
        indices.push_back(indexOffset + 2);
        indices.push_back(indexOffset + 2);
        indices.push_back(indexOffset + 3);
        indices.push_back(indexOffset + 0);
        indexOffset += 4;
    }
    
    float calculateAO(bool side1, bool side2, bool corner) const {
        if (side1 && side2) return AO_STRENGTH;
        int occlusion = (int)side1 + (int)side2 + (int)corner;
        switch (occlusion) {
            case 0: return 0.0f * AO_STRENGTH;
            case 1: return 0.4f * AO_STRENGTH;
            case 2: return 0.6f * AO_STRENGTH;
            case 3: return 0.7f * AO_STRENGTH;
        }
        return 0.0f;
    }
    
    static const float faceVertices[6][4][3];
    static const unsigned int faceIndices[6][6];
    static const int faceNormals[6][3];
    static const int faceTangents[6][3];
};

const float MeshManager::faceVertices[6][4][3] = {
    {{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}}, // Front (+Z)
    {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}}, // Back (-Z)
    {{1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, // Right (+X)
    {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}}, // Left (-X)
    {{0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // Top (+Y)
    {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}  // Bottom (-Y)
};
const unsigned int MeshManager::faceIndices[6][6] = {
    {0, 1, 2, 2, 3, 0},
    {0, 1, 2, 2, 3, 0},
    {0, 1, 2, 2, 3, 0},
    {0, 1, 2, 2, 3, 0},
    {0, 1, 2, 2, 3, 0},
    {0, 1, 2, 2, 3, 0}
};
const int MeshManager::faceNormals[6][3] = {
    {0, 0, 1},
    {0, 0, -1},
    {1, 0, 0},
    {-1, 0, 0},
    {0, 1, 0},
    {0, -1, 0}
};
const int MeshManager::faceTangents[6][3] = {
    {1, 0, 0},
    {-1, 0, 0},
    {0, 0, -1},
    {0, 0, 1},
    {1, 0, 0},
    {1, 0, 0}
};

#endif
