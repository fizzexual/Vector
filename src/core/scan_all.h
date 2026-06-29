#pragma once
#include <vector>

#include "core/scan_env.h"
#include "core/types.h"

namespace vector_core {

struct ScanOptions {
  bool steam = true;
  bool epic = true;
  bool gog = true;
};

// Run all enabled scanners, isolate per-store failures, and dedupe by uid.
std::vector<Game> scan_all(const ScanEnv& env, const ScanOptions& opts = {});

}  // namespace vector_core
