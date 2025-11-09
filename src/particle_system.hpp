// particle_system.hpp
#ifndef PARTICLE_SYSTEM_HPP
#define PARTICLE_SYSTEM_HPP

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#endif

#include <vector>
#include <random>
#include "../shared/types.hpp"
#include "../shared/config.hpp"

struct Particle {
    Vector3 position;
    Vector3 velocity;
    float uvX, uvY;  // UV coordinates for the particle texture
    float size;
    float life;      // Time remaining (seconds)
    float maxLife;   // Total lifetime
};

class ParticleSystem {
public:
    ParticleSystem() {
        // Initialize random number generator
        randomEngine.seed(std::random_device{}());
        
        // Create VAO and VBO for particles
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
    }
    
    ~ParticleSystem() {
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
    }
    
    // Spawn particles when a block is broken
    void spawnBlockBreakParticles(int blockX, int blockY, int blockZ, BlockType blockType, int textureIndex) {
        const int particleCount = 16 + (rand() % 5); // 8-12 particles
        const float blockCenterX = blockX + 0.5f;
        const float blockCenterY = blockY + 0.5f;
        const float blockCenterZ = blockZ + 0.5f;
        
        // Calculate UV coordinates from texture atlas index
        int tileX = textureIndex % ATLAS_TILES_WIDTH;
        int tileY = textureIndex / ATLAS_TILES_WIDTH;
        float baseU = (tileX * ATLAS_TILE_SIZE) / float(ATLAS_WIDTH);
        float baseV = (tileY * ATLAS_TILE_SIZE) / float(ATLAS_HEIGHT);
        float tileUSize = ATLAS_TILE_SIZE / float(ATLAS_WIDTH);
        float tileVSize = ATLAS_TILE_SIZE / float(ATLAS_HEIGHT);
        
        std::uniform_real_distribution<float> posOffset(-0.3f, 0.3f);
        std::uniform_real_distribution<float> velocityDist(-2.0f, 2.0f);
        std::uniform_real_distribution<float> upwardVelocity(2.0f, 4.5f);
        std::uniform_real_distribution<float> uvOffset(0.0f, 1.0f);
        std::uniform_real_distribution<float> lifeDist(0.6f, 1.0f);
        std::uniform_real_distribution<float> sizeDist(0.08f, 0.14f);
        
        for (int i = 0; i < particleCount; ++i) {
            Particle p;
            
            // Start position: slightly randomized around block center
            p.position.x = blockCenterX + posOffset(randomEngine);
            p.position.y = blockCenterY + posOffset(randomEngine);
            p.position.z = blockCenterZ + posOffset(randomEngine);
            
            // Velocity: explode outward and upward
            p.velocity.x = velocityDist(randomEngine);
            p.velocity.y = upwardVelocity(randomEngine);
            p.velocity.z = velocityDist(randomEngine);
            
            // Random UV offset within the block's texture
            float uvOffsetU = uvOffset(randomEngine) * 0.25f * tileUSize;
            float uvOffsetV = uvOffset(randomEngine) * 0.25f * tileVSize;
            p.uvX = baseU + uvOffsetU;
            p.uvY = baseV + uvOffsetV;
            
            // Particle properties
            p.size = sizeDist(randomEngine);
            p.maxLife = lifeDist(randomEngine);
            p.life = p.maxLife;
            
            particles.push_back(p);
        }
    }
    
    // Update all particles
    void update(float deltaTime) {
        const float gravity = -15.0f;
        const float drag = 0.98f;
        
        // Update particles
        for (auto it = particles.begin(); it != particles.end();) {
            Particle& p = *it;
            
            // Update lifetime
            p.life -= deltaTime;
            if (p.life <= 0.0f) {
                it = particles.erase(it);
                continue;
            }
            
            // Apply gravity
            p.velocity.y += gravity * deltaTime;
            
            // Apply drag
            p.velocity.x *= drag;
            p.velocity.z *= drag;
            
            // Update position
            p.position.x += p.velocity.x * deltaTime;
            p.position.y += p.velocity.y * deltaTime;
            p.position.z += p.velocity.z * deltaTime;
            
            ++it;
        }
    }
    
