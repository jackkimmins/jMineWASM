// game/hotbar.hpp
#ifndef GAME_HOTBAR_HPP
#define GAME_HOTBAR_HPP

#include <vector>

inline void Game::renderHotbar() {
    // Hotbar configuration
    const int slotSize = 50;        // Size of each hotbar slot
    const int slotSpacing = 4;      // Space between slots
    const int totalSlots = 9;
    const int hotbarWidth = totalSlots * slotSize + (totalSlots - 1) * slotSpacing;
    const int hotbarHeight = slotSize;
    const int hotbarX = (canvasWidth - hotbarWidth) / 2;
    const int hotbarY = canvasHeight - hotbarHeight - 20; // 20px from bottom
    const int iconPadding = 4;      // Padding inside each slot for the block icon
    
    // Switch to orthographic projection for 2D UI
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    
    // Use text renderer's 2D projection for drawing slots
    mat4 orthoProj = textRenderer.getProjectionMatrix();
    
    // Draw each slot background and selection indicator
    for (int i = 0; i < totalSlots; i++) {
        int slotX = hotbarX + i * (slotSize + slotSpacing);
        int slotY = hotbarY;
        
        bool isSelected = (i == hotbarInventory.getSelectedSlot());
        
        // Draw slot background (darker rectangle)
        float bgColor = isSelected ? 0.3f : 0.2f;
        float bgAlpha = isSelected ? 0.9f : 0.7f;
        
        // Draw semi-transparent background
        textRenderer.drawBox(slotX, slotY, slotSize, slotSize, bgColor, bgColor, bgColor, bgAlpha);
        
        // Draw border
        if (isSelected) {
            // Bright white border for selected slot
            textRenderer.drawBoxOutline(slotX, slotY, slotSize, slotSize, 1.0f, 1.0f, 1.0f, 1.0f, 3.0f);
        } else {
            // Subtle gray border for unselected slots
            textRenderer.drawBoxOutline(slotX, slotY, slotSize, slotSize, 0.5f, 0.5f, 0.5f, 0.6f, 2.0f);
        }
    }
    
    // Now render block face textures in each slot
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Use hotbar shader for texture rendering
    hotbarShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureAtlas);
    GLint texLoc = hotbarShader->getUniform("uTexture");
    glUniform1i(texLoc, 0);
    
    glBindVertexArray(hotbarVAO);
    
    for (int i = 0; i < totalSlots; i++) {
        if (hotbarInventory.isSlotEmpty(i)) {
            continue; // Skip empty slots
        }
        
        BlockType blockType = hotbarInventory.getBlockTypeAt(i);
        int slotX = hotbarX + i * (slotSize + slotSpacing);
        int slotY = hotbarY;
        
        // Calculate icon position with padding
        float iconX = slotX + iconPadding;
        float iconY = slotY + iconPadding;
        float iconSize = slotSize - 2 * iconPadding;
        
        // Convert to NDC (Normalized Device Coordinates)
        float x0 = (iconX / float(canvasWidth)) * 2.0f - 1.0f;
        float y0 = 1.0f - (iconY / float(canvasHeight)) * 2.0f;
        float x1 = ((iconX + iconSize) / float(canvasWidth)) * 2.0f - 1.0f;
        float y1 = 1.0f - ((iconY + iconSize) / float(canvasHeight)) * 2.0f;
        
        // Get texture coordinates for the top face of this block
        int texIdx = meshManager.getTextureIndex(blockType, FACE_TOP);
        int tileX = texIdx % 8; // 8 tiles per row in 128x128 atlas with 16x16 tiles
        int tileY = texIdx / 8;
        
        const float uvInset = 0.001f;
        float u0 = (tileX * 16.0f) / 128.0f + uvInset;
        float v0 = (tileY * 16.0f) / 128.0f + uvInset;
        float u1 = ((tileX + 1) * 16.0f) / 128.0f - uvInset;
        float v1 = ((tileY + 1) * 16.0f) / 128.0f - uvInset;
        
        // Create a simple quad (2 triangles) showing the block face
        float vertices[20] = {
            // Position (x, y, z), TexCoord (u, v)
            x0, y1, 0.0f,  u0, v1,  // Bottom-left
            x1, y1, 0.0f,  u1, v1,  // Bottom-right
            x1, y0, 0.0f,  u1, v0,  // Top-right
            x0, y0, 0.0f,  u0, v0   // Top-left
        };
        
        static const unsigned int indices[6] = { 0, 1, 2, 2, 3, 0 };
        
        // Upload vertices
        glBindBuffer(GL_ARRAY_BUFFER, hotbarVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, hotbarEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        
        // Use identity matrix for MVP (vertices already in NDC)
        mat4 identityMat = {0};
        identityMat.data[0] = identityMat.data[5] = identityMat.data[10] = identityMat.data[15] = 1.0f;
        glUniformMatrix4fv(hotbarMvpLoc, 1, GL_FALSE, identityMat.data);
        
        // Draw the block icon
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
    
    glBindVertexArray(0);
    
    // Restore state
    glEnable(GL_DEPTH_TEST);
    
    // Restore main shader
    shader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureAtlas);
}

#endif // GAME_HOTBAR_HPP
