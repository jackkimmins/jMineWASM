// camera.hpp

#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <cmath>
#include "../shared/config.hpp"
#include "../shared/types.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class Camera {
public:
    float yaw = -90.0f, pitch = 0.0f;
    float x = WORLD_SIZE_X / 2.0f, y = WORLD_SIZE_Y / 2.0f, z = WORLD_SIZE_Z / 2.0f;

    mat4 getViewMatrix() const {
        float radYaw = yaw * M_PI / 180.0f;
        float radPitch = pitch * M_PI / 180.0f;
        float frontX = cosf(radYaw) * cosf(radPitch);
        float frontY = sinf(radPitch);
        float frontZ = sinf(radYaw) * cosf(radPitch);
        return lookAt(x, y, z, x + frontX, y + frontY, z + frontZ, 0.0f, 1.0f, 0.0f);
    }

    Vector3 getFrontVector() const {
        float radYaw = yaw * M_PI / 180.0f;
        float radPitch = pitch * M_PI / 180.0f;
        float frontX = cosf(radYaw) * cosf(radPitch);
        float frontY = sinf(radPitch);
        float frontZ = sinf(radYaw) * cosf(radPitch);
        return { frontX, frontY, frontZ };
    }

    Vector3 getRightVector() const {
        float radYaw = (yaw + 90.0f) * M_PI / 180.0f;
        float rightX = cosf(radYaw);
        float rightY = 0.0f;
        float rightZ = sinf(radYaw);
        return { rightX, rightY, rightZ };
    }

private:
    mat4 lookAt(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ) const {
        float f[3] = { centerX - eyeX, centerY - eyeY, centerZ - eyeZ };
        float f_mag = std::sqrt(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
        for(auto &val : f) val /= f_mag;

        float up[3] = { upX, upY, upZ };
        float up_mag = std::sqrt(up[0]*up[0] + up[1]*up[1] + up[2]*up[2]);
        for(auto &val : up) val /= up_mag;

        float s[3] = {
            f[1]*up[2] - f[2]*up[1],
            f[2]*up[0] - f[0]*up[2],
            f[0]*up[1] - f[1]*up[0]
        };
        float s_mag = std::sqrt(s[0]*s[0] + s[1]*s[1] + s[2]*s[2]);
        for(auto &val : s) val /= s_mag;

        float u[3] = {
            s[1]*f[2] - s[2]*f[1],
            s[2]*f[0] - s[0]*f[2],
            s[0]*f[1] - s[1]*f[0]
        };

        mat4 view;
        view.data[0] = s[0]; view.data[1] = u[0]; view.data[2] = -f[0]; view.data[3] = 0.0f;
        view.data[4] = s[1]; view.data[5] = u[1]; view.data[6] = -f[1]; view.data[7] = 0.0f;
        view.data[8] = s[2]; view.data[9] = u[2]; view.data[10] = -f[2]; view.data[11] = 0.0f;
        view.data[12] = -(s[0]*eyeX + s[1]*eyeY + s[2]*eyeZ);
        view.data[13] = -(u[0]*eyeX + u[1]*eyeY + u[2]*eyeZ);
        view.data[14] = f[0]*eyeX + f[1]*eyeY + f[2]*eyeZ;
        view.data[15] = 1.0f;
        return view;
    }
};

#endif // CAMERA_HPP