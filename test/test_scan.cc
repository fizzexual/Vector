#include "third_party/doctest.h"

#include <map>

#include "core/scan_all.h"
#include "core/scan_epic.h"
#include "core/scan_gog.h"
#include "core/scan_steam.h"

using namespace vector_core;

namespace {
const Game* find_uid(const std::vector<Game>& games, const std::string& uid) {
  for (const auto& g : games)
    if (g.uid == uid) return &g;
  return nullptr;
}
}  // namespace

TEST_CASE("scan_steam detects games across multiple libraries") {
  std::map<std::string, std::string> files = {
      {"C:/Steam/steamapps/libraryfolders.vdf",
       R"vdf("libraryfolders"{ "0"{"path" "C:\\Steam"} "1"{"path" "D:\\GameLib"} })vdf"},
      {"C:/Steam/steamapps/appmanifest_440.acf",
       R"acf("AppState"{"appid" "440" "name" "Team Fortress 2" "installdir" "Team Fortress 2" "SizeOnDisk" "15000000000"})acf"},
      {"D:/GameLib/steamapps/appmanifest_570.acf",
       R"acf("AppState"{"appid" "570" "name" "Dota 2" "installdir" "dota 2 beta" "SizeOnDisk" "40000000000"})acf"},
  };
  std::map<std::string, std::vector<std::string>> dirs = {
      {"C:/Steam/steamapps", {"appmanifest_440.acf"}},
      {"D:/GameLib/steamapps", {"appmanifest_570.acf"}},
  };

  ScanEnv env;
  env.reg_value = [](const std::string& hive, const std::string&,
                     const std::string& value) -> std::optional<std::string> {
    if (hive == "HKCU" && value == "SteamPath") return std::string("C:/Steam");
    return std::nullopt;
  };
  env.read_file = [files](const std::string& p) -> std::optional<std::string> {
    auto it = files.find(p);
    return it == files.end() ? std::nullopt : std::optional<std::string>(it->second);
  };
  env.list_files = [dirs](const std::string& d,
                          const std::string&) -> std::vector<std::string> {
    auto it = dirs.find(d);
    return it == dirs.end() ? std::vector<std::string>() : it->second;
  };
  env.exists = [](const std::string&) { return true; };

  auto games = scan_steam(env);
  REQUIRE(games.size() == 2);

  const Game* tf2 = find_uid(games, "steam:440");
  REQUIRE(tf2 != nullptr);
  CHECK(tf2->name == "Team Fortress 2");
  CHECK(tf2->installed);
  CHECK(tf2->sizeBytes == 15000000000ULL);
  CHECK(tf2->launch.kind == "uri");
  CHECK(tf2->launch.value == "steam://rungameid/440");
  CHECK(tf2->art.cover.find("440/library_600x900.jpg") != std::string::npos);
  CHECK(tf2->installDir == "C:/Steam/steamapps/common/Team Fortress 2");

  const Game* dota = find_uid(games, "steam:570");
  REQUIRE(dota != nullptr);
  CHECK(dota->installDir == "D:/GameLib/steamapps/common/dota 2 beta");
}

TEST_CASE("scan_steam returns nothing when Steam is absent") {
  ScanEnv env;
  env.reg_value = [](const std::string&, const std::string&,
                     const std::string&) -> std::optional<std::string> {
    return std::nullopt;
  };
  CHECK(scan_steam(env).empty());
}

TEST_CASE("scan_gog reads games from the registry") {
  ScanEnv env;
  env.reg_subkeys = [](const std::string& hive,
                       const std::string& sub) -> std::vector<std::string> {
    if (hive == "HKLM" && sub.find("GOG.com\\Games") != std::string::npos)
      return {"1207664663"};
    return {};
  };
  env.reg_value = [](const std::string&, const std::string& key,
                     const std::string& value) -> std::optional<std::string> {
    if (key.find("1207664663") == std::string::npos) return std::nullopt;
    if (value == "gameName") return std::string("The Witcher: Enhanced Edition");
    if (value == "path") return std::string("D:\\GOG\\Witcher");
    if (value == "exe") return std::string("witcher.exe");
    return std::nullopt;
  };

  auto games = scan_gog(env);
  REQUIRE(games.size() == 1);
  CHECK(games[0].uid == "gog:1207664663");
  CHECK(games[0].name == "The Witcher: Enhanced Edition");
  CHECK(games[0].source == Source::Gog);
  CHECK(games[0].launch.value == "D:/GOG/Witcher/witcher.exe");
}

TEST_CASE("scan_epic parses an .item manifest") {
  ScanEnv env;
  env.program_data = "C:/ProgramData";
  env.list_files = [](const std::string& dir,
                      const std::string&) -> std::vector<std::string> {
    if (dir.find("Epic") != std::string::npos) return {"abc.item"};
    return {};
  };
  env.read_file = [](const std::string& p) -> std::optional<std::string> {
    if (p.find("abc.item") == std::string::npos) return std::nullopt;
    return std::string(R"({"DisplayName":"Rocket League","AppName":"Sugar","InstallLocation":"D:\\Epic\\rocketleague","LaunchExecutable":"Binaries\\Win64\\RocketLeague.exe","CatalogNamespace":"ns","CatalogItemId":"item","InstallSize":22000000000})");
  };

  auto games = scan_epic(env);
  REQUIRE(games.size() == 1);
  CHECK(games[0].uid == "epic:Sugar");
  CHECK(games[0].name == "Rocket League");
  CHECK(games[0].launch.kind == "uri");
  CHECK(games[0].launch.value.find("com.epicgames.launcher://") != std::string::npos);
}
