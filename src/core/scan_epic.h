#pragma once
#include <vector>

#include "core/scan_env.h"
#include "core/types.h"

namespace vector_core {

// Detect installed Epic Games via the launcher's *.item JSON manifests.
std::vector<Game> scan_epic(const ScanEnv& env);

}  // namespace vector_core
