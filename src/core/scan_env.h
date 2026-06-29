// Injected I/O for scanners — registry, filesystem, known folders.
// Production wires this to Win32 (make_system_scan_env). Tests provide fakes,
// so scanners are unit-testable without Steam/Epic/GOG installed.
#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace vector_core {

struct ScanEnv {
  // Read a REG_SZ value. `hive` is "HKCU" or "HKLM".
  std::function<std::optional<std::string>(const std::string& hive,
                                           const std::string& subkey,
                                           const std::string& value)>
      reg_value;

  // Enumerate immediate subkey names under a key.
  std::function<std::vector<std::string>(const std::string& hive,
                                         const std::string& subkey)>
      reg_subkeys;

  // Read an entire file as bytes. nullopt if missing/unreadable.
  std::function<std::optional<std::string>(const std::string& path)> read_file;

  // List file names (not full paths) in `dir` matching a wildcard `glob`.
  std::function<std::vector<std::string>(const std::string& dir,
                                         const std::string& glob)>
      list_files;

  // True if a file or directory exists.
  std::function<bool(const std::string& path)> exists;

  // Known folders.
  std::string program_data;  // e.g. "C:/ProgramData"
};

// Build a ScanEnv backed by real Win32 APIs.
ScanEnv make_system_scan_env();

}  // namespace vector_core
