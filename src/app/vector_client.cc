#include "app/vector_client.h"

#include "include/cef_app.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"
#include "bridge/query_handler.h"

namespace {
VectorClient* g_instance = nullptr;
}

VectorClient::VectorClient() {
  g_instance = this;

  CefMessageRouterConfig config;  // defaults: window.cefQuery
  router_ = CefMessageRouterBrowserSide::Create(config);
  query_handler_ = std::make_unique<VectorQueryHandler>();
  router_->AddHandler(query_handler_.get(), /*first=*/false);
}

VectorClient::~VectorClient() {
  g_instance = nullptr;
}

// static
VectorClient* VectorClient::GetInstance() {
  return g_instance;
}

bool VectorClient::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  return router_->OnProcessMessageReceived(browser, frame, source_process,
                                           message);
}

void VectorClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browsers_.push_back(browser);
}

bool VectorClient::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  if (browsers_.size() == 1) {
    is_closing_ = true;
  }
  return false;  // Allow the close.
}

void VectorClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  router_->OnBeforeClose(browser);
  for (auto it = browsers_.begin(); it != browsers_.end(); ++it) {
    if ((*it)->IsSame(browser)) {
      browsers_.erase(it);
      break;
    }
  }
  if (browsers_.empty()) {
    CefQuitMessageLoop();
  }
}

void VectorClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                 const CefString& title) {
  CEF_REQUIRE_UI_THREAD();
  if (auto browser_view = CefBrowserView::GetForBrowser(browser)) {
    if (auto window = browser_view->GetWindow()) {
      window->SetTitle(title);
    }
  }
}

bool VectorClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefRequest> request,
                                  bool user_gesture,
                                  bool is_redirect) {
  router_->OnBeforeBrowse(browser, frame);
  return false;
}

void VectorClient::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                             TerminationStatus status,
                                             int error_code,
                                             const CefString& error_string) {
  router_->OnRenderProcessTerminated(browser);
}

void VectorClient::OnDraggableRegionsChanged(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const std::vector<CefDraggableRegion>& regions) {
  CEF_REQUIRE_UI_THREAD();
  if (auto browser_view = CefBrowserView::GetForBrowser(browser)) {
    if (auto window = browser_view->GetWindow()) {
      window->SetDraggableRegions(regions);
    }
  }
}
