#include "core/scan_steam.h"

#include <set>

#include "core/strings.h"
#include "core/vdf.h"

namespace vector_core {
namespace {

const char* kCdn = "https://cdn.cloudflare.steamstatic.com/steam/apps/";

// appids that are tools/redistributables, not games.
bool is_non_game(const std::string& appid) {
  return appid == "228980";  // Steamworks Common Redistributables
}

}  // namespace

std::vector<Game> scan_steam(const ScanEnv& env) {
  std::vector<Game> games;

  std::optional<std::string> steam_path;
  if (env.reg_value) {
    steam_path = env.reg_value("HKCU", "Software\\Valve\\Steam", "SteamPath");
    if (!steam_path || steam_path->empty()) {
      steam_path = env.reg_value("HKLM", "SOFTWARE\\WOW6432Node\\Valve\\Steam",
                                 "InstallPath");
    }
  }
  if (!steam_path || steam_path->empty()) return games;

  const std::string steam = strip_trailing_slash(to_forward_slashes(*steam_path));

  // Collect every Steam library folder (the install dir plus extras).
  std::vector<std::string> libraries;
  std::set<std::string> library_set;
  auto add_library = [&](const std::string& p) {
    std::string norm = strip_trailing_slash(to_forward_slashes(p));
    if (!norm.empty() && library_set.insert(norm).second) libraries.push_back(norm);
  };
  add_library(steam);

  if (env.read_file) {
    auto vdf = env.read_file(steam + "/steamapps/libraryfolders.vdf");
    if (vdf) {
      if (auto root = parse_vdf(*vdf)) {
        if (const VdfNode* folders = root->get("libraryfolders")) {
          for (const auto& [key, node] : folders->children) {
            if (node.is_object) add_library(node.get_str("path"));
          }
        }
      }
    }
  }

  std::set<std::string> seen;
  for (const auto& lib : libraries) {
    const std::string steamapps = lib + "/steamapps";
    if (!env.list_files) break;
    for (const std::string& fname : env.list_files(steamapps, "appmanifest_*.acf")) {
      auto content = env.read_file(steamapps + "/" + fname);
      if (!content) continue;
      auto root = parse_vdf(*content);
      if (!root) continue;
      const VdfNode* app = root->get("AppState");
      if (!app) continue;

      std::string appid = app->get_str("appid");
      std::string name = app->get_str("name");
      std::string installdir = app->get_str("installdir");
      if (appid.empty() || name.empty() || is_non_game(appid)) continue;
      if (!seen.insert(appid).second) continue;

      Game g;
      g.source = Source::Steam;
      g.id = appid;
      g.uid = std::string("steam:") + appid;
      g.name = name;
      g.installed = true;
      if (!installdir.empty())
        g.installDir = steamapps + "/common/" + installdir;
      g.sizeBytes = to_u64(app->get_str("SizeOnDisk"));
      g.lastPlayed = to_u64(app->get_str("LastPlayed"));
      g.art.cover = std::string(kCdn) + appid + "/library_600x900.jpg";
      g.art.hero = std::string(kCdn) + appid + "/library_hero.jpg";
      g.art.logo = std::string(kCdn) + appid + "/logo.png";
      g.launch.kind = "uri";
      g.launch.value = std::string("steam://rungameid/") + appid;
      games.push_back(std::move(g));
    }
  }

  return games;
}

}  // namespace vector_core
