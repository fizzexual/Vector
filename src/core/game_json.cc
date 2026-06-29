#include "core/game_json.h"

namespace vector_core {

namespace {
Source source_from(const std::string& s) {
  if (s == "steam") return Source::Steam;
  if (s == "epic") return Source::Epic;
  if (s == "gog") return Source::Gog;
  return Source::Manual;
}
}  // namespace

nlohmann::json to_json(const Game& g) {
  nlohmann::json j;
  j["uid"] = g.uid;
  j["source"] = source_id(g.source);
  j["id"] = g.id;
  j["name"] = g.name;
  j["installed"] = g.installed;
  j["installDir"] = g.installDir;
  j["executable"] = g.executable;
  j["sizeBytes"] = g.sizeBytes;
  j["art"] = {{"cover", g.art.cover}, {"hero", g.art.hero}, {"logo", g.art.logo}};
  j["launch"] = {{"kind", g.launch.kind}, {"value", g.launch.value}};
  j["lastPlayed"] = g.lastPlayed;
  j["favorite"] = g.favorite;
  j["hidden"] = g.hidden;
  j["addedAt"] = g.addedAt;
  return j;
}

Game game_from_json(const nlohmann::json& j) {
  Game g;
  g.uid = j.value("uid", std::string());
  g.source = source_from(j.value("source", std::string("manual")));
  g.id = j.value("id", std::string());
  g.name = j.value("name", std::string());
  g.installed = j.value("installed", false);
  g.installDir = j.value("installDir", std::string());
  g.executable = j.value("executable", std::string());
  g.sizeBytes = j.value("sizeBytes", 0ULL);
  if (j.contains("art") && j["art"].is_object()) {
    const auto& a = j["art"];
    g.art.cover = a.value("cover", std::string());
    g.art.hero = a.value("hero", std::string());
    g.art.logo = a.value("logo", std::string());
  }
  if (j.contains("launch") && j["launch"].is_object()) {
    const auto& l = j["launch"];
    g.launch.kind = l.value("kind", std::string());
    g.launch.value = l.value("value", std::string());
  }
  g.lastPlayed = j.value("lastPlayed", 0ULL);
  g.favorite = j.value("favorite", false);
  g.hidden = j.value("hidden", false);
  g.addedAt = j.value("addedAt", 0ULL);
  return g;
}

}  // namespace vector_core
