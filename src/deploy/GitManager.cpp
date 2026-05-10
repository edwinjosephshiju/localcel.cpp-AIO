#include "GitManager.hpp"
#include "../deps/DependencyManager.hpp"
#include "../core/Utils.hpp"
#include "../core/AppManager.hpp"
#include <regex>
#include <thread>
#include <atomic>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace GitManager {

    std::wstring g_GHUserCache = L""; 
    std::atomic<bool> g_FetchingGHUser{false};

    std::wstring GetGHUser() {
        if (!g_GHUserCache.empty()) return g_GHUserCache;
        std::wstring gh = DependencyManager::ResolveExecutable(L"gh.exe"); if (gh.empty()) return L"";
        std::string output = Utils::ExecSyncAndRead(L"\"" + gh + L"\" auth status");
        std::regex rx("Logged in to github\\.com account ([a-zA-Z0-9-]+)"); std::smatch match;
        if (std::regex_search(output, match, rx)) { g_GHUserCache = Utils::UTF8ToWide(match[1]); return g_GHUserCache; }
        return L"";
    }

    void Login() {
        std::wstring gh = DependencyManager::ResolveExecutable(L"gh.exe"); if (gh.empty()) return;
#ifdef _WIN32
        STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = { 0 }; 
        std::wstring cmd = L"\"" + gh + L"\" auth login --web --scopes repo,delete_repo";
        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end()); cmdBuf.push_back(0); 
        CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
#endif
    }

    void Logout() {
        std::wstring gh = DependencyManager::ResolveExecutable(L"gh.exe"); if (gh.empty()) return;
        Utils::ExecHiddenSync(L"\"" + gh + L"\" auth logout --hostname github.com", AppManager::g_BaseDir); 
        g_GHUserCache = L"";
    }

    void DeployApp(const std::wstring& appName, bool currentlyDeployed, std::function<void(const std::wstring&)> logCb, std::function<void()> onDone) {
        std::wstring git = DependencyManager::ResolveExecutable(L"git.exe"); 
        std::wstring gh = DependencyManager::ResolveExecutable(L"gh.exe");

        std::thread([appName, currentlyDeployed, git, gh, logCb, onDone]() {
            fs::path cwd = AppManager::g_AppsDir / appName;
            auto log = [&](const std::wstring& msg) { if(logCb) logCb(appName + L"|tunnel|[GIT] " + msg + L"\r\n"); };
            
            AppConfig app;
            for (auto& a : AppManager::g_Apps) { if(a.name == appName) { app = a; break; } }

            if (currentlyDeployed) {
                log(L"--- Disabling GitHub Pages ---"); 
                std::string repoJson = Utils::ExecSyncAndRead(L"\"" + gh + L"\" repo view --json nameWithOwner -q .nameWithOwner");
                std::wstring repoName = Utils::TrimRight(Utils::UTF8ToWide(repoJson));
                if (!repoName.empty()) { 
                    Utils::ExecHiddenSync(L"\"" + gh + L"\" api -X DELETE /repos/" + repoName + L"/pages", cwd); 
                    log(L"GitHub Pages disabled (Repository intact)."); 
                }
                app.gh_pages_deployed = false;
            } else {
                log(L"--- Starting GitHub Deploy ---"); 
                Utils::ExecHiddenSync(L"\"" + git + L"\" config user.name Localcel", cwd); 
                Utils::ExecHiddenSync(L"\"" + git + L"\" config user.email deploy@localcel", cwd);
                Utils::ExecHiddenSync(L"\"" + git + L"\" add .", cwd); 
                Utils::ExecHiddenSync(L"\"" + git + L"\" commit -m \"Deploy\"", cwd);
                
                std::string remotes = Utils::ExecSyncAndRead(L"\"" + git + L"\" remote -v");
                if (remotes.find("origin") == std::string::npos) { 
                    log(L"Creating public GitHub repository..."); 
                    Utils::ExecHiddenSync(L"\"" + gh + L"\" repo create " + appName + L" --public --source=. --push", cwd);
                } else { 
                    log(L"Pushing to GitHub..."); 
                    Utils::ExecHiddenSync(L"\"" + git + L"\" push -u origin main", cwd); 
                }
                
                log(L"Enabling Pages..."); 
                std::string repoJson = Utils::ExecSyncAndRead(L"\"" + gh + L"\" repo view --json nameWithOwner -q .nameWithOwner");
                std::wstring repoName = Utils::TrimRight(Utils::UTF8ToWide(repoJson));
                if (!repoName.empty()) 
                    Utils::ExecHiddenSync(L"\"" + gh + L"\" api -X POST /repos/" + repoName + L"/pages -f source[branch]=main -f source[path]=/", cwd);
                
                app.gh_pages_deployed = true; 
                log(L"Deployed successfully!");
            }
            AppManager::SaveAppConfig(app); 
            if(onDone) onDone();
        }).detach();
    }

    void DeleteRepo(const std::wstring& appName) {
        std::wstring gh = DependencyManager::ResolveExecutable(L"gh.exe");
        if (!gh.empty()) {
            std::wstring repoName = Utils::TrimRight(Utils::UTF8ToWide(Utils::ExecSyncAndRead(L"\"" + gh + L"\" repo view --json nameWithOwner -q .nameWithOwner")));
            if (repoName.empty()) repoName = appName; 
            Utils::ExecHiddenSync(L"\"" + gh + L"\" repo delete " + repoName + L" --yes", AppManager::g_AppsDir / appName);
        }
    }
}
