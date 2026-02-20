// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Valmantas Paliksa
#include "logger.h"
#include <cstdarg>
#include <cstring>
#include <windows.h>

Logger& Logger::instance() {
    static Logger s;
    return s;
}

void Logger::init(const wchar_t* dllDirectory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file) return;

    wchar_t path[MAX_PATH];
    wcscpy_s(path, dllDirectory);
    wcscat_s(path, L"\\dinput8_wrapper.log");

    m_file = _wfopen(path, L"a");
    if (m_file) {
        fprintf(m_file, "\n=== dinput8 wrapper loaded ===\n");
        fflush(m_file);
    }
}

void Logger::setLevel(LogLevel level) {
    m_level = level;
}

void Logger::log(LogLevel level, const char* fmt, ...) {
    if (level > m_level || !m_file) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(m_file, "[%02d:%02d:%02d.%03d] ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    const char* prefix = "";
    switch (level) {
        case LogLevel::Error: prefix = "[ERROR] "; break;
        case LogLevel::Warn:  prefix = "[WARN]  "; break;
        case LogLevel::Info:  prefix = "[INFO]  "; break;
        case LogLevel::Debug: prefix = "[DEBUG] "; break;
        default: break;
    }
    fputs(prefix, m_file);

    va_list args;
    va_start(args, fmt);
    vfprintf(m_file, fmt, args);
    va_end(args);

    fputc('\n', m_file);
    fflush(m_file);
}

void Logger::logW(LogLevel level, const wchar_t* fmt, ...) {
    if (level > m_level || !m_file) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(m_file, "[%02d:%02d:%02d.%03d] ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    const char* prefix = "";
    switch (level) {
        case LogLevel::Error: prefix = "[ERROR] "; break;
        case LogLevel::Warn:  prefix = "[WARN]  "; break;
        case LogLevel::Info:  prefix = "[INFO]  "; break;
        case LogLevel::Debug: prefix = "[DEBUG] "; break;
        default: break;
    }
    fputs(prefix, m_file);

    va_list args;
    va_start(args, fmt);
    vfwprintf(m_file, fmt, args);
    va_end(args);

    fputc('\n', m_file);
    fflush(m_file);
}

void Logger::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
}

Logger::~Logger() {
    close();
}
