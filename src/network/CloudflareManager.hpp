#pragma once
#include <string>
#include <vector>

struct TunnelInfo {
    std::wstring id;
    std::wstring name;
};

namespace CloudflareManager {
    std::vector<TunnelInfo> GetActiveTunnels();
    void DeleteTunnel(const std::wstring& id);
    void Login();
}
