// localcel_native.cpp - Pure Win32 API Implementation of Localcel (Modern Theme for MSVC)
// Compile with MSVC: 
// rc resource.rc
// cl localcel_native.cpp resource.res /std:c++20 /EHsc /O2 /Fe:Localcel.exe

#define _CRT_SECURE_NO_WARNINGS

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 // Target Windows 10/11
#endif

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "WindowsUI.hpp"
#ifdef _WIN32
#include "../core/AppManager.hpp"
#include "../core/ProcessManager.hpp"
#include "../core/Utils.hpp"
#include "../network/CloudflareManager.hpp"
#include "../deploy/GitManager.hpp"
#include "../deps/DependencyManager.hpp"

#include <objidl.h>  
#include <gdiplus.h> 
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <shobjidl.h>

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <fstream>
#include <sstream>
#include <regex>
#include <atomic>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// --- Embedded Resource IDs ---
#define IDI_APP_ICON 101
#define IDI_APP_LOGO 102 

// --- MSVC Automatic Linker Instructions ---
#pragma comment(linker, "/SUBSYSTEM:WINDOWS") 
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdiplus.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#define WM_APP_LOG              (WM_APP + 1)
#define WM_APP_TRAY             (WM_APP + 2)
#define WM_APP_DEPLOY_DONE      (WM_APP + 3)
#define WM_APP_UPDATE_AVAILABLE (WM_APP + 4)

#define ID_LISTBOX      101
#define ID_BTN_NEW      102
#define ID_BTN_CFLOGIN  103
#define ID_BTN_GITLOGIN 104
#define ID_BTN_TUNNEL   105
#define ID_BTN_START    106
#define ID_BTN_STOP     107
#define ID_BTN_DEPLOY   108
#define ID_BTN_EDIT     109
#define ID_BTN_DEL      110
#define ID_BTN_TAB_SVR  111
#define ID_BTN_TAB_TUN  112
#define ID_EDIT_SERVER  113
#define ID_EDIT_TUNNEL  114
#define ID_STAT_URL     115
#define ID_TRAY_SHOW    1001
#define ID_TRAY_EXIT    1002


// --- Globals ---

bool EnsureDependencyUI(const std::wstring& name, const std::wstring& exeName, const std::wstring& packageId);
extern HWND g_hWndMain;
int ShowModernAlert(HWND hwnd, const std::wstring& title, const std::wstring& text, int btns = 0, const wchar_t* icon = nullptr);

bool EnsureDependencyUI(const std::wstring& name, const std::wstring& exeName, const std::wstring& packageId) {
    return DependencyManager::EnsureDependency(name, exeName, packageId, [&](const std::wstring& msg) {
        return ShowModernAlert(g_hWndMain, L"Dependency Missing", msg, 0x0002 | 0x0008, L"td_info_icon") == 6; // IDYES
    });
}
HWND g_hWndMain = NULL;
HWND g_hListBox, g_hBtnNew, g_hBtnCFLogin, g_hBtnGitLogin, g_hBtnTunnel;
HWND g_hStatLogo, g_hStatAppTitle, g_hStatURL;
HWND g_hBtnStart, g_hBtnStop, g_hBtnDeploy, g_hBtnEdit, g_hBtnDel;
HWND g_hBtnTabServer, g_hBtnTabTunnel, g_hEditServer, g_hEditTunnel;
HWND g_hSepMain, g_hSepVert, g_hSepSide;
HFONT g_hFontNormal, g_hFontTitle, g_hFontAppTitle, g_hFontConsolas;
NOTIFYICONDATAW g_nid = { 0 };
 int g_SelectedAppIdx = -1;
Gdiplus::Image* g_pLogoImage = nullptr; 
IStream* g_pLogoStream = nullptr; 
HICON g_hIconOriginal = NULL;
HICON g_hIconCurrent = NULL;

void RefreshAppList(); void UpdateUIState(); void OnStop(); void SaveAppConfig(const AppConfig& app);


Gdiplus::Image* LoadImageFromResource(int resId) {
    HRSRC hRes = FindResourceW(GetModuleHandle(NULL), MAKEINTRESOURCEW(resId), (LPCWSTR)RT_RCDATA);
    if (!hRes) return nullptr;
    DWORD size = SizeofResource(GetModuleHandle(NULL), hRes);
    HGLOBAL hResLoad = LoadResource(GetModuleHandle(NULL), hRes);
    void* pData = LockResource(hResLoad);

    HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, size);
    void* pBuffer = GlobalLock(hBuffer);
    memcpy(pBuffer, pData, size);
    GlobalUnlock(hBuffer);

    IStream* pStream = nullptr;
    if (CreateStreamOnHGlobal(hBuffer, TRUE, &pStream) == S_OK) {
        Gdiplus::Image* img = Gdiplus::Image::FromStream(pStream);
        if (g_pLogoStream) g_pLogoStream->Release();
        g_pLogoStream = pStream; 
        return img;
    }
    GlobalFree(hBuffer);
    return nullptr;
}

// --- Theme Architecture Engine ---
using fnSetPreferredAppMode = int (WINAPI*)(int);
using fnAllowDarkModeForWindow = bool (WINAPI*)(HWND, bool);
using fnFlushMenuThemes = void (WINAPI*)();

LRESULT CALLBACK ButtonSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

struct ThemeEngine {
    bool IsDark = false;
    HBRUSH hbrBackground = NULL;
    HBRUSH hbrControlBg = NULL;
    COLORREF colBackground = RGB(255, 255, 255);
    COLORREF colControlBg = RGB(255, 255, 255);
    COLORREF colText = RGB(0,0,0);
    COLORREF colLink = RGB(0, 102, 204);

    fnSetPreferredAppMode pSetPreferredAppMode = nullptr;
    fnAllowDarkModeForWindow pAllowDarkModeForWindow = nullptr;
    fnFlushMenuThemes pFlushMenuThemes = nullptr;

    ThemeEngine() {} 
    ~ThemeEngine() { Cleanup(); }

    void Init() {
        HMODULE hUxTheme = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (hUxTheme) {
            pSetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135));
            pAllowDarkModeForWindow = (fnAllowDarkModeForWindow)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(133));
            pFlushMenuThemes = (fnFlushMenuThemes)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136));
        }
        Update();
    }

    void Cleanup() {
        if (hbrBackground) DeleteObject(hbrBackground);
        if (hbrControlBg) DeleteObject(hbrControlBg);
    }

    HICON CreateInvertedIcon(HICON hOriginal) {
        Gdiplus::Bitmap* bmp = Gdiplus::Bitmap::FromHICON(hOriginal);
        if (!bmp || bmp->GetLastStatus() != Gdiplus::Ok) return hOriginal;

        UINT w = bmp->GetWidth(), h = bmp->GetHeight();
        Gdiplus::Bitmap newBmp(w, h, PixelFormat32bppARGB);
        Gdiplus::Graphics g(&newBmp);
        g.Clear(Gdiplus::Color::Transparent);

        Gdiplus::ColorMatrix cm = {
            -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
             0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
             0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
             0.0f,  0.0f,  0.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f,  0.0f, 1.0f
        };
        Gdiplus::ImageAttributes attr;
        attr.SetColorMatrix(&cm);
        g.DrawImage(bmp, Gdiplus::Rect(0, 0, w, h), 0, 0, w, h, Gdiplus::UnitPixel, &attr);

        HICON hNewIcon = NULL;
        newBmp.GetHICON(&hNewIcon);
        delete bmp;
        return hNewIcon ? hNewIcon : hOriginal;
    }

    void Update() {
        DWORD useLight = 1; DWORD size = sizeof(useLight);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &useLight, &size);
        IsDark = (useLight == 0);

        if (pSetPreferredAppMode) {
            pSetPreferredAppMode(IsDark ? 2 : 1); 
            if (pFlushMenuThemes) pFlushMenuThemes();
        }

        Cleanup();
        if (IsDark) {
            colBackground = RGB(32, 32, 32);
            hbrBackground = CreateSolidBrush(colBackground);      
            hbrControlBg = CreateSolidBrush(RGB(40, 40, 40)); 
            colControlBg = RGB(40, 40, 40);      
            colText = RGB(255, 255, 255);
            colLink = RGB(96, 172, 255);                            
        } else {
            colBackground = RGB(243, 243, 243);
            hbrBackground = CreateSolidBrush(colBackground);
            hbrControlBg = CreateSolidBrush(RGB(255, 255, 255));
            colControlBg = RGB(255, 255, 255);
            colText = RGB(10, 10, 10); 
            colLink = RGB(0, 102, 204);
        }

        if (g_hIconOriginal) {
            if (g_hIconCurrent && g_hIconCurrent != g_hIconOriginal) DestroyIcon(g_hIconCurrent);
            g_hIconCurrent = IsDark ? CreateInvertedIcon(g_hIconOriginal) : g_hIconOriginal;
            
            if (g_hWndMain) {
                SendMessage(g_hWndMain, WM_SETICON, ICON_BIG, (LPARAM)g_hIconCurrent);
                SendMessage(g_hWndMain, WM_SETICON, ICON_SMALL, (LPARAM)g_hIconCurrent);
                g_nid.hIcon = g_hIconCurrent;
                Shell_NotifyIconW(NIM_MODIFY, &g_nid);
            }
        }
    }

    void ApplyToWindow(HWND hWnd) {
        BOOL isDark = IsDark ? TRUE : FALSE;
        DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &isDark, sizeof(isDark)); 
        DWORD cornerPref = DWMWCP_ROUND; 
        DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
    }

    void ApplyToControls(const std::vector<HWND>& ctrls) {
        LPCWSTR theme = IsDark ? L"DarkMode_Explorer" : L"Explorer";
        LPCWSTR themeCFD = IsDark ? L"DarkMode_CFD" : L"Explorer"; 
        
        for (HWND h : ctrls) {
            if (h) {
                if (pAllowDarkModeForWindow) pAllowDarkModeForWindow(h, IsDark);

                wchar_t clsName[64]; GetClassNameW(h, clsName, 64);
                if (_wcsicmp(clsName, L"Button") == 0) {
                    SetWindowSubclass(h, ButtonSubclassProc, 4, 0);
                    SetWindowTheme(h, theme, NULL);
                } else if (_wcsicmp(clsName, L"ComboBox") == 0) {
                    SetWindowTheme(h, themeCFD, NULL); 
                } else if (_wcsicmp(clsName, L"ListBox") == 0 || _wcsicmp(clsName, L"Edit") == 0) {
                    SetWindowTheme(h, theme, NULL); 
                } else {
                    SetWindowTheme(h, theme, NULL);
                }
                InvalidateRect(h, NULL, TRUE);
            }
        }
    }
} g_Theme;

