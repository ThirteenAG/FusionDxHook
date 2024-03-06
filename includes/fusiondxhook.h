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
typedef void VkDevice;
typedef void VkPresentInfoKHR;
typedef enum VkResult {
    VK_SUCCESS = 0
} VkResult;
#if defined(_WIN32)
#define VKAPI_CALL __stdcall
#else
#define VKAPI_CALL
#endif
//#include <vulkan/vulkan.h>
#endif
#endif

#if FUSIONDXHOOK_USE_SAFETYHOOK
#include "safetyhook/safetyhook.hpp"
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
                    struct __declspec(uuid("7385e5df-8fe8-41d5-86b6-d7b48547b6cf")) IDirect3DDevice8;
                    IUnknown* pDevice = nullptr;
                    if (ptr->QueryInterface(__uuidof(IDirect3DDevice8), (void**)&pDevice) == S_OK)
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
    }
#else
    static inline void HookD3D10_1() {}
#endif

#if FUSIONDXHOOK_INCLUDE_D3D11
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

        auto D3D11CreateDeviceAndSwapChain = GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain");
        if (D3D11CreateDeviceAndSwapChain == NULL)
            return;

        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
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
            if (((HRESULT(WINAPI*)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**))
                (D3D11CreateDeviceAndSwapChain))(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevels, 2, D3D11_SDK_VERSION, &swapChainDesc, &SwapChain, &Device, &featureLevel, &Context) >= 0)
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
                    D3D11::onPresentEvent(pSwapChain);
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

                bind(hD3D11, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("Present"), Present, presentOriginal);
                bind(hD3D11, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("ResizeBuffers"), ResizeBuffers, resizeBuffersOriginal);
                bind(hD3D11, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("Release"), Release, releaseOriginal);
                #endif
                Context->Release();
                Device->Release();
                SwapChain->Release();
            }
        });
    }
#else
    static inline void HookD3D11() {}
#endif

#if FUSIONDXHOOK_INCLUDE_D3D12
public:
    struct D3D12 {
        static inline Event<> onInitEvent = {};
        static inline Event<> onShutdownEvent = {};
        static inline Event<IDXGISwapChain*> onPresentEvent = {};
        static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onBeforeResizeEvent = {};
        static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onAfterResizeEvent = {};
        static inline Event<> onReleaseEvent = {};
    };
