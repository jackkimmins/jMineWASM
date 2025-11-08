#ifndef REMOTE_PLAYER_HPP
#define REMOTE_PLAYER_HPP

#include <string>
#include <chrono>

struct RemotePlayer {
    std::string id;
    float x, y, z;
    float yaw, pitch;
    std::chrono::steady_clock::time_point lastUpdate;

    RemotePlayer() : x(0), y(0), z(0), yaw(0), pitch(0) {}

    RemotePlayer(const std::string& playerId, float px, float py, float pz, float pyaw = 0, float ppitch = 0)
        : id(playerId), x(px), y(py), z(pz), yaw(pyaw), pitch(ppitch),
          lastUpdate(std::chrono::steady_clock::now()) {}
};

#endif // REMOTE_PLAYER_HPP