#pragma once
#include <vector>

#include "core/scan_env.h"
#include "core/types.h"

namespace vector_core {

// Detect installed Steam games via the registry + libraryfolders.vdf +
// appmanifest_*.acf files.
std::vector<Game> scan_steam(const ScanEnv& env);

}  // namespace vector_core
