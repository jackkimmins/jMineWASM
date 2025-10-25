// mesh.hpp
#ifndef MESH_HPP
#define MESH_HPP

#include <unordered_map>

// ChunkMesh - Individual mesh for each chunk
class ChunkMesh {
public:
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    GLuint VAO, VBO, EBO;
    bool isSetup = false;
    
    ChunkMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
    }
    
    void setup() {
        if (vertices.empty()) return;
        
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
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
        isSetup = true;
    }
    
    void draw() const {
        if (!isSetup || indices.empty()) return;
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    
    ~ChunkMesh() {
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glDeleteVertexArrays(1, &VAO);
    }
};

// MeshManager - Manages all chunk meshes
class MeshManager {
public:
    std::unordered_map<ChunkCoord, std::unique_ptr<ChunkMesh>> chunkMeshes;
    
    // Generate mesh for a single chunk
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
        
        mesh->vertices.clear();
        mesh->indices.clear();
        unsigned int currentIndex = 0;
        
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    const Block& block = chunk->blocks[x][y][z];
                    if (!block.isSolid) continue;
                    
                    float worldX = (cx * CHUNK_SIZE) + x;
                    float worldY = (cy * CHUNK_HEIGHT) + y;
                    float worldZ = (cz * CHUNK_SIZE) + z;
                    
                    // Check each face and add if not occluded
                    if (!isSolid(world, cx * CHUNK_SIZE + x + 1, cy * CHUNK_HEIGHT + y, cz * CHUNK_SIZE + z))
                        addFace(*mesh, worldX, worldY, worldZ, currentIndex, FACE_RIGHT, block.type, world);
                    if (!isSolid(world, cx * CHUNK_SIZE + x - 1, cy * CHUNK_HEIGHT + y, cz * CHUNK_SIZE + z))
                        addFace(*mesh, worldX, worldY, worldZ, currentIndex, FACE_LEFT, block.type, world);
                    if (!isSolid(world, cx * CHUNK_SIZE + x, cy * CHUNK_HEIGHT + y + 1, cz * CHUNK_SIZE + z))
                        addFace(*mesh, worldX, worldY, worldZ, currentIndex, FACE_TOP, block.type, world);
                    if (!isSolid(world, cx * CHUNK_SIZE + x, cy * CHUNK_HEIGHT + y - 1, cz * CHUNK_SIZE + z) && !(block.type == BLOCK_BEDROCK && y == 0))
                        addFace(*mesh, worldX, worldY, worldZ, currentIndex, FACE_BOTTOM, block.type, world);
                    if (!isSolid(world, cx * CHUNK_SIZE + x, cy * CHUNK_HEIGHT + y, cz * CHUNK_SIZE + z + 1))
                        addFace(*mesh, worldX, worldY, worldZ, currentIndex, FACE_FRONT, block.type, world);
                    if (!isSolid(world, cx * CHUNK_SIZE + x, cy * CHUNK_HEIGHT + y, cz * CHUNK_SIZE + z - 1))
                        addFace(*mesh, worldX, worldY, worldZ, currentIndex, FACE_BACK, block.type, world);
                }
            }
        }
        
        mesh->setup();
    }
    
    // Regenerate dirty chunks
    void updateDirtyChunks(World& world) {
        for (const auto& coord : world.getLoadedChunks()) {
            auto it = world.getChunks().find(coord);
            if (it != world.getChunks().end() && it->second->isDirty) {
                generateChunkMesh(world, coord.x, coord.y, coord.z);
                it->second->isDirty = false;
            }
        }
    }
    
    // Draw all visible chunks within render distance
    void drawVisibleChunks(float playerX, float playerZ) {
        int centerChunkX = static_cast<int>(std::floor(playerX / CHUNK_SIZE));
        int centerChunkZ = static_cast<int>(std::floor(playerZ / CHUNK_SIZE));
        
        for (auto& pair : chunkMeshes) {
            const ChunkCoord& coord = pair.first;
            
            // Check if within render distance (cylindrical)
            int dx = coord.x - centerChunkX;
            int dz = coord.z - centerChunkZ;
            if (dx * dx + dz * dz > RENDER_DISTANCE * RENDER_DISTANCE) continue;
            
            pair.second->draw();
        }
    }
    
    // Remove mesh for unloaded chunk
    void removeChunkMesh(int cx, int cy, int cz) {
        ChunkCoord coord{cx, cy, cz};
        chunkMeshes.erase(coord);
    }
    
private:
    // Check if block is solid at world coordinates
    bool isSolid(const World& world, int x, int y, int z) const {
        return world.isSolidAt(x, y, z);
    }
    
    int getTextureIndex(BlockType blockType, FaceDirection face) const {
        switch (blockType) {
            case BLOCK_GRASS:
                switch (face) {
                    case FACE_TOP: return 3;
                    case FACE_BOTTOM: return 1;
                    default: return 4;
                }
            case BLOCK_STONE: return 0;
            case BLOCK_DIRT: return 1;
            case BLOCK_PLANKS: return 2;
            case BLOCK_BEDROCK: return 5;
            case BLOCK_COAL_ORE: return 6;
            case BLOCK_IRON_ORE: return 7;
            default: return 0;
        }
    }
    
    void addFace(ChunkMesh& mesh, float x, float y, float z, unsigned int& indexOffset, 
                 FaceDirection face, BlockType blockType, const World& world) {
        int faceIndex = static_cast<int>(face);
        int textureIndex = getTextureIndex(blockType, face);
        
        // Compute texture coordinates
        int tileX = textureIndex % ATLAS_TILES_WIDTH;
        int tileY = textureIndex / ATLAS_TILES_WIDTH;
        
        float u0 = (tileX * ATLAS_TILE_SIZE) / static_cast<float>(ATLAS_TILES_WIDTH);
        float v0 = (tileY * ATLAS_TILE_SIZE) / static_cast<float>(ATLAS_TILES_HEIGHT);
        float u1 = ((tileX + 1) * ATLAS_TILE_SIZE) / static_cast<float>(ATLAS_TILES_WIDTH);
        float v1 = ((tileY + 1) * ATLAS_TILE_SIZE) / static_cast<float>(ATLAS_TILES_HEIGHT);
        
        float texCoords[4][2] = {
            {u0, v1}, {u1, v1}, {u1, v0}, {u0, v0}
        };
        
        // Calculate AO values
        float aoValues[4];
        for (int i = 0; i < 4; ++i) {
            int dx = static_cast<int>(faceVertices[faceIndex][i][0]);
            int dy = static_cast<int>(faceVertices[faceIndex][i][1]);
            int dz = static_cast<int>(faceVertices[faceIndex][i][2]);
            
            int px = static_cast<int>(x + dx);
            int py = static_cast<int>(y + dy);
            int pz = static_cast<int>(z + dz);
            
            bool side1 = isSolid(world, px + faceNormals[faceIndex][0], py + faceNormals[faceIndex][1], pz + faceNormals[faceIndex][2]);
            bool side2 = isSolid(world, px + faceTangents[faceIndex][0], py + faceTangents[faceIndex][1], pz + faceTangents[faceIndex][2]);
            bool corner = isSolid(world, px + faceNormals[faceIndex][0] + faceTangents[faceIndex][0],
                                       py + faceNormals[faceIndex][1] + faceTangents[faceIndex][1],
                                       pz + faceNormals[faceIndex][2] + faceTangents[faceIndex][2]);
            
            aoValues[i] = calculateAO(side1, side2, corner);
        }
        
        // Add vertices
        for (int i = 0; i < 4; ++i) {
            mesh.vertices.push_back(faceVertices[faceIndex][i][0] + x);
            mesh.vertices.push_back(faceVertices[faceIndex][i][1] + y);
            mesh.vertices.push_back(faceVertices[faceIndex][i][2] + z);
            mesh.vertices.push_back(texCoords[i][0]);
            mesh.vertices.push_back(texCoords[i][1]);
            mesh.vertices.push_back(aoValues[i]);
        }
        
        // Add indices
        for (int i = 0; i < 6; ++i) {
            mesh.indices.push_back(indexOffset + faceIndices[faceIndex][i]);
        }
        indexOffset += 4;
    }
    
    float calculateAO(bool side1, bool side2, bool corner) const {
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
    
    // Face vertices and indices
    static const float faceVertices[6][4][3];
    static const unsigned int faceIndices[6][6];
    static const int faceNormals[6][3];
    static const int faceTangents[6][3];
};

const float MeshManager::faceVertices[6][4][3] = {
    {{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}},
    {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}},
    {{1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
};

const unsigned int MeshManager::faceIndices[6][6] = {
    {0, 1, 2, 2, 3, 0}, {0, 1, 2, 2, 3, 0}, {0, 1, 2, 2, 3, 0},
    {0, 1, 2, 2, 3, 0}, {0, 1, 2, 2, 3, 0}, {0, 1, 2, 2, 3, 0}
};

const int MeshManager::faceNormals[6][3] = {
    {0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}
};

const int MeshManager::faceTangents[6][3] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {1, 0, 0}
};

#endif
