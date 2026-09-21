#pragma once
#ifndef __FUSIONDXHOOK_H__
#define __FUSIONDXHOOK_H__
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <SubAuth.h>
#include <stdint.h>
#include <map>
#include <thread>
#include <mutex>
#include <numeric>
#include <algorithm>
#include <functional>
#include <typeindex>
#include <typeinfo>
#include "methods.h"

//#define FUSIONDXHOOK_INCLUDE_D3D8     1
//#define FUSIONDXHOOK_INCLUDE_D3D9     1
//#define FUSIONDXHOOK_INCLUDE_D3D10    1
//#define FUSIONDXHOOK_INCLUDE_D3D10_1  1
//#define FUSIONDXHOOK_INCLUDE_D3D11    1
//#define FUSIONDXHOOK_INCLUDE_D3D12    1
//#define FUSIONDXHOOK_INCLUDE_OPENGL   1
//#define FUSIONDXHOOK_INCLUDE_VULKAN   1
//#define FUSIONDXHOOK_USE_SAFETYHOOK   1
//#define DELAYED_BIND 2000ms

#ifdef DELAYED_BIND
#define _DELAYED_BIND using namespace std::chrono_literals; std::this_thread::sleep_for(DELAYED_BIND);
#else
#define _DELAYED_BIND
#endif // DELAYED_BIND

#if FUSIONDXHOOK_INCLUDE_D3D8
//to avoid conflicts with dx9 sdk
#include "minidx8.h"
#endif

#if FUSIONDXHOOK_INCLUDE_D3D9
#include <d3d9.h>
#endif

#if FUSIONDXHOOK_INCLUDE_D3D10 || FUSIONDXHOOK_INCLUDE_D3D10_1
#include <dxgi.h>
#include <d3d10_1.h>
#include <d3d10.h>
#endif

#if FUSIONDXHOOK_INCLUDE_D3D11
#include <dxgi.h>
#include <d3d11.h>
#endif

#if FUSIONDXHOOK_INCLUDE_D3D12
#include <dxgi.h>
#include <d3d12.h>
#endif

#if FUSIONDXHOOK_INCLUDE_OPENGL
#include <gl/GL.h>
#endif

#if FUSIONDXHOOK_INCLUDE_VULKAN
#ifndef VULKAN_H_
typedef void* VkQueue;
typedef void* VkPhysicalDevice;
typedef void VkDeviceCreateInfo;
typedef void VkAllocationCallbacks;
typedef void* VkDevice;
typedef void VkPresentInfoKHR;
typedef void VkSwapchainCreateInfoKHR;
typedef void* VkSwapchainKHR;
typedef enum VkResult {
    VK_SUCCESS = 0,
    VK_ERROR_DEVICE_LOST = -4
} VkResult;
#if defined(_WIN32)
#define VKAPI_CALL __stdcall
#else
#define VKAPI_CALL
#endif
typedef VkResult(VKAPI_CALL* PFN_vkGetDeviceProcAddr)(VkDevice, const char*);
//#include <vulkan/vulkan.h>
#endif
#endif

#if FUSIONDXHOOK_USE_SAFETYHOOK
#if !defined(SAFETYHOOK_ARCH_X86_32) && !defined(SAFETYHOOK_ARCH_X86_64)
#include "safetyhook/safetyhook.hpp"
#endif
#endif


class FusionDxHook
{
private:
    static inline std::map<const HMODULE, std::map<std::type_index, std::vector<uintptr_t*>>> deviceMethods;

    class HookWindow
    {
    public:
        HookWindow(std::wstring_view className = L"FusionDxHook", std::wstring windowName = L"FusionDxHook")
        {
            windowClass.cbSize = sizeof(WNDCLASSEX);
            windowClass.style = CS_HREDRAW | CS_VREDRAW;
            windowClass.lpfnWndProc = DefWindowProc;
            windowClass.cbClsExtra = 0;
            windowClass.cbWndExtra = 0;
            windowClass.hInstance = GetModuleHandle(NULL);
            windowClass.hIcon = NULL;
            windowClass.hCursor = NULL;
            windowClass.hbrBackground = NULL;
            windowClass.lpszMenuName = NULL;
            windowClass.lpszClassName = className.data();
            windowClass.hIconSm = NULL;
            RegisterClassExW(&windowClass);
            window = CreateWindowW(windowClass.lpszClassName, windowName.data(), WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, windowClass.hInstance, NULL);
        }
        ~HookWindow()
        {
            DestroyWindow(window);
            UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        }
        static HWND GetHookWindow(HookWindow& hw)
        {
            return hw.window;
        }
    private:
        WNDCLASSEX windowClass{};
        HWND window{};
    };

    template <class... Ts>
    static inline void copyMethods(HMODULE mod, Ts&& ... inputs)
    {
        ([&] {
            auto vtbl_info = std::get<0>(inputs);
            auto ptr = std::get<1>(inputs);
            deviceMethods[mod][typeid(vtbl_info)].resize(vtbl_info.GetNumberOfMethods());
            std::memcpy(deviceMethods.at(mod).at(typeid(vtbl_info)).data(), ptr, vtbl_info.GetNumberOfMethods() * sizeof(uintptr_t));
        } (), ...);
    }

    template<typename... Args>
    class Event : public std::function<void(Args...)>
    {
    public:
        using std::function<void(Args...)>::function;

    private:
        std::vector<std::function<void(Args...)>> handlers;

    public:
        void operator+=(std::function<void(Args...)> handler)
        {
            handlers.push_back(handler);
        }

        void operator()(Args... args) const
        {
            if (!handlers.empty())
            {
                for (auto& handler : handlers)
                {
                    handler(args...);
                }
            }
        }
    };

public:
    static inline Event<> onInitEvent = {};
    static inline Event<> onShutdownEvent = {};

#if FUSIONDXHOOK_INCLUDE_D3D8
public:
    struct D3D8 {
        static inline Event<> onInitEvent = {};
        static inline Event<> onShutdownEvent = {};
        static inline Event<D3D8_LPDIRECT3DDEVICE8> onPresentEvent = {};
        static inline Event<D3D8_LPDIRECT3DDEVICE8> onResetEvent = {};
        static inline Event<D3D8_LPDIRECT3DDEVICE8> onEndSceneEvent = {};
        static inline Event<> onReleaseEvent = {};
    };
private:
    static inline void HookD3D8()
    {
        DllCallbackHandler::RegisterUnloadCallback(L"d3d8.dll", []() { D3D8::onShutdownEvent(); });

        auto hD3D8 = GetModuleHandleW(L"d3d8.dll");
        if (!hD3D8) return;
        HMODULE hD3D8Guard = LoadLibraryW(L"d3d8.dll");
        auto Direct3DCreate8 = GetProcAddress(hD3D8, "Direct3DCreate8");
        if (!Direct3DCreate8) return;
        auto Direct3D8 = ((D3D8_LPDIRECT3D8(WINAPI*)(uint32_t))(Direct3DCreate8))(220);
        if (!Direct3D8) return;

        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
            static constexpr auto D3D8_D3DADAPTER_DEFAULT = 0;
            static constexpr auto D3D8_D3DCREATE_SOFTWARE_VERTEXPROCESSING = 0x00000020L;
            static constexpr auto D3D8_D3DCREATE_DISABLE_DRIVER_MANAGEMENT = 0x00000100L;

            auto hookWindowCtor = HookWindow(L"FusionDxHookD3D8", L"FusionDxHookD3D8");
            auto hWnd = HookWindow::GetHookWindow(hookWindowCtor);

            D3DDISPLAYMODE_D3D8 ds{};
            Direct3D8->GetAdapterDisplayMode(D3D8_D3DADAPTER_DEFAULT, &ds);

            D3DPRESENT_PARAMETERS_D3D8 params;
            params.BackBufferWidth = 0;
            params.BackBufferHeight = 0;
            params.BackBufferFormat = ds.Format;
            params.BackBufferCount = 0;
            params.MultiSampleType = D3D8_D3DMULTISAMPLE_NONE;
            params.SwapEffect = D3D8_D3DSWAPEFFECT_DISCARD;
            params.hDeviceWindow = hWnd;
            params.Windowed = 1;
            params.EnableAutoDepthStencil = 0;
            params.Flags = 0;
            params.FullScreen_RefreshRateInHz = 0;
            params.FullScreen_PresentationInterval = 0;

            D3D8_LPDIRECT3DDEVICE8 Device;
            if (Direct3D8->CreateDevice(D3DADAPTER_DEFAULT, D3D8_D3DDEVTYPE_REF, hWnd, DWORD(D3D8_D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3D8_D3DCREATE_DISABLE_DRIVER_MANAGEMENT), &params, (void**)&Device) >= 0)
            {
                copyMethods(hD3D8,
                    std::forward_as_tuple(IDirect3DDevice8VTBL(), *(uintptr_t**)Device)
                );

                #if FUSIONDXHOOK_USE_SAFETYHOOK
                static SafetyHookInline presentOriginal = {};
                static SafetyHookInline resetOriginal = {};
                static SafetyHookInline endSceneOriginal = {};
                static SafetyHookInline releaseOriginal = {};

                auto D3D8Present = [](D3D8_LPDIRECT3DDEVICE8 pDevice, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion) -> HRESULT
                {
                    D3D8::onPresentEvent(pDevice);
                    return presentOriginal.unsafe_stdcall<HRESULT>(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
                };

                auto D3D8Reset = [](D3D8_LPDIRECT3DDEVICE8 pDevice, D3DPRESENT_PARAMETERS_D3D8* pPresentationParameters) -> HRESULT
                {
                    D3D8::onResetEvent(pDevice);
                    return resetOriginal.unsafe_stdcall<HRESULT>(pDevice, pPresentationParameters);
                };

                auto D3D8EndScene = [](D3D8_LPDIRECT3DDEVICE8 pDevice) -> HRESULT
                {
                    D3D8::onEndSceneEvent(pDevice);
                    return endSceneOriginal.unsafe_stdcall<HRESULT>(pDevice);
                };

                auto D3D8Release = [](IUnknown* ptr) -> ULONG
                {
                    struct __declspec(uuid("7385e5df-8fe8-41d5-86b6-d7b48547b6cf")) uuidIDirect3DDevice8;
                    IUnknown* pDevice = nullptr;
                    if (ptr->QueryInterface(__uuidof(uuidIDirect3DDevice8), (void**)&pDevice) == S_OK)
                    {
                        auto ref_count = releaseOriginal.unsafe_stdcall<ULONG>(pDevice);
                        if (pDevice == ptr && ref_count == 1)
                            D3D8::onReleaseEvent();
                    }
                    return releaseOriginal.unsafe_stdcall<ULONG>(ptr);
                };

                static HRESULT(WINAPI* Present)(D3D8_LPDIRECT3DDEVICE8, CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*) = D3D8Present;
                static HRESULT(WINAPI* Reset)(D3D8_LPDIRECT3DDEVICE8, D3DPRESENT_PARAMETERS_D3D8*) = D3D8Reset;
                static HRESULT(WINAPI* EndScene)(D3D8_LPDIRECT3DDEVICE8 pDevice) = D3D8EndScene;
                static ULONG(WINAPI* Release)(IUnknown*) = D3D8Release;

                _DELAYED_BIND

                bind(hD3D8, typeid(IDirect3DDevice8VTBL), IDirect3DDevice8VTBL().GetIndex("Present"), Present, presentOriginal);
                bind(hD3D8, typeid(IDirect3DDevice8VTBL), IDirect3DDevice8VTBL().GetIndex("Reset"), Reset, resetOriginal);
                bind(hD3D8, typeid(IDirect3DDevice8VTBL), IDirect3DDevice8VTBL().GetIndex("EndScene"), EndScene, endSceneOriginal);
                bind(hD3D8, typeid(IDirect3DDevice8VTBL), IDirect3DDevice8VTBL().GetIndex("Release"), Release, releaseOriginal);
                #endif
                Device->Release();
            }
            Direct3D8->Release();
        });

        if (hD3D8Guard)
            FreeLibrary(hD3D8Guard);
    }
#else
    static inline void HookD3D8() {}
#endif

#if FUSIONDXHOOK_INCLUDE_D3D9
public:
    struct D3D9 {
        static inline Event<> onInitEvent = {};
        static inline Event<> onShutdownEvent = {};
        static inline Event<LPDIRECT3DDEVICE9> onPresentEvent = {};
        static inline Event<LPDIRECT3DDEVICE9> onResetEvent = {};
        static inline Event<LPDIRECT3DDEVICE9> onEndSceneEvent = {};
        static inline Event<> onReleaseEvent = {};
    };
private:
    static inline void HookD3D9()
    {
        DllCallbackHandler::RegisterUnloadCallback(L"d3d9.dll", []() { D3D9::onShutdownEvent(); });

        auto hD3D9 = GetModuleHandleW(L"d3d9.dll");
        if (!hD3D9) return;
        HMODULE hD3D9Guard = LoadLibraryW(L"d3d9.dll");
        auto Direct3DCreate9 = GetProcAddress(hD3D9, "Direct3DCreate9");
        if (!Direct3DCreate9) return;
        auto Direct3D9 = ((LPDIRECT3D9(WINAPI*)(uint32_t))(Direct3DCreate9))(D3D_SDK_VERSION);
        if (!Direct3D9) return;

        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
            auto hookWindowCtor = HookWindow(L"FusionDxHookD3D9", L"FusionDxHookD3D9");
            auto hWnd = HookWindow::GetHookWindow(hookWindowCtor);

            D3DPRESENT_PARAMETERS params;
            params.BackBufferWidth = 0;
            params.BackBufferHeight = 0;
            params.BackBufferFormat = D3DFMT_UNKNOWN;
            params.BackBufferCount = 0;
            params.MultiSampleType = D3DMULTISAMPLE_NONE;
            params.MultiSampleQuality = NULL;
            params.SwapEffect = D3DSWAPEFFECT_DISCARD;
            params.hDeviceWindow = hWnd;
            params.Windowed = 1;
            params.EnableAutoDepthStencil = 0;
            params.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
            params.Flags = NULL;
            params.FullScreen_RefreshRateInHz = 0;
            params.PresentationInterval = 0;

            LPDIRECT3DDEVICE9 Device;
            if (Direct3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT, &params, &Device) >= 0)
            {
                copyMethods(hD3D9,
                    std::forward_as_tuple(IDirect3DDevice9VTBL(), *(uintptr_t**)Device)
                );

                #if FUSIONDXHOOK_USE_SAFETYHOOK
                static SafetyHookInline presentOriginal = {};
                static SafetyHookInline presentExOriginal = {};
                static SafetyHookInline resetOriginal = {};
                static SafetyHookInline resetExOriginal = {};
                static SafetyHookInline endSceneOriginal = {};
                static SafetyHookInline releaseOriginal = {};

                auto D3D9Present = [](LPDIRECT3DDEVICE9 pDevice, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion) -> HRESULT
                {
                    D3D9::onPresentEvent(pDevice);
                    return presentOriginal.unsafe_stdcall<HRESULT>(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
                };

                auto D3D9PresentEx = [](LPDIRECT3DDEVICE9EX pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, DWORD dwFlags) -> HRESULT
                {
                    D3D9::onPresentEvent(pDevice);
                    return presentExOriginal.unsafe_stdcall<HRESULT>(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
                };

                auto D3D9Reset = [](LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) -> HRESULT
                {
                    D3D9::onResetEvent(pDevice);
                    return resetOriginal.unsafe_stdcall<HRESULT>(pDevice, pPresentationParameters);
                };

                auto D3D9ResetEx = [](LPDIRECT3DDEVICE9EX pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode) -> HRESULT
                {
                    D3D9::onResetEvent(pDevice);
                    return resetExOriginal.unsafe_stdcall<HRESULT>(pDevice, pPresentationParameters, pFullscreenDisplayMode);
                };

                auto D3D9EndScene = [](LPDIRECT3DDEVICE9 pDevice) -> HRESULT
                {
                    D3D9::onEndSceneEvent(pDevice);
                    return endSceneOriginal.unsafe_stdcall<HRESULT>(pDevice);
                };

                auto D3D9Release = [](IUnknown* ptr) -> ULONG
                {
                    IUnknown* pDevice = nullptr;
                    if (ptr->QueryInterface(__uuidof(IDirect3DDevice9), (void**)&pDevice) == S_OK)
                    {
                        auto ref_count = releaseOriginal.unsafe_stdcall<ULONG>(pDevice);
                        if (pDevice == ptr && ref_count == 1)
                            D3D9::onReleaseEvent();
                    }
                    return releaseOriginal.unsafe_stdcall<ULONG>(ptr);
                };

                static HRESULT(WINAPI* Present)(LPDIRECT3DDEVICE9, CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*) = D3D9Present;
                static HRESULT(WINAPI* PresentEx)(LPDIRECT3DDEVICE9EX, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD) = D3D9PresentEx;
                static HRESULT(WINAPI* Reset)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*) = D3D9Reset;
                static HRESULT(WINAPI* ResetEx)(LPDIRECT3DDEVICE9EX, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*) = D3D9ResetEx;
                static HRESULT(WINAPI* EndScene)(LPDIRECT3DDEVICE9) = D3D9EndScene;
                static ULONG(WINAPI* Release)(IUnknown*) = D3D9Release;

                _DELAYED_BIND

                hD3D9 = GetModuleHandleW(L"d3d9.dll");
                if (!hD3D9) return;

                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("Present"), Present, presentOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("PresentEx"), PresentEx, presentExOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("Reset"), Reset, resetOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("ResetEx"), ResetEx, resetExOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("EndScene"), EndScene, endSceneOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("Release"), Release, releaseOriginal);
                #endif
                Device->Release();
            }
            Direct3D9->Release();
        });

        if (hD3D9Guard)
            FreeLibrary(hD3D9Guard);
    }
