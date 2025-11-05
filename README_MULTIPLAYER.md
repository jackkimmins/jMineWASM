# jMineWASM - Multiplayer Voxel Game

A Minecraft-inspired voxel game built with C++20, WebAssembly, and OpenGL ES 3.0, featuring authoritative multiplayer with real-time block synchronization.

## Features

- **Procedural World Generation**: Perlin noise terrain with caves, ores, trees, and foliage
- **Multiplayer Support**: Authoritative server with area-of-interest (AOI) based streaming
- **Block Editing**: Real-time synchronized block placement/removal across all clients
- **Optimistic Updates**: Immediate local feedback with server reconciliation
- **Efficient Networking**: RLE compression + base64 encoding, cached serialization
- **Memory Bounded**: Automatic chunk unloading outside view distance
- **Protocol Versioning**: Client/server version mismatch detection

## Quick Start

### Build and Run

```bash
# Build everything (client + server)
make all

# Prepare deployment files
make www

# Run server
./bin/jMineServer --root ./www --port 8888

# Or use shortcut
make run
```

### Open Two Clients

1. **Browser 1**: Navigate to `http://localhost:8888`
   - Game loads with WebAssembly
   - Connects to WebSocket at `ws://localhost:8888/ws`
   
2. **Browser 2**: Open same URL in another tab/window/browser
   - Both clients connect to same server
   - Both clients see the same world
   - Block edits from either client visible to both

### Testing Multiplayer

1. **Move around**: Chunks load/unload smoothly as you move
2. **Place blocks** (right-click): Other player sees blocks appear
3. **Break blocks** (left-click): Other player sees blocks disappear
4. **Check console**: See network messages for chunk loads, edits, updates

## Build Requirements

### Client (WebAssembly)
- Emscripten SDK (em++)
- OpenGL ES 3.0 support in browser

### Server (Native Linux)
- g++ with C++20 support
- Boost libraries:
  - boost_system
  - boost_filesystem
  
Install on Ubuntu/Debian:
```bash
sudo apt install build-essential libboost-all-dev
```

## Architecture

### Network Protocol

**Client → Server**:
- `hello`: Handshake with protocol version
- `set_interest`: Update AOI (center chunk + radius)
- `pose`: Player position updates (10 Hz)
- `edit`: Block placement/removal requests

**Server → Client**:
- `hello_ok`: Handshake response with world config
- `chunk_full`: RLE-compressed chunk data
- `chunk_unload`: Notify client to free chunk
- `block_update`: Single block change broadcast

### Area of Interest (AOI) System

- Server tracks which chunks each client has loaded
- On `set_interest`, server computes delta:
  - **Enters**: New chunks (send `chunk_full`)
  - **Exits**: Old chunks (send `chunk_unload`)
- Only sends diffs, not full AOI each time
- Chunks cached per-revision to avoid recomputation

### Memory Management

**Server**:
- Generates chunks on-demand
- Caches serialized RLE data per (cx,cy,cz,rev)
- Invalidates cache entry when chunk edited (rev++)

**Client**:
- Loads chunks from server
- Frees chunks on `chunk_unload` (CPU + GPU mesh)
- Tracks chunk revisions to reject stale updates

### Block Editing Flow

1. **Client optimistic update**: Apply immediately for responsiveness
2. **Client sends edit intent**: `{"op":"edit","kind":"place|remove",...}`
3. **Server validates**: Bounds, bedrock, distance (≤6 blocks), rate limit (20/sec)
4. **Server applies**: Updates authoritative world, increments chunk revision
5. **Server broadcasts**: `block_update` to all clients in AOI
6. **All clients reconcile**: Apply update if revision is newer

## Performance

### Chunk Sizes
- **Uncompressed**: 4096 blocks × 2 bytes = 8 KB per chunk
- **RLE compressed**: Typically 400-1200 bytes (80-85% reduction)
- **Base64 overhead**: ~33% (final size 600-1600 bytes per chunk)
- **JSON wrapper**: ~100 bytes

### Network Traffic
- **Initial load**: ~100-200 chunks = 100-200 KB (render distance 8)
- **Movement**: 10-30 chunks/sec when moving fast
- **Block edits**: ~150 bytes per edit
- **Pose updates**: ~50 bytes @ 10 Hz = 500 bytes/sec

