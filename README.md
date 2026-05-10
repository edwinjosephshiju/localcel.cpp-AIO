# Localcel.cpp AIO (All-in-One)

> A modular C++20 Win32 desktop app for managing local server apps, tunnels, and deployment workflows.

[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue?logo=windows)](https://github.com/edwinjosephshiju/localcel.cpp-AIO)
[![Language](https://img.shields.io/badge/built%20with-C%2B%2B20%20%2F%20Win32-blue?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/github/license/edwinjosephshiju/localcel.cpp-AIO)](LICENSE)

Localcel.cpp AIO is the all-in-one C++ rewrite of Localcel, focused on native performance, low overhead, and a fully modular codebase.

## Highlights

- Native Win32 UI with dark mode support
- Modular architecture (`core`, `ui`, `network`, `deploy`, `deps`)
- Process lifecycle management with Windows Job Objects
- Workspace-based app configuration and logging
- Cloudflare tunnel and Git/GitHub deployment integration

## Repository structure

```text
src/
  core/      # app config, process management, utilities
  ui/        # Win32 desktop UI
  network/   # Cloudflare tunnel operations
  deploy/    # GitHub deployment helpers
  deps/      # dependency resolution/installation
  main.cpp   # application entry point
```

## Runtime behavior

- Uses a user config file at: `%USERPROFILE%\.localcel\config.json`
- Creates and manages a `localcel_workspace` with:
  - `apps/` (per-app config)
  - `logs/` (runtime logs)

## Build notes

This repository is currently Windows/MSVC-oriented.

### Prerequisites

- Visual Studio Build Tools (MSVC)
- Windows SDK (`rc.exe`)

### Available build entry points

- `build.cpp` (script-style builder)
- `localcel_all_in_one.cpp` (single translation unit build)

> Note: in the current repository state, build paths/scripts reference Windows resource inputs that are not included.  
> To build from source, add a `resource.rc` at the repository root (with resources matching IDs used by the app, such as `IDI_APP_ICON` and `IDI_APP_LOGO`) and update script paths if your local layout differs.

## Running

A prebuilt binary is included in:

- `dist/Localcel.exe`

## License

Distributed under the MIT License. See [LICENSE](LICENSE).

---

Based on the original [localcel.cpp](https://github.com/edwinjosephshiju/localcel.cpp) architecture.
