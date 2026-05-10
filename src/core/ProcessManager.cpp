#include "ProcessManager.hpp"
#include "AppManager.hpp"
#include "Utils.hpp"
#include <fstream>
#include <regex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace ProcessManager {
    std::map<std::wstring, std::unique_ptr<ManagedProcess>> g_Servers;
    std::map<std::wstring, std::unique_ptr<ManagedProcess>> g_Tunnels;

    void StopAll() {
        for (auto& p : g_Servers) p.second->Stop();
        for (auto& p : g_Tunnels) p.second->Stop();
    }

    bool IsPortInUse(int port) {
#ifdef _WIN32
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); if (s == INVALID_SOCKET) return false;
        sockaddr_in addr = { 0 }; addr.sin_family = AF_INET; addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr); int res = connect(s, (sockaddr*)&addr, sizeof(addr));
        closesocket(s); return res == 0;
#else
        return false;
#endif
    }
}

ManagedProcess::ManagedProcess(const std::wstring& n, bool tunnel, LogCallback logCb) : name(n), is_tunnel(tunnel), onLog(logCb) {}

ManagedProcess::~ManagedProcess() { Stop(); }

void ManagedProcess::Start(const std::wstring& cmdline, const std::wstring& cwd) {
    if (running) return;
#ifdef _WIN32
    HANDLE hPipeRead, hPipeWrite; SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    if (!CreatePipe(&hPipeRead, &hPipeWrite, &sa, 0)) return; SetHandleInformation(hPipeRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) }; si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    si.hStdOutput = hPipeWrite; si.hStdError = hPipeWrite; PROCESS_INFORMATION pi = { 0 };

    std::vector<wchar_t> cmdBuf(cmdline.begin(), cmdline.end()); cmdBuf.push_back(L'\0');

    hJob = CreateJobObjectW(NULL, NULL);
    if (hJob) { JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 }; jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE; SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli)); }

    if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP, NULL, cwd.c_str(), &si, &pi)) {
        if (hJob) AssignProcessToJobObject(hJob, pi.hProcess);
        ResumeThread(pi.hThread); hProcess = pi.hProcess; pid = pi.dwProcessId; running = true; CloseHandle(pi.hThread);

        std::wstring typePrefix = is_tunnel ? L"tunnel|" : L"server|";
        std::wstring startMsg = name + L"|" + typePrefix + L"[Localcel] Process started (PID: " + std::to_wstring(pid) + L")\r\n";
        
        if (onLog) onLog(startMsg);

        fs::path logFile = AppManager::g_LogsDir / (name + (is_tunnel ? L"_tunnel.log" : L".log"));
        std::wofstream ofsStart(logFile, std::ios::app);
        ofsStart << L"[Localcel] Process started (PID: " << pid << L")\n";
        ofsStart.flush();

        readerThread = std::thread([this, hPipeRead, logFile]() {
            char buf[1024]; DWORD read; 
            std::wregex urlRegex(L"https://[a-zA-Z0-9-]+\\.trycloudflare\\.com");
            while (ReadFile(hPipeRead, buf, sizeof(buf) - 1, &read, NULL) && read > 0) {
                buf[read] = '\0'; std::wstring wline = Utils::UTF8ToWide(buf);
                std::wofstream ofs(logFile, std::ios::app); ofs << wline; ofs.flush();
                if (is_tunnel && url.empty()) { std::wsmatch match; if (std::regex_search(wline, match, urlRegex)) url = match.str(); }
                if (onLog) onLog(name + L"|" + (is_tunnel ? L"tunnel|" : L"server|") + wline);
            }
            CloseHandle(hPipeRead); running = false;
            
            std::wstring stopMsg = name + L"|" + (is_tunnel ? L"tunnel|" : L"server|") + L"[Localcel] Process terminated.\r\n";
            if (onLog) onLog(stopMsg);
        });
    }
    CloseHandle(hPipeWrite);
#endif
}

void ManagedProcess::Stop() {
#ifdef _WIN32
    if (hJob) { CloseHandle(hJob); hJob = NULL; } else if (hProcess) TerminateProcess(hProcess, 0);
    if (hProcess) { WaitForSingleObject(hProcess, 3000); CloseHandle(hProcess); hProcess = NULL; }
#endif
    running = false; if (readerThread.joinable()) readerThread.join(); url = L"";
}

bool ManagedProcess::IsRunning() const { return running; }
