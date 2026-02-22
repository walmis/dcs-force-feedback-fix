// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#pragma once

#include <windows.h>
#include <dinput.h>
#include <string>

// Per-device FFB policy resolved from config.
struct FFBPolicy {
    bool enabled = true;   // false = all FFB operations silently blocked
    int  scale   = 100;    // 0-100 force magnitude scaling
};

// Stateless helper that applies FFB policy decisions and logging for one device.
class FFBFilter {
public:
    FFBFilter(const FFBPolicy& policy, const std::wstring& deviceName);

    bool isFFBAllowed() const { return m_policy.enabled; }
    int  getScale()     const { return m_policy.scale; }
    const std::wstring& deviceName() const { return m_deviceName; }

    // Scale type-specific force magnitudes in a DIEFFECT copy (modifies in place).
    // effectGuid is required to correctly identify the type-specific data struct.
    void scaleEffect(DIEFFECT* pEffect, REFGUID effectGuid) const;

    // --------------- Logging helpers ---------------
    void logEffectCreation(REFGUID rguid) const;
    void logEffectStart(DWORD dwIterations, DWORD dwFlags) const;
    void logEffectStop() const;
    void logEffectParams(const DIEFFECT* pEffect) const;
    void logCommand(DWORD dwCommand) const;

    static const char* effectGuidToString(REFGUID guid);
    static const char* ffbCommandToString(DWORD cmd);

private:
    FFBPolicy    m_policy;
    std::wstring m_deviceName;
};
