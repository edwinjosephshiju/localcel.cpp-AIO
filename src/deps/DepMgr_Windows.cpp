#include "DependencyManager.hpp"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <vector>

namespace DependencyManager {
    std::wstring ResolveExecutable(const std::wstring& exeName) {
        wchar_t path[MAX_PATH];
        if (SearchPathW(NULL, exeName.c_str(), L".exe", MAX_PATH, path, NULL)) return path;
        if (SearchPathW(NULL, exeName.c_str(), L".cmd", MAX_PATH, path, NULL)) return path;
        return L"";
    }

    bool EnsureDependency(const std::wstring& name, const std::wstring& exeName, const std::wstring& packageId, std::function<bool(const std::wstring&)> askUser) {
        if (!ResolveExecutable(exeName).empty()) return true;
        std::wstring msg = name + L" is required but not found.\n\nWould you like to install it automatically via winget?";
        if (askUser && askUser(msg)) {
            STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = { 0 };
            std::wstring cmd = L"winget install --id " + packageId + L" -e --accept-package-agreements --accept-source-agreements";
            std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end()); cmdBuf.push_back(0);
            if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, INFINITE); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
            }
            return !ResolveExecutable(exeName).empty();
        }
        return false;
    }
}
#endif
