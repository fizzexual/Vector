#pragma once
#include "core/types.h"
#include "third_party/json.hpp"

namespace vector_core {

nlohmann::json to_json(const Game& g);
Game game_from_json(const nlohmann::json& j);

}  // namespace vector_core
