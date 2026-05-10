#pragma once
#include <string>
#include <map>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

class ManagedProcess {
public:
    std::wstring name;
    bool is_tunnel;
#ifdef _WIN32
    HANDLE hProcess = NULL;
    HANDLE hJob = NULL;
    DWORD pid = 0;
#endif
    std::wstring url;
    std::atomic<bool> running{ false };
    std::thread readerThread;

    using LogCallback = std::function<void(const std::wstring&)>;
    LogCallback onLog;

    ManagedProcess(const std::wstring& n, bool tunnel, LogCallback logCb = nullptr);
    ~ManagedProcess();

    void Start(const std::wstring& cmdline, const std::wstring& cwd);
    void Stop();
    bool IsRunning() const;
};

namespace ProcessManager {
    extern std::map<std::wstring, std::unique_ptr<ManagedProcess>> g_Servers;
    extern std::map<std::wstring, std::unique_ptr<ManagedProcess>> g_Tunnels;

    void StopAll();
    bool IsPortInUse(int port);
}
