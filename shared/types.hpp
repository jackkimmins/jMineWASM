// shared/types.hpp
#ifndef SHARED_TYPES_HPP
#define SHARED_TYPES_HPP

#include <cstddef>
#include <functional>

// Utility Matrix Structure
struct mat4 { 
    float data[16] = {0}; 
};

// Vector Types
struct Vector3 { 
    float x, y, z; 
};

struct Vector3i { 
    int x, y, z; 
};

// Block Types
enum BlockType {
    BLOCK_STONE,
    BLOCK_DIRT,
    BLOCK_PLANKS,
    BLOCK_GRASS,
    BLOCK_BEDROCK,
    BLOCK_COAL_ORE,
    BLOCK_IRON_ORE,
    BLOCK_LOG,
    BLOCK_LEAVES,
    BLOCK_WATER,
    BLOCK_SAND,
    BLOCK_TALL_GRASS,
    BLOCK_ORANGE_FLOWER,
    BLOCK_BLUE_FLOWER,
    BLOCK_COBBLESTONE,
    BLOCK_GLASS,
    BLOCK_CLAY,
    BLOCK_SNOW
};

// Face Direction for rendering
enum FaceDirection {
    FACE_FRONT = 0,
    FACE_BACK = 1,
    FACE_RIGHT = 2,
    FACE_LEFT = 3,
    FACE_TOP = 4,
    FACE_BOTTOM = 5
};

// Block Structure
struct Block {
    bool isSolid = false;
    BlockType type = BLOCK_STONE;
};

// Chunk Coordinate
struct ChunkCoord {
    int x, y, z;
    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

// Hash specialization for ChunkCoord
namespace std {
    template <>
    struct hash<ChunkCoord> {
        size_t operator()(const ChunkCoord& c) const {
            return ((hash<int>()(c.x) ^ (hash<int>()(c.y) << 1)) >> 1) ^ (hash<int>()(c.z) << 1);
        }
    };
}

#endif // SHARED_TYPES_HPP
