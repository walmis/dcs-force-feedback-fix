// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#pragma once
//
// WrapperDirectInput8<Unicode>
//
// Wraps IDirectInput8A or IDirectInput8W.
// Intercepts CreateDevice to wrap each joystick device with WrapperDevice8
// so that FFB policy is applied automatically.
//
#include <windows.h>
#include <dinput.h>
#include <type_traits>

template<bool Unicode>
class WrapperDirectInput8
    : public std::conditional_t<Unicode, IDirectInput8W, IDirectInput8A>
{
public:
    using Base          = std::conditional_t<Unicode, IDirectInput8W, IDirectInput8A>;
    using Char          = std::conditional_t<Unicode, wchar_t, char>;
    using DevIfaceT     = std::conditional_t<Unicode, IDirectInputDevice8W, IDirectInputDevice8A>;
    using EnumDevCbT    = std::conditional_t<Unicode, LPDIENUMDEVICESCALLBACKW, LPDIENUMDEVICESCALLBACKA>;
    using ActFmtT       = std::conditional_t<Unicode, DIACTIONFORMATW, DIACTIONFORMATA>;
    using EnumSemCbT    = std::conditional_t<Unicode, LPDIENUMDEVICESBYSEMANTICSCBW, LPDIENUMDEVICESBYSEMANTICSCBA>;
    using CfgDevParamsT = std::conditional_t<Unicode, DICONFIGUREDEVICESPARAMSW, DICONFIGUREDEVICESPARAMSA>;

    WrapperDirectInput8(Base* real);
    virtual ~WrapperDirectInput8();

    // ---- IUnknown ----
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override;
    ULONG   STDMETHODCALLTYPE AddRef() override;
    ULONG   STDMETHODCALLTYPE Release() override;

    // ---- IDirectInput8 ----
    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID rguid, DevIfaceT** lplpDevice,
                                           LPUNKNOWN punkOuter) override;
    HRESULT STDMETHODCALLTYPE EnumDevices(DWORD dwDevType, EnumDevCbT lpCallback,
                                          LPVOID pvRef, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE GetDeviceStatus(REFGUID rguidInstance) override;
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND hwndOwner, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE hinst, DWORD dwVersion) override;
    HRESULT STDMETHODCALLTYPE FindDevice(REFGUID rguidClass, const Char* ptszName,
                                         LPGUID pguidInstance) override;
    HRESULT STDMETHODCALLTYPE EnumDevicesBySemantics(const Char* ptszUserName,
                                                     ActFmtT* lpdiActionFormat,
                                                     EnumSemCbT lpCallback,
                                                     LPVOID pvRef, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK lpdiCallback,
                                               CfgDevParamsT* lpdiCDParams,
                                               DWORD dwFlags, LPVOID pvRefData) override;

private:
    Base*         m_real;
    volatile LONG m_refCount = 1;
};

using WrapperDirectInput8A = WrapperDirectInput8<false>;
using WrapperDirectInput8W = WrapperDirectInput8<true>;