// --- TaskDialog Hook for Native Theming ---
HRESULT CALLBACK TaskDialogCallbackProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData) {
    if (msg == TDN_CREATED) {
        g_Theme.ApplyToWindow(hwnd);
    }
    return S_OK;
}

// --- Modern TaskDialog Alert System ---
int ShowModernAlert(HWND hwnd, const std::wstring& title, const std::wstring& text, TASKDIALOG_COMMON_BUTTON_FLAGS btns, PCWSTR icon) {
    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    tdc.hwndParent = hwnd;
    tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT;
    tdc.pszWindowTitle = title.c_str();
    tdc.pszMainInstruction = title.c_str();
    tdc.pszContent = text.c_str();
    tdc.dwCommonButtons = btns;
    tdc.pszMainIcon = icon;
    tdc.pfCallback = TaskDialogCallbackProc; 
    int btn = 0;
    TaskDialogIndirect(&tdc, &btn, NULL, NULL);
    return btn;
}

LRESULT CALLBACK LogoSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps); RECT rc; GetClientRect(hWnd, &rc);
        
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

        FillRect(hdcMem, &rc, g_Theme.hbrBackground);
        
        if (g_pLogoImage) {
            Gdiplus::Graphics g(hdcMem);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            
            UINT imgW = g_pLogoImage->GetWidth(), imgH = g_pLogoImage->GetHeight();
            int drawW = rc.right - rc.left, drawH = (imgH * drawW) / imgW;
            if (drawH > (rc.bottom - rc.top)) { drawH = rc.bottom - rc.top; drawW = (imgW * drawH) / imgH; }
            int yOff = ((rc.bottom - rc.top) - drawH) / 2;
            int xOff = ((rc.right - rc.left) - drawW) / 2;
            
            Gdiplus::ImageAttributes imgAttr;
            if (!g_Theme.IsDark) {
                Gdiplus::ColorMatrix colorMatrix = {
                    -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
                     0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
                     0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
                     0.0f,  0.0f,  0.0f,  1.0f, 0.0f,
                     1.0f,  1.0f,  1.0f,  0.0f, 1.0f
                };
                imgAttr.SetColorMatrix(&colorMatrix);
            }
            Gdiplus::Rect destRect(xOff, yOff, drawW, drawH);
            g.DrawImage(g_pLogoImage, destRect, 0, 0, imgW, imgH, Gdiplus::UnitPixel, &imgAttr);
        }
        
        BitBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hbmOld); DeleteObject(hbmMem); DeleteDC(hdcMem);

        EndPaint(hWnd, &ps); return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_ERASEBKGND) return 1; 
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK ButtonSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch(msg) {
        case WM_MOUSEMOVE: {
            if (!GetPropW(hWnd, L"Hovered")) {
                TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
                TrackMouseEvent(&tme);
                SetPropW(hWnd, L"Hovered", (HANDLE)1);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }
        case WM_MOUSELEAVE: { RemovePropW(hWnd, L"Hovered"); InvalidateRect(hWnd, NULL, FALSE); break; }
        case WM_DESTROY: { RemovePropW(hWnd, L"Hovered"); break; }
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

LRESULT DrawOwnerDrawButton(LPARAM lParam) {
    LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
    bool isHovered = GetPropW(pdis->hwndItem, L"Hovered") != NULL;
    bool isPressed = (pdis->itemState & ODS_SELECTED);
    bool isDisabled = (pdis->itemState & ODS_DISABLED);
    bool isActiveTab = GetPropW(pdis->hwndItem, L"ActiveTab") != NULL;

    COLORREF bgCol, borderCol, textCol;
    if (g_Theme.IsDark) {
        if (isDisabled) { bgCol = RGB(45,45,45); borderCol = RGB(60,60,60); textCol = RGB(130,130,130); }
        else if (isPressed || isActiveTab) { bgCol = RGB(75,75,75); borderCol = RGB(105,105,105); textCol = RGB(255,255,255); }
        else if (isHovered) { bgCol = RGB(65,65,65); borderCol = RGB(95,95,95); textCol = RGB(255,255,255); }
        else { bgCol = RGB(50,50,50); borderCol = RGB(80,80,80); textCol = RGB(255,255,255); }
    } else {
        if (isDisabled) { bgCol = RGB(240,240,240); borderCol = RGB(210,210,210); textCol = RGB(160,160,160); }
        else if (isPressed || isActiveTab) { bgCol = RGB(204,228,247); borderCol = RGB(0,84,153); textCol = RGB(10,10,10); }
        else if (isHovered) { bgCol = RGB(229,241,251); borderCol = RGB(0,120,215); textCol = RGB(10,10,10); }
        else { bgCol = RGB(251,251,251); borderCol = RGB(190,190,190); textCol = RGB(10,10,10); }
    }

    RECT rc = pdis->rcItem;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;

    HDC hdcMem = CreateCompatibleDC(pdis->hDC);
    HBITMAP hbmMem = CreateCompatibleBitmap(pdis->hDC, w, h);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

    RECT rcMem = {0, 0, w, h};
    FillRect(hdcMem, &rcMem, g_Theme.hbrBackground);

    HBRUSH hBg = CreateSolidBrush(bgCol);
    HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
    HGDIOBJ hOldBrush = SelectObject(hdcMem, hBg);
    HGDIOBJ hOldPen = SelectObject(hdcMem, hPen);

    RoundRect(hdcMem, rcMem.left, rcMem.top, rcMem.right, rcMem.bottom, 8, 8);

    SelectObject(hdcMem, hOldBrush); SelectObject(hdcMem, hOldPen);
    DeleteObject(hBg); DeleteObject(hPen);

    wchar_t text[256]; GetWindowTextW(pdis->hwndItem, text, 256);
    SetBkMode(hdcMem, TRANSPARENT); SetTextColor(hdcMem, textCol);
    HFONT hFont = (HFONT)SendMessage(pdis->hwndItem, WM_GETFONT, 0, 0);
    HGDIOBJ hOldFont = SelectObject(hdcMem, hFont);
    DrawTextW(hdcMem, text, -1, &rcMem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdcMem, hOldFont);

    BitBlt(pdis->hDC, rc.left, rc.top, w, h, hdcMem, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hbmOld); DeleteObject(hbmMem); DeleteDC(hdcMem);
    return TRUE;
}

// --- Windows 11 Native GDI+ ListBox Selection Pill ---
LRESULT DrawOwnerDrawListBox(LPARAM lParam) {
    LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
    if (pdis->itemID == -1) return TRUE;

    HDC hdc = pdis->hDC; RECT rc = pdis->rcItem;
    bool isSelected = (pdis->itemState & ODS_SELECTED);

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);
    RECT rcMem = {0, 0, rc.right - rc.left, rc.bottom - rc.top};

    HBRUSH hBg = CreateSolidBrush(g_Theme.colBackground);
    FillRect(hdcMem, &rcMem, hBg);
    DeleteObject(hBg);

    Gdiplus::Graphics g(hdcMem);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    if (isSelected) {
        Gdiplus::SolidBrush selBrush(g_Theme.IsDark ? Gdiplus::Color(255, 75, 75, 75) : Gdiplus::Color(255, 232, 240, 250));
        Gdiplus::GraphicsPath path;
        int r = 5, d = r * 2;
        int x = rcMem.left + 4, y = rcMem.top + 2, w = rcMem.right - rcMem.left - 8, h = rcMem.bottom - rcMem.top - 4;
        path.AddArc(x + w - d, y, d, d, 270, 90);
        path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
        path.AddArc(x, y + h - d, d, d, 90, 90);
        path.AddArc(x, y, d, d, 180, 90);
        path.CloseFigure();
        g.FillPath(&selBrush, &path);
    }

    wchar_t text[256];
    SendMessageW(pdis->hwndItem, LB_GETTEXT, pdis->itemID, (LPARAM)text);
    std::wstring sText(text);

    bool isRunning = (sText.substr(0, 3) == L"[*]");
    std::wstring dispText = sText.length() > 4 ? sText.substr(4) : L"";

    Gdiplus::SolidBrush dotBrush(isRunning ? Gdiplus::Color(255, 50, 200, 90) : (g_Theme.IsDark ? Gdiplus::Color(255, 100, 100, 100) : Gdiplus::Color(255, 180, 180, 180)));
    int dotSize = 10;
    int dotY = rcMem.top + (rcMem.bottom - rcMem.top - dotSize) / 2;
    g.FillEllipse(&dotBrush, rcMem.left + 15, dotY, dotSize, dotSize);

    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, g_Theme.colText);
    HFONT hFont = (HFONT)SendMessage(pdis->hwndItem, WM_GETFONT, 0, 0);
    HGDIOBJ oldF = SelectObject(hdcMem, hFont);
    
    RECT rcText = rcMem;
    rcText.left += 38; 
    DrawTextW(hdcMem, dispText.c_str(), -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    
    SelectObject(hdcMem, oldF);
    
    BitBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hdcMem, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hbmOld); DeleteObject(hbmMem); DeleteDC(hdcMem);

    return TRUE;
}

LRESULT CALLBACK SeparatorSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps); RECT rc; GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, g_Theme.hbrBackground);
        COLORREF lineColor = g_Theme.IsDark ? RGB(75, 75, 75) : RGB(210, 210, 210);
        HBRUSH hBrush = CreateSolidBrush(lineColor);
        if (rc.right > rc.bottom) rc.bottom = rc.top + 1; else rc.right = rc.left + 1; 
        FillRect(hdc, &rc, hBrush); DeleteObject(hBrush);
        EndPaint(hWnd, &ps); return 0;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

LRESULT CommonCtlColor(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    HDC hdc = (HDC)wParam; HWND hCtrl = (HWND)lParam;
    
    if (msg == WM_CTLCOLORSTATIC) {
        wchar_t clsName[64]; GetClassNameW(hCtrl, clsName, 64);
        if (_wcsicmp(clsName, L"Edit") == 0) { 
            SetTextColor(hdc, g_Theme.colText);
            SetBkColor(hdc, g_Theme.colControlBg);
            return (LRESULT)g_Theme.hbrControlBg;
        }
        if (hCtrl == g_hStatURL) SetTextColor(hdc, g_Theme.colLink);
        else SetTextColor(hdc, g_Theme.colText);
        SetBkMode(hdc, TRANSPARENT); return (LRESULT)g_Theme.hbrBackground;
    }
    else if (msg == WM_CTLCOLORLISTBOX || msg == WM_CTLCOLOREDIT) { 
        SetTextColor(hdc, g_Theme.colText);
        SetBkColor(hdc, g_Theme.colBackground); 
        return (LRESULT)g_Theme.hbrBackground;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// --- Utility Functions ---
POINT GetCenterCoords(HWND hParent, int childW, int childH) {
    if (!hParent) return {CW_USEDEFAULT, CW_USEDEFAULT};
    RECT rcParent; GetWindowRect(hParent, &rcParent);
    HMONITOR hMon = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) }; GetMonitorInfoW(hMon, &mi);

    int x = rcParent.left + ((rcParent.right - rcParent.left) - childW) / 2;
    int y = rcParent.top + ((rcParent.bottom - rcParent.top) - childH) / 2;

    if (x < mi.rcWork.left) x = mi.rcWork.left;
    if (y < mi.rcWork.top) y = mi.rcWork.top;
    if (x + childW > mi.rcWork.right) x = mi.rcWork.right - childW;
    if (y + childH > mi.rcWork.bottom) y = mi.rcWork.bottom - childH;
    return {x, y};
}

