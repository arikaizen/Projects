# Windows Event Log Forwarder - Usage Guide

## Overview

The Windows Event Log Forwarder now supports both **real-time monitoring** and **historical event querying**. You can choose the mode when launching the forwarder to suit your specific needs.

## Command-Line Syntax

```
log_forwarder.exe [server_address] [port] [mode] [hours_back]
```

### Parameters

| Parameter | Description | Default | Required |
|-----------|-------------|---------|----------|
| `server_address` | SIEM server IP address or hostname | 127.0.0.1 | No |
| `port` | SIEM server TCP port number | 8089 | No |
| `mode` | Reading mode (see below) | realtime | No |
| `hours_back` | Hours to look back (for `recent` mode) | 24 | No |

## Reading Modes

### 1. **Real-Time Mode** (Default)

Monitors Windows Event Logs and forwards only **future events** (events that occur after the forwarder starts).

**Usage:**
```bash
log_forwarder.exe
log_forwarder.exe 192.168.1.100 8089
log_forwarder.exe 192.168.1.100 8089 realtime
```

**Characteristics:**
- ✅ Monitors events from the moment it starts
- ✅ Runs indefinitely (until manually stopped)
- ✅ Low resource usage (event-driven, no polling)
- ✅ Ideal for continuous SIEM monitoring
- ❌ Does NOT forward events that occurred before startup

**When to use:**
- Continuous 24/7 monitoring
- Production SIEM integration
- Real-time security monitoring

---

### 2. **Historical Mode - All Events**

Reads and forwards **ALL historical events** from the Windows Event Log.

**Usage:**
```bash
log_forwarder.exe 192.168.1.100 8089 all
```

**Characteristics:**
- ✅ Reads every event in the log
- ✅ Starts from the oldest event
- ✅ Terminates after forwarding all events
- ⚠️ Can forward thousands/millions of events
- ⚠️ May take a long time depending on log size

**When to use:**
- Initial SIEM population
- Migrating to a new SIEM system
- Forensic analysis of all historical data
- Disaster recovery scenarios

**⚠️ Warning:** This mode can forward a massive number of events. Ensure your SIEM can handle the volume.

---

### 3. **Historical Mode - Recent Events**

Reads and forwards events from the **last N hours**.

**Usage:**
```bash
# Last 24 hours (default)
log_forwarder.exe 192.168.1.100 8089 recent

# Last 12 hours
log_forwarder.exe 192.168.1.100 8089 recent 12

# Last 1 hour
log_forwarder.exe 192.168.1.100 8089 recent 1

# Last 7 days (168 hours)
log_forwarder.exe 192.168.1.100 8089 recent 168
```

**Characteristics:**
- ✅ Reads only recent events
- ✅ Configurable time window
- ✅ Terminates after forwarding all matching events
- ✅ Faster than reading all events

**When to use:**
- Backfilling recent events after forwarder downtime
- Testing SIEM integration with recent data
- Forensic analysis of recent incidents
- Catch-up after maintenance window

**Example Timeline:**
```
Current Time: 2:00 PM
Command: log_forwarder.exe 192.168.1.100 8089 recent 6

Events Forwarded:
├── 8:00 AM ← Oldest event forwarded
├── 9:00 AM
├── 10:00 AM
├── 11:00 AM
├── 12:00 PM
├── 1:00 PM
└── 2:00 PM ← Newest event forwarded

Events Ignored:
└── 7:59 AM and earlier ← Too old (outside 6-hour window)
```

---

## Usage Examples

### Example 1: Default Real-Time Monitoring
```bash
# Uses all defaults (127.0.0.1:8089, realtime mode)
log_forwarder.exe
```

**Output:**
```
========================================
Windows Event Log Forwarder for SIEM
========================================
Server: 127.0.0.1:8089
Mode: Real-Time Monitoring
========================================

[EventLogReader] Mode: REAL-TIME monitoring
[EventLogReader] Monitoring Windows Event Logs (real-time)...
[ForwarderAPI] Forwarded (1): {"event_id":"7045","level":"4",...}
[ForwarderAPI] Forwarded (2): {"event_id":"4624","level":"4",...}
...
```

---

### Example 2: Custom SIEM Server, Real-Time
```bash
log_forwarder.exe 192.168.1.100 8089 realtime
```

Forwards future events to custom SIEM server at 192.168.1.100:8089.

---

### Example 3: Forward ALL Historical Events
```bash
log_forwarder.exe 192.168.1.100 8089 all
```

**Output:**
```
========================================
Windows Event Log Forwarder for SIEM
========================================
Server: 192.168.1.100:8089
Mode: Historical (All Events)
========================================

[EventLogReader] Mode: HISTORICAL (All Events)
[EventLogReader] Reading Windows Event Logs (historical)...
[ForwarderAPI] Forwarded (1): {"event_id":"6005","level":"4",...}
[ForwarderAPI] Forwarded (2): {"event_id":"6006","level":"4",...}
...
[ForwarderAPI] Forwarded (15423): {"event_id":"7045","level":"4",...}
[EventLogReader] Finished reading historical events
[EventLogReader] Total events forwarded: 15423
[ForwarderAPI] Event log processing completed
```

---

### Example 4: Forward Last 12 Hours of Events
```bash
log_forwarder.exe 192.168.1.100 8089 recent 12
```

