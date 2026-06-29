// Application object. In the browser process it creates the main window; in the
// render process it installs the renderer side of the JS<->native message
// router (window.cefQuery).
#pragma once
#include "include/cef_app.h"
#include "include/wrapper/cef_message_router.h"

class VectorApp : public CefApp,
                  public CefBrowserProcessHandler,
                  public CefRenderProcessHandler {
 public:
  VectorApp();

  // CefApp:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
    return this;
  }

  // CefBrowserProcessHandler:
  void OnContextInitialized() override;
  CefRefPtr<CefClient> GetDefaultClient() override;

  // CefRenderProcessHandler:
  void OnWebKitInitialized() override;
  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override;
  void OnContextReleased(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefV8Context> context) override;
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

 private:
  // Lives in the render process.
  CefRefPtr<CefMessageRouterRendererSide> render_router_;

  IMPLEMENT_REFCOUNTING(VectorApp);
};
