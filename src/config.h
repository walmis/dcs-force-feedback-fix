// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#pragma once

#include <climits>
#include <string>
#include <vector>
#include "logger.h"

struct DeviceRule {
    std::wstring nameMatch;    // Case-insensitive substring to match against product name
    bool         ffbEnabled;   // true = allow FFB, false = block FFB
    int          ffbScale;     // 0-100 scale percentage (only meaningful when ffbEnabled=true)
};

class Config {
public:
    static Config& instance();

    // Load from INI file. Returns true if file was found and parsed.
    bool load(const wchar_t* iniPath);

    // [General]
    bool     enabled   = true;
    LogLevel logLevel  = LogLevel::Info;

    // [FFB]
    bool ffbEnabled      = true;
    bool ffbLogEffects   = true;
    int  ffbDefaultScale = 100;
    bool ffbAutoRestart  = true;   // auto-restart effects after device reconnect

    // [FFBDevices] — ordered rules, first match wins
    std::vector<DeviceRule> deviceRules;

    // ---- device enumeration order ------------------------------------
    //
    // Windows caches the order DirectInput reports devices in, and a game
    // that gives force feedback to the first devices it sees will keep
    // giving it to the same ones no matter which stick the user wants.
    // Nothing short of unplugging hardware moves that order.
    //
    // Off unless [DeviceOrder] lists something: some games identify a
    // device by its position rather than its name, and reordering
    // underneath one that did not ask would scramble its bindings.
    struct OrderEntry {
        std::wstring nameMatch;  // same case-insensitive substring as above
        int          rank = 0;   // lower comes first
    };
    std::vector<OrderEntry> deviceOrder;

    /// Whether any reordering is configured.
    bool orderingActive() const { return !deviceOrder.empty(); }

    /// Where this device should sit, or INT_MAX for one nobody listed -
    /// unlisted devices keep their order, after the listed ones.
    int orderRank(const wchar_t* productName) const;

    // Look up the FFB policy for a given device product name.
    // Writes results into outEnabled and outScale.
    void getDevicePolicy(const wchar_t* productName,
                         bool& outEnabled, int& outScale) const;

private:
    Config() = default;
    static std::wstring trim(const std::wstring& s);
    static std::wstring toLower(const std::wstring& s);
};