### Rate Limits
- **Block edits**: 60 burst, 20/sec sustained per client
- **Pose updates**: 10 Hz (100ms interval)
- **AOI updates**: As needed when player chunk changes

## Configuration

### Environment Variables

**Client** (browser):
- `GAME_WS_URL`: WebSocket endpoint (default: auto-detect from page origin)
  - Example: `ws://localhost:8888/ws`
  - Example: `wss://myserver.com/ws`

**Server** (command line):
- `--root <path>`: Static file directory (default: `./www`)
- `--port <port>`: HTTP/WebSocket port (default: `8888`)

### Protocol Version

Defined in `shared/protocol.hpp`:
```cpp
constexpr int PROTOCOL_VERSION = 1;
```

Clients with mismatched versions are rejected on handshake.

## Development

### Project Structure

```
jMineWASM/
├── src/              # Client-side game code
│   ├── main.cpp      # Entry point
│   ├── game.hpp      # Game loop, rendering, input
│   ├── camera.hpp    # First-person camera
│   ├── mesh.hpp      # Chunk mesh generation
│   ├── shaders.hpp   # GLSL shaders
│   ├── perlin_noise.hpp  # Noise generation
│   └── world_generation.hpp  # Terrain generation
├── shared/           # Platform-agnostic code
│   ├── config.hpp    # World constants
│   ├── types.hpp     # BlockType, Vector3, etc.
│   ├── chunk.hpp     # Chunk data structure
│   ├── protocol.hpp  # Network opcodes
│   └── serialization.hpp  # RLE + base64
├── server/           # Server-side code
│   ├── main.cpp      # HTTP + WebSocket listener
│   ├── hub.hpp       # Client management
│   └── hub.cpp       # Protocol handlers
├── client/           # Client-only networking
│   └── net.hpp       # Emscripten WebSocket wrapper
├── assets/           # Textures
├── build/            # Client WASM output
├── www/              # Deployment files
└── bin/              # Server binary
```

### Build Targets

```bash
make all       # Build client + server
make client    # Build WebAssembly client only
make server    # Build native server only
make www       # Copy client files to www/
make package   # Build + prepare everything
make clean     # Remove all build artifacts
make run       # Build + run server
```

### Adding Features

1. **New block type**: Add to `BlockType` enum in `shared/types.hpp`
2. **New message**: Add opcode to `shared/protocol.hpp`, implement handlers
3. **Client logic**: Update `src/game.hpp`
4. **Server logic**: Update `server/hub.cpp`

## Known Limitations

- **No persistence**: World changes lost on server restart
- **Single-threaded**: Server runs on one thread (sufficient for ~10 clients)
- **No authentication**: Anyone can connect
- **No chunk ownership**: No protection against griefing
- **JSON protocol**: Text-based (could be optimized to binary)

## Future Improvements

- [ ] Persistent world storage (SQLite/file-based)
- [ ] Player authentication and permissions
- [ ] Chunk ownership and protection
- [ ] Binary protocol (MessagePack/Protobuf)
- [ ] Compression (permessage-deflate)
- [ ] Multi-threaded server
- [ ] Client reconnection with exponential backoff
- [ ] Player avatars and name tags
- [ ] Inventory and crafting system
- [ ] Day/night cycle
- [ ] Mob spawning

## Troubleshooting

### "Protocol mismatch" error
- Client and server have different `PROTOCOL_VERSION`
- Rebuild both client and server: `make clean && make all`

### Chunks not loading
- Check browser console for WebSocket errors
- Verify server is running: `ps aux | grep jMineServer`
- Check server logs for connection messages

### High memory usage
- Memory grows with render distance
- Default render distance: 8 chunks (radius)
- ~100-200 MB typical for full AOI

### Lag or stuttering
- Check network console for slow chunk loads
- Reduce render distance in `shared/config.hpp`
- Check server CPU usage

## License

[Add your license here]

## Credits

- Terrain generation inspired by Minecraft
- Perlin noise implementation based on Ken Perlin's algorithm
- Built with Emscripten, Boost.Beast, OpenGL ES 3.0
