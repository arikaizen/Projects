# Splunk Integration Setup for Windows Log Forwarder

This guide explains how to configure Splunk to receive Windows Event Logs from the log forwarder on a link-local address.

## Table of Contents

- [Overview](#overview)
- [Link-Local Addresses](#link-local-addresses)
- [Splunk HEC Configuration](#splunk-hec-configuration)
- [Testing the Connection](#testing-the-connection)
- [Troubleshooting](#troubleshooting)

## Overview

The Windows Log Forwarder sends JSON-formatted event logs over TCP to a SIEM server. For Splunk integration, we use the HTTP Event Collector (HEC) endpoint to receive logs on a link-local network address.

### Architecture

```
Windows Event Log → Log Forwarder → TCP Socket → Splunk HEC (Link-Local)
```

## Link-Local Addresses

Link-local addresses are IP addresses that are only valid for communications within the local network segment.

### IPv4 Link-Local (APIPA)
- **Range**: 169.254.0.0/16
- **Example**: 169.254.1.1, 169.254.10.50
- **Use Case**: Automatic addressing when DHCP is unavailable

### IPv6 Link-Local
- **Range**: fe80::/10
- **Example**: fe80::1, fe80::a00:27ff:fe4e:66a1
- **Use Case**: Every IPv6-enabled interface automatically gets one

### Finding Your Link-Local Address

**Windows:**
```cmd
# View all network adapters and their addresses
ipconfig

# Filter for link-local addresses
ipconfig | findstr "Link-local"
ipconfig | findstr "169.254"
```

**PowerShell:**
```powershell
# View IPv4 link-local addresses
Get-NetIPAddress -AddressFamily IPv4 | Where-Object {$_.IPAddress -like "169.254.*"}

# View IPv6 link-local addresses
Get-NetIPAddress -AddressFamily IPv6 | Where-Object {$_.PrefixOrigin -eq "WellKnown"}
```

## Splunk HEC Configuration

### Step 1: Enable HTTP Event Collector

1. **Log into Splunk Web** (typically `http://localhost:8000`)

2. **Navigate to HEC Settings**:
   - Click **Settings** → **Data inputs** → **HTTP Event Collector**

3. **Enable HEC**:
   - Click **Global Settings**
   - Check **All Tokens** to enable HEC
   - Set **HTTP Port Number** (default: 8088)
   - For testing, uncheck **Enable SSL** (not recommended for production)
   - Click **Save**

### Step 2: Create a New HEC Token

1. **Add New Token**:
   - Click **New Token**
   - Enter a name: `windows_event_logs`
   - Click **Next**

2. **Configure Input Settings**:
   - **Source type**: `json` or `_json`
   - **Index**: `main` (or create a dedicated index like `windows_events`)
   - Click **Review**

3. **Review and Submit**:
   - Review settings
   - Click **Submit**
   - **Copy the token value** (you'll need this)

### Step 3: Configure HEC to Listen on Link-Local Address

Edit the Splunk inputs.conf file to bind HEC to your link-local address.

**Location**:
```
$SPLUNK_HOME/etc/apps/splunk_httpinput/local/inputs.conf
```

**Configuration for IPv4 Link-Local**:
```ini
[http]
disabled = 0
port = 8088
enableSSL = 0
# Bind to specific link-local address
host = 169.254.1.1

[http://windows_event_logs]
disabled = 0
token = <your-token-here>
indexes = main
sourcetype = json
```

**Configuration for IPv6 Link-Local**:
```ini
[http]
disabled = 0
port = 8088
enableSSL = 0
# IPv6 link-local (note the brackets)
host = [fe80::1]

[http://windows_event_logs]
disabled = 0
token = <your-token-here>
indexes = main
sourcetype = json
```

### Step 4: Restart Splunk

**Windows**:
```cmd
cd C:\Program Files\Splunk\bin
splunk restart
```

**Linux/Mac**:
```bash
$SPLUNK_HOME/bin/splunk restart
```

### Step 5: Verify HEC is Listening

**Check with netstat (Windows)**:
```cmd
# Check if port 8088 is listening
netstat -an | findstr "8088"

# Should show something like:
# TCP    169.254.1.1:8088       0.0.0.0:0              LISTENING
```

**Test with curl**:
```bash
curl -k http://169.254.1.1:8088/services/collector/event \
  -H "Authorization: Splunk <your-token>" \
  -d '{"event": "test message", "source": "test"}'
```

## Configuring the Log Forwarder

### Method 1: Update Test Configuration

Edit `tst/test_log_forwarder.cpp`:

```cpp
// At the top of the file, modify these constants:
const std::string SPLUNK_SERVER = "169.254.1.1";  // Your link-local address
const int SPLUNK_PORT = 8088;                      // Splunk HEC port
```

### Method 2: Command-Line Arguments

Run the log forwarder with your Splunk link-local address:

```cmd
# Basic usage
log_forwarder.exe 169.254.1.1 8088

# With realtime monitoring
log_forwarder.exe 169.254.1.1 8088 realtime

# Forward last 24 hours of logs
log_forwarder.exe 169.254.1.1 8088 recent 24

# Forward all historical logs
log_forwarder.exe 169.254.1.1 8088 all
```

## Testing the Connection

### Step 1: Build and Run the Tests

```cmd
# Build the test
cd tst
build_log_forwarder_test.bat

# Run the test
cd ..\bin
test_log_forwarder.exe
```

### Step 2: Verify in Splunk

1. **Open Splunk Web**: `http://localhost:8000`

2. **Search for Events**:
   ```spl
   index=main sourcetype=json
   | head 10
   ```

3. **Search for Specific Event IDs**:
   ```spl
   index=main sourcetype=json EventID=4624
   | stats count by EventID, Level
   ```

4. **View Recent Test Events**:
   ```spl
   index=main sourcetype=json
   | search Computer="TEST-MACHINE"
   | table _time EventID Level Message
   ```

## Troubleshooting

### Connection Refused

**Problem**: Cannot connect to Splunk HEC

**Solutions**:
1. Verify Splunk is running: `netstat -an | findstr "8088"`
2. Check firewall rules: Allow inbound TCP on port 8088
3. Verify HEC is enabled in Splunk Web
4. Check if Splunk is bound to correct address

**Windows Firewall Rule**:
```cmd
netsh advfirewall firewall add rule name="Splunk HEC" dir=in action=allow protocol=TCP localport=8088
```

### HEC Not Listening on Link-Local Address

**Problem**: Splunk HEC not accessible on link-local address

**Solutions**:
1. Verify `inputs.conf` has correct `host` setting
2. Check Splunk logs: `$SPLUNK_HOME/var/log/splunk/splunkd.log`
3. Try binding to `0.0.0.0` (all interfaces) for testing:
   ```ini
   [http]
   host = 0.0.0.0
   port = 8088
   ```

### Events Not Appearing in Splunk

**Problem**: Connection succeeds but no events in Splunk

**Solutions**:
1. Verify token is correct
2. Check index permissions
3. Review HEC logs in Splunk Web:
   - **Settings** → **HTTP Event Collector** → **View Logs**
4. Verify JSON format is valid
5. Check time range in Splunk search (events might be in the past)

### SSL Certificate Errors

**Problem**: SSL handshake failures

**Solutions**:
1. For testing, disable SSL in Splunk HEC:
   ```ini
   [http]
   enableSSL = 0
   ```
2. For production, use proper SSL certificates
3. Update log forwarder to support HTTPS (requires SSL library)

## Advanced Configuration

### Creating a Dedicated Index

1. **Create Index**:
   - **Settings** → **Indexes** → **New Index**
   - Name: `windows_events`
   - Set retention policy as needed

2. **Update HEC Token**:
   - Edit token to use `windows_events` index
   - Update `inputs.conf` if needed

### High Availability Setup

For production environments, consider:

1. **Multiple Splunk Indexers**: Load balance across indexers
2. **HEC Load Balancer**: Use a load balancer in front of HEC endpoints
3. **Persistent Queues**: Configure forwarder to queue events during outages
4. **Monitoring**: Set up alerts for connection failures

### Security Best Practices

1. **Enable SSL/TLS**: Use HTTPS for production
2. **Token Rotation**: Regularly rotate HEC tokens
3. **Network Segmentation**: Use firewalls to restrict access
4. **Authentication**: Use Splunk's authentication mechanisms
5. **Encryption**: Encrypt data in transit and at rest

## Performance Tuning

### Splunk HEC Settings

```ini
[http]
disabled = 0
port = 8088
maxSockets = 1024
maxThreads = 512
```

### Forwarder Optimization

- **Batch Size**: Send multiple events per connection
- **Connection Pooling**: Reuse TCP connections
- **Compression**: Enable compression for large payloads
- **Buffer Size**: Adjust socket buffer sizes

## Example Searches

### Security Event Analysis

```spl
index=windows_events sourcetype=json Channel=Security
| stats count by EventID
| sort -count
| head 20
```

### Failed Login Attempts

```spl
index=windows_events EventID=4625
| stats count by Computer, IpAddress
| sort -count
```

### System Uptime Monitoring

```spl
index=windows_events EventID=6005 OR EventID=6006
| stats count by EventID, Computer
```

## References

- [Splunk HTTP Event Collector Documentation](https://docs.splunk.com/Documentation/Splunk/latest/Data/UsetheHTTPEventCollector)
- [Splunk inputs.conf Reference](https://docs.splunk.com/Documentation/Splunk/latest/Admin/Inputsconf)
- [RFC 3927 - Dynamic Configuration of IPv4 Link-Local Addresses](https://tools.ietf.org/html/rfc3927)
- [RFC 4862 - IPv6 Stateless Address Autoconfiguration](https://tools.ietf.org/html/rfc4862)

## Support

For issues or questions:
- Check Splunk logs: `$SPLUNK_HOME/var/log/splunk/`
- Review forwarder logs: `forwarder_logs.csv`
- Test connectivity: `test_log_forwarder.exe`
- Verify firewall rules
- Consult Splunk documentation

---

**Note**: Link-local addresses are for local network communication only. For cross-network forwarding, use routable IP addresses and consider VPN or secure tunneling solutions.
