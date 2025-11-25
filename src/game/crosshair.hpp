#ifndef GAME_CROSSHAIR_HPP
#define GAME_CROSSHAIR_HPP

inline void Game::renderCrosshair() {
    if (canvasWidth == 0 || canvasHeight == 0) return;
    bool needsUpload = (canvasWidth != lastCrosshairWidth) || (canvasHeight != lastCrosshairHeight) || crosshairVertexCount == 0;
    
    // Calculate center of screen
    float centerX = canvasWidth / 2.0f;
    float centerY = canvasHeight / 2.0f;
    
    // Crosshair dimensions (in pixels)
    float crosshairSize = 10.0f;  // Length of each arm
    float crosshairThickness = 2.0f;  // Thickness of the lines
    float crosshairGap = 0.0f;  // No gap - connected in the center
    
    // Create orthographic projection matrix for 2D rendering
    float left = 0.0f;
    float right = static_cast<float>(canvasWidth);
    float bottom = static_cast<float>(canvasHeight);
    float top = 0.0f;
    float nearPlane = -1.0f;
    float farPlane = 1.0f;
    
    mat4 orthoProj;
    orthoProj.data[0] = 2.0f / (right - left);
    orthoProj.data[1] = 0.0f;
    orthoProj.data[2] = 0.0f;
    orthoProj.data[3] = 0.0f;
    
    orthoProj.data[4] = 0.0f;
    orthoProj.data[5] = 2.0f / (top - bottom);
    orthoProj.data[6] = 0.0f;
    orthoProj.data[7] = 0.0f;
    
    orthoProj.data[8] = 0.0f;
    orthoProj.data[9] = 0.0f;
    orthoProj.data[10] = -2.0f / (farPlane - nearPlane);
    orthoProj.data[11] = 0.0f;
    
    orthoProj.data[12] = -(right + left) / (right - left);
    orthoProj.data[13] = -(top + bottom) / (top - bottom);
    orthoProj.data[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    orthoProj.data[15] = 1.0f;
    
    // Rebuild vertex data only when size changes
    if (needsUpload) {
        lastCrosshairWidth = canvasWidth;
        lastCrosshairHeight = canvasHeight;
        
        float leftX = centerX - crosshairGap - crosshairSize;
        float leftY = centerY - crosshairThickness / 2.0f;
        
        float rightX = centerX + crosshairGap;
        float rightY = centerY - crosshairThickness / 2.0f;
        
        float topX = centerX - crosshairThickness / 2.0f;
        float topY = centerY - crosshairGap - crosshairSize;
        
        float bottomX = centerX - crosshairThickness / 2.0f;
        float bottomY = centerY + crosshairGap;
        
        float verts[48] = {
            // Left arm
            leftX, leftY,
            leftX + crosshairSize, leftY,
            leftX, leftY + crosshairThickness,
            leftX + crosshairSize, leftY,
            leftX + crosshairSize, leftY + crosshairThickness,
            leftX, leftY + crosshairThickness,
            // Right arm
            rightX, rightY,
            rightX + crosshairSize, rightY,
            rightX, rightY + crosshairThickness,
            rightX + crosshairSize, rightY,
            rightX + crosshairSize, rightY + crosshairThickness,
            rightX, rightY + crosshairThickness,
            // Top arm
            topX, topY,
            topX + crosshairThickness, topY,
            topX, topY + crosshairSize,
            topX + crosshairThickness, topY,
            topX + crosshairThickness, topY + crosshairSize,
            topX, topY + crosshairSize,
            // Bottom arm
            bottomX, bottomY,
            bottomX + crosshairThickness, bottomY,
            bottomX, bottomY + crosshairSize,
            bottomX + crosshairThickness, bottomY,
            bottomX + crosshairThickness, bottomY + crosshairSize,
            bottomX, bottomY + crosshairSize
        };
        
        glBindVertexArray(crosshairVAO);
        glBindBuffer(GL_ARRAY_BUFFER, crosshairVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        crosshairVertexCount = 24; // 4 quads * 6 vertices
    }
    
    // Use crosshair shader
    crosshairShader->use();
    glUniformMatrix4fv(crosshairProjLoc, 1, GL_FALSE, orthoProj.data);
    
    glBindVertexArray(crosshairVAO);
    
    // Enable blending with invert mode for color inversion effect
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
    
    // Disable depth test for 2D overlay
    glDisable(GL_DEPTH_TEST);
    
    glDrawArrays(GL_TRIANGLES, 0, crosshairVertexCount);
    
    // Restore OpenGL state
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glBindVertexArray(0);
}

#endif // GAME_CROSSHAIR_HPP
