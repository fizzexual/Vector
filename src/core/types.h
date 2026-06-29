// Core domain types for Vector. CEF-independent and unit-testable.
#pragma once
#include <cstdint>
#include <string>

namespace vector_core {

enum class Source { Steam, Epic, Gog, Manual };

inline const char* source_id(Source s) {
  switch (s) {
    case Source::Steam: return "steam";
    case Source::Epic:  return "epic";
    case Source::Gog:   return "gog";
    case Source::Manual:return "manual";
  }
  return "manual";
}

struct Artwork {
  std::string cover;
  std::string hero;
  std::string logo;
};

struct LaunchSpec {
  std::string kind;   // "uri" | "exe"
  std::string value;
};

struct Game {
  std::string uid;    // "${source}:${id}"
  std::string id;
  std::string name;
  Source source = Source::Manual;
  bool installed = false;
  std::string installDir;
  std::string executable;
  uint64_t sizeBytes = 0;
  Artwork art;
  LaunchSpec launch;
  uint64_t lastPlayed = 0;   // epoch seconds, best-effort
  bool favorite = false;
  bool hidden = false;
  uint64_t addedAt = 0;
};

}  // namespace vector_core
