#include "CloudflareManager.hpp"
#include "../deps/DependencyManager.hpp"
#include "../core/Utils.hpp"
#include "../core/AppManager.hpp"
#include <regex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace CloudflareManager {

    std::vector<TunnelInfo> GetActiveTunnels() {
        std::vector<TunnelInfo> tunnels;
        std::wstring cfExe = DependencyManager::ResolveExecutable(L"cloudflared.exe");
        if (cfExe.empty()) return tunnels;

        std::string out = Utils::ExecSyncAndRead(L"\"" + cfExe + L"\" tunnel list");
        std::regex rx("([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\\s+([^\\s]+)");
        std::sregex_iterator next(out.begin(), out.end(), rx), end;
        while (next != end) {
            std::smatch match = *next; 
            TunnelInfo ti = { Utils::UTF8ToWide(match[1].str()), Utils::UTF8ToWide(match[2].str()) };
            tunnels.push_back(ti);
            next++;
        }
        return tunnels;
    }

    void DeleteTunnel(const std::wstring& id) {
        std::wstring cfExe = DependencyManager::ResolveExecutable(L"cloudflared.exe");
        if (cfExe.empty()) return;
        Utils::ExecHiddenSync(L"\"" + cfExe + L"\" tunnel delete -f " + id, AppManager::g_BaseDir);
    }

    void Login() {
        std::wstring cfExe = DependencyManager::ResolveExecutable(L"cloudflared.exe");
        if (cfExe.empty()) return;

#ifdef _WIN32
        STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = { 0 }; 
        std::wstring cmd = L"\"" + cfExe + L"\" tunnel login";
        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end()); cmdBuf.push_back(0); 
        CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
#endif
    }
}
