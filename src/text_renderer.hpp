// text_renderer.hpp
#ifndef TEXT_RENDERER_HPP
#define TEXT_RENDERER_HPP

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#endif

#include <string>
#include <vector>
#include <array>
#include <iostream>
#include "shaders.hpp"

// Forward declare stbi_load and stbi_image_free (defined in stb_image.h which is included in main.cpp)
extern "C" {
    unsigned char *stbi_load(char const *filename, int *x, int *y, int *channels_in_file, int desired_channels);
    void stbi_image_free(void *retval_from_stbi_load);
}

class TextRenderer {
public:
    TextRenderer() : fontTexture(0), textShader(nullptr), overlayShader(nullptr),
                     textVAO(0), textVBO(0), overlayVAO(0), overlayVBO(0),
                     screenWidth(1920), screenHeight(1080) {}

    ~TextRenderer() {
        cleanup();
    }

    bool init(int width, int height) {
        screenWidth = width;
        screenHeight = height;

        // Load font texture
        if (!loadFontTexture("/assets/font.png")) {
            std::cerr << "Failed to load font texture" << std::endl;
            return false;
        }

        // Create text shader (2D with texture)
        const char* textVertexSrc = R"(#version 300 es
            precision mediump float;
            layout(location = 0) in vec2 aPos;
            layout(location = 1) in vec2 aTexCoord;

            uniform mat4 uProjection;
            out vec2 TexCoord;

            void main() {
                gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
                TexCoord = aTexCoord;
            })";

        const char* textFragmentSrc = R"(#version 300 es
            precision mediump float;

            in vec2 TexCoord;
            uniform sampler2D uFontTexture;
            uniform vec3 uTextColor;
            uniform float uAlpha;
            out vec4 FragColor;

