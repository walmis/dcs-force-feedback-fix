// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#include "wrapper_device8.h"
#include "wrapper_effect.h"
#include "logger.h"

// ============================================================================
// Construction / destruction
// ============================================================================
template<bool U>
WrapperDevice8<U>::WrapperDevice8(Base* real, std::shared_ptr<FFBFilter> filter)
    : m_real(real), m_filter(std::move(filter))
{
    LOG_INFO("WrapperDevice8<%s> created for [%ls]  FFB=%s  scale=%d%%",
             U ? "W" : "A",
             m_filter->deviceName().c_str(),
             m_filter->isFFBAllowed() ? "allowed" : "BLOCKED",
             m_filter->getScale());
}

template<bool U>
WrapperDevice8<U>::~WrapperDevice8() {
    LOG_DEBUG("WrapperDevice8<%s> destroyed for [%ls]", U ? "W" : "A",
              m_filter->deviceName().c_str());
    if (m_real) m_real->Release();
}

// ============================================================================
// IUnknown
// ============================================================================
template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_POINTER;

    if (riid == IID_IUnknown) {
        *ppvObj = static_cast<Base*>(this);
        AddRef();
        return S_OK;
    }

    // Match the correct A or W IID
    if constexpr (U) {
        if (riid == IID_IDirectInputDevice8W) {
            *ppvObj = static_cast<IDirectInputDevice8W*>(this);
            AddRef();
            return S_OK;
        }
    } else {
        if (riid == IID_IDirectInputDevice8A) {
            *ppvObj = static_cast<IDirectInputDevice8A*>(this);
            AddRef();
            return S_OK;
        }
    }

    // Forward unknown IIDs to the real device
    *ppvObj = nullptr;
    return m_real->QueryInterface(riid, ppvObj);
}

template<bool U>
ULONG STDMETHODCALLTYPE WrapperDevice8<U>::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

template<bool U>
ULONG STDMETHODCALLTYPE WrapperDevice8<U>::Release() {
    ULONG c = InterlockedDecrement(&m_refCount);
    if (c == 0) delete this;
    return c;
}

// ============================================================================
// Simple pass-through methods
// ============================================================================

