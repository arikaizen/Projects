# Windows Event Log Forwarder - Unit Tests

This directory contains comprehensive unit tests for the Windows Event Log Forwarder project using the Google Test framework.

## Test Coverage

### ✅ test_json_utils.cpp
**Tests for**: `json_utils.cpp`

- JSON string escaping
- Special character handling (quotes, backslashes, newlines, tabs)
- Unicode character support
- Control character escaping
- Edge cases (empty strings, long strings)

**Total Tests**: 13

### ✅ test_logger.cpp
**Tests for**: `logger.cpp`

- Logger initialization
- CSV file creation with headers
- Logging at different levels (INFO, WARNING, ERROR, DEBUG)
- CSV escaping (commas, quotes, newlines)
- Thread safety with concurrent logging
- Append mode behavior
- Global logger instance management
- Flush operations

**Total Tests**: 18

### ✅ test_log_forwarder.cpp
**Tests for**: `log_forwarder.cpp`

- Constructor and initialization
- WSAStartup/Winsock initialization
- TCP connection establishment
- Connection failure handling
- Send operations
- Disconnect and reconnect
- Mock SIEM server integration
- Edge cases (empty strings, large data)

**Total Tests**: 16

### ✅ test_event_log_reader.cpp
**Tests for**: `event_log_reader.cpp`

- Event property extraction (EventID, Level, Channel, Computer, etc.)
- JSON formatting of events
- Special character escaping in JSON
- Multiple event handling
- Invalid handle error handling
- Output consistency

**Total Tests**: 13

**Total**: 60+ unit tests

## Real-Time Console Monitor

### 🖥️ test_realtime_console.cpp
**Purpose**: Standalone test program that reads Windows Event Logs and prints them to the console in real-time with color coding.

This is NOT a unit test but a demonstration/testing tool that:
- Monitors Windows Event Logs without requiring a SIEM server
- Prints events as JSON directly to the console
- Supports both real-time and historical modes
- Uses ANSI color codes for visual clarity
- Perfect for testing event log reading functionality

**Features**:
- ✅ Real-time event monitoring
- ✅ Historical event reading (all events or recent N hours)
- ✅ Color-coded console output
- ✅ Event counter and timing information
- ✅ Multiple channel support (System, Application, Security)
- ✅ Command-line configuration

**Usage**:
```bash
# Build the console test program
build_realtime_console.bat

# Run with defaults (System log, real-time mode)
..\bin\test_realtime_console.exe

# Monitor Application log in real-time
..\bin\test_realtime_console.exe Application realtime

# Read last 12 hours from System log
..\bin\test_realtime_console.exe System recent 12

# Read ALL historical events from Security log
..\bin\test_realtime_console.exe Security all

# Show help
..\bin\test_realtime_console.exe --help
```

**Example Output**:
```
========================================
  Windows Event Log Real-Time Monitor
========================================

[Monitor] Channel: System
[Monitor] Mode: REAL-TIME (Future events only)
[Monitor] Waiting for new events... (Press Ctrl+C to stop)

[Event #1 | +0s]
{"event_id":"7036","level":"4","channel":"System","computer":"DESKTOP-ABC123",...}

[Event #2 | +5s]
{"event_id":"7040","level":"4","channel":"System","computer":"DESKTOP-ABC123",...}
```

**When to use**:
- Testing event log reader functionality
- Verifying events are being generated
- Debugging JSON formatting
- Demonstrating the forwarder capabilities without SIEM server
- Troubleshooting permission issues (Security log access)

**Build separately** from unit tests using:
```bash
build_realtime_console.bat
```

---

## Requirements

### 1. Compiler
- MinGW-w64 with g++ compiler
- C++17 support

### 2. Google Test Framework
The tests use Google Test (gtest) framework. You need to download it:

**Option A - Git Clone (Recommended)**:
```bash
cd tst/
git clone https://github.com/google/googletest.git
```

**Option B - Manual Download**:
1. Go to: https://github.com/google/googletest/releases
2. Download the latest release
3. Extract to `tst/googletest/`

## Building and Running Tests

### Quick Start

1. **Navigate to tst directory**:
   ```bash
   cd siem/forwarder/windows/tst
   ```

2. **Install Google Test** (if not already done):
   ```bash
   git clone https://github.com/google/googletest.git
   ```

3. **Build and run all tests**:
   ```bash
   build_tests.bat
   ```

### Running Specific Tests

Run tests from a specific test suite:
```bash
build_tests.bat JsonUtilsTest.*
build_tests.bat LoggerTest.*
build_tests.bat LogForwarderTest.*
build_tests.bat EventLogReaderTest.*
```

Run a specific test:
```bash
build_tests.bat JsonUtilsTest.EscapeJson_WithQuotes
build_tests.bat LoggerTest.ThreadSafety_ConcurrentLogging
```

### Google Test Filter Patterns

```bash
# Run all tests in LoggerTest
build_tests.bat LoggerTest.*

# Run tests that match a pattern
build_tests.bat *Thread*

# Run multiple specific tests
build_tests.bat LoggerTest.LogInfo*:LoggerTest.LogError*

# Exclude tests
build_tests.bat -*Thread*
```

## Test Organization

