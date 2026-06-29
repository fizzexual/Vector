#pragma once
#include <vector>

#include "core/scan_env.h"
#include "core/types.h"

namespace vector_core {

// Detect installed GOG games via the registry (GOG.com\Games\<id>).
std::vector<Game> scan_gog(const ScanEnv& env);

}  // namespace vector_core
