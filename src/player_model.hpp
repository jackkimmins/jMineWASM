// player_model.hpp
// Minecraft-style player model with skin texture support
#ifndef PLAYER_MODEL_HPP
#define PLAYER_MODEL_HPP

#include <vector>
#include <cmath>

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#endif

#include "../shared/types.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Minecraft player dimensions (in blocks)
namespace PlayerDimensions {
    // Body parts dimensions
    constexpr float HEAD_WIDTH = 0.5f;    // 8 pixels / 16 = 0.5 blocks
    constexpr float HEAD_HEIGHT = 0.5f;
    constexpr float HEAD_DEPTH = 0.5f;
    
    constexpr float BODY_WIDTH = 0.5f;
    constexpr float BODY_HEIGHT = 0.75f;  // 12 pixels / 16 = 0.75 blocks
    constexpr float BODY_DEPTH = 0.25f;   // 4 pixels / 16 = 0.25 blocks
    
    constexpr float ARM_WIDTH = 0.25f;    // 4 pixels / 16 = 0.25 blocks
    constexpr float ARM_HEIGHT = 0.75f;
    constexpr float ARM_DEPTH = 0.25f;
    
    constexpr float LEG_WIDTH = 0.25f;
    constexpr float LEG_HEIGHT = 0.75f;
    constexpr float LEG_DEPTH = 0.25f;
    
    // Total height (head + body + legs)
    constexpr float TOTAL_HEIGHT = HEAD_HEIGHT + BODY_HEIGHT + LEG_HEIGHT; // 2.0 blocks
}

// UV coordinates for Minecraft 64x64 skin
// Format: [minU, minV, maxU, maxV]
namespace SkinUV {
    // Texture is 64x64 pixels
    constexpr float PIXEL = 1.0f / 64.0f;
    
    // Head (front, back, right, left, top, bottom)
    constexpr float HEAD_FRONT[4]  = {8*PIXEL,  8*PIXEL,  16*PIXEL, 16*PIXEL};
    constexpr float HEAD_BACK[4]   = {24*PIXEL, 8*PIXEL,  32*PIXEL, 16*PIXEL};
    constexpr float HEAD_RIGHT[4]  = {8*PIXEL,  8*PIXEL,  0*PIXEL,  16*PIXEL};
    constexpr float HEAD_LEFT[4]   = {24*PIXEL, 8*PIXEL,  16*PIXEL, 16*PIXEL};
    constexpr float HEAD_TOP[4]    = {8*PIXEL,  0*PIXEL,  16*PIXEL, 8*PIXEL};
    constexpr float HEAD_BOTTOM[4] = {16*PIXEL, 0*PIXEL,  24*PIXEL, 8*PIXEL};
    
    // Body (front, back, right, left, top, bottom)
    constexpr float BODY_FRONT[4]  = {20*PIXEL, 20*PIXEL, 28*PIXEL, 32*PIXEL};
    constexpr float BODY_BACK[4]   = {32*PIXEL, 20*PIXEL, 40*PIXEL, 32*PIXEL};
    constexpr float BODY_RIGHT[4]  = {16*PIXEL, 20*PIXEL, 20*PIXEL, 32*PIXEL};
    constexpr float BODY_LEFT[4]   = {28*PIXEL, 20*PIXEL, 32*PIXEL, 32*PIXEL};
    constexpr float BODY_TOP[4]    = {20*PIXEL, 16*PIXEL, 28*PIXEL, 20*PIXEL};
    constexpr float BODY_BOTTOM[4] = {28*PIXEL, 16*PIXEL, 36*PIXEL, 20*PIXEL};
    
    // Right Arm
    constexpr float RARM_FRONT[4]  = {44*PIXEL, 20*PIXEL, 48*PIXEL, 32*PIXEL};
    constexpr float RARM_BACK[4]   = {52*PIXEL, 20*PIXEL, 56*PIXEL, 32*PIXEL};
    constexpr float RARM_RIGHT[4]  = {40*PIXEL, 20*PIXEL, 44*PIXEL, 32*PIXEL};
    constexpr float RARM_LEFT[4]   = {48*PIXEL, 20*PIXEL, 52*PIXEL, 32*PIXEL};
    constexpr float RARM_TOP[4]    = {44*PIXEL, 16*PIXEL, 48*PIXEL, 20*PIXEL};
    constexpr float RARM_BOTTOM[4] = {48*PIXEL, 16*PIXEL, 52*PIXEL, 20*PIXEL};
    
