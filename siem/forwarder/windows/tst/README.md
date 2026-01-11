# Windows Event Log Forwarder - Tests

This directory contains standalone tests with **no external dependencies**.

## Test Files

### Event Log Reader Tests
- **`test_event_log_reader_simple.cpp`** - Tests for event_log_reader component
  - Tests all event extraction functions
  - Tests all output formats (JSON, XML, Plain Text)
  - 11 comprehensive tests

### Realtime Console Tests
- **`test_realtime_console.cpp`** - Realtime event monitoring demo

## Build Scripts

- **`build_simple_tests.bat`** - Build and run event_log_reader tests
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
