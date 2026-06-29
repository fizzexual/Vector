// Dispatches window.cefQuery requests from the UI to the native core.
#pragma once
#include "include/wrapper/cef_message_router.h"

#include "core/store.h"

class VectorQueryHandler : public CefMessageRouterBrowserSide::Handler {
 public:
  VectorQueryHandler();

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override;

 private:
  vector_core::Store store_;
};