#else
    static inline void HookD3D9() {}
#endif

#if FUSIONDXHOOK_INCLUDE_D3D10
public:
    struct D3D10 {
        static inline Event<> onInitEvent = {};
        static inline Event<> onShutdownEvent = {};
        static inline Event<IDXGISwapChain*> onPresentEvent = {};
        static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onBeforeResizeEvent = {};
        static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onAfterResizeEvent = {};
        static inline Event<> onReleaseEvent = {};
    };
private:
    static inline void HookD3D10()
    {
        DllCallbackHandler::RegisterUnloadCallback(L"d3d10.dll", []() { D3D10::onShutdownEvent(); });

        auto hD3D10 = GetModuleHandleW(L"d3d10.dll");
        if (!hD3D10) return;
        HMODULE hD3D10Guard = LoadLibraryW(L"d3d10.dll");
        bool isDXGILoaded = false;
        auto hDXGI = GetModuleHandleW(L"dxgi.dll");
        if (!hDXGI)
            hDXGI = LoadLibraryW(L"dxgi.dll");
        else
            isDXGILoaded = true;

        if (!hDXGI) return;

        auto CreateDXGIFactory = GetProcAddress(hDXGI, "CreateDXGIFactory");
        if (!CreateDXGIFactory) return;

        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
            IDXGIFactory* Factory;
            if (((HRESULT(WINAPI*)(const IID&, void**))(CreateDXGIFactory))(__uuidof(IDXGIFactory), (void**)&Factory) < 0)
                return;

            IDXGIAdapter* Adapter;
            if (Factory->EnumAdapters(0, &Adapter) == DXGI_ERROR_NOT_FOUND)
                return;

            auto D3D10CreateDeviceAndSwapChain = GetProcAddress(hD3D10, "D3D10CreateDeviceAndSwapChain");
            if (D3D10CreateDeviceAndSwapChain == NULL)
                return;

            auto hookWindowCtor = HookWindow(L"FusionDxHookD3D10", L"FusionDxHookD3D10");
            auto hWnd = HookWindow::GetHookWindow(hookWindowCtor);

            DXGI_RATIONAL refreshRate;
            refreshRate.Numerator = 60;
            refreshRate.Denominator = 1;

            DXGI_MODE_DESC bufferDesc;
            bufferDesc.Width = 100;
            bufferDesc.Height = 100;
            bufferDesc.RefreshRate = refreshRate;
            bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
            bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

            DXGI_SAMPLE_DESC sampleDesc;
            sampleDesc.Count = 1;
            sampleDesc.Quality = 0;

            DXGI_SWAP_CHAIN_DESC swapChainDesc;
            swapChainDesc.BufferDesc = bufferDesc;
            swapChainDesc.SampleDesc = sampleDesc;
            swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapChainDesc.BufferCount = 1;
            swapChainDesc.OutputWindow = hWnd;
            swapChainDesc.Windowed = 1;
            swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

            ID3D10Device* Device;
            IDXGISwapChain* SwapChain;
            if (((HRESULT(WINAPI*)(IDXGIAdapter*, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D10Device**))
                (D3D10CreateDeviceAndSwapChain))(Adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &swapChainDesc, &SwapChain, &Device) >= 0)
            {
                copyMethods(hD3D10,
                    std::forward_as_tuple(IDXGISwapChainVTBL(), *(uintptr_t**)SwapChain),
                    std::forward_as_tuple(ID3D10DeviceVTBL(), *(uintptr_t**)Device)
                );

                #if FUSIONDXHOOK_USE_SAFETYHOOK
                static SafetyHookInline presentOriginal = {};
                static SafetyHookInline resizeBuffersOriginal = {};
                static SafetyHookInline releaseOriginal = {};

                auto D3D10Present = [](IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) -> HRESULT
                {
                    D3D10::onPresentEvent(pSwapChain);
                    return presentOriginal.unsafe_stdcall<HRESULT>(pSwapChain, SyncInterval, Flags);
                };

                auto D3D10ResizeBuffers = [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) -> HRESULT
                {
                    D3D10::onBeforeResizeEvent(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                    HRESULT result = resizeBuffersOriginal.unsafe_stdcall<HRESULT>(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                    D3D10::onAfterResizeEvent(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                    return result;
                };

                auto D3D10Release = [](IUnknown* ptr) -> ULONG
                {
                    IUnknown* pSwapChain = nullptr;
                    if (ptr->QueryInterface(__uuidof(IDXGISwapChain), (void**)&pSwapChain) == S_OK)
                    {
                        auto ref_count = releaseOriginal.unsafe_stdcall<ULONG>(pSwapChain);
                        if (pSwapChain == ptr && ref_count == 1)
                            D3D10::onReleaseEvent();
                    }
                    return releaseOriginal.unsafe_stdcall<ULONG>(ptr);
                };

                static HRESULT(WINAPI* Present)(IDXGISwapChain*, UINT, UINT) = D3D10Present;
                static HRESULT(WINAPI* ResizeBuffers)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) = D3D10ResizeBuffers;
                static ULONG(WINAPI* Release)(IUnknown*) = D3D10Release;

                _DELAYED_BIND

                hD3D10 = GetModuleHandleW(L"d3d10.dll");
                if (!hD3D10) return;

                bind(hD3D10, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("Present"), Present, presentOriginal);
                bind(hD3D10, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("ResizeBuffers"), ResizeBuffers, resizeBuffersOriginal);
                bind(hD3D10, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("Release"), Release, releaseOriginal);
                #endif
                Device->Release();
                SwapChain->Release();
            }
            Factory->Release();

            if (!isDXGILoaded)
                FreeLibrary(hDXGI);
        });

        if (hD3D10Guard)
            FreeLibrary(hD3D10Guard);
    }
#else
    static inline void HookD3D10() {}
#endif

#if FUSIONDXHOOK_INCLUDE_D3D10_1
public:
    struct D3D10_1 {
        static inline Event<> onInitEvent = {};
        static inline Event<> onShutdownEvent = {};
        static inline Event<IDXGISwapChain*> onPresentEvent = {};
        static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onBeforeResizeEvent = {};
        static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onAfterResizeEvent = {};
        static inline Event<> onReleaseEvent = {};
    };
private:
    static inline void HookD3D10_1()
    {
        DllCallbackHandler::RegisterUnloadCallback(L"d3d10_1.dll", []() { D3D10_1::onShutdownEvent(); });

        auto hD3D10_1 = GetModuleHandleW(L"d3d10_1.dll");
        if (!hD3D10_1) return;
        HMODULE hD3D10_1Guard = LoadLibraryW(L"d3d10_1.dll");
        bool isDXGILoaded = false;
        auto hDXGI = GetModuleHandleW(L"dxgi.dll");
        if (!hDXGI)
            hDXGI = LoadLibraryW(L"dxgi.dll");
        else
            isDXGILoaded = true;

        if (!hDXGI) return;

        auto CreateDXGIFactory = GetProcAddress(hDXGI, "CreateDXGIFactory");
        if (!CreateDXGIFactory) return;

        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
            IDXGIFactory* Factory;
            if (((HRESULT(WINAPI*)(const IID&, void**))(CreateDXGIFactory))(__uuidof(IDXGIFactory), (void**)&Factory) < 0)
                return;

            IDXGIAdapter* Adapter;
            if (Factory->EnumAdapters(0, &Adapter) == DXGI_ERROR_NOT_FOUND)
                return;

            auto D3D10CreateDeviceAndSwapChain1 = GetProcAddress(hD3D10_1, "D3D10CreateDeviceAndSwapChain1");
            if (D3D10CreateDeviceAndSwapChain1 == NULL)
                return;

            auto hookWindowCtor = HookWindow(L"FusionDxHookD3D10_1", L"FusionDxHookD3D10_1");
            auto hWnd = HookWindow::GetHookWindow(hookWindowCtor);

            DXGI_RATIONAL refreshRate;
            refreshRate.Numerator = 60;
            refreshRate.Denominator = 1;

            DXGI_MODE_DESC bufferDesc;
            bufferDesc.Width = 100;
            bufferDesc.Height = 100;
            bufferDesc.RefreshRate = refreshRate;
            bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
            bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

            DXGI_SAMPLE_DESC sampleDesc;
            sampleDesc.Count = 1;
            sampleDesc.Quality = 0;

            DXGI_SWAP_CHAIN_DESC swapChainDesc;
            swapChainDesc.BufferDesc = bufferDesc;
            swapChainDesc.SampleDesc = sampleDesc;
            swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapChainDesc.BufferCount = 1;
            swapChainDesc.OutputWindow = hWnd;
            swapChainDesc.Windowed = 1;
            swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

            ID3D10Device1* Device;
            IDXGISwapChain* SwapChain;
            if (((HRESULT(WINAPI*)(IDXGIAdapter * pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC * pSwapChainDesc, IDXGISwapChain * *ppSwapChain, ID3D10Device1 * *ppDevice))
                (D3D10CreateDeviceAndSwapChain1))(Adapter, D3D10_DRIVER_TYPE_HARDWARE, nullptr, 0, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &swapChainDesc, &SwapChain, &Device) >= 0)
            {
                copyMethods(hD3D10_1,
                    std::forward_as_tuple(ID3D10DeviceVTBL(), *(uintptr_t**)Device),
                    std::forward_as_tuple(IDXGISwapChainVTBL(), *(uintptr_t**)SwapChain)
                );

                #if FUSIONDXHOOK_USE_SAFETYHOOK
                static SafetyHookInline presentOriginal = {};
                static SafetyHookInline resizeBuffersOriginal = {};
                static SafetyHookInline releaseOriginal = {};

                auto D3D10_1Present = [](IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) -> HRESULT
                {
                    D3D10_1::onPresentEvent(pSwapChain);
                    return presentOriginal.unsafe_stdcall<HRESULT>(pSwapChain, SyncInterval, Flags);
                };

                auto D3D10_1ResizeBuffers = [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) -> HRESULT
                {
                    D3D10_1::onBeforeResizeEvent(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                    HRESULT result = resizeBuffersOriginal.unsafe_stdcall<HRESULT>(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                    D3D10_1::onAfterResizeEvent(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                    return result;
                };

                auto D3D10_1Release = [](IUnknown* ptr) -> ULONG
                {
                    IUnknown* pSwapChain = nullptr;
                    if (ptr->QueryInterface(__uuidof(IDXGISwapChain), (void**)&pSwapChain) == S_OK)
                    {
                        auto ref_count = releaseOriginal.unsafe_stdcall<ULONG>(pSwapChain);
                        if (pSwapChain == ptr && ref_count == 1)
                            D3D10_1::onReleaseEvent();
                    }
                    return releaseOriginal.unsafe_stdcall<ULONG>(ptr);
                };

                static HRESULT(WINAPI* Present)(IDXGISwapChain*, UINT, UINT) = D3D10_1Present;
                static HRESULT(WINAPI* ResizeBuffers)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) = D3D10_1ResizeBuffers;
                static ULONG(WINAPI* Release)(IUnknown*) = D3D10_1Release;

                _DELAYED_BIND

                hD3D10_1 = GetModuleHandleW(L"d3d10_1.dll");
                if (!hD3D10_1) return;

                bind(hD3D10_1, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("Present"), Present, presentOriginal);
                bind(hD3D10_1, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("ResizeBuffers"), ResizeBuffers, resizeBuffersOriginal);
                bind(hD3D10_1, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("Release"), Release, releaseOriginal);
                #endif
                Device->Release();
                SwapChain->Release();
            }
            Factory->Release();

            if (!isDXGILoaded)
                FreeLibrary(hDXGI);
        });

        if (hD3D10_1Guard)
            FreeLibrary(hD3D10_1Guard);
    }
#else
    static inline void HookD3D10_1() {}
#endif

#if FUSIONDXHOOK_INCLUDE_D3D11
private:
    // The device the hook library makes for itself, see HookD3D11, which is what the
    // context of the game is told apart from, see InstallD3D11ContextHooks. The flag is
    // set for as long as that device is being made, since the context of it is seen
    // before the device is, and it belongs to the thread that makes it.
    static inline ID3D11Device* ownD3D11Device = nullptr;
    static inline thread_local bool bOwnD3D11DeviceCreation = false;
public:
    struct D3D11 {
        static inline Event<> onInitEvent = {};
        static inline Event<> onShutdownEvent = {};
        static inline Event<IDXGISwapChain*> onPresentEvent = {};
        static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onBeforeResizeEvent = {};
        static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onAfterResizeEvent = {};
        static inline Event<> onReleaseEvent = {};
    };
