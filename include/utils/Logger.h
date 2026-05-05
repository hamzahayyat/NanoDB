#pragma once
#include <cstdio>
#include <cstdarg>
#include <ctime>

// ─────────────────────────────────────────────
//  Logger – thread-safe-ish file + stdout log
// ─────────────────────────────────────────────
class Logger {
public:
    static FILE* logFile;

    static void init(const char* path = "nanodb_execution.log") {
        logFile = std::fopen(path, "w");
    }

    static void close() {
        if (logFile) { std::fclose(logFile); logFile = nullptr; }
    }

    static void log(const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        std::printf("[LOG] %s\n", buf);
        if (logFile) std::fprintf(logFile, "[LOG] %s\n", buf);
    }

    static void info(const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        std::printf("[INFO] %s\n", buf);
        if (logFile) std::fprintf(logFile, "[INFO] %s\n", buf);
    }

    static void error(const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        std::fprintf(stderr, "[ERROR] %s\n", buf);
        if (logFile) std::fprintf(logFile, "[ERROR] %s\n", buf);
    }
};

inline FILE* Logger::logFile = nullptr;
