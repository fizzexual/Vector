// Small string helpers shared by the scanners.
#pragma once
#include <cstdint>
#include <string>

namespace vector_core {

inline std::string to_forward_slashes(std::string s) {
  for (auto& c : s) {
    if (c == '\\') c = '/';
  }
  return s;
}

inline uint64_t to_u64(const std::string& s) {
  if (s.empty()) return 0;
  try {
    return std::stoull(s);
  } catch (...) {
    return 0;
  }
}

inline std::string strip_trailing_slash(std::string s) {
  while (!s.empty() && (s.back() == '/' || s.back() == '\\')) s.pop_back();
  return s;
}

}  // namespace vector_core
