# Localcel.cpp (All-In-One Modularized)

> **High-performance, modular C++20 Win32 GUI for local server management and sharing.**

[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue?logo=windows)](https://github.com/edwinjosephshiju/localcel.cpp-AIO)
[![Language](https://img.shields.io/badge/built%20with-C%2B%2B20%20%2F%20Win32-blue?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/github/license/edwinjosephshiju/localcel.cpp-AIO)](LICENSE)

**Localcel.cpp AIO** is the fully modularized, pure C++ evolution of the original Localcel project. It transitions from a monolithic Python/PyQt6 architecture to a high-performance, decoupled C++/Win32 system designed for maximum speed, stability, and zero runtime dependencies (no Python required).

---

## 🏗️ Modular Architecture

The project has been re-architected into discrete modules for better maintainability and cross-platform extensibility:

### 1. `src/core/`
- **`AppManager`**: Handles workspace loading, app configuration (JSON), and filesystem orchestration.
- **`ProcessManager`**: Manages the lifecycle of Node.js servers and Cloudflare tunnels using native Windows **Job Objects** for robust cleanup.
- **`Utils`**: Cross-platform helper functions for UTF-8 conversion, process execution, and JSON parsing.

### 2. `src/ui/`
- **`WindowsUI`**: A pure Win32 API implementation featuring a modern GDI+ theme engine, dark mode support, and native Windows 11 UI components (Mica-inspired effects).

### 3. `src/network/`
- **`CloudflareManager`**: Interface for managing Cloudflare Tunnels (`cloudflared`), including authentication and tunnel lifecycle management.

### 4. `src/deploy/`
- **`GitManager`**: Orchestrates GitHub repository creation and GitHub Pages deployment via `git` and `gh` CLI tools.

### 5. `src/deps/`
- **`DependencyManager`**: A polymorphic module that automatically detects and installs missing tools (Git, GitHub CLI, Cloudflare) via platform-native package managers like `winget`.

---

## 🚀 Key Improvements over Python Version

| Feature | Python Version | C++ (AIO) Version |
|---|---|---|
| **Binary Size** | ~9 MB (Compressed) | **~700 KB** (Native) |
| **Memory Usage** | High (PyQt6 Runtime) | **Extremely Low** (Native Win32) |
| **Startup Speed** | ~2-3 Seconds | **Near Instant** |
| **Stability** | Process-based cleanup | **Job Object-based** (Atomic cleanup) |
| **Dependencies** | Requires Python 3 + PyQt6 | **Zero Runtime Dependencies** |

---

## 🛠️ Build System

The project features a **Universal Support Build System** written in pure C++:

### `build.cpp`
A standalone C++ builder that detects the environment, compiles resources, and invokes the MSVC compiler with optimal flags for performance and size.

### `localcel_all_in_one.cpp`
An **Amalgamation (Unity Build)** file that allows for single-unit compilation, significantly simplifying the build process for different IDEs and CI/CD pipelines.

---

## 🔨 How to Build

### Prerequisites
- **Visual Studio Build Tools** (MSVC Compiler)
- **Windows SDK** (for `rc.exe`)

### 1. Simple Build (Recommended)
Open a **Developer Command Prompt for VS** and run:
```powershell
cl build.cpp /std:c++20 /EHsc
./build.exe
```

### 2. Manual Compilation
```powershell
rc resource.rc
cl localcel_all_in_one.cpp resource.res /std:c++20 /EHsc /O2 /I"src" /Fe:Localcel.exe user32.lib kernel32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib ws2_32.lib shlwapi.lib dwmapi.lib uxtheme.lib advapi32.lib gdiplus.lib
```

---

## 📄 License
Distributed under the **MIT License**. See `LICENSE` for details.

---

## ⭐ Support
If you find this project useful, please give it a star!

*Based on the original [Localcel.cpp](https://github.com/edwinjosephshiju/Localcel) architecture.*
