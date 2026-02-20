// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#pragma once

#include <windows.h>
#include <dinput.h>
#include <memory>
#include "ffb_filter.h"

// Wraps IDirectInputEffect, intercepting Start/Stop/SetParameters/Download
// to apply FFB blocking and scaling per device policy.
//
// Supports a "null" mode (m_real == nullptr) for devices where FFB is blocked
// and the real device refused to create the effect — all calls return DI_OK.
class WrapperEffect : public IDirectInputEffect {
public:
    // Wrap a real effect with a filter policy.
    WrapperEffect(IDirectInputEffect* real, std::shared_ptr<FFBFilter> filter);

    // Null-effect constructor: no underlying real effect, everything is a no-op.
    WrapperEffect(REFGUID effectGuid, std::shared_ptr<FFBFilter> filter);

    virtual ~WrapperEffect();

    // ---- IUnknown ----
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override;
    ULONG   STDMETHODCALLTYPE AddRef() override;
    ULONG   STDMETHODCALLTYPE Release() override;

    // ---- IDirectInputEffect ----
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE hinst, DWORD dwVersion, REFGUID rguid) override;
    HRESULT STDMETHODCALLTYPE GetEffectGuid(LPGUID pguid) override;
    HRESULT STDMETHODCALLTYPE GetParameters(LPDIEFFECT peff, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE SetParameters(LPCDIEFFECT peff, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE Start(DWORD dwIterations, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE Stop() override;
    HRESULT STDMETHODCALLTYPE GetEffectStatus(LPDWORD pdwFlags) override;
    HRESULT STDMETHODCALLTYPE Download() override;
    HRESULT STDMETHODCALLTYPE Unload() override;
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE pesc) override;

private:
    IDirectInputEffect*        m_real;      // may be nullptr (null-effect mode)
    GUID                       m_guid;      // cached effect GUID
    std::shared_ptr<FFBFilter> m_filter;
    volatile LONG              m_refCount = 1;
};