int g_PromptPortResult = -1; HWND g_hPortDlg = NULL;
LRESULT CALLBACK PortPromptWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hEdit; static HWND hBtnOK, hBtnCancel;
    switch(msg) {
    case WM_CREATE:
        g_Theme.ApplyToWindow(hWnd);
        CreateWindowW(WC_STATIC, L"Port is in use. Choose a new free port:", WS_CHILD|WS_VISIBLE, 15,15,250,20, hWnd, NULL, NULL, NULL);
        hEdit = CreateWindowW(WC_EDIT, L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_NUMBER, 15, 45, 100, 24, hWnd, NULL, NULL, NULL);
        SetWindowTextW(hEdit, std::to_wstring(g_PromptPortResult).c_str());
        hBtnOK = CreateWindowW(WC_BUTTON, L"OK", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 130, 42, 60, 28, hWnd, (HMENU)IDOK, NULL, NULL);
        hBtnCancel = CreateWindowW(WC_BUTTON, L"Cancel", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 200, 42, 60, 28, hWnd, (HMENU)IDCANCEL, NULL, NULL);
        g_Theme.ApplyToControls({ hEdit, hBtnOK, hBtnCancel });
        break;
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
        if (pdis->CtlType == ODT_BUTTON) return DrawOwnerDrawButton(lParam);
        break;
    }
    case WM_ERASEBKGND: { HDC hdc = (HDC)wParam; RECT rc; GetClientRect(hWnd, &rc); FillRect(hdc, &rc, g_Theme.hbrBackground); return 1; }
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: return CommonCtlColor(hWnd, msg, wParam, lParam);
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            wchar_t buf[64]; GetWindowTextW(hEdit, buf, 64); g_PromptPortResult = _wtoi(buf); EnableWindow(g_hWndMain, TRUE); DestroyWindow(hWnd);
        } else if (LOWORD(wParam) == IDCANCEL) { g_PromptPortResult = -1; EnableWindow(g_hWndMain, TRUE); DestroyWindow(hWnd); }
        break;
    case WM_CLOSE: g_PromptPortResult = -1; EnableWindow(g_hWndMain, TRUE); DestroyWindow(hWnd); break;
    default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int PromptForPort(int suggestedPort) {
    g_PromptPortResult = suggestedPort; EnableWindow(g_hWndMain, FALSE);
    int w = 300, h = 130;
    POINT pt = GetCenterCoords(g_hWndMain, w, h);
    g_hPortDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"PortPromptClass", L"Port Conflict", WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_VISIBLE, pt.x, pt.y, w, h, g_hWndMain, NULL, GetModuleHandle(NULL), NULL);
    MSG msg; while(IsWindow(g_hPortDlg) && GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return g_PromptPortResult;
}

std::vector<TunnelInfo> g_ActiveTunnels; HWND g_hTunnelDlg = NULL, g_hTunnelList = NULL;
LRESULT CALLBACK TunnelManagerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hBtnRef, hBtnDel;
    switch (msg) {
    case WM_CREATE:
        g_Theme.ApplyToWindow(hWnd);
        CreateWindowW(WC_STATIC, L"Active Cloudflare Tunnels:", WS_CHILD|WS_VISIBLE, 15,15,200,20, hWnd, NULL, NULL, NULL);
        g_hTunnelList = CreateWindowW(WC_LISTBOX, L"", WS_CHILD|WS_VISIBLE|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT, 15,40,360,200, hWnd, (HMENU)101, NULL, NULL);
        SetWindowSubclass(g_hTunnelList, ListBoxSubclassProc, 6, 0);

        hBtnRef = CreateWindowW(WC_BUTTON, L"Refresh", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 15,245,100,30, hWnd, (HMENU)102, NULL, NULL);
        hBtnDel = CreateWindowW(WC_BUTTON, L"Delete Selected", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 125,245,140,30, hWnd, (HMENU)103, NULL, NULL);
        g_Theme.ApplyToControls({ g_hTunnelList, hBtnRef, hBtnDel });
        SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(102, BN_CLICKED), 0);
        break;
    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
        if (lpmis->CtlID == 101) { lpmis->itemHeight = 36; return TRUE; }
        break;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
        if (pdis->CtlType == ODT_BUTTON) return DrawOwnerDrawButton(lParam);
        if (pdis->CtlType == ODT_LISTBOX) return DrawOwnerDrawListBox(lParam);
        break;
    }
    case WM_SIZE: {
        int w = LOWORD(lParam), h = HIWORD(lParam);
        MoveWindow(g_hTunnelList, 15, 40, w - 30, h - 90, TRUE);
        MoveWindow(hBtnRef, 15, h - 40, 100, 30, TRUE);
        MoveWindow(hBtnDel, 125, h - 40, 140, 30, TRUE);
        return 0;
    }
    case WM_GETMINMAXINFO: { MINMAXINFO* mmi = (MINMAXINFO*)lParam; mmi->ptMinTrackSize.x = 400; mmi->ptMinTrackSize.y = 350; return 0; }
    case WM_ERASEBKGND: { HDC hdc = (HDC)wParam; RECT rc; GetClientRect(hWnd, &rc); FillRect(hdc, &rc, g_Theme.hbrBackground); return 1; }
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: return CommonCtlColor(hWnd, msg, wParam, lParam);
    case WM_COMMAND:
        if (LOWORD(wParam) == 102) {
            SendMessage(g_hTunnelList, LB_RESETCONTENT, 0, 0); g_ActiveTunnels.clear();
            std::wstring cfExe = DependencyManager::ResolveExecutable(L"cloudflared.exe"); if (cfExe.empty()) return 0;
            std::string out = Utils::ExecSyncAndRead(L"\"" + cfExe + L"\" tunnel list");
            std::regex rx("([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\\s+([^\\s]+)");
            std::sregex_iterator next(out.begin(), out.end(), rx), end;
            while (next != end) {
                std::smatch match = *next; TunnelInfo ti = { Utils::UTF8ToWide(match[1].str()), Utils::UTF8ToWide(match[2].str()) };
                g_ActiveTunnels.push_back(ti); std::wstring display = L"[*] " + ti.name + L"  (" + ti.id + L")";
                SendMessageW(g_hTunnelList, LB_ADDSTRING, 0, (LPARAM)display.c_str()); next++;
            }
        } else if (LOWORD(wParam) == 103) {
            int sel = SendMessage(g_hTunnelList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < g_ActiveTunnels.size()) {
                if (ShowModernAlert(hWnd, L"Confirm", L"Delete this tunnel?", TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, TD_WARNING_ICON) == IDYES) {
                    Utils::ExecHiddenSync(L"\"" + DependencyManager::ResolveExecutable(L"cloudflared.exe") + L"\" tunnel delete -f " + g_ActiveTunnels[sel].id, AppManager::g_BaseDir);
                    SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(102, BN_CLICKED), 0);
                }
            }
        }
        break;
    case WM_CLOSE: EnableWindow(g_hWndMain, TRUE); DestroyWindow(hWnd); g_hTunnelDlg = NULL; break;
    default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

