#ifndef GAME_RAYCAST_HPP
#define GAME_RAYCAST_HPP

inline Game::RaycastHit Game::raycast(float maxDistance) {
    RaycastHit result;
    result.hit = false;
    Vector3 origin = { camera.x, camera.y, camera.z };
    Vector3 direction = camera.getFrontVector();

    float len = std::sqrt(direction.x*direction.x + direction.y*direction.y + direction.z*direction.z);
    if (len != 0) {
        direction.x /= len;
        direction.y /= len;
        direction.z /= len;
    }

    int bx = static_cast<int>(std::floor(origin.x));
    int by = static_cast<int>(std::floor(origin.y));
    int bz = static_cast<int>(std::floor(origin.z));
    int prevX = bx, prevY = by, prevZ = bz;

    int stepX = (direction.x >= 0 ? 1 : -1);
    int stepY = (direction.y >= 0 ? 1 : -1);
    int stepZ = (direction.z >= 0 ? 1 : -1);

    float tMaxX = intbound(origin.x, direction.x);
    float tMaxY = intbound(origin.y, direction.y);
    float tMaxZ = intbound(origin.z, direction.z);
    float tDeltaX = (direction.x != 0 ? stepX / direction.x : INFINITY);
    float tDeltaY = (direction.y != 0 ? stepY / direction.y : INFINITY);
    float tDeltaZ = (direction.z != 0 ? stepZ / direction.z : INFINITY);
    float traveled = 0.0f;

    while (traveled <= maxDistance) {
        // Selectable includes solids and tall grass (non-solid)
        if (isBlockSelectable(bx, by, bz)) {
            result.hit = true;
            result.blockPosition = { bx, by, bz };
            result.adjacentPosition = { prevX, prevY, prevZ };
            return result;
        }

        prevX = bx; prevY = by; prevZ = bz;
        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                bx += stepX;
                traveled = tMaxX;
                tMaxX += tDeltaX;
            } else {
                bz += stepZ;
                traveled = tMaxZ;
                tMaxZ += tDeltaZ;
            }
        } else {
            if (tMaxY < tMaxZ) {
                by += stepY;
                traveled = tMaxY;
                tMaxY += tDeltaY;
            } else {
                bz += stepZ;
                traveled = tMaxZ;
                tMaxZ += tDeltaZ;
            }
        }
    }

    return result;
}

inline float Game::intbound(float s, float ds) {
    if (ds == 0.0f) {
        return INFINITY;
    } else {
        float sFloor = std::floor(s);
        if (ds > 0) {
            return (sFloor + 1.0f - s) / ds;
        } else {
            return (s - sFloor) / -ds;
        }
    }
}

#endif // GAME_RAYCAST_HPP