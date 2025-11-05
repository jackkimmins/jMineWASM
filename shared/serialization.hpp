// shared/serialization.hpp
// Chunk serialization with RLE compression
#ifndef SHARED_SERIALIZATION_HPP
#define SHARED_SERIALIZATION_HPP

#include <vector>
#include <cstdint>
#include <string>
#include "types.hpp"
#include "config.hpp"

namespace Serialization {

// Base64 encoding
inline std::string base64_encode(const std::vector<uint8_t>& data) {
    static const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);
    
    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t triple = (data[i] << 16);
        if (i + 1 < data.size()) triple |= (data[i + 1] << 8);
        if (i + 2 < data.size()) triple |= data[i + 2];
        
        result += b64chars[(triple >> 18) & 0x3F];
        result += b64chars[(triple >> 12) & 0x3F];
        result += (i + 1 < data.size()) ? b64chars[(triple >> 6) & 0x3F] : '=';
        result += (i + 2 < data.size()) ? b64chars[triple & 0x3F] : '=';
    }
    
    return result;
}

// Base64 decoding
inline std::vector<uint8_t> base64_decode(const std::string& encoded) {
    static const uint8_t b64index[256] = {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 62,  0,  0,  0, 63,
       52, 53, 54, 55, 56, 57, 58, 59, 60, 61,  0,  0,  0,  0,  0,  0,
        0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
       15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,  0,  0,  0,  0,
        0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
       41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,  0,  0,  0,  0,  0
    };
    
    std::vector<uint8_t> result;
    result.reserve((encoded.size() / 4) * 3);
    
    for (size_t i = 0; i < encoded.size(); i += 4) {
        uint32_t triple = (b64index[(uint8_t)encoded[i]] << 18);
        if (i + 1 < encoded.size()) triple |= (b64index[(uint8_t)encoded[i + 1]] << 12);
        if (i + 2 < encoded.size() && encoded[i + 2] != '=') triple |= (b64index[(uint8_t)encoded[i + 2]] << 6);
        if (i + 3 < encoded.size() && encoded[i + 3] != '=') triple |= b64index[(uint8_t)encoded[i + 3]];
        
        result.push_back((triple >> 16) & 0xFF);
        if (i + 2 < encoded.size() && encoded[i + 2] != '=') result.push_back((triple >> 8) & 0xFF);
        if (i + 3 < encoded.size() && encoded[i + 3] != '=') result.push_back(triple & 0xFF);
    }
    
    return result;
}

// RLE encode chunk data: (uint16 count, uint8 type, uint8 solid)
inline std::vector<uint8_t> encodeChunk(const uint8_t types[], const uint8_t solids[]) {
    const int totalBlocks = CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE;
    std::vector<uint8_t> result;
    result.reserve(totalBlocks / 4); // Estimate
    
    int i = 0;
    while (i < totalBlocks) {
        uint8_t currentType = types[i];
        uint8_t currentSolid = solids[i];
        uint16_t count = 1;
        
        // Count consecutive identical blocks (max 65535)
        while (i + count < totalBlocks && count < 65535 &&
               types[i + count] == currentType &&
               solids[i + count] == currentSolid) {
            ++count;
        }
        
        // Write: count (2 bytes), type (1 byte), solid (1 byte)
        result.push_back((count >> 8) & 0xFF);
        result.push_back(count & 0xFF);
        result.push_back(currentType);
        result.push_back(currentSolid);
        
        i += count;
    }
    
    return result;
}

// RLE decode chunk data
inline void decodeChunk(const std::vector<uint8_t>& encoded, uint8_t types[], uint8_t solids[]) {
    const int totalBlocks = CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE;
    int outIdx = 0;
    
    for (size_t i = 0; i + 3 < encoded.size() && outIdx < totalBlocks; i += 4) {
        uint16_t count = (encoded[i] << 8) | encoded[i + 1];
        uint8_t type = encoded[i + 2];
        uint8_t solid = encoded[i + 3];
        
        for (uint16_t j = 0; j < count && outIdx < totalBlocks; ++j) {
            types[outIdx] = type;
            solids[outIdx] = solid;
            ++outIdx;
        }
    }
}

} // namespace Serialization

#endif // SHARED_SERIALIZATION_HPP
