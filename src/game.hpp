#ifndef GAME_HPP
#define GAME_HPP

#include "net.hpp"
#include "../shared/protocol.hpp"
#include "../shared/serialization.hpp"
#include "text_renderer.hpp"
#include "player_model.hpp"
#include "chat_system.hpp"
#include "inventory.hpp"
#include "particle_system.hpp"

#include <sstream>
#include <unordered_map>
#include <string>
#include <chrono>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

#include "game/state.hpp"
#include "game/remote_player.hpp"
#include "game/core.hpp"
#include "game/helpers.hpp"
#include "game/math.hpp"
#include "game/init.hpp"
#include "game/loop.hpp"
#include "game/input.hpp"
#include "game/menu.hpp"
#include "game/world.hpp"
#include "game/physics.hpp"
#include "game/raycast.hpp"
#include "game/render.hpp"
#include "game/network.hpp"
#include "game/chat.hpp"
#include "game/hotbar.hpp"
#include "game/crosshair.hpp"

#endif // GAME_HPP