struct AppEditorData { bool isEdit; AppConfig config; HWND hName, hPort, hType, hEntry, hDomain; HWND hBtnS, hBtnC; HWND hLblEntry, hLblDomain; };
AppEditorData* g_pEditorData = nullptr; HWND g_hAppDlg = NULL;
LRESULT CALLBACK AppEditorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
    case WM_CREATE: {
        g_Theme.ApplyToWindow(hWnd);
        CreateWindowW(WC_STATIC, L"Name:", WS_CHILD|WS_VISIBLE, 15,20,90,20, hWnd, NULL, NULL, NULL);
        g_pEditorData->hName = CreateWindowW(WC_EDIT, L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL, 110,15,200,24, hWnd, NULL, NULL, NULL);
        
        CreateWindowW(WC_STATIC, L"Port:", WS_CHILD|WS_VISIBLE, 15,55,90,20, hWnd, NULL, NULL, NULL);
        g_pEditorData->hPort = CreateWindowW(WC_EDIT, L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_NUMBER, 110,50,200,24, hWnd, NULL, NULL, NULL);
        
        CreateWindowW(WC_STATIC, L"Type:", WS_CHILD|WS_VISIBLE, 15,90,90,20, hWnd, NULL, NULL, NULL);
        g_pEditorData->hType = CreateWindowW(WC_COMBOBOX, L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST, 110,85,200,200, hWnd, (HMENU)1001, NULL, NULL);
        SendMessageW(g_pEditorData->hType, CB_ADDSTRING, 0, (LPARAM)L"Node.js"); SendMessageW(g_pEditorData->hType, CB_ADDSTRING, 0, (LPARAM)L"Static (GitHub)"); SendMessageW(g_pEditorData->hType, CB_ADDSTRING, 0, (LPARAM)L"Static (CF Tunnel)");
        
        g_pEditorData->hLblEntry = CreateWindowW(WC_STATIC, L"Entry File:", WS_CHILD|WS_VISIBLE, 15,125,90,20, hWnd, NULL, NULL, NULL);
        g_pEditorData->hEntry = CreateWindowW(WC_EDIT, L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL, 110,120,200,24, hWnd, NULL, NULL, NULL);
        
        g_pEditorData->hLblDomain = CreateWindowW(WC_STATIC, L"Custom Domain:", WS_CHILD|WS_VISIBLE, 15,160,95,20, hWnd, NULL, NULL, NULL);
        g_pEditorData->hDomain = CreateWindowW(WC_EDIT, L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL, 110,155,200,24, hWnd, NULL, NULL, NULL);
        
        g_pEditorData->hBtnS = CreateWindowW(WC_BUTTON, L"Save", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 140,210,80,30, hWnd, (HMENU)IDOK, NULL, NULL);
        g_pEditorData->hBtnC = CreateWindowW(WC_BUTTON, L"Cancel", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 230,210,80,30, hWnd, (HMENU)IDCANCEL, NULL, NULL);

        g_Theme.ApplyToControls({ g_pEditorData->hName, g_pEditorData->hPort, g_pEditorData->hType, g_pEditorData->hDomain, g_pEditorData->hEntry, g_pEditorData->hBtnS, g_pEditorData->hBtnC });

        if (g_pEditorData->isEdit) {
            SetWindowTextW(g_pEditorData->hName, g_pEditorData->config.name.c_str()); EnableWindow(g_pEditorData->hName, FALSE);
            SetWindowTextW(g_pEditorData->hPort, std::to_wstring(g_pEditorData->config.port).c_str());
            int typeIdx = 0; if (g_pEditorData->config.app_type == L"static_gh") typeIdx = 1; else if (g_pEditorData->config.app_type == L"static_cf") typeIdx = 2;
            SendMessage(g_pEditorData->hType, CB_SETCURSEL, typeIdx, 0);
            SetWindowTextW(g_pEditorData->hEntry, g_pEditorData->config.entry.c_str());
            SetWindowTextW(g_pEditorData->hDomain, g_pEditorData->config.domain.c_str());
        } else {
            SetWindowTextW(g_pEditorData->hPort, L"3000"); SendMessage(g_pEditorData->hType, CB_SETCURSEL, 0, 0); 
            SetWindowTextW(g_pEditorData->hEntry, L"server.js");
        }
        SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(1001, CBN_SELCHANGE), 0);
        break;
    }
    case WM_SIZE: {
        int w = LOWORD(lParam), h = HIWORD(lParam);
        MoveWindow(g_pEditorData->hName, 115, 15, w - 135, 25, TRUE);
        MoveWindow(g_pEditorData->hPort, 115, 50, w - 135, 25, TRUE);
        MoveWindow(g_pEditorData->hType, 115, 85, w - 135, 200, TRUE);
        MoveWindow(g_pEditorData->hEntry, 115, 120, w - 135, 25, TRUE);
        MoveWindow(g_pEditorData->hDomain, 115, 155, w - 135, 25, TRUE);
        
        MoveWindow(g_pEditorData->hBtnS, w - 190, h - 45, 80, 30, TRUE);
        MoveWindow(g_pEditorData->hBtnC, w - 95, h - 45, 80, 30, TRUE);
        return 0;
    }
    case WM_GETMINMAXINFO: { MINMAXINFO* mmi = (MINMAXINFO*)lParam; mmi->ptMinTrackSize.x = 350; mmi->ptMinTrackSize.y = 300; return 0; }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
        if (pdis->CtlType == ODT_BUTTON) return DrawOwnerDrawButton(lParam);
        break;
    }
    case WM_ERASEBKGND: { HDC hdc = (HDC)wParam; RECT rc; GetClientRect(hWnd, &rc); FillRect(hdc, &rc, g_Theme.hbrBackground); return 1; }
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: return CommonCtlColor(hWnd, msg, wParam, lParam);
    case WM_COMMAND: {
        if (LOWORD(wParam) == 1001 && HIWORD(wParam) == CBN_SELCHANGE) {
            int typeIdx = SendMessage(g_pEditorData->hType, CB_GETCURSEL, 0, 0);
            if (typeIdx == 0) { // Node.js
                ShowWindow(g_pEditorData->hLblEntry, SW_SHOW); ShowWindow(g_pEditorData->hEntry, SW_SHOW);
                ShowWindow(g_pEditorData->hLblDomain, SW_SHOW); ShowWindow(g_pEditorData->hDomain, SW_SHOW);
            } else if (typeIdx == 1) { // Static (GitHub)
                ShowWindow(g_pEditorData->hLblEntry, SW_HIDE); ShowWindow(g_pEditorData->hEntry, SW_HIDE);
                ShowWindow(g_pEditorData->hLblDomain, SW_HIDE); ShowWindow(g_pEditorData->hDomain, SW_HIDE);
            } else { // Static (CF Tunnel)
                ShowWindow(g_pEditorData->hLblEntry, SW_HIDE); ShowWindow(g_pEditorData->hEntry, SW_HIDE);
                ShowWindow(g_pEditorData->hLblDomain, SW_SHOW); ShowWindow(g_pEditorData->hDomain, SW_SHOW);
            }
            return 0;
        }
        if (LOWORD(wParam) == IDCANCEL) SendMessage(hWnd, WM_CLOSE, 0, 0);
        else if (LOWORD(wParam) == IDOK) {
            wchar_t buf[256]; GetWindowTextW(g_pEditorData->hName, buf, 256); std::wstring name = buf;
            GetWindowTextW(g_pEditorData->hPort, buf, 256); int port = _wtoi(buf);
            GetWindowTextW(g_pEditorData->hDomain, buf, 256); std::wstring domStr = buf;
            GetWindowTextW(g_pEditorData->hEntry, buf, 256); std::wstring entryStr = buf;
            
            int tIdx = SendMessage(g_pEditorData->hType, CB_GETCURSEL, 0, 0);
            std::wstring type = (tIdx == 1) ? L"static_gh" : ((tIdx == 2) ? L"static_cf" : L"node");

            if (name.empty() || port <= 0) { ShowModernAlert(hWnd, L"Error", L"Invalid fields provided. Please enter a valid App Name and Port.", TDCBF_OK_BUTTON, TD_ERROR_ICON); return 0; }
            g_pEditorData->config.name = name; g_pEditorData->config.port = port; g_pEditorData->config.app_type = type;
            
            if (type == L"node") { 
                g_pEditorData->config.entry = entryStr.empty() ? L"server.js" : entryStr; 
                g_pEditorData->config.domain = domStr; 
            } else if (type == L"static_cf") { 
                g_pEditorData->config.entry = L""; 
                g_pEditorData->config.domain = domStr; 
            } else {
                g_pEditorData->config.entry = L""; 
                g_pEditorData->config.domain = L""; 
            }
            
            bool wasRunning = ProcessManager::g_Servers.count(name) && ProcessManager::g_Servers[name]->IsRunning();
            if (wasRunning && g_pEditorData->isEdit) { ProcessManager::g_Servers[name]->Stop(); if (ProcessManager::g_Tunnels.count(name)) ProcessManager::g_Tunnels[name]->Stop(); }
            AppManager::SaveAppConfig(g_pEditorData->config);

            if (!g_pEditorData->isEdit) {
                fs::path appDir = AppManager::g_AppsDir / name;
                if (type == L"node") {
                    std::wofstream out(appDir / g_pEditorData->config.entry);
                    out << L"const http = require('http');\n"
                        << L"const port = process.env.PORT || " << port << L";\n"
                        << L"const server = http.createServer((req, res) => {\n"
                        << L"    console.log(`[${new Date().toISOString()}] ${req.method} ${req.url}`);\n"
                        << L"    res.end('Localcel: " << name << L" is running!');\n"
                        << L"});\n"
                        << L"server.listen(port, () => {\n"
                        << L"    console.log(`Server successfully started and listening on port ${port}`);\n"
                        << L"});\n";
                } else {
                    std::wofstream out(appDir / L"index.html");
                    out << L"<!DOCTYPE html>\n<html>\n<head><title>" << name << L"</title><style>body { font-family: sans-serif; text-align: center; padding: 50px; } h1 { color: #2D3748; }</style></head>\n<body><h1>Localcel: " << name << L" is running!</h1><p>This is a static site managed by Localcel.</p></body>\n</html>";
                    std::wstring git = DependencyManager::ResolveExecutable(L"git.exe");
                    if (!git.empty()) {
                        Utils::ExecHiddenSync(L"\"" + git + L"\" init", appDir);
                        Utils::ExecHiddenSync(L"\"" + git + L"\" config user.name \"Localcel Deployer\"", appDir);
                        Utils::ExecHiddenSync(L"\"" + git + L"\" config user.email \"deploy@localcel.app\"", appDir);
                        Utils::ExecHiddenSync(L"\"" + git + L"\" add .", appDir);
                        Utils::ExecHiddenSync(L"\"" + git + L"\" commit -m \"Initial commit\"", appDir);
                        Utils::ExecHiddenSync(L"\"" + git + L"\" branch -M main", appDir);
                    }
                }
            }
            SendMessage(hWnd, WM_CLOSE, 0, 0); RefreshAppList();
            for(size_t i=0; i<AppManager::g_Apps.size(); ++i) { if (AppManager::g_Apps[i].name == name) { g_SelectedAppIdx = i; SendMessage(g_hListBox, LB_SETCURSEL, i, 0); break; } }
            UpdateUIState(); if (wasRunning && g_pEditorData->isEdit) SendMessage(g_hWndMain, WM_COMMAND, ID_BTN_START, 0);
        }
        break;
    }
    case WM_CLOSE: EnableWindow(g_hWndMain, TRUE); DestroyWindow(hWnd); g_hAppDlg = NULL; delete g_pEditorData; g_pEditorData = nullptr; break;
    default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// --- UI Helpers ---
void LogAppend(HWND hEdit, const std::wstring& text) {
    int len = GetWindowTextLength(hEdit); SendMessage(hEdit, EM_SETSEL, len, len);
    std::wstring clean = text; size_t pos = 0;
    while ((pos = clean.find(L"\n", pos)) != std::wstring::npos) { if (pos == 0 || clean[pos - 1] != L'\r') { clean.replace(pos, 1, L"\r\n"); pos += 2; } else pos++; }
    SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)clean.c_str()); SendMessage(hEdit, EM_SCROLLCARET, 0, 0);
}

void LoadLogHistory(HWND hEdit, const fs::path& logPath) {
    SetWindowTextW(hEdit, L""); if (!fs::exists(logPath)) return;
    std::ifstream file(logPath, std::ios::binary | std::ios::ate); if (!file) return;
    std::streamsize size = file.tellg(); if (size > 50000) file.seekg(size - 50000); else file.seekg(0, std::ios::beg);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()); LogAppend(hEdit, Utils::UTF8ToWide(content));
}

void RefreshAppList() {
    SendMessage(g_hListBox, LB_RESETCONTENT, 0, 0); AppManager::LoadApps();
    for (const auto& app : AppManager::g_Apps) {
        std::wstring display = app.name;
        if (app.app_type == L"static_gh") display += L" (GitHub Pages)"; else if (app.app_type == L"static_cf") display += L" (CF Tunnel)";
        if (ProcessManager::g_Servers.count(app.name) && ProcessManager::g_Servers[app.name]->IsRunning()) display = L"[*] " + display; else display = L"[ ] " + display;
        SendMessageW(g_hListBox, LB_ADDSTRING, 0, (LPARAM)display.c_str());
    }
    if (g_SelectedAppIdx >= 0 && g_SelectedAppIdx < AppManager::g_Apps.size()) SendMessage(g_hListBox, LB_SETCURSEL, g_SelectedAppIdx, 0);
}

