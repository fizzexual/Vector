# Vector — Desktop Game Launcher (CEF)

- **Date:** 2026-06-29
- **Status:** Draft for review
- **Owner:** fizzexual

## 1. Summary

Vector is a native Windows desktop application — a personal game library and launcher in the spirit of Steam and GOG Galaxy. It **automatically detects** the games already installed on the PC across multiple stores, presents them in a polished, dark, cover-art library, and **launches** them directly. No manual cataloguing required; the "auto game adder" is the core of the product.

Vector is built on **CEF (the Chromium Embedded Framework)** — the same technology Steam itself uses: a native C++ host that embeds Chromium to render the UI, with game detection and launching written in native C++ (Win32). It is a brand-new, self-contained application, unrelated to the existing TotalReach CRM in this repository, and lives in its own `vector/` folder.

## 2. Goals

- A real, installable native Windows app (`Vector.exe`) built on CEF — not a web page, not a script, not Electron.
- **Automatic detection** of installed games from **Steam, Epic Games, and GOG**.
- A professional, dark, responsive library UI rendered by embedded Chromium: cover grid, search, sort, platform/status filters, and a game detail view.
- **Launch** a detected game directly from Vector.
- Real cover art where available (Steam), with a clean fallback tile otherwise.
- Remembers user state between runs: favorites, hidden games, manually added games, and a cached library for instant startup.
- A **manual "Add a game"** path for anything not auto-detected.

## 3. Non-goals (YAGNI)

- No store/storefront, purchasing, or browsing games for sale.
- No accounts, cloud sync, or online backend. Everything is local.
- No social features (friends, chat, achievements feed).
- No in-app downloading/installing/updating of games — Vector detects and launches what is already installed.
- No macOS/Linux build (Windows only).
- No live playtime tracking in v1 (best-effort "last played" only).
- No Xbox/Microsoft Store, EA, Ubisoft, or Battle.net detection in v1 (architecture leaves room to add them).
- We do **not** compile Chromium/CEF from source — we link the official prebuilt CEF binary distribution.

## 4. Technology and rationale

| Concern | Choice | Why |
| --- | --- | --- |
| App framework | **CEF 149** (Chromium 149.0.7827.201), prebuilt `standard` win64 distribution | The genuine article — exactly how Steam embeds Chromium. Prebuilt binaries mean no Chromium-from-source build. |
| Host language | **C++17, MSVC** (VS Build Tools 2022) | Prebuilt CEF is MSVC-built; the host must match its ABI/runtime. (MinGW `g++` is present but unusable against MSVC CEF.) |
| Build | **CMake** (Visual Studio 17 2022 generator) | CEF ships first-class CMake macros; the VS generator auto-detects Build Tools. |
| Native↔JS bridge | **CefMessageRouter** (`window.cefQuery`) | The standard, secure CEF mechanism for JS→native calls with async replies. |
| UI assets | **Vanilla HTML/CSS/JS** served via a custom `vector://app/` scheme | No framework/build toolchain coupled into the C++ project; instant load; full fetch/module support via a registered standard+secure scheme. |
| JSON | vendored **nlohmann/json** (single header) | Epic manifests + our persistence file. |
| Tests | vendored **doctest** (single header) + CTest | Pure C++ unit tests with no CEF dependency. |
| Persistence | hand-rolled JSON store at `%APPDATA%\Vector\vector.json` | Small, dependency-free, easy to inspect. |
| Registry/FS | **Win32 API** (`RegGetValueW`, etc.) | Native, no shelling out. |

