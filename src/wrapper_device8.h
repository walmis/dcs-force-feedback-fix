// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#pragma once
//
// WrapperDevice8<Unicode>
//
// Wraps IDirectInputDevice8A (Unicode=false) or IDirectInputDevice8W (Unicode=true).
// Intercepts FFB-related calls (CreateEffect, SendForceFeedbackCommand) and
// delegates everything else to the real device.
//
#include <windows.h>
#include <dinput.h>
#include <memory>
#include <string>
#include <type_traits>

#include "ffb_filter.h"

class WrapperEffect;

template<bool Unicode>
class WrapperDevice8
    : public std::conditional_t<Unicode, IDirectInputDevice8W, IDirectInputDevice8A>
{
public:
    // ---- Convenience type aliases ----
    using Base          = std::conditional_t<Unicode, IDirectInputDevice8W, IDirectInputDevice8A>;
    using Char          = std::conditional_t<Unicode, wchar_t, char>;
    using DevInstT      = std::conditional_t<Unicode, DIDEVICEINSTANCEW, DIDEVICEINSTANCEA>;
    using DevObjInstT   = std::conditional_t<Unicode, DIDEVICEOBJECTINSTANCEW, DIDEVICEOBJECTINSTANCEA>;
    using EffInfoT      = std::conditional_t<Unicode, DIEFFECTINFOW, DIEFFECTINFOA>;
    using ActFmtT       = std::conditional_t<Unicode, DIACTIONFORMATW, DIACTIONFORMATA>;
    using ImgInfoT      = std::conditional_t<Unicode, DIDEVICEIMAGEINFOHEADERW, DIDEVICEIMAGEINFOHEADERA>;
    using EnumObjCbT    = std::conditional_t<Unicode, LPDIENUMDEVICEOBJECTSCALLBACKW, LPDIENUMDEVICEOBJECTSCALLBACKA>;
    using EnumFxCbT     = std::conditional_t<Unicode, LPDIENUMEFFECTSCALLBACKW, LPDIENUMEFFECTSCALLBACKA>;
    using EnumSemCbT    = std::conditional_t<Unicode, LPDIENUMDEVICESBYSEMANTICSCBW, LPDIENUMDEVICESBYSEMANTICSCBA>;

    WrapperDevice8(Base* real, std::shared_ptr<FFBFilter> filter);
    virtual ~WrapperDevice8();

    // ---- IUnknown ----
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override;
    ULONG   STDMETHODCALLTYPE AddRef() override;
    ULONG   STDMETHODCALLTYPE Release() override;

    // ---- IDirectInputDevice8 ----
    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS lpDIDevCaps) override;
    HRESULT STDMETHODCALLTYPE EnumObjects(EnumObjCbT lpCallback, LPVOID pvRef, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID rguidProp, LPDIPROPHEADER pdiph) override;
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID rguidProp, LPCDIPROPHEADER pdiph) override;
    HRESULT STDMETHODCALLTYPE Acquire() override;
    HRESULT STDMETHODCALLTYPE Unacquire() override;
    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD cbData, LPVOID lpvData) override;
    HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod,
                                            LPDWORD pdwInOut, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT lpdf) override;
    HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE hEvent) override;
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND hwnd, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE GetObjectInfo(DevObjInstT* pdidoi, DWORD dwObj, DWORD dwHow) override;
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(DevInstT* pdidi) override;
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND hwndOwner, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE hinst, DWORD dwVersion, REFGUID rguid) override;

    // ---- FFB-intercepted ----
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID rguid, LPCDIEFFECT lpeff,
                                           LPDIRECTINPUTEFFECT* ppdeff, LPUNKNOWN punkOuter) override;
    HRESULT STDMETHODCALLTYPE EnumEffects(EnumFxCbT lpCallback, LPVOID pvRef, DWORD dwEffType) override;
    HRESULT STDMETHODCALLTYPE GetEffectInfo(EffInfoT* pdei, REFGUID rguid) override;
    HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD pdwOut) override;
    HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK lpCallback,
                                                        LPVOID pvRef, DWORD fl) override;
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE pesc) override;
    HRESULT STDMETHODCALLTYPE Poll() override;
    HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD cbObjectData, LPCDIDEVICEOBJECTDATA rgdod,
                                             LPDWORD pdwInOut, DWORD fl) override;

    // ---- DI8-specific ----
    HRESULT STDMETHODCALLTYPE BuildActionMap(ActFmtT* lpdiaf, const Char* lpszUserName, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE SetActionMap(ActFmtT* lpdiaf, const Char* lpszUserName, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE GetImageInfo(ImgInfoT* lpdiDevImageInfoHeader) override;
    HRESULT STDMETHODCALLTYPE EnumEffectsInFile(const Char* lpszFileName, LPDIENUMEFFECTSINFILECALLBACK pec, LPVOID pvRef, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE WriteEffectToFile(const Char* lpszFileName, DWORD dwEntries, LPDIFILEEFFECT rgDiFileEft, DWORD dwFlags) override;

private:
    Base*                      m_real;
    std::shared_ptr<FFBFilter> m_filter;
    volatile LONG              m_refCount = 1;
};

using WrapperDevice8A = WrapperDevice8<false>;
using WrapperDevice8W = WrapperDevice8<true>;
