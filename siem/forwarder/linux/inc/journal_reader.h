/**
 * @file journal_reader.h
 * @brief Linux systemd journal reading and monitoring API
 *
 * This module provides functionality to subscribe to systemd journal,
 * read log entries in real-time, and extract log properties.
 */

#ifndef JOURNAL_READER_H
#define JOURNAL_READER_H

#include <string>
#include <systemd/sd-journal.h>

/**
 * @brief Format a journal entry as JSON
 *
 * Extracts all relevant properties from a systemd journal entry and formats them
 * into a JSON string suitable for transmission to the SIEM server.
 *
 * @param journal Pointer to the systemd journal
 * @return JSON-formatted string containing log data
 */
std::string formatJournalEntryAsJson(sd_journal* journal);

#endif // JOURNAL_READER_H
