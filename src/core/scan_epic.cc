#include "core/scan_epic.h"

#include "core/strings.h"
#include "third_party/json.hpp"

namespace vector_core {

std::vector<Game> scan_epic(const ScanEnv& env) {
  std::vector<Game> games;
  if (env.program_data.empty() || !env.list_files || !env.read_file) return games;

  const std::string dir =
      to_forward_slashes(env.program_data) +
      "/Epic/EpicGamesLauncher/Data/Manifests";

  for (const std::string& fname : env.list_files(dir, "*.item")) {
    auto content = env.read_file(dir + "/" + fname);
    if (!content) continue;

    nlohmann::json j = nlohmann::json::parse(*content, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) continue;

    std::string name = j.value("DisplayName", std::string());
    std::string app_name = j.value("AppName", std::string());
    if (name.empty() || app_name.empty()) continue;

    std::string install = to_forward_slashes(j.value("InstallLocation", std::string()));
    std::string exe = j.value("LaunchExecutable", std::string());
    std::string ns = j.value("CatalogNamespace", std::string());
    std::string item = j.value("CatalogItemId", std::string());

    Game g;
    g.source = Source::Epic;
    g.id = app_name;
    g.uid = std::string("epic:") + app_name;
    g.name = name;
    g.installed = !install.empty();
    g.installDir = install;
    if (!exe.empty()) g.executable = install + "/" + exe;
    g.sizeBytes = j.value("InstallSize", 0ULL);

    if (!ns.empty() && !item.empty()) {
      g.launch.kind = "uri";
      g.launch.value = "com.epicgames.launcher://apps/" + ns + "%3A" + item +
                       "%3A" + app_name + "?action=launch&silent=true";
    } else if (!g.executable.empty()) {
      g.launch.kind = "exe";
      g.launch.value = g.executable;
    }
    games.push_back(std::move(g));
  }

  return games;
}

}  // namespace vector_core