            void main() {
                float alpha = texture(uFontTexture, TexCoord).a;
                if (alpha < 0.1) discard;
                FragColor = vec4(uTextColor, alpha * uAlpha);
            })";

        textShader = new Shader(textVertexSrc, textFragmentSrc);
        projectionLoc = textShader->getUniform("uProjection");
        textColorLoc = textShader->getUniform("uTextColor");
        textAlphaLoc = textShader->getUniform("uAlpha");
        fontTextureLoc = textShader->getUniform("uFontTexture");

        // Create overlay shader (for screen darkening)
        const char* overlayVertexSrc = R"(#version 300 es
            precision mediump float;
            layout(location = 0) in vec2 aPos;

            void main() {
                gl_Position = vec4(aPos, 0.0, 1.0);
            })";

        const char* overlayFragmentSrc = R"(#version 300 es
            precision mediump float;

            uniform vec4 uColor;
            out vec4 FragColor;

            void main() {
                FragColor = uColor;
            })";

        overlayShader = new Shader(overlayVertexSrc, overlayFragmentSrc);
        overlayColorLoc = overlayShader->getUniform("uColor");

        // Setup VAO/VBO for text rendering
        glGenVertexArrays(1, &textVAO);
        glGenBuffers(1, &textVBO);

        // Setup VAO/VBO for overlay rendering
        glGenVertexArrays(1, &overlayVAO);
        glGenBuffers(1, &overlayVBO);

        glBindVertexArray(overlayVAO);
        glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);

        // Full-screen quad
        float overlayVertices[] = {
            -1.0f,  1.0f,
            -1.0f, -1.0f,
             1.0f, -1.0f,
            -1.0f,  1.0f,
             1.0f, -1.0f,
             1.0f,  1.0f
        };

        glBufferData(GL_ARRAY_BUFFER, sizeof(overlayVertices), overlayVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);

        updateProjection(width, height);

        std::cout << "TextRenderer initialized successfully" << std::endl;
        return true;
    }

    void updateProjection(int width, int height) {
        screenWidth = width;
        screenHeight = height;

        // Create orthographic projection matrix for 2D rendering
        // Maps (0,0) to top-left, (width, height) to bottom-right
        float left = 0.0f;
        float right = static_cast<float>(width);
        float bottom = static_cast<float>(height);
        float top = 0.0f;
        float nearPlane = -1.0f;
        float farPlane = 1.0f;

        projectionMatrix[0] = 2.0f / (right - left);
        projectionMatrix[1] = 0.0f;
        projectionMatrix[2] = 0.0f;
        projectionMatrix[3] = 0.0f;

        projectionMatrix[4] = 0.0f;
        projectionMatrix[5] = 2.0f / (top - bottom);
        projectionMatrix[6] = 0.0f;
        projectionMatrix[7] = 0.0f;

        projectionMatrix[8] = 0.0f;
        projectionMatrix[9] = 0.0f;
        projectionMatrix[10] = -2.0f / (farPlane - nearPlane);
        projectionMatrix[11] = 0.0f;

        projectionMatrix[12] = -(right + left) / (right - left);
        projectionMatrix[13] = -(top + bottom) / (top - bottom);
        projectionMatrix[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
        projectionMatrix[15] = 1.0f;
    }

    void drawText(const std::string& text, float x, float y, float scale = 1.0f,
                  float r = 1.0f, float g = 1.0f, float b = 1.0f, float alpha = 1.0f) {

        if (text.empty()) return;

        // Atlas/grid constants
        const float cellW = 8.0f;
        const float cellH = 8.0f;
        const float atlasW = 128.0f;
        const float atlasH = 128.0f;
        const int   cols   = 16;

        std::vector<float> vertices;
        vertices.reserve(text.length() * 24); // 6 vertices * 4 floats per character

        float currentX = x;
        float currentY = y;

        for (char c : text) {
            unsigned char ch = static_cast<unsigned char>(c);

            // Special characters
            if (ch == '\n') {
                currentY += cellH * scale;
                currentX = x;
                continue;
            }
            if (ch == '\t') {
                currentX += (tabSpaces * spaceAdvancePx()) * scale;
                continue;
            }

            int idx = static_cast<int>(ch) - firstCodepoint;
            if (idx < 0 || idx >= glyphCount) {
                // Unknown: advance like space and skip drawing
                currentX += spaceAdvancePx() * scale;
                continue;
            }

            // UVs for this cell
            int col = idx % cols;
            int row = idx / cols;

            float u0 = (col * cellW) / atlasW;
            float v0 = (row * cellH) / atlasH;
            float u1 = u0 + (cellW / atlasW);
            float v1 = v0 + (cellH / atlasH);

            // Quad is fixed to the cell size to avoid texture stretching
            const float qw = cellW * scale;
            const float qh = cellH * scale;

            // Two triangles (quad)
            vertices.push_back(currentX);        vertices.push_back(currentY);        vertices.push_back(u0); vertices.push_back(v0);
            vertices.push_back(currentX + qw);   vertices.push_back(currentY);        vertices.push_back(u1); vertices.push_back(v0);
            vertices.push_back(currentX);        vertices.push_back(currentY + qh);   vertices.push_back(u0); vertices.push_back(v1);

            vertices.push_back(currentX + qw);   vertices.push_back(currentY);        vertices.push_back(u1); vertices.push_back(v0);
            vertices.push_back(currentX + qw);   vertices.push_back(currentY + qh);   vertices.push_back(u1); vertices.push_back(v1);
            vertices.push_back(currentX);        vertices.push_back(currentY + qh);   vertices.push_back(u0); vertices.push_back(v1);

            // Advance by per-glyph width (in pixels)
            currentX += advancePx(ch) * scale;
        }

        if (vertices.empty()) return;

        // Render the text
        textShader->use();
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, projectionMatrix);
        glUniform3f(textColorLoc, r, g, b);
        glUniform1f(textAlphaLoc, alpha);

        // Bind font texture to texture unit 1 (avoid conflict with block atlas on unit 0)
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, fontTexture);
        glUniform1i(fontTextureLoc, 1);

        glBindVertexArray(textVAO);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

        // Position attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        // Texture coordinate attribute
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 4));

        glBindVertexArray(0);

        // Restore OpenGL state: switch back to texture unit 0
        glActiveTexture(GL_TEXTURE0);
    }

    // Draw centered text (single line). Uses per-glyph advances.
    void drawTextCentered(const std::string& text, float y, float scale = 1.0f,
                          float r = 1.0f, float g = 1.0f, float b = 1.0f, float alpha = 1.0f) {
        float w = measureLineWidth(text, scale);
        float x = (screenWidth - w) * 0.5f;
        drawText(text, x, y, scale, r, g, b, alpha);
    }

    // Optional: measure the width of a single line (stops at '\n')
    float measureLineWidth(const std::string& text, float scale = 1.0f) const {
        float width = 0.0f;
        for (char c : text) {
            unsigned char ch = static_cast<unsigned char>(c);
            if (ch == '\n') break;
            if (ch == '\t') { width += (tabSpaces * spaceAdvancePx()) * scale; continue; }
            width += advancePx(ch) * scale;
        }
        return width;
    }

    // Draw screen overlay (for darkening the screen)
    void drawOverlay(float r, float g, float b, float alpha) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        overlayShader->use();
        glUniform4f(overlayColorLoc, r, g, b, alpha);

        glBindVertexArray(overlayVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        
        glDisable(GL_BLEND);
    }
    
    // Draw a rectangle at specified screen coordinates
    void drawRect(float x, float y, float width, float height, float r, float g, float b, float alpha) {
        overlayShader->use();
        glUniform4f(overlayColorLoc, r, g, b, alpha);
        
        // Convert screen coordinates to NDC (-1 to 1)
        float ndcLeft = (x / screenWidth) * 2.0f - 1.0f;
        float ndcRight = ((x + width) / screenWidth) * 2.0f - 1.0f;
        float ndcTop = 1.0f - (y / screenHeight) * 2.0f;
        float ndcBottom = 1.0f - ((y + height) / screenHeight) * 2.0f;
        
        float vertices[] = {
            ndcLeft,  ndcTop,
            ndcLeft,  ndcBottom,
            ndcRight, ndcBottom,
            ndcLeft,  ndcTop,
            ndcRight, ndcBottom,
            ndcRight, ndcTop
        };
        
        GLuint tempVAO, tempVBO;
        glGenVertexArrays(1, &tempVAO);
        glGenBuffers(1, &tempVBO);
        glBindVertexArray(tempVAO);
        glBindBuffer(GL_ARRAY_BUFFER, tempVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glDeleteBuffers(1, &tempVBO);
        glDeleteVertexArrays(1, &tempVAO);
    }

    int getScreenWidth() const { return screenWidth; }
    int getScreenHeight() const { return screenHeight; }
    
    // Get the projection matrix for 2D rendering
    mat4 getProjectionMatrix() const {
        mat4 result;
        for (int i = 0; i < 16; i++) {
            result.data[i] = projectionMatrix[i];
        }
        return result;
    }
    
    // Draw a filled box at specified screen coordinates
    void drawBox(float x, float y, float width, float height, float r, float g, float b, float alpha) {
        drawRect(x, y, width, height, r, g, b, alpha);
    }
    
    // Draw a box outline at specified screen coordinates
    void drawBoxOutline(float x, float y, float width, float height, float r, float g, float b, float alpha, float thickness = 2.0f) {
        // Top edge
        drawRect(x, y, width, thickness, r, g, b, alpha);
        // Bottom edge
        drawRect(x, y + height - thickness, width, thickness, r, g, b, alpha);
        // Left edge
        drawRect(x, y, thickness, height, r, g, b, alpha);
        // Right edge
        drawRect(x + width - thickness, y, thickness, height, r, g, b, alpha);
    }

private:
    // ===== Font metrics (baked from JSON) =====
    static constexpr int firstCodepoint = 32;
    static constexpr int lastCodepoint  = 126;
    static constexpr int glyphCount     = lastCodepoint - firstCodepoint + 1;
    static constexpr int tabSpaces      = 4;

    // Lazily-initialized static to keep header-only
    static const std::array<uint8_t, glyphCount>& advanceTable() {
        static const std::array<uint8_t, glyphCount> t = {
            /* 32..126 advance widths in pixels */
            4, 2, 4, 7, 6, 7, 6, 2, 4, 4, 6, 6, 3, 6, 2, 7,
            7, 4, 7, 6, 7, 7, 7, 7, 7, 7, 2, 3, 4, 6, 4, 6,
            6, 7, 7, 7, 7, 7, 7, 7, 7, 5, 7, 7, 7, 7, 7, 7,
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 4, 7, 4, 4, 7,
            3, 6, 6, 6, 6, 7, 5, 7, 6, 2, 4, 6, 2, 7, 6, 7,
            6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 4, 2, 4, 6
        };
        return t;
    }

    inline float advancePx(unsigned char ch) const {
        if (ch < firstCodepoint || ch > lastCodepoint) return spaceAdvancePx();
        return static_cast<float>(advanceTable()[ch - firstCodepoint]);
    }
    inline static float spaceAdvancePx() { return static_cast<float>(advanceTable()[0]); } // codepoint 32

private:
    GLuint fontTexture;
    Shader* textShader;
    Shader* overlayShader;
    GLuint textVAO, textVBO;
    GLuint overlayVAO, overlayVBO;
    int screenWidth, screenHeight;

    GLint projectionLoc;
    GLint textColorLoc;
    GLint textAlphaLoc;
    GLint fontTextureLoc;
    GLint overlayColorLoc;

    float projectionMatrix[16] = {0};

    bool loadFontTexture(const char* path) {
        int width, height, channels;
        unsigned char* data = stbi_load(path, &width, &height, &channels, 4);

        if (!data) {
            std::cerr << "Failed to load font texture: " << path << std::endl;
            return false;
        }

        glGenTextures(1, &fontTexture);
        glBindTexture(GL_TEXTURE_2D, fontTexture);

        // Use nearest filtering for crisp pixel font
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

        stbi_image_free(data);

        std::cout << "Loaded font texture: " << width << "x" << height << std::endl;
        return true;
    }

    void cleanup() {
        if (fontTexture) {
            glDeleteTextures(1, &fontTexture);
            fontTexture = 0;
        }
        if (textVAO) {
            glDeleteVertexArrays(1, &textVAO);
            textVAO = 0;
        }
        if (textVBO) {
            glDeleteBuffers(1, &textVBO);
            textVBO = 0;
        }
        if (overlayVAO) {
            glDeleteVertexArrays(1, &overlayVAO);
            overlayVAO = 0;
        }
        if (overlayVBO) {
            glDeleteBuffers(1, &overlayVBO);
            overlayVBO = 0;
        }
        if (textShader) {
            delete textShader;
            textShader = nullptr;
        }
        if (overlayShader) {
            delete overlayShader;
            overlayShader = nullptr;
        }
    }
};

#endif // TEXT_RENDERER_HPP
