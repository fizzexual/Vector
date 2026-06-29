#include "bridge/query_handler.h"

#include <windows.h>
#include <commdlg.h>

#include <string>

#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "core/launcher.h"
#include "core/scan_all.h"
#include "core/scan_env.h"
#include "third_party/json.hpp"

namespace {

std::string narrow(const wchar_t* w) {
  if (!w || !*w) return "";
  int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  if (len <= 1) return "";
  std::string s(len - 1, 0);
  WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), len, nullptr, nullptr);
  return s;
}

std::string pick_file(const wchar_t* filter) {
  wchar_t buf[1024] = {0};
  OPENFILENAMEW ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFilter = filter;
  ofn.lpstrFile = buf;
  ofn.nMaxFile = 1024;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
  if (GetOpenFileNameW(&ofn)) return narrow(buf);
  return "";
}

void window_command(CefRefPtr<CefBrowser> browser, const std::string& action) {
  auto browser_view = CefBrowserView::GetForBrowser(browser);
  CefRefPtr<CefWindow> window = browser_view ? browser_view->GetWindow() : nullptr;
  if (!window) return;
  if (action == "minimize") {
    window->Minimize();
  } else if (action == "maximize") {
    if (window->IsMaximized()) window->Restore();
    else window->Maximize();
  } else if (action == "close") {
    window->Close();
  }
}

const wchar_t kExeFilter[] = L"Executables\0*.exe\0All files\0*.*\0";
const wchar_t kImgFilter[] = L"Images\0*.png;*.jpg;*.jpeg;*.webp;*.bmp\0All files\0*.*\0";

}  // namespace

VectorQueryHandler::VectorQueryHandler() = default;

bool VectorQueryHandler::OnQuery(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 int64_t query_id,
                                 const CefString& request,
                                 bool persistent,
                                 CefRefPtr<Callback> callback) {
  nlohmann::json j = nlohmann::json::parse(request.ToString(), nullptr, false);
  if (j.is_discarded() || !j.is_object()) {
    callback->Failure(-2, "Malformed request");
    return true;
  }
  const std::string cmd = j.value("cmd", std::string());

  if (cmd == "getSettings") {
    callback->Success(store_.settings_json().dump());
  } else if (cmd == "getLibrary") {
    callback->Success(store_.library_json().dump());
  } else if (cmd == "scan") {
    vector_core::ScanEnv env = vector_core::make_system_scan_env();
    store_.update_cache(vector_core::scan_all(env, store_.scan_options()));
    callback->Success(store_.library_json().dump());
  } else if (cmd == "getStores") {
    vector_core::ScanEnv env = vector_core::make_system_scan_env();
    vector_core::ScanOptions opts = store_.scan_options();
    nlohmann::json lib = store_.library_json();
    auto count_src = [&lib](const char* src) {
      int n = 0;
      for (const auto& g : lib)
        if (g.value("source", std::string()) == src) n++;
      return n;
    };

    nlohmann::json arr = nlohmann::json::array();

    auto steam_path = env.reg_value("HKCU", "Software\\Valve\\Steam", "SteamPath");
    if (!steam_path || steam_path->empty())
      steam_path = env.reg_value("HKLM", "SOFTWARE\\WOW6432Node\\Valve\\Steam",
                                 "InstallPath");
    bool steam_ok = steam_path && !steam_path->empty();
    arr.push_back({{"id", "steam"}, {"installed", steam_ok},
                   {"path", steam_ok ? *steam_path : std::string()},
                   {"count", count_src("steam")}, {"enabled", opts.steam}});

    std::string epic_dir =
        env.program_data + "/Epic/EpicGamesLauncher/Data/Manifests";
    bool epic_ok = env.exists && env.exists(epic_dir);
    arr.push_back({{"id", "epic"}, {"installed", epic_ok},
                   {"path", epic_ok ? epic_dir : std::string()},
                   {"count", count_src("epic")}, {"enabled", opts.epic}});

    bool gog_ok =
        env.reg_subkeys &&
        !env.reg_subkeys("HKLM", "SOFTWARE\\WOW6432Node\\GOG.com\\Games").empty();
    arr.push_back({{"id", "gog"}, {"installed", gog_ok}, {"path", std::string()},
                   {"count", count_src("gog")}, {"enabled", opts.gog}});

    for (const char* id : {"xbox", "ea", "ubisoft", "battlenet"})
      arr.push_back({{"id", id}, {"installed", false}, {"comingSoon", true},
                     {"count", 0}, {"enabled", false}});

    callback->Success(arr.dump());
  } else if (cmd == "launch") {
    auto game = store_.find(j.value("uid", std::string()));
    if (game && vector_core::launch_game(*game)) {
      callback->Success("{\"ok\":true}");
    } else {
      callback->Failure(-1, "Couldn't start the game");
    }
  } else if (cmd == "showInFolder") {
    auto game = store_.find(j.value("uid", std::string()));
    if (game) vector_core::show_in_folder(*game);
    callback->Success("{\"ok\":true}");
  } else if (cmd == "setFavorite") {
    store_.set_favorite(j.value("uid", std::string()), j.value("value", false));
    callback->Success("{\"ok\":true}");
  } else if (cmd == "setHidden") {
    store_.set_hidden(j.value("uid", std::string()), j.value("value", false));
    callback->Success("{\"ok\":true}");
  } else if (cmd == "addManualGame") {
    nlohmann::json input =
        j.contains("input") ? j["input"] : nlohmann::json::object();
    callback->Success(store_.add_manual(input).dump());
  } else if (cmd == "removeManualGame") {
    store_.remove_manual(j.value("uid", std::string()));
    callback->Success("{\"ok\":true}");
  } else if (cmd == "pickExecutable") {
    callback->Success(nlohmann::json(pick_file(kExeFilter)).dump());
  } else if (cmd == "pickImage") {
    callback->Success(nlohmann::json(pick_file(kImgFilter)).dump());
  } else if (cmd == "window") {
    window_command(browser, j.value("action", std::string()));
    callback->Success("{\"ok\":true}");
  } else {
    callback->Failure(-3, "Unknown command");
  }
  return true;
}