private:
    static inline void HookD3D11()
    {
        DllCallbackHandler::RegisterUnloadCallback(L"d3d11.dll", []() { D3D11::onShutdownEvent(); });

        auto hD3D11 = GetModuleHandleW(L"d3d11.dll");
        if (!hD3D11) return;
        HMODULE hD3D11Guard = LoadLibraryW(L"d3d11.dll");
        auto D3D11CreateDeviceAndSwapChain = GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain");
        if (D3D11CreateDeviceAndSwapChain == NULL)
            return;

        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
            // What the discovery is for is the addresses of the methods of the
            // interfaces, which are what the hooks of the library are put on, and there
            // are no such hooks to put on anything when safetyhook is off.
#if FUSIONDXHOOK_USE_SAFETYHOOK
            auto hookWindowCtor = HookWindow(L"FusionDxHookD3D11", L"FusionDxHookD3D11");
            auto hWnd = HookWindow::GetHookWindow(hookWindowCtor);

            D3D_FEATURE_LEVEL featureLevel;
            const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_11_0 };

            DXGI_RATIONAL refreshRate;
            refreshRate.Numerator = 60;
            refreshRate.Denominator = 1;

            DXGI_MODE_DESC bufferDesc;
            bufferDesc.Width = 100;
            bufferDesc.Height = 100;
            bufferDesc.RefreshRate = refreshRate;
            bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
            bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

            DXGI_SAMPLE_DESC sampleDesc;
            sampleDesc.Count = 1;
            sampleDesc.Quality = 0;

            DXGI_SWAP_CHAIN_DESC swapChainDesc;
            swapChainDesc.BufferDesc = bufferDesc;
            swapChainDesc.SampleDesc = sampleDesc;
            swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapChainDesc.BufferCount = 1;
            swapChainDesc.OutputWindow = hWnd;
            swapChainDesc.Windowed = 1;
            swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

            ID3D11Device* Device;
            ID3D11DeviceContext* Context;
            IDXGISwapChain* SwapChain;

            bOwnD3D11DeviceCreation = true;
            const HRESULT ownDeviceResult = ((HRESULT(WINAPI*)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**))
                (D3D11CreateDeviceAndSwapChain))(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevels, 2, D3D11_SDK_VERSION, &swapChainDesc, &SwapChain, &Device, &featureLevel, &Context);
            bOwnD3D11DeviceCreation = false;

            if (ownDeviceResult >= 0)
            {
                copyMethods(hD3D11,
                    std::forward_as_tuple(ID3D11DeviceVTBL(), *(uintptr_t**)Device),
                    std::forward_as_tuple(ID3D11DeviceContextVTBL(), *(uintptr_t**)Context),
                    std::forward_as_tuple(IDXGISwapChainVTBL(), *(uintptr_t**)SwapChain)
                );

                #if FUSIONDXHOOK_USE_SAFETYHOOK
                static SafetyHookInline presentOriginal = {};
                static SafetyHookInline resizeBuffersOriginal = {};
                static SafetyHookInline releaseOriginal = {};

                auto D3D11Present = [](IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) -> HRESULT
                {
                    // the hook of Direct3D 12 sits on the same function, this one
                    // only reports the frames of a Direct3D 11 swap chain
                    if (GetSwapChainKind(pSwapChain) != SwapChainKind::Direct3D12)
                    {
                        D3D11::onPresentEvent(pSwapChain);
                    }

                    return presentOriginal.unsafe_stdcall<HRESULT>(pSwapChain, SyncInterval, Flags);
                };

                auto D3D11ResizeBuffers = [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) -> HRESULT
                {
                    D3D11::onBeforeResizeEvent(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                    HRESULT result = resizeBuffersOriginal.unsafe_stdcall<HRESULT>(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                    D3D11::onAfterResizeEvent(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                    return result;
                };

                auto D3D11Release = [](IUnknown* ptr) -> ULONG
                {
                    IUnknown* pSwapChain = nullptr;
                    if (ptr->QueryInterface(__uuidof(IDXGISwapChain), (void**)&pSwapChain) == S_OK)
                    {
                        auto ref_count = releaseOriginal.unsafe_stdcall<ULONG>(pSwapChain);
                        if (pSwapChain == ptr && ref_count == 1)
                            D3D11::onReleaseEvent();
                    }
                    return releaseOriginal.unsafe_stdcall<ULONG>(ptr);
                };

                static HRESULT(WINAPI* Present)(IDXGISwapChain*, UINT, UINT) = D3D11Present;
                static HRESULT(WINAPI* ResizeBuffers)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) = D3D11ResizeBuffers;
                static ULONG(WINAPI* Release)(IUnknown*) = D3D11Release;

                _DELAYED_BIND

                hD3D11 = GetModuleHandleW(L"d3d11.dll");
                if (!hD3D11) return;

                bind(hD3D11, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("Present"), Present, presentOriginal);
                bind(hD3D11, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("ResizeBuffers"), ResizeBuffers, resizeBuffersOriginal);
                bind(hD3D11, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("Release"), Release, releaseOriginal);
                #endif
                ownD3D11Device = Device;
                ownD3D11Device->AddRef();

                Context->Release();
                Device->Release();
                SwapChain->Release();
            }
#endif
        });

        if (hD3D11Guard)
            FreeLibrary(hD3D11Guard);
    }
#else
    static inline void HookD3D11() {}
#endif

#if FUSIONDXHOOK_INCLUDE_D3D12
private:
    static inline ptrdiff_t commandQueueOffset = 0;
public:
    struct D3D12 {
        static inline Event<> onInitEvent = {};
        static inline Event<> onShutdownEvent = {};
        static inline Event<IDXGISwapChain*> onPresentEvent = {};
        static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onBeforeResizeEvent = {};
        static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onAfterResizeEvent = {};
        static inline Event<ID3D12CommandQueue*, UINT, const ID3D12CommandList**> onExecuteCommandListsEvent = {};
        static inline Event<> onReleaseEvent = {};
        static inline ID3D12CommandQueue* GetCommandQueueFromSwapChain(IDXGISwapChain* pSwapChain)
        {
            if (commandQueueOffset)
                return *(ID3D12CommandQueue**)((uintptr_t)pSwapChain + commandQueueOffset);
            return nullptr;
        }
    };
private:
    static inline void HookD3D12()
    {
        DllCallbackHandler::RegisterUnloadCallback(L"d3d12.dll", []() { D3D12::onShutdownEvent(); });

        auto hD3D12 = GetModuleHandleW(L"d3d12.dll");
        if (!hD3D12) return;
        HMODULE hD3D12Guard = LoadLibraryW(L"d3d12.dll");
        bool isDXGILoaded = false;
        auto hDXGI = GetModuleHandleW(L"dxgi.dll");
        if (!hDXGI)
            hDXGI = LoadLibraryW(L"dxgi.dll");
        else
            isDXGILoaded = true;

        if (!hDXGI) return;

        auto CreateDXGIFactory = GetProcAddress(hDXGI, "CreateDXGIFactory");
        if (CreateDXGIFactory == NULL)
            return;

        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
            IDXGIFactory* Factory;
            if (((HRESULT(WINAPI*)(const IID&, void**))(CreateDXGIFactory))(__uuidof(IDXGIFactory), (void**)&Factory) < 0)
                return;

            IDXGIAdapter* adapter;
            if (Factory->EnumAdapters(0, &adapter) == DXGI_ERROR_NOT_FOUND)
                return;

            auto D3D12CreateDevice = GetProcAddress(hD3D12, "D3D12CreateDevice");
            if (D3D12CreateDevice == NULL)
                return;

            ID3D12Device* Device;
            if (((HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, const IID&, void**))(D3D12CreateDevice))(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&Device) >= 0)
            {
                D3D12_COMMAND_QUEUE_DESC queueDesc;
                queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
                queueDesc.Priority = 0;
                queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
                queueDesc.NodeMask = 0;

                ID3D12CommandQueue* CommandQueue;
                if (Device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&CommandQueue) < 0)
                    return;

                ID3D12CommandAllocator* CommandAllocator;
                if (Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&CommandAllocator) < 0)
                    return;

                ID3D12GraphicsCommandList* CommandList;
                if (Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocator, NULL, __uuidof(ID3D12GraphicsCommandList), (void**)&CommandList) < 0)
                    return;

                auto hookWindowCtor = HookWindow(L"FusionDxHookD3D12", L"FusionDxHookD3D12");
                auto hWnd = HookWindow::GetHookWindow(hookWindowCtor);

                DXGI_RATIONAL refreshRate;
                refreshRate.Numerator = 60;
                refreshRate.Denominator = 1;

                DXGI_MODE_DESC bufferDesc;
                bufferDesc.Width = 100;
                bufferDesc.Height = 100;
                bufferDesc.RefreshRate = refreshRate;
                bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
                bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

                DXGI_SAMPLE_DESC sampleDesc;
                sampleDesc.Count = 1;
                sampleDesc.Quality = 0;

                DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
                swapChainDesc.BufferDesc = bufferDesc;
                swapChainDesc.SampleDesc = sampleDesc;
                swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                swapChainDesc.BufferCount = 2;
                swapChainDesc.OutputWindow = hWnd;
                swapChainDesc.Windowed = 1;
                swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

                IDXGISwapChain* SwapChain;
                if (Factory->CreateSwapChain(CommandQueue, &swapChainDesc, &SwapChain) >= 0)
                {
                    copyMethods(hD3D12,
                        std::forward_as_tuple(ID3D12DeviceVTBL(), *(uintptr_t**)Device),
                        std::forward_as_tuple(IDirect3DDevice12CommandQueueVTBL(), *(uintptr_t**)CommandQueue),
                        std::forward_as_tuple(IDirect3DDevice12CommandAllocatorVTBL(), *(uintptr_t**)CommandAllocator),
                        std::forward_as_tuple(ID3D12GraphicsCommandListVTBL(), *(uintptr_t**)CommandList),
                        std::forward_as_tuple(IDXGISwapChainVTBL(), *(uintptr_t**)SwapChain)
                    );

                    for (int i = 0; i < 1024; i++)
                    {
                        uintptr_t swPtr = (uintptr_t)SwapChain;
                        uintptr_t target = swPtr + (i * sizeof(uintptr_t));
                        if (IsBadReadPtr((void*)target, sizeof(uintptr_t)))
                            break;

                        uintptr_t value = *(uintptr_t*)(target);

                        if (value == (uintptr_t)CommandQueue)
                        {
                            commandQueueOffset = (i * sizeof(uintptr_t));
                            break;
                        }
                    }

                    #if FUSIONDXHOOK_USE_SAFETYHOOK
                    static SafetyHookInline presentOriginal = {};
                    static SafetyHookInline resizeBuffersOriginal = {};
                    static SafetyHookInline executeCommandListsOriginal = {};
                    static SafetyHookInline releaseOriginal = {};

                    auto D3D12Present = [](IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) -> HRESULT
                    {
                        // the hook of Direct3D 11 sits on the same function, this
                        // one only reports the frames of a Direct3D 12 swap chain
                        if (GetSwapChainKind(pSwapChain) != SwapChainKind::Direct3D11)
                        {
                            D3D12::onPresentEvent(pSwapChain);
                        }

                        return presentOriginal.unsafe_stdcall<HRESULT>(pSwapChain, SyncInterval, Flags);
                    };

                    auto D3D12ResizeBuffers = [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) -> HRESULT
                    {
                        D3D12::onBeforeResizeEvent(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                        HRESULT result = resizeBuffersOriginal.unsafe_stdcall<HRESULT>(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                        D3D12::onAfterResizeEvent(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                        return result;
                    };

                    auto D3D12ExecuteCommandLists = [](ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, const ID3D12CommandList** ppCommandLists)
                    {
                        D3D12::onExecuteCommandListsEvent(pCommandQueue, NumCommandLists, ppCommandLists);
                        return executeCommandListsOriginal.unsafe_stdcall(pCommandQueue, NumCommandLists, ppCommandLists);
                    };

                    auto D3D12Release = [](IUnknown* ptr) -> ULONG
                    {
                        IUnknown* pSwapChain = nullptr;
                        if (ptr->QueryInterface(__uuidof(IDXGISwapChain), (void**)&pSwapChain) == S_OK)
                        {
                            auto ref_count = releaseOriginal.unsafe_stdcall<ULONG>(pSwapChain);
                            if (pSwapChain == ptr && ref_count == 1)
                                D3D12::onReleaseEvent();
                        }
                        return releaseOriginal.unsafe_stdcall<ULONG>(ptr);
                    };

                    static HRESULT(WINAPI* Present)(IDXGISwapChain*, UINT, UINT) = D3D12Present;
                    static HRESULT(WINAPI* ResizeBuffers)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) = D3D12ResizeBuffers;
                    static void(WINAPI* ExecuteCommandLists)(ID3D12CommandQueue*, UINT, const ID3D12CommandList**) = D3D12ExecuteCommandLists;
                    static ULONG(WINAPI* Release)(IUnknown*) = D3D12Release;

                    _DELAYED_BIND

                    hD3D12 = GetModuleHandleW(L"d3d12.dll");
                    if (!hD3D12) return;

                    bind(hD3D12, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("Present"), Present, presentOriginal);
                    bind(hD3D12, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("ResizeBuffers"), ResizeBuffers, resizeBuffersOriginal);
                    bind(hD3D12, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("Release"), Release, releaseOriginal);
                    bind(hD3D12, typeid(IDirect3DDevice12CommandQueueVTBL), IDirect3DDevice12CommandQueueVTBL().GetIndex("ExecuteCommandLists"), ExecuteCommandLists, executeCommandListsOriginal);
                    #endif
                    CommandQueue->Release();
                    CommandAllocator->Release();
                    CommandList->Release();
                    SwapChain->Release();
                }
                Device->Release();
            }
            if (!isDXGILoaded)
                FreeLibrary(hDXGI);
        });

        if (hD3D12Guard)
            FreeLibrary(hD3D12Guard);
    }
#else
    static inline void HookD3D12() {}
#endif

#if FUSIONDXHOOK_INCLUDE_OPENGL
public:
    struct OPENGL {
        static inline Event<> onInitEvent = {};
        static inline Event<> onShutdownEvent = {};
        static inline Event<HDC> onSwapBuffersEvent = {};
    };
private:
    // A frame can be handed over through either of the two swap functions, and
    // gdi's SwapBuffers reaches the driver without going through the one
    // opengl32 exports. Both are hooked and report the same event, but only the
    // first one of a frame gets through. The flag stays set while the original
    // runs, so a swap function that ends up in the other hook does not report
    // the same frame a second time.
    static inline thread_local bool inSwapDispatch = false;

    static inline bool BeginSwapDispatch(HDC hdc)
    {
        if (inSwapDispatch)
            return false;

        inSwapDispatch = true;
        OPENGL::onSwapBuffersEvent(hdc);
        return true;
    }

    static inline void EndSwapDispatch(bool owner)
    {
        if (owner)
            inSwapDispatch = false;
    }

    static inline void HookOPENGL()
    {
        DllCallbackHandler::RegisterUnloadCallback(L"opengl32.dll", []() { OPENGL::onShutdownEvent(); });

        auto hOpenGL32 = GetModuleHandleW(L"opengl32.dll");
        auto hGdi32 = GetModuleHandleW(L"gdi32.dll");
        if (!hOpenGL32 && !hGdi32) return;
        HMODULE hOpenGL32Guard = hOpenGL32 ? LoadLibraryW(L"opengl32.dll") : nullptr;
        HMODULE hGdi32Guard = hGdi32 ? LoadLibraryW(L"gdi32.dll") : nullptr;
        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
            OpenGLVTBL ogl;
            deviceMethods[hOpenGL32][typeid(OpenGLVTBL)].resize(ogl.GetNumberOfMethods());
            for (unsigned int i = 0; i < ogl.GetNumberOfMethods(); i++)
            {
                deviceMethods.at(hOpenGL32).at(typeid(OpenGLVTBL)).at(i) = (uintptr_t*)GetProcAddress(hOpenGL32, ogl.GetMethod(i));
            }

            // the frame of a window that presents through the gdi never reaches
            // wglSwapBuffers, that is why this one is hooked as well
            Gdi32VTBL gdi;
            deviceMethods[hGdi32][typeid(Gdi32VTBL)].resize(gdi.GetNumberOfMethods());
            for (unsigned int i = 0; i < gdi.GetNumberOfMethods(); i++)
            {
                deviceMethods.at(hGdi32).at(typeid(Gdi32VTBL)).at(i) = (uintptr_t*)GetProcAddress(hGdi32, gdi.GetMethod(i));
            }

            #if FUSIONDXHOOK_USE_SAFETYHOOK
            static SafetyHookInline wglSwapBuffersOriginal = {};
            static SafetyHookInline swapBuffersOriginal = {};

            auto OpenGLwglSwapBuffers = [](HDC hDc) -> BOOL
            {
                const bool owner = BeginSwapDispatch(hDc);
                const BOOL result = wglSwapBuffersOriginal.unsafe_stdcall<BOOL>(hDc);
                EndSwapDispatch(owner);
                return result;
            };

            auto Gdi32SwapBuffers = [](HDC hDc) -> BOOL
            {
                const bool owner = BeginSwapDispatch(hDc);
                const BOOL result = swapBuffersOriginal.unsafe_stdcall<BOOL>(hDc);
                EndSwapDispatch(owner);
                return result;
            };

            static BOOL(__stdcall* wglSwapBuffers)(HDC) = OpenGLwglSwapBuffers;
            static BOOL(__stdcall* swapBuffers)(HDC) = Gdi32SwapBuffers;

            _DELAYED_BIND

            hOpenGL32 = GetModuleHandleW(L"opengl32.dll");
            if (hOpenGL32)
                bind(hOpenGL32, typeid(OpenGLVTBL), ogl.GetIndex("wglSwapBuffers"), wglSwapBuffers, wglSwapBuffersOriginal);

            hGdi32 = GetModuleHandleW(L"gdi32.dll");
            if (hGdi32)
                bind(hGdi32, typeid(Gdi32VTBL), gdi.GetIndex("SwapBuffers"), swapBuffers, swapBuffersOriginal);
            #endif
        });

        if (hOpenGL32Guard)
            FreeLibrary(hOpenGL32Guard);
        if (hGdi32Guard)
            FreeLibrary(hGdi32Guard);
    }
