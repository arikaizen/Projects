# Linux System Log Forwarder

A C++ application that reads Linux system logs via systemd journald and forwards them to the SIEM server in real-time.

## Features

- Real-time systemd journal monitoring
- Forwards logs from all systemd services
- JSON-formatted log transmission
- Automatic reconnection on network failure
- Configurable SIEM server address and port

## Prerequisites

### Linux Build Requirements

- **CMake** (3.10 or higher)
- **GCC/G++** or Clang compiler
- **libsystemd-dev** - systemd development libraries
- **make** - Build automation tool

### SIEM Server Requirements

- SIEM server must be running and listening on port 8089
- Network connectivity between forwarder and SIEM server

## Automated Prerequisite Installation

### Option 1: Automatic Installation (Recommended)

```bash
./check_prerequisites.sh
```

This script will:
- ✅ Detect your package manager (apt, dnf, yum, pacman)
- ✅ Check for CMake, G++, libsystemd-dev, and make
- ✅ Offer to install missing packages automatically
- ✅ Verify all prerequisites are met

### Supported Distributions

- **Ubuntu/Debian**: Uses apt
- **Fedora**: Uses dnf
- **RHEL/CentOS**: Uses yum
- **Arch Linux**: Uses pacman

**Recommended workflow:**
1. Run `./check_prerequisites.sh`
2. Follow prompts to install missing tools
3. Run `./build.sh` to compile

## Building the Forwarder

### Automatic Build

Simply run the build script:

```bash
./build.sh
```

This will:
1. Check for CMake and libsystemd
2. Generate build files
3. Compile the project
4. Place the executable in `bin/log_forwarder`

### Manual Build

```bash
# Create build directory
mkdir build
cd build

# Generate build files
cmake ..

# Build the project
make -j$(nproc)

# The executables will be in: bin/log_forwarder and bin/test_forwarder
```

## Running the Forwarder

### Default (localhost:8089)

```bash
cd bin
./log_forwarder
```

### Custom SIEM Server

```bash
./log_forwarder <server_ip> <port>
```

Example:
```bash
./log_forwarder 192.168.1.100 8089
```

### Running as Service

To run as a systemd service, create `/etc/systemd/system/log-forwarder.service`:

```ini
[Unit]
Description=SIEM Log Forwarder
After=network.target

[Service]
Type=simple
ExecStart=/path/to/bin/log_forwarder 192.168.1.100 8089
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Then enable and start:
```bash
sudo systemctl enable log-forwarder
sudo systemctl start log-forwarder
sudo systemctl status log-forwarder
```

## Testing the Forwarder

A test program is included to verify the forwarder works correctly without needing a full SIEM server.

### Running the Test

The build process creates two executables in the `bin/` folder:
- `log_forwarder` - The main forwarder
- `test_forwarder` - Mock SIEM server for testing

**Step 1: Start the test server (Terminal 1)**
```bash
cd bin
./test_forwarder
```

This starts a mock SIEM server that listens on port 8089 and validates incoming logs.

**Step 2: Run the forwarder (Terminal 2)**
```bash
cd bin
./log_forwarder
```

The forwarder will connect to the test server and begin forwarding system logs.

**Step 3: Observe the results**

The test server will display:
- Each received log in JSON format
- Validation results for each log
- Statistics (total logs, valid logs, invalid logs)
- Final test results

**Expected Output:**
```
[TEST] Received Log #1:
{"message":"Started Session 1 of user root","priority":"6","unit":"systemd","hostname":"linux-server","pid":"1","timestamp":1234567890}
[TEST] ✓ Log validation PASSED
[TEST] Statistics: 1 total, 1 valid, 0 invalid
```

If all logs pass validation, you'll see:
```
[TEST] ✓✓✓ ALL TESTS PASSED ✓✓✓
```

## Configuration

The forwarder monitors all systemd journal entries in real-time, including:
- **systemd services**: Service start/stop, failures
- **kernel logs**: Hardware, drivers, kernel messages
- **application logs**: Any application using systemd logging
- **user sessions**: Login/logout events

## Log Format

Logs are forwarded in JSON format:

```json
{
  "message": "Service started successfully",
  "priority": "6",
  "unit": "nginx.service",
  "hostname": "web-server-01",
  "pid": "1234",
  "timestamp": 1704067200000000
}
```

### Priority Levels

- 0: Emergency
- 1: Alert
- 2: Critical
- 3: Error
- 4: Warning
- 5: Notice
- 6: Informational
- 7: Debug

## Integration with SIEM

### Starting the SIEM Server

The SIEM server must have a listener on port 8089 to receive forwarded logs:

```python
python siem/main/src/app.py
```

### Search Integration

When a search is performed in the SIEM Search app, it will query both:
1. Local SIEM database
2. Real-time logs from connected forwarders

## Troubleshooting

### "Failed to open journal"
- Ensure systemd is running
- Check journal permissions: `sudo journalctl --verify`
- Run forwarder with sufficient permissions

### "Unable to connect to server"
- Verify SIEM server is running
- Check firewall allows connections on port 8089
- Verify server IP address and port

### Build Errors
- Ensure CMake is installed: `cmake --version`
- Ensure libsystemd is installed: `pkg-config --exists libsystemd && echo OK`
- Try cleaning build directory: `rm -rf build`

## Development

### Project Structure

```
siem/forwarder/linux/
├── src/
│   ├── main.cpp              # Entry point
│   ├── log_forwarder.cpp     # Network layer
│   ├── journal_reader.cpp    # Journald integration
│   ├── json_utils.cpp        # JSON formatting
│   └── forwarder_api.cpp     # Main API
├── inc/
│   ├── log_forwarder.h       # Network API
│   ├── journal_reader.h      # Journal API
│   ├── json_utils.h          # JSON utilities
│   └── forwarder_api.h       # Main API
├── tst/
│   └── test_forwarder.cpp    # Test suite
├── bin/                       # Compiled executables
├── build/                     # Build artifacts
├── CMakeLists.txt            # Build configuration
├── build.sh                  # Build script
├── check_prerequisites.sh    # Prerequisite checker
└── README.md                 # This file
```

### Adding New Features

1. Modify source files in `src/`
2. Run `./build.sh` to recompile
3. Test with SIEM server or test suite

## License

Part of the SIEM project.
