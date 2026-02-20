// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
//
// dllmain.cpp — DLL entry point and exported functions for the dinput8 proxy.
//
// This DLL is placed alongside the game executable (e.g. DCS World).
// It loads the real system dinput8.dll, initialises config + logging,
// and wraps the DirectInput8 interfaces to intercept FFB operations.
//
#define INITGUID  // define GUIDs in this translation unit

#include <windows.h>
#include <dinput.h>

#include "proxy.h"
#include "config.h"
#include "logger.h"
#include "wrapper_dinput8.h"

// Globals
static wchar_t g_dllDirectory[MAX_PATH] = {};

// ============================================================================
// Initialisation helpers
// ============================================================================
static void initWrapper(HMODULE hSelf) {
    // Determine our DLL's directory (for config + log files)
    GetModuleFileNameW(hSelf, g_dllDirectory, MAX_PATH);
    // Strip filename to get the directory
    wchar_t* lastSlash = wcsrchr(g_dllDirectory, L'\\');
    if (lastSlash) *lastSlash = L'\0';

    // Start logging first (uses default Info level)
    Logger::instance().init(g_dllDirectory);
    LOG_INFO("dinput8 wrapper initialising from: %ls", g_dllDirectory);

    // Load config
    wchar_t iniPath[MAX_PATH];
    swprintf_s(iniPath, L"%s\\dinput8.ini", g_dllDirectory);

    if (Config::instance().load(iniPath)) {
        LOG_INFO("Config loaded from: %ls", iniPath);
    } else {
        LOG_WARN("Config file not found: %ls  (using defaults)", iniPath);
    }

    Logger::instance().setLevel(Config::instance().logLevel);
    LOG_INFO("Log level set to %d", static_cast<int>(Config::instance().logLevel));
    LOG_INFO("Wrapper enabled=%s  FFB global=%s  defaultScale=%d%%",
             Config::instance().enabled ? "true" : "false",
             Config::instance().ffbEnabled ? "true" : "false",
             Config::instance().ffbDefaultScale);

    // Load the real system dinput8.dll
    if (!OriginalDI8::instance().load()) {
        LOG_ERROR("FATAL: could not load original dinput8.dll!");
    }
}

// ============================================================================
// DLL entry point
// ============================================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID /*lpReserved*/) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            initWrapper(hModule);
            break;

        case DLL_PROCESS_DETACH:
            LOG_INFO("dinput8 wrapper unloading");
            OriginalDI8::instance().unload();
            Logger::instance().close();
            break;
    }
    return TRUE;
}

// ============================================================================
// Exported: DirectInput8Create
// ============================================================================
extern "C" HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst,
    DWORD     dwVersion,
    REFIID    riidltf,
    LPVOID*   ppvOut,
    LPUNKNOWN punkOuter)
{
    LOG_INFO("DirectInput8Create called (version=0x%08lx)", dwVersion);

    auto& orig = OriginalDI8::instance();
    if (!orig.DirectInput8Create) {
        LOG_ERROR("Original DirectInput8Create not available!");
        return DIERR_NOTINITIALIZED;
    }

    // If wrapper is disabled, pure pass-through
    if (!Config::instance().enabled) {
        LOG_INFO("Wrapper disabled — passing through to real DLL");
        return orig.DirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
    }

    // Call the real DirectInput8Create
    LPVOID realInterface = nullptr;
    HRESULT hr = orig.DirectInput8Create(hinst, dwVersion, riidltf, &realInterface, punkOuter);
    if (FAILED(hr) || !realInterface) {
        LOG_ERROR("Real DirectInput8Create failed: 0x%08lx", hr);
        if (ppvOut) *ppvOut = nullptr;
        return hr;
    }

    // Wrap the interface
    if (riidltf == IID_IDirectInput8W) {
        LOG_INFO("Wrapping IDirectInput8W");
        auto* wrapped = new WrapperDirectInput8W(
            static_cast<IDirectInput8W*>(realInterface));
        *ppvOut = static_cast<IDirectInput8W*>(wrapped);
    }
    else if (riidltf == IID_IDirectInput8A) {
        LOG_INFO("Wrapping IDirectInput8A");
        auto* wrapped = new WrapperDirectInput8A(
            static_cast<IDirectInput8A*>(realInterface));
        *ppvOut = static_cast<IDirectInput8A*>(wrapped);
    }
    else {
        // Unknown interface — return as-is
        LOG_WARN("DirectInput8Create: unknown IID requested, returning unwrapped");
        *ppvOut = realInterface;
    }

    return hr;
}

// ============================================================================
// Exported: other DLL entry points (forwarded to real DLL)
// ============================================================================
extern "C" HRESULT WINAPI DllCanUnloadNow() {
    auto& orig = OriginalDI8::instance();
    if (orig.DllCanUnloadNow)
        return orig.DllCanUnloadNow();
    return S_FALSE;
}

extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    auto& orig = OriginalDI8::instance();
    if (orig.DllGetClassObject)
        return orig.DllGetClassObject(rclsid, riid, ppv);
    return CLASS_E_CLASSNOTAVAILABLE;
}

extern "C" HRESULT WINAPI DllRegisterServer() {
    auto& orig = OriginalDI8::instance();
    if (orig.DllRegisterServer)
        return orig.DllRegisterServer();
    return E_FAIL;
}

extern "C" HRESULT WINAPI DllUnregisterServer() {
    auto& orig = OriginalDI8::instance();
    if (orig.DllUnregisterServer)
        return orig.DllUnregisterServer();
    return E_FAIL;
}
