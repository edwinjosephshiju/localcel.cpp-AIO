#include "Utils.hpp"
#include <windows.h>
#include <regex>

namespace Utils {
    std::wstring UTF8ToWide(const std::string& utf8) {
        if (utf8.empty()) return std::wstring();
        int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        std::wstring wide(size, 0); MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], size);
        wide.resize(wcslen(wide.c_str())); return wide;
    }

    std::string WideToUTF8(const std::wstring& wide) {
        if (wide.empty()) return std::string();
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(size, 0); WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], size, nullptr, nullptr);
        utf8.resize(strlen(utf8.c_str())); return utf8;
    }

    std::wstring TrimRight(std::wstring s) { s.erase(s.find_last_not_of(L" \n\r\t") + 1); return s; }

    void ForceRemoveAll(const fs::path& p) {
        if (!fs::exists(p)) return;
        try { for (auto& entry : fs::recursive_directory_iterator(p)) SetFileAttributesW(entry.path().c_str(), FILE_ATTRIBUTE_NORMAL); fs::remove_all(p); } catch (...) {}
    }

    std::string ExecSyncAndRead(const std::wstring& cmd) {
#ifdef _WIN32
        HANDLE hPipeRead, hPipeWrite; SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        if (!CreatePipe(&hPipeRead, &hPipeWrite, &sa, 0)) return "";
        SetHandleInformation(hPipeRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si = { sizeof(si) }; si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
        si.hStdOutput = hPipeWrite; si.hStdError = hPipeWrite; PROCESS_INFORMATION pi = { 0 };
        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end()); cmdBuf.push_back(L'\0'); std::string output;

        if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(hPipeWrite); char buf[1024]; DWORD read;
            while (ReadFile(hPipeRead, buf, sizeof(buf) - 1, &read, NULL) && read > 0) { buf[read] = '\0'; output += buf; }
            CloseHandle(hPipeRead); WaitForSingleObject(pi.hProcess, INFINITE); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        } else { CloseHandle(hPipeWrite); CloseHandle(hPipeRead); }
        return output;
#else
        return "";
#endif
    }

    void ExecHiddenSync(const std::wstring& cmd, const fs::path& cwd) {
#ifdef _WIN32
        STARTUPINFOW si = { sizeof(si) }; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; PROCESS_INFORMATION pi = { 0 };
        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end()); cmdBuf.push_back(L'\0');
        if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, cwd.c_str(), &si, &pi)) { WaitForSingleObject(pi.hProcess, INFINITE); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
#endif
    }

    std::wstring ExtractJsonWString(const std::wstring& json, const std::wstring& key) {
        std::wregex rx(L"\"" + key + L"\"\\s*:\\s*\"([^\"]*)\""); std::wsmatch match;
        if (std::regex_search(json, match, rx)) return match[1]; return L"";
    }

    int ExtractJsonInt(const std::wstring& json, const std::wstring& key) {
        std::wregex rx(L"\"" + key + L"\"\\s*:\\s*(\\d+)"); std::wsmatch match;
        if (std::regex_search(json, match, rx)) return std::stoi(match[1]); return 0;
    }

    bool ExtractJsonBool(const std::wstring& json, const std::wstring& key) {
        std::wregex rx(L"\"" + key + L"\"\\s*:\\s*(true|false)"); std::wsmatch match;
        if (std::regex_search(json, match, rx)) return match[1] == L"true"; return false;
    }
}
