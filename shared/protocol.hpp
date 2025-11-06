// shared/protocol.hpp
// Network protocol message opcodes and constants
#ifndef SHARED_PROTOCOL_HPP
#define SHARED_PROTOCOL_HPP

#include <string>

// Protocol version - increment when changing protocol
constexpr int PROTOCOL_VERSION = 2;

// Client → Server opcodes
namespace ClientOp {
    constexpr const char* HELLO = "hello";
    constexpr const char* SET_INTEREST = "set_interest";
    constexpr const char* POSE = "pose";  // Now includes: x, y, z, yaw, pitch
    constexpr const char* EDIT = "edit";
}

// Server → Client opcodes
namespace ServerOp {
    constexpr const char* HELLO_OK = "hello_ok";
    constexpr const char* CHUNK_FULL = "chunk_full";
    constexpr const char* CHUNK_UNLOAD = "chunk_unload";
    constexpr const char* BLOCK_UPDATE = "block_update";
    constexpr const char* PLAYER_SNAPSHOT = "player_snapshot";  // Broadcasts all player positions and rotations
}

#endif // SHARED_PROTOCOL_HPP
