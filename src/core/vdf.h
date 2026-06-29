// Parser for Valve's KeyValues text format (.vdf / .acf).
// Used to read Steam's libraryfolders.vdf and appmanifest_*.acf.
// Pure: no I/O, fully unit-testable.
#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vector_core {

struct VdfNode {
  bool is_object = false;
  std::string value;                                        // when !is_object
  std::vector<std::pair<std::string, VdfNode>> children;    // when is_object (ordered, dups allowed)

  // Case-insensitive lookup of the first child with the given key.
  const VdfNode* get(const std::string& key) const;
  // Scalar value of a child key, or `def` if missing / is an object.
  std::string get_str(const std::string& key, const std::string& def = "") const;
};

// Parses a full VDF document into a root object whose children are the
// top-level key/value pairs. Returns std::nullopt only on gross failure
// (the parser is lenient and skips malformed fragments).
std::optional<VdfNode> parse_vdf(const std::string& text);

}  // namespace vector_core