void UpdateUIState() {
    if (g_SelectedAppIdx < 0 || g_SelectedAppIdx >= AppManager::g_Apps.size()) {
        SetWindowTextW(g_hStatAppTitle, L"Select an application"); SetWindowTextW(g_hStatURL, L"Not running");
        EnableWindow(g_hBtnStart, FALSE); EnableWindow(g_hBtnStop, FALSE); EnableWindow(g_hBtnDeploy, FALSE); EnableWindow(g_hBtnEdit, FALSE); EnableWindow(g_hBtnDel, FALSE);
        InvalidateRect(g_hBtnStart, NULL, FALSE); InvalidateRect(g_hBtnStop, NULL, FALSE); InvalidateRect(g_hBtnDeploy, NULL, FALSE);
        return;
    }
    AppConfig& app = AppManager::g_Apps[g_SelectedAppIdx]; bool isRunning = ProcessManager::g_Servers.count(app.name) && ProcessManager::g_Servers[app.name]->IsRunning();

    std::wstring title = app.name + (app.app_type == L"static_gh" ? L" (GitHub Pages)" : (app.app_type == L"static_cf" ? L" (CF Tunnel)" : L""));
    SetWindowTextW(g_hStatAppTitle, title.c_str());

    EnableWindow(g_hBtnStart, !isRunning); EnableWindow(g_hBtnStop, isRunning); EnableWindow(g_hBtnEdit, TRUE); EnableWindow(g_hBtnDel, TRUE);
    ShowWindow(g_hBtnDeploy, app.app_type == L"static_gh" ? SW_SHOW : SW_HIDE);
    SetWindowTextW(g_hBtnDeploy, app.gh_pages_deployed ? L"Undeploy" : L"Deploy"); EnableWindow(g_hBtnDeploy, TRUE);

    InvalidateRect(g_hBtnStart, NULL, FALSE); InvalidateRect(g_hBtnStop, NULL, FALSE); InvalidateRect(g_hBtnDeploy, NULL, FALSE);

    std::wstring localUrl = L"http://localhost:" + std::to_wstring(app.port); std::wstring displayUrl = L"";

    auto GetGHLink = [app]() -> std::wstring {
        if (GitManager::g_GHUserCache.empty() && !GitManager::g_FetchingGHUser) { GitManager::g_FetchingGHUser = true; std::thread([]() { GitManager::g_GHUserCache = GitManager::GetGHUser(); GitManager::g_FetchingGHUser = false; }).detach(); }
        if (!GitManager::g_GHUserCache.empty()) return L"https://" + GitManager::g_GHUserCache + L".github.io/" + app.name + L"/"; return L"";
    };

    if (isRunning) {
        if (app.app_type == L"static_gh") {
            displayUrl = L"Local: " + localUrl;
            if (app.gh_pages_deployed) { std::wstring ghLink = GetGHLink(); if (!ghLink.empty()) displayUrl += L"   |   Live: " + ghLink; }
        } else {
            if (ProcessManager::g_Tunnels.count(app.name) && !ProcessManager::g_Tunnels[app.name]->url.empty()) displayUrl = ProcessManager::g_Tunnels[app.name]->url;
            else displayUrl = L"Local: " + localUrl + L"  (Tunnelling...)";
        }
    } else {
        if (app.app_type == L"static_gh" && app.gh_pages_deployed) { std::wstring ghLink = GetGHLink(); if (!ghLink.empty()) displayUrl = L"Stopped   |   Live: " + ghLink; else displayUrl = L"Stopped   |   Live: Fetching URL..."; }
        else displayUrl = L"Stopped";
    }
    SetWindowTextW(g_hStatURL, displayUrl.c_str());

    std::wstring tooltip = L"Localcel - Running:"; bool anyRunning = false;
    for (auto& pair : ProcessManager::g_Servers) { 
        if (pair.second->IsRunning()) { tooltip += L"\n  " + pair.first; anyRunning = true; } 
    }
    if (!anyRunning) tooltip = L"Localcel - No running servers";
    wcscpy_s(g_nid.szTip, tooltip.substr(0, 127).c_str()); Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    
    InvalidateRect(g_hListBox, NULL, FALSE); 
}

void SwitchTab(int tabIdx) { 
    if (tabIdx == 0) {
        ShowWindow(g_hEditServer, SW_SHOW); ShowWindow(g_hEditTunnel, SW_HIDE);
        SetPropW(g_hBtnTabServer, L"ActiveTab", (HANDLE)1); RemovePropW(g_hBtnTabTunnel, L"ActiveTab");
    } else {
        ShowWindow(g_hEditServer, SW_HIDE); ShowWindow(g_hEditTunnel, SW_SHOW);
        RemovePropW(g_hBtnTabServer, L"ActiveTab"); SetPropW(g_hBtnTabTunnel, L"ActiveTab", (HANDLE)1);
    }
    InvalidateRect(g_hBtnTabServer, NULL, FALSE); InvalidateRect(g_hBtnTabTunnel, NULL, FALSE);
}

// --- Action Handlers ---
void OnStart() {
    if (g_SelectedAppIdx < 0) return; AppConfig& app = AppManager::g_Apps[g_SelectedAppIdx];

    if (ProcessManager::IsPortInUse(app.port)) { 
        int freePort = app.port + 1; 
        while (ProcessManager::IsPortInUse(freePort) && freePort <= 65535) freePort++;
        
        std::wstring msg = L"Port " + std::to_wstring(app.port) + L" is currently in use by another program.\n\nWould you like Localcel to automatically switch this app to free port " + std::to_wstring(freePort) + L"?";
        
        if (ShowModernAlert(g_hWndMain, L"Port Busy", msg, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, TD_WARNING_ICON) == IDYES) {
            app.port = freePort; 
            AppManager::SaveAppConfig(app); 
            AppManager::g_Apps[g_SelectedAppIdx] = app; 
        } else {
            ShowModernAlert(g_hWndMain, L"Start Cancelled", L"The server could not be started because the port is busy.", TDCBF_OK_BUTTON, TD_ERROR_ICON);
            return;
        }
    }

    ProcessManager::g_Servers[app.name] = std::make_unique<ManagedProcess>(app.name, false); ProcessManager::g_Tunnels[app.name] = std::make_unique<ManagedProcess>(app.name, true);
    fs::path appDir = AppManager::g_AppsDir / app.name;

    if (app.app_type == L"node") {
        if (!EnsureDependencyUI(L"Node.js", L"node.exe", L"OpenJS.NodeJS")) return;
        
        // --- NODE.JS PORT INJECTION ---
        // Seamlessly monkey-patch net.Server to force the node app to use the Localcel assigned port 
        // without physically modifying their server.js code. 
        fs::path preloadPath = appDir / L".localcel_preload.js";
        std::wofstream preload(preloadPath);
        preload << L"const net = require('net');\n"
                << L"const oldListen = net.Server.prototype.listen;\n"
                << L"net.Server.prototype.listen = function(...args) {\n"
                << L"    if (args.length > 0) {\n"
                << L"        if (typeof args[0] === 'number') args[0] = parseInt(process.env.PORT, 10);\n"
                << L"        else if (typeof args[0] === 'object' && args[0] !== null && args[0].port) args[0].port = parseInt(process.env.PORT, 10);\n"
                << L"    }\n"
                << L"    return oldListen.apply(this, args);\n"
                << L"};\n";
        preload.close();

        SetEnvironmentVariableW(L"PORT", std::to_wstring(app.port).c_str());
        
        // Use NODE_OPTIONS --require to inject the hook natively before the user script runs
        wchar_t existingOpts[1024] = {0};
        GetEnvironmentVariableW(L"NODE_OPTIONS", existingOpts, 1024);
        std::wstring nodeOpts = L"--require ./.localcel_preload.js";
        if (wcslen(existingOpts) > 0) nodeOpts = std::wstring(existingOpts) + L" " + nodeOpts;
        SetEnvironmentVariableW(L"NODE_OPTIONS", nodeOpts.c_str());

        ProcessManager::g_Servers[app.name]->Start(L"\"" + DependencyManager::ResolveExecutable(L"node.exe") + L"\" " + app.entry, appDir.wstring());
        
        if (wcslen(existingOpts) > 0) SetEnvironmentVariableW(L"NODE_OPTIONS", existingOpts);
        else SetEnvironmentVariableW(L"NODE_OPTIONS", NULL);

    } else {
        std::wstring pyExe = DependencyManager::ResolveExecutable(L"python.exe"); if (pyExe.empty()) pyExe = DependencyManager::ResolveExecutable(L"python3.exe");
        if (pyExe.empty()) { if (!EnsureDependencyUI(L"Python", L"python.exe", L"Python.Python.3.12")) return; pyExe = DependencyManager::ResolveExecutable(L"python.exe"); }
        ProcessManager::g_Servers[app.name]->Start(L"\"" + pyExe + L"\" -u -m http.server " + std::to_wstring(app.port), appDir.wstring());
    }

    if (app.app_type != L"static_gh") {
        std::wstring cfExe = DependencyManager::ResolveExecutable(L"cloudflared.exe");
        if (!cfExe.empty()) {
            if (!app.domain.empty()) {
                std::wstring tunnelName = L"localcel_" + app.name; Utils::ExecHiddenSync(L"\"" + cfExe + L"\" tunnel create " + tunnelName, AppManager::g_BaseDir);
                std::string listOut = Utils::ExecSyncAndRead(L"\"" + cfExe + L"\" tunnel list"); 
                std::string u8Name = Utils::WideToUTF8(tunnelName);
                std::regex idRx("([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\\s+" + u8Name); 
                std::smatch m;
                if (std::regex_search(listOut, m, idRx)) {
                    std::wstring tid = Utils::UTF8ToWide(m[1].str()); 
                    
                    std::wstring* msgObj = new std::wstring(app.name + L"|tunnel|Binding Custom Domain to Tunnel ID: " + tid + L"\r\n");
                    PostMessageW(g_hWndMain, WM_APP_LOG, 0, (LPARAM)msgObj);
                    
                    Utils::ExecHiddenSync(L"\"" + cfExe + L"\" tunnel route dns -f " + tunnelName + L" " + app.domain, AppManager::g_BaseDir);
                    fs::path configPath = AppManager::g_AppsDir / app.name / L"tunnel.yml"; 
                    fs::path credFile = AppManager::GetUserProfile() / L".cloudflared" / (tid + L".json");
                    std::string credStr = Utils::WideToUTF8(credFile.wstring()); std::replace(credStr.begin(), credStr.end(), '\\', '/');
                    std::ofstream outYaml(configPath); outYaml << "tunnel: " << Utils::WideToUTF8(tid) << "\ncredentials-file: " << credStr << "\n";
                    outYaml << "ingress:\n  - hostname: " << Utils::WideToUTF8(app.domain) << "\n    service: http://localhost:" << app.port << "\n  - service: http_status:404\n"; outYaml.close();
                    ProcessManager::g_Tunnels[app.name]->Start(L"\"" + cfExe + L"\" tunnel --config tunnel.yml run", appDir.wstring()); 
                    ProcessManager::g_Tunnels[app.name]->url = L"https://" + app.domain;
                } else {
                    std::wstring* msgObj = new std::wstring(app.name + L"|tunnel|Error: Could not retrieve Cloudflare Tunnel ID. Try deleting the tunnel via Tunnel Manager.\r\n");
                    PostMessageW(g_hWndMain, WM_APP_LOG, 0, (LPARAM)msgObj);
                }
            } else { ProcessManager::g_Tunnels[app.name]->Start(L"\"" + cfExe + L"\" tunnel --url http://localhost:" + std::to_wstring(app.port), AppManager::g_BaseDir.wstring()); }
        }
    } else { std::wstring* msgObj = new std::wstring(app.name + L"|tunnel|GitHub Pages mode active. Local preview running. Use 'Deploy' to push to GitHub.\r\n"); PostMessageW(g_hWndMain, WM_APP_LOG, 0, (LPARAM)msgObj); }
    RefreshAppList(); UpdateUIState();
}

