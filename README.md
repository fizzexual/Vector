# Vector

A native Windows desktop game launcher in the spirit of Steam and GOG Galaxy, built on the **Chromium Embedded Framework (CEF)** — the same class of technology Steam's own client uses.

Vector automatically detects the games already installed on your PC across **Steam, Epic Games, and GOG**, presents them in a polished dark cover-art library, and launches them directly.

## Highlights

- **Real native app** (`Vector.exe`) — a C++ host embedding CEF 149 (Chromium 149). Not Electron, not a web wrapper.
- **Automatic detection** ("auto game adder") of Steam / Epic / GOG libraries via the Windows registry and each store's install manifests.
- **Dark, responsive UI** — cover-art grid, search, sort, platform/status filters, game detail view, and manual add.
- **Launches games directly** (`steam://rungameid/…`, Epic and GOG).

## Tech

- **C++20**, MSVC (Visual Studio Build Tools 2022)
- **CEF 149** (official prebuilt binary distribution)
- **CMake** with the Visual Studio / Ninja generator
- Vanilla HTML/CSS/JS renderer served to the embedded browser; native↔JS bridge over CEF's message router

## Build

Prerequisites: Visual Studio Build Tools 2022 (C++ x64), CMake 3.21+, and the CEF 149 Windows 64-bit binary distribution extracted to `third_party/cef/`.

```sh
cmake -G "Visual Studio 17 2022" -A x64 -DUSE_SANDBOX=OFF -S . -B build
cmake --build build --config Release --target Vector
```

The app is produced at `build/Release/Vector.exe`.

> `third_party/cef/` (the prebuilt SDK, ~870 MB) and `build/` are git-ignored — fetch CEF separately when building.

## Status

Active development. See [docs/design.md](docs/design.md) for the full design.