    // Render all particles
    void render(const mat4& view, const mat4& projection, GLuint textureAtlas, const Vector3& cameraPos) {
        if (particles.empty()) return;
        
        // Build vertex data for all particles
        std::vector<float> vertices;
        vertices.reserve(particles.size() * 4 * 8); // 4 vertices per particle, 8 floats per vertex
        
        // Camera right and up vectors for billboarding
        Vector3 right = {view.data[0], view.data[4], view.data[8]};
        Vector3 up = {view.data[1], view.data[5], view.data[9]};
        
        for (const Particle& p : particles) {
            // Calculate alpha based on remaining life
            float alpha = p.life / p.maxLife;
            
            // Calculate the four corners of the billboard quad
            float halfSize = p.size * 0.5f;
            
            // Bottom-left
            Vector3 pos0 = {
                p.position.x - right.x * halfSize - up.x * halfSize,
                p.position.y - right.y * halfSize - up.y * halfSize,
                p.position.z - right.z * halfSize - up.z * halfSize
            };
            
            // Bottom-right
            Vector3 pos1 = {
                p.position.x + right.x * halfSize - up.x * halfSize,
                p.position.y + right.y * halfSize - up.y * halfSize,
                p.position.z + right.z * halfSize - up.z * halfSize
            };
            
            // Top-right
            Vector3 pos2 = {
                p.position.x + right.x * halfSize + up.x * halfSize,
                p.position.y + right.y * halfSize + up.y * halfSize,
                p.position.z + right.z * halfSize + up.z * halfSize
            };
            
            // Top-left
            Vector3 pos3 = {
                p.position.x - right.x * halfSize + up.x * halfSize,
                p.position.y - right.y * halfSize + up.y * halfSize,
                p.position.z - right.z * halfSize + up.z * halfSize
            };
            
            // UV size for a small portion of the texture
            float uvSize = 0.05f; // Small portion of the tile
            
            // Add 4 vertices for this particle (position, UV, alpha)
            // Vertex 0 (bottom-left)
            vertices.insert(vertices.end(), {pos0.x, pos0.y, pos0.z, p.uvX, p.uvY + uvSize, alpha});
            
            // Vertex 1 (bottom-right)
            vertices.insert(vertices.end(), {pos1.x, pos1.y, pos1.z, p.uvX + uvSize, p.uvY + uvSize, alpha});
            
            // Vertex 2 (top-right)
            vertices.insert(vertices.end(), {pos2.x, pos2.y, pos2.z, p.uvX + uvSize, p.uvY, alpha});
            
            // Vertex 3 (top-left)
            vertices.insert(vertices.end(), {pos3.x, pos3.y, pos3.z, p.uvX, p.uvY, alpha});
        }
        
        if (vertices.empty()) return;
        
        // Upload vertex data
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
        
        // Position attribute (location 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        
        // UV attribute (location 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        
        // Alpha attribute (location 2)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
        
        // Bind texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureAtlas);
        
        // Draw all particles as quads (using triangle fan per quad)
        // We'll use GL_TRIANGLES and manually create indices
        std::vector<unsigned int> indices;
        indices.reserve(particles.size() * 6);
        for (size_t i = 0; i < particles.size(); ++i) {
            unsigned int baseIdx = i * 4;
            // First triangle (0, 1, 2)
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 2);
            // Second triangle (0, 2, 3)
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 3);
        }
        
        // Create temporary EBO for indices
        GLuint ebo;
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_DYNAMIC_DRAW);
        
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        
        // Clean up
        glDeleteBuffers(1, &ebo);
        glBindVertexArray(0);
    }
    
    int getParticleCount() const {
        return particles.size();
    }
    
private:
    std::vector<Particle> particles;
    std::mt19937 randomEngine;
    GLuint vao = 0;
    GLuint vbo = 0;
};

#endif // PARTICLE_SYSTEM_HPP