    // Left Arm
    constexpr float LARM_FRONT[4]  = {36*PIXEL, 52*PIXEL, 40*PIXEL, 64*PIXEL};
    constexpr float LARM_BACK[4]   = {44*PIXEL, 52*PIXEL, 48*PIXEL, 64*PIXEL};
    constexpr float LARM_RIGHT[4]  = {32*PIXEL, 52*PIXEL, 36*PIXEL, 64*PIXEL};
    constexpr float LARM_LEFT[4]   = {40*PIXEL, 52*PIXEL, 44*PIXEL, 64*PIXEL};
    constexpr float LARM_TOP[4]    = {36*PIXEL, 48*PIXEL, 40*PIXEL, 52*PIXEL};
    constexpr float LARM_BOTTOM[4] = {40*PIXEL, 48*PIXEL, 44*PIXEL, 52*PIXEL};
    
    // Right Leg
    constexpr float RLEG_FRONT[4]  = {4*PIXEL,  20*PIXEL, 8*PIXEL,  32*PIXEL};
    constexpr float RLEG_BACK[4]   = {12*PIXEL, 20*PIXEL, 16*PIXEL, 32*PIXEL};
    constexpr float RLEG_RIGHT[4]  = {0*PIXEL,  20*PIXEL, 4*PIXEL,  32*PIXEL};
    constexpr float RLEG_LEFT[4]   = {8*PIXEL,  20*PIXEL, 12*PIXEL, 32*PIXEL};
    constexpr float RLEG_TOP[4]    = {4*PIXEL,  16*PIXEL, 8*PIXEL,  20*PIXEL};
    constexpr float RLEG_BOTTOM[4] = {8*PIXEL,  16*PIXEL, 12*PIXEL, 20*PIXEL};
    
    // Left Leg
    constexpr float LLEG_FRONT[4]  = {20*PIXEL, 52*PIXEL, 24*PIXEL, 64*PIXEL};
    constexpr float LLEG_BACK[4]   = {28*PIXEL, 52*PIXEL, 32*PIXEL, 64*PIXEL};
    constexpr float LLEG_RIGHT[4]  = {16*PIXEL, 52*PIXEL, 20*PIXEL, 64*PIXEL};
    constexpr float LLEG_LEFT[4]   = {24*PIXEL, 52*PIXEL, 28*PIXEL, 64*PIXEL};
    constexpr float LLEG_TOP[4]    = {20*PIXEL, 48*PIXEL, 24*PIXEL, 52*PIXEL};
    constexpr float LLEG_BOTTOM[4] = {24*PIXEL, 48*PIXEL, 28*PIXEL, 52*PIXEL};
}

// Helper to add a cuboid face with texture coordinates
inline void addCuboidFace(std::vector<float>& vertices, std::vector<unsigned int>& indices,
                          float x, float y, float z, float w, float h, float d,
                          int face, const float uv[4]) {
    unsigned int baseIndex = vertices.size() / 5;
    
    // Vertex format: [x, y, z, u, v]
    switch(face) {
        case 0: // Front (+Z)
            vertices.insert(vertices.end(), {
                x,     y,     z+d,  uv[0], uv[3],
                x+w,   y,     z+d,  uv[2], uv[3],
                x+w,   y+h,   z+d,  uv[2], uv[1],
                x,     y+h,   z+d,  uv[0], uv[1]
            });
            break;
        case 1: // Back (-Z)
            vertices.insert(vertices.end(), {
                x+w,   y,     z,    uv[0], uv[3],
                x,     y,     z,    uv[2], uv[3],
                x,     y+h,   z,    uv[2], uv[1],
                x+w,   y+h,   z,    uv[0], uv[1]
            });
            break;
        case 2: // Right (+X)
            vertices.insert(vertices.end(), {
                x+w,   y,     z+d,  uv[0], uv[3],
                x+w,   y,     z,    uv[2], uv[3],
                x+w,   y+h,   z,    uv[2], uv[1],
                x+w,   y+h,   z+d,  uv[0], uv[1]
            });
            break;
        case 3: // Left (-X)
            vertices.insert(vertices.end(), {
                x,     y,     z,    uv[0], uv[3],
                x,     y,     z+d,  uv[2], uv[3],
                x,     y+h,   z+d,  uv[2], uv[1],
                x,     y+h,   z,    uv[0], uv[1]
            });
            break;
        case 4: // Top (+Y)
            vertices.insert(vertices.end(), {
                x,     y+h,   z+d,  uv[0], uv[3],
                x+w,   y+h,   z+d,  uv[2], uv[3],
                x+w,   y+h,   z,    uv[2], uv[1],
                x,     y+h,   z,    uv[0], uv[1]
            });
            break;
        case 5: // Bottom (-Y)
            vertices.insert(vertices.end(), {
                x,     y,     z,    uv[0], uv[3],
                x+w,   y,     z,    uv[2], uv[3],
                x+w,   y,     z+d,  uv[2], uv[1],
                x,     y,     z+d,  uv[0], uv[1]
            });
            break;
    }
    
    // Two triangles per face
    indices.insert(indices.end(), {
        baseIndex, baseIndex+1, baseIndex+2,
        baseIndex, baseIndex+2, baseIndex+3
    });
}

