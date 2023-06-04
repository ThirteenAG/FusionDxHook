#define FUSIONDXHOOK_INCLUDE_D3D8     1
#define FUSIONDXHOOK_INCLUDE_D3D9     1
#define FUSIONDXHOOK_INCLUDE_D3D10    1
#define FUSIONDXHOOK_INCLUDE_D3D10_1  1
#define FUSIONDXHOOK_INCLUDE_D3D11    1
#define FUSIONDXHOOK_INCLUDE_D3D12    1
#define FUSIONDXHOOK_INCLUDE_OPENGL   1
#define FUSIONDXHOOK_INCLUDE_VULKAN   1
#define FUSIONDXHOOK_USE_MINHOOK      1

#include "FusionDxHook.h"
#include "ModuleList.hpp"
#include <iostream>
#include <fstream>

BOOL CALLBACK enumWindowCallback(HWND hWnd, LPARAM lparam) {
    auto curProcId = GetCurrentProcessId();
    DWORD winProcId = 0;
    GetWindowThreadProcessId(hWnd, &winProcId);
    if (curProcId == winProcId)
        SendMessage(hWnd, WM_CLOSE, 0, 0);
    return TRUE;
}

void CloseWindow()
{
    EnumWindows((WNDENUMPROC)enumWindowCallback, 0);
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    static std::once_flag flag;
    std::call_once(flag, []()
    {
        FusionDxHook::onInitEvent += []()
        {

        };

        FusionDxHook::Init();

#ifdef FUSIONDXHOOK_INCLUDE_D3D8
        FusionDxHook::D3D8::onInitEvent += []()
        {

        };

        FusionDxHook::D3D8::onPresentEvent += [](D3D8_LPDIRECT3DDEVICE8 pDevice)
        {
            // reinterpret_cast<LPDIRECT3DDEVICE8>(pDevice);
            std::ofstream outfile("onPresentEvent.txt");
            outfile.close();
            CloseWindow();
        };

        FusionDxHook::D3D8::onEndSceneEvent += [](D3D8_LPDIRECT3DDEVICE8 pDevice)
        {
            // reinterpret_cast<LPDIRECT3DDEVICE8>(pDevice);
        };

        FusionDxHook::D3D8::onResetEvent += [](D3D8_LPDIRECT3DDEVICE8 pDevice)
        {
            // reinterpret_cast<LPDIRECT3DDEVICE8>(pDevice);
        };

        FusionDxHook::D3D8::onReleaseEvent += []()
        {
            std::ofstream outfile("onReleaseEvent.txt");
            outfile.close();
        };

        FusionDxHook::D3D8::onShutdownEvent += []()
        {

        };
#endif // FUSIONDXHOOK_INCLUDE_D3D8

#ifdef FUSIONDXHOOK_INCLUDE_D3D9
        FusionDxHook::D3D9::onInitEvent += []()
        {

        };

        FusionDxHook::D3D9::onPresentEvent += [](LPDIRECT3DDEVICE9 pDevice)
        {
            std::ofstream outfile("onPresentEvent.txt");
            outfile.close();
            CloseWindow();
        };

        FusionDxHook::D3D9::onEndSceneEvent += [](LPDIRECT3DDEVICE9 pDevice)
        {

        };

        FusionDxHook::D3D9::onResetEvent += [](LPDIRECT3DDEVICE9 pDevice)
        {

        };

        FusionDxHook::D3D9::onReleaseEvent += []()
        {
            std::ofstream outfile("onReleaseEvent.txt");
            outfile.close();
        };

        FusionDxHook::D3D9::onShutdownEvent += []()
        {

        };
#endif // FUSIONDXHOOK_INCLUDE_D3D9

#ifdef FUSIONDXHOOK_INCLUDE_D3D10
        FusionDxHook::D3D10::onInitEvent += []()
        {

        };

        FusionDxHook::D3D10::onPresentEvent += [](IDXGISwapChain*)
        {
            std::ofstream outfile("onPresentEvent.txt");
            outfile.close();
            CloseWindow();
        };

        FusionDxHook::D3D10::onBeforeResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };

        FusionDxHook::D3D10::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };

        FusionDxHook::D3D10::onReleaseEvent += []()
        {
            std::ofstream outfile("onReleaseEvent.txt");
            outfile.close();
        };

        FusionDxHook::D3D10::onShutdownEvent += []()
        {

        };
#endif // FUSIONDXHOOK_INCLUDE_D3D10

#ifdef FUSIONDXHOOK_INCLUDE_D3D10_1
        FusionDxHook::D3D10_1::onInitEvent += []()
        {

        };

        FusionDxHook::D3D10_1::onPresentEvent += [](IDXGISwapChain*)
        {
            std::ofstream outfile("onPresentEvent.txt");
            outfile.close();
            CloseWindow();
        };

        FusionDxHook::D3D10_1::onBeforeResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };

        FusionDxHook::D3D10_1::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };

        FusionDxHook::D3D10_1::onReleaseEvent += []()
        {
            std::ofstream outfile("onReleaseEvent.txt");
            outfile.close();
        };

        FusionDxHook::D3D10_1::onShutdownEvent += []()
        {

        };
