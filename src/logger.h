// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#pragma once

#include <cstdio>
#include <mutex>

enum class LogLevel : int {
    None  = 0,
    Error = 1,
    Warn  = 2,
    Info  = 3,
    Debug = 4
};

class Logger {
public:
    static Logger& instance();

    void init(const wchar_t* dllDirectory);
    void setLevel(LogLevel level);
    LogLevel level() const { return m_level; }

    void log(LogLevel level, const char* fmt, ...);
    void logW(LogLevel level, const wchar_t* fmt, ...);
    void close();

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    FILE*      m_file  = nullptr;
    LogLevel   m_level = LogLevel::Info;
    std::mutex m_mutex;
};

#define LOG_ERROR(fmt, ...) Logger::instance().log(LogLevel::Error, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Logger::instance().log(LogLevel::Warn,  fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  Logger::instance().log(LogLevel::Info,  fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) Logger::instance().log(LogLevel::Debug, fmt, ##__VA_ARGS__)