// Helper to add a complete cuboid (all 6 faces)
inline void addCuboid(std::vector<float>& vertices, std::vector<unsigned int>& indices,
                      float x, float y, float z, float w, float h, float d,
                      const float* uvFront, const float* uvBack, const float* uvRight,
                      const float* uvLeft, const float* uvTop, const float* uvBottom) {
    addCuboidFace(vertices, indices, x, y, z, w, h, d, 0, uvFront);
    addCuboidFace(vertices, indices, x, y, z, w, h, d, 1, uvBack);
    addCuboidFace(vertices, indices, x, y, z, w, h, d, 2, uvRight);
    addCuboidFace(vertices, indices, x, y, z, w, h, d, 3, uvLeft);
    addCuboidFace(vertices, indices, x, y, z, w, h, d, 4, uvTop);
    addCuboidFace(vertices, indices, x, y, z, w, h, d, 5, uvBottom);
}

class PlayerModel {
public:
    GLuint vao, vbo, ebo;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    PlayerModel() {
        generateModel();
        setupBuffers();
    }
    
    void generateModel() {
        using namespace PlayerDimensions;
        
        // All parts are centered around origin (0, 0, 0) and positioned relative to feet
        // The model will be transformed in world space when rendering
        
        // Head - centered on X and Z, top of body
        float headX = -HEAD_WIDTH / 2.0f;
        float headY = BODY_HEIGHT + LEG_HEIGHT;
        float headZ = -HEAD_DEPTH / 2.0f;
        addCuboid(vertices, indices, headX, headY, headZ, HEAD_WIDTH, HEAD_HEIGHT, HEAD_DEPTH,
                  SkinUV::HEAD_FRONT, SkinUV::HEAD_BACK, SkinUV::HEAD_RIGHT, 
                  SkinUV::HEAD_LEFT, SkinUV::HEAD_TOP, SkinUV::HEAD_BOTTOM);
        
        // Body - centered on X and Z, above legs
        float bodyX = -BODY_WIDTH / 2.0f;
        float bodyY = LEG_HEIGHT;
        float bodyZ = -BODY_DEPTH / 2.0f;
        addCuboid(vertices, indices, bodyX, bodyY, bodyZ, BODY_WIDTH, BODY_HEIGHT, BODY_DEPTH,
                  SkinUV::BODY_FRONT, SkinUV::BODY_BACK, SkinUV::BODY_RIGHT,
                  SkinUV::BODY_LEFT, SkinUV::BODY_TOP, SkinUV::BODY_BOTTOM);
        
        // Right Arm - attached to right side of body
        float rarmX = -BODY_WIDTH / 2.0f - ARM_WIDTH;
        float rarmY = LEG_HEIGHT + BODY_HEIGHT - ARM_HEIGHT;
        float rarmZ = -ARM_DEPTH / 2.0f;
        addCuboid(vertices, indices, rarmX, rarmY, rarmZ, ARM_WIDTH, ARM_HEIGHT, ARM_DEPTH,
                  SkinUV::RARM_FRONT, SkinUV::RARM_BACK, SkinUV::RARM_RIGHT,
                  SkinUV::RARM_LEFT, SkinUV::RARM_TOP, SkinUV::RARM_BOTTOM);
        
        // Left Arm - attached to left side of body
        float larmX = BODY_WIDTH / 2.0f;
        float larmY = LEG_HEIGHT + BODY_HEIGHT - ARM_HEIGHT;
        float larmZ = -ARM_DEPTH / 2.0f;
        addCuboid(vertices, indices, larmX, larmY, larmZ, ARM_WIDTH, ARM_HEIGHT, ARM_DEPTH,
                  SkinUV::LARM_FRONT, SkinUV::LARM_BACK, SkinUV::LARM_RIGHT,
                  SkinUV::LARM_LEFT, SkinUV::LARM_TOP, SkinUV::LARM_BOTTOM);
        
        // Right Leg - bottom right
        float rlegX = -LEG_WIDTH - 0.01f;  // Small gap between legs
        float rlegY = 0.0f;
        float rlegZ = -LEG_DEPTH / 2.0f;
        addCuboid(vertices, indices, rlegX, rlegY, rlegZ, LEG_WIDTH, LEG_HEIGHT, LEG_DEPTH,
                  SkinUV::RLEG_FRONT, SkinUV::RLEG_BACK, SkinUV::RLEG_RIGHT,
                  SkinUV::RLEG_LEFT, SkinUV::RLEG_TOP, SkinUV::RLEG_BOTTOM);
        
        // Left Leg - bottom left
        float llegX = 0.01f;  // Small gap between legs
        float llegY = 0.0f;
        float llegZ = -LEG_DEPTH / 2.0f;
        addCuboid(vertices, indices, llegX, llegY, llegZ, LEG_WIDTH, LEG_HEIGHT, LEG_DEPTH,
                  SkinUV::LLEG_FRONT, SkinUV::LLEG_BACK, SkinUV::LLEG_RIGHT,
                  SkinUV::LLEG_LEFT, SkinUV::LLEG_TOP, SkinUV::LLEG_BOTTOM);
    }
    
    void setupBuffers() {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        
        glBindVertexArray(vao);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        
        // Position attribute (location 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        
        // Texture coordinate attribute (location 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        
        glBindVertexArray(0);
    }
    
    void draw() const {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    
    ~PlayerModel() {
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ebo);
    }
};

// Matrix helper functions for player rendering
// OpenGL uses column-major matrices: data[col * 4 + row]
inline mat4 matrixMultiply(const mat4& a, const mat4& b) {
    mat4 result;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result.data[col * 4 + row] = 0.0f;
            for (int k = 0; k < 4; ++k) {
                result.data[col * 4 + row] += a.data[k * 4 + row] * b.data[col * 4 + k];
            }
        }
    }
    return result;
}

// Column-major translation matrix
inline mat4 createTranslationMatrix(float x, float y, float z) {
    mat4 m;
    // Identity with translation in last column
    m.data[0] = 1;  m.data[4] = 0;  m.data[8] = 0;   m.data[12] = x;
    m.data[1] = 0;  m.data[5] = 1;  m.data[9] = 0;   m.data[13] = y;
    m.data[2] = 0;  m.data[6] = 0;  m.data[10] = 1;  m.data[14] = z;
    m.data[3] = 0;  m.data[7] = 0;  m.data[11] = 0;  m.data[15] = 1;
    return m;
}

// Column-major Y-axis rotation matrix (yaw)
inline mat4 createRotationMatrixY(float angleRadians) {
    mat4 m;
    float c = cosf(angleRadians);
    float s = sinf(angleRadians);
    // Rotation around Y axis
    m.data[0] = c;   m.data[4] = 0;  m.data[8] = s;   m.data[12] = 0;
    m.data[1] = 0;   m.data[5] = 1;  m.data[9] = 0;   m.data[13] = 0;
    m.data[2] = -s;  m.data[6] = 0;  m.data[10] = c;  m.data[14] = 0;
    m.data[3] = 0;   m.data[7] = 0;  m.data[11] = 0;  m.data[15] = 1;
    return m;
}

// Create model matrix for a player at given position with rotation
inline mat4 createPlayerModelMatrix(float x, float y, float z, float yawDegrees) {
    // Model matrix = Translation * Rotation
    // This transforms model space coordinates to world space
    mat4 translation = createTranslationMatrix(x, y, z);
    // Negate yaw to fix inverted rotation (player looks right = model turns right for others)
    mat4 rotation = createRotationMatrixY((-yawDegrees + 95.75f) * M_PI / 180.0f);
    return matrixMultiply(translation, rotation);
}

#endif // PLAYER_MODEL_HPP