void OnStop() {
    if (g_SelectedAppIdx < 0) return; AppConfig& app = AppManager::g_Apps[g_SelectedAppIdx];
    if (ProcessManager::g_Servers.count(app.name)) ProcessManager::g_Servers[app.name]->Stop(); if (ProcessManager::g_Tunnels.count(app.name)) ProcessManager::g_Tunnels[app.name]->Stop();
    RefreshAppList(); UpdateUIState();
}

void OnDeploy() {
    if (g_SelectedAppIdx < 0) return; AppConfig& app = AppManager::g_Apps[g_SelectedAppIdx]; if (app.app_type != L"static_gh") return;
    if (!EnsureDependencyUI(L"Git", L"git.exe", L"Git.Git")) return; if (!EnsureDependencyUI(L"GitHub CLI", L"gh.exe", L"GitHub.cli")) return;
    if (GitManager::GetGHUser().empty()) { ShowModernAlert(g_hWndMain, L"Login Required", L"Please log in to GitHub first using the Git Login button.", TDCBF_OK_BUTTON, TD_WARNING_ICON); return; }

    std::wstring git = DependencyManager::ResolveExecutable(L"git.exe"); std::wstring gh = DependencyManager::ResolveExecutable(L"gh.exe");
    EnableWindow(g_hBtnDeploy, FALSE); SetWindowTextW(g_hBtnDeploy, app.gh_pages_deployed ? L"Undeploying..." : L"Deploying...");

    std::thread([app, git, gh]() mutable {
        fs::path cwd = AppManager::g_AppsDir / app.name;
        auto log = [app](const std::wstring& msg) { std::wstring* obj = new std::wstring(app.name + L"|tunnel|[GIT] " + msg + L"\r\n"); PostMessageW(g_hWndMain, WM_APP_LOG, 0, (LPARAM)obj); };
        if (app.gh_pages_deployed) {
            log(L"--- Disabling GitHub Pages ---"); std::string repoJson = Utils::ExecSyncAndRead(L"\"" + gh + L"\" repo view --json nameWithOwner -q .nameWithOwner");
            std::wstring repoName = Utils::TrimRight(Utils::UTF8ToWide(repoJson));
            if (!repoName.empty()) { Utils::ExecHiddenSync(L"\"" + gh + L"\" api -X DELETE /repos/" + repoName + L"/pages", cwd); log(L"GitHub Pages disabled (Repository intact)."); }
            app.gh_pages_deployed = false;
        } else {
            log(L"--- Starting GitHub Deploy ---"); Utils::ExecHiddenSync(L"\"" + git + L"\" config user.name Localcel", cwd); Utils::ExecHiddenSync(L"\"" + git + L"\" config user.email deploy@localcel", cwd);
            Utils::ExecHiddenSync(L"\"" + git + L"\" add .", cwd); Utils::ExecHiddenSync(L"\"" + git + L"\" commit -m \"Deploy\"", cwd);
            std::string remotes = Utils::ExecSyncAndRead(L"\"" + git + L"\" remote -v");
            if (remotes.find("origin") == std::string::npos) { log(L"Creating public GitHub repository..."); Utils::ExecHiddenSync(L"\"" + gh + L"\" repo create " + app.name + L" --public --source=. --push", cwd);
            } else { log(L"Pushing to GitHub..."); Utils::ExecHiddenSync(L"\"" + git + L"\" push -u origin main", cwd); }
            log(L"Enabling Pages..."); std::string repoJson = Utils::ExecSyncAndRead(L"\"" + gh + L"\" repo view --json nameWithOwner -q .nameWithOwner");
            std::wstring repoName = Utils::TrimRight(Utils::UTF8ToWide(repoJson));
            if (!repoName.empty()) Utils::ExecHiddenSync(L"\"" + gh + L"\" api -X POST /repos/" + repoName + L"/pages -f source[branch]=main -f source[path]=/", cwd);
            app.gh_pages_deployed = true; log(L"Deployed successfully!");
        }
        AppManager::SaveAppConfig(app); PostMessageW(g_hWndMain, WM_APP_DEPLOY_DONE, 0, 0);
    }).detach();
}

void OnCFLogin() {
    if (!EnsureDependencyUI(L"Cloudflare Tunnel", L"cloudflared.exe", L"Cloudflare.cloudflared")) return;
    fs::path certPath = AppManager::GetUserProfile() / L".cloudflared" / L"cert.pem";
    if (fs::exists(certPath)) {
        if (ShowModernAlert(g_hWndMain, L"Cloudflare Login", L"You are already logged in to Cloudflare.\n\nDo you want to re-authenticate and overwrite your token?", TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, TD_INFORMATION_ICON) == IDNO) return;
        fs::remove(certPath);
    }
    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = { 0 }; std::wstring cmd = L"\"" + DependencyManager::ResolveExecutable(L"cloudflared.exe") + L"\" tunnel login";
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end()); cmdBuf.push_back(0); CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
}

void OnGitLogin() {
    if (!EnsureDependencyUI(L"GitHub CLI", L"gh.exe", L"GitHub.cli")) return; if (!EnsureDependencyUI(L"Git", L"git.exe", L"Git.Git")) return;
    std::wstring user = GitManager::GetGHUser();
    if (!user.empty()) {
        if (ShowModernAlert(g_hWndMain, L"GitHub Login", L"You are logged in to GitHub as:\n\n" + user + L"\n\nDo you want to log out and switch accounts?", TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, TD_INFORMATION_ICON) == IDYES) {
            Utils::ExecHiddenSync(L"\"" + DependencyManager::ResolveExecutable(L"gh.exe") + L"\" auth logout --hostname github.com", AppManager::g_BaseDir); GitManager::g_GHUserCache = L"";
        } else return;
    }
    ShowModernAlert(g_hWndMain, L"Git Login", L"Localcel uses the official GitHub CLI for secure authentication.\n\nA command prompt will open with a one-time code. Copy the code, press Enter to open your browser, and paste it.", TDCBF_OK_BUTTON, TD_INFORMATION_ICON);
    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = { 0 }; std::wstring cmd = L"\"" + DependencyManager::ResolveExecutable(L"gh.exe") + L"\" auth login --web --scopes repo,delete_repo";
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end()); cmdBuf.push_back(0); CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
}

