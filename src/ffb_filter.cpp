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

// Helper: is this a condition-type effect?
static bool isConditionEffect(REFGUID guid) {
    return guid == GUID_Spring || guid == GUID_Damper ||
           guid == GUID_Inertia || guid == GUID_Friction;
}

// Helper: is this a periodic-type effect?
static bool isPeriodicEffect(REFGUID guid) {
    return guid == GUID_Square || guid == GUID_Sine ||
           guid == GUID_Triangle || guid == GUID_SawtoothUp ||
           guid == GUID_SawtoothDown;
}

void FFBFilter::scaleEffect(DIEFFECT* pEffect, REFGUID effectGuid) const {
    if (!pEffect || m_policy.scale >= 100) return;

    float factor = m_policy.scale / 100.0f;

    // Scale gain (global effect strength 0-10000)
    pEffect->dwGain = static_cast<DWORD>(pEffect->dwGain * factor);

    if (!pEffect->lpvTypeSpecificParams || pEffect->cbTypeSpecificParams == 0)
        return;

    // Constant force — DICONSTANTFORCE { lMagnitude }
    if (effectGuid == GUID_ConstantForce &&
        pEffect->cbTypeSpecificParams >= sizeof(DICONSTANTFORCE))
    {
        auto* p = static_cast<DICONSTANTFORCE*>(pEffect->lpvTypeSpecificParams);
        p->lMagnitude = static_cast<LONG>(p->lMagnitude * factor);
    }
    // Ramp force — DIRAMPFORCE { lStart, lEnd }
    else if (effectGuid == GUID_RampForce &&
             pEffect->cbTypeSpecificParams >= sizeof(DIRAMPFORCE))
    {
        auto* p = static_cast<DIRAMPFORCE*>(pEffect->lpvTypeSpecificParams);
        p->lStart = static_cast<LONG>(p->lStart * factor);
        p->lEnd   = static_cast<LONG>(p->lEnd   * factor);
    }
    // Periodic — DIPERIODIC { dwMagnitude, lOffset, dwPhase, dwPeriod }
    // Scale magnitude only; offset/phase/period are positional, not force.
    else if (isPeriodicEffect(effectGuid) &&
             pEffect->cbTypeSpecificParams >= sizeof(DIPERIODIC))
    {
        auto* p = static_cast<DIPERIODIC*>(pEffect->lpvTypeSpecificParams);
        p->dwMagnitude = static_cast<DWORD>(p->dwMagnitude * factor);
    }
    // Condition — DICONDITION[] (one per axis)
    // Scale coefficients and saturation; do NOT scale offset or deadband.
    else if (isConditionEffect(effectGuid) &&
             pEffect->cbTypeSpecificParams >= sizeof(DICONDITION))
    {
        DWORD count = pEffect->cbTypeSpecificParams / sizeof(DICONDITION);
        auto* conds = static_cast<DICONDITION*>(pEffect->lpvTypeSpecificParams);
        for (DWORD i = 0; i < count; ++i) {
            conds[i].lPositiveCoefficient =
                static_cast<LONG>(conds[i].lPositiveCoefficient * factor);
            conds[i].lNegativeCoefficient =
                static_cast<LONG>(conds[i].lNegativeCoefficient * factor);
            conds[i].dwPositiveSaturation =
                static_cast<DWORD>(conds[i].dwPositiveSaturation * factor);
            conds[i].dwNegativeSaturation =
                static_cast<DWORD>(conds[i].dwNegativeSaturation * factor);
            // lOffset and lDeadBand are NOT scaled — they are positional
        }
    }
    // Custom force — DICUSTOMFORCE { cChannels, cSamples, dwSamplePeriod, rglForceData[] }
    else if (effectGuid == GUID_CustomForce &&
             pEffect->cbTypeSpecificParams >= sizeof(DICUSTOMFORCE))
    {
        auto* p = static_cast<DICUSTOMFORCE*>(pEffect->lpvTypeSpecificParams);
        if (p->rglForceData) {
            for (DWORD i = 0; i < p->cSamples * p->cChannels; ++i) {
                p->rglForceData[i] = static_cast<LONG>(p->rglForceData[i] * factor);
            }
        }
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
