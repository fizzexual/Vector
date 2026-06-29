// Browser-level client: window lifecycle, title, and the browser side of the
// JS<->native message router (dispatched by VectorQueryHandler).
#pragma once
#include <list>
#include <memory>

#include "include/cef_client.h"
#include "include/wrapper/cef_message_router.h"

class VectorQueryHandler;

class VectorClient : public CefClient,
                     public CefLifeSpanHandler,
                     public CefDisplayHandler,
                     public CefRequestHandler {
 public:
  VectorClient();
  ~VectorClient() override;

  static VectorClient* GetInstance();

  // CefClient:
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

  // CefLifeSpanHandler:
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefDisplayHandler:
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

  // CefRequestHandler:
  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override;
  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                 TerminationStatus status,
                                 int error_code,
                                 const CefString& error_string) override;

 private:
  std::list<CefRefPtr<CefBrowser>> browsers_;
  bool is_closing_ = false;

  // Declared before router_ so router_ is destroyed first (it holds a raw
  // pointer to query_handler_).
  std::unique_ptr<VectorQueryHandler> query_handler_;
  CefRefPtr<CefMessageRouterBrowserSide> router_;

  IMPLEMENT_REFCOUNTING(VectorClient);
};
