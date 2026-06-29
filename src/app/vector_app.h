// Browser-process application object: creates the main window.
#pragma once
#include "include/cef_app.h"

class VectorApp : public CefApp, public CefBrowserProcessHandler {
 public:
  VectorApp();

  // CefApp:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  // CefBrowserProcessHandler:
  void OnContextInitialized() override;
  CefRefPtr<CefClient> GetDefaultClient() override;

 private:
  IMPLEMENT_REFCOUNTING(VectorApp);
};
