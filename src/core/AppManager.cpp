#include "AppManager.hpp"
#include "Utils.hpp"
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace AppManager {
    fs::path g_BaseDir;
    fs::path g_AppsDir;
    fs::path g_LogsDir;
    std::vector<AppConfig> g_Apps;

    fs::path GetUserProfile() {
#ifdef _WIN32
        const wchar_t* up = _wgetenv(L"USERPROFILE"); return fs::path(up ? up : L"C:\\");
#else
        const char* home = getenv("HOME"); return fs::path(home ? home : "/");
#endif
    }

    void EnsureDirectories() {
        g_AppsDir = g_BaseDir / L"apps";
        g_LogsDir = g_BaseDir / L"logs";
        fs::create_directories(g_AppsDir);
        fs::create_directories(g_LogsDir);
    }

    bool TryLoadWorkspace() {
        fs::path localcelDir = GetUserProfile() / L".localcel";
        fs::create_directories(localcelDir);
        fs::path configPath = localcelDir / L"config.json";
        
        if (fs::exists(configPath)) {
            std::wifstream in(configPath);
            std::wstringstream ss; ss << in.rdbuf();
            g_BaseDir = Utils::ExtractJsonWString(ss.str(), L"workspace");
            if (fs::exists(g_BaseDir)) { EnsureDirectories(); return true; }
        }
        return false;
    }

    void SetWorkspaceDir(const fs::path& dir) {
        g_BaseDir = (dir.filename() != L"localcel_workspace") ? dir / L"localcel_workspace" : dir;
        fs::create_directories(g_BaseDir);
        
        fs::path localcelDir = GetUserProfile() / L".localcel";
        fs::path configPath = localcelDir / L"config.json";
        std::wofstream out(configPath);
        out << L"{\"workspace\": \"" << g_BaseDir.wstring() << L"\"}";
        EnsureDirectories();
    }

    void LoadApps() {
        g_Apps.clear(); EnsureDirectories();
        for (const auto& entry : fs::directory_iterator(g_AppsDir)) {
            if (entry.is_directory() && fs::exists(entry.path() / L"config.json")) {
                std::wifstream in(entry.path() / L"config.json"); std::wstringstream ss; ss << in.rdbuf(); std::wstring json = ss.str();
                AppConfig app;
                app.name = Utils::ExtractJsonWString(json, L"name"); app.port = Utils::ExtractJsonInt(json, L"port");
                app.entry = Utils::ExtractJsonWString(json, L"entry"); app.domain = Utils::ExtractJsonWString(json, L"domain");
                app.app_type = Utils::ExtractJsonWString(json, L"app_type"); app.github_repo = Utils::ExtractJsonWString(json, L"github_repo"); app.gh_pages_deployed = Utils::ExtractJsonBool(json, L"gh_pages_deployed");
                if (app.app_type.empty()) app.app_type = L"node";
                g_Apps.push_back(app);
            }
        }
    }

    std::wstring SerializeJson(const AppConfig& app) {
        std::wstringstream ss; ss << L"{\n  \"name\": \"" << app.name << L"\",\n  \"port\": " << app.port << L",\n  \"entry\": \"" << app.entry << L"\",\n";
        if (!app.domain.empty()) ss << L"  \"domain\": \"" << app.domain << L"\",\n";
        ss << L"  \"app_type\": \"" << app.app_type << L"\",\n  \"github_repo\": \"" << app.github_repo << L"\",\n  \"gh_pages_deployed\": " << (app.gh_pages_deployed ? L"true" : L"false") << L"\n}";
        return ss.str();
    }

    void SaveAppConfig(const AppConfig& app) {
        fs::create_directories(g_AppsDir / app.name);
        std::wofstream out(g_AppsDir / app.name / L"config.json");
        out << SerializeJson(app);
    }
}
