// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#include "wrapper_effect.h"
#include "logger.h"
#include <cstring>

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

    if (!m_filter->isFFBAllowed()) return DI_OK;  // silently swallow

    if (!m_real) return DI_OK;

    // If scaling is active, work on a copy
    if (m_filter->getScale() < 100 && peff) {
        DIEFFECT copy = *peff;
        m_filter->scaleEffect(&copy);
        return m_real->SetParameters(&copy, dwFlags);
    }

    return m_real->SetParameters(peff, dwFlags);
}

HRESULT STDMETHODCALLTYPE WrapperEffect::Start(DWORD dwIterations, DWORD dwFlags) {
    m_filter->logEffectStart(dwIterations, dwFlags);
    if (!m_filter->isFFBAllowed()) return DI_OK;
    if (!m_real) return DI_OK;
    return m_real->Start(dwIterations, dwFlags);
}

HRESULT STDMETHODCALLTYPE WrapperEffect::Stop() {
    m_filter->logEffectStop();
    if (!m_filter->isFFBAllowed()) return DI_OK;
    if (!m_real) return DI_OK;
    return m_real->Stop();
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
    return m_real->Download();
}

HRESULT STDMETHODCALLTYPE WrapperEffect::Unload() {
    if (!m_real) return DI_OK;
    return m_real->Unload();
}

HRESULT STDMETHODCALLTYPE WrapperEffect::Escape(LPDIEFFESCAPE pesc) {
    if (!m_real) return DIERR_UNSUPPORTED;
    return m_real->Escape(pesc);
}
