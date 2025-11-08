#ifndef GAME_MATH_HPP
#define GAME_MATH_HPP

inline mat4 Game::perspective(float fov, float aspect, float near, float far) const {
    mat4 proj;
    float tanHalfFovy = tanf(fov / 2.0f);
    proj.data[0] = 1.0f / (aspect * tanHalfFovy);
    proj.data[5] = 1.0f / tanHalfFovy;
    proj.data[10] = -(far + near) / (far - near);
    proj.data[11] = -1.0f;
    proj.data[14] = -(2.0f * far * near) / (far - near);
    return proj;
}

inline mat4 Game::multiply(const mat4& a, const mat4& b) const {
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

#endif // GAME_MATH_HPP