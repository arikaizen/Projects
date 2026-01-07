/**
 * @file logger.cpp
 * @brief Implementation of CSV-based logger
 *
 * Provides thread-safe CSV logging functionality for the forwarder.
 */

#include "logger.h"   // Logger definitions
#include <iostream>    // std::cerr
#include <sstream>     // std::ostringstream
#include <iomanip>     // std::put_time, std::setfill, std::setw
#include <ctime>       // std::tm, localtime_s
#include <chrono>      // system_clock, duration_cast

// Global logger instance
Logger* g_logger = nullptr;

Logger::Logger(const std::string& filepath)
    : logFilePath(filepath), isOpen(false) {
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

bool Logger::initialize() {
    std::lock_guard<std::mutex> lock(logMutex);

    // Open file in append mode to preserve existing logs
    logFile.open(logFilePath, std::ios::out | std::ios::app);

    if (!logFile.is_open()) {
        std::cerr << "[Logger] Failed to open log file: " << logFilePath << std::endl;
        return false;
    }

    isOpen = true;

    // Check if file is empty (new file) and write CSV header
    logFile.seekp(0, std::ios::end);
    if (logFile.tellp() == 0) {
        logFile << "Timestamp,Level,Component,Message,Details" << std::endl;
    }

    return true;
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;

    std::tm tm_buf;
    // localtime_s: Convert time_t to local time. Returns 0 on success, nonzero error code on failure. Fills tm_buf structure with broken-down time.
    localtime_s(&tm_buf, &time_t_now);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::DEBUG:   return "DEBUG";
        default:                return "UNKNOWN";
    }
}

std::string Logger::escapeCSV(const std::string& value) {
    std::string escaped = value;

    // If value contains comma, quote, or newline, wrap in quotes
    bool needsQuotes = (value.find(',') != std::string::npos ||
                        value.find('"') != std::string::npos ||
                        value.find('\n') != std::string::npos);

    if (needsQuotes) {
        // Escape existing quotes by doubling them
        size_t pos = 0;
        while ((pos = escaped.find('"', pos)) != std::string::npos) {
            escaped.insert(pos, "\"");
            pos += 2;
        }
        escaped = "\"" + escaped + "\"";
    }

    return escaped;
}

void Logger::log(LogLevel level, const std::string& component,
                 const std::string& message, const std::string& details) {
    if (!isOpen) {
        return;
    }

    std::lock_guard<std::mutex> lock(logMutex);

    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = logLevelToString(level);

    // Write CSV row: Timestamp,Level,Component,Message,Details
    logFile << escapeCSV(timestamp) << ","
            << escapeCSV(levelStr) << ","
            << escapeCSV(component) << ","
            << escapeCSV(message) << ","
            << escapeCSV(details) << std::endl;

    // Auto-flush for real-time logging
    logFile.flush();
}

void Logger::info(const std::string& component, const std::string& message,
                  const std::string& details) {
    log(LogLevel::INFO, component, message, details);
}

void Logger::warning(const std::string& component, const std::string& message,
                     const std::string& details) {
    log(LogLevel::WARNING, component, message, details);
}

void Logger::error(const std::string& component, const std::string& message,
                   const std::string& details) {
    log(LogLevel::ERROR, component, message, details);
}

void Logger::debug(const std::string& component, const std::string& message,
                   const std::string& details) {
    log(LogLevel::DEBUG, component, message, details);
}

bool Logger::isReady() const {
    return isOpen;
}

void Logger::flush() {
    if (isOpen) {
        std::lock_guard<std::mutex> lock(logMutex);
        logFile.flush();
    }
}

// Global logger functions
bool initializeGlobalLogger(const std::string& filepath) {
    if (g_logger != nullptr) {
        delete g_logger;
    }

    g_logger = new Logger(filepath);
    if (!g_logger->initialize()) {
        delete g_logger;
        g_logger = nullptr;
        return false;
    }

    g_logger->info("Logger", "Logger initialized successfully", filepath);
    return true;
}

void shutdownGlobalLogger() {
    if (g_logger != nullptr) {
        g_logger->info("Logger", "Logger shutting down", "");
        g_logger->flush();
        delete g_logger;
        g_logger = nullptr;
    }
}
