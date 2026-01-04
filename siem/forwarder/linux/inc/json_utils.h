/**
 * @file json_utils.h
 * @brief JSON formatting and escaping utilities
 *
 * This module provides utility functions for JSON string formatting,
 * including proper escaping of special characters.
 */

#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <string>

/**
 * @brief Escape special characters in a string for JSON compliance
 *
 * Converts special characters (quotes, backslashes, control characters)
 * into their JSON-escaped equivalents to ensure valid JSON output.
 *
 * @param str Input string to escape
 * @return JSON-safe escaped string
 */
std::string escapeJson(const std::string& str);

#endif // JSON_UTILS_H
