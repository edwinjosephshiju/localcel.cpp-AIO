#pragma once
#include <string>
#include <functional>
#include <atomic>

namespace GitManager {
    extern std::wstring g_GHUserCache;
    extern std::atomic<bool> g_FetchingGHUser;
    std::wstring GetGHUser();
    void Login();
    void Logout();
    void DeployApp(const std::wstring& appName, bool currentlyDeployed, std::function<void(const std::wstring&)> logCb, std::function<void()> onDone);
    void DeleteRepo(const std::wstring& appName);
}
