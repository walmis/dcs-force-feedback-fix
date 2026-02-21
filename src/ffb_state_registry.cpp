// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#include "ffb_state_registry.h"
#include "logger.h"
#include <algorithm>
#include <cstring>

// ============================================================================
// Singleton
// ============================================================================
FFBStateRegistry& FFBStateRegistry::instance() {
    static FFBStateRegistry s;
    return s;
}

std::wstring FFBStateRegistry::toLower(const std::wstring& s) {
    std::wstring r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::towlower);
    return r;
}

// ============================================================================
// Deep-copy helpers
// ============================================================================

// Deep-copy a DIEFFECT structure into an EffectStateRecord.
// All pointer fields are captured into owned vectors; the params struct's
// pointers are then redirected to those vectors' data.
static void deepCopyParams(EffectStateRecord& rec, const DIEFFECT* peff) {
    if (!peff) {
        rec.hasParams = false;
        return;
    }

    rec.params = *peff;  // shallow copy first

    // Axes array
    if (peff->rgdwAxes && peff->cAxes > 0) {
        rec.axes.assign(peff->rgdwAxes, peff->rgdwAxes + peff->cAxes);
        rec.params.rgdwAxes = rec.axes.data();
    } else {
        rec.axes.clear();
        rec.params.rgdwAxes = nullptr;
    }

    // Direction array
    if (peff->rglDirection && peff->cAxes > 0) {
        rec.directions.assign(peff->rglDirection, peff->rglDirection + peff->cAxes);
        rec.params.rglDirection = rec.directions.data();
    } else {
        rec.directions.clear();
        rec.params.rglDirection = nullptr;
    }

    // Type-specific data (flat struct: DICONSTANTFORCE, DICONDITION[], etc.)
    if (peff->lpvTypeSpecificParams && peff->cbTypeSpecificParams > 0) {
        rec.typeSpecific.resize(peff->cbTypeSpecificParams);
        std::memcpy(rec.typeSpecific.data(),
                    peff->lpvTypeSpecificParams,
                    peff->cbTypeSpecificParams);
        rec.params.lpvTypeSpecificParams = rec.typeSpecific.data();
    } else {
        rec.typeSpecific.clear();
        rec.params.lpvTypeSpecificParams = nullptr;
        rec.params.cbTypeSpecificParams = 0;
    }

    // Envelope (optional)
    if (peff->lpEnvelope) {
        rec.envelope = *peff->lpEnvelope;
        rec.hasEnvelope = true;
        rec.params.lpEnvelope = &rec.envelope;
    } else {
        rec.hasEnvelope = false;
        rec.params.lpEnvelope = nullptr;
    }

    rec.hasParams = true;
}

// ============================================================================
// Recording
// ============================================================================

void FFBStateRegistry::recordStart(const std::wstring& deviceName,
                                   REFGUID effectGuid,
                                   DWORD iterations, DWORD flags)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& rec = m_records[toLower(deviceName)][effectGuid];
    rec.guid           = effectGuid;
    rec.wasRunning     = true;
    rec.lastIterations = iterations;
    rec.lastStartFlags = flags;
}

void FFBStateRegistry::recordStop(const std::wstring& deviceName,
                                  REFGUID effectGuid)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto nameIt = m_records.find(toLower(deviceName));
    if (nameIt == m_records.end()) return;
    auto guidIt = nameIt->second.find(effectGuid);
    if (guidIt == nameIt->second.end()) return;
    guidIt->second.wasRunning = false;
}

void FFBStateRegistry::recordParams(const std::wstring& deviceName,
                                    REFGUID effectGuid,
                                    const DIEFFECT* peff)
{
    if (!peff) return;

    LOG_DEBUG("FFBStateRegistry::recordParams [%ls] axes=%lu typeSpec=%lu "
              "gain=%lu duration=%lu envelope=%s",
              deviceName.c_str(),
              peff->cAxes,
              peff->cbTypeSpecificParams,
              peff->dwGain,
              peff->dwDuration,
              peff->lpEnvelope ? "yes" : "no");

    std::lock_guard<std::mutex> lock(m_mutex);
    auto& rec = m_records[toLower(deviceName)][effectGuid];
    rec.guid = effectGuid;
    deepCopyParams(rec, peff);
}

// ============================================================================
// Querying
// ============================================================================

bool FFBStateRegistry::wasRunning(const std::wstring& deviceName,
                                  REFGUID effectGuid,
                                  DWORD& outIterations,
                                  DWORD& outFlags) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto nameIt = m_records.find(toLower(deviceName));
    if (nameIt == m_records.end()) return false;
    auto guidIt = nameIt->second.find(effectGuid);
    if (guidIt == nameIt->second.end()) return false;
    if (!guidIt->second.wasRunning) return false;
    outIterations = guidIt->second.lastIterations;
    outFlags      = guidIt->second.lastStartFlags;
    return true;
}

const EffectStateRecord* FFBStateRegistry::getRecord(
    const std::wstring& deviceName, REFGUID effectGuid) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto nameIt = m_records.find(toLower(deviceName));
    if (nameIt == m_records.end()) return nullptr;
    auto guidIt = nameIt->second.find(effectGuid);
    if (guidIt == nameIt->second.end()) return nullptr;
    return &guidIt->second;
}

// ============================================================================
// Maintenance
// ============================================================================

void FFBStateRegistry::clearDevice(const std::wstring& deviceName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_records.erase(toLower(deviceName));
}

void FFBStateRegistry::clearAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_records.clear();
}
