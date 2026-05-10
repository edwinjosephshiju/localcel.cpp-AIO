#pragma once
#include <string>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace Utils {
    std::wstring UTF8ToWide(const std::string& utf8);
    std::string WideToUTF8(const std::wstring& wide);
    std::wstring TrimRight(std::wstring s);
    void ForceRemoveAll(const fs::path& p);
    
    std::string ExecSyncAndRead(const std::wstring& cmd);
    void ExecHiddenSync(const std::wstring& cmd, const fs::path& cwd);
    
    std::wstring ExtractJsonWString(const std::wstring& json, const std::wstring& key);
    int ExtractJsonInt(const std::wstring& json, const std::wstring& key);
    bool ExtractJsonBool(const std::wstring& json, const std::wstring& key);
}
