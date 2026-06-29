#include "core/store.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

#include "core/game_json.h"
#include "core/strings.h"

namespace vector_core {
namespace {

namespace fs = std::filesystem;

std::wstring widen(const std::string& s) {
  if (s.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring w(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
  return w;
}

std::string appdata_dir() {
  DWORD n = GetEnvironmentVariableW(L"APPDATA", nullptr, 0);
  if (n == 0) return "";
  std::wstring buf(n, L'\0');
  DWORD got = GetEnvironmentVariableW(L"APPDATA", buf.data(), n);
  buf.resize(got);
  int len = WideCharToMultiByte(CP_UTF8, 0, buf.c_str(), (int)buf.size(),
                                nullptr, 0, nullptr, nullptr);
  std::string s(len, 0);
  WideCharToMultiByte(CP_UTF8, 0, buf.c_str(), (int)buf.size(), s.data(), len,
                      nullptr, nullptr);
  return to_forward_slashes(s);
}

uint64_t now_seconds() {
  return (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string parent_dir(const std::string& path) {
  size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? "" : path.substr(0, slash);
}

}  // namespace

Store::Store() {
  std::string base = appdata_dir();
  if (base.empty()) base = ".";
  path_ = base + "/Vector/vector.json";
  load();
}

void Store::load() {
  std::ifstream f(widen(path_).c_str(), std::ios::binary);
  if (!f) return;
  std::string text((std::istreambuf_iterator<char>(f)),
                   std::istreambuf_iterator<char>());
  nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
  if (j.is_discarded() || !j.is_object()) return;

  for (const auto& u : j.value("favorites", nlohmann::json::array()))
    favorites_.insert(u.get<std::string>());
  for (const auto& u : j.value("hidden", nlohmann::json::array()))
    hidden_.insert(u.get<std::string>());
  if (j.contains("customArt") && j["customArt"].is_object()) {
    for (auto it = j["customArt"].begin(); it != j["customArt"].end(); ++it)
      custom_art_[it.key()] = it.value().get<std::string>();
  }
  for (const auto& g : j.value("manualGames", nlohmann::json::array()))
    manual_.push_back(game_from_json(g));
  if (j.contains("cache") && j["cache"].is_object()) {
    for (const auto& g : j["cache"].value("games", nlohmann::json::array()))
      cache_.push_back(game_from_json(g));
  }
  if (j.contains("settings") && j["settings"].is_object()) {
    const auto& s = j["settings"];
    scan_on_launch_ = s.value("scanOnLaunch", true);
    default_sort_ = s.value("defaultSort", std::string("recent"));
    if (s.contains("stores") && s["stores"].is_object()) {
      stores_.steam = s["stores"].value("steam", true);
      stores_.epic = s["stores"].value("epic", true);
      stores_.gog = s["stores"].value("gog", true);
    }
  }
}

void Store::save() const {
  nlohmann::json j;
  j["favorites"] = favorites_;
  j["hidden"] = hidden_;
  j["customArt"] = custom_art_;
  j["manualGames"] = nlohmann::json::array();
  for (const auto& g : manual_) j["manualGames"].push_back(to_json(g));
  j["cache"] = {{"games", nlohmann::json::array()}, {"scannedAt", now_seconds()}};
  for (const auto& g : cache_) j["cache"]["games"].push_back(to_json(g));
  j["settings"] = settings_json();

  std::error_code ec;
  fs::create_directories(fs::path(widen(parent_dir(path_))), ec);
  std::ofstream f(widen(path_).c_str(), std::ios::binary | std::ios::trunc);
  if (f) f << j.dump(2);
}

nlohmann::json Store::settings_json() const {
  return {{"scanOnLaunch", scan_on_launch_},
          {"stores",
           {{"steam", stores_.steam}, {"epic", stores_.epic}, {"gog", stores_.gog}}},
          {"defaultSort", default_sort_}};
}

ScanOptions Store::scan_options() const { return stores_; }

void Store::set_settings(const nlohmann::json& patch) {
  if (patch.contains("scanOnLaunch")) scan_on_launch_ = patch["scanOnLaunch"].get<bool>();
  if (patch.contains("defaultSort")) default_sort_ = patch["defaultSort"].get<std::string>();
  if (patch.contains("stores") && patch["stores"].is_object()) {
    stores_.steam = patch["stores"].value("steam", stores_.steam);
    stores_.epic = patch["stores"].value("epic", stores_.epic);
    stores_.gog = patch["stores"].value("gog", stores_.gog);
  }
  save();
}

void Store::apply_overlay(Game& g) const {
  g.favorite = favorites_.count(g.uid) > 0;
  g.hidden = hidden_.count(g.uid) > 0;
  auto it = custom_art_.find(g.uid);
  if (it != custom_art_.end()) g.art.cover = it->second;
}

std::vector<Game> Store::merged() const {
  std::vector<Game> out;
  out.reserve(cache_.size() + manual_.size());
  for (Game g : cache_) {
    apply_overlay(g);
    out.push_back(std::move(g));
  }
  for (Game g : manual_) {
    apply_overlay(g);
    out.push_back(std::move(g));
  }
  return out;
}

nlohmann::json Store::library_json() const {
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& g : merged()) arr.push_back(to_json(g));
  return arr;
}

void Store::update_cache(std::vector<Game> games) {
  cache_ = std::move(games);
  save();
}

void Store::set_favorite(const std::string& uid, bool value) {
  if (value) favorites_.insert(uid);
  else favorites_.erase(uid);
  save();
}

void Store::set_hidden(const std::string& uid, bool value) {
  if (value) hidden_.insert(uid);
  else hidden_.erase(uid);
  save();
}

nlohmann::json Store::add_manual(const nlohmann::json& input) {
  Game g;
  g.source = Source::Manual;
  uint64_t now = now_seconds();
  g.id = std::to_string(now);
  g.uid = "manual:" + g.id;
  g.name = input.value("name", std::string("Untitled game"));
  g.executable = to_forward_slashes(input.value("executable", std::string()));
  g.installed = !g.executable.empty();
  g.installDir = parent_dir(g.executable);
  if (!g.executable.empty()) {
    g.launch.kind = "exe";
    g.launch.value = g.executable;
  }
  std::string art = input.value("art", std::string());
  if (!art.empty()) g.art.cover = art;
  g.addedAt = now;

  manual_.push_back(g);
  save();

  Game overlaid = g;
  apply_overlay(overlaid);
  return to_json(overlaid);
}

void Store::remove_manual(const std::string& uid) {
  manual_.erase(std::remove_if(manual_.begin(), manual_.end(),
                               [&](const Game& g) { return g.uid == uid; }),
                manual_.end());
  favorites_.erase(uid);
  hidden_.erase(uid);
  custom_art_.erase(uid);
  save();
}

std::optional<Game> Store::find(const std::string& uid) const {
  for (const auto& g : merged()) {
    if (g.uid == uid) return g;
  }
  return std::nullopt;
}

}  // namespace vector_core
