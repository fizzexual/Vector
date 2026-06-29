#pragma once
#include "core/types.h"

namespace vector_core {

// Launch a game via its LaunchSpec (steam://, epic uri, or an exe path).
bool launch_game(const Game& g);

// Open the game's install folder in Explorer.
bool show_in_folder(const Game& g);

}  // namespace vector_core
