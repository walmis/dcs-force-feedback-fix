// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#include "wrapper_dinput8.h"
#include "wrapper_device8.h"
#include "ffb_filter.h"
#include "config.h"
#include "logger.h"
#include <algorithm>
#include <vector>
#include <memory>
#include <string>

// ============================================================================
// Callback trampolines for device-hide filtering in EnumDevices
// ============================================================================

// Helper: convert a narrow (ANSI) product name to wide for policy lookups.
static std::wstring ansiToWide(const char* s) {
    int len = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s, -1, ws.data(), len);
    if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
    return ws;
}

struct EnumDevCtxW {
    LPDIENUMDEVICESCALLBACKW userCb;
    LPVOID                   userRef;
    // NOTE: EnumDevices is synchronous — ctx is alive for the entire call.
    static BOOL CALLBACK cb(LPCDIDEVICEINSTANCEW lpddi, LPVOID pv) {
        auto* c = static_cast<EnumDevCtxW*>(pv);
        if (Config::instance().isDeviceHidden(lpddi->tszProductName)) {
            LOG_INFO("EnumDevices: hiding [%ls]", lpddi->tszProductName);
            return DIENUM_CONTINUE;
        }
        return c->userCb(lpddi, c->userRef);
    }
};

struct EnumDevCtxA {
    LPDIENUMDEVICESCALLBACKA userCb;
    LPVOID                   userRef;
    // NOTE: EnumDevices is synchronous — ctx is alive for the entire call.
    static BOOL CALLBACK cb(LPCDIDEVICEINSTANCEA lpddi, LPVOID pv) {
        auto* c = static_cast<EnumDevCtxA*>(pv);
        std::wstring ws = ansiToWide(lpddi->tszProductName);
        if (Config::instance().isDeviceHidden(ws.c_str())) {
            LOG_INFO("EnumDevices: hiding [%ls]", ws.c_str());
            return DIENUM_CONTINUE;
        }
        return c->userCb(lpddi, c->userRef);
    }
};

// Callback trampolines for device-hide filtering in EnumDevicesBySemantics

struct EnumSemCtxW {
    LPDIENUMDEVICESBYSEMANTICSCBW userCb;
    LPVOID                        userRef;
    // NOTE: EnumDevicesBySemantics is synchronous — ctx is alive for the entire call.
    static BOOL CALLBACK cb(LPCDIDEVICEINSTANCEW lpddi,
                             IDirectInputDevice8W* pdid,
                             DWORD dwFlags, DWORD dwRemaining, LPVOID pv) {
        auto* c = static_cast<EnumSemCtxW*>(pv);
        if (Config::instance().isDeviceHidden(lpddi->tszProductName)) {
            LOG_INFO("EnumDevicesBySemantics: hiding [%ls]", lpddi->tszProductName);
            return DIENUM_CONTINUE;
        }
        return c->userCb(lpddi, pdid, dwFlags, dwRemaining, c->userRef);
    }
};

struct EnumSemCtxA {
    LPDIENUMDEVICESBYSEMANTICSCBA userCb;
    LPVOID                        userRef;
    // NOTE: EnumDevicesBySemantics is synchronous — ctx is alive for the entire call.
    static BOOL CALLBACK cb(LPCDIDEVICEINSTANCEA lpddi,
                             IDirectInputDevice8A* pdid,
                             DWORD dwFlags, DWORD dwRemaining, LPVOID pv) {
        auto* c = static_cast<EnumSemCtxA*>(pv);
        std::wstring ws = ansiToWide(lpddi->tszProductName);
        if (Config::instance().isDeviceHidden(ws.c_str())) {
            LOG_INFO("EnumDevicesBySemantics: hiding [%ls]", ws.c_str());
            return DIENUM_CONTINUE;
        }
        return c->userCb(lpddi, pdid, dwFlags, dwRemaining, c->userRef);
    }
};

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

    // Query name and check if device should be hidden
    std::wstring name = queryDeviceName(realDevice);

    if (Config::instance().isDeviceHidden(name.c_str())) {
        LOG_INFO("CreateDevice: hiding device [%ls]", name.c_str());
        realDevice->Release();
        *lplpDevice = nullptr;
        return DIERR_DEVICENOTREG;
    }

    // Resolve FFB policy

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
// ---------------------------------------------------------------------------
// Enumeration
//
// Two jobs, both optional; the pass-through below is what happens when
// neither applies.
//
// 1. Agreement with capabilities.  GetCapabilities strips the force feedback
//    flags from a blocked device, but enumeration hands that same device
//    back when the caller asks for force feedback devices only - so a game
//    is told two contradictory things about it.
//
// 2. Order.  Windows caches the order DirectInput reports devices in, and a
//    game that gives force feedback to the first devices it sees will keep
//    giving it to the same ones.  Nothing short of unplugging hardware moves
//    that order, which is why a separate wrapper exists purely to change it.
//
//    Off unless [DeviceOrder] asks for it: some games identify a device by
//    its position rather than its name, so reordering underneath one that
//    did not ask would scramble its bindings.
// ---------------------------------------------------------------------------
namespace {

/// Whether the game should be shown this device when it asked for force
/// feedback devices.
inline bool ffbVisible(const wchar_t* name) {
    bool enabled = true;
    int  scale   = 100;
    Config::instance().getDevicePolicy(name, enabled, scale);
    return enabled;
}

template<bool Unicode>
struct Collected {
    using InstT = std::conditional_t<Unicode, DIDEVICEINSTANCEW, DIDEVICEINSTANCEA>;
    std::vector<InstT> devices;
};

BOOL CALLBACK collectW(LPCDIDEVICEINSTANCEW lpddi, LPVOID pvRef) {
    static_cast<Collected<true>*>(pvRef)->devices.push_back(*lpddi);
    return DIENUM_CONTINUE;
}

BOOL CALLBACK collectA(LPCDIDEVICEINSTANCEA lpddi, LPVOID pvRef) {
    static_cast<Collected<false>*>(pvRef)->devices.push_back(*lpddi);
    return DIENUM_CONTINUE;
}

}  // namespace

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDirectInput8<U>::EnumDevices(
    DWORD dwDevType, EnumDevCbT lpCallback, LPVOID pvRef, DWORD dwFlags)
{
    const bool wantFFB    = (dwFlags & DIEDFL_FORCEFEEDBACK) != 0;
    const bool reordering = Config::instance().orderingActive();

    if ((!wantFFB && !reordering) || !lpCallback)
        return m_real->EnumDevices(dwDevType, lpCallback, pvRef, dwFlags);

    Collected<U> collected;
    auto* gather = reinterpret_cast<EnumDevCbT>(U ? (void*)collectW
                                                  : (void*)collectA);
    HRESULT hr = m_real->EnumDevices(dwDevType, gather, &collected, dwFlags);
    if (FAILED(hr)) return hr;

    // Decide what to keep and where it goes before handing any of it on.
    struct Slot { size_t index; int rank; };
    std::vector<Slot> order;
    order.reserve(collected.devices.size());

    for (size_t i = 0; i < collected.devices.size(); ++i) {
        const auto& inst = collected.devices[i];

        wchar_t name[MAX_PATH] = L"";
        if constexpr (U) {
            wcsncpy_s(name, inst.tszProductName, _TRUNCATE);
        } else {
            MultiByteToWideChar(CP_ACP, 0, inst.tszProductName, -1, name, MAX_PATH);
        }

        // Only the force feedback enumeration: a caller that did not ask
        // for DIEDFL_FORCEFEEDBACK still sees every device, so a blocked
        // one stays fully usable for its axes and buttons.
        if (wantFFB && !ffbVisible(name)) {
            LOG_INFO("EnumDevices: hiding [%ls] from the force feedback list "
                     "(FFB blocked by policy)", name);
            continue;
        }
        order.push_back({ i, reordering ? Config::instance().orderRank(name)
                                        : 0 });
    }

    if (reordering) {
        // stable, so everything sharing a rank - including the whole
        // unlisted tail - keeps the order Windows gave it
        std::stable_sort(order.begin(), order.end(),
                         [](const Slot& a, const Slot& b) {
                             return a.rank < b.rank;
                         });
    }

    for (const auto& slot : order) {
        if (lpCallback(&collected.devices[slot.index], pvRef) == DIENUM_STOP)
            break;
    }
    return hr;
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
    if (!lpCallback)
        return m_real->EnumDevicesBySemantics(
            ptszUserName, lpdiActionFormat, lpCallback, pvRef, dwFlags);

    if constexpr (U) {
        EnumSemCtxW ctx{ lpCallback, pvRef };
        return m_real->EnumDevicesBySemantics(
            ptszUserName, lpdiActionFormat, EnumSemCtxW::cb, &ctx, dwFlags);
    } else {
        EnumSemCtxA ctx{ lpCallback, pvRef };
        return m_real->EnumDevicesBySemantics(
            ptszUserName, lpdiActionFormat, EnumSemCtxA::cb, &ctx, dwFlags);
    }
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