#else
    static inline void HookOPENGL() {}
#endif

#if FUSIONDXHOOK_INCLUDE_VULKAN
public:
    struct VULKAN {
        static inline Event<> onInitEvent = {};
        static inline Event<> onShutdownEvent = {};
        static inline Event<VkQueue, const VkPresentInfoKHR*> onVkQueuePresentKHREvent = {};
        static inline Event<VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*> onvkCreateDeviceEvent = {};
        // the format and the size of the images only exist in the description of
        // the swap chain, nothing can be asked for them afterwards
        static inline Event<VkDevice, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*> onVkCreateSwapchainKHREvent = {};
    };
private:
    static inline void HookVULKAN()
    {
        DllCallbackHandler::RegisterUnloadCallback(L"vulkan-1.dll", []() { VULKAN::onShutdownEvent(); });

        auto hVulkan1 = GetModuleHandleW(L"vulkan-1.dll");
        if (!hVulkan1)
            return;

        HMODULE hVulkan1Guard = LoadLibraryW(L"vulkan-1.dll");

        // The loader is unloaded and loaded again in the middle of a session, an
        // emulator asks it whether Vulkan is available at all and frees it right
        // after, and every load is a new image that does not contain the hooks.
        // They are therefore installed for every load, two loads can come in on
        // two threads, and the delay below lets the module settle first.
        static std::mutex vulkanMutex;
        std::lock_guard<std::mutex> vulkanLock(vulkanMutex);

        VulkanVTBL vk;
        deviceMethods[hVulkan1][typeid(VulkanVTBL)].resize(vk.GetNumberOfMethods());

        for (unsigned int i = 0; i < vk.GetNumberOfMethods(); i++)
        {
            deviceMethods.at(hVulkan1).at(typeid(VulkanVTBL)).at(i) = (uintptr_t*)GetProcAddress(hVulkan1, vk.GetMethod(i));
        }

        #if FUSIONDXHOOK_USE_SAFETYHOOK
        //
        // No delay and no thread here: the device is created right after the
        // loader is loaded, so the hook for it has to be in place before the
        // load returns.
        //

        // the module may be gone again by now, the loader is not unloaded in the
        // middle of a notification, but be sure
        hVulkan1 = GetModuleHandleW(L"vulkan-1.dll");

        if (!hVulkan1)
            return;

        // The callbacks forward to the trampoline of the hook of the image that
        // is loaded right now, which is the newest one.
        static SafetyHookInline* gpQueuePresentHook = nullptr;
        static SafetyHookInline* gpCreateDeviceHook = nullptr;
        static SafetyHookInline* gpCreateSwapchainHook = nullptr;

        // The functions a driver hands out for one of its devices are its own, an
        // application that asks for them with vkGetDeviceProcAddr never returns
        // into the loader, and that is what an emulator does with the calls of
        // the swap chain: the entry points of the module are skipped entirely.
        // They are therefore hooked as well, as soon as a device exists.
        static SafetyHookInline* gpDeviceQueuePresentHook = nullptr;
        static SafetyHookInline* gpDeviceCreateSwapchainHook = nullptr;

        static PFN_vkGetDeviceProcAddr gpGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)GetProcAddress(hVulkan1, "vkGetDeviceProcAddr");

        // A call that comes in through the loader ends up in the driver, where the
        // second hook reports the same frame a second time. The frame may only be
        // drawn once.
        static thread_local bool bInLoaderCall = false;

        // the installers below are handed over as plain functions, a callback of
        // the hooks cannot capture anything
        static void (*pHookVulkanDevice)(VkDevice) = nullptr;

        auto VULKANvkQueuePresentKHRDevice = [](VkQueue queue, const VkPresentInfoKHR* pPresentInfo) -> VkResult
        {
            if (!bInLoaderCall)
            {
                VULKAN::onVkQueuePresentKHREvent(queue, pPresentInfo);
            }

            if (!gpDeviceQueuePresentHook)
                return VK_ERROR_DEVICE_LOST;

            return gpDeviceQueuePresentHook->unsafe_stdcall<VkResult>(queue, pPresentInfo);
        };

        auto VULKANvkCreateSwapchainKHRDevice = [](VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) -> VkResult
        {
            if (!bInLoaderCall)
            {
                VULKAN::onVkCreateSwapchainKHREvent(device, pCreateInfo, pAllocator, pSwapchain);
            }

            if (!gpDeviceCreateSwapchainHook)
                return VK_ERROR_DEVICE_LOST;

            return gpDeviceCreateSwapchainHook->unsafe_stdcall<VkResult>(device, pCreateInfo, pAllocator, pSwapchain);
        };

        static VkResult(VKAPI_CALL* vkQueuePresentKHRDevice)(VkQueue, const VkPresentInfoKHR*) = VULKANvkQueuePresentKHRDevice;
        static VkResult(VKAPI_CALL* vkCreateSwapchainKHRDevice)(VkDevice, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*) = VULKANvkCreateSwapchainKHRDevice;

        // The device functions can only be reached with a device, so this runs
        // once the device of the application exists.
        auto VULKANhookVulkanDevice = [](VkDevice device)
        {
            if (!gpGetDeviceProcAddr || !device)
                return;

            void* present = (void*)gpGetDeviceProcAddr(device, "vkQueuePresentKHR");
            void* createSwapchain = (void*)gpGetDeviceProcAddr(device, "vkCreateSwapchainKHR");

            gpDeviceQueuePresentHook = InstallAddressHook(present, (void*)vkQueuePresentKHRDevice, gpDeviceQueuePresentHook);
            gpDeviceCreateSwapchainHook = InstallAddressHook(createSwapchain, (void*)vkCreateSwapchainKHRDevice, gpDeviceCreateSwapchainHook);
        };

        pHookVulkanDevice = VULKANhookVulkanDevice;

        auto VULKANvkQueuePresentKHR = [](VkQueue queue, const VkPresentInfoKHR* pPresentInfo) -> VkResult
        {
            VULKAN::onVkQueuePresentKHREvent(queue, pPresentInfo);

            if (!gpQueuePresentHook)
                return VK_ERROR_DEVICE_LOST;

            // the loader forwards to the driver of the device, which is where the
            // second hook sits
            bInLoaderCall = true;
            const VkResult result = gpQueuePresentHook->unsafe_stdcall<VkResult>(queue, pPresentInfo);
            bInLoaderCall = false;

            return result;
        };

        auto VULKANvkCreateDevice = [](VkPhysicalDevice gpu, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) -> VkResult
        {
            if (!gpCreateDeviceHook)
                return VK_ERROR_DEVICE_LOST;

            // the device does not exist before this call is over, so the hook of
            // its functions can only be installed after it
            const VkResult result = gpCreateDeviceHook->unsafe_stdcall<VkResult>(gpu, pCreateInfo, pAllocator, pDevice);

            if (result == VK_SUCCESS && pDevice && *pDevice && pHookVulkanDevice)
                pHookVulkanDevice(*pDevice);

            VULKAN::onvkCreateDeviceEvent(gpu, pCreateInfo, pAllocator, pDevice);
            return result;
        };

        auto VULKANvkCreateSwapchainKHR = [](VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) -> VkResult
        {
            VULKAN::onVkCreateSwapchainKHREvent(device, pCreateInfo, pAllocator, pSwapchain);

            if (!gpCreateSwapchainHook)
                return VK_ERROR_DEVICE_LOST;

            bInLoaderCall = true;
            const VkResult result = gpCreateSwapchainHook->unsafe_stdcall<VkResult>(device, pCreateInfo, pAllocator, pSwapchain);
            bInLoaderCall = false;

            return result;
        };

        static VkResult(VKAPI_CALL* vkQueuePresentKHR)(VkQueue, const VkPresentInfoKHR*) = VULKANvkQueuePresentKHR;
        static VkResult(VKAPI_CALL* vkCreateDevice)(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*) = VULKANvkCreateDevice;
        static VkResult(VKAPI_CALL* vkCreateSwapchainKHR)(VkDevice, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*) = VULKANvkCreateSwapchainKHR;

        // the trampoline of the new hook has to be known before it can be called
        auto queuePresentHook = InstallHook(hVulkan1, typeid(VulkanVTBL), vk.GetIndex("vkQueuePresentKHR"), vkQueuePresentKHR, gpQueuePresentHook);
        auto createDeviceHook = InstallHook(hVulkan1, typeid(VulkanVTBL), vk.GetIndex("vkCreateDevice"), vkCreateDevice, gpCreateDeviceHook);
        auto createSwapchainHook = InstallHook(hVulkan1, typeid(VulkanVTBL), vk.GetIndex("vkCreateSwapchainKHR"), vkCreateSwapchainKHR, gpCreateSwapchainHook);

        gpQueuePresentHook = queuePresentHook;
        gpCreateDeviceHook = createDeviceHook;
        gpCreateSwapchainHook = createSwapchainHook;
        #endif

        if (hVulkan1Guard)
            FreeLibrary(hVulkan1Guard);
    }
#else
    static inline void HookVULKAN() {}
#endif

#if FUSIONDXHOOK_INCLUDE_D3D11 || FUSIONDXHOOK_INCLUDE_D3D12
public:
    // ---------------------------------------------------------------------------
    // The frame of a game, and the place in it where the UI of the game begins.
    //
    // A renderer that records its frame on one thread and has it executed by another
    // one cannot be told where to put a draw by the thread that records it: a draw
    // made there lands wherever that thread happens to be in its own stream of
    // commands, which is not in the middle of a frame at all. The thread that executes
    // the frame is the one that presents it, and the frame is executed by the calls
    // that are made on the context of the device or on a command list, which is where
    // a draw can be put in the middle of a frame. The two draws that bound the place
    // are the one that puts the finished frame into the buffer that is presented and
    // the first one of the UI that is drawn over it.
    //
    // Making an inline hook is a thing of its own and every host has a way of doing
    // it already, so the hook is asked of the host, see HookFunction. What is here is
    // what belongs to the API and not to the game: which draws land in the buffer that
    // is presented, which state a command list had, and what has to be put back after
    // a draw was put between two draws of the game. Which of those draws is the frame
    // of the game and which one is the UI over it is a thing of the renderer of the
    // game and of no API, so it is left to the handler.
    // ---------------------------------------------------------------------------
    struct Frame
    {
        // Installs an inline hook at target that runs destination, and returns what
        // has to be called to run what was there, or nullptr when no hook could be
        // made. The hook stays alive for as long as the game runs, so it is the host
        // that keeps it.
        using HookFunction = void* (*)(void* target, void* destination);

    private:
        static inline HookFunction createHook = nullptr;

        // Whether what is being drawn now is the caller's own, which is not part of the
        // frame: it must not be read as one, and it must not be drawn for a second time
        // either, see the note of Frame.
        static inline bool bInsideDraw = false;

        static inline void* Hook(void* target, void* destination)
        {
            return (createHook && target) ? createHook(target, destination) : nullptr;
        }

    public:
#if FUSIONDXHOOK_INCLUDE_D3D11
        // A draw that lands in the buffer that is presented, reported on the thread
        // that executes the frame right before the draw is made, with the shader the
        // draw is about to be made with. What the handler draws with the context lands
        // in the target it is handed, in front of that draw.
        //
        // Which of those draws is the frame of the game and which one is the UI over it
        // is a thing of the renderer of the game and not of the API, so it is the
        // handler that decides: all that is known here is that the draw goes into the
        // buffer that is presented.
        static inline Event<ID3D11DeviceContext*, ID3D11RenderTargetView*, const void*> onPresentedDrawD3D11Event = {};
#endif

#if FUSIONDXHOOK_INCLUDE_D3D12
        // The same for Direct3D 12, except that the handler records into the list the
        // draw is about to be recorded into. What it records changes the state of that
        // list, so it sets bRecorded when it has recorded something, and every setting
        // the list had is put back for it afterwards: nothing of a list can be read
        // back out of it, so what the game had set is kept here as it was set.
        static inline Event<ID3D12GraphicsCommandList*, const void*, bool&> onPresentedDrawD3D12Event = {};

        // A submission of the queue of the game, right before it is executed, with the
        // lists it carries. A frame of Direct3D 12 can reach the buffer that is
        // presented through more than one of them, and which one of them carries the
        // UI is a thing of the renderer of the game, so it is the handler that decides
        // whether this is the place to draw at.
        static inline Event<ID3D12CommandQueue*, unsigned, ID3D12CommandList* const*> onSubmitD3D12Event = {};

        // The queue the frames of the game are submitted on, which is the one anything
        // recorded into a list of a frame has to be submitted with. DXGI does not hand
        // it out for a chain of Direct3D 12 resources, so it is read out of the chain
        // by FusionDxHook, and caught from the submissions of the game before that.
        static inline ID3D12CommandQueue* GetD3D12CommandQueue(IDXGISwapChain* pSwapChain)
        {
            if (pGameQueue)
                return pGameQueue;

            return FusionDxHook::D3D12::GetCommandQueueFromSwapChain(pSwapChain);
        }
#endif

        // Installs the hooks of every API that is enabled. Called once, before the
        // game has made a device of any of them.
        static inline void Init(HookFunction createInlineHook)
        {
            createHook = createInlineHook;

#if FUSIONDXHOOK_INCLUDE_D3D11
            InstallD3D11Hooks();
#endif

#if FUSIONDXHOOK_INCLUDE_D3D12
            InstallD3D12Hooks();
#endif
        }

        // The present of a frame, which only the host can see, since the function the
        // game presents through is a thing of the game. The chain is what the buffers
        // of the frame are read from, and the frame ends here: what is drawn from this
        // point on belongs to the next one.
        static inline void OnPresent(IDXGISwapChain* pSwapChain)
        {
            if (!pSwapChain)
                return;

            const SwapChainKind kind = GetSwapChainKind(pSwapChain);

#if FUSIONDXHOOK_INCLUDE_D3D11
            // The buffers of the chain are the ones a frame is put into and drawn over
            // at the end of it, which is what a draw that goes into one of them is
            // told apart from the rest of the draws by, see PresentedDrawD3D11.
            if (kind != SwapChainKind::Direct3D12)
            {
                RefreshD3D11BackBuffers(pSwapChain);

                // The device of the chain is the device of the frame, and the context it
                // hands out is the one the frame is executed with. This is asked of
                // every present, because the device of the game can be made before the
                // hooks of the frame are installed at all, and the hooks only ever make
                // it onto a context that is asked for here.
                if (!bContextHooksInstalled)
                {
                    ID3D11Device* pDevice = nullptr;
                    pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice);

                    if (pDevice)
                    {
                        ID3D11DeviceContext* pContext = nullptr;
                        pDevice->GetImmediateContext(&pContext);

                        if (pContext)
                        {
                            InstallD3D11ContextHooks(pContext);
                            pContext->Release();
                        }

                        pDevice->Release();
                    }
                }
            }
#endif

#if FUSIONDXHOOK_INCLUDE_D3D12
            if (kind != SwapChainKind::Direct3D11)
            {
                RefreshD3D12SwapChainBuffers(pSwapChain);

                // The queue the frames are submitted on is what anything recorded into
                // a list of one has to be submitted with as well, and it is caught from
                // the submissions of the game or from a queue of our own that shares
                // its table of methods.
                InstallD3D12QueueHooksFromGameDevice(pSwapChain);
                InstallD3D12QueueHooksFromOwnQueue();

                CheckD3D12GameQueue(pSwapChain);
            }
#endif
        }

        // Before the game resizes a chain of a frame. DXGI refuses to resize a chain
        // while anything still holds a reference to a buffer of it, and what holds one
        // is what reads the buffers of the frame, see RefreshD3D11BackBuffers and
        // RefreshD3D12SwapChainBuffers, so they are given up here, before the resize
        // that a size check somewhere else would only see afterwards.
        static inline void OnBeforeResize()
        {
#if FUSIONDXHOOK_INCLUDE_D3D11
            ReleaseD3D11BackBuffers();
#endif

#if FUSIONDXHOOK_INCLUDE_D3D12
            ReleaseD3D12SwapChainBuffers();

            // The commands that were recorded into the lists of the frames are
            // submitted by the game, so the fence of anything of the caller says
            // nothing about them, and what is freed here must not still be read by
            // one of them.
            WaitForD3D12Queue();
#endif
        }

