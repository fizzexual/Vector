#include "core/scan_all.h"

#include <set>

#include "core/scan_epic.h"
#include "core/scan_gog.h"
#include "core/scan_steam.h"

namespace vector_core {

std::vector<Game> scan_all(const ScanEnv& env, const ScanOptions& opts) {
  std::vector<Game> all;

  auto run = [&](std::vector<Game> (*fn)(const ScanEnv&)) {
    try {
      for (auto& g : fn(env)) all.push_back(std::move(g));
    } catch (...) {
      // One store failing must never block the others.
    }
  };

  if (opts.steam) run(scan_steam);
  if (opts.epic) run(scan_epic);
  if (opts.gog) run(scan_gog);

  // Dedupe by uid, preserving first occurrence.
  std::set<std::string> seen;
  std::vector<Game> out;
  out.reserve(all.size());
  for (auto& g : all) {
    if (seen.insert(g.uid).second) out.push_back(std::move(g));
  }
  return out;
}

}  // namespace vector_core
