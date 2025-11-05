# jMineWASM
C++ Minecraft-like voxel game compiled to WebAssembly using Emscripten.

- World Size is `1024 x 1024` blocks
- **Now with multiplayer!** Authoritative server with real-time block synchronization

## Play Game
The game is playable via any modern web browser:
[https://jmine.appserver.uk/](https://jmine.appserver.uk/)

## Multiplayer Quickstart

Run your own server and play with friends:

```bash
# Build everything
make all

# Run server (builds + launches on port 8888)
make run

# Open browser to http://localhost:8888
# Open multiple tabs/browsers to test multiplayer
```

**Documentation:**
- **[Multiplayer Guide](README_MULTIPLAYER.md)** - Architecture, network protocol, performance
- **[Testing Guide](TESTING.md)** - Comprehensive test procedures and benchmarks
- **[Block Editing](BLOCK_EDITING.md)** - Technical details on authoritative edits

## To-Do List
- [x] Procedural World Generation
- [x] Block Texture Atlas
- [x] Random Ore Veins
- [x] Cave Generation
- [x] Trees
- [ ] Lighting Engine (Day/Night Cycle + Block Light Emission)
- [x] Water Lakes/Rivers/Oceans
- [x] Distance Fog
- [x] Tall Grass & Flowers
- [ ] Flowable Liquids
- [ ] Mobs (Animals + Hostile)
- [ ] Main Menu
- [ ] Multithreading for World and Mesh Gen
- [ ] View-frustum Culling
- [ ] Save/Load World
- [ ] Sound Effects
- [ ] Mobile/Touch Controls
- [ ] Desktop Version w/ OpenGL
- [x] **Multiplayer** (Authoritative server with WebSockets)


## Versions

### V1.2 - Realism Update
Water, trees, fog, flying, and much better world generation.

Playable via: [https://35beb8db.jmine.pages.dev/](https://35beb8db.jmine.pages.dev/)
![v1.2](https://github.com/jackkimmins/jMineWASM/blob/main/screenshots/v1.2.jpg?raw=true)

### V1.1 - The Expansion
Much larger world size of 1024x1024 blocks.

Playable via: [https://09dd440a.jmine.pages.dev/](https://09dd440a.jmine.pages.dev/)
![v1.1](https://github.com/jackkimmins/jMineWASM/blob/main/screenshots/v1.1.jpg?raw=true)

### V1.0 - The Generation
Procedural world generation, chunking, block types, and basic mining/building.

Playable via: [https://3a6e6c0b.jmine.pages.dev/](https://3a6e6c0b.jmine.pages.dev/)
![v1.0](https://github.com/jackkimmins/jMineWASM/blob/main/screenshots/v1.0.jpg?raw=true)

### V0.0 - Beta/Concept
Available via old repo: [jackkimmins/3D-Cube-Game: Browser-based 3D block Game - C++ and WebAssembly w/ SDL2](https://github.com/jackkimmins/3D-Cube-Game)

Playable via: https://3d-cube-game.appserver.uk/
![v0.0](https://raw.githubusercontent.com/jackkimmins/3D-Cube-Game/refs/heads/main/screenshots/GameScreenshot.jpg)