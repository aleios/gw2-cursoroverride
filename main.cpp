#include <windows.h>
#include <string>
#include <cstdint>
#include <imgui.h>

#include <MinHook.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* arcdps export table */
struct arcdps_exports {
    uintptr_t size; /* size of exports table */
    uint32_t sig; /* pick a number between 0 and uint32_t max that isn't used by other modules */
    uint32_t imguivers; /* set this to IMGUI_VERSION_NUM. if you don't use imgui, 18000 (as of 2021-02-02) */
    const char* out_name; /* name string */
    const char* out_build; /* build string */
    void* wnd_nofilter; /* wndproc callback, fn(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam), return assigned to umsg */
    void* combat; /* combat event callback, fn(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision) */
    void* imgui; /* id3dd9::present callback, before imgui::render, fn(uint32_t not_charsel_or_loading) */
    void* options_end; /* id3dd9::present callback, appending to the end of options window in arcdps, fn() */
    void* combat_local;  /* combat event callback like area but from chat log, fn(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision) */
    void* wnd_filter; /* wndproc callback like above, input filered using modifiers */
    void* options_windows; /* called once per 'window' option checkbox, with null at the end, non-zero return disables drawing that option, fn(char* windowname) */
};

arcdps_exports arcExports;
void dll_init(HANDLE hModule);
void dll_exit();
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, ImGuiContext* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion);
extern "C" __declspec(dllexport) void* get_release_addr();
arcdps_exports* mod_init();
uintptr_t mod_release();
uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void log_file(char* str);
void log_arc(char* str);
void* filelog;
void* arclog;

HCURSOR myCursor = nullptr;

#define GW2MCO_VERSION "1.0"

// ==== Hooks ====
typedef HCURSOR(WINAPI *SetCursorHook)(HCURSOR hCursor);
SetCursorHook pSetCursor = nullptr;
SetCursorHook pSetCursorTarget;
HCURSOR WINAPI detourSetCursor(HCURSOR hCursor)
{
    return pSetCursor(myCursor);
}

typedef ULONG_PTR(WINAPI *SetClassLongPtrAHook)(HWND,int,LONG_PTR);
SetClassLongPtrAHook pSetClassLongPtrA = nullptr;
SetClassLongPtrAHook pSetClassLongPtrATarget;
ULONG_PTR detourSetClassLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong)
{
    if(nIndex == GCLP_HCURSOR)
        return pSetClassLongPtrA(hWnd, nIndex, (LONG_PTR)myCursor);
    return pSetClassLongPtrA(hWnd, nIndex, dwNewLong);
}

typedef ULONG_PTR(WINAPI *SetClassLongPtrWHook)(HWND,int,LONG_PTR);
SetClassLongPtrWHook pSetClassLongPtrW = nullptr;
SetClassLongPtrWHook pSetClassLongPtrWTarget;
ULONG_PTR detourSetClassLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong)
{
    if(nIndex == GCLP_HCURSOR)
        return pSetClassLongPtrA(hWnd, nIndex, (LONG_PTR)myCursor);
    return pSetClassLongPtrW(hWnd, nIndex, dwNewLong);
}

// ==== Logging functions from ArcDps ====

void log_file(char* str)
{
    auto* log = (size_t(*)(char*))filelog;
    if(log) (*log)(str);
}

void log_arc(char* str)
{
    auto* log = (size_t(*)(char*))arclog;
    if(log) (*log)(str);
}

