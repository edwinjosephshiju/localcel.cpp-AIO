#include "DependencyManager.hpp"
#ifdef __APPLE__
#include <cstdlib>

namespace DependencyManager {
    std::wstring ResolveExecutable(const std::wstring& exeName) {
        std::string cmd = "which " + std::string(exeName.begin(), exeName.end()) + " > /dev/null 2>&1";
        if (system(cmd.c_str()) == 0) return exeName;
        return L"";
    }

    bool EnsureDependency(const std::wstring& name, const std::wstring& exeName, const std::wstring& packageId, std::function<bool(const std::wstring&)> askUser) {
        if (!ResolveExecutable(exeName).empty()) return true;
        std::wstring msg = name + L" is required but not found. Would you like to install it via brew?";
        if (askUser && askUser(msg)) {
            std::string cmd = "osascript -e 'tell app \"Terminal\" to do script \"brew install " + std::string(packageId.begin(), packageId.end()) + "\"'";
            system(cmd.c_str());
            return !ResolveExecutable(exeName).empty();
        }
        return false;
    }
}
#endif
