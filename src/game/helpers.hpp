#ifndef GAME_HELPERS_HPP
#define GAME_HELPERS_HPP

inline float Game::calculateDeltaTime() {
    auto now = std::chrono::steady_clock::now();
    float delta = std::chrono::duration<float>(now - lastFrame).count();
    lastFrame = now;
    return delta;
}

#endif // GAME_HELPERS_HPP