// --- Main Window Procedure ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hWndMain = hWnd;
        
        g_Theme.Init(); 
        
        g_hIconOriginal = (HICON)LoadImageW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
        if (!g_hIconOriginal) g_hIconOriginal = LoadIcon(NULL, IDI_APPLICATION);
        
        g_Theme.Update(); 
        g_Theme.ApplyToWindow(hWnd); 
        g_pLogoImage = LoadImageFromResource(IDI_APP_LOGO);

        g_hFontNormal = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_hFontTitle = CreateFontW(22, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_hFontAppTitle = CreateFontW(28, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable Display");
        g_hFontConsolas = CreateFontW(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Consolas");

        g_hStatLogo = CreateWindowW(WC_STATIC, L"", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, NULL, NULL, NULL);
        SetWindowSubclass(g_hStatLogo, LogoSubclassProc, 5, 0);

        g_hListBox = CreateWindowW(WC_LISTBOX, L"", WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT, 0,0,0,0, hWnd, (HMENU)ID_LISTBOX, NULL, NULL);
        SetWindowSubclass(g_hListBox, ListBoxSubclassProc, 6, 0); 

        g_hBtnNew = CreateWindowW(WC_BUTTON, L"New App", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 0,0,0,0, hWnd, (HMENU)ID_BTN_NEW, NULL, NULL);
        g_hBtnCFLogin = CreateWindowW(WC_BUTTON, L"CF Login", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 0,0,0,0, hWnd, (HMENU)ID_BTN_CFLOGIN, NULL, NULL);
        g_hBtnGitLogin = CreateWindowW(WC_BUTTON, L"Git Login", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 0,0,0,0, hWnd, (HMENU)ID_BTN_GITLOGIN, NULL, NULL);
        g_hBtnTunnel = CreateWindowW(WC_BUTTON, L"Tunnel Manager", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 0,0,0,0, hWnd, (HMENU)ID_BTN_TUNNEL, NULL, NULL);
        
        g_hSepVert = CreateWindowW(WC_STATIC, L"", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, NULL, NULL, NULL);
        g_hSepSide = CreateWindowW(WC_STATIC, L"", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, NULL, NULL, NULL);
        g_hSepMain = CreateWindowW(WC_STATIC, L"", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, NULL, NULL, NULL);
        SetWindowSubclass(g_hSepVert, SeparatorSubclassProc, 1, 0);
        SetWindowSubclass(g_hSepSide, SeparatorSubclassProc, 2, 0);
        SetWindowSubclass(g_hSepMain, SeparatorSubclassProc, 3, 0);

        g_hStatAppTitle = CreateWindowW(WC_STATIC, L"Select an application", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, NULL, NULL, NULL);
        g_hStatURL = CreateWindowW(WC_STATIC, L"Not running", WS_CHILD|WS_VISIBLE|SS_NOTIFY, 0,0,0,0, hWnd, (HMENU)ID_STAT_URL, NULL, NULL);
        
        g_hBtnStart = CreateWindowW(WC_BUTTON, L"Start", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 0,0,0,0, hWnd, (HMENU)ID_BTN_START, NULL, NULL);
        g_hBtnStop = CreateWindowW(WC_BUTTON, L"Stop", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 0,0,0,0, hWnd, (HMENU)ID_BTN_STOP, NULL, NULL);
        g_hBtnDeploy = CreateWindowW(WC_BUTTON, L"Deploy", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 0,0,0,0, hWnd, (HMENU)ID_BTN_DEPLOY, NULL, NULL);
        g_hBtnEdit = CreateWindowW(WC_BUTTON, L"Edit App", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 0,0,0,0, hWnd, (HMENU)ID_BTN_EDIT, NULL, NULL);
        g_hBtnDel = CreateWindowW(WC_BUTTON, L"Delete", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 0,0,0,0, hWnd, (HMENU)ID_BTN_DEL, NULL, NULL);
        
        g_hBtnTabServer = CreateWindowW(WC_BUTTON, L"Server Logs", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 0,0,0,0, hWnd, (HMENU)ID_BTN_TAB_SVR, NULL, NULL);
        g_hBtnTabTunnel = CreateWindowW(WC_BUTTON, L"Tunnel / Deploy Logs", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW, 0,0,0,0, hWnd, (HMENU)ID_BTN_TAB_TUN, NULL, NULL);
        SetPropW(g_hBtnTabServer, L"ActiveTab", (HANDLE)1);

        g_hEditServer = CreateWindowW(WC_EDIT, L"", WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY, 0,0,0,0, hWnd, (HMENU)ID_EDIT_SERVER, NULL, NULL);
        g_hEditTunnel = CreateWindowW(WC_EDIT, L"", WS_CHILD|WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY, 0,0,0,0, hWnd, (HMENU)ID_EDIT_TUNNEL, NULL, NULL);

        SendMessage(g_hListBox, WM_SETFONT, (WPARAM)g_hFontNormal, 0); 
        SendMessage(g_hStatAppTitle, WM_SETFONT, (WPARAM)g_hFontAppTitle, 0);
        SendMessage(g_hStatURL, WM_SETFONT, (WPARAM)g_hFontNormal, 0); 
        SendMessage(g_hEditServer, WM_SETFONT, (WPARAM)g_hFontConsolas, 0); 
        SendMessage(g_hEditTunnel, WM_SETFONT, (WPARAM)g_hFontConsolas, 0);
        
        std::vector<HWND> btns = {g_hBtnNew, g_hBtnCFLogin, g_hBtnGitLogin, g_hBtnTunnel, g_hBtnStart, g_hBtnStop, g_hBtnDeploy, g_hBtnEdit, g_hBtnDel, g_hBtnTabServer, g_hBtnTabTunnel};
        for (HWND b : btns) SendMessage(b, WM_SETFONT, (WPARAM)g_hFontNormal, 0);

        g_Theme.ApplyToControls({g_hListBox, g_hEditServer, g_hEditTunnel, g_hBtnNew, g_hBtnCFLogin, g_hBtnGitLogin, g_hBtnTunnel, g_hBtnStart, g_hBtnStop, g_hBtnDeploy, g_hBtnEdit, g_hBtnDel, g_hBtnTabServer, g_hBtnTabTunnel});

        g_nid.cbSize = sizeof(NOTIFYICONDATA); g_nid.hWnd = hWnd; g_nid.uID = 1; 
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_nid.uCallbackMessage = WM_APP_TRAY; 
        Shell_NotifyIconW(NIM_ADD, &g_nid);

        RefreshAppList(); SetTimer(hWnd, 1, 1000, NULL);

        std::thread([]() {
            std::string out = Utils::ExecSyncAndRead(L"winget upgrade --accept-source-agreements");
            std::wstring* pUpdates = new std::wstring();
            if (out.find("Cloudflare.cloudflared") != std::string::npos) *pUpdates += L"Cloudflare.cloudflared ";
            if (out.find("GitHub.cli") != std::string::npos) *pUpdates += L"GitHub.cli ";
            if (out.find("Git.Git") != std::string::npos) *pUpdates += L"Git.Git ";
            
            if (!pUpdates->empty()) {
                PostMessageW(g_hWndMain, WM_APP_UPDATE_AVAILABLE, (WPARAM)pUpdates, 0);
            } else {
                delete pUpdates;
            }
        }).detach();

        break;
    }
    case WM_SETTINGCHANGE: {
        if (lParam && wcscmp((LPCWSTR)lParam, L"ImmersiveColorSet") == 0) {
            g_Theme.Update();
            g_Theme.ApplyToWindow(hWnd);
            g_Theme.ApplyToControls({g_hListBox, g_hEditServer, g_hEditTunnel, g_hBtnNew, g_hBtnCFLogin, g_hBtnGitLogin, g_hBtnTunnel, g_hBtnStart, g_hBtnStop, g_hBtnDeploy, g_hBtnEdit, g_hBtnDel, g_hBtnTabServer, g_hBtnTabTunnel});
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;
    }
    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
        if (lpmis->CtlID == ID_LISTBOX) { lpmis->itemHeight = 36; return TRUE; }
        break;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
        if (pdis->CtlType == ODT_BUTTON) return DrawOwnerDrawButton(lParam);
        if (pdis->CtlType == ODT_LISTBOX) return DrawOwnerDrawListBox(lParam);
        break;
    }
    case WM_ERASEBKGND: { 
        HDC hdc = (HDC)wParam; RECT rc; GetClientRect(hWnd, &rc); 
        FillRect(hdc, &rc, g_Theme.hbrBackground); 
        return 1; 
    }
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: return CommonCtlColor(hWnd, msg, wParam, lParam);
    case WM_SETCURSOR: {
        if ((HWND)wParam == g_hStatURL) {
            wchar_t urlText[512]; GetWindowTextW(g_hStatURL, urlText, 512); std::wstring text(urlText);
            if (text.find(L"http") != std::wstring::npos) { SetCursor(LoadCursor(NULL, IDC_HAND)); return TRUE; }
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    case WM_GETMINMAXINFO: { MINMAXINFO* mmi = (MINMAXINFO*)lParam; mmi->ptMinTrackSize.x = 650; mmi->ptMinTrackSize.y = 450; return 0; }
    case WM_SIZE: {
        int width = LOWORD(lParam), height = HIWORD(lParam);
        int sidebarW = 220; int pad = 15;

        MoveWindow(g_hStatLogo, pad, pad, sidebarW - 10, 45, TRUE);
        MoveWindow(g_hSepSide, pad, pad + 55, sidebarW, 1, TRUE); 
        MoveWindow(g_hListBox, pad, pad + 65, sidebarW, height - 250, TRUE); 
        
        int sBtnY = height - 165;
        MoveWindow(g_hBtnNew, pad, sBtnY, sidebarW, 30, TRUE);
        MoveWindow(g_hBtnCFLogin, pad, sBtnY + 40, (sidebarW/2) - 5, 30, TRUE);
        MoveWindow(g_hBtnGitLogin, pad + (sidebarW/2) + 5, sBtnY + 40, (sidebarW/2) - 5, 30, TRUE);
        MoveWindow(g_hBtnTunnel, pad, sBtnY + 80, sidebarW, 30, TRUE);

        MoveWindow(g_hSepVert, pad + sidebarW + 10, pad, 1, height - (pad*2), TRUE);

        int mainX = pad + sidebarW + 25; int mainW = width - mainX - pad;
        MoveWindow(g_hStatAppTitle, mainX, pad, mainW, 35, TRUE); 
        MoveWindow(g_hStatURL, mainX, pad + 38, mainW, 25, TRUE);
        
        int btnW = 80; int actionY = pad + 75; 
        MoveWindow(g_hBtnStart, mainX, actionY, btnW, 30, TRUE); 
        MoveWindow(g_hBtnStop, mainX + btnW + 10, actionY, btnW, 30, TRUE);
        MoveWindow(g_hBtnDeploy, mainX + (btnW + 10)*2, actionY, btnW, 30, TRUE); 
        MoveWindow(g_hBtnEdit, mainX + (btnW + 10)*3, actionY, btnW, 30, TRUE); 
        MoveWindow(g_hBtnDel, width - pad - btnW, actionY, btnW, 30, TRUE); 

        int sepY = actionY + 45;
        MoveWindow(g_hSepMain, mainX, sepY, mainW, 1, TRUE);

        int tabY = sepY + 15; 
        MoveWindow(g_hBtnTabServer, mainX, tabY, 110, 30, TRUE);
        MoveWindow(g_hBtnTabTunnel, mainX + 120, tabY, 180, 30, TRUE);
        
        int editY = tabY + 40;
        MoveWindow(g_hEditServer, mainX, editY, mainW, height - editY - pad, TRUE); 
        MoveWindow(g_hEditTunnel, mainX, editY, mainW, height - editY - pad, TRUE);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == ID_BTN_TAB_SVR) SwitchTab(0);
        else if (LOWORD(wParam) == ID_BTN_TAB_TUN) SwitchTab(1);
        else if (LOWORD(wParam) == ID_LISTBOX && HIWORD(wParam) == LBN_SELCHANGE) {
            g_SelectedAppIdx = SendMessage(g_hListBox, LB_GETCURSEL, 0, 0);
            if (g_SelectedAppIdx >= 0 && g_SelectedAppIdx < AppManager::g_Apps.size()) {
                LoadLogHistory(g_hEditServer, AppManager::g_LogsDir / (AppManager::g_Apps[g_SelectedAppIdx].name + L".log"));
                LoadLogHistory(g_hEditTunnel, AppManager::g_LogsDir / (AppManager::g_Apps[g_SelectedAppIdx].name + L"_tunnel.log"));
            }
            UpdateUIState();
        }
        else if (LOWORD(wParam) == ID_STAT_URL && HIWORD(wParam) == STN_CLICKED) {
            wchar_t urlText[512]; GetWindowTextW(g_hStatURL, urlText, 512); std::wstring text(urlText);
            size_t pos_https = text.rfind(L"https://"), pos_http = text.rfind(L"http://");
            size_t pos = (pos_https != std::wstring::npos) ? pos_https : pos_http;
            if (pos != std::wstring::npos) {
                std::wstring url = text.substr(pos); size_t space = url.find(L' ');
                if (space != std::wstring::npos) url = url.substr(0, space);
                ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
        }
        else if (LOWORD(wParam) == ID_BTN_START) OnStart(); else if (LOWORD(wParam) == ID_BTN_STOP) OnStop();
        else if (LOWORD(wParam) == ID_BTN_DEPLOY) OnDeploy(); else if (LOWORD(wParam) == ID_BTN_CFLOGIN) OnCFLogin();
        else if (LOWORD(wParam) == ID_BTN_GITLOGIN) OnGitLogin();
        else if (LOWORD(wParam) == ID_BTN_DEL) {
            if (g_SelectedAppIdx >= 0) {
                AppConfig& app = AppManager::g_Apps[g_SelectedAppIdx];
                if (app.app_type == L"static_gh") {
                    if (ShowModernAlert(hWnd, L"Delete Application", L"Do you want to delete the remote GitHub repository as well?\n\nYes: Deletes remote repo AND local files.\nNo: Deletes local files only.", TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON, TD_WARNING_ICON) == IDYES) {
                        std::wstring gh = DependencyManager::ResolveExecutable(L"gh.exe");
                        if (!gh.empty()) {
                            std::wstring repoName = Utils::TrimRight(Utils::UTF8ToWide(Utils::ExecSyncAndRead(L"\"" + gh + L"\" repo view --json nameWithOwner -q .nameWithOwner")));
                            if (repoName.empty()) repoName = app.name; Utils::ExecHiddenSync(L"\"" + gh + L"\" repo delete " + repoName + L" --yes", AppManager::g_AppsDir / app.name);
                        }
                    } else if (ShowModernAlert(hWnd, L"Confirm Deletion", L"Are you sure you want to delete this application?", TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, TD_WARNING_ICON) != IDYES) return 0;
                } else if (ShowModernAlert(hWnd, L"Confirm Deletion", L"Are you sure you want to delete this application and all its files?", TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, TD_WARNING_ICON) != IDYES) return 0;
                
                OnStop(); Utils::ForceRemoveAll(AppManager::g_AppsDir / app.name); g_SelectedAppIdx = -1; RefreshAppList(); UpdateUIState();
            }
        }
        else if (LOWORD(wParam) == ID_BTN_NEW) {
            if (g_hAppDlg) return 0; g_pEditorData = new AppEditorData{false, AppConfig{}}; EnableWindow(hWnd, FALSE);
            int w = 380, h = 300; POINT pt = GetCenterCoords(hWnd, w, h);
            g_hAppDlg = CreateWindowExW(0, L"AppEditorClass", L"New App", WS_OVERLAPPEDWINDOW | WS_VISIBLE, pt.x, pt.y, w, h, hWnd, NULL, GetModuleHandle(NULL), NULL);
        }
        else if (LOWORD(wParam) == ID_BTN_EDIT) {
            if (g_SelectedAppIdx < 0 || g_hAppDlg) return 0; g_pEditorData = new AppEditorData{true, AppManager::g_Apps[g_SelectedAppIdx]}; EnableWindow(hWnd, FALSE);
            int w = 380, h = 300; POINT pt = GetCenterCoords(hWnd, w, h);
            g_hAppDlg = CreateWindowExW(0, L"AppEditorClass", L"Edit App", WS_OVERLAPPEDWINDOW | WS_VISIBLE, pt.x, pt.y, w, h, hWnd, NULL, GetModuleHandle(NULL), NULL);
        }
        else if (LOWORD(wParam) == ID_BTN_TUNNEL) {
            if (g_hTunnelDlg) return 0; EnableWindow(hWnd, FALSE);
            int w = 550, h = 450; POINT pt = GetCenterCoords(hWnd, w, h);
            g_hTunnelDlg = CreateWindowExW(0, L"TunnelManagerClass", L"Tunnel Manager", WS_OVERLAPPEDWINDOW | WS_VISIBLE, pt.x, pt.y, w, h, hWnd, NULL, GetModuleHandle(NULL), NULL);
        }
        else if (LOWORD(wParam) == ID_TRAY_SHOW) { ShowWindow(hWnd, SW_RESTORE); SetForegroundWindow(hWnd); }
        else if (LOWORD(wParam) == ID_TRAY_EXIT) {
            bool hasRunning = false; std::wstring rNames = L"";
            for (auto& p : ProcessManager::g_Servers) if (p.second->IsRunning()) { hasRunning = true; rNames += p.first + L"\n"; }
            if (hasRunning && ShowModernAlert(hWnd, L"Servers Running", L"The following servers are still running:\n\n" + rNames + L"\nClosing Localcel will stop these servers. Are you sure you want to exit?", TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, TD_WARNING_ICON) == IDNO) return 0;
            ProcessManager::StopAll(); Shell_NotifyIconW(NIM_DELETE, &g_nid); PostQuitMessage(0);
        }
        break;
    }
    case WM_TIMER: UpdateUIState(); break;
    case WM_APP_LOG: {
        std::wstring* msgObj = (std::wstring*)lParam;
        if (msgObj) {
            size_t sep1 = msgObj->find(L'|'), sep2 = msgObj->find(L'|', sep1 + 1);
            if (sep1 != std::wstring::npos && sep2 != std::wstring::npos) {
                std::wstring name = msgObj->substr(0, sep1), type = msgObj->substr(sep1 + 1, sep2 - sep1 - 1), text = msgObj->substr(sep2 + 1);
                if (g_SelectedAppIdx >= 0 && AppManager::g_Apps[g_SelectedAppIdx].name == name) LogAppend(type == L"server" ? g_hEditServer : g_hEditTunnel, text);
            }
            delete msgObj;
        }
        break;
    }
    case WM_APP_DEPLOY_DONE: RefreshAppList(); UpdateUIState(); break;
    case WM_APP_UPDATE_AVAILABLE: {
        std::wstring* pUpdates = (std::wstring*)wParam;
        if (pUpdates) {
            std::wstring msg = L"Updates are available for your dependencies:\n\n" + *pUpdates + L"\n\nWould you like to install them now?";
            if (ShowModernAlert(hWnd, L"Updates Available", msg, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, TD_INFORMATION_ICON) == IDYES) {
                STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = { 0 };
                std::wstring cmd = L"cmd.exe /c \"";
                if (pUpdates->find(L"Cloudflare.cloudflared") != std::wstring::npos) cmd += L"winget upgrade --id Cloudflare.cloudflared -e --accept-package-agreements --accept-source-agreements & ";
                if (pUpdates->find(L"GitHub.cli") != std::wstring::npos) cmd += L"winget upgrade --id GitHub.cli -e --accept-package-agreements --accept-source-agreements & ";
                if (pUpdates->find(L"Git.Git") != std::wstring::npos) cmd += L"winget upgrade --id Git.Git -e --accept-package-agreements --accept-source-agreements & ";
                cmd += L"echo. & echo Updates complete. & pause\"";
                std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end()); cmdBuf.push_back(0);
                CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
            }
            delete pUpdates;
        }
        break;
    }
    case WM_APP_TRAY: {
        if (lParam == WM_LBUTTONDBLCLK) { ShowWindow(hWnd, SW_RESTORE); SetForegroundWindow(hWnd); }
        else if (lParam == WM_RBUTTONUP) {
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"Show Localcel"); AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL); AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
            POINT pt; GetCursorPos(&pt); SetForegroundWindow(hWnd); TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL); DestroyMenu(hMenu);
        }
        break;
    }
    case WM_CLOSE: {
        bool hasRunning = false; for (auto& p : ProcessManager::g_Servers) if (p.second->IsRunning()) hasRunning = true;
        if (hasRunning) { 
            ShowWindow(hWnd, SW_HIDE); 
            g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO; 
            g_nid.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON; 
            g_nid.hBalloonIcon = g_hIconOriginal;
            wcscpy_s(g_nid.szInfoTitle, L"Background Mode"); 
            wcscpy_s(g_nid.szInfo, L"Localcel is keeping your servers alive."); 
            Shell_NotifyIconW(NIM_MODIFY, &g_nid); 
            
            g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            return 0; 
        }
        ProcessManager::StopAll(); Shell_NotifyIconW(NIM_DELETE, &g_nid); PostQuitMessage(0); break;
    }
    case WM_DESTROY: ProcessManager::StopAll(); Shell_NotifyIconW(NIM_DELETE, &g_nid); PostQuitMessage(0); break;
    default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}


