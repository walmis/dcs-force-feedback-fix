// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#pragma once

#include <windows.h>
#include <dinput.h>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// Captures the last-known state of a single DirectInput effect.
// Used to replay parameters + auto-start after device reconnection.
struct EffectStateRecord {
    GUID  guid          = {};
    bool  wasRunning    = false;
    DWORD lastIterations = 0;
    DWORD lastStartFlags = 0;

    // Deep-copied DIEFFECT parameters
    bool                hasParams      = false;
    DIEFFECT            params         = {};      // top-level struct (pointers replaced below)
    std::vector<DWORD>  axes;                     // rgdwAxes copy
    std::vector<LONG>   directions;               // rglDirection copy
    std::vector<BYTE>   typeSpecific;             // lpvTypeSpecificParams copy
    DIENVELOPE          envelope       = {};
    bool                hasEnvelope    = false;
};

// Comparer for GUID as a map key
struct GUIDLess {
    bool operator()(const GUID& a, const GUID& b) const {
        return memcmp(&a, &b, sizeof(GUID)) < 0;
    }
};

// Global singleton tracking FFB effect state across device lifetimes.
//
// When a device is disconnected and DCS re-creates the device + effects,
// this registry allows the wrapper to detect which effects were previously
// running and auto-start them with their last-known parameters.
//
// Keyed by device product name (case-insensitive) + effect GUID.
// Thread-safe via internal mutex.
class FFBStateRegistry {
public:
    static FFBStateRegistry& instance();

    // ---- Recording (called by WrapperEffect) ----

    // Record that effect was started. Marks wasRunning=true.
    void recordStart(const std::wstring& deviceName, REFGUID effectGuid,
                     DWORD iterations, DWORD flags);

    // Record that effect was stopped. Marks wasRunning=false.
    void recordStop(const std::wstring& deviceName, REFGUID effectGuid);

    // Deep-copy the DIEFFECT parameters for later replay.
    void recordParams(const std::wstring& deviceName, REFGUID effectGuid,
                      const DIEFFECT* peff);

    // ---- Querying (called by WrapperDevice8::CreateEffect) ----

    // Returns true if this effect type was previously running on this device.
    // Fills outIterations/outFlags with the last Start() arguments.
    bool wasRunning(const std::wstring& deviceName, REFGUID effectGuid,
                    DWORD& outIterations, DWORD& outFlags) const;

    // Get the full record for parameter replay. Returns nullptr if not found.
    // The returned pointer remains valid only while the caller holds no other
    // registry lock (single-threaded usage from CreateEffect is fine).
    const EffectStateRecord* getRecord(const std::wstring& deviceName,
                                       REFGUID effectGuid) const;

    // ---- Maintenance ----

    // Clear all records for a device (e.g. DISFFC_RESET).
    void clearDevice(const std::wstring& deviceName);

    // Clear everything.
    void clearAll();

private:
    FFBStateRegistry() = default;
    ~FFBStateRegistry() = default;

    static std::wstring toLower(const std::wstring& s);

    // Device name (lowered) → (effect GUID → record)
    std::map<std::wstring, std::map<GUID, EffectStateRecord, GUIDLess>> m_records;
    mutable std::mutex m_mutex;
};
