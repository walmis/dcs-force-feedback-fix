// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#include "proxy.h"
#include "logger.h"
#include <cstdio>

OriginalDI8& OriginalDI8::instance() {
    static OriginalDI8 s;
    return s;
}

bool OriginalDI8::load() {
    if (hModule) return true;

    // Build path to the real system dinput8.dll
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);

    wchar_t dllPath[MAX_PATH];
    swprintf_s(dllPath, L"%s\\dinput8.dll", sysDir);

    hModule = LoadLibraryW(dllPath);
    if (!hModule) {
        LOG_ERROR("Failed to load original dinput8.dll from %ls (error %lu)",
                  dllPath, GetLastError());
        return false;
    }

    DirectInput8Create  = reinterpret_cast<PFN_DirectInput8Create>(
                              GetProcAddress(hModule, "DirectInput8Create"));
    DllCanUnloadNow     = reinterpret_cast<PFN_DllCanUnloadNow>(
                              GetProcAddress(hModule, "DllCanUnloadNow"));
    DllGetClassObject   = reinterpret_cast<PFN_DllGetClassObject>(
                              GetProcAddress(hModule, "DllGetClassObject"));
    DllRegisterServer   = reinterpret_cast<PFN_DllRegisterServer>(
                              GetProcAddress(hModule, "DllRegisterServer"));
    DllUnregisterServer = reinterpret_cast<PFN_DllUnregisterServer>(
                              GetProcAddress(hModule, "DllUnregisterServer"));

    if (!DirectInput8Create) {
        LOG_ERROR("Could not find DirectInput8Create in original dinput8.dll");
        return false;
    }

    LOG_INFO("Loaded original dinput8.dll from %ls", dllPath);
    return true;
}

void OriginalDI8::unload() {
    if (hModule) {
        FreeLibrary(hModule);
        hModule = nullptr;
    }
    DirectInput8Create  = nullptr;
    DllCanUnloadNow     = nullptr;
    DllGetClassObject   = nullptr;
    DllRegisterServer   = nullptr;
    DllUnregisterServer = nullptr;
}