#endif // FUSIONDXHOOK_INCLUDE_D3D10_1

#ifdef FUSIONDXHOOK_INCLUDE_D3D11
        FusionDxHook::D3D11::onInitEvent += []()
        {

        };

        FusionDxHook::D3D11::onPresentEvent += [](IDXGISwapChain*)
        {
            std::ofstream outfile("onPresentEvent.txt");
            outfile.close();
            CloseWindow();
        };

        FusionDxHook::D3D11::onBeforeResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };

        FusionDxHook::D3D11::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };

        FusionDxHook::D3D11::onReleaseEvent += []()
        {
            std::ofstream outfile("onReleaseEvent.txt");
            outfile.close();
        };

        FusionDxHook::D3D11::onShutdownEvent += []()
        {

        };
#endif // FUSIONDXHOOK_INCLUDE_D3D11

#ifdef FUSIONDXHOOK_INCLUDE_D3D12
        FusionDxHook::D3D12::onInitEvent += []()
        {

        };

        FusionDxHook::D3D12::onPresentEvent += [](IDXGISwapChain*)
        {
            std::ofstream outfile("onPresentEvent.txt");
            outfile.close();
            CloseWindow();
        };

        FusionDxHook::D3D12::onBeforeResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };

        FusionDxHook::D3D12::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };

        FusionDxHook::D3D12::onReleaseEvent += []()
        {
            std::ofstream outfile("onReleaseEvent.txt");
            outfile.close();
        };

        FusionDxHook::D3D12::onShutdownEvent += []()
        {

        };
#endif // FUSIONDXHOOK_INCLUDE_D3D12

#ifdef FUSIONDXHOOK_INCLUDE_OPENGL
        FusionDxHook::OPENGL::onInitEvent += []()
        {

        };

        FusionDxHook::OPENGL::onSwapBuffersEvent += [](HDC hDC)
        {
            std::ofstream outfile("onSwapBuffersEvent.txt");
            outfile.close();
            TerminateProcess(GetCurrentProcess(), 0);
        };

        FusionDxHook::OPENGL::onShutdownEvent += []()
        {

        };
#endif // FUSIONDXHOOK_INCLUDE_OPENGL

#ifdef FUSIONDXHOOK_INCLUDE_VULKAN
        FusionDxHook::VULKAN::onInitEvent += []()
        {

        };

        FusionDxHook::VULKAN::onvkCreateDeviceEvent += [](VkPhysicalDevice gpu, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
        {

        };

        FusionDxHook::VULKAN::onVkQueuePresentKHREvent += [](VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
        {
            std::ofstream outfile("onVkQueuePresentKHREvent.txt");
            outfile.close();
            CloseWindow();
        };

        FusionDxHook::VULKAN::onShutdownEvent += []()
        {

        };
#endif // FUSIONDXHOOK_INCLUDE_VULKAN

        FusionDxHook::onShutdownEvent += []()
        {

        };
    });
}

bool IsUALPresent()
{
    ModuleList dlls;
    dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
    for (auto& e : dlls.m_moduleList)
    {
        if (GetProcAddress(std::get<HMODULE>(e), "DirectInput8Create") != NULL &&
            GetProcAddress(std::get<HMODULE>(e), "DirectSoundCreate8") != NULL && 
            GetProcAddress(std::get<HMODULE>(e), "InternetOpenA") != NULL)
            return true;
    }
    return false;
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID)
{
    DisableThreadLibraryCalls(hInstance);

    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        if (!IsUALPresent()) { InitializeASI(); }
        break;
    case DLL_PROCESS_DETACH:
        FusionDxHook::DeInit();
        break;
    }

    return TRUE;
}