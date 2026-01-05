# Windows Event Log Forwarder

A Windows service/application that reads Windows Event Logs and forwards them to a SIEM server.

## Features

- Reads events from Windows Event Log (Application, Security, System, etc.)
- Forwards events to SIEM server via TCP socket
- JSON format event data
- Configurable polling interval
- Support for multiple event log channels

## Requirements

- Windows 7 or later
- Visual Studio 2015+ or MinGW-w64
- CMake 3.10 or later
- Administrator privileges (for reading Security logs)

## Building

### Quick Build

Simply run the build script:

```cmd
build.bat
```

The script will:
1. Detect your compiler (Visual Studio or MinGW)
2. Generate build files with CMake
3. Compile the project
4. Place executables in the `bin/` folder

### Manual Build

```cmd
mkdir build
cd build

:: For Visual Studio
cmake ..
cmake --build . --config Release

:: For MinGW
cmake -G "MinGW Makefiles" ..
mingw32-make

cd ..
```

## Usage

### Basic Usage

```cmd
bin\log_forwarder.exe <server> <port> [channel] [interval]
```

**Arguments:**
- `server` - SIEM server IP address or hostname
- `port` - SIEM server port
- `channel` - Event log channel (default: Application)
  - Common channels: Application, Security, System, Setup
- `interval` - Polling interval in seconds (default: 60)

### Examples

Forward Application events every 60 seconds:
```cmd
bin\log_forwarder.exe 192.168.1.100 5000
```

Forward Security events every 30 seconds:
```cmd
bin\log_forwarder.exe 192.168.1.100 5000 Security 30
```

Forward System events every 120 seconds:
```cmd
bin\log_forwarder.exe 192.168.1.100 5000 System 120
```

## Testing

Run the test executable to verify functionality:

```cmd
bin\test_forwarder.exe
```

**Note:** Some tests require a SIEM server running on localhost:5000

## Event Data Format

Events are sent as JSON with the following structure:

```json
{
  "timestamp": "2024-01-01T12:00:00",
  "eventId": "4624",
  "level": "Information",
  "source": "Microsoft-Windows-Security-Auditing",
  "computer": "WORKSTATION01",
  "message": "An account was successfully logged on..."
}
```

## Configuration

### Running as a Windows Service

To run continuously, you can:
1. Use Task Scheduler to run at startup
2. Convert to a Windows Service (requires additional code)
3. Run in a background terminal

### Firewall Configuration

Ensure outbound connections to your SIEM server are allowed:
```cmd
netsh advfirewall firewall add rule name="SIEM Forwarder" dir=out action=allow protocol=TCP remoteport=5000
```

## Troubleshooting

### "Access Denied" errors
- Run as Administrator (required for Security log)
- Check Event Log service is running

### Connection failures
- Verify SIEM server is running and accessible
- Check firewall settings
- Verify server address and port

### Missing winevt.h or winsock2.h
- Install Windows SDK
- For Visual Studio: Install C++ development tools
- For MinGW: Ensure MinGW-w64 is properly installed

## Project Structure

```
windows/
├── src/
│   ├── main.cpp              # Main application entry point
│   ├── event_log_reader.cpp  # Event log reading implementation
│   └── log_forwarder.cpp     # Network forwarding implementation
├── inc/
│   ├── event_log_reader.h    # Event log reader interface
│   └── log_forwarder.h       # Forwarder interface
├── tst/
│   └── test_forwarder.cpp    # Unit tests
├── bin/                      # Output binaries (created after build)
├── build/                    # CMake build files (created after build)
├── CMakeLists.txt            # CMake configuration
├── build.bat                 # Build script
└── README.md                 # This file
```

## License

Part of the SIEM project.
