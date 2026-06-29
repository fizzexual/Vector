#include "app/vector_app.h"

#include <windows.h>

#include <string>

#include "include/cef_browser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"
#include "app/vector_client.h"

namespace {

// Absolute file:// URL to the bundled UI (resources/app/index.html, copied
// next to the executable at build time). Spaces are percent-encoded so the
// URL is valid even under paths like "D:\New folder\...".
std::string AppIndexUrl() {
  wchar_t buf[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring path(buf);
  size_t slash = path.find_last_of(L"\\/");
  std::wstring dir = (slash == std::wstring::npos) ? L"." : path.substr(0, slash);
  std::wstring file = dir + L"\\resources\\app\\index.html";
  for (auto& c : file) {
    if (c == L'\\') c = L'/';
  }
  CefString cef_path(file);
  std::string utf8 = cef_path.ToString();

  std::string encoded;
  for (char c : utf8) {
    if (c == ' ') {
      encoded += "%20";
    } else {
      encoded += c;
    }
  }
  return "file:///" + encoded;
}

// Hosts the BrowserView inside a top-level window.
class VectorWindowDelegate : public CefWindowDelegate {
 public:
  explicit VectorWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
      : browser_view_(browser_view) {}

  VectorWindowDelegate(const VectorWindowDelegate&) = delete;
  VectorWindowDelegate& operator=(const VectorWindowDelegate&) = delete;

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window->AddChildView(browser_view_);
    window->SetTitle("Vector");
    window->CenterWindow(CefSize(1180, 760));
    window->Show();
    browser_view_->RequestFocus();
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    browser_view_ = nullptr;
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    return browser ? browser->GetHost()->TryCloseBrowser() : true;
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(1180, 760);
  }

  CefSize GetMinimumSize(CefRefPtr<CefView> view) override {
    return CefSize(900, 560);
  }

  cef_show_state_t GetInitialShowState(CefRefPtr<CefWindow> window) override {
    return CEF_SHOW_STATE_NORMAL;
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;
  IMPLEMENT_REFCOUNTING(VectorWindowDelegate);
};

}  // namespace

VectorApp::VectorApp() = default;

void VectorApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  CefRefPtr<VectorClient> client(new VectorClient());
  CefBrowserSettings browser_settings;
  std::string url = AppIndexUrl();

  CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
      client, url, browser_settings, nullptr, nullptr, nullptr);

  CefWindow::CreateTopLevelWindow(new VectorWindowDelegate(browser_view));
}

CefRefPtr<CefClient> VectorApp::GetDefaultClient() {
  return VectorClient::GetInstance();
}
