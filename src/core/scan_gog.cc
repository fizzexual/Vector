#include "core/scan_gog.h"

#include "core/strings.h"

namespace vector_core {

std::vector<Game> scan_gog(const ScanEnv& env) {
  std::vector<Game> games;
  if (!env.reg_subkeys || !env.reg_value) return games;

  const std::string root = "SOFTWARE\\WOW6432Node\\GOG.com\\Games";
  for (const std::string& id : env.reg_subkeys("HKLM", root)) {
    const std::string key = root + "\\" + id;
    auto name = env.reg_value("HKLM", key, "gameName");
    if (!name || name->empty()) continue;

    auto path = env.reg_value("HKLM", key, "path");
    auto exe = env.reg_value("HKLM", key, "exe");
    auto launch_cmd = env.reg_value("HKLM", key, "launchCommand");

    Game g;
    g.source = Source::Gog;
    g.id = id;
    g.uid = std::string("gog:") + id;
    g.name = *name;
    g.installed = path && !path->empty();
    if (path) g.installDir = to_forward_slashes(*path);

    std::string exe_path;
    if (path && exe && !exe->empty())
      exe_path = to_forward_slashes(*path) + "/" + to_forward_slashes(*exe);

    if (!exe_path.empty()) {
      g.executable = exe_path;
      g.launch.kind = "exe";
      g.launch.value = exe_path;
    } else if (launch_cmd && !launch_cmd->empty()) {
      g.launch.kind = "exe";
      g.launch.value = to_forward_slashes(*launch_cmd);
    }
    games.push_back(std::move(g));
  }

  return games;
}

}  // namespace vector_core
