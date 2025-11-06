# Multiplayer Player Rendering System

## Overview

This Minecraft clone now features a complete multiplayer player rendering system where all connected players can see each other in real-time. The system uses WebSockets for client-server communication and renders Minecraft-style player models with custom skins.

## Architecture

### Protocol Changes (v2)

The network protocol has been updated from version 1 to version 2 to support player rotation data:

- **POSE Message** (Client → Server): Now includes `yaw` and `pitch` in addition to position
  ```json
  {"op":"pose","x":100.5,"y":65.0,"z":200.3,"yaw":-45.0,"pitch":10.0}
  ```

- **PLAYER_SNAPSHOT Message** (Server → Client): Broadcasts all connected players' positions and rotations
  ```json
  {
    "op":"player_snapshot",
    "players":[
      {"id":"client1","x":100.5,"y":65.0,"z":200.3,"yaw":-45.0,"pitch":10.0},
      {"id":"client2","x":105.2,"y":64.8,"z":198.7,"yaw":90.0,"pitch":-5.0}
    ]
  }
  ```

## Components

### 1. Player Model (`src/player_model.hpp`)

A complete Minecraft-style player model implementation featuring:

- **Accurate Proportions**: Matches Minecraft's player dimensions
  - Head: 8×8×8 pixels (0.5×0.5×0.5 blocks)
  - Body: 8×12×4 pixels (0.5×0.75×0.25 blocks)
  - Arms: 4×12×4 pixels each (0.25×0.75×0.25 blocks)
  - Legs: 4×12×4 pixels each (0.25×0.75×0.25 blocks)
  - Total height: 2.0 blocks (32 pixels)

- **UV Mapping**: Complete texture mapping for 64×64 Minecraft skin format
  - Head: Front, back, left, right, top, bottom faces
  - Body: All faces with correct UV coordinates
  - Left/Right Arms: Separate textures for each arm
  - Left/Right Legs: Separate textures for each leg

- **Rendering**: Efficient cuboid-based mesh generation with proper face culling

### 2. Remote Player Tracking (`src/game.hpp`)

The `RemotePlayer` structure tracks other players:
```cpp
struct RemotePlayer {
    std::string id;       // Unique player identifier
    float x, y, z;        // World position (y at feet)
    float yaw, pitch;     // Camera rotation
    std::chrono::steady_clock::time_point lastUpdate;  // For timeout detection
};
```

Players are stored in a map: `std::unordered_map<std::string, RemotePlayer> remotePlayers;`

### 3. Player Skin Texture

- **Location**: `/assets/skin.png`
- **Format**: Standard 64×64 pixel Minecraft skin
- **Compatibility**: Any Minecraft player skin can be used directly
- **Loading**: Automatically loaded during game initialization with nearest-neighbor filtering

### 4. Server-Side Player Management (`server/hub.hpp` & `server/hub.cpp`)

The server tracks each connected client's pose data:
```cpp
class ClientSession {
    float lastPoseX, lastPoseY, lastPoseZ;
    float lastYaw, lastPitch;
    std::chrono::steady_clock::time_point lastPoseUpdate;
};
```

#### Broadcasting Logic
- When any client sends a POSE update, the server broadcasts a PLAYER_SNAPSHOT to all clients
- Broadcast rate: ~10 Hz (controlled by client sending rate)
- Each snapshot contains all connected players' current positions and rotations

## Rendering Pipeline

### Client-Side Rendering Flow

1. **Update Phase** (in `mainLoop()`):
   - Client sends POSE updates every 100ms
   - Client receives PLAYER_SNAPSHOT messages from server
   - Remote player data is parsed and stored in `remotePlayers` map

2. **Render Phase** (in `render()`):
   ```cpp
   // After rendering chunks and before UI
   if (isOnlineMode) {
       renderRemotePlayers(view);
   }
   ```

3. **Player Rendering** (in `renderRemotePlayers()`):
   - Switch to player shader
   - Bind player skin texture
   - Enable face culling for proper rendering
   - For each remote player:
     - Create model matrix with position and yaw rotation
     - Compute MVP matrix
     - Draw player model
   - Restore main shader and block texture

### Shader System

**Player Vertex Shader**:
- Simple transformation: `gl_Position = uMVP * vec4(aPos, 1.0)`
- Passes texture coordinates to fragment shader

