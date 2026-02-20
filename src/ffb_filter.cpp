// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#include "ffb_filter.h"
#include "config.h"
#include "logger.h"

FFBFilter::FFBFilter(const FFBPolicy& policy, const std::wstring& deviceName)
    : m_policy(policy)
    , m_deviceName(deviceName)
{}

// ---------------------------------------------------------------------------
// Force scaling
// ---------------------------------------------------------------------------
void FFBFilter::scaleEffect(DIEFFECT* pEffect) const {
    if (!pEffect || m_policy.scale >= 100) return;

    float factor = m_policy.scale / 100.0f;

    // Scale gain (global effect strength 0-10000)
    pEffect->dwGain = static_cast<DWORD>(pEffect->dwGain * factor);

    if (!pEffect->lpvTypeSpecificParams || pEffect->cbTypeSpecificParams == 0)
        return;

    // Constant force
    if (pEffect->cbTypeSpecificParams >= sizeof(DICONSTANTFORCE)) {
        auto* p = static_cast<DICONSTANTFORCE*>(pEffect->lpvTypeSpecificParams);
        p->lMagnitude = static_cast<LONG>(p->lMagnitude * factor);
    }
    // Ramp force
    else if (pEffect->cbTypeSpecificParams >= sizeof(DIRAMPFORCE)) {
        auto* p = static_cast<DIRAMPFORCE*>(pEffect->lpvTypeSpecificParams);
        p->lStart = static_cast<LONG>(p->lStart * factor);
        p->lEnd   = static_cast<LONG>(p->lEnd   * factor);
    }
    // Periodic force (sine, square, triangle, sawtooth)
    else if (pEffect->cbTypeSpecificParams >= sizeof(DIPERIODIC)) {
        auto* p = static_cast<DIPERIODIC*>(pEffect->lpvTypeSpecificParams);
        p->dwMagnitude = static_cast<DWORD>(p->dwMagnitude * factor);
    }
}

// ---------------------------------------------------------------------------
// GUID helpers
// ---------------------------------------------------------------------------
const char* FFBFilter::effectGuidToString(REFGUID guid) {
    if (guid == GUID_ConstantForce) return "ConstantForce";
    if (guid == GUID_RampForce)     return "RampForce";
    if (guid == GUID_Square)        return "Square";
    if (guid == GUID_Sine)          return "Sine";
    if (guid == GUID_Triangle)      return "Triangle";
    if (guid == GUID_SawtoothUp)    return "SawtoothUp";
    if (guid == GUID_SawtoothDown)  return "SawtoothDown";
    if (guid == GUID_Spring)        return "Spring";
    if (guid == GUID_Damper)        return "Damper";
    if (guid == GUID_Inertia)       return "Inertia";
    if (guid == GUID_Friction)      return "Friction";
    if (guid == GUID_CustomForce)   return "CustomForce";
    return "Unknown";
}

const char* FFBFilter::ffbCommandToString(DWORD cmd) {
    switch (cmd) {
        case DISFFC_RESET:           return "RESET";
        case DISFFC_STOPALL:         return "STOPALL";
        case DISFFC_PAUSE:           return "PAUSE";
        case DISFFC_CONTINUE:        return "CONTINUE";
        case DISFFC_SETACTUATORSON:  return "SETACTUATORSON";
        case DISFFC_SETACTUATORSOFF: return "SETACTUATORSOFF";
        default:                     return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
void FFBFilter::logEffectCreation(REFGUID rguid) const {
    if (!Config::instance().ffbLogEffects) return;
    LOG_INFO("FFB [%ls] CreateEffect: type=%s  policy=%s  scale=%d%%",
             m_deviceName.c_str(),
             effectGuidToString(rguid),
             m_policy.enabled ? "allow" : "BLOCK",
             m_policy.scale);
}

void FFBFilter::logEffectStart(DWORD dwIterations, DWORD dwFlags) const {
    if (!Config::instance().ffbLogEffects) return;
    LOG_INFO("FFB [%ls] Effect.Start: iterations=%lu  flags=0x%lx",
             m_deviceName.c_str(), dwIterations, dwFlags);
}

void FFBFilter::logEffectStop() const {
    if (!Config::instance().ffbLogEffects) return;
    LOG_INFO("FFB [%ls] Effect.Stop", m_deviceName.c_str());
}

void FFBFilter::logEffectParams(const DIEFFECT* pEffect) const {
    if (!Config::instance().ffbLogEffects || !pEffect) return;
    LOG_DEBUG("FFB [%ls] Effect.SetParams: gain=%lu  duration=%lu  samplePeriod=%lu  axes=%lu",
              m_deviceName.c_str(),
              pEffect->dwGain,
              pEffect->dwDuration,
              pEffect->dwSamplePeriod,
              pEffect->cAxes);
}

void FFBFilter::logCommand(DWORD dwCommand) const {
    if (!Config::instance().ffbLogEffects) return;
    LOG_INFO("FFB [%ls] SendCommand: %s (0x%lx)  policy=%s",
             m_deviceName.c_str(),
             ffbCommandToString(dwCommand),
             dwCommand,
             m_policy.enabled ? "allow" : "BLOCK");
}
