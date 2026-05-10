#pragma once
#include <string>
#include <functional>

namespace DependencyManager {
    std::wstring ResolveExecutable(const std::wstring& exeName);
    bool EnsureDependency(const std::wstring& name, const std::wstring& exeName, const std::wstring& packageId, std::function<bool(const std::wstring&)> askUser);
}
