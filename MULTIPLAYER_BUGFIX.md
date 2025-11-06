# Multiplayer Player Rendering - Bug Fixes

## Issues Fixed

### 1. Players Seeing Themselves
**Problem:** All clients were receiving their own player data in the player_snapshot, causing them to see themselves rendered a distance away.

**Solution:**
- Modified `Hub::broadcastPlayerSnapshot()` to send personalized messages to each client
- Each client now receives a list that excludes their own player data
- Added client ID to the `hello_ok` response so clients know their own ID
- Added safety check in client to filter out their own ID

### 2. Position Alignment
**Problem:** The player model would appear to "follow" the viewing player because it was being rendered at world coordinates but the viewing camera was also at those coordinates.

**Solution:**
- Server now correctly excludes the receiving client from the player list
- Only remote players are rendered, not the local player
- Player models are positioned at their actual world coordinates (x, y, z)

## Changes Made

### Server Side (`server/hub.cpp`)
1. **Updated `handleHello()`:**
   - Now includes `client_id` in the hello_ok response
   - Example: `{"op":"hello_ok","client_id":"client1",...}`

2. **Updated `broadcastPlayerSnapshot()`:**
   - Changed from broadcasting one message to all clients
   - Now creates personalized messages for each client
   - Excludes the receiving client from their own player list
   - Each client only sees OTHER players, not themselves

3. **Updated pose handling:**
   - Now correctly stores yaw and pitch rotation data
   - Broadcasts player snapshots when pose updates are received

### Client Side (`src/game.hpp`)
1. **Added client ID tracking:**
   - Added `myClientId` field to Game class
   - Stored when receiving hello_ok from server

2. **Updated `handleHelloOk()`:**
   - Parses and stores the client_id from server
   - Logs the assigned ID for debugging

3. **Updated `handlePlayerSnapshot()`:**
   - Added safety check to skip own player ID
   - Only creates RemotePlayer entries for other players

## Protocol Version
Updated protocol version from 1 to 2 to reflect the changes in message format.

## Testing
After these changes:
- Players should only see OTHER players in the world
- Player positions should correctly match their actual locations
- No "ghost" self-player should appear
- Multiple players can see each other at the correct positions

## Example Flow
1. Client1 connects → Server assigns "client1" ID
2. Client2 connects → Server assigns "client2" ID
3. Client1 moves → Server broadcasts to Client2 (excluding Client1's own data)
4. Client2 sees Client1's player model at correct position
5. Client2 does NOT see their own player model