// FFB-related DIDEVCAPS flags to clear when FFB is blocked
static constexpr DWORD FFB_CAPS_FLAGS =
    DIDC_FORCEFEEDBACK      |   // 0x00000100  device supports FFB
    DIDC_FFATTACK           |   // 0x00000200  attack envelope
    DIDC_FFFADE             |   // 0x00000400  fade envelope
    DIDC_SATURATION         |   // 0x00000800
    DIDC_POSNEGCOEFFICIENTS |   // 0x00001000
    DIDC_POSNEGSATURATION   |   // 0x00002000
    DIDC_DEADBAND           |   // 0x00004000
    DIDC_STARTDELAY;            // 0x00008000

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::GetCapabilities(LPDIDEVCAPS lpDIDevCaps) {
    HRESULT hr = m_real->GetCapabilities(lpDIDevCaps);

    if (SUCCEEDED(hr) && lpDIDevCaps && !m_filter->isFFBAllowed()) {
        // Strip all FFB-related flags so the game thinks this is a plain device.
        lpDIDevCaps->dwFlags &= ~FFB_CAPS_FLAGS;
        // Zero FFB timing fields too
        lpDIDevCaps->dwFFSamplePeriod     = 0;
        lpDIDevCaps->dwFFMinTimeResolution = 0;
        LOG_DEBUG("GetCapabilities [%ls]: stripped FFB caps flags",
                  m_filter->deviceName().c_str());
    }

    return hr;
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::EnumObjects(
    EnumObjCbT lpCallback, LPVOID pvRef, DWORD dwFlags)
{
    return m_real->EnumObjects(lpCallback, pvRef, dwFlags);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::GetProperty(
    REFGUID rguidProp, LPDIPROPHEADER pdiph)
{
    return m_real->GetProperty(rguidProp, pdiph);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::SetProperty(
    REFGUID rguidProp, LPCDIPROPHEADER pdiph)
{
    return m_real->SetProperty(rguidProp, pdiph);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::Acquire() {
    return m_real->Acquire();
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::Unacquire() {
    return m_real->Unacquire();
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::GetDeviceState(DWORD cbData, LPVOID lpvData) {
    return m_real->GetDeviceState(cbData, lpvData);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::GetDeviceData(
    DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags)
{
    return m_real->GetDeviceData(cbObjectData, rgdod, pdwInOut, dwFlags);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::SetDataFormat(LPCDIDATAFORMAT lpdf) {
    return m_real->SetDataFormat(lpdf);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::SetEventNotification(HANDLE hEvent) {
    return m_real->SetEventNotification(hEvent);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::SetCooperativeLevel(HWND hwnd, DWORD dwFlags) {
    return m_real->SetCooperativeLevel(hwnd, dwFlags);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::GetObjectInfo(
    DevObjInstT* pdidoi, DWORD dwObj, DWORD dwHow)
{
    return m_real->GetObjectInfo(pdidoi, dwObj, dwHow);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::GetDeviceInfo(DevInstT* pdidi) {
    return m_real->GetDeviceInfo(pdidi);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::RunControlPanel(HWND hwndOwner, DWORD dwFlags) {
    return m_real->RunControlPanel(hwndOwner, dwFlags);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::Initialize(
    HINSTANCE hinst, DWORD dwVersion, REFGUID rguid)
{
    return m_real->Initialize(hinst, dwVersion, rguid);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::Escape(LPDIEFFESCAPE pesc) {
    return m_real->Escape(pesc);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::Poll() {
    return m_real->Poll();
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::SendDeviceData(
    DWORD cbObjectData, LPCDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD fl)
{
    return m_real->SendDeviceData(cbObjectData, rgdod, pdwInOut, fl);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::BuildActionMap(
    ActFmtT* lpdiaf, const Char* lpszUserName, DWORD dwFlags)
{
    return m_real->BuildActionMap(lpdiaf, lpszUserName, dwFlags);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::SetActionMap(
    ActFmtT* lpdiaf, const Char* lpszUserName, DWORD dwFlags)
{
    return m_real->SetActionMap(lpdiaf, lpszUserName, dwFlags);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::GetImageInfo(
    ImgInfoT* lpdiDevImageInfoHeader)
{
    return m_real->GetImageInfo(lpdiDevImageInfoHeader);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::EnumEffectsInFile(
    const Char* lpszFileName, LPDIENUMEFFECTSINFILECALLBACK pec, LPVOID pvRef, DWORD dwFlags)
{
    return m_real->EnumEffectsInFile(lpszFileName, pec, pvRef, dwFlags);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::WriteEffectToFile(
    const Char* lpszFileName, DWORD dwEntries, LPDIFILEEFFECT rgDiFileEft, DWORD dwFlags)
{
    return m_real->WriteEffectToFile(lpszFileName, dwEntries, rgDiFileEft, dwFlags);
}

// ============================================================================
// FFB-intercepted methods
// ============================================================================
template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::CreateEffect(
    REFGUID rguid, LPCDIEFFECT lpeff, LPDIRECTINPUTEFFECT* ppdeff, LPUNKNOWN punkOuter)
{
    m_filter->logEffectCreation(rguid);

    if (!ppdeff) return E_POINTER;

    // Try to create the real effect on the underlying device
    IDirectInputEffect* realEffect = nullptr;
    HRESULT hr = m_real->CreateEffect(rguid, lpeff, &realEffect, punkOuter);

    if (SUCCEEDED(hr) && realEffect) {
        // Wrap the real effect with our filter
        *ppdeff = new WrapperEffect(realEffect, m_filter);
        return hr;
    }

    // If the real device can't create the effect (e.g. vJoy) and FFB is blocked,
    // return a null-effect so the caller doesn't see an error.
    if (!m_filter->isFFBAllowed()) {
        LOG_DEBUG("Real CreateEffect failed (hr=0x%08lx) but FFB blocked — returning null effect", hr);
        *ppdeff = new WrapperEffect(rguid, m_filter);
        return DI_OK;
    }

    // Otherwise propagate the real error
    *ppdeff = nullptr;
    return hr;
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::EnumEffects(
    EnumFxCbT lpCallback, LPVOID pvRef, DWORD dwEffType)
{
    return m_real->EnumEffects(lpCallback, pvRef, dwEffType);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::GetEffectInfo(EffInfoT* pdei, REFGUID rguid) {
    return m_real->GetEffectInfo(pdei, rguid);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::GetForceFeedbackState(LPDWORD pdwOut) {
    if (!m_filter->isFFBAllowed()) {
        if (pdwOut) *pdwOut = 0;
        return DI_OK;
    }
    return m_real->GetForceFeedbackState(pdwOut);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::SendForceFeedbackCommand(DWORD dwFlags) {
    m_filter->logCommand(dwFlags);

    if (!m_filter->isFFBAllowed()) {
        return DI_OK;  // silently swallow
    }
    return m_real->SendForceFeedbackCommand(dwFlags);
}

template<bool U>
HRESULT STDMETHODCALLTYPE WrapperDevice8<U>::EnumCreatedEffectObjects(
    LPDIENUMCREATEDEFFECTOBJECTSCALLBACK lpCallback, LPVOID pvRef, DWORD fl)
{
    return m_real->EnumCreatedEffectObjects(lpCallback, pvRef, fl);
}

// ============================================================================
// Explicit template instantiations
// ============================================================================
template class WrapperDevice8<false>;  // A
template class WrapperDevice8<true>;   // W
