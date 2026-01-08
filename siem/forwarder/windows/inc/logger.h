/**
 * @file logger.h
 * @brief CSV-based logging API for Windows Event Log Forwarder
 *
 * This module provides CSV-formatted logging functionality to track
 * forwarder operations, connections, errors, and event forwarding status.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <string>   // std::string
#include <fstream>  // std::ofstream
#include <mutex>    // std::mutex, std::lock_guard

/**
 * @enum LogLevel
 * @brief Severity levels for log messages
 */
enum class LogLevel {
    INFO,       ///< Informational messages
    WARNING,    ///< Warning messages
    ERROR,      ///< Error messages
    DEBUG       ///< Debug messages
};

/**
 * @class Logger
 * @brief Thread-safe CSV logger for forwarder operations
 *
 * This class provides CSV-formatted logging to track all forwarder activities.
 * CSV format: Timestamp,Level,Component,Message,Details
 *
 * Example CSV output:
 * 2026-01-07 14:30:45.123,INFO,LogForwarder,Connected to server,192.168.1.100:8089
 * 2026-01-07 14:30:46.987,INFO,EventReader,Event forwarded,EventID=4624
 */
class Logger {
private:
    std::ofstream logFile;              ///< Output file stream
    std::mutex logMutex;                ///< Mutex for thread-safe logging
    std::string logFilePath;            ///< Path to log file
    bool isOpen;                        ///< File open status

    /**
    * @brief Get current timestamp in CSV format
    * @return Formatted timestamp string (YYYY-MM-DD HH:MM:SS.sss)
     */
    std::string getCurrentTimestamp();

    /**
     * @brief Convert log level enum to string
     * @param level Log level enum value
     * @return String representation of log level
     */
    std::string logLevelToString(LogLevel level);

    /**
     * @brief Escape CSV special characters in a string
     * @param value String to escape
     * @return CSV-safe string with escaped quotes and commas
     */
    std::string escapeCSV(const std::string& value);

public:
    /**
     * @brief Construct a new Logger object
     * @param filepath Path to CSV log file (default: forwarder_logs.csv)
     */
    Logger(const std::string& filepath = "forwarder_logs.csv");

    // Deleted copy/move to avoid shared file handles
    Logger(const Logger&) = delete;            // copy ctor
    Logger& operator=(const Logger&) = delete; // copy assign
    Logger(Logger&&) = delete;                 // move ctor
    Logger& operator=(Logger&&) = delete;      // move assign

    /**
     * @brief Destroy the Logger object and close file
     */
    ~Logger();

    /**
     * @brief Initialize logger and create CSV file with headers
     * @return true if initialization successful, false otherwise
     */
    bool initialize();

    /**
     * @brief Log a message to CSV file
     * @param level Severity level of the message
     * @param component Component/module name (e.g., "LogForwarder", "EventReader")
     * @param message Main log message
     * @param details Additional details (optional)
     */
    void log(LogLevel level, const std::string& component,
             const std::string& message, const std::string& details = "");

    /**
     * @brief Log INFO level message
     * @param component Component/module name
     * @param message Main log message
     * @param details Additional details (optional)
     */
    void info(const std::string& component, const std::string& message,
              const std::string& details = "");

    /**
     * @brief Log WARNING level message
     * @param component Component/module name
     * @param message Main log message
     * @param details Additional details (optional)
     */
    void warning(const std::string& component, const std::string& message,
                 const std::string& details = "");

    /**
     * @brief Log ERROR level message
     * @param component Component/module name
     * @param message Main log message
     * @param details Additional details (optional)
     */
    void error(const std::string& component, const std::string& message,
               const std::string& details = "");

    /**
     * @brief Log DEBUG level message
     * @param component Component/module name
     * @param message Main log message
     * @param details Additional details (optional)
     */
    void debug(const std::string& component, const std::string& message,
               const std::string& details = "");

    /**
     * @brief Check if logger is properly initialized
     * @return true if logger is open and ready, false otherwise
     */
    bool isReady() const;

    /**
     * @brief Flush buffered log entries to disk
     */
    void flush();
};

// Global logger instance declaration
extern Logger* g_logger;

/**
 * @brief Initialize global logger instance
 * @param filepath Path to CSV log file (default: forwarder_logs.csv)
 * @return true if initialization successful, false otherwise
 */
bool initializeGlobalLogger(const std::string& filepath = "forwarder_logs.csv");

/**
 * @brief Shutdown global logger instance
 */
void shutdownGlobalLogger();

#endif // LOGGER_H
