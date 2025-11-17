#ifndef GAME_CROSSHAIR_HPP
#define GAME_CROSSHAIR_HPP

inline void Game::renderCrosshair() {
    int width, height;
    emscripten_get_canvas_element_size("canvas", &width, &height);
    
    // Calculate center of screen
    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    
    // Crosshair dimensions (in pixels)
    float crosshairSize = 10.0f;  // Length of each arm
    float crosshairThickness = 2.0f;  // Thickness of the lines
    float crosshairGap = 0.0f;  // No gap - connected in the center
    
    // Create orthographic projection matrix for 2D rendering
    float left = 0.0f;
    float right = static_cast<float>(width);
    float bottom = static_cast<float>(height);
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
    
    // Define crosshair vertices (4 rectangles forming a plus sign)
    // Horizontal line (left arm)
    float leftX = centerX - crosshairGap - crosshairSize;
    float leftY = centerY - crosshairThickness / 2.0f;
    
    // Horizontal line (right arm)
    float rightX = centerX + crosshairGap;
    float rightY = centerY - crosshairThickness / 2.0f;
    
    // Vertical line (top arm)
    float topX = centerX - crosshairThickness / 2.0f;
    float topY = centerY - crosshairGap - crosshairSize;
    
    // Vertical line (bottom arm)
    float bottomX = centerX - crosshairThickness / 2.0f;
    float bottomY = centerY + crosshairGap;
    
    // Build vertex array with 4 rectangles (2 triangles each = 6 vertices per rectangle)
    std::vector<float> vertices;
    
    // Left arm (horizontal)
    vertices.push_back(leftX); vertices.push_back(leftY);
    vertices.push_back(leftX + crosshairSize); vertices.push_back(leftY);
    vertices.push_back(leftX); vertices.push_back(leftY + crosshairThickness);
    
    vertices.push_back(leftX + crosshairSize); vertices.push_back(leftY);
    vertices.push_back(leftX + crosshairSize); vertices.push_back(leftY + crosshairThickness);
    vertices.push_back(leftX); vertices.push_back(leftY + crosshairThickness);
    
    // Right arm (horizontal)
    vertices.push_back(rightX); vertices.push_back(rightY);
    vertices.push_back(rightX + crosshairSize); vertices.push_back(rightY);
    vertices.push_back(rightX); vertices.push_back(rightY + crosshairThickness);
    
    vertices.push_back(rightX + crosshairSize); vertices.push_back(rightY);
    vertices.push_back(rightX + crosshairSize); vertices.push_back(rightY + crosshairThickness);
    vertices.push_back(rightX); vertices.push_back(rightY + crosshairThickness);
    
    // Top arm (vertical)
    vertices.push_back(topX); vertices.push_back(topY);
    vertices.push_back(topX + crosshairThickness); vertices.push_back(topY);
    vertices.push_back(topX); vertices.push_back(topY + crosshairSize);
    
    vertices.push_back(topX + crosshairThickness); vertices.push_back(topY);
    vertices.push_back(topX + crosshairThickness); vertices.push_back(topY + crosshairSize);
    vertices.push_back(topX); vertices.push_back(topY + crosshairSize);
    
    // Bottom arm (vertical)
    vertices.push_back(bottomX); vertices.push_back(bottomY);
    vertices.push_back(bottomX + crosshairThickness); vertices.push_back(bottomY);
    vertices.push_back(bottomX); vertices.push_back(bottomY + crosshairSize);
    
    vertices.push_back(bottomX + crosshairThickness); vertices.push_back(bottomY);
    vertices.push_back(bottomX + crosshairThickness); vertices.push_back(bottomY + crosshairSize);
    vertices.push_back(bottomX); vertices.push_back(bottomY + crosshairSize);
    
    // Use crosshair shader
    crosshairShader->use();
    glUniformMatrix4fv(crosshairProjLoc, 1, GL_FALSE, orthoProj.data);
    
    // Bind VAO and upload vertex data
    glBindVertexArray(crosshairVAO);
    glBindBuffer(GL_ARRAY_BUFFER, crosshairVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    
    // Enable blending with invert mode for color inversion effect
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
    
    // Disable depth test for 2D overlay
    glDisable(GL_DEPTH_TEST);
    
    // Draw the crosshair
    glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 2);
    
    // Restore OpenGL state
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glBindVertexArray(0);
}

#endif // GAME_CROSSHAIR_HPP
