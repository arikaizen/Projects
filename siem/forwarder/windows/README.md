# Windows Event Log Forwarder

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
- **Visual Studio 2019** or later with C++ development tools
  - Or **MinGW-w64** for GCC compilation
- **Windows SDK** (included with Visual Studio)

### SIEM Server Requirements

- SIEM server must be running and listening on port 8089
- Network connectivity between forwarder and SIEM server

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
