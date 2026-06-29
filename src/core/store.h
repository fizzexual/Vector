// Local persistence: favorites, hidden, manual games, custom art, settings,
// and the last-scan library cache. Stored as JSON in %APPDATA%/Vector.
#pragma once
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "core/scan_all.h"
#include "core/types.h"
#include "third_party/json.hpp"

namespace vector_core {

class Store {
 public:
  Store();

  nlohmann::json settings_json() const;
  ScanOptions scan_options() const;
  bool scan_on_launch() const { return scan_on_launch_; }
  void set_settings(const nlohmann::json& patch);

  // cache + manual games, with favorite/hidden/custom-art overlays applied.
  nlohmann::json library_json() const;
  void update_cache(std::vector<Game> games);

  void set_favorite(const std::string& uid, bool value);
  void set_hidden(const std::string& uid, bool value);
  nlohmann::json add_manual(const nlohmann::json& input);
  void remove_manual(const std::string& uid);

  std::optional<Game> find(const std::string& uid) const;

 private:
  void load();
  void save() const;
  std::vector<Game> merged() const;
  void apply_overlay(Game& g) const;

  std::string path_;
  std::set<std::string> favorites_;
  std::set<std::string> hidden_;
  std::map<std::string, std::string> custom_art_;
  std::vector<Game> manual_;
  std::vector<Game> cache_;

  bool scan_on_launch_ = true;
  ScanOptions stores_;
  std::string default_sort_ = "recent";
};

}  // namespace vector_core
