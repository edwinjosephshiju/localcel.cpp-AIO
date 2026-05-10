#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

struct AppConfig {
    std::wstring name;
    int port = 3000;
    std::wstring entry = L"server.js";
    std::wstring domain = L"";
    std::wstring app_type = L"node";
    std::wstring github_repo = L"";
    bool gh_pages_deployed = false;
};

namespace AppManager {
    extern fs::path g_BaseDir;
    extern fs::path g_AppsDir;
    extern fs::path g_LogsDir;
    extern std::vector<AppConfig> g_Apps;

    fs::path GetUserProfile();
    bool TryLoadWorkspace();
    void SetWorkspaceDir(const fs::path& dir);
    void EnsureDirectories();
    void LoadApps();
    void SaveAppConfig(const AppConfig& app);
    std::wstring SerializeJson(const AppConfig& app);
}
