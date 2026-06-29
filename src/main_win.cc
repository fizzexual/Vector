// Vector — Windows entry point. Bootstraps CEF and runs the message loop.
#include <windows.h>

#include "include/cef_app.h"
#include "app/vector_app.h"

namespace {

int RunMain(HINSTANCE hInstance, void* sandbox_info) {
  CefMainArgs main_args(hInstance);

  // The same VectorApp is used in the browser process and every sub-process,
  // so the renderer-side message router (window.cefQuery) is installed in the
  // render process. It must be passed to CefExecuteProcess for that to happen.
  CefRefPtr<VectorApp> app(new VectorApp);

  // CEF sub-processes (render, GPU, utility) share this executable. This call
  // detects sub-process launches and runs the appropriate logic.
  int exit_code = CefExecuteProcess(main_args, app.get(), sandbox_info);
  if (exit_code >= 0) {
    return exit_code;
  }

  CefSettings settings;
  settings.no_sandbox = true;

  if (!CefInitialize(main_args, settings, app.get(), sandbox_info)) {
    return CefGetExitCode();
  }

  CefRunMessageLoop();
  CefShutdown();
  return 0;
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR lpCmdLine,
                      int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  UNREFERENCED_PARAMETER(nCmdShow);
  return RunMain(hInstance, nullptr);
}
