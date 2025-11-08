// mesh.hpp
#ifndef MESH_HPP
#define MESH_HPP

#include <unordered_map>
#include <vector>
#include <memory>

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

                    float worldX = (cx * CHUNK_SIZE) + x;
                    float worldY = (cy * CHUNK_HEIGHT) + y;
                    float worldZ = (cz * CHUNK_SIZE) + z;

                    // Plants like tall grass are non-solid and rendered as crossed quads.
                    if (block.type == BLOCK_TALL_GRASS || block.type == BLOCK_ORANGE_FLOWER || block.type == BLOCK_BLUE_FLOWER) {
                        addBillboardPlant(*mesh, worldX, worldY, worldZ, solidIndex, block.type);
                        continue;
                    }

                    if (!block.isSolid) continue;
                    
                    bool isWater = (block.type == BLOCK_WATER);
                    bool isFoliage = (block.type == BLOCK_LEAVES);

                    auto shouldRenderFaceAgainst = [&](const Block* neighbor, BlockType selfType) -> bool {
                        // If neighbor is nullptr, it means the chunk boundary block doesn't exist yet
                        // In this case, assume it's solid to prevent rendering internal faces
                        if (!neighbor) return false;
                        
                        // If neighbor is not solid (air), render the face
                        if (!neighbor->isSolid) return true;
                        
                        // Special handling for leaves
                        if (selfType == BLOCK_LEAVES && neighbor->type == BLOCK_LEAVES) return false;
                        if (selfType == BLOCK_LEAVES) return true;
                        
                        // Render faces against water and leaves
                        if (neighbor->type == BLOCK_WATER || neighbor->type == BLOCK_LEAVES) return true;
                        
                        // Don't render faces against other solid blocks
                        return false;
                    };

                    // Right (+X)
                    {
                        int nx = cx * CHUNK_SIZE + x + 1;
                        int ny = cy * CHUNK_HEIGHT + y;
                        int nz = cz * CHUNK_SIZE + z;
                        const Block* neighbor = world.getBlockAt(nx, ny, nz);
                        if (isWater) {
                            // Only render water face if neighbor exists and is not water
                            // If neighbor is nullptr, assume solid to prevent rendering at chunk boundaries
                            if (neighbor && neighbor->type != BLOCK_WATER && (!neighbor->isSolid || neighbor->type == BLOCK_LEAVES)) {
                                addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_RIGHT, block.type, world, isWater, isFoliage);
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
                            // Only render water face if neighbor exists and is not water
                            // If neighbor is nullptr, assume solid to prevent rendering at chunk boundaries
                            if (neighbor && neighbor->type != BLOCK_WATER && (!neighbor->isSolid || neighbor->type == BLOCK_LEAVES)) {
                                addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_LEFT, block.type, world, isWater, isFoliage);
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
                            // Only render water face if neighbor exists and is not water
                            // If neighbor is nullptr, assume solid to prevent rendering at chunk boundaries
                            if (neighbor && neighbor->type != BLOCK_WATER && (!neighbor->isSolid || neighbor->type == BLOCK_LEAVES)) {
                                addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_TOP, block.type, world, isWater, isFoliage);
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
                                // Only render water face if neighbor exists and is not water
                                // If neighbor is nullptr, assume solid to prevent rendering at chunk boundaries
                                if (neighbor && neighbor->type != BLOCK_WATER && (!neighbor->isSolid || neighbor->type == BLOCK_LEAVES)) {
                                    addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_BOTTOM, block.type, world, isWater, isFoliage);
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
                            // Only render water face if neighbor exists and is not water
                            // If neighbor is nullptr, assume solid to prevent rendering at chunk boundaries
                            if (neighbor && neighbor->type != BLOCK_WATER && (!neighbor->isSolid || neighbor->type == BLOCK_LEAVES)) {
                                addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_FRONT, block.type, world, isWater, isFoliage);
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
                            // Only render water face if neighbor exists and is not water
                            // If neighbor is nullptr, assume solid to prevent rendering at chunk boundaries
                            if (neighbor && neighbor->type != BLOCK_WATER && (!neighbor->isSolid || neighbor->type == BLOCK_LEAVES)) {
                                addFace(*mesh, worldX, worldY, worldZ, isWater ? waterIndex : solidIndex, FACE_BACK, block.type, world, isWater, isFoliage);
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
        static int callCount = 0;
        callCount++;
        if (callCount % 60 == 0) {  // Log every 60 calls (roughly once per second at 60fps)
            std::cout << "[MESH] updateDirtyChunks called (call #" << callCount << "), checking " << world.getLoadedChunks().size() << " loaded chunks" << std::endl;
        }
        
        int dirtyCount = 0;
        int checkedCount = 0;
        for (const auto& coord : world.getLoadedChunks()) {
            checkedCount++;
            Chunk* chunk = world.getChunk(coord.x, coord.y, coord.z);
            if (chunk) {
                if (chunk->isDirty) {
                    dirtyCount++;
                    std::cout << "[MESH] Regenerating mesh for dirty chunk (" << coord.x << "," << coord.y << "," << coord.z << ")" << std::endl;
                    generateChunkMesh(world, coord.x, coord.y, coord.z);
                    chunk->isDirty = false;
                    std::cout << "[MESH] Mesh regeneration complete for chunk (" << coord.x << "," << coord.y << "," << coord.z << ")" << std::endl;
                }
            } else {
                std::cout << "[MESH] WARNING: Could not get chunk (" << coord.x << "," << coord.y << "," << coord.z << ") for dirty check" << std::endl;
            }
        }
        if (dirtyCount > 0) {
            std::cout << "[MESH] Updated " << dirtyCount << " dirty chunks out of " << checkedCount << " loaded chunks" << std::endl;
        }
    }
    
    void drawVisibleChunksSolid(float playerX, float playerZ, const Frustum* frustum = nullptr) {
        int centerChunkX = static_cast<int>(std::floor(playerX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(playerZ / CHUNK_SIZE));
        
        int chunksRendered = 0;
        int chunksCulled = 0;
        
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
                    chunksCulled++;
                    continue;
                }
            }
            
            pair.second->drawSolid();
            chunksRendered++;
        }
        
        // Debug output (can be removed in production)
        static int frameCount = 0;
        if (++frameCount % 60 == 0) {
            // std::cout << "[RENDER] Chunks rendered: " << chunksRendered << ", culled: " << chunksCulled << std::endl;
        }
    }
    
    void drawVisibleChunksWater(float playerX, float playerZ, const Frustum* frustum = nullptr) {
        int centerChunkX = static_cast<int>(std::floor(playerX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(playerZ / CHUNK_SIZE));
        
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
            default:             return 0;
        }
    }

    // Cross-billboard (two vertical quads) for plants like tall grass, rotated to block diagonals.
    void addBillboardPlant(ChunkMesh& mesh, float x, float y, float z, unsigned int& indexOffset, BlockType blockType) {
        int textureIndex = getTextureIndex(blockType, FACE_FRONT);
        int tileX = textureIndex % ATLAS_TILES_WIDTH;
        int tileY = textureIndex / ATLAS_TILES_WIDTH;

        // Add small UV inset to prevent texture bleeding
        const float uvInset = 0.001f;
        float u0 = (tileX * ATLAS_TILE_SIZE) / float(ATLAS_WIDTH) + uvInset;
        float v0 = (tileY * ATLAS_TILE_SIZE) / float(ATLAS_HEIGHT) + uvInset;
        float u1 = ((tileX + 1) * ATLAS_TILE_SIZE) / float(ATLAS_WIDTH) - uvInset;
        float v1 = ((tileY + 1) * ATLAS_TILE_SIZE) / float(ATLAS_HEIGHT) - uvInset;
        float uv[4][2] = { {u0,v1}, {u1,v1}, {u1,v0}, {u0,v0} };

        std::vector<float>& vertices = mesh.solidVertices;
        std::vector<unsigned int>& indices = mesh.solidIndices;

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
                 FaceDirection face, BlockType blockType, const World& world, bool isWater, bool isFoliage) {
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
