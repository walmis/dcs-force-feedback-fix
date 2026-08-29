// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#include "wrapper_effect.h"
#include "ffb_state_registry.h"
#include "logger.h"
#include <cstring>

// ---------------------------------------------------------------------------
// Return codes
//
// Every force feedback call used to be a bare `return m_real->...()`, so a
// log recorded that the game asked and never whether it worked.  A Start that
// came back DIERR_NOTEXCLUSIVEACQUIRED read exactly like one that played.
// That difference is the whole diagnosis when a stick goes quiet, so failures
// are named here - at Info, since a log is only collected when something is
// already wrong.
// ---------------------------------------------------------------------------
namespace {

const char* diErrorName(HRESULT hr) {
    switch (hr) {
    case DI_OK:                        return "DI_OK";
    case DI_NOEFFECT:                  return "DI_NOEFFECT";
    case DI_TRUNCATED:                 return "DI_TRUNCATED";
    case DIERR_NOTEXCLUSIVEACQUIRED:   return "DIERR_NOTEXCLUSIVEACQUIRED";
    case DIERR_NOTACQUIRED:            return "DIERR_NOTACQUIRED";
    case DIERR_INPUTLOST:              return "DIERR_INPUTLOST";
    case DIERR_INCOMPLETEEFFECT:       return "DIERR_INCOMPLETEEFFECT";
    case DIERR_EFFECTPLAYING:          return "DIERR_EFFECTPLAYING";
    case DIERR_DEVICEFULL:             return "DIERR_DEVICEFULL";
    case DIERR_DEVICENOTREG:           return "DIERR_DEVICENOTREG";
    case DIERR_INVALIDPARAM:           return "DIERR_INVALIDPARAM";
    case DIERR_NOINTERFACE:            return "DIERR_NOINTERFACE";
    case DIERR_OUTOFMEMORY:            return "DIERR_OUTOFMEMORY";
    case DIERR_UNSUPPORTED:            return "DIERR_UNSUPPORTED";
    case DIERR_UNPLUGGED:              return "DIERR_UNPLUGGED";
    // not DirectInput codes, but ones that do turn up: the underlying
    // device handle going invalid, and exclusive access refused
    case E_HANDLE:                     return "E_HANDLE (device handle invalid)";
    case E_ACCESSDENIED:               return "E_ACCESSDENIED";
    default:                           return "";
    }
}

/// Report a call that did not succeed, and hand the result straight back.
HRESULT reported(const wchar_t* device, const char* call, HRESULT hr) {
    if (FAILED(hr)) {
        const char* named = diErrorName(hr);
        if (*named)
            LOG_INFO("FFB [%ls] %s FAILED: %s", device, call, named);
        else
            LOG_INFO("FFB [%ls] %s FAILED: hr=0x%08lx", device, call, hr);
    }
    return hr;
}

}  // namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------
WrapperEffect::WrapperEffect(IDirectInputEffect* real, std::shared_ptr<FFBFilter> filter)
    : m_real(real)
    , m_filter(std::move(filter))
    , m_guid{}
{
    if (m_real) m_real->GetEffectGuid(&m_guid);
    LOG_DEBUG("WrapperEffect created (real=%p) for [%ls]",
              m_real, m_filter->deviceName().c_str());
}

WrapperEffect::WrapperEffect(REFGUID effectGuid, std::shared_ptr<FFBFilter> filter)
    : m_real(nullptr)
    , m_filter(std::move(filter))
    , m_guid(effectGuid)
{
    LOG_DEBUG("WrapperEffect created (NULL-effect) for [%ls]",
              m_filter->deviceName().c_str());
}

WrapperEffect::~WrapperEffect() {
    LOG_DEBUG("WrapperEffect destroyed for [%ls]", m_filter->deviceName().c_str());
    if (m_real) m_real->Release();
}

// ---------------------------------------------------------------------------
// IUnknown
// ---------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE WrapperEffect::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_POINTER;

    if (riid == IID_IUnknown || riid == IID_IDirectInputEffect) {
        *ppvObj = static_cast<IDirectInputEffect*>(this);
        AddRef();
        return S_OK;
    }

    *ppvObj = nullptr;
    if (m_real) return m_real->QueryInterface(riid, ppvObj);
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WrapperEffect::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE WrapperEffect::Release() {
    ULONG c = InterlockedDecrement(&m_refCount);
    if (c == 0) delete this;
    return c;
}

// ---------------------------------------------------------------------------
// IDirectInputEffect — pass-through or blocking/scaling
// ---------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE WrapperEffect::Initialize(
    HINSTANCE hinst, DWORD dwVersion, REFGUID rguid)
{
    if (!m_real) return DI_OK;
    return m_real->Initialize(hinst, dwVersion, rguid);
}

HRESULT STDMETHODCALLTYPE WrapperEffect::GetEffectGuid(LPGUID pguid) {
    if (!pguid) return E_POINTER;
    *pguid = m_guid;
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE WrapperEffect::GetParameters(LPDIEFFECT peff, DWORD dwFlags) {
    if (!m_real) {
        // Null-effect: zero out what we can
        if (peff) std::memset(peff, 0, sizeof(DIEFFECT));
        return DI_OK;
    }
    return m_real->GetParameters(peff, dwFlags);
}

HRESULT STDMETHODCALLTYPE WrapperEffect::SetParameters(LPCDIEFFECT peff, DWORD dwFlags) {
    m_filter->logEffectParams(peff);

    // Record params for auto-restart on reconnect
    FFBStateRegistry::instance().recordParams(
        m_filter->deviceName(), m_guid, peff);

    if (!m_filter->isFFBAllowed()) return DI_OK;  // silently swallow

    if (!m_real) return DI_OK;

    // If scaling is active, work on a copy
    if (m_filter->getScale() < 100 && peff) {
        DIEFFECT copy = *peff;
        m_filter->scaleEffect(&copy, m_guid);
        return reported(m_filter->deviceName().c_str(), "Effect.SetParameters",
                        m_real->SetParameters(&copy, dwFlags));
    }

    return reported(m_filter->deviceName().c_str(), "Effect.SetParameters",
                    m_real->SetParameters(peff, dwFlags));
}

HRESULT STDMETHODCALLTYPE WrapperEffect::Start(DWORD dwIterations, DWORD dwFlags) {
    m_filter->logEffectStart(dwIterations, dwFlags);

    // Record start for auto-restart on reconnect
    FFBStateRegistry::instance().recordStart(
        m_filter->deviceName(), m_guid, dwIterations, dwFlags);

    if (!m_filter->isFFBAllowed()) return DI_OK;
    if (!m_real) return DI_OK;
    return reported(m_filter->deviceName().c_str(), "Effect.Start",
                    m_real->Start(dwIterations, dwFlags));
}

HRESULT STDMETHODCALLTYPE WrapperEffect::Stop() {
    m_filter->logEffectStop();

    // Record stop so auto-restart knows not to restart stopped effects
    FFBStateRegistry::instance().recordStop(
        m_filter->deviceName(), m_guid);

    if (!m_filter->isFFBAllowed()) return DI_OK;
    if (!m_real) return DI_OK;
    return reported(m_filter->deviceName().c_str(), "Effect.Stop",
                    m_real->Stop());
}

HRESULT STDMETHODCALLTYPE WrapperEffect::GetEffectStatus(LPDWORD pdwFlags) {
    if (!m_filter->isFFBAllowed() || !m_real) {
        if (pdwFlags) *pdwFlags = 0;
        return DI_OK;
    }
    return m_real->GetEffectStatus(pdwFlags);
}

HRESULT STDMETHODCALLTYPE WrapperEffect::Download() {
    if (!m_filter->isFFBAllowed()) return DI_OK;
    if (!m_real) return DI_OK;
    return reported(m_filter->deviceName().c_str(), "Effect.Download",
                    m_real->Download());
}

HRESULT STDMETHODCALLTYPE WrapperEffect::Unload() {
    if (!m_real) return DI_OK;
    return m_real->Unload();
}

HRESULT STDMETHODCALLTYPE WrapperEffect::Escape(LPDIEFFESCAPE pesc) {
    if (!m_real) return DIERR_UNSUPPORTED;
    return m_real->Escape(pesc);
}
