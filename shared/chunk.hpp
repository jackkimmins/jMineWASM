// shared/chunk.hpp
#ifndef SHARED_CHUNK_HPP
#define SHARED_CHUNK_HPP

#include "types.hpp"
#include "config.hpp"

class Chunk {
public:
    Block blocks[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE]; // [x][y][z]
    bool isGenerated = false;
    bool isDirty = false;
    bool isFullyProcessed = false;
    ChunkCoord coord;
    
    Chunk() = default;
    Chunk(int cx, int cy, int cz) : coord{cx, cy, cz} {}
};

#endif // SHARED_CHUNK_HPP
