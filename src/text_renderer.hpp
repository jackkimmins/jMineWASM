// text_renderer.hpp
#ifndef TEXT_RENDERER_HPP
#define TEXT_RENDERER_HPP

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#endif

#include <string>
#include <vector>
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

        // Character size in the font atlas
        const float charWidth = 8.0f;
        const float charHeight = 8.0f;
        const float atlasWidth = 128.0f;
        const float atlasHeight = 128.0f;
        const int charsPerRow = 16;

        std::vector<float> vertices;
        vertices.reserve(text.length() * 24); // 6 vertices * 4 floats per character

        float currentX = x;
        float currentY = y;

        for (char c : text) {
            unsigned char ch = static_cast<unsigned char>(c);
            
            // Handle special characters
            if (ch == '\n') {
                currentY += charHeight * scale;
                currentX = x;
                continue;
            }
            if (ch == '\t') {
                currentX += charWidth * scale * 4;
                continue;
            }

            // Calculate texture coordinates
            int charIndex = ch - 32; // Font starts at ASCII 32 (space)
            if (charIndex < 0 || charIndex >= 95) {
                charIndex = 0; // Default to space for invalid characters
            }

            int col = charIndex % charsPerRow;
            int row = charIndex / charsPerRow;

            float u0 = (col * charWidth) / atlasWidth;
            float v0 = (row * charHeight) / atlasHeight;
            float u1 = u0 + (charWidth / atlasWidth);
            float v1 = v0 + (charHeight / atlasHeight);

            // Character dimensions on screen
            float w = charWidth * scale;
            float h = charHeight * scale;

            // Two triangles forming a quad
            // Triangle 1
            vertices.push_back(currentX);     vertices.push_back(currentY);     vertices.push_back(u0); vertices.push_back(v0);
            vertices.push_back(currentX + w); vertices.push_back(currentY);     vertices.push_back(u1); vertices.push_back(v0);
            vertices.push_back(currentX);     vertices.push_back(currentY + h); vertices.push_back(u0); vertices.push_back(v1);
            
            // Triangle 2
            vertices.push_back(currentX + w); vertices.push_back(currentY);     vertices.push_back(u1); vertices.push_back(v0);
            vertices.push_back(currentX + w); vertices.push_back(currentY + h); vertices.push_back(u1); vertices.push_back(v1);
            vertices.push_back(currentX);     vertices.push_back(currentY + h); vertices.push_back(u0); vertices.push_back(v1);

            currentX += w;
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

        glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 4);
        
        glBindVertexArray(0);
        
        // Restore OpenGL state: switch back to texture unit 0
        // Note: We don't rebind textures here - that's the responsibility of the caller
        glActiveTexture(GL_TEXTURE0);
    }

    // Draw centered text
    void drawTextCentered(const std::string& text, float y, float scale = 1.0f,
                          float r = 1.0f, float g = 1.0f, float b = 1.0f, float alpha = 1.0f) {
        float charWidth = 8.0f * scale;
        float textWidth = text.length() * charWidth;
        float x = (screenWidth - textWidth) / 2.0f;
        drawText(text, x, y, scale, r, g, b, alpha);
    }

    // Draw screen overlay (for darkening the screen)
    void drawOverlay(float r, float g, float b, float alpha) {
        overlayShader->use();
        glUniform4f(overlayColorLoc, r, g, b, alpha);
        
        glBindVertexArray(overlayVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    int getScreenWidth() const { return screenWidth; }
    int getScreenHeight() const { return screenHeight; }

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
