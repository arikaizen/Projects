# Windows Event Log Forwarder - Tests

This directory contains standalone tests with **no external dependencies**.

## Test Files

### Event Log Reader Tests
- **`test_event_log_reader_simple.cpp`** - Tests for event_log_reader component
  - Tests all event extraction functions
  - Tests all output formats (JSON, XML, Plain Text)
  - 11 comprehensive tests

### Log Forwarder Tests
- **`test_log_forwarder.cpp`** - Comprehensive tests for log_forwarder component
  - Unit tests for TCP socket functionality
  - Connection, send, disconnect, reconnect tests
  - Integration tests with Splunk HEC on link-local address
  - Mock TCP server for isolated testing
  - 13+ comprehensive tests

### Realtime Console Tests
- **`test_realtime_console.cpp`** - Realtime event monitoring demo

## Build Scripts

- **`build_simple_tests.bat`** - Build and run event_log_reader tests
- **`build_log_forwarder_test.bat`** - Build log_forwarder tests
- **`build_realtime_console.bat`** - Build realtime console

## Running Tests

### Event Log Reader Tests
```batch
cd tst
build_simple_tests.bat
```

Output:
```
Testing: getEventProperty - Event ID... [PASS]
Testing: getRawEventXml - Returns XML... [PASS]
Testing: formatEventAsPlainText - Returns formatted text... [PASS]
...
Results: 11 passed, 0 failed
```

### Log Forwarder Tests
```batch
cd tst
build_log_forwarder_test.bat
cd ..\bin
test_log_forwarder.exe
```

**Prerequisites**:
- Configure Splunk HEC on link-local address (see `docs/SPLUNK_SETUP.md`)
- Update `SPLUNK_SERVER` and `SPLUNK_PORT` in `test_log_forwarder.cpp`
- Or tests will skip Splunk integration tests if server unavailable

Output:
```
========================================
Log Forwarder Tests
========================================

Running Unit Tests...
========================================
Testing: LogForwarder constructor... [PASS]
Testing: LogForwarder initialize (WSA startup)... [PASS]
Testing: LogForwarder send log successfully... [PASS]
...

Running Splunk Integration Tests...
========================================
Testing: LogForwarder connect to Splunk on link-local address...
  Attempting to connect to: 169.254.1.1:8088
  [SUCCESS] Connected to Splunk!
[PASS]
...

Test Results
========================================
Passed: 13
Failed: 0
Total:  13
========================================
```

### Realtime Console
```batch
cd tst
build_realtime_console.bat
```

## Test Requirements

- MinGW-w64 with g++ compiler
- Windows Event Log API (wevtapi.lib)
- No external test frameworks needed

## Adding New Tests

To add a new test:

1. Add a test function:
```cpp
bool test_MyNewFeature() {
    TEST_START("My new feature");

    // Your test code here
    ASSERT_TRUE(condition, "Error message");

    TEST_PASS();
    return true;
}
```

2. Call it from main():
```cpp
int main() {
    // ...
    test_MyNewFeature();
    // ...
}
```

3. Build and run:
```batch
build_simple_tests.bat
```

## Test Macros

- `TEST_START(name)` - Start a test
- `TEST_PASS()` - Mark test as passed
- `TEST_FAIL(msg)` - Mark test as failed
- `ASSERT_TRUE(condition, msg)` - Assert condition is true
- `ASSERT_FALSE(condition, msg)` - Assert condition is false

## Splunk Integration

The log forwarder tests include integration tests with Splunk HEC (HTTP Event Collector) on link-local addresses.

### Quick Setup

1. **Find your link-local address**:
   ```cmd
   ipconfig | findstr "Link-local"
   ```

2. **Configure Splunk HEC**:
   - Enable HEC in Splunk Web (Settings → Data Inputs → HTTP Event Collector)
   - Create a new token
   - Configure to listen on your link-local address (see `docs/SPLUNK_SETUP.md`)

3. **Update test configuration**:
   - Edit `test_log_forwarder.cpp`
   - Set `SPLUNK_SERVER` to your link-local address (e.g., `169.254.1.1`)
   - Set `SPLUNK_PORT` to your HEC port (default: `8088`)

4. **Run tests**:
   ```batch
   cd tst
   build_log_forwarder_test.bat
   cd ..\bin
   test_log_forwarder.exe
   ```

For detailed Splunk configuration instructions, see [`docs/SPLUNK_SETUP.md`](../docs/SPLUNK_SETUP.md).