#if FUSIONDXHOOK_INCLUDE_D3D11
    private:
        // The buffers of the chain, which is what a frame is presented with: the chain
        // alternates between them while it flips, so every one of them is the buffer
        // that is presented as far as a draw is concerned.
        static constexpr int MaxBackBuffers = 4;
        static inline ID3D11Texture2D* backBuffers[MaxBackBuffers] = {};
        static inline int numBackBuffers = 0;

        static inline ID3D11RenderTargetView* pBoundTarget = nullptr;   // borrowed, the game owns it
        static inline const void* pBoundPixelShader = nullptr;          // borrowed, the game owns it
        static inline bool bTargetIsBackBuffer = false;
        static inline bool bContextHooksInstalled = false;

        static inline void RefreshD3D11BackBuffers(IDXGISwapChain* pSwapChain)
        {
            DXGI_SWAP_CHAIN_DESC desc{};

            if (FAILED(pSwapChain->GetDesc(&desc)))
                return;

            const int buffers = (desc.BufferCount < (UINT)MaxBackBuffers) ? (int)desc.BufferCount : MaxBackBuffers;

            for (int i = 0; i < MaxBackBuffers; i++)
            {
                ID3D11Texture2D* pBuffer = nullptr;

                if (i < buffers)
                    pSwapChain->GetBuffer((UINT)i, __uuidof(ID3D11Texture2D), (void**)&pBuffer);

                if (pBuffer != backBuffers[i])
                {
                    if (backBuffers[i])
                        backBuffers[i]->Release();

                    backBuffers[i] = pBuffer;
                }
                else if (pBuffer)
                {
                    pBuffer->Release();
                }
            }

            numBackBuffers = buffers;
        }

        static inline void ReleaseD3D11BackBuffers()
        {
            for (int i = 0; i < MaxBackBuffers; i++)
            {
                if (backBuffers[i])
                {
                    backBuffers[i]->Release();
                    backBuffers[i] = nullptr;
                }
            }

            numBackBuffers = 0;
        }

        static inline bool IsBackBufferView(ID3D11RenderTargetView* pView)
        {
            if (!pView || !numBackBuffers)
                return false;

            ID3D11Resource* pResource = nullptr;
            pView->GetResource(&pResource);

            bool bResult = false;

            for (int i = 0; i < numBackBuffers && !bResult; i++)
                bResult = (pResource == (ID3D11Resource*)backBuffers[i]);

            if (pResource)
                pResource->Release();

            return bResult;
        }

        // A draw that lands in the buffer that is presented is reported to the caller
        // before it is made, which is the only place a draw of the caller's can be put
        // in the middle of a frame of this API, see the note of Frame.
        static inline void PresentedDrawD3D11(ID3D11DeviceContext* pContext, ID3D11RenderTargetView* pTarget)
        {
            if (bInsideDraw)
                return;

            bInsideDraw = true;
            onPresentedDrawD3D11Event(pContext, pTarget, pBoundPixelShader);
            bInsideDraw = false;
        }

        // The functions of the context, which every draw of the frame and every draw
        // of the caller goes through: what is patched is the function of the interface
        // itself, not the table of one object, since two contexts of the same kind do
        // not have to share a table of methods, see InstallD3D11ContextHooks.
        using PSSetShaderFn = void(__stdcall*)(ID3D11DeviceContext*, ID3D11PixelShader*, const ID3D11ClassInstance* const*, UINT);
        using DrawFn = void(__stdcall*)(ID3D11DeviceContext*, UINT, UINT);
        using DrawIndexedFn = void(__stdcall*)(ID3D11DeviceContext*, UINT, UINT, INT);
        using DrawInstancedFn = void(__stdcall*)(ID3D11DeviceContext*, UINT, UINT, UINT, UINT);
        using DrawIndexedInstancedFn = void(__stdcall*)(ID3D11DeviceContext*, UINT, UINT, UINT, INT, UINT);
        using OMSetRenderTargetsFn = void(__stdcall*)(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*);
        using OMSetRenderTargetsAndUAVsFn = void(__stdcall*)(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*,
            ID3D11DepthStencilView*, UINT, ID3D11UnorderedAccessView* const*, const UINT*);

        static inline PSSetShaderFn pPSSetShader = nullptr;
        static inline DrawFn pDraw = nullptr;
        static inline DrawIndexedFn pDrawIndexed = nullptr;
        static inline DrawInstancedFn pDrawInstanced = nullptr;
        static inline DrawIndexedInstancedFn pDrawIndexedInstanced = nullptr;
        static inline OMSetRenderTargetsFn pOMSetRenderTargets = nullptr;
        static inline OMSetRenderTargetsAndUAVsFn pOMSetRenderTargetsAndUAVs = nullptr;

        static void __stdcall PSSetShaderHook(ID3D11DeviceContext* pContext, ID3D11PixelShader* pPixelShader,
            const ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)
        {
            pBoundPixelShader = pPixelShader;
            pPSSetShader(pContext, pPixelShader, ppClassInstances, NumClassInstances);
        }

        static void __stdcall OMSetRenderTargetsHook(ID3D11DeviceContext* pContext, UINT NumViews,
            ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView)
        {
            pBoundTarget = (NumViews && ppRenderTargetViews) ? ppRenderTargetViews[0] : nullptr;
            bTargetIsBackBuffer = IsBackBufferView(pBoundTarget);

            pOMSetRenderTargets(pContext, NumViews, ppRenderTargetViews, pDepthStencilView);
        }

        // The unordered access views are not read here, so nothing is known about the
        // target after this one was called: it counts as a target of the renderer,
        // never the one that is presented.
        static void __stdcall OMSetRenderTargetsAndUAVsHook(ID3D11DeviceContext* pContext, UINT NumViews,
            ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView,
            UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts)
        {
            pBoundTarget = nullptr;
            bTargetIsBackBuffer = false;

            pOMSetRenderTargetsAndUAVs(pContext, NumViews, ppRenderTargetViews, pDepthStencilView,
                NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
        }

        static void __stdcall DrawHook(ID3D11DeviceContext* pContext, UINT VertexCount, UINT StartVertexLocation)
        {
            if (bTargetIsBackBuffer)
                PresentedDrawD3D11(pContext, pBoundTarget);

            pDraw(pContext, VertexCount, StartVertexLocation);
        }

        static void __stdcall DrawIndexedHook(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
        {
            if (bTargetIsBackBuffer)
                PresentedDrawD3D11(pContext, pBoundTarget);

            pDrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
        }

        static void __stdcall DrawInstancedHook(ID3D11DeviceContext* pContext, UINT VertexCountPerInstance, UINT InstanceCount,
            UINT StartVertexLocation, UINT StartInstanceLocation)
        {
            if (bTargetIsBackBuffer)
                PresentedDrawD3D11(pContext, pBoundTarget);

            pDrawInstanced(pContext, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
        }

        static void __stdcall DrawIndexedInstancedHook11(ID3D11DeviceContext* pContext, UINT IndexCountPerInstance, UINT InstanceCount,
            UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
        {
            if (bTargetIsBackBuffer)
                PresentedDrawD3D11(pContext, pBoundTarget);

            pDrawIndexedInstanced(pContext, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
        }

        // The context of the device is what the frame of the game is executed with, and
        // the functions of the interface are what every call of it is dispatched
        // through, both the ones of the game and the ones of the caller, so they are
        // patched once, for the first context that is seen, and every context of the
        // device goes through them afterwards.
        static inline void InstallD3D11ContextHooks(ID3D11DeviceContext* pContext)
        {
            if (!pContext)
                return;

            ID3D11Device* pDevice = nullptr;
            pContext->GetDevice(&pDevice);

            const bool bOwnDevice = (pDevice && pDevice == ownD3D11Device);

            if (pDevice)
                pDevice->Release();

            // The device of the library itself is what the game never draws with.
            if (bOwnDevice || bOwnD3D11DeviceCreation)
                return;

            void** pVtable = *(void***)pContext;

            ID3D11DeviceContextVTBL table;

            // A function is hooked once, which is what the pointer of the one that is
            // already there says: hooking the same function twice would put the second
            // hook in front of the first for ever, and every call would run both.
            if (!pPSSetShader) pPSSetShader = (PSSetShaderFn)Hook(pVtable[table.GetIndex("PSSetShader")], PSSetShaderHook);
            if (!pDraw) pDraw = (DrawFn)Hook(pVtable[table.GetIndex("Draw")], DrawHook);
            if (!pDrawIndexed) pDrawIndexed = (DrawIndexedFn)Hook(pVtable[table.GetIndex("DrawIndexed")], DrawIndexedHook);
            if (!pDrawInstanced) pDrawInstanced = (DrawInstancedFn)Hook(pVtable[table.GetIndex("DrawInstanced")], DrawInstancedHook);
            if (!pDrawIndexedInstanced) pDrawIndexedInstanced = (DrawIndexedInstancedFn)Hook(pVtable[table.GetIndex("DrawIndexedInstanced")], DrawIndexedInstancedHook11);
            if (!pOMSetRenderTargets) pOMSetRenderTargets = (OMSetRenderTargetsFn)Hook(pVtable[table.GetIndex("OMSetRenderTargets")], OMSetRenderTargetsHook);
            if (!pOMSetRenderTargetsAndUAVs) pOMSetRenderTargetsAndUAVs = (OMSetRenderTargetsAndUAVsFn)Hook(pVtable[table.GetIndex("OMSetRenderTargetsAndUnorderedAccessViews")], OMSetRenderTargetsAndUAVsHook);

            bContextHooksInstalled = pPSSetShader && pDraw && pDrawIndexed && pDrawInstanced && pDrawIndexedInstanced &&
                pOMSetRenderTargets && pOMSetRenderTargetsAndUAVs;
        }

        // The device of the game is what the context the frame is executed with is made
        // with, so both of the ways one can be made are watched, and both of them end
        // in the same hooks, see InstallD3D11ContextHooks.
        static HRESULT __stdcall D3D11CreateDeviceHook(void* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE hSoftware, UINT Flags,
            const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
            ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext)
        {
            const HRESULT hr = pCreateDevice(pAdapter, DriverType, hSoftware, Flags, pFeatureLevels,
                FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);

            if (SUCCEEDED(hr) && ppImmediateContext)
                InstallD3D11ContextHooks(*ppImmediateContext);

            return hr;
        }

        static HRESULT __stdcall D3D11CreateDeviceAndSwapChainHook(void* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE hSoftware, UINT Flags,
            const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
            IDXGISwapChain** ppSwapChain, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext)
        {
            const HRESULT hr = pCreateDeviceAndSwapChain(pAdapter, DriverType, hSoftware, Flags, pFeatureLevels,
                FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);

            if (SUCCEEDED(hr) && ppImmediateContext)
                InstallD3D11ContextHooks(*ppImmediateContext);

            return hr;
        }

        using CreateDeviceFn = HRESULT(__stdcall*)(void*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT,
            ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
        using CreateDeviceAndSwapChainFn = HRESULT(__stdcall*)(void*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT,
            const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

        static inline CreateDeviceFn pCreateDevice = nullptr;
        static inline CreateDeviceAndSwapChainFn pCreateDeviceAndSwapChain = nullptr;

        static inline void InstallD3D11Hooks()
        {
            // Loaded here on purpose: the game loads it on its own when it is about to
            // make a device, and by then it would be too late for a hook of the call
            // that makes it.
            HMODULE hD3D11 = LoadLibraryA("d3d11.dll");

            if (!hD3D11)
                return;

            if (void* pProc = (void*)GetProcAddress(hD3D11, "D3D11CreateDevice"))
                pCreateDevice = (CreateDeviceFn)Hook(pProc, D3D11CreateDeviceHook);

            if (void* pProc = (void*)GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain"))
                pCreateDeviceAndSwapChain = (CreateDeviceAndSwapChainFn)Hook(pProc, D3D11CreateDeviceAndSwapChainHook);
        }
#endif // FUSIONDXHOOK_INCLUDE_D3D11

#if FUSIONDXHOOK_INCLUDE_D3D12
    private:
        // The command lists of the frame are recorded into from more than one thread
        // at a time, so everything that is counted here is counted under a lock.
        static inline std::mutex dx12Lock;

        // The state of a command list as the game last set it. Nothing of it can be
        // read back out of a list, so it is kept here as it is set, which is what has
        // to be put back after the draws of the caller were recorded into the list of
        // the game, see RestoreListState.
        struct ListState
        {
            static constexpr int MaxRootParameters = 16;
            static constexpr int MaxRootConstants = 64;
            static constexpr int MaxViewports = 8;
            static constexpr int MaxVertexBuffers = 8;
            static constexpr int MaxTargets = 8;

            struct RootParameter
            {
                int kind = 0; // 0 none, 1 table, 2 constant buffer, 3 resource, 4 access, 5 constants
                D3D12_GPU_DESCRIPTOR_HANDLE table{};
                D3D12_GPU_VIRTUAL_ADDRESS view = 0;
                UINT constants[MaxRootConstants] = {};
                UINT numConstants = 0;
            };

            ID3D12RootSignature* pRootSignature = nullptr;
            ID3D12PipelineState* pPipelineState = nullptr;
            ID3D12DescriptorHeap* pHeaps[2] = {};
            unsigned numHeaps = 0;
            RootParameter rootParameters[MaxRootParameters];
            D3D12_CPU_DESCRIPTOR_HANDLE targets[MaxTargets] = {};
            unsigned numTargets = 0;
            D3D12_CPU_DESCRIPTOR_HANDLE depth{};
            bool bHasDepth = false;
            D3D12_VERTEX_BUFFER_VIEW vertexBuffers[MaxVertexBuffers] = {};
            unsigned numVertexBuffers = 0;
            D3D12_INDEX_BUFFER_VIEW indexBuffer{};
            bool bHasIndexBuffer = false;
            D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
            D3D12_VIEWPORT viewports[MaxViewports] = {};
            unsigned numViewports = 0;
            D3D12_RECT scissors[MaxViewports] = {};
            unsigned numScissors = 0;
        };

        struct ListInfo
        {
            // Whether the target this list is drawing into is the buffer that is
            // presented, which is per list, since the game records several of them at
            // the same time.
            bool bTargetIsPresented = false;

            // The shader the list was last given, which is the shader of the draw the
            // caller is told about, and all of the frame that is read here.
            const void* pLastPipelineState = nullptr;
            ListState state;
        };

        static inline std::map<ID3D12GraphicsCommandList*, ListInfo> dx12Lists;
        static inline bool bQueueHooksInstalled = false;
        static inline bool bListHooksInstalled = false;
        static inline bool bDeviceHooksInstalled = false;

        // The buffers of the chain, and the render target descriptors the game made for
        // them: a draw is known to land in the buffer that is presented by the
        // descriptor it was bound with.
        static constexpr int MaxDx12SwapChainBuffers = 4;
        static inline ID3D12Resource* dx12SwapChainBuffers[MaxDx12SwapChainBuffers] = {};
        static inline int numDx12SwapChainBuffers = 0;

        // Every render target descriptor the game made, and with which resource, which
        // is how a draw into the buffer that is presented is told apart from the rest.
        // The chain itself is known only once it has presented, which is later than the
        // descriptors of its buffers are made, so the resources are looked up when a
        // descriptor is bound rather than when it was made.
        struct RenderTargetView
        {
            SIZE_T handle = 0;
            const ID3D12Resource* pResource = nullptr;
        };

        static constexpr int MaxDx12RenderTargetViews = 512;
        static inline RenderTargetView dx12RenderTargetViews[MaxDx12RenderTargetViews];
        static inline int numDx12RenderTargetViews = 0;

        // The queue the frames of the game are submitted on, see GetD3D12CommandQueue.
        static inline ID3D12CommandQueue* pGameQueue = nullptr;
        static inline ID3D12Device* pGameQueueDevice = nullptr;

        static inline bool IsPresentedHandle(SIZE_T handle)
        {
            for (int i = 0; i < numDx12RenderTargetViews; i++)
            {
                if (dx12RenderTargetViews[i].handle != handle)
                    continue;

                for (int j = 0; j < numDx12SwapChainBuffers; j++)
                {
                    if (dx12RenderTargetViews[i].pResource == dx12SwapChainBuffers[j])
                        return true;
                }
            }

            return false;
        }

        static inline void RefreshD3D12SwapChainBuffers(IDXGISwapChain* pSwapChain)
        {
            DXGI_SWAP_CHAIN_DESC desc{};

            if (FAILED(pSwapChain->GetDesc(&desc)))
                return;

            const int buffers = (desc.BufferCount < (UINT)MaxDx12SwapChainBuffers) ? (int)desc.BufferCount : MaxDx12SwapChainBuffers;

            for (int i = 0; i < MaxDx12SwapChainBuffers; i++)
            {
                ID3D12Resource* pBuffer = nullptr;

                if (i < buffers)
                    pSwapChain->GetBuffer((UINT)i, __uuidof(ID3D12Resource), (void**)&pBuffer);

                if (pBuffer != dx12SwapChainBuffers[i])
                {
                    if (dx12SwapChainBuffers[i])
                        dx12SwapChainBuffers[i]->Release();

                    dx12SwapChainBuffers[i] = pBuffer;
                }
                else if (pBuffer)
                {
                    pBuffer->Release();
                }
            }

            numDx12SwapChainBuffers = buffers;
        }

        // The descriptors that were made for the buffers of a chain that is gone go
        // with it, and the table they are in is capped, so a long session would
        // otherwise fill it up with the descriptors of chains that no longer exist.
        static inline void ReleaseD3D12SwapChainBuffers()
        {
            std::lock_guard<std::mutex> guard(dx12Lock);

            for (int i = 0; i < MaxDx12SwapChainBuffers; i++)
            {
                if (dx12SwapChainBuffers[i])
                {
                    dx12SwapChainBuffers[i]->Release();
                    dx12SwapChainBuffers[i] = nullptr;
                }
            }

            numDx12SwapChainBuffers = 0;
            numDx12RenderTargetViews = 0;
        }

        // The commands that were recorded into the lists of the frames are submitted by
        // the game, so the fence of the caller says nothing about when they are done
        // with what they use: the queue of the game is what is waited on. The wait is
        // bounded, so that a device that is gone cannot hang it.
        static inline void WaitForD3D12Queue()
        {
            if (!pGameQueue)
                return;

            ID3D12Device* pDevice = nullptr;
            pGameQueue->GetDevice(__uuidof(ID3D12Device), (void**)&pDevice);

            if (!pDevice)
                return;

            ID3D12Fence* pFence = nullptr;

            if (SUCCEEDED(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&pFence)) && pFence)
            {
                constexpr UINT64 FenceValue = 1;
                HANDLE hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

                if (hEvent && SUCCEEDED(pGameQueue->Signal(pFence, FenceValue)))
                {
                    if (pFence->GetCompletedValue() < FenceValue)
                    {
                        pFence->SetEventOnCompletion(FenceValue, hEvent);
                        WaitForSingleObject(hEvent, 5000);
                    }
                }

                if (hEvent)
                    CloseHandle(hEvent);

                pFence->Release();
            }

            pDevice->Release();
        }

        // The queue that was caught from the submissions of the game is only used once
        // it is known to belong to the same device as the chain, which is the device
        // everything that is drawn has to be made on.
        static inline void CheckD3D12GameQueue(IDXGISwapChain* pSwapChain)
        {
            ID3D12Device* pChainDevice = nullptr;
            pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&pChainDevice);

            if (pGameQueue && pChainDevice && pGameQueueDevice != pChainDevice)
            {
                pGameQueue->Release();
                pGameQueue = nullptr;

                if (pGameQueueDevice)
                    pGameQueueDevice->Release();

                pGameQueueDevice = nullptr;
            }

            if (pChainDevice)
                pChainDevice->Release();
        }

        using CreateRenderTargetViewFn = void(__stdcall*)(ID3D12Device*, ID3D12Resource*, const D3D12_RENDER_TARGET_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE);
        using CreateDevice12Fn = HRESULT(__stdcall*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
        using DrawInstanced12Fn = void(__stdcall*)(ID3D12GraphicsCommandList*, UINT, UINT, UINT, UINT);
        using DrawIndexedInstanced12Fn = void(__stdcall*)(ID3D12GraphicsCommandList*, UINT, UINT, UINT, INT, UINT);
        using SetPipelineStateFn = void(__stdcall*)(ID3D12GraphicsCommandList*, ID3D12PipelineState*);
        using OMSetRenderTargets12Fn = void(__stdcall*)(ID3D12GraphicsCommandList*, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE*, BOOL, const D3D12_CPU_DESCRIPTOR_HANDLE*);
        using ResetFn = HRESULT(__stdcall*)(ID3D12GraphicsCommandList*, ID3D12CommandAllocator*, ID3D12PipelineState*);
        using SetDescriptorHeapsFn = void(__stdcall*)(ID3D12GraphicsCommandList*, UINT, ID3D12DescriptorHeap* const*);
        using SetGraphicsRootSignatureFn = void(__stdcall*)(ID3D12GraphicsCommandList*, ID3D12RootSignature*);
        using SetGraphicsRootDescriptorTableFn = void(__stdcall*)(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE);
        using SetGraphicsRootViewFn = void(__stdcall*)(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_VIRTUAL_ADDRESS);
        using SetGraphicsRoot32BitConstantFn = void(__stdcall*)(ID3D12GraphicsCommandList*, UINT, UINT, UINT);
        using SetGraphicsRoot32BitConstantsFn = void(__stdcall*)(ID3D12GraphicsCommandList*, UINT, UINT, const void*, UINT);
        using IASetVertexBuffersFn = void(__stdcall*)(ID3D12GraphicsCommandList*, UINT, UINT, const D3D12_VERTEX_BUFFER_VIEW*);
        using IASetIndexBufferFn = void(__stdcall*)(ID3D12GraphicsCommandList*, const D3D12_INDEX_BUFFER_VIEW*);
        using IASetPrimitiveTopologyFn = void(__stdcall*)(ID3D12GraphicsCommandList*, D3D12_PRIMITIVE_TOPOLOGY);
        using RSSetViewportsFn = void(__stdcall*)(ID3D12GraphicsCommandList*, UINT, const D3D12_VIEWPORT*);
        using RSSetScissorRectsFn = void(__stdcall*)(ID3D12GraphicsCommandList*, UINT, const D3D12_RECT*);
        using ExecuteCommandListsFn = void(__stdcall*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

        static inline CreateRenderTargetViewFn pCreateRenderTargetView = nullptr;
        static inline CreateDevice12Fn pCreateDevice12 = nullptr;
        static inline DrawInstanced12Fn pDrawInstanced12 = nullptr;
        static inline DrawIndexedInstanced12Fn pDrawIndexedInstanced12 = nullptr;
        static inline SetPipelineStateFn pSetPipelineState = nullptr;
        static inline OMSetRenderTargets12Fn pOMSetRenderTargets12 = nullptr;
        static inline ResetFn pReset12 = nullptr;
        static inline SetDescriptorHeapsFn pSetDescriptorHeaps = nullptr;
        static inline SetGraphicsRootSignatureFn pSetGraphicsRootSignature = nullptr;
        static inline SetGraphicsRootDescriptorTableFn pSetGraphicsRootDescriptorTable = nullptr;
        static inline SetGraphicsRootViewFn pSetGraphicsRootConstantBufferView = nullptr;
        static inline SetGraphicsRootViewFn pSetGraphicsRootShaderResourceView = nullptr;
        static inline SetGraphicsRootViewFn pSetGraphicsRootUnorderedAccessView = nullptr;
        static inline SetGraphicsRoot32BitConstantFn pSetGraphicsRoot32BitConstant = nullptr;
        static inline SetGraphicsRoot32BitConstantsFn pSetGraphicsRoot32BitConstants = nullptr;
        static inline IASetVertexBuffersFn pIASetVertexBuffers = nullptr;
        static inline IASetIndexBufferFn pIASetIndexBuffer = nullptr;
        static inline IASetPrimitiveTopologyFn pIASetPrimitiveTopology = nullptr;
        static inline RSSetViewportsFn pRSSetViewports = nullptr;
        static inline RSSetScissorRectsFn pRSSetScissorRects = nullptr;
        static inline ExecuteCommandListsFn pExecuteCommandLists = nullptr;

        static void __stdcall CreateRenderTargetViewHook(ID3D12Device* pDevice, ID3D12Resource* pResource,
            const D3D12_RENDER_TARGET_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (numDx12RenderTargetViews < MaxDx12RenderTargetViews)
                {
                    dx12RenderTargetViews[numDx12RenderTargetViews].handle = DestDescriptor.ptr;
                    dx12RenderTargetViews[numDx12RenderTargetViews].pResource = pResource;
                    ++numDx12RenderTargetViews;
                }
            }

            pCreateRenderTargetView(pDevice, pResource, pDesc, DestDescriptor);
        }

        // A draw that lands in the buffer that is presented is reported to the caller
        // before it is recorded, which is the only place a draw of the caller's can be
        // put in the middle of a frame of this API, see the note of Frame. What the
        // caller records belongs to the state of the list, so it says that it recorded
        // something, and what the game had set is put back for it afterwards.
        static inline void PresentedDrawD3D12(ID3D12GraphicsCommandList* pList)
        {
            if (bInsideDraw)
                return;

            const void* pPipelineState = nullptr;

            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                const auto it = dx12Lists.find(pList);

                if (it == dx12Lists.end() || !it->second.bTargetIsPresented)
                    return;

                pPipelineState = it->second.pLastPipelineState;
            }

            bool bRecorded = false;

            bInsideDraw = true;
            onPresentedDrawD3D12Event(pList, pPipelineState, bRecorded);
            bInsideDraw = false;

            if (bRecorded)
                RestoreListState(pList);
        }

        // Puts the state of a list back to what the game last set on it: the heaps come
        // before the tables that are in them, and the signature before the parameters
        // it describes. The shader of the game is put back as well, and it has to be:
        // the game keeps its own record of what it has set, and a draw that shares its
        // shader with the one before it is drawn without it being set again.
        static inline void RestoreListState(ID3D12GraphicsCommandList* pList)
        {
            ListState state;

            {
                std::lock_guard<std::mutex> guard(dx12Lock);
                state = dx12Lists[pList].state;
            }

            bInsideDraw = true;

            if (state.numHeaps)
                pList->SetDescriptorHeaps(state.numHeaps, state.pHeaps);

            if (state.pRootSignature)
            {
                pList->SetGraphicsRootSignature(state.pRootSignature);

                for (int i = 0; i < ListState::MaxRootParameters; i++)
                {
                    const ListState::RootParameter& parameter = state.rootParameters[i];

                    switch (parameter.kind)
                    {
                        case 1: pList->SetGraphicsRootDescriptorTable(i, parameter.table); break;
                        case 2: pList->SetGraphicsRootConstantBufferView(i, parameter.view); break;
                        case 3: pList->SetGraphicsRootShaderResourceView(i, parameter.view); break;
                        case 4: pList->SetGraphicsRootUnorderedAccessView(i, parameter.view); break;
                        case 5:
                            if (parameter.numConstants)
                                pList->SetGraphicsRoot32BitConstants(i, parameter.numConstants, parameter.constants, 0);
                            break;
                        default: break;
                    }
                }
            }

            // The shader of the game is put back as well, and it has to be: the game
            // keeps its own record of what it has set, and a draw that shares its
            // shader with the one before it is drawn without it being set again.
            if (state.pPipelineState)
                pList->SetPipelineState(state.pPipelineState);

            if (state.numTargets)
                pList->OMSetRenderTargets(state.numTargets, state.targets, FALSE, state.bHasDepth ? &state.depth : nullptr);

            if (state.numVertexBuffers)
                pList->IASetVertexBuffers(0, state.numVertexBuffers, state.vertexBuffers);

            if (state.bHasIndexBuffer)
                pList->IASetIndexBuffer(&state.indexBuffer);

            if (state.topology != D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
                pList->IASetPrimitiveTopology(state.topology);

            if (state.numViewports)
                pList->RSSetViewports(state.numViewports, state.viewports);

            if (state.numScissors)
                pList->RSSetScissorRects(state.numScissors, state.scissors);

            bInsideDraw = false;
        }

        static void __stdcall DrawInstancedHook12(ID3D12GraphicsCommandList* pList, UINT VertexCountPerInstance,
            UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
        {
            PresentedDrawD3D12(pList);

            pDrawInstanced12(pList, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
        }

        static void __stdcall DrawIndexedInstancedHook(ID3D12GraphicsCommandList* pList, UINT IndexCountPerInstance,
            UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
        {
            PresentedDrawD3D12(pList);

            pDrawIndexedInstanced12(pList, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
        }

        static void __stdcall SetPipelineStateHook(ID3D12GraphicsCommandList* pList, ID3D12PipelineState* pPipelineState)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                // The draws of the caller are not the draws of the frame, so the shader
                // they use is not what the frame is read from.
                if (!bInsideDraw)
                {
                    ListInfo& info = dx12Lists[pList];
                    info.pLastPipelineState = pPipelineState;
                    info.state.pPipelineState = pPipelineState;
                }
            }

            pSetPipelineState(pList, pPipelineState);
        }

        static void __stdcall OMSetRenderTargetsHook12(ID3D12GraphicsCommandList* pList, UINT NumRenderTargetDescriptors,
            const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange,
            const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                ListInfo& info = dx12Lists[pList];

                info.bTargetIsPresented = false;

                if (NumRenderTargetDescriptors && pRenderTargetDescriptors)
                    info.bTargetIsPresented = IsPresentedHandle(pRenderTargetDescriptors[0].ptr);

                if (!bInsideDraw)
                {
                    ListState& state = info.state;
                    state.numTargets = (NumRenderTargetDescriptors < (UINT)ListState::MaxTargets) ? NumRenderTargetDescriptors : (UINT)ListState::MaxTargets;

                    for (UINT i = 0; i < state.numTargets; i++)
                        state.targets[i] = pRenderTargetDescriptors[i];

                    state.bHasDepth = (pDepthStencilDescriptor != nullptr);

                    if (pDepthStencilDescriptor)
                        state.depth = *pDepthStencilDescriptor;
                }
            }

            pOMSetRenderTargets12(pList, NumRenderTargetDescriptors, pRenderTargetDescriptors,
                RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
        }

        static void __stdcall SetDescriptorHeapsHook(ID3D12GraphicsCommandList* pList, UINT NumDescriptorHeaps,
            ID3D12DescriptorHeap* const* ppDescriptorHeaps)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (!bInsideDraw)
                {
                    ListState& state = dx12Lists[pList].state;
                    state.numHeaps = 0;

                    for (UINT i = 0; i < NumDescriptorHeaps && state.numHeaps < 2; i++)
                        state.pHeaps[state.numHeaps++] = ppDescriptorHeaps[i];
                }
            }

            pSetDescriptorHeaps(pList, NumDescriptorHeaps, ppDescriptorHeaps);
        }

        static void __stdcall SetGraphicsRootSignatureHook(ID3D12GraphicsCommandList* pList, ID3D12RootSignature* pRootSignature)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (!bInsideDraw)
                {
                    ListState& state = dx12Lists[pList].state;
                    state.pRootSignature = pRootSignature;

                    // a signature leaves every parameter of the one before it undefined
                    for (auto& parameter : state.rootParameters)
                        parameter.kind = 0;
                }
            }

            pSetGraphicsRootSignature(pList, pRootSignature);
        }

        static void __stdcall SetGraphicsRootDescriptorTableHook(ID3D12GraphicsCommandList* pList, UINT RootParameterIndex,
            D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (!bInsideDraw && RootParameterIndex < ListState::MaxRootParameters)
                {
                    ListState::RootParameter& parameter = dx12Lists[pList].state.rootParameters[RootParameterIndex];
                    parameter.kind = 1;
                    parameter.table = BaseDescriptor;
                }
            }

            pSetGraphicsRootDescriptorTable(pList, RootParameterIndex, BaseDescriptor);
        }

        static void __stdcall SetGraphicsRootConstantBufferViewHook(ID3D12GraphicsCommandList* pList, UINT RootParameterIndex,
            D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (!bInsideDraw && RootParameterIndex < ListState::MaxRootParameters)
                {
                    ListState::RootParameter& parameter = dx12Lists[pList].state.rootParameters[RootParameterIndex];
                    parameter.kind = 2;
                    parameter.view = BufferLocation;
                }
            }

            pSetGraphicsRootConstantBufferView(pList, RootParameterIndex, BufferLocation);
        }

        static void __stdcall SetGraphicsRootShaderResourceViewHook(ID3D12GraphicsCommandList* pList, UINT RootParameterIndex,
            D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (!bInsideDraw && RootParameterIndex < ListState::MaxRootParameters)
                {
                    ListState::RootParameter& parameter = dx12Lists[pList].state.rootParameters[RootParameterIndex];
                    parameter.kind = 3;
                    parameter.view = BufferLocation;
                }
            }

            pSetGraphicsRootShaderResourceView(pList, RootParameterIndex, BufferLocation);
        }

        static void __stdcall SetGraphicsRootUnorderedAccessViewHook(ID3D12GraphicsCommandList* pList, UINT RootParameterIndex,
            D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (!bInsideDraw && RootParameterIndex < ListState::MaxRootParameters)
                {
                    ListState::RootParameter& parameter = dx12Lists[pList].state.rootParameters[RootParameterIndex];
                    parameter.kind = 4;
                    parameter.view = BufferLocation;
                }
            }

            pSetGraphicsRootUnorderedAccessView(pList, RootParameterIndex, BufferLocation);
        }

        static void __stdcall SetGraphicsRoot32BitConstantHook(ID3D12GraphicsCommandList* pList, UINT RootParameterIndex,
            UINT SrcData, UINT DestOffsetIn32BitValues)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (!bInsideDraw && RootParameterIndex < ListState::MaxRootParameters &&
                    DestOffsetIn32BitValues < ListState::MaxRootConstants)
                {
                    ListState::RootParameter& parameter = dx12Lists[pList].state.rootParameters[RootParameterIndex];
                    parameter.kind = 5;
                    parameter.constants[DestOffsetIn32BitValues] = SrcData;

                    if (parameter.numConstants < DestOffsetIn32BitValues + 1)
                        parameter.numConstants = DestOffsetIn32BitValues + 1;
                }
            }

            pSetGraphicsRoot32BitConstant(pList, RootParameterIndex, SrcData, DestOffsetIn32BitValues);
        }

        static void __stdcall SetGraphicsRoot32BitConstantsHook(ID3D12GraphicsCommandList* pList, UINT RootParameterIndex,
            UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (!bInsideDraw && RootParameterIndex < ListState::MaxRootParameters)
                {
                    ListState::RootParameter& parameter = dx12Lists[pList].state.rootParameters[RootParameterIndex];
                    parameter.kind = 5;

                    for (UINT i = 0; i < Num32BitValuesToSet; i++)
                    {
                        const UINT at = DestOffsetIn32BitValues + i;

                        if (at >= ListState::MaxRootConstants)
                            break;

                        parameter.constants[at] = ((const UINT*)pSrcData)[i];

                        if (parameter.numConstants < at + 1)
                            parameter.numConstants = at + 1;
                    }
                }
            }

            pSetGraphicsRoot32BitConstants(pList, RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
        }

        static void __stdcall IASetVertexBuffersHook(ID3D12GraphicsCommandList* pList, UINT StartSlot, UINT NumViews,
            const D3D12_VERTEX_BUFFER_VIEW* pViews)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (!bInsideDraw)
                {
                    ListState& state = dx12Lists[pList].state;

                    for (UINT i = 0; i < NumViews; i++)
                    {
                        const UINT slot = StartSlot + i;

                        if (slot >= ListState::MaxVertexBuffers)
                            break;

                        state.vertexBuffers[slot] = pViews[i];

                        if (state.numVertexBuffers < slot + 1)
                            state.numVertexBuffers = slot + 1;
                    }
                }
            }

            pIASetVertexBuffers(pList, StartSlot, NumViews, pViews);
        }

        static void __stdcall IASetIndexBufferHook(ID3D12GraphicsCommandList* pList, const D3D12_INDEX_BUFFER_VIEW* pView)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (!bInsideDraw)
                {
                    ListState& state = dx12Lists[pList].state;
                    state.bHasIndexBuffer = (pView != nullptr);

                    if (pView)
                        state.indexBuffer = *pView;
                }
            }

            pIASetIndexBuffer(pList, pView);
        }

        static void __stdcall IASetPrimitiveTopologyHook(ID3D12GraphicsCommandList* pList, D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (!bInsideDraw)
                    dx12Lists[pList].state.topology = PrimitiveTopology;
            }

            pIASetPrimitiveTopology(pList, PrimitiveTopology);
        }

        static void __stdcall RSSetViewportsHook(ID3D12GraphicsCommandList* pList, UINT NumViewports, const D3D12_VIEWPORT* pViewports)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (!bInsideDraw)
                {
                    ListState& state = dx12Lists[pList].state;
                    state.numViewports = (NumViewports < (UINT)ListState::MaxViewports) ? NumViewports : (UINT)ListState::MaxViewports;

                    for (UINT i = 0; i < state.numViewports; i++)
                        state.viewports[i] = pViewports[i];
                }
            }

            pRSSetViewports(pList, NumViewports, pViewports);
        }

        static void __stdcall RSSetScissorRectsHook(ID3D12GraphicsCommandList* pList, UINT NumRects, const D3D12_RECT* pRects)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);

                if (!bInsideDraw)
                {
                    ListState& state = dx12Lists[pList].state;
                    state.numScissors = (NumRects < (UINT)ListState::MaxViewports) ? NumRects : (UINT)ListState::MaxViewports;

                    for (UINT i = 0; i < state.numScissors; i++)
                        state.scissors[i] = pRects[i];
                }
            }

            pRSSetScissorRects(pList, NumRects, pRects);
        }

        static HRESULT __stdcall ResetHook(ID3D12GraphicsCommandList* pList, ID3D12CommandAllocator* pAllocator, ID3D12PipelineState* pInitialState)
        {
            {
                std::lock_guard<std::mutex> guard(dx12Lock);
                dx12Lists.erase(pList);
            }

            return pReset12(pList, pAllocator, pInitialState);
        }

        // The table of methods of a command list is shared by all of them, so one is
        // patched, and it is the first one that is ever submitted, which is where a
        // list pointer is first seen.
        static inline void InstallD3D12ListHooks(void* pCommandList)
        {
            if (!pCommandList || bListHooksInstalled)
                return;

            void** pVtable = *(void***)pCommandList;

            ID3D12GraphicsCommandListVTBL table;

            pReset12 = (ResetFn)Hook(pVtable[table.GetIndex("Reset")], ResetHook);
            pDrawInstanced12 = (DrawInstanced12Fn)Hook(pVtable[table.GetIndex("DrawInstanced")], DrawInstancedHook12);
            pDrawIndexedInstanced12 = (DrawIndexedInstanced12Fn)Hook(pVtable[table.GetIndex("DrawIndexedInstanced")], DrawIndexedInstancedHook);
            pSetPipelineState = (SetPipelineStateFn)Hook(pVtable[table.GetIndex("SetPipelineState")], SetPipelineStateHook);
            pOMSetRenderTargets12 = (OMSetRenderTargets12Fn)Hook(pVtable[table.GetIndex("OMSetRenderTargets")], OMSetRenderTargetsHook12);

            pSetDescriptorHeaps = (SetDescriptorHeapsFn)Hook(pVtable[table.GetIndex("SetDescriptorHeaps")], SetDescriptorHeapsHook);
            pSetGraphicsRootSignature = (SetGraphicsRootSignatureFn)Hook(pVtable[table.GetIndex("SetGraphicsRootSignature")], SetGraphicsRootSignatureHook);
            pSetGraphicsRootDescriptorTable = (SetGraphicsRootDescriptorTableFn)Hook(pVtable[table.GetIndex("SetGraphicsRootDescriptorTable")], SetGraphicsRootDescriptorTableHook);
            pSetGraphicsRootConstantBufferView = (SetGraphicsRootViewFn)Hook(pVtable[table.GetIndex("SetGraphicsRootConstantBufferView")], SetGraphicsRootConstantBufferViewHook);
            pSetGraphicsRootShaderResourceView = (SetGraphicsRootViewFn)Hook(pVtable[table.GetIndex("SetGraphicsRootShaderResourceView")], SetGraphicsRootShaderResourceViewHook);
            pSetGraphicsRootUnorderedAccessView = (SetGraphicsRootViewFn)Hook(pVtable[table.GetIndex("SetGraphicsRootUnorderedAccessView")], SetGraphicsRootUnorderedAccessViewHook);
            pSetGraphicsRoot32BitConstant = (SetGraphicsRoot32BitConstantFn)Hook(pVtable[table.GetIndex("SetGraphicsRoot32BitConstant")], SetGraphicsRoot32BitConstantHook);
            pSetGraphicsRoot32BitConstants = (SetGraphicsRoot32BitConstantsFn)Hook(pVtable[table.GetIndex("SetGraphicsRoot32BitConstants")], SetGraphicsRoot32BitConstantsHook);
            pIASetVertexBuffers = (IASetVertexBuffersFn)Hook(pVtable[table.GetIndex("IASetVertexBuffers")], IASetVertexBuffersHook);
            pIASetIndexBuffer = (IASetIndexBufferFn)Hook(pVtable[table.GetIndex("IASetIndexBuffer")], IASetIndexBufferHook);
            pIASetPrimitiveTopology = (IASetPrimitiveTopologyFn)Hook(pVtable[table.GetIndex("IASetPrimitiveTopology")], IASetPrimitiveTopologyHook);
            pRSSetViewports = (RSSetViewportsFn)Hook(pVtable[table.GetIndex("RSSetViewports")], RSSetViewportsHook);
            pRSSetScissorRects = (RSSetScissorRectsFn)Hook(pVtable[table.GetIndex("RSSetScissorRects")], RSSetScissorRectsHook);

            // The draws are what everything else is for, so a list that cannot be read
            // is not watched at all rather than watched halfway.
            if (!pDrawInstanced12 || !pDrawIndexedInstanced12 || !pSetPipelineState || !pOMSetRenderTargets12)
                return;

            bListHooksInstalled = true;
        }

        static void __stdcall ExecuteCommandListsHook(ID3D12CommandQueue* pQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists)
        {
            // The queue of a frame is the one it is submitted on, and the submissions of
            // anything else are made on one of its own. The reference is taken here,
            // because the one the caller holds is not ours to keep, and a queue that is
            // used after its last reference is gone takes the driver down with it.
            if (!bInsideDraw && !pGameQueue)
            {
                pGameQueue = pQueue;
                pQueue->AddRef();
                pQueue->GetDevice(__uuidof(ID3D12Device), (void**)&pGameQueueDevice);
            }

            // What is submitted here is read before it is let through, which is where a
            // frame of this API that is carried by more than one submission can still
            // be drawn at, see the note of onSubmitD3D12Event.
            if (!bInsideDraw)
            {
                bInsideDraw = true;
                onSubmitD3D12Event(pQueue, NumCommandLists, ppCommandLists);
                bInsideDraw = false;
            }

            if (NumCommandLists && ppCommandLists)
                InstallD3D12ListHooks((void*)ppCommandLists[0]);

            pExecuteCommandLists(pQueue, NumCommandLists, ppCommandLists);
        }

        static inline void InstallD3D12QueueHooks(ID3D12CommandQueue* pQueue)
        {
            if (!pQueue || bQueueHooksInstalled)
                return;

            IDirect3DDevice12CommandQueueVTBL table;

            pExecuteCommandLists = (ExecuteCommandListsFn)Hook((*(void***)pQueue)[table.GetIndex("ExecuteCommandLists")], ExecuteCommandListsHook);

            if (!pExecuteCommandLists)
                return;

            bQueueHooksInstalled = true;
        }

        // The queue of the game cannot be reached from the swap chain, and what is
        // shared by all the queues of a device is the table their methods are in, so a
        // queue of our own is made once, only to get at it: nothing is ever submitted
        // on it, and what is patched is the method of the table, which is the one the
        // queue of the game calls.
        static inline void InstallD3D12QueueHooksFromOwnQueue()
        {
            if (bQueueHooksInstalled)
                return;

            HMODULE hD3D12 = GetModuleHandleA("d3d12.dll");

            if (!hD3D12)
                return;

            auto pProc = (CreateDevice12Fn)GetProcAddress(hD3D12, "D3D12CreateDevice");

            if (!pProc)
                return;

            ID3D12Device* pDevice = nullptr;

            if (FAILED(pProc(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&pDevice)) || !pDevice)
                return;

            D3D12_COMMAND_QUEUE_DESC desc{};
            desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

            ID3D12CommandQueue* pQueue = nullptr;

            if (SUCCEEDED(pDevice->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), (void**)&pQueue)))
            {
                InstallD3D12QueueHooks(pQueue);
                pQueue->Release();
            }

            pDevice->Release();
        }

        // A queue made on the device of the game is the one that shares its table with
        // the queues the game draws with, which is the surest way at it: the device is
        // what the chain of a frame hands out.
        static inline void InstallD3D12QueueHooksFromGameDevice(IDXGISwapChain* pSwapChain)
        {
            if (bQueueHooksInstalled)
                return;

            ID3D12Device* pDevice = nullptr;
            ID3D12CommandQueue* pFromChain = nullptr;

            pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&pDevice);
            pSwapChain->GetDevice(__uuidof(ID3D12CommandQueue), (void**)&pFromChain);

            if (pFromChain)
            {
                InstallD3D12QueueHooks(pFromChain);
                pFromChain->Release();
            }

            if (!bQueueHooksInstalled && pDevice)
            {
                D3D12_COMMAND_QUEUE_DESC desc{};
                desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

                ID3D12CommandQueue* pQueue = nullptr;

                if (SUCCEEDED(pDevice->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), (void**)&pQueue)))
                {
                    InstallD3D12QueueHooks(pQueue);
                    pQueue->Release();
                }
            }

            if (pDevice)
                pDevice->Release();
        }

        // The descriptors of the buffers of the chain are what tells a draw into the
        // buffer that is presented from every other draw, and they are all made long
        // before a frame is recorded, so the device is hooked from the call that makes
        // it, which is before the chain is made at all.
        static inline void InstallD3D12DeviceVtableHooks(IUnknown* pUnknown)
        {
            if (bDeviceHooksInstalled || !pUnknown)
                return;

            ID3D12Device* pDevice = nullptr;

            if (FAILED(pUnknown->QueryInterface(__uuidof(ID3D12Device), (void**)&pDevice)) || !pDevice)
                return;

            ID3D12DeviceVTBL table;

            pCreateRenderTargetView = (CreateRenderTargetViewFn)Hook((*(void***)pDevice)[table.GetIndex("CreateRenderTargetView")], CreateRenderTargetViewHook);

            pDevice->Release();

            if (!pCreateRenderTargetView)
                return;

            bDeviceHooksInstalled = true;
        }

        static HRESULT __stdcall D3D12CreateDeviceHook(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice)
        {
            const HRESULT hr = pCreateDevice12(pAdapter, MinimumFeatureLevel, riid, ppDevice);

            if (SUCCEEDED(hr) && ppDevice && *ppDevice)
                InstallD3D12DeviceVtableHooks((IUnknown*)*ppDevice);

            return hr;
        }

        static inline void InstallD3D12Hooks()
        {
            // Loaded here on purpose: the game loads it on its own only once it is about
            // to make a device, and by then it would already be too late for the
            // descriptors of a chain.
            HMODULE hD3D12 = LoadLibraryA("d3d12.dll");

            if (!hD3D12)
                return;

            void* pProc = (void*)GetProcAddress(hD3D12, "D3D12CreateDevice");

            if (!pProc)
                return;

            pCreateDevice12 = (CreateDevice12Fn)Hook(pProc, D3D12CreateDeviceHook);
        }
