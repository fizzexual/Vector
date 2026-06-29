// Win32-backed implementation of ScanEnv (production).
#include "core/scan_env.h"

#include <windows.h>

#include <fstream>
#include <iterator>

#include "core/strings.h"

namespace vector_core {
namespace {

std::wstring widen(const std::string& s) {
  if (s.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring w(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
  return w;
}

std::string narrow(const std::wstring& w) {
  if (w.empty()) return "";
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0,
                              nullptr, nullptr);
  std::string s(n, 0);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr,
                      nullptr);
  return s;
}

HKEY hive_key(const std::string& hive) {
  return hive == "HKLM" ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

std::optional<std::string> reg_value_impl(const std::string& hive,
                                          const std::string& subkey,
                                          const std::string& value) {
  HKEY root = hive_key(hive);
  std::wstring wsub = widen(subkey), wval = widen(value);
  DWORD flags = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND;
  DWORD size = 0;
  if (RegGetValueW(root, wsub.c_str(), wval.c_str(), flags, nullptr, nullptr,
                   &size) != ERROR_SUCCESS ||
      size == 0) {
    return std::nullopt;
  }
  std::wstring buf(size / sizeof(wchar_t), L'\0');
  if (RegGetValueW(root, wsub.c_str(), wval.c_str(), flags, nullptr, buf.data(),
                   &size) != ERROR_SUCCESS) {
    return std::nullopt;
  }
  while (!buf.empty() && buf.back() == L'\0') buf.pop_back();
  return narrow(buf);
}

std::vector<std::string> reg_subkeys_impl(const std::string& hive,
                                          const std::string& subkey) {
  std::vector<std::string> out;
  HKEY h;
  if (RegOpenKeyExW(hive_key(hive), widen(subkey).c_str(), 0, KEY_READ, &h) !=
      ERROR_SUCCESS) {
    return out;
  }
  wchar_t name[256];
  for (DWORD i = 0;; ++i) {
    DWORD len = 256;
    if (RegEnumKeyExW(h, i, name, &len, nullptr, nullptr, nullptr, nullptr) !=
        ERROR_SUCCESS) {
      break;
    }
    out.push_back(narrow(std::wstring(name, len)));
  }
  RegCloseKey(h);
  return out;
}

std::optional<std::string> read_file_impl(const std::string& path) {
  std::ifstream f(widen(path).c_str(), std::ios::binary);
  if (!f) return std::nullopt;
  return std::string((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
}

std::vector<std::string> list_files_impl(const std::string& dir,
                                         const std::string& glob) {
  std::vector<std::string> out;
  std::wstring pattern = widen(dir + "/" + glob);
  for (auto& c : pattern) {
    if (c == L'/') c = L'\\';
  }
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return out;
  do {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      out.push_back(narrow(fd.cFileName));
    }
  } while (FindNextFileW(h, &fd));
  FindClose(h);
  return out;
}

bool exists_impl(const std::string& path) {
  std::wstring w = widen(path);
  for (auto& c : w) {
    if (c == L'/') c = L'\\';
  }
  return GetFileAttributesW(w.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::string env_var(const wchar_t* name) {
  DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
  if (n == 0) return "";
  std::wstring buf(n, L'\0');
  DWORD got = GetEnvironmentVariableW(name, buf.data(), n);
  buf.resize(got);
  return narrow(buf);
}

}  // namespace

ScanEnv make_system_scan_env() {
  ScanEnv e;
  e.reg_value = reg_value_impl;
  e.reg_subkeys = reg_subkeys_impl;
  e.read_file = read_file_impl;
  e.list_files = list_files_impl;
  e.exists = exists_impl;
  e.program_data = to_forward_slashes(env_var(L"ProgramData"));
  return e;
}

}  // namespace vector_core
