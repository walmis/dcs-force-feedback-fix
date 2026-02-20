// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#pragma once

#include <windows.h>
#include <dinput.h>

// Function pointer types for the real dinput8.dll exports
using PFN_DirectInput8Create   = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
using PFN_DllCanUnloadNow      = HRESULT(WINAPI*)();
using PFN_DllGetClassObject    = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
using PFN_DllRegisterServer    = HRESULT(WINAPI*)();
using PFN_DllUnregisterServer  = HRESULT(WINAPI*)();

// Holds the real dinput8.dll module and function pointers.
struct OriginalDI8 {
    HMODULE hModule = nullptr;

    PFN_DirectInput8Create   DirectInput8Create  = nullptr;
    PFN_DllCanUnloadNow      DllCanUnloadNow     = nullptr;
    PFN_DllGetClassObject    DllGetClassObject   = nullptr;
    PFN_DllRegisterServer    DllRegisterServer   = nullptr;
    PFN_DllUnregisterServer  DllUnregisterServer = nullptr;

    // Load the real system dinput8.dll. Safe to call multiple times.
    bool load();
    void unload();

    static OriginalDI8& instance();
};
