// mesh.hpp
#ifndef MESH_HPP
#define MESH_HPP

#include <unordered_map>

class ChunkMesh {
public:
    std::vector<float> solidVertices;
    std::vector<unsigned int> solidIndices;
    std::vector<float> waterVertices;
    std::vector<unsigned int> waterIndices;
    GLuint solidVAO, solidVBO, solidEBO;
    GLuint waterVAO, waterVBO, waterEBO;
    bool isSetup = false;
    
    ChunkMesh() {
        glGenVertexArrays(1, &solidVAO);
        glGenBuffers(1, &solidVBO);
        glGenBuffers(1, &solidEBO);
        glGenVertexArrays(1, &waterVAO);
        glGenBuffers(1, &waterVBO);
        glGenBuffers(1, &waterEBO);
    }
    
    void setup() {
        if (!solidVertices.empty()) {
            glBindVertexArray(solidVAO);
            glBindBuffer(GL_ARRAY_BUFFER, solidVBO);
            glBufferData(GL_ARRAY_BUFFER, solidVertices.size() * sizeof(float), solidVertices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, solidEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, solidIndices.size() * sizeof(unsigned int), solidIndices.data(), GL_STATIC_DRAW);
            
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
            glBindVertexArray(0);
        }
        
        if (!waterVertices.empty()) {
            glBindVertexArray(waterVAO);
            glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
            glBufferData(GL_ARRAY_BUFFER, waterVertices.size() * sizeof(float), waterVertices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, waterIndices.size() * sizeof(unsigned int), waterIndices.data(), GL_STATIC_DRAW);
            
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
            glBindVertexArray(0);
        }
        
        isSetup = true;
    }
    
    void drawSolid() const {
        if (!isSetup || solidIndices.empty()) return;
        glBindVertexArray(solidVAO);
        glDrawElements(GL_TRIANGLES, solidIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    
    void drawWater() const {
        if (!isSetup || waterIndices.empty()) return;
        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, waterIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    
    ~ChunkMesh() {
        glDeleteBuffers(1, &solidVBO);
        glDeleteBuffers(1, &solidEBO);
        glDeleteVertexArrays(1, &solidVAO);
        glDeleteBuffers(1, &waterVBO);
        glDeleteBuffers(1, &waterEBO);
        glDeleteVertexArrays(1, &waterVAO);
    }
};

class MeshManager {
public:
    std::unordered_map<ChunkCoord, std::unique_ptr<ChunkMesh>> chunkMeshes;
    
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
        mesh->waterVertices.clear();
        mesh->waterIndices.clear();
        unsigned int solidIndex = 0;
        unsigned int waterIndex = 0;
        
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    const Block& block = chunk->blocks[x][y][z];
                    if (!block.isSolid) continue;
                    
                    float worldX = (cx * CHUNK_SIZE) + x;
                    float worldY = (cy * CHUNK_HEIGHT) + y;
                    float worldZ = (cz * CHUNK_SIZE) + z;
                    
                    bool isWater = (block.type == BLOCK_WATER);
                    bool isFoliage = (block.type == BLOCK_LEAVES);

                    auto shouldRenderFaceAgainst = [&](const Block* neighbor, BlockType selfType) -> bool {
                        if (!neighbor || !neighbor->isSolid) return true;
                        if (selfType == BLOCK_LEAVES && neighbor->type == BLOCK_LEAVES) return false;
                        if (selfType == BLOCK_LEAVES) return true;
                        if (neighbor->type == BLOCK_WATER || neighbor->type == BLOCK_LEAVES) return true;
                        return false;
                    };

                    // Right (+X)
                    {
                        int nx = cx * CHUNK_SIZE + x + 1;
                        int ny = cy * CHUNK_HEIGHT + y;
                        int nz = cz * CHUNK_SIZE + z;
                        const Block* neighbor = world.getBlockAt(nx, ny, nz);
                        if (isWater) {
                            if (!neighbor || (!neighbor->isSolid || neighbor->type == BLOCK_WATER)) {
                                if (!neighbor || neighbor->type != BLOCK_WATER) {
                                    addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_RIGHT, block.type, world, isWater, isFoliage);
                                }
                            }
                        } else {
                            if (shouldRenderFaceAgainst(neighbor, block.type)) {
                                addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_RIGHT, block.type, world, isWater, isFoliage);
                            }
                        }
                    }
                    // Left (-X)
                    {
                        int nx = cx * CHUNK_SIZE + x - 1;
                        int ny = cy * CHUNK_HEIGHT + y;
                        int nz = cz * CHUNK_SIZE + z;
                        const Block* neighbor = world.getBlockAt(nx, ny, nz);
                        if (isWater) {
                            if (!neighbor || (!neighbor->isSolid || neighbor->type == BLOCK_WATER)) {
                                if (!neighbor || neighbor->type != BLOCK_WATER) {
                                    addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_LEFT, block.type, world, isWater, isFoliage);
                                }
                            }
                        } else {
                            if (shouldRenderFaceAgainst(neighbor, block.type)) {
                                addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_LEFT, block.type, world, isWater, isFoliage);
                            }
                        }
                    }
                    // Top (+Y)
                    {
                        int nx = cx * CHUNK_SIZE + x;
                        int ny = cy * CHUNK_HEIGHT + y + 1;
                        int nz = cz * CHUNK_SIZE + z;
                        const Block* neighbor = world.getBlockAt(nx, ny, nz);
                        if (isWater) {
                            if (!neighbor || (!neighbor->isSolid || neighbor->type == BLOCK_WATER)) {
                                if (!neighbor || neighbor->type != BLOCK_WATER) {
                                    addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_TOP, block.type, world, isWater, isFoliage);
                                }
                            }
                        } else {
                            if (shouldRenderFaceAgainst(neighbor, block.type)) {
                                addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_TOP, block.type, world, isWater, isFoliage);
                            }
                        }
                    }
                    // Bottom (-Y)
                    {
                        int nx = cx * CHUNK_SIZE + x;
                        int ny = cy * CHUNK_HEIGHT + y - 1;
                        int nz = cz * CHUNK_SIZE + z;
                        const Block* neighbor = world.getBlockAt(nx, ny, nz);
                        bool isBottomFaceBedrock = (block.type == BLOCK_BEDROCK && y == 0);
                        if (!isBottomFaceBedrock) {
                            if (isWater) {
                                if (!neighbor || (!neighbor->isSolid || neighbor->type == BLOCK_WATER)) {
                                    if (!neighbor || neighbor->type != BLOCK_WATER) {
                                        addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_BOTTOM, block.type, world, isWater, isFoliage);
                                    }
                                }
                            } else {
                                if (shouldRenderFaceAgainst(neighbor, block.type)) {
                                    addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_BOTTOM, block.type, world, isWater, isFoliage);
                                }
                            }
                        }
                    }
                    // Front (+Z)
                    {
                        int nx = cx * CHUNK_SIZE + x;
                        int ny = cy * CHUNK_HEIGHT + y;
                        int nz = cz * CHUNK_SIZE + z + 1;
                        const Block* neighbor = world.getBlockAt(nx, ny, nz);
                        if (isWater) {
                            if (!neighbor || (!neighbor->isSolid || neighbor->type == BLOCK_WATER)) {
                                if (!neighbor || neighbor->type != BLOCK_WATER) {
                                    addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_FRONT, block.type, world, isWater, isFoliage);
                                }
                            }
                        } else {
                            if (shouldRenderFaceAgainst(neighbor, block.type)) {
                                addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_FRONT, block.type, world, isWater, isFoliage);
                            }
                        }
                    }
                    // Back (-Z)
                    {
                        int nx = cx * CHUNK_SIZE + x;
                        int ny = cy * CHUNK_HEIGHT + y;
                        int nz = cz * CHUNK_SIZE + z - 1;
                        const Block* neighbor = world.getBlockAt(nx, ny, nz);
                        if (isWater) {
                            if (!neighbor || (!neighbor->isSolid || neighbor->type == BLOCK_WATER)) {
                                if (!neighbor || neighbor->type != BLOCK_WATER) {
                                    addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_BACK, block.type, world, isWater, isFoliage);
                                }
                            }
                        } else {
                            if (shouldRenderFaceAgainst(neighbor, block.type)) {
                                addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_BACK, block.type, world, isWater, isFoliage);
                            }
                        }
                    }
                }
            }
        }
        
        mesh->setup();
    }
    
    void updateDirtyChunks(World& world) {
        for (const auto& coord : world.getLoadedChunks()) {
            auto it = world.getChunks().find(coord);
            if (it != world.getChunks().end() && it->second->isDirty) {
                generateChunkMesh(world, coord.x, coord.y, coord.z);
                it->second->isDirty = false;
            }
        }
    }
    
    void drawVisibleChunksSolid(float playerX, float playerZ) {
        int centerChunkX = static_cast<int>(std::floor(playerX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(playerZ / CHUNK_SIZE));
        
        for (auto& pair : chunkMeshes) {
            const ChunkCoord& coord = pair.first;
            int dx = coord.x - centerChunkX;
            int dz = coord.z - centerChunkZ;
            if (dx * dx + dz * dz > RENDER_DISTANCE * RENDER_DISTANCE) continue;
            pair.second->drawSolid();
        }
    }
    
    void drawVisibleChunksWater(float playerX, float playerZ) {
        int centerChunkX = static_cast<int>(std::floor(playerX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(playerZ / CHUNK_SIZE));
        
        for (auto& pair : chunkMeshes) {
            const ChunkCoord& coord = pair.first;
            int dx = coord.x - centerChunkX;
            int dz = coord.z - centerChunkZ;
            if (dx * dx + dz * dz > RENDER_DISTANCE * RENDER_DISTANCE) continue;
            pair.second->drawWater();
        }
    }
    
    void removeChunkMesh(int cx, int cy, int cz) {
        ChunkCoord coord{cx, cy, cz};
        chunkMeshes.erase(coord);
    }
    
private:
    bool isSolid(const World& world, int x, int y, int z) const {
        return world.isSolidAt(x, y, z);
    }
    
    bool isOpaque(const World& world, int x, int y, int z) const {
        if (x < 0 || x >= WORLD_SIZE_X || y < 0 || y >= WORLD_SIZE_Y || z < 0 || z >= WORLD_SIZE_Z)
            return false;
        const Block* block = world.getBlockAt(x, y, z);
        if (!block || !block->isSolid) return false;
        return block->type != BLOCK_WATER && block->type != BLOCK_LEAVES;
    }
    
    int getTextureIndex(BlockType blockType, FaceDirection face) const {
        switch (blockType) {
            case BLOCK_GRASS:
                switch (face) {
                    case FACE_TOP:    return 3;
                    case FACE_BOTTOM: return 1;
                    default:          return 4;
                }
            case BLOCK_STONE:    return 0;
            case BLOCK_DIRT:     return 1;
            case BLOCK_PLANKS:   return 2;
            case BLOCK_BEDROCK:  return 5;
            case BLOCK_COAL_ORE: return 6;
            case BLOCK_IRON_ORE: return 7;
            case BLOCK_LOG:      return 8;
            case BLOCK_LEAVES:   return 9;
            case BLOCK_WATER:    return 10;
            case BLOCK_SAND:     return 14;
            default:             return 0;
        }
    }
    
    void addFace(ChunkMesh& mesh, float x, float y, float z, unsigned int& indexOffset, 
                 FaceDirection face, BlockType blockType, const World& world, bool isWater, bool isFoliage) {
        int faceIndex = static_cast<int>(face);
        int textureIndex = getTextureIndex(blockType, face);
        
        int tileX = textureIndex % ATLAS_TILES_WIDTH;
        int tileY = textureIndex / ATLAS_TILES_WIDTH;
        float u0 = (tileX * ATLAS_TILE_SIZE) / float(ATLAS_TILES_WIDTH);
        float v0 = (tileY * ATLAS_TILE_SIZE) / float(ATLAS_TILES_HEIGHT);
        float u1 = ((tileX + 1) * ATLAS_TILE_SIZE) / float(ATLAS_TILES_WIDTH);
        float v1 = ((tileY + 1) * ATLAS_TILE_SIZE) / float(ATLAS_TILES_HEIGHT);
        float texCoords[4][2] = { {u0,v1}, {u1,v1}, {u1,v0}, {u0,v0} };
        
        float aoValues[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        if (!isWater && !isFoliage) { // no AO on water or leaves
            for (int i = 0; i < 4; ++i) {
                int dx = (int)faceVertices[faceIndex][i][0];
                int dy = (int)faceVertices[faceIndex][i][1];
                int dz = (int)faceVertices[faceIndex][i][2];
                int px = (int)(x + dx);
                int py = (int)(y + dy);
                int pz = (int)(z + dz);
                bool side1  = isSolid(world, px + faceNormals[faceIndex][0], py + faceNormals[faceIndex][1], pz + faceNormals[faceIndex][2]);
                bool side2  = isSolid(world, px + faceTangents[faceIndex][0], py + faceTangents[faceIndex][1], pz + faceTangents[faceIndex][2]);
                bool corner = isSolid(world, px + faceNormals[faceIndex][0] + faceTangents[faceIndex][0],
                                           py + faceNormals[faceIndex][1] + faceTangents[faceIndex][1],
                                           pz + faceNormals[faceIndex][2] + faceTangents[faceIndex][2]);
                aoValues[i] = calculateAO(side1, side2, corner);
            }
        }
        
        bool lowerWaterTop = false;
        if (isWater && face == FACE_TOP) {
            const Block* above = world.getBlockAt((int)x, (int)y + 1, (int)z);
            if (!above || above->type != BLOCK_WATER) {
                lowerWaterTop = true;
            }
        }
        
        std::vector<float>& vertices = isWater ? mesh.waterVertices : mesh.solidVertices;
        std::vector<unsigned int>& indices = isWater ? mesh.waterIndices : mesh.solidIndices;

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