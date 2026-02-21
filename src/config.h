// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#pragma once

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

    // Look up the FFB policy for a given device product name.
    // Writes results into outEnabled and outScale.
    void getDevicePolicy(const wchar_t* productName,
                         bool& outEnabled, int& outScale) const;

private:
    Config() = default;
    static std::wstring trim(const std::wstring& s);
    static std::wstring toLower(const std::wstring& s);
};
