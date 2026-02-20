// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#include "config.h"
#include <fstream>
#include <algorithm>
#include <cctype>

Config& Config::instance() {
    static Config s;
    return s;
}

std::wstring Config::trim(const std::wstring& s) {
    auto start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    auto end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::wstring Config::toLower(const std::wstring& s) {
    std::wstring r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::towlower);
    return r;
}

bool Config::load(const wchar_t* iniPath) {
    std::wifstream file(iniPath);
    if (!file.is_open()) return false;

    std::wstring line;
    std::wstring section;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == L';' || line[0] == L'#')
            continue;

        // Section header
        if (line.front() == L'[' && line.back() == L']') {
            section = toLower(trim(line.substr(1, line.size() - 2)));
            continue;
        }

        // Key=Value
        auto eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;

        std::wstring key   = trim(line.substr(0, eq));
        std::wstring value = trim(line.substr(eq + 1));
        std::wstring keyLo = toLower(key);
        std::wstring valLo = toLower(value);

        if (section == L"general") {
            if (keyLo == L"enabled")
                enabled = (valLo == L"true" || valLo == L"1");
            else if (keyLo == L"loglevel") {
                int lvl = _wtoi(value.c_str());
                if (lvl >= 0 && lvl <= 4)
                    logLevel = static_cast<LogLevel>(lvl);
            }
        }
        else if (section == L"ffb") {
            if (keyLo == L"enabled")
                ffbEnabled = (valLo == L"true" || valLo == L"1");
            else if (keyLo == L"logeffects")
                ffbLogEffects = (valLo == L"true" || valLo == L"1");
            else if (keyLo == L"defaultscale") {
                int s = _wtoi(value.c_str());
                ffbDefaultScale = std::clamp(s, 0, 100);
            }
        }
        else if (section == L"ffbdevices") {
            DeviceRule rule;
            rule.nameMatch = key;  // keep original case for display

            if (valLo == L"block") {
                rule.ffbEnabled = false;
                rule.ffbScale   = 0;
            }
            else if (valLo == L"allow") {
                rule.ffbEnabled = true;
                rule.ffbScale   = 100;
            }
            else {
                int s = _wtoi(value.c_str());
                rule.ffbEnabled = (s > 0);
                rule.ffbScale   = std::clamp(s, 0, 100);
            }
            deviceRules.push_back(rule);
        }
    }

    return true;
}

void Config::getDevicePolicy(const wchar_t* productName,
                             bool& outEnabled, int& outScale) const
{
    // Default: use global settings
    outEnabled = ffbEnabled;
    outScale   = ffbDefaultScale;

    if (!productName) return;

    std::wstring nameLo = toLower(productName);

    for (const auto& rule : deviceRules) {
        std::wstring matchLo = toLower(rule.nameMatch);
        if (nameLo.find(matchLo) != std::wstring::npos) {
            outEnabled = rule.ffbEnabled;
            outScale   = rule.ffbScale;
            return;  // first match wins
        }
    }
}
