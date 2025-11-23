#ifndef GAME_STATE_HPP
#define GAME_STATE_HPP

enum class GameState {
    MAIN_MENU,         // Main menu/title screen
    USERNAME_INPUT,    // Username input screen (before connecting)
    LOADING,           // Initial loading
    CONNECTING,        // Connecting to server (online mode)
    WAITING_FOR_WORLD, // Waiting for world data from server
    PLAYING,           // Normal gameplay
    DISCONNECTED       // Disconnected from server
};

#endif // GAME_STATE_HPP