#endif // FUSIONDXHOOK_INCLUDE_D3D12
    };
#endif // FUSIONDXHOOK_INCLUDE_D3D11 || FUSIONDXHOOK_INCLUDE_D3D12

public:
    static inline void Init()
    {
        // doesn't work without a thread
        DllCallbackHandler::RegisterCallback(L"d3d8.dll",     []() { std::thread([]() { HookD3D8();    }).detach(); });
        DllCallbackHandler::RegisterCallback(L"d3d9.dll",     []() { std::thread([]() { HookD3D9();    }).detach(); });
        DllCallbackHandler::RegisterCallback(L"d3d10.dll",    []() { std::thread([]() { HookD3D10();   }).detach(); });
        DllCallbackHandler::RegisterCallback(L"d3d10_1.dll",  []() { std::thread([]() { HookD3D10_1(); }).detach(); });
        DllCallbackHandler::RegisterCallback(L"d3d11.dll",    []() { std::thread([]() { HookD3D11();   }).detach(); });
        DllCallbackHandler::RegisterCallback(L"d3d12.dll",    []() { std::thread([]() { HookD3D12();   }).detach(); });
        DllCallbackHandler::RegisterCallback(L"opengl32.dll", []() { std::thread([]() { HookOPENGL();  }).detach(); });
        // The Vulkan loader is loaded, freed and loaded again while a session
        // runs, and the device is created right after each load, so this one is
        // installed on the thread that loads the module instead of on a thread
        // of its own with a delay in front of it.
        DllCallbackHandler::RegisterCallback(L"vulkan-1.dll", []() { HookVULKAN();  });

        onInitEvent();
    }

    static inline void DeInit()
    {
        onShutdownEvent();
    }

