// mesh.hpp
#ifndef MESH_HPP
#define MESH_HPP

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

#endif