**Output:**
```
========================================
Windows Event Log Forwarder for SIEM
========================================
Server: 192.168.1.100:8089
Mode: Historical (Last 12 hours)
========================================

[EventLogReader] Mode: HISTORICAL (Recent)
[EventLogReader] Reading Windows Event Logs (historical)...
[ForwarderAPI] Forwarded (1): {"event_id":"4672","level":"4",...}
[ForwarderAPI] Forwarded (2): {"event_id":"4624","level":"4",...}
...
[ForwarderAPI] Forwarded (234): {"event_id":"7040","level":"4",...}
[EventLogReader] Finished reading historical events
[EventLogReader] Total events forwarded: 234
[ForwarderAPI] Event log processing completed
```

---

## Help Command

Get usage information:

```bash
log_forwarder.exe --help
log_forwarder.exe -h
log_forwarder.exe help
log_forwarder.exe /?
```

---

## How It Works

### Real-Time Mode

```
┌─────────────────────────────────────────┐
│  Windows Event Log Service              │
│  (Continuously generates events)        │
└────────────┬────────────────────────────┘
             │
             │ EvtSubscribe(EvtSubscribeToFutureEvents)
             ▼
┌─────────────────────────────────────────┐
│  Event Log Forwarder                    │
│  - Waits for new events (blocking)      │
│  - Formats as JSON                      │
│  - Forwards to SIEM                     │
└────────────┬────────────────────────────┘
             │
             │ TCP Socket
             ▼
┌─────────────────────────────────────────┐
│  SIEM Server                            │
│  (Receives JSON events in real-time)    │
└─────────────────────────────────────────┘
```

### Historical Mode

```
┌─────────────────────────────────────────┐
│  Windows Event Log Storage              │
│  (Past events stored on disk)           │
└────────────┬────────────────────────────┘
             │
             │ EvtQuery(XPath time filter)
             ▼
┌─────────────────────────────────────────┐
│  Event Log Forwarder                    │
│  - Queries events by time range         │
│  - Batches of 10 events at a time       │
│  - Formats as JSON                      │
│  - Forwards to SIEM                     │
│  - Terminates when done                 │
└────────────┬────────────────────────────┘
             │
             │ TCP Socket
             ▼
┌─────────────────────────────────────────┐
│  SIEM Server                            │
│  (Receives historical JSON events)      │
└─────────────────────────────────────────┘
```

---

## XPath Query Examples

When using historical modes, the forwarder generates XPath queries to filter events by time:

### Recent Mode (last 24 hours)
```xpath
*[System[TimeCreated[@SystemTime>='2026-01-07T10:30:00.000Z']]]
```

### All Events
```xpath
*
```

The queries use Windows Event Log's XPath syntax to efficiently filter events at the Windows API level, minimizing processing overhead.

---

## Logging

All operations are logged to `forwarder_logs.csv`:

```csv
Timestamp,Level,Component,Message,Details
2026-01-08 10:30:00,INFO,Main,Mode set to HISTORICAL_RECENT,Hours back: 12
2026-01-08 10:30:01,INFO,EventLogReader,Mode: Historical query,Reading events from last 12 hours
2026-01-08 10:30:01,INFO,EventLogReader,Successfully subscribed/queried event log,Historical mode
2026-01-08 10:30:05,INFO,ForwarderAPI,Event forwarded successfully,Total: 100
2026-01-08 10:30:10,INFO,EventLogReader,Finished reading historical events,Total forwarded: 234
```

---

## Best Practices

### For Production Monitoring
```bash
# Run in real-time mode as a Windows Service
log_forwarder.exe 192.168.1.100 8089 realtime
```

### For Initial Setup
```bash
# Step 1: Forward last 24 hours to populate SIEM
log_forwarder.exe 192.168.1.100 8089 recent 24

# Step 2: Start real-time monitoring
log_forwarder.exe 192.168.1.100 8089 realtime
```

### After Downtime
```bash
# Calculate downtime hours and backfill
log_forwarder.exe 192.168.1.100 8089 recent 4
```

### For Forensic Analysis
```bash
# Forward all events for investigation
log_forwarder.exe 192.168.1.100 8089 all
```

---

## Troubleshooting

### "Failed to subscribe/query event log channel"
**Cause:** Insufficient permissions
**Solution:** Run as Administrator

```bash
# Right-click -> Run as Administrator
# OR in elevated cmd:
log_forwarder.exe 192.168.1.100 8089 recent 12
```

### "No events forwarded" in Historical Mode
**Possible causes:**
1. No events in the specified time range
2. Windows Event Log is empty
3. Insufficient permissions

**Check:**
```bash
# Open Event Viewer and verify events exist in the System log
eventvwr.msc
```

### Historical Query Takes Too Long
**For very large logs:**
```bash
# Use smaller time windows
log_forwarder.exe 192.168.1.100 8089 recent 1   # 1 hour at a time
```

---

## Performance Considerations

| Mode | Events/Second | CPU Usage | Memory Usage | Network Usage |
|------|---------------|-----------|--------------|---------------|
| Realtime | 10-100 | Low | 10-20 MB | Low (bursty) |
| Historical (Recent) | 100-1000 | Medium | 10-20 MB | High (sustained) |
| Historical (All) | 100-1000 | Medium | 10-20 MB | Very High |

---

## Backward Compatibility

The forwarder remains **fully backward compatible**:

```bash
# Old usage still works (defaults to realtime mode)
log_forwarder.exe
log_forwarder.exe 192.168.1.100
log_forwarder.exe 192.168.1.100 8089
```

---

## Advanced Configuration

For more complex time ranges, you can extend the `EventReadMode::HISTORICAL_RANGE` mode to accept custom start/end times in ISO 8601 format (future enhancement).

---

## See Also

- [Architecture Diagram](architecture_diagram.md) - System design and data flow
- [Class Diagram](class_diagram.md) - Code structure
- [Sequence Diagram](sequence_diagram.md) - Runtime behavior
- [README](../README.md) - Project overview
