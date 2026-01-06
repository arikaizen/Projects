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

A C++ application that reads Windows Event Logs and forwards them to the SIEM server in real-time.

## Features

- Real-time Windows Event Log monitoring
- Forwards logs from System, Application, and Security channels
- JSON-formatted log transmission
- Automatic reconnection on network failure
- Configurable SIEM server address and port

## Prerequisites

### Windows Build Requirements

- **CMake** (3.10 or higher): [Download CMake](https://cmake.org/download/)
- **MinGW-w64** (PRIMARY REQUIREMENT - ~200MB, includes Windows SDK)
  - **Perfect for Windows 10** - lightweight and fast
  - Download: [MinGW-w64 Builds](https://github.com/niXman/mingw-builds-binaries/releases)
  - Get latest: `x86_64-*-release-posix-seh-ucrt-*.7z` (look for newest version)
  - Extract to `C:\mingw64`
  - Add `C:\mingw64\bin` to PATH environment variable
  - Includes winevt.h and all Windows Event Log API headers
- **Visual Studio 2019+** (OPTIONAL ALTERNATIVE - ~7GB)
  - Only if you already have it installed
  - Otherwise, use MinGW-w64 for better performance on Windows 10

### SIEM Server Requirements

- SIEM server must be running and listening on port 8089
- Network connectivity between forwarder and SIEM server

## Automated Prerequisite Installation

**NEW!** Automated scripts to check and install build tools:

### Option 1: PowerShell (Recommended - Can auto-install CMake)

```powershell
.\install_prerequisites.ps1
```

This script will:
- ✅ Check for CMake, Visual Studio, and Windows SDK
- ✅ Automatically download and install CMake if missing
- ✅ Provide download links for Visual Studio
- ✅ Verify all prerequisites are met

### Option 2: Batch Script (Check only)

```batch
check_prerequisites.bat
```

This script will:
- ✅ Check for all required build tools
- ✅ Provide download links if tools are missing
- ✅ Open download pages in browser

**Recommended workflow:**
1. Run `install_prerequisites.ps1` (PowerShell)
2. Follow prompts to install missing tools
3. Restart terminal after installation
4. Run `build.bat` to compile

## Building the Forwarder

### Automatic Build (Windows)

Simply run the build script:

```batch
build.bat
```

This will:
1. Check for CMake installation
2. Generate build files
3. Compile the project
4. Place the executable in `bin/log_forwarder.exe`

### Manual Build (Windows)

```batch
# Create build directory
mkdir build
cd build

# Generate build files
cmake .. -G "Visual Studio 16 2019" -A x64

# Build the project
cmake --build . --config Release

# The executable will be in: bin/log_forwarder.exe
```

### Alternative: MinGW Build

```batch
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

## Running the Forwarder

### Default (localhost:8089)

```batch
log_forwarder.exe
```

### Custom SIEM Server

```batch
log_forwarder.exe <server_ip> <port>
```

Example:
```batch
log_forwarder.exe 192.168.1.100 8089
```

### Running as Administrator

**Important**: To read Security event logs, you must run the forwarder with administrator privileges:

1. Right-click `log_forwarder.exe`
2. Select "Run as administrator"

Or from an elevated command prompt:
```batch
cd bin
log_forwarder.exe
```

## Testing the Forwarder

A test program is included to verify the forwarder works correctly without needing a full SIEM server.

### Running the Test

The build process creates two executables in the `bin/` folder:
- `log_forwarder.exe` - The main forwarder
- `test_forwarder.exe` - Mock SIEM server for testing

**Step 1: Start the test server (Terminal 1)**
```batch
cd bin
test_forwarder.exe
```

This starts a mock SIEM server that listens on port 8089 and validates incoming logs.

**Step 2: Run the forwarder (Terminal 2)**
```batch
cd bin
log_forwarder.exe
```

The forwarder will connect to the test server and begin forwarding Windows Event Logs.

**Step 3: Observe the results**

The test server will display:
- Each received log in JSON format
- Validation results for each log
- Statistics (total logs, valid logs, invalid logs)
- Final test results

**Expected Output:**
```
[TEST] Received Log #1:
{"event_id":"1234","level":"4","channel":"System","computer":"YOUR-PC","timestamp":133123456789}
[TEST] ✓ Log validation PASSED
[TEST] Statistics: 1 total, 1 valid, 0 invalid
```

If all logs pass validation, you'll see:
```
[TEST] ✓✓✓ ALL TESTS PASSED ✓✓✓
```

## Configuration

The forwarder monitors these Windows Event Log channels:
- **System**: System-level events
- **Application**: Application events
- **Security**: Security and audit events (requires admin)

To modify monitored channels, edit `src/main.cpp` and rebuild.

## Log Format

Logs are forwarded in JSON format:

```json
{
  "event_id": "1234",
  "level": "4",
  "channel": "System",
  "computer": "WORKSTATION-01",
  "timestamp": 133123456789012345
}
```

## Integration with SIEM

### Starting the SIEM Server

The SIEM server must have a listener on port 8089 to receive forwarded logs:

```python
# This will be added to the SIEM app
```

### Search Integration

When a search is performed in the SIEM Search app, it will query both:
1. Local SIEM database
2. Real-time logs from connected forwarders

## Troubleshooting

### "Failed to subscribe to event log"
- Run as Administrator
- Check Windows Event Log service is running: `services.msc`

### "Unable to connect to server"
- Verify SIEM server is running
- Check firewall allows connections on port 8089
- Verify server IP address and port

### Build Errors
- Ensure CMake is in PATH: `cmake --version`
- Ensure Visual Studio C++ tools are installed
- Try cleaning build directory: `rmdir /s /q build`

## Development

### Project Structure

```
siem/forwarder/windows/
├── src/
│   └── main.cpp          # Main forwarder application
├── inc/                  # Header files (if needed)
├── tst/                  # Test files
├── bin/                  # Output binaries (generated)
├── build/                # Build artifacts (generated)
├── CMakeLists.txt        # CMake configuration
├── build.bat             # Automatic build script
└── README.md             # This file
```

### Adding New Features

1. Modify `src/main.cpp`
2. Run `build.bat` to recompile
3. Test with SIEM server

## License

Part of the SIEM project.