Rejected: **CEF-from-source** (multi-hour, 100+ GB, no benefit); **Electron** (great, but the user explicitly wants CEF/Steam's approach); **Tauri** (Rust + system webview, not CEF); **Python/Tk** (explicitly unwanted).

Deliberate dependency avoidance: GOG detection uses the **registry** rather than the Galaxy SQLite DB, so we avoid a native SQLite module and a DB-lock fight while Galaxy is running.

## 5. Architecture

### 5.1 CEF process model and bootstrap

CEF is multi-process (browser + render/GPU/utility subprocesses). A single `Vector.exe` serves every role: the same binary re-launches itself as its own subprocesses, distinguished at startup by `CefExecuteProcess`.

```
WinMain
  CefEnableHighDPISupport (via manifest)
  CefExecuteProcess(args, app)   ── if subprocess: run + return immediately
  CefSettings (multi_threaded_message_loop=false, custom scheme registered)
  CefInitialize(app)
  app.OnContextInitialized → create top-level window + CefBrowser(vector://app/index.html)
  CefRunMessageLoop()
  CefShutdown()
```

Based on the SDK's `tests/cefsimple` + `tests/shared` (included in the `standard` distribution).

### 5.2 Host objects (C++)

- **`VectorApp : CefApp, CefBrowserProcessHandler, CefRenderProcessHandler`**
  - Registers the `vector` custom scheme (standard, secure, CORS-enabled, fetch-enabled).
  - `OnContextInitialized` (browser process): creates the native window and the browser, installs the scheme handler factory.
  - Render process: owns the `CefMessageRouterRendererSide` so `window.cefQuery` exists in the page.
- **`VectorClient : CefClient, CefLifeSpanHandler, CefLoadHandler, CefDisplayHandler, CefKeyboardHandler`**
  - Tracks the browser, sets the window title, handles close, routes messages to the message router, optional F12 devtools in debug.
- **`VectorQueryHandler : CefMessageRouterBrowserSide::Handler`**
  - Receives `window.cefQuery` requests, parses the JSON command, dispatches to the core services on a worker thread, and replies with a JSON result via `callback->Success(json)` / `Failure(code, msg)`.
- **`AppSchemeHandlerFactory` / `AppSchemeHandler : CefResourceHandler`**
  - Serves `vector://app/<path>` from the on-disk `resources/app/` directory with correct MIME types.

### 5.3 The bridge (JS ↔ native)

A small JS shim in the renderer wraps `window.cefQuery` into a promise API mirroring the product's needs:

```js
// resources/app/bridge.js
window.vector = {
  scan:           ()        => call({ cmd: 'scan' }),
  getLibrary:     ()        => call({ cmd: 'getLibrary' }),
  launch:         (uid)     => call({ cmd: 'launch', uid }),
  showInFolder:   (uid)     => call({ cmd: 'showInFolder', uid }),
  setFavorite:    (uid, v)  => call({ cmd: 'setFavorite', uid, value: v }),
  setHidden:      (uid, v)  => call({ cmd: 'setHidden', uid, value: v }),
  addManualGame:  (input)   => call({ cmd: 'addManualGame', input }),
  removeManualGame:(uid)    => call({ cmd: 'removeManualGame', uid }),
  pickExecutable: ()        => call({ cmd: 'pickExecutable' }),
  pickImage:      ()        => call({ cmd: 'pickImage' }),
  getSettings:    ()        => call({ cmd: 'getSettings' }),
  setSettings:    (patch)   => call({ cmd: 'setSettings', patch }),
};
```

`call` issues one `window.cefQuery` and resolves/rejects a Promise. The handler dispatches each `cmd` to a core service (§5.4). Long operations (`scan`) run on a CEF worker thread (`TID_FILE_USER_BLOCKING`) so the UI thread never blocks; the reply is posted back when done.

### 5.4 Core services (C++, CEF-independent, unit-tested)

Each is a small unit with one purpose and injected I/O (file reader + registry reader) so it can be tested with fixtures, no launcher installed.

- **`core/vdf`** — `parse_vdf(text) -> VdfNode`. Pure parser for Valve's `.vdf`/`.acf`.
- **`core/scan_steam`** — registry `SteamPath` → `libraryfolders.vdf` → `appmanifest_*.acf` → `Game[]`.
- **`core/scan_epic`** — `*.item` JSON manifests → `Game[]`.
- **`core/scan_gog`** — registry `GOG.com\Games\*` → `Game[]`.
- **`core/scan_all`** — runs enabled scanners, isolates per-store failures, dedupes by `uid`.
- **`core/artwork`** — resolves cover/hero/logo; Steam local `librarycache` or CDN URL; otherwise a generated tile descriptor.
- **`core/launcher`** — `build_launch_command(game)` (pure, tested) + `launch()`/`show_in_folder()` via `ShellExecuteW`.
- **`core/store`** — load/save the JSON persistence file; apply overlays (favorites/hidden/custom art/manual) onto the scanned set.
- **`core/registry_win`** — thin Win32 registry reader behind an interface for test injection.

## 6. Data model

### 6.1 `Game` (runtime, serialized to JSON for the bridge)

```jsonc
{
  "uid": "steam:1091500",      // `${source}:${id}` — stable identity
  "source": "steam",            // steam | epic | gog | manual
  "id": "1091500",
  "name": "Cyberpunk 2077",
  "installed": true,
  "installDir": "D:/SteamLibrary/steamapps/common/Cyberpunk 2077",
  "executable": "bin/x64/Cyberpunk2077.exe",
  "sizeBytes": 70000000000,
  "art": { "cover": "https://.../library_600x900.jpg", "hero": "", "logo": "" },
  "launch": { "kind": "uri", "value": "steam://rungameid/1091500" },
  "lastPlayed": 0,
  "favorite": false,
  "hidden": false,
  "addedAt": 0
}
```

### 6.2 Persistence (`%APPDATA%\Vector\vector.json`)

```jsonc
{
  "favorites": ["steam:1091500"],
  "hidden": [],
  "manualGames": [ /* Game-shaped, source:"manual" */ ],
  "customArt": { "epic:Fortnite": "C:/.../cover.png" },
  "settings": {
    "scanOnLaunch": true,
    "stores": { "steam": true, "epic": true, "gog": true },
    "defaultSort": "recent"
  },
  "cache": { "games": [ /* last scan */ ], "scannedAt": 0 }
}
```

`getLibrary` returns `cache.games` with favorite/hidden/custom-art overlays applied and manual games merged, so the UI paints instantly on launch; a background `scan` then refreshes it.

## 7. Detection details (concrete)

**Steam** — registry `HKCU\Software\Valve\Steam` `SteamPath` (fallback `HKLM\...\WOW6432Node\Valve\Steam` `InstallPath`) → `steamapps/libraryfolders.vdf` (every library `path`) → each `steamapps/appmanifest_<appid>.acf` (`appid`, `name`, `installdir`, `SizeOnDisk`, `StateFlags`, `LastPlayed`). Install path `<lib>/steamapps/common/<installdir>`. Art: local `appcache/librarycache/.../<appid>_library_600x900.jpg` else `https://cdn.cloudflare.steamstatic.com/steam/apps/<appid>/library_600x900.jpg`. Launch `steam://rungameid/<appid>`.

**Epic** — `%PROGRAMDATA%\Epic\EpicGamesLauncher\Data\Manifests\*.item` (JSON: `DisplayName`, `InstallLocation`, `LaunchExecutable`, `AppName`, `CatalogNamespace`, `CatalogItemId`, `InstallSize`). Launch `com.epicgames.launcher://apps/<ns>:<catalogItemId>:<appName>?action=launch&silent=true`, fallback to running `LaunchExecutable`. Art: generated tile.

**GOG** — registry `HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\<gameId>` (`gameName`, `path`, `exe`, `launchCommand`). Launch `launchCommand`/`exe`. Art: generated tile.

## 8. Renderer UI (vanilla)

`resources/app/` — the dark library from the approved mockup:

- `index.html`, `styles.css`, `app.js`, `bridge.js`, `assets/`.
- Layout: titlebar (Vector brand, frameless window controls) · sidebar (Library, Recent, Favorites, Installed, platform filters, Add a game, Settings) · top bar (search, sort, **Rescan**) · responsive cover grid.
- `GameCard`: cover art, platform badge, favorite heart, hover **Play**.
- `GameDetail`: hero art, size, install path, Play / Show in folder / favorite.
- `AddGameModal`: pick `.exe` + title + optional art (native dialogs via the bridge).
- Search/sort/filter computed in JS over the in-memory `Game[]`.
- Frameless window: custom titlebar drag region via `-webkit-app-region`, with native min/max/close routed through the bridge.

## 9. Data flow

1. Launch → host reads `cache` → renderer paints instantly.
2. If `settings.scanOnLaunch`, host runs `scan_all` on a worker thread → updates cache → notifies renderer.
3. **Rescan** button → `scan` query → grid updates on reply.
4. Filter/sort/search → renderer-only.
5. **Play** → `launch` query → `ShellExecuteW`; failures return a structured error → toast.

## 10. Error handling

- No launcher / missing path → that scanner returns empty; never throws to the aggregator.
- Parse failure on one manifest/key → skip + log + continue; one bad file never drops a library.
- Missing art → generated tile; never a broken image.
- Launch failure → caught in host, returned as `{ok:false,code,msg}` → non-blocking toast.
- Empty library → friendly empty state noting which launchers were detected.
- CEF init failure → log to `%APPDATA%\Vector\vector.log` and show a native message box.

## 11. Testing strategy

Pure C++ units compiled into `vector_tests.exe` (doctest + CTest), runnable here with no launcher installed:

- `parse_vdf` — nesting, quoted values, comments, malformed input.
- `scan_steam`/`scan_epic`/`scan_gog` — fixture files + fake registry reader → expected `Game[]`.
- `build_launch_command` — correct URI/exe per source.
- `store` overlay/merge — cache + favorites/hidden/custom-art/manual.
- `registry_win` parser — against captured values.

**Manual verification:** build `vector_tests.exe` and run CTest (full); configure + build `Vector.exe` (confirms the CEF link + host compile); launch it (confirms the process starts and CEF initializes). The renderer UI is additionally smoke-tested in a normal browser against a **mock `window.vector`**, so the layout/interactions are verified visually even though the native CEF window can't be observed through tooling. Final visual confirmation: the user runs `Vector.exe`.

## 12. Folder structure

```
vector/
  CMakeLists.txt
  cmake/                       (CEF macros + FindCEF wiring)
  third_party/cef/             (extracted CEF SDK — git-ignored)
  src/
    main_win.cpp               (WinMain bootstrap)
    app/ vector_app.{h,cc}  vector_client.{h,cc}
    bridge/ query_handler.{h,cc}
    scheme/ app_scheme_handler.{h,cc}
    core/ types.h  vdf.{h,cc}  scan_steam.{h,cc}  scan_epic.{h,cc}
          scan_gog.{h,cc}  scan_all.{h,cc}  artwork.{h,cc}
          launcher.{h,cc}  store.{h,cc}  registry_win.{h,cc}
    third_party/ json.hpp  doctest.h
  resources/app/               (index.html, styles.css, app.js, bridge.js, assets)
  test/ *.cc  fixtures/
  .gitignore                   (third_party/cef, build/)
```

## 13. Build & packaging

- Configure: `cmake -G "Visual Studio 17 2022" -A x64 -DCEF_ROOT=third_party/cef -S vector -B vector/build` (fallback `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` if CMake 4.x rejects an old policy).
- Build: `cmake --build vector/build --config Release`.
- CEF's `COPY_FILES` macro lays `libcef.dll`, `*.bin`, `resources/`, `locales/` next to `Vector.exe`; we also copy `resources/app/`.
- **Distribution:** first a runnable portable Release folder; then a real installer via **Inno Setup** or **NSIS** (may require installing the installer tool — flagged as a later milestone).

## 14. Build order (milestones)

1. **Extract CEF + scaffold + "hello window"** — CMake project links libcef + wrapper; `Vector.exe` opens a CEF window loading `vector://app/index.html` (static placeholder). Confirms the whole toolchain end-to-end.
2. **Bridge + Steam end-to-end** — message router + `window.vector`; `core/vdf` + `core/scan_steam` (+ tests); real Steam games render in the grid with art and a working **Play**. *(First usable app.)*
3. **Full library UX** — favorites, hidden, manual add, search, sort, filters, detail view, settings, empty state, persistence.
4. **Epic + GOG** — both scanners (+ tests) into the aggregate.
5. **Package & polish** — frameless window chrome, app icon, installer, final pass.

## 15. Honest limitations

- Bigger, slower build than Electron; first CMake configure + `libcef_dll_wrapper` compile takes a while.
- The native CEF window can't be observed through the assistant's tooling; verification is via unit tests, a successful build, a confirmed process launch, and a browser preview of the UI with a mocked bridge. Final visual confirmation is the user's.
- CEF runtime files (~hundreds of MB) sit beside the exe and in `third_party/` — git-ignored, not committed.
- Auto-detection only finds games for launchers actually installed.
- Rich cover art is automatic for Steam; Epic/GOG use a generated tile (a SteamGridDB key could fill full art later).
- A signed installer needs a code-signing cert (out of scope); the installer will be unsigned.