**Player Fragment Shader**:
- Samples skin texture
- Alpha testing (discard if alpha < 0.1)
- No lighting effects (future enhancement)

## Usage

### For Players

1. **Connect to Server**: Players automatically appear when they connect
2. **Movement**: Your position and view direction are sent to server at 10 Hz
3. **Visibility**: You can see all other connected players rendered as Minecraft-style characters
4. **Timeout**: Players who disconnect or become inactive are removed after 5 seconds

### For Developers

#### Adding Custom Player Skins

Replace `/assets/skin.png` with any 64×64 Minecraft skin file. The UV mapping follows the standard Minecraft format:

```
Skin Layout (64x64):
- Head:       (0,0) to (32,16)
- Body:       (16,16) to (40,32)
- Right Arm:  (40,16) to (56,32)
- Left Arm:   (32,48) to (48,64)
- Right Leg:  (0,16) to (16,32)
- Left Leg:   (16,48) to (32,64)
```

#### Extending the System

**Adding Player Animations** (Future Enhancement):
```cpp
// In RemotePlayer struct, add:
float walkCycle;
float armSwing;

// In renderRemotePlayers(), apply transformations before drawing
```

**Adding Player Names**:
```cpp
// Render player name above each player using TextRenderer
textRenderer.drawTextCentered(rp.id, screenPos.x, screenPos.y, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f);
```

**Per-Player Skins**:
- Server sends skin URL/identifier in PLAYER_SNAPSHOT
- Client downloads and caches player-specific textures
- Use texture array or atlas for multiple skins

## Performance Considerations

- **Model Geometry**: Each player model has ~216 vertices (6 cuboids × 6 faces × 6 vertices)
- **Texture Size**: 64×64 RGBA = 16 KB per skin
- **Network Traffic**: ~50 bytes per player per snapshot (10 Hz = 500 bytes/sec per player)
- **Rendering Cost**: Minimal - uses instanced rendering approach with single draw call per player

## Debugging

### Enable Debug Logging

Look for these console messages:
```
[GAME] New player connected: client2
[GAME] Player snapshot: 3 remote players
[GAME] Removing stale player: client1
```

### Common Issues

**Players Not Visible**:
- Check that `isOnlineMode` is true
- Verify PLAYER_SNAPSHOT messages are being received
- Ensure players are within render distance

**Players Appear Distorted**:
- Verify skin.png is exactly 64×64 pixels
- Check UV coordinates match Minecraft format

**Choppy Movement**:
- Increase POSE send rate (reduce POSE_SEND_INTERVAL)
- Add interpolation between updates

## Future Enhancements

1. **Smooth Interpolation**: Lerp between position/rotation updates
2. **Walking Animation**: Animate legs and arms based on movement
3. **Player Names**: Display username above each player
4. **Multiple Skins**: Support different skins per player
5. **Crouching/Jumping**: Different poses for player states
6. **Player Hitbox**: Enable player-to-player interaction
7. **LOD System**: Reduce detail for distant players
8. **First-Person Arms**: Show local player's arms holding tools

## Technical Notes

### Coordinate System
- Y-axis is vertical (up)
- Player position Y is at feet level
- Total player height is 2.0 blocks (matching Minecraft)

### Rotation
- Yaw: Horizontal rotation (-180° to 180°), 0° = facing north
- Pitch: Vertical rotation (-90° to 90°), positive = looking up
- Roll: Not implemented (players always upright)

### Matrix Transformations
```
ModelMatrix = Translation × RotationY(yaw)
MVP = Projection × View × Model
```

## Files Modified/Created

### Created:
- `src/player_model.hpp` - Complete player model implementation

### Modified:
- `shared/protocol.hpp` - Updated protocol version, added rotation fields
- `src/game.hpp` - Added RemotePlayer struct, player rendering system
- `server/hub.hpp` - Added rotation tracking fields
- `server/hub.cpp` - Parse rotation data, broadcast player snapshots

### Assets Required:
- `assets/skin.png` - 64×64 Minecraft player skin (already present)

## Conclusion

The multiplayer player rendering system is now fully functional, allowing players to see each other in real-time with proper Minecraft-style models and custom skins. The system is designed to be extensible for future features like animations, per-player skins, and player interaction.