private:
    static inline void HookD3D12()
    {
        DllCallbackHandler::RegisterUnloadCallback(L"d3d12.dll", []() { D3D12::onShutdownEvent(); });

        auto hD3D12 = GetModuleHandleW(L"d3d12.dll");
        if (!hD3D12) return;

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

                    #if FUSIONDXHOOK_USE_SAFETYHOOK
                    static SafetyHookInline presentOriginal = {};
                    static SafetyHookInline resizeBuffersOriginal = {};
                    static SafetyHookInline releaseOriginal = {};

                    auto D3D12Present = [](IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) -> HRESULT
                    {
                        D3D12::onPresentEvent(pSwapChain);
                        return presentOriginal.unsafe_stdcall<HRESULT>(pSwapChain, SyncInterval, Flags);
                    };

                    auto D3D12ResizeBuffers = [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) -> HRESULT
                    {
                        D3D12::onBeforeResizeEvent(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                        HRESULT result = resizeBuffersOriginal.unsafe_stdcall<HRESULT>(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                        D3D12::onAfterResizeEvent(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
                        return result;
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
                    static ULONG(WINAPI* Release)(IUnknown*) = D3D12Release;

                    _DELAYED_BIND

                    bind(hD3D12, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("Present"), Present, presentOriginal);
                    bind(hD3D12, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("ResizeBuffers"), ResizeBuffers, resizeBuffersOriginal);
                    bind(hD3D12, typeid(IDXGISwapChainVTBL), IDXGISwapChainVTBL().GetIndex("Release"), Release, releaseOriginal);
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
    static inline void HookOPENGL()
    {
        DllCallbackHandler::RegisterUnloadCallback(L"opengl32.dll", []() { OPENGL::onShutdownEvent(); });

        auto hOpenGL32 = GetModuleHandleW(L"opengl32.dll");
        if (!hOpenGL32) return;

        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
            OpenGLVTBL ogl;
            deviceMethods[hOpenGL32][typeid(OpenGLVTBL)].resize(ogl.GetNumberOfMethods());
            for (unsigned int i = 0; i < ogl.GetNumberOfMethods(); i++)
            {
                deviceMethods.at(hOpenGL32).at(typeid(OpenGLVTBL)).at(i) = (uintptr_t*)GetProcAddress(hOpenGL32, ogl.GetMethod(i));
            }

            #if FUSIONDXHOOK_USE_SAFETYHOOK
            static SafetyHookInline wglSwapBuffersOriginal = {};

            auto OpenGLwglSwapBuffers = [](HDC hDc) -> BOOL
            {
                OPENGL::onSwapBuffersEvent(hDc);
                return wglSwapBuffersOriginal.unsafe_stdcall<BOOL>(hDc);
            };

            static BOOL(__stdcall* wglSwapBuffers)(HDC) = OpenGLwglSwapBuffers;

            _DELAYED_BIND

            bind(hOpenGL32, typeid(OpenGLVTBL), ogl.GetIndex("wglSwapBuffers"), wglSwapBuffers, wglSwapBuffersOriginal);
            #endif
        });
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
    };
private:
    static inline void HookVULKAN()
    {
        DllCallbackHandler::RegisterUnloadCallback(L"vulkan-1.dll", []() { VULKAN::onShutdownEvent(); });

        auto hVulkan1 = GetModuleHandleW(L"vulkan-1.dll");
        if (!hVulkan1) return;

        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
            VulkanVTBL vk;
            deviceMethods[hVulkan1][typeid(VulkanVTBL)].resize(vk.GetNumberOfMethods());
            for (unsigned int i = 0; i < vk.GetNumberOfMethods(); i++)
            {
                deviceMethods.at(hVulkan1).at(typeid(VulkanVTBL)).at(i) = (uintptr_t*)GetProcAddress(hVulkan1, vk.GetMethod(i));
            }

            #if FUSIONDXHOOK_USE_SAFETYHOOK
            static SafetyHookInline vkQueuePresentKHROriginal = {};
            static SafetyHookInline vkCreateDeviceOriginal = {};

            auto VULKANvkQueuePresentKHR = [](VkQueue queue, const VkPresentInfoKHR* pPresentInfo) -> VkResult
            {
                VULKAN::onVkQueuePresentKHREvent(queue, pPresentInfo);
                return vkQueuePresentKHROriginal.unsafe_stdcall<VkResult>(queue, pPresentInfo);
            };

            auto VULKANvkCreateDevice = [](VkPhysicalDevice gpu, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) -> VkResult
            {
                VULKAN::onvkCreateDeviceEvent(gpu, pCreateInfo, pAllocator, pDevice);
                return vkCreateDeviceOriginal.unsafe_stdcall<VkResult>(gpu, pCreateInfo, pAllocator, pDevice);
            };

            static VkResult(VKAPI_CALL* vkQueuePresentKHR)(VkQueue, const VkPresentInfoKHR*) = VULKANvkQueuePresentKHR;
            static VkResult(VKAPI_CALL* vkCreateDevice)(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*) = VULKANvkCreateDevice;

            _DELAYED_BIND

            bind(hVulkan1, typeid(VulkanVTBL), vk.GetIndex("vkQueuePresentKHR"), vkQueuePresentKHR, vkQueuePresentKHROriginal);
            bind(hVulkan1, typeid(VulkanVTBL), vk.GetIndex("vkCreateDevice"), vkCreateDevice, vkCreateDeviceOriginal);
            #endif
        });
    }
#else
    static inline void HookVULKAN() {}
#endif

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
        DllCallbackHandler::RegisterCallback(L"vulkan-1.dll", []() { std::thread([]() { HookVULKAN();  }).detach(); });

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
    static inline void unbind(SafetyHookInline& hook)
    {
        hook.reset();
    }
#endif

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