// ==== Image loading ====
static HICON createIcon(const std::string& filename)
{
    // Try load icon image from filesystem.
    int imageWidth, imageHeight, imageChannels;
    auto* pixels = stbi_load(filename.c_str(), &imageWidth, &imageHeight, &imageChannels, 4);

    if(!pixels)
    {
        return nullptr;
    }

    // Construct a bitmap
    BITMAPV5HEADER bitmapHeader{};
    ZeroMemory(&bitmapHeader, sizeof(bitmapHeader));

    bitmapHeader.bV5Size        = sizeof(bitmapHeader);
    bitmapHeader.bV5Width       = imageWidth;
    bitmapHeader.bV5Height      = -imageHeight;
    bitmapHeader.bV5Planes      = 1;
    bitmapHeader.bV5BitCount    = 32;
    bitmapHeader.bV5Compression = BI_BITFIELDS;
    bitmapHeader.bV5RedMask     = 0x00ff0000;
    bitmapHeader.bV5GreenMask   = 0x0000ff00;
    bitmapHeader.bV5BlueMask    = 0x000000ff;
    bitmapHeader.bV5AlphaMask   = 0xff000000;

    // Create RGBA bitmap
    HDC dc;
    unsigned char* target;
    dc = GetDC(nullptr);
    auto color = CreateDIBSection(dc, (BITMAPINFO*)&bitmapHeader, DIB_RGB_COLORS, (void**)&target, nullptr, (DWORD)0);
    ReleaseDC(nullptr, dc);

    if(!color)
    {
        stbi_image_free(pixels);
        return nullptr;
    }

    auto mask = CreateBitmap(imageWidth, imageHeight, 1, 1, nullptr);
    if(!mask)
    {
        stbi_image_free(pixels);
        DeleteObject(color);
        return nullptr;
    }

    // Copy pixels to bitmap
    unsigned char* source = pixels;
    for (int i = 0;  i < imageWidth * imageHeight;  i++)
    {
        target[0] = source[2];
        target[1] = source[1];
        target[2] = source[0];
        target[3] = source[3];
        target += 4;
        source += 4;
    }

    // Free stbi image data.
    stbi_image_free(pixels);

    // Create icon info and handle.
    ICONINFO icon;
    ZeroMemory(&icon, sizeof(icon));
    icon.fIcon    = FALSE;
    icon.xHotspot = imageWidth / 2;
    icon.yHotspot = imageHeight / 2;
    icon.hbmMask  = mask;
    icon.hbmColor = color;

    HICON handle = CreateIconIndirect(&icon);
    DeleteObject(color);
    DeleteObject(mask);

    return handle;
}

static HCURSOR loadCur(const std::string& filename)
{
    return (HCURSOR)LoadImage(nullptr, filename.c_str(), IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE);
}

static HCURSOR createCursor(const std::string& filename)
{
    if(filename.ends_with(".png"))
        return static_cast<HCURSOR>(createIcon(filename));
    return loadCur(filename);
}

// ==== Arcdps Entry point ====
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, ImGuiContext* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion)
{
    ImGui::SetCurrentContext((ImGuiContext*)imguictx);
    ImGui::SetAllocatorFunctions((void *(*)(size_t, void*))mallocfn, (void(*)(void*, void*))freefn);

    filelog = (void*)GetProcAddress((HMODULE)arcdll, "e3");
    arclog = (void*)GetProcAddress((HMODULE)arcdll, "e8");

    return reinterpret_cast<void*>(mod_init);
}

extern "C" __declspec(dllexport) void* get_release_addr()
{
    return reinterpret_cast<void*>(mod_release);
}

// ==== Extension entry point ====
arcdps_exports* mod_init()
{
    MH_Initialize();
    MH_CreateHookApiEx(L"user32", "SetCursor", reinterpret_cast<LPVOID>(&detourSetCursor), reinterpret_cast<void**>(&pSetCursor), reinterpret_cast<void**>(&pSetCursorTarget));
    MH_CreateHookApiEx(L"user32", "SetClassLongPtrA", reinterpret_cast<LPVOID>(&detourSetClassLongPtrA), reinterpret_cast<void**>(&pSetClassLongPtrA), reinterpret_cast<void**>(&pSetClassLongPtrATarget));
    MH_CreateHookApiEx(L"user32", "SetClassLongPtrW", reinterpret_cast<LPVOID>(&detourSetClassLongPtrW), reinterpret_cast<void**>(&pSetClassLongPtrW), reinterpret_cast<void**>(&pSetClassLongPtrWTarget));
    MH_EnableHook(MH_ALL_HOOKS);

    log_arc((char*)"[Cursor Override] Hooks initialized");

    // Load cursor.
    myCursor = createCursor("cursor.ani");

    // Tell arc what we are.
    memset(&arcExports, 0, sizeof(arcExports));
    arcExports.size = sizeof(arcExports);
    arcExports.sig = 0x79137910;
    arcExports.imguivers = IMGUI_VERSION_NUM;
    arcExports.out_name = "curoverride";
    arcExports.out_build = GW2MCO_VERSION;
    arcExports.wnd_nofilter = reinterpret_cast<void*>(mod_wnd);
    return &arcExports;
}

uintptr_t mod_release()
{
    pSetCursor(LoadCursor(nullptr, IDC_ARROW));
    DeleteObject(myCursor);

    // Remove all hooks.
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    log_arc((char*)"[Cursor Override] Hooks destroyed");

    return 0;
}

uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(uMsg == WM_SETCURSOR)
    {
        SetCursor(myCursor);
        return 0;
    }

    return uMsg;
}

// ==== DLL entry ====
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ulReasonForCall, LPVOID lpReserved)
{
    switch (ulReasonForCall)
    {
        case DLL_PROCESS_ATTACH:
            dll_init(hModule);
            break;
        case DLL_PROCESS_DETACH:
            dll_exit();
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        default:
            break;
    }
    return 1;
}

void dll_init(HANDLE hModule)
{
}

void dll_exit()
{
}