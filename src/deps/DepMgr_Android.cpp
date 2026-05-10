#include "DependencyManager.hpp"
#ifdef __ANDROID__

namespace DependencyManager {
    std::wstring ResolveExecutable(const std::wstring& exeName) {
        // On Android, dependencies are typically packed natively.
        return L"";
    }

    bool EnsureDependency(const std::wstring& name, const std::wstring& exeName, const std::wstring& packageId, std::function<bool(const std::wstring&)> askUser) {
        // Handled via native app side
        return false;
    }
}
#endif
