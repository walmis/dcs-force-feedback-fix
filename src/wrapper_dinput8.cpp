// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#include "wrapper_dinput8.h"
#include "wrapper_device8.h"
#include "ffb_filter.h"
#include "config.h"
#include "logger.h"
#include <memory>
#include <string>

// ============================================================================
// Construction / destruction
// ============================================================================
template<bool U>
WrapperDirectInput8<U>::WrapperDirectInput8(Base* real)
    : m_real(real)
{
    LOG_INFO("WrapperDirectInput8<%s> created", U ? "W" : "A");
}

template<bool U>
WrapperDirectInput8<U>::~WrapperDirectInput8() {
    LOG_DEBUG("WrapperDirectInput8<%s> destroyed", U ? "W" : "A");
    if (m_real) m_real->Release();
}

// ============================================================================
// IUnknown
// ============================================================================
template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDirectInput8<U>::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_POINTER;

    if (riid == IID_IUnknown) {
        *ppvObj = static_cast<Base*>(this);
        AddRef();
        return S_OK;
    }
    if constexpr (U) {
        if (riid == IID_IDirectInput8W) {
            *ppvObj = static_cast<IDirectInput8W*>(this);
            AddRef();
            return S_OK;
        }
    } else {
        if (riid == IID_IDirectInput8A) {
            *ppvObj = static_cast<IDirectInput8A*>(this);
            AddRef();
            return S_OK;
        }
    }

    *ppvObj = nullptr;
    return m_real->QueryInterface(riid, ppvObj);
}

template<bool U>
ULONG STDMETHODCALLTYPE WrapperDirectInput8<U>::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

template<bool U>
ULONG STDMETHODCALLTYPE WrapperDirectInput8<U>::Release() {
    ULONG c = InterlockedDecrement(&m_refCount);
    if (c == 0) delete this;
    return c;
}

// ============================================================================
// CreateDevice — the main interception point
// ============================================================================

// Helper: query device product name (wide string) from a real device.
static std::wstring queryDeviceName(IDirectInputDevice8W* dev) {
    DIDEVICEINSTANCEW di{};
    di.dwSize = sizeof(di);
    if (SUCCEEDED(dev->GetDeviceInfo(&di)))
        return di.tszProductName;
    return L"<unknown>";
}

static std::wstring queryDeviceName(IDirectInputDevice8A* dev) {
    DIDEVICEINSTANCEA di{};
    di.dwSize = sizeof(di);
    if (SUCCEEDED(dev->GetDeviceInfo(&di))) {
        // Convert narrow product name to wide for consistent policy lookup
        int len = MultiByteToWideChar(CP_ACP, 0, di.tszProductName, -1, nullptr, 0);
        std::wstring ws(len, L'\0');
        MultiByteToWideChar(CP_ACP, 0, di.tszProductName, -1, ws.data(), len);
        if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
        return ws;
    }
    return L"<unknown>";
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDirectInput8<U>::CreateDevice(
    REFGUID rguid, DevIfaceT** lplpDevice, LPUNKNOWN punkOuter)
{
    if (!lplpDevice) return E_POINTER;

    // Create the real device
    DevIfaceT* realDevice = nullptr;
    HRESULT hr = m_real->CreateDevice(rguid, &realDevice, punkOuter);
    if (FAILED(hr) || !realDevice) {
        *lplpDevice = nullptr;
        return hr;
    }

    // If wrapper is globally disabled, return unwrapped device
    if (!Config::instance().enabled) {
        *lplpDevice = realDevice;
        return hr;
    }

    // Query name and resolve FFB policy
    std::wstring name = queryDeviceName(realDevice);

    bool ffbEnabled = true;
    int  ffbScale   = 100;
    Config::instance().getDevicePolicy(name.c_str(), ffbEnabled, ffbScale);

    LOG_INFO("CreateDevice: [%ls]  FFB=%s  scale=%d%%",
             name.c_str(),
             ffbEnabled ? "allowed" : "BLOCKED",
             ffbScale);

    FFBPolicy policy;
    policy.enabled = ffbEnabled;
    policy.scale   = ffbScale;

    auto filter = std::make_shared<FFBFilter>(policy, name);

    // Wrap the device
    *lplpDevice = new WrapperDevice8<U>(realDevice, filter);
    return hr;
}

// ============================================================================
// Pass-through methods
// ============================================================================
template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDirectInput8<U>::EnumDevices(
    DWORD dwDevType, EnumDevCbT lpCallback, LPVOID pvRef, DWORD dwFlags)
{
    return m_real->EnumDevices(dwDevType, lpCallback, pvRef, dwFlags);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDirectInput8<U>::GetDeviceStatus(REFGUID rguidInstance) {
    return m_real->GetDeviceStatus(rguidInstance);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDirectInput8<U>::RunControlPanel(
    HWND hwndOwner, DWORD dwFlags)
{
    return m_real->RunControlPanel(hwndOwner, dwFlags);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDirectInput8<U>::Initialize(
    HINSTANCE hinst, DWORD dwVersion)
{
    return m_real->Initialize(hinst, dwVersion);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDirectInput8<U>::FindDevice(
    REFGUID rguidClass, const Char* ptszName, LPGUID pguidInstance)
{
    return m_real->FindDevice(rguidClass, ptszName, pguidInstance);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDirectInput8<U>::EnumDevicesBySemantics(
    const Char* ptszUserName, ActFmtT* lpdiActionFormat,
    EnumSemCbT lpCallback, LPVOID pvRef, DWORD dwFlags)
{
    return m_real->EnumDevicesBySemantics(
        ptszUserName, lpdiActionFormat, lpCallback, pvRef, dwFlags);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDirectInput8<U>::ConfigureDevices(
    LPDICONFIGUREDEVICESCALLBACK lpdiCallback, CfgDevParamsT* lpdiCDParams,
    DWORD dwFlags, LPVOID pvRefData)
{
    return m_real->ConfigureDevices(lpdiCallback, lpdiCDParams, dwFlags, pvRefData);
}

// ============================================================================
// Explicit instantiations
// ============================================================================
template class WrapperDirectInput8<false>;  // A
template class WrapperDirectInput8<true>;   // W