#if FUSIONDXHOOK_USE_SAFETYHOOK
    static inline void bind(HMODULE module, std::type_index type_index, uint16_t func_index, void* function, SafetyHookInline& hook)
    {
        auto target = deviceMethods.at(module).at(type_index).at(func_index);
        hook = safetyhook::create_inline(target, function);
    }

    // Does the function already begin with a jump to this destination? The hooks
    // belong to a module image and are installed again for every load, a second
    // call for the same image must not chain another jump in front of the first.
    static inline bool IsHookedBy(void* target, void* destination)
    {
        if (!target || !destination)
            return false;

        auto* code = (uint8_t*)target;
        int32_t offset = 0;

        if (code[0] == 0xE9)                                    // jmp rel32
        {
            memcpy(&offset, code + 1, sizeof(offset));
            return (void*)(code + 5 + offset) == destination;
        }

        if (code[0] == 0xFF && code[1] == 0x25)                 // jmp qword ptr [rip + rel32]
        {
            memcpy(&offset, code + 2, sizeof(offset));
            return *(void**)(code + 6 + offset) == destination;
        }

        return false;
    }

    // Does a jump begin the function at all, no matter where it goes? A hook is not
    // recognizable at the address itself: safetyhook puts a jump into its own
    // trampoline and only the jump at the end of that trampoline goes on to the hook
    // function, so a comparison of the jump destination with the hook function never
    // finds a hook that is already there.
    static inline bool BeginsWithJump(void* target)
    {
        if (!target)
            return false;

        auto* code = (uint8_t*)target;

        return code[0] == 0xE9 || (code[0] == 0xFF && code[1] == 0x25);
    }

    // The addresses that were hooked, and for which destination. Hooking an address a
    // second time takes the jump of the first hook for the original code of the
    // function, and the trampoline of the second hook then ends in the first hook
    // again: passing the call on runs the hook once more, and the calls go round in
    // circles until the stack is gone. A driver hands the same address out for every
    // device it creates, so this is the ordinary case and not an edge case.
    struct HookedAddress
    {
        void* target;
        void* destination;
    };

    static inline std::vector<HookedAddress>& HookedAddresses()
    {
        static std::vector<HookedAddress> addresses;
        return addresses;
    }

    static inline std::mutex& HookedAddressesLock()
    {
        static std::mutex lock;
        return lock;
    }

    // Creates the hook and returns the object that keeps it alive, unless the address
    // is hooked for this destination already.
    static inline SafetyHookInline* CreateHook(void* target, void* destination, SafetyHookInline* pCurrent)
    {
        if (!target || !destination)
            return pCurrent;

        std::scoped_lock guard(HookedAddressesLock());

        // A jump that goes straight to the hook function.
        if (IsHookedBy(target, destination))
            return pCurrent;

        if (BeginsWithJump(target))
        {
            for (const auto& entry : HookedAddresses())
            {
                // A game or an emulator can unload a module and load it again, PPSSPP
                // frees the Vulkan loader right after asking whether Vulkan is
                // available at all, and every load is a fresh image with a copy of the
                // original code in it. That copy has no jump and has to be hooked, so
                // the jump decides and not the address alone.
                if (entry.target == target && entry.destination == destination)
                    return pCurrent;
            }
        }

        auto hook = safetyhook::InlineHook::create(target, destination);

        if (!hook)
            return pCurrent;

        HookedAddresses().push_back({target, destination});

        return new SafetyHookInline(std::move(*hook));
    }

    // Installs a hook into the module image that is loaded right now and returns
    // the object that keeps it alive.
    //
    // A game or an emulator can unload a module and load it again, PPSSPP frees
    // the Vulkan loader right after asking whether Vulkan is available at all and
    // loads it again when it goes on to use it, and every load is a fresh image
    // that has no trace of the hooks in it. The object of an earlier load is left
    // alone on purpose: resetting it would write into code that is not mapped any
    // more.
    static inline SafetyHookInline* InstallHook(HMODULE module, std::type_index type_index, uint16_t func_index, void* destination, SafetyHookInline* pCurrent)
    {
        auto target = deviceMethods.at(module).at(type_index).at(func_index);

        return CreateHook(target, destination, pCurrent);
    }
    static inline void unbind(SafetyHookInline& hook)
    {
        hook.reset();
    }

    // The same, for an address that was handed out by the API itself. A Vulkan
    // driver hands its device functions out with vkGetDeviceProcAddr and they
    // live in the driver, not in the module that exports the entry points.
    static inline SafetyHookInline* InstallAddressHook(void* target, void* destination, SafetyHookInline* pCurrent)
    {
        return CreateHook(target, destination, pCurrent);
    }
#endif

    // Direct3D 11 and Direct3D 12 present through the very same function of dxgi,
    // so both hooks end up on one address and both run for every frame. This is
    // what tells the two apart: a swap chain of Direct3D 12 hands out Direct3D 12
    // resources, one of Direct3D 11 hands out textures.
    enum class SwapChainKind
    {
        Unknown,
        Direct3D11,
        Direct3D12,
    };

    static inline SwapChainKind GetSwapChainKind(IDXGISwapChain* pSwapChain)
    {
        if (!pSwapChain)
            return SwapChainKind::Unknown;

        #if FUSIONDXHOOK_INCLUDE_D3D12
        ID3D12Resource* pResource12 = nullptr;
        if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D12Resource), (void**)&pResource12)) && pResource12)
        {
            pResource12->Release();
            return SwapChainKind::Direct3D12;
        }
        #endif

        #if FUSIONDXHOOK_INCLUDE_D3D11 || FUSIONDXHOOK_INCLUDE_D3D10 || FUSIONDXHOOK_INCLUDE_D3D10_1
        ID3D11Texture2D* pTexture11 = nullptr;
        if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pTexture11)) && pTexture11)
        {
            pTexture11->Release();
            return SwapChainKind::Direct3D11;
        }
        #endif

        return SwapChainKind::Unknown;
    }

private:
    class DllCallbackHandler
    {
    public:
        static inline void RegisterCallback(std::wstring_view module_name, std::function<void()>&& fn)
        {
            if (module_name.empty() || GetModuleHandleW(module_name.data()) != NULL)
            {
                fn();
            }
            else
            {
                RegisterDllNotification();
                std::wstring s(module_name);
                std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                GetCallbackList().emplace(s, std::forward<std::function<void()>>(fn));
            }
        }
        static inline void RegisterUnloadCallback(std::wstring_view module_name, std::function<void()>&& fn)
        {
            RegisterDllNotification();
            std::wstring s(module_name);
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            GetShutdownList().emplace(s, std::forward<std::function<void()>>(fn));
        }
    private:
        static inline void callOnLoad(std::wstring_view module_name)
        {
            std::wstring s(module_name);
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            if (GetCallbackList().count(s.data()))
            {
                GetCallbackList().at(s.data())();
            }
        }
        static inline void callOnUnload(std::wstring_view module_name)
        {
            std::wstring s(module_name);
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            if (GetShutdownList().count(s.data()))
            {
                GetShutdownList().at(s.data())();
            }
        }

    private:
        static inline std::map<std::wstring, std::function<void()>>& GetCallbackList()
        {
            return onLoad;
        }
        static inline std::map<std::wstring, std::function<void()>>& GetShutdownList()
        {
            return onUnload;
        }

        typedef NTSTATUS(NTAPI* _LdrRegisterDllNotification) (ULONG, PVOID, PVOID, PVOID);
        typedef NTSTATUS(NTAPI* _LdrUnregisterDllNotification) (PVOID);

        typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA
        {
            ULONG Flags;                    //Reserved.
            PUNICODE_STRING FullDllName;    //The full path name of the DLL module.
            PUNICODE_STRING BaseDllName;    //The base file name of the DLL module.
            PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
            ULONG SizeOfImage;              //The size of the DLL image, in bytes.
        } LDR_DLL_LOADED_NOTIFICATION_DATA, LDR_DLL_UNLOADED_NOTIFICATION_DATA, * PLDR_DLL_LOADED_NOTIFICATION_DATA, * PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

        typedef union _LDR_DLL_NOTIFICATION_DATA
        {
            LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
            LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
        } LDR_DLL_NOTIFICATION_DATA, * PLDR_DLL_NOTIFICATION_DATA;

        typedef NTSTATUS(NTAPI* PLDR_MANIFEST_PROBER_ROUTINE)
            (
                IN HMODULE DllBase,
                IN PCWSTR FullDllPath,
                OUT PHANDLE ActivationContext
                );

        typedef NTSTATUS(NTAPI* PLDR_ACTX_LANGUAGE_ROURINE)
            (
                IN HANDLE Unk,
                IN USHORT LangID,
                OUT PHANDLE ActivationContext
                );

        typedef void(NTAPI* PLDR_RELEASE_ACT_ROUTINE)
            (
                IN HANDLE ActivationContext
                );

        typedef VOID(NTAPI* fnLdrSetDllManifestProber)
            (
                IN PLDR_MANIFEST_PROBER_ROUTINE ManifestProberRoutine,
                IN PLDR_ACTX_LANGUAGE_ROURINE CreateActCtxLanguageRoutine,
                IN PLDR_RELEASE_ACT_ROUTINE ReleaseActCtxRoutine
                );

    private:
        static inline void CALLBACK LdrDllNotification(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context)
        {
            static constexpr auto LDR_DLL_NOTIFICATION_REASON_LOADED = 1;
            static constexpr auto LDR_DLL_NOTIFICATION_REASON_UNLOADED = 2;
            if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED)
            {
                callOnLoad(NotificationData->Loaded.BaseDllName->Buffer);
            }
            else if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_UNLOADED)
            {
                callOnUnload(NotificationData->Loaded.BaseDllName->Buffer);
            }
        }

        static inline NTSTATUS NTAPI ProbeCallback(IN HMODULE DllBase, IN PCWSTR FullDllPath, OUT PHANDLE ActivationContext)
        {
            std::wstring str(FullDllPath);
            callOnLoad(str.substr(str.find_last_of(L"/\\") + 1));

            HANDLE actx = NULL;
            ACTCTXW act = { 0 };

            act.cbSize = sizeof(act);
            act.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
            act.lpSource = FullDllPath;
            act.hModule = DllBase;
            act.lpResourceName = ISOLATIONAWARE_MANIFEST_RESOURCE_ID;
            *ActivationContext = 0;
            actx = CreateActCtxW(&act);
            if (actx == INVALID_HANDLE_VALUE)
                return 0xC000008B; //STATUS_RESOURCE_NAME_NOT_FOUND;
            *ActivationContext = actx;
            return STATUS_SUCCESS;
        }

        static inline void RegisterDllNotification()
        {
            LdrRegisterDllNotification = (_LdrRegisterDllNotification)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrRegisterDllNotification");
            if (LdrRegisterDllNotification)
            {
              if (!cookie)
                LdrRegisterDllNotification(0, LdrDllNotification, 0, &cookie);
            }
            else
            {
                LdrSetDllManifestProber = (fnLdrSetDllManifestProber)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrSetDllManifestProber");
                if (LdrSetDllManifestProber)
                {
                    LdrSetDllManifestProber(&ProbeCallback, NULL, &ReleaseActCtx);
                }
            }
        }

        static inline void UnRegisterDllNotification()
        {
            LdrUnregisterDllNotification = (_LdrUnregisterDllNotification)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrUnregisterDllNotification");
            if (LdrUnregisterDllNotification && cookie)
                LdrUnregisterDllNotification(cookie);
        }

    private:
        static inline _LdrRegisterDllNotification   LdrRegisterDllNotification;
        static inline _LdrUnregisterDllNotification LdrUnregisterDllNotification;
        static inline void* cookie;
        static inline fnLdrSetDllManifestProber     LdrSetDllManifestProber;
        static inline std::map<std::wstring, std::function<void()>> onLoad;
        static inline std::map<std::wstring, std::function<void()>> onUnload;
    };
};

#endif // __FUSIONDXHOOK_H__