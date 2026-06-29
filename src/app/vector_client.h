// Browser-level client: window lifecycle + title.
#pragma once
#include <list>

#include "include/cef_client.h"

class VectorClient : public CefClient,
                     public CefLifeSpanHandler,
                     public CefDisplayHandler {
 public:
  VectorClient();
  ~VectorClient() override;

  static VectorClient* GetInstance();

  // CefClient:
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

  // CefLifeSpanHandler:
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefDisplayHandler:
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

 private:
  std::list<CefRefPtr<CefBrowser>> browsers_;
  bool is_closing_ = false;

  IMPLEMENT_REFCOUNTING(VectorClient);
};