namespace WindowsUI {
    int RunUI(HINSTANCE hInstance, int nCmdShow) {
        SetCurrentProcessExplicitAppUserModelID(L"Localcel");
        INITCOMMONCONTROLSEX icex; icex.dwSize = sizeof(INITCOMMONCONTROLSEX); icex.dwICC = ICC_STANDARD_CLASSES; InitCommonControlsEx(&icex);
        WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa); 
        
        if (!AppManager::TryLoadWorkspace()) {
            BROWSEINFOW bi = { 0 }; bi.lpszTitle = L"Select Workspace Directory"; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl != 0) {
                wchar_t path[MAX_PATH]; SHGetPathFromIDListW(pidl, path);
                IMalloc* imalloc = 0; if (SUCCEEDED(SHGetMalloc(&imalloc))) { imalloc->Free(pidl); imalloc->Release(); }
                AppManager::SetWorkspaceDir(std::filesystem::path(path));
            } else ExitProcess(0);
        }

        Gdiplus::GdiplusStartupInput gdiplusStartupInput; ULONG_PTR gdiplusToken; Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
        HICON hAppIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(101), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);

        WNDCLASSW wc = { 0 }; wc.lpfnWndProc = WndProc; wc.hInstance = hInstance; wc.hIcon = hAppIcon ? hAppIcon : LoadIcon(NULL, IDI_APPLICATION); wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = NULL; wc.lpszClassName = L"LocalcelNative"; RegisterClassW(&wc);
        WNDCLASSW wcAppEdit = { 0 }; wcAppEdit.lpfnWndProc = AppEditorWndProc; wcAppEdit.hInstance = hInstance; wcAppEdit.hbrBackground = NULL; wcAppEdit.lpszClassName = L"AppEditorClass"; wcAppEdit.hCursor = LoadCursor(NULL, IDC_ARROW); RegisterClassW(&wcAppEdit);
        WNDCLASSW wcTunMgr = { 0 }; wcTunMgr.lpfnWndProc = TunnelManagerWndProc; wcTunMgr.hInstance = hInstance; wcTunMgr.hbrBackground = NULL; wcTunMgr.lpszClassName = L"TunnelManagerClass"; wcTunMgr.hCursor = LoadCursor(NULL, IDC_ARROW); RegisterClassW(&wcTunMgr);
        WNDCLASSW wcPort = { 0 }; wcPort.lpfnWndProc = PortPromptWndProc; wcPort.hInstance = hInstance; wcPort.hbrBackground = NULL; wcPort.lpszClassName = L"PortPromptClass"; wcPort.hCursor = LoadCursor(NULL, IDC_ARROW); RegisterClassW(&wcPort);

        int mainW = 850, mainH = 600;
        RECT rcWork; SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
        int mainX = rcWork.left + (rcWork.right - rcWork.left - mainW) / 2;
        int mainY = rcWork.top + (rcWork.bottom - rcWork.top - mainH) / 2;

        HWND hWnd = CreateWindowW(L"LocalcelNative", L"Localcel", WS_OVERLAPPEDWINDOW, mainX, mainY, mainW, mainH, NULL, NULL, hInstance, NULL);
        ShowWindow(hWnd, nCmdShow); UpdateWindow(hWnd);

        MSG msg; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        
        if (g_pLogoStream) g_pLogoStream->Release();
        if (g_hIconCurrent && g_hIconCurrent != g_hIconOriginal) DestroyIcon(g_hIconCurrent);
        if (g_hIconOriginal) DestroyIcon(g_hIconOriginal);
        
        Gdiplus::GdiplusShutdown(gdiplusToken);
        WSACleanup(); return (int)msg.wParam;
    }
}
#endif
