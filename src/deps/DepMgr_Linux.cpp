#include "DependencyManager.hpp"
#ifdef __linux__
#ifndef __ANDROID__
#include <cstdlib>

namespace DependencyManager {
    std::wstring ResolveExecutable(const std::wstring& exeName) {
        // Simple stub for linux
        std::string cmd = "which " + std::string(exeName.begin(), exeName.end()) + " > /dev/null 2>&1";
        if (system(cmd.c_str()) == 0) return exeName;
        return L"";
    }

    bool EnsureDependency(const std::wstring& name, const std::wstring& exeName, const std::wstring& packageId, std::function<bool(const std::wstring&)> askUser) {
        if (!ResolveExecutable(exeName).empty()) return true;
        std::wstring msg = name + L" is required but not found. Would you like to install it?";
        if (askUser && askUser(msg)) {
            // apt logic (stub)
            std::string cmd = "x-terminal-emulator -e sudo apt-get install -y " + std::string(packageId.begin(), packageId.end());
            system(cmd.c_str());
            return !ResolveExecutable(exeName).empty();
        }
        return false;
    }
}
#endif
#endif
