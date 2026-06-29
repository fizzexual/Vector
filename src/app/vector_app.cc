#include "app/vector_app.h"

#include <windows.h>

#include <fstream>
#include <iterator>
#include <string>

#include "include/cef_browser.h"
#include "include/cef_image.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"
#include "app/vector_client.h"

namespace {

std::wstring ExeDir() {
  wchar_t buf[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring path(buf);
  size_t slash = path.find_last_of(L"\\/");
  return (slash == std::wstring::npos) ? L"." : path.substr(0, slash);
}

// Absolute, space-safe file:// URL to the bundled UI.
std::string AppIndexUrl() {
  std::wstring file = ExeDir() + L"\\resources\\app\\index.html";
  for (auto& c : file) {
    if (c == L'\\') c = L'/';
  }
  std::string utf8 = CefString(file).ToString();
  std::string encoded;
  for (char c : utf8) {
    if (c == ' ') encoded += "%20";
    else encoded += c;
  }
  return "file:///" + encoded;
}

CefRefPtr<CefImage> LoadAppIcon() {
  std::wstring p = ExeDir() + L"\\resources\\app\\icon.png";
  std::ifstream f(p.c_str(), std::ios::binary);
  if (!f) return nullptr;
  std::string data((std::istreambuf_iterator<char>(f)),
                   std::istreambuf_iterator<char>());
  if (data.empty()) return nullptr;
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  image->AddPNG(1.0f, data.data(), data.size());
  return image;
}

// Hosts the BrowserView inside a frameless top-level window. The in-app HTML
// titlebar provides the window chrome (drag, min/max/close).
class VectorWindowDelegate : public CefWindowDelegate {
 public:
  explicit VectorWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
      : browser_view_(browser_view) {}

  VectorWindowDelegate(const VectorWindowDelegate&) = delete;
  VectorWindowDelegate& operator=(const VectorWindowDelegate&) = delete;

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window->AddChildView(browser_view_);
    window->SetTitle("Vector");
    if (auto icon = LoadAppIcon()) window->SetWindowIcon(icon);
    window->CenterWindow(CefSize(1180, 760));
    window->Show();
    browser_view_->RequestFocus();
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    browser_view_ = nullptr;
  }

  bool IsFrameless(CefRefPtr<CefWindow> window) override { return true; }
  bool CanResize(CefRefPtr<CefWindow> window) override { return true; }
  bool CanMaximize(CefRefPtr<CefWindow> window) override { return true; }
  bool CanMinimize(CefRefPtr<CefWindow> window) override { return true; }

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

// ---- Render process: JS<->native message router ----

void VectorApp::OnWebKitInitialized() {
  CefMessageRouterConfig config;  // defaults: window.cefQuery / cefQueryCancel
  render_router_ = CefMessageRouterRendererSide::Create(config);
}

void VectorApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Context> context) {
  if (render_router_) render_router_->OnContextCreated(browser, frame, context);
}

void VectorApp::OnContextReleased(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefV8Context> context) {
  if (render_router_) render_router_->OnContextReleased(browser, frame, context);
}

bool VectorApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         CefProcessId source_process,
                                         CefRefPtr<CefProcessMessage> message) {
  if (render_router_) {
    return render_router_->OnProcessMessageReceived(browser, frame,
                                                    source_process, message);
  }
  return false;
}
