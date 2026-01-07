# Windows Event Log Forwarder - Sequence Diagram

## Event Forwarding Flow

```mermaid
sequenceDiagram
    actor User
    participant Main
    participant GlobalLogger
    participant Logger
    participant ForwarderAPI
    participant LogForwarder
    participant EventLogReader
    participant JsonUtils
    participant WindowsEventLog
    participant SIEMServer

    %% Initialization Phase
    User->>Main: Execute log_forwarder.exe [server] [port]
    activate Main

    Main->>GlobalLogger: initializeGlobalLogger("forwarder_logs.csv")
    activate GlobalLogger
    GlobalLogger->>Logger: new Logger("forwarder_logs.csv")
    activate Logger
    Logger->>Logger: initialize()
    Logger-->>Logger: Create CSV file with headers
    Logger-->>GlobalLogger: Logger instance
    GlobalLogger-->>Main: success
    deactivate GlobalLogger

    Main->>Logger: info("Main", "Forwarder starting")
    Logger-->>Logger: Write to CSV

    Main->>Main: Parse command-line arguments
    Main->>Logger: info("Main", "Target SIEM server", "IP:PORT")

    Main->>ForwarderAPI: runForwarder(serverAddress, serverPort)
    activate ForwarderAPI

    %% Forwarder Initialization
    ForwarderAPI->>LogForwarder: new LogForwarder(server, port)
    activate LogForwarder

    ForwarderAPI->>LogForwarder: initialize()
    LogForwarder->>LogForwarder: WSAStartup()
    LogForwarder->>Logger: info("LogForwarder", "WSA initialized")
    LogForwarder-->>ForwarderAPI: success

    %% Connection Phase
    ForwarderAPI->>Logger: info("ForwarderAPI", "Attempting connection")

    loop Retry until connected
        ForwarderAPI->>LogForwarder: connect()
        LogForwarder->>LogForwarder: Create socket
        LogForwarder->>SIEMServer: TCP connect
        alt Connection successful
            SIEMServer-->>LogForwarder: Connection established
            LogForwarder->>Logger: info("LogForwarder", "Connected")
            LogForwarder-->>ForwarderAPI: success
        else Connection failed
            SIEMServer-->>LogForwarder: Connection refused
            LogForwarder->>Logger: error("LogForwarder", "Connect failed")
            LogForwarder-->>ForwarderAPI: failure
            ForwarderAPI->>ForwarderAPI: sleep(RECONNECT_DELAY_MS)
        end
    end

    ForwarderAPI->>Logger: info("ForwarderAPI", "Connection established")

    %% Event Monitoring Phase
    ForwarderAPI->>WindowsEventLog: EvtSubscribe("System", EvtSubscribeToFutureEvents)
    WindowsEventLog-->>ForwarderAPI: Subscription handle

    ForwarderAPI->>Logger: info("EventLogReader", "Subscribed to event log")

    %% Main Event Loop
    loop Continuous monitoring
        ForwarderAPI->>WindowsEventLog: EvtNext() - Wait for events
        WindowsEventLog-->>ForwarderAPI: Event batch (up to 10 events)

        loop For each event in batch
            ForwarderAPI->>EventLogReader: formatEventAsJson(hEvent)
            activate EventLogReader

            EventLogReader->>EventLogReader: getEventProperty(EventID)
            EventLogReader->>EventLogReader: getEventProperty(Level)
            EventLogReader->>EventLogReader: getEventProperty(Channel)
            EventLogReader->>EventLogReader: getEventProperty(Computer)
            EventLogReader->>EventLogReader: getEventProperty(TimeCreated)

            EventLogReader->>JsonUtils: escapeJson(propertyValue)
            activate JsonUtils
            JsonUtils-->>EventLogReader: Escaped string
            deactivate JsonUtils

            EventLogReader-->>ForwarderAPI: JSON string
            deactivate EventLogReader

            %% Check connection and reconnect if needed
            alt Not connected
                ForwarderAPI->>Logger: warning("ForwarderAPI", "Connection lost")
                ForwarderAPI->>LogForwarder: connect()
                LogForwarder->>SIEMServer: TCP connect
                SIEMServer-->>LogForwarder: Connection established
                LogForwarder->>Logger: info("LogForwarder", "Reconnected")
            end

            %% Send event to SIEM
            ForwarderAPI->>LogForwarder: sendLog(jsonString)
            LogForwarder->>LogForwarder: Append newline
            LogForwarder->>SIEMServer: send() via TCP

            alt Send successful
                SIEMServer-->>LogForwarder: ACK
                LogForwarder->>Logger: debug("LogForwarder", "Log sent", bytes)
                LogForwarder-->>ForwarderAPI: success
                ForwarderAPI->>Logger: info("ForwarderAPI", "Event forwarded")
            else Send failed
                SIEMServer-->>LogForwarder: Error
                LogForwarder->>Logger: error("LogForwarder", "Send failed", error)
                LogForwarder-->>ForwarderAPI: failure
                ForwarderAPI->>Logger: error("ForwarderAPI", "Forward failed")
            end

            ForwarderAPI->>WindowsEventLog: EvtClose(hEvent)
        end

        ForwarderAPI->>ForwarderAPI: sleep(100ms)
    end

    %% Shutdown (in case of graceful exit)
    ForwarderAPI->>LogForwarder: disconnect()
    LogForwarder->>SIEMServer: closesocket()
    LogForwarder->>Logger: info("LogForwarder", "Disconnected")
    LogForwarder->>LogForwarder: WSACleanup()
    deactivate LogForwarder

    ForwarderAPI-->>Main: exit code
    deactivate ForwarderAPI

    Main->>Logger: info("Main", "Shutting down")
    Main->>GlobalLogger: shutdownGlobalLogger()
    activate GlobalLogger
    GlobalLogger->>Logger: flush()
    GlobalLogger->>Logger: delete Logger
    deactivate Logger
    deactivate GlobalLogger

    Main-->>User: Exit
    deactivate Main
```

