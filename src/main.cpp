#include "core/AppManager.hpp"
#include "ui/WindowsUI.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    if (!AppManager::TryLoadWorkspace()) {
        // RunUI handles workspace setup if TryLoadWorkspace fails.
    }
    return WindowsUI::RunUI(hInstance, nCmdShow);
}
#else
int main(int argc, char** argv) {
    // Other OS entry point (stubbed)
    return 0;
}
#endif
