#include "core/launcher.h"

#include <windows.h>
#include <shellapi.h>

#include <string>

namespace vector_core {
namespace {

std::wstring widen(const std::string& s) {
  if (s.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring w(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
  return w;
}

std::wstring to_backslashes(std::wstring s) {
  for (auto& c : s) {
    if (c == L'/') c = L'\\';
  }
  return s;
}

bool shell_open(const std::wstring& target) {
  HINSTANCE rc = ShellExecuteW(nullptr, L"open", target.c_str(), nullptr,
                               nullptr, SW_SHOWNORMAL);
  return reinterpret_cast<INT_PTR>(rc) > 32;
}

}  // namespace

bool launch_game(const Game& g) {
  if (g.launch.value.empty()) return false;
  std::wstring target = widen(g.launch.value);
  if (g.launch.kind == "exe") {
    target = to_backslashes(target);
  }
  return shell_open(target);
}

bool show_in_folder(const Game& g) {
  std::string path = !g.installDir.empty() ? g.installDir : g.executable;
  if (path.empty()) return false;
  return shell_open(to_backslashes(widen(path)));
}

}  // namespace vector_core
