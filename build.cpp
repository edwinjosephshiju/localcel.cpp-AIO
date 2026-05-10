#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

int main() {
    std::cout << "Localcel Universal Build Script" << std::endl;
    std::cout << "Target: Windows (MSVC)" << std::endl;

    // 1. Find resource file
    std::string resCmd = "rc ../resource.rc";
    std::cout << "Compiling resources: " << resCmd << std::endl;
    if (std::system(resCmd.c_str()) != 0) {
        std::cerr << "Failed to compile resources. Make sure 'rc.exe' is in your PATH." << std::endl;
        return 1;
    }

    // 2. Collect all .cpp files
    std::vector<std::string> sources = {
        "src/main.cpp",
        "src/ui/WindowsUI.cpp",
        "src/core/AppManager.cpp",
        "src/core/ProcessManager.cpp",
        "src/core/Utils.cpp",
        "src/network/CloudflareManager.cpp",
        "src/deploy/GitManager.cpp",
        "src/deps/DepMgr_Windows.cpp"
    };

    std::string clCmd = "cl ";
    for (const auto& src : sources) {
        clCmd += src + " ";
    }
    clCmd += "../resource.res ";
    clCmd += "/std:c++20 /EHsc /O2 /I\"src\" /Fe:Localcel.exe ";
    clCmd += "user32.lib kernel32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib ws2_32.lib shlwapi.lib dwmapi.lib uxtheme.lib advapi32.lib gdiplus.lib";

    std::cout << "Compiling: " << clCmd << std::endl;
    int result = std::system(clCmd.c_str());
    
    if (result == 0) {
        std::cout << "Build successful: Localcel.exe" << std::endl;
    } else {
        std::cerr << "Build failed. Error code: " << result << std::endl;
        std::cerr << "Make sure you are running this from a 'Developer Command Prompt for VS'." << std::endl;
    }

    return result;
}