## Sequence Description

### 1. Initialization Phase (Lines 1-35)
- User launches the application
- Main initializes the global CSV logger
- Command-line arguments are parsed
- Logger records startup configuration

### 2. Connection Phase (Lines 36-62)
- ForwarderAPI creates LogForwarder instance
- Winsock is initialized (WSAStartup)
- Connection attempt to SIEM server
- Retry loop with exponential backoff if connection fails
- All connection attempts are logged

### 3. Event Monitoring Phase (Lines 63-72)
- Subscribe to Windows Event Log channel ("System")
- Subscription handle obtained
- Ready to receive events

### 4. Main Event Loop (Lines 73-122)
- **Event Retrieval**: Wait for new events (up to 10 per batch)
- **Event Processing**: For each event:
  1. Extract properties (EventID, Level, Channel, Computer, TimeCreated)
  2. Format as JSON with proper escaping
  3. Check connection status
  4. Reconnect if connection was lost
  5. Send JSON to SIEM server via TCP
  6. Log success/failure
  7. Close event handle
- **Throttling**: Small delay between batches

### 5. Shutdown Phase (Lines 123-140)
- Graceful disconnection from SIEM server
- Close socket and cleanup Winsock
- Flush all pending log entries
- Destroy logger instance
- Exit application

## Key Interactions

| Source | Target | Purpose |
|--------|--------|---------|
| All Components | Logger | Log all operations, errors, warnings |
| ForwarderAPI | LogForwarder | Network communication management |
| ForwarderAPI | EventLogReader | Event extraction and JSON formatting |
| EventLogReader | JsonUtils | String escaping for JSON compliance |
| LogForwarder | SIEM Server | TCP transmission of JSON events |
| ForwarderAPI | Windows Event Log | Subscribe and retrieve events |

## Error Handling Flow

```mermaid
graph TD
    A[Operation] --> B{Success?}
    B -->|Yes| C[Log INFO/DEBUG]
    B -->|No| D[Log ERROR/WARNING]
    D --> E{Recoverable?}
    E -->|Yes| F[Retry with backoff]
    E -->|No| G[Fail gracefully]
    F --> A
    C --> H[Continue]
    G --> H
```

## Logging Points

Every significant operation is logged with:
- **Timestamp**: Exact time of operation
- **Level**: INFO, WARNING, ERROR, or DEBUG
- **Component**: Which module logged the event
- **Message**: Description of the operation
- **Details**: Additional context (IP addresses, error codes, byte counts, etc.)

All logs are written to `forwarder_logs.csv` in real-time for monitoring and troubleshooting.