```
tst/
├── test_json_utils.cpp        # JSON utility tests
├── test_logger.cpp            # Logger tests
├── test_log_forwarder.cpp     # Network forwarder tests
├── test_event_log_reader.cpp  # Event log reading tests
├── test_main.cpp              # Test runner entry point
├── test_realtime_console.cpp  # Real-time console monitor (standalone)
├── build_tests.bat            # Build and run unit tests
├── build_realtime_console.bat # Build console monitor
├── README.md                  # This file
├── googletest/                # Google Test framework (you download)
└── bin/                       # Build output (created automatically)
    ├── test_runner.exe        # Unit test executable
    ├── test_realtime_console.exe  # Console monitor executable
    ├── *.o                    # Object files
    └── test_*.csv             # Test log files (temporary)
```

## Understanding Test Results

### Successful Test Run
```
[==========] Running 60 tests from 4 test suites.
[----------] Global test environment set-up.
[----------] 13 tests from JsonUtilsTest
[ RUN      ] JsonUtilsTest.EscapeJson_SimpleString
[       OK ] JsonUtilsTest.EscapeJson_SimpleString (0 ms)
...
[----------] 13 tests from JsonUtilsTest (45 ms total)
...
[==========] 60 tests from 4 test suites ran. (2341 ms total)
[  PASSED  ] 60 tests.
```

### Failed Test Run
```
[  FAILED  ] LoggerTest.Initialize_CreatesFileWithHeader
Expected: (content.find("Timestamp")) != (std::string::npos)
  Actual: 18446744073709551615 vs 18446744073709551615
```

## Test Types

### Unit Tests
Pure unit tests that test individual functions in isolation:
- `test_json_utils.cpp` - All tests
- `test_logger.cpp` - Most tests

### Integration Tests
Tests that require system resources or external dependencies:
- `test_log_forwarder.cpp` - Requires network/sockets
- `test_event_log_reader.cpp` - Requires Windows Event Log access

## Common Issues

### Issue: Google Test not found
**Error**: `ERROR: Google Test not found!`

**Solution**:
```bash
cd test/
git clone https://github.com/google/googletest.git
```

### Issue: Event log tests fail
**Error**: `No events available in System log`

**Solution**: These tests require actual Windows Event Log access. Run as Administrator or ensure System log has events.

### Issue: Network tests fail
**Error**: `Connection failed` or `Address already in use`

**Solution**:
- Ensure no other service is using port 19999
- Check firewall settings
- Try running as Administrator

### Issue: Thread safety tests fail
**Error**: Line count mismatch in concurrent logging tests

**Solution**: This is rare but can happen under extreme system load. Re-run the tests.

## Writing New Tests

### Test Structure

```cpp
#include <gtest/gtest.h>
#include "../inc/your_module.h"

// Test fixture (optional)
class YourModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code before each test
    }

    void TearDown() override {
        // Cleanup code after each test
    }
};

// Test case
TEST_F(YourModuleTest, TestName_Scenario_ExpectedBehavior) {
    // Arrange
    int input = 5;

    // Act
    int result = yourFunction(input);

    // Assert
    EXPECT_EQ(result, 10);
}
```

### Assertions

```cpp
// Equality
EXPECT_EQ(actual, expected);
ASSERT_EQ(actual, expected);  // Stops test on failure

// Boolean
EXPECT_TRUE(condition);
EXPECT_FALSE(condition);

// Comparison
EXPECT_LT(val1, val2);  // Less than
EXPECT_LE(val1, val2);  // Less than or equal
EXPECT_GT(val1, val2);  // Greater than
EXPECT_GE(val1, val2);  // Greater than or equal

// Strings
EXPECT_STREQ(str1, str2);
EXPECT_STRNE(str1, str2);

// Exceptions
EXPECT_THROW(statement, exception_type);
EXPECT_NO_THROW(statement);
```

## Continuous Integration

To integrate with CI/CD:

```bash
# Run tests and capture exit code
build_tests.bat
if %ERRORLEVEL% NEQ 0 exit /b 1
```

## Performance

Test execution time (approximate):
- JSON Utils: ~50ms
- Logger: ~500ms (includes thread safety tests)
- Log Forwarder: ~1000ms (network operations)
- Event Log Reader: ~800ms (Windows API calls)

**Total**: ~2-3 seconds for all tests

## Best Practices

1. **Run tests before committing**: Ensure all tests pass before pushing code
2. **Add tests for new features**: Every new function should have tests
3. **Test edge cases**: Empty strings, null pointers, large data
4. **Test error conditions**: Not just happy paths
5. **Keep tests fast**: Mock external dependencies when possible
6. **Use descriptive names**: `TestName_Scenario_ExpectedBehavior`

## Troubleshooting

### All Tests Failing

1. Verify Google Test is installed:
   ```bash
   dir googletest\googletest\include\gtest
   ```

2. Check compiler version:
   ```bash
   g++ --version
   ```

3. Try clean build:
   ```bash
   rmdir /s /q bin
   rmdir /s /q googletest\build
   build_tests.bat
   ```

### Specific Test Failing

1. Run only that test:
   ```bash
   build_tests.bat TestSuite.SpecificTest
   ```

2. Add debug output:
   ```cpp
   std::cout << "Debug: value = " << value << std::endl;
   ```

3. Use debugger:
   ```bash
   gdb bin/test_runner.exe
   ```

## Additional Resources

- [Google Test Documentation](https://google.github.io/googletest/)
- [Google Test Primer](https://google.github.io/googletest/primer.html)
- [Google Test Advanced Guide](https://google.github.io/googletest/advanced.html)

## Contributing

When adding new tests:
1. Follow the existing test structure
2. Use meaningful test names
3. Add comments for complex test logic
4. Update this README with new test counts
5. Ensure all tests pass before committing

## License

Same as the main project.
