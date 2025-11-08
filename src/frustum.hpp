// frustum.hpp
#ifndef FRUSTUM_HPP
#define FRUSTUM_HPP

#include "../shared/types.hpp"
#include <cmath>

// Plane equation: ax + by + cz + d = 0
struct Plane {
    float a, b, c, d;
    
    // Normalize the plane equation
    void normalize() {
        float mag = sqrtf(a * a + b * b + c * c);
        a /= mag;
        b /= mag;
        c /= mag;
        d /= mag;
    }
    
    // Calculate signed distance from point to plane
    // Positive = in front of plane, Negative = behind plane
    float distanceToPoint(float x, float y, float z) const {
        return a * x + b * y + c * z + d;
    }
};

// Axis-Aligned Bounding Box
struct AABB {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
    
    AABB() : minX(0), minY(0), minZ(0), maxX(0), maxY(0), maxZ(0) {}
    
    AABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ)
        : minX(minX), minY(minY), minZ(minZ), maxX(maxX), maxY(maxY), maxZ(maxZ) {}
    
    // Get the center point of the AABB
    void getCenter(float& cx, float& cy, float& cz) const {
        cx = (minX + maxX) * 0.5f;
        cy = (minY + maxY) * 0.5f;
        cz = (minZ + maxZ) * 0.5f;
    }
    
    // Get the half-extents (radius) of the AABB
    void getExtents(float& ex, float& ey, float& ez) const {
        ex = (maxX - minX) * 0.5f;
        ey = (maxY - minY) * 0.5f;
        ez = (maxZ - minZ) * 0.5f;
    }
};

class Frustum {
public:
    // Frustum planes: Left, Right, Bottom, Top, Near, Far
    enum {
        PLANE_LEFT = 0,
        PLANE_RIGHT,
        PLANE_BOTTOM,
        PLANE_TOP,
        PLANE_NEAR,
        PLANE_FAR,
        PLANE_COUNT = 6
    };
    
    Plane planes[PLANE_COUNT];
    
    // Extract frustum planes from the combined view-projection matrix
    // Uses the Gribb-Hartmann method
    void extractFromMatrix(const mat4& mvp) {
        // Left plane
        planes[PLANE_LEFT].a = mvp.data[3]  + mvp.data[0];
        planes[PLANE_LEFT].b = mvp.data[7]  + mvp.data[4];
        planes[PLANE_LEFT].c = mvp.data[11] + mvp.data[8];
        planes[PLANE_LEFT].d = mvp.data[15] + mvp.data[12];
        
        // Right plane
        planes[PLANE_RIGHT].a = mvp.data[3]  - mvp.data[0];
        planes[PLANE_RIGHT].b = mvp.data[7]  - mvp.data[4];
        planes[PLANE_RIGHT].c = mvp.data[11] - mvp.data[8];
        planes[PLANE_RIGHT].d = mvp.data[15] - mvp.data[12];
        
        // Bottom plane
        planes[PLANE_BOTTOM].a = mvp.data[3]  + mvp.data[1];
        planes[PLANE_BOTTOM].b = mvp.data[7]  + mvp.data[5];
        planes[PLANE_BOTTOM].c = mvp.data[11] + mvp.data[9];
        planes[PLANE_BOTTOM].d = mvp.data[15] + mvp.data[13];
        
        // Top plane
        planes[PLANE_TOP].a = mvp.data[3]  - mvp.data[1];
        planes[PLANE_TOP].b = mvp.data[7]  - mvp.data[5];
        planes[PLANE_TOP].c = mvp.data[11] - mvp.data[9];
        planes[PLANE_TOP].d = mvp.data[15] - mvp.data[13];
        
        // Near plane
        planes[PLANE_NEAR].a = mvp.data[3]  + mvp.data[2];
        planes[PLANE_NEAR].b = mvp.data[7]  + mvp.data[6];
        planes[PLANE_NEAR].c = mvp.data[11] + mvp.data[10];
        planes[PLANE_NEAR].d = mvp.data[15] + mvp.data[14];
        
        // Far plane
        planes[PLANE_FAR].a = mvp.data[3]  - mvp.data[2];
        planes[PLANE_FAR].b = mvp.data[7]  - mvp.data[6];
        planes[PLANE_FAR].c = mvp.data[11] - mvp.data[10];
        planes[PLANE_FAR].d = mvp.data[15] - mvp.data[14];
        
        // Normalize all planes
        for (int i = 0; i < PLANE_COUNT; ++i) {
            planes[i].normalize();
        }
    }
    
    // Test if a point is inside the frustum
    bool containsPoint(float x, float y, float z) const {
        for (int i = 0; i < PLANE_COUNT; ++i) {
            if (planes[i].distanceToPoint(x, y, z) < 0) {
                return false;
            }
        }
        return true;
    }
    
    // Test if a sphere is inside or intersecting the frustum
    // Returns: true if sphere is at least partially visible
    bool intersectsSphere(float cx, float cy, float cz, float radius) const {
        for (int i = 0; i < PLANE_COUNT; ++i) {
            float distance = planes[i].distanceToPoint(cx, cy, cz);
            if (distance < -radius) {
                return false; // Sphere is completely outside this plane
            }
        }
        return true;
    }
    
    // Test if an AABB is inside or intersecting the frustum
    // Returns: true if AABB is at least partially visible
    // This uses the "positive vertex" test - very efficient for AABBs
    bool intersectsAABB(const AABB& box) const {
        for (int i = 0; i < PLANE_COUNT; ++i) {
            const Plane& plane = planes[i];
            
            // Find the "positive vertex" - the vertex of the box
            // that is furthest along the plane normal direction
            float px = (plane.a >= 0) ? box.maxX : box.minX;
            float py = (plane.b >= 0) ? box.maxY : box.minY;
            float pz = (plane.c >= 0) ? box.maxZ : box.minZ;
            
            // If the positive vertex is outside (behind) this plane,
            // the entire box is outside the frustum
            if (plane.distanceToPoint(px, py, pz) < 0) {
                return false;
            }
        }
        return true;
    }
    
    // Alternative AABB test using center and extents (slightly faster for some cases)
    bool intersectsAABB(float cx, float cy, float cz, float ex, float ey, float ez) const {
        for (int i = 0; i < PLANE_COUNT; ++i) {
            const Plane& plane = planes[i];
            
            // Calculate the "effective radius" of the box along the plane normal
            float effectiveRadius = ex * fabsf(plane.a) + 
                                   ey * fabsf(plane.b) + 
                                   ez * fabsf(plane.c);
            
            // Calculate distance from box center to plane
            float distance = plane.distanceToPoint(cx, cy, cz);
            
            // If the box is completely outside this plane, it's not visible
            if (distance < -effectiveRadius) {
                return false;
            }
        }
        return true;
    }
    
    // Convenience method: Test if a chunk is visible
    bool isChunkVisible(int chunkX, int chunkY, int chunkZ, 
                       int chunkSizeX, int chunkSizeY, int chunkSizeZ) const {
        // Calculate chunk bounds in world space
        float minX = chunkX * chunkSizeX;
        float minY = chunkY * chunkSizeY;
        float minZ = chunkZ * chunkSizeZ;
        float maxX = minX + chunkSizeX;
        float maxY = minY + chunkSizeY;
        float maxZ = minZ + chunkSizeZ;
        
        AABB chunkBox(minX, minY, minZ, maxX, maxY, maxZ);
        return intersectsAABB(chunkBox);
    }
};

#endif // FRUSTUM_HPP
