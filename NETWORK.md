# Network Mode Documentation

## Overview

The game now supports two modes of operation:
- **Offline Mode** (default): Full local world generation
- **Online Mode** (network-ready): Connects to WebSocket server for multiplayer/networked gameplay

## Running Offline (Default)

Simply build and run as before:

```bash
make client
# Open build/index.html in browser
```

The game will detect no `GAME_WS_URL` environment variable and run in offline mode with full local generation.

## Running in Network-Ready Mode

Set the `GAME_WS_URL` environment variable before the game starts:

### Option 1: Set in JavaScript (before loading WASM)
```javascript
// In shell_minimal.html or before Module initialization
Module.preRun = [function() {
    ENV.GAME_WS_URL = "ws://localhost:9001";
}];
```

### Option 2: Runtime environment injection
```bash
# Start a test WebSocket server first
# Then load with URL parameter or modify the HTML to set ENV variable
```

### Expected Behavior in Online Mode:
1. Game connects to WebSocket server on startup
2. Sends `{"op":"hello","proto":1}` message
3. As player moves, sends `{"op":"set_interest","center":[cx,cz],"radius":8}` 
4. Waits for server responses to populate chunks
5. **No local generation occurs** (relies entirely on server)

## Message Protocol

### Client → Server Messages

#### 1. Hello
```json
{
  "op": "hello",
  "proto": 1
}
```
Sent on initial connection to identify protocol version.

#### 2. Set Interest
```json
{
  "op": "set_interest",
  "center": [10, 15],
  "radius": 8
}
```
Sent when player changes chunks. Tells server which chunks to stream.

#### 3. Pose (Future)
```json
{
  "op": "pose",
  "x": 512.5,
  "y": 75.2,
  "z": 512.5,
  "yaw": -90.0,
  "pitch": 0.0
}
```
Player position/orientation update.

#### 4. Edit (Future)
```json
{
  "op": "edit",
  "x": 100,
  "y": 50,
  "z": 100,
  "type": 1,
  "solid": true
}
```
Block placement/destruction.

### Server → Client Messages

#### 1. Hello OK
```json
{
  "op": "hello_ok",
  "server_version": "1.0.0",
  "player_id": "p123"
}
```
Server acknowledges connection.

#### 2. Chunk Full
```json
{
  "op": "chunk_full",
  "cx": 10,
  "cy": 0,
  "cz": 15,
  "types": [0,1,1,2,3,...],
  "solids": [true,true,true,false,...]
}
```
Complete chunk data. Arrays have CHUNK_SIZE × CHUNK_HEIGHT × CHUNK_SIZE = 4096 elements.

#### 3. Chunk Unload
```json
{
  "op": "chunk_unload",
  "cx": 5,
  "cy": 0,
  "cz": 5
}
```
Tells client to remove chunk from memory.

#### 4. Block Update
```json
{
  "op": "block_update",
  "x": 100,
  "y": 50,
  "z": 100,
  "type": 3,
  "solid": true
}
```
Single block change (efficient for edits).

#### 5. Player Snapshot (Future Multiplayer)
```json
{
  "op": "player_snapshot",
  "players": [
    {"id": "p1", "x": 100, "y": 50, "z": 100, "yaw": 45, "pitch": 0},
    {"id": "p2", "x": 200, "y": 55, "z": 150, "yaw": -90, "pitch": 10}
  ]
}
```
Other players' positions for rendering.

## Console Logging

All network activity is logged with `[NET]` and `[GAME]` prefixes:

```
[NET] Connecting to: ws://localhost:9001
[NET] ✓ Connected
[NET] → {"op":"hello","proto":1}
[NET] ← {"op":"hello_ok","server_version":"1.0.0"}
[GAME] ✓ Server accepted hello
[NET] → {"op":"set_interest","center":[32,32],"radius":8}
[NET] ← {"op":"chunk_full","cx":32,"cy":0,"cz":32,...}
[GAME] → Received chunk_full (stub: would call setChunkData + generateMesh)
```

## Testing with Echo Server

For quick testing, you can use a WebSocket echo server:

```bash
# Install wscat (WebSocket client/server)
npm install -g wscat

# Run echo server
wscat --listen 9001
```

The client will connect and send messages. The echo server will reflect them back, demonstrating the message flow (though the game won't actually process the echoed chunks).

## Current Implementation Status

✅ **Implemented:**
- WebSocket connection management
- Message serialization/deserialization skeleton
- Offline/online mode switching
- Hello and set_interest messages
- Message handler stubs with logging

⏳ **Stub/Future:**
- Full JSON parsing for chunk_full/block_update
- Actual chunk data deserialization
- Player pose updates
- Block edit networking
- Multiplayer player rendering

## Architecture Notes

The networking layer is cleanly separated:
- `shared/protocol.hpp` - Protocol constants (shared with future server)
- `client/net.hpp` - WebSocket wrapper (browser-only)
- `src/game.hpp` - Integration with game loop
- World generation remains in `src/world_generation.hpp` (offline fallback)

No Boost required; uses native Emscripten WebSocket API.
