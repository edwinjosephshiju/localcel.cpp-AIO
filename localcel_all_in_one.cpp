// localcel_all_in_one.cpp - Universal Amalgamation for Localcel
// This file includes all modules for a single-unit build.

// Core Configuration & Macros
#define _CRT_SECURE_NO_WARNINGS
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// Include all module implementations
// (Note: We include .cpp files directly for a single-executable "Unity Build")
#include "src/core/Utils.cpp"
#include "src/core/AppManager.cpp"
#include "src/core/ProcessManager.cpp"
#include "src/network/CloudflareManager.cpp"
#include "src/deploy/GitManager.cpp"
#include "src/deps/DepMgr_Windows.cpp"
#include "src/ui/WindowsUI.cpp"
#include "src/main.cpp"

// To compile this file directly:
// rc ../resource.rc
// cl localcel_all_in_one.cpp ../resource.res /std:c++20 /EHsc /O2 /I"src" /Fe:Localcel.exe
