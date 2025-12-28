# ExfilShield

A Windows service for monitoring and controlling external device connections to prevent unauthorized data exfiltration.

ExfilShield acts as an endpoint-level "data egress firewall" - lightweight, reliable, and silent - designed for system-level integration. This project was created to gain hands-on experience with C++ Windows endpoint security development, service architecture, and low-level device management APIs.

## Features

- **Real-time Device Monitoring** - Hooks into Windows device notification system to detect hardware arrivals and removals instantly
- **Policy-based Control** - Whitelist, blacklist, or audit devices based on Vendor ID, Product ID, and serial number
- **Multiple Device Classes** - Monitors USB devices, disk drives, volumes, HID devices, and COM ports
- **Container-aware Grouping** - Correctly handles multi-function USB devices as a single entity
- **Dual Logging** - Writes to daily-rotating log files and Windows Event Viewer
- **Non-blocking Architecture** - Event-driven worker thread processes device events without polling

## Architecture

```
+-----------------------------------------------------------------------+
|                         Windows Kernel                                 |
|     Device Connect/Disconnect --> RegisterDeviceNotification          |
+----------------------------------+------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                      ServiceCtrlHandler                                |
|              Receives SERVICE_CONTROL_DEVICEEVENT                     |
+----------------------------------+------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                   Event Queue (Thread-safe)                           |
|             EnqueueDeviceEvent() --> DeviceEventSignal                |
+----------------------------------+------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                        Worker Thread                                   |
|                    ProcessDeviceEvents()                              |
|                                                                        |
|   +-----------------+   +-----------------+   +------------------+     |
|   | DeviceIdentity  |-->| PolicyManager   |-->| DeviceControl    |     |
|   | Build profile   |   | Evaluate policy |   | Enable/Disable   |     |
|   +-----------------+   +-----------------+   +------------------+     |
+----------------------------------+------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                          Logging                                       |
|            Logger (File) + EventWriter (Event Viewer)                 |
+-----------------------------------------------------------------------+
```

## Getting Started

### Prerequisites

- Windows 10 or Windows 11
- Visual Studio 2022 (v143 toolset)
- Windows SDK 10.0 or later
- C++20 support enabled

### Dependencies

- [nlohmann/json](https://github.com/nlohmann/json) - JSON parsing library (included in `vendor/`)

### Building

1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/ExfilShield.git
   cd ExfilShield
   ```

2. Download the nlohmann/json header and place it at:
   ```
   vendor/include/nlohmann/json.hpp
   ```

   You can download it from the [nlohmann/json releases](https://github.com/nlohmann/json/releases) (single include version).

3. Open `ExfilShield.sln` in Visual Studio 2022

4. Select configuration (Debug/Release) and platform (x86/x64)

5. Build the solution (Ctrl+Shift+B)

### Installation

1. Build the project in Release mode (x64 recommended)

2. Run the installation script as Administrator:
   ```batch
   scripts\install.bat
   ```

   This will:
   - Create the configuration directory at `C:\ProgramData\ExfilShield\`
   - Copy the example configuration file
   - Register and start the Windows service

**Manual Installation:**

```batch
:: Create config directory
mkdir "C:\ProgramData\ExfilShield"

:: Copy configuration
copy examples\config.example.json "C:\ProgramData\ExfilShield\config.json"

:: Install service (run as Administrator)
sc create ExfilShield binPath= "C:\Path\To\ExfilShield.exe" start= auto
sc description ExfilShield "Device monitoring service for data exfiltration prevention"
sc start ExfilShield
```

## Configuration

### Config File Location

```
C:\ProgramData\ExfilShield\config.json
```

### Policy Structure

```json
{
  "actions": {
    "default": "block",
    "blacklist": "block"
  },
  "whitelist": [
    { "vid": "046D", "pid": "C534", "serial": "" }
  ],
  "blacklist": [
    { "vid": "0781", "pid": "", "serial": "" }
  ]
}
```

| Section | Description |
|---------|-------------|
| `actions.default` | Action for devices not matching any rule: `allow`, `block`, or `audit` |
| `actions.blacklist` | Action for blacklisted devices |
| `whitelist` | Devices always permitted regardless of default action |
| `blacklist` | Devices blocked or audited based on `actions.blacklist` |

### Device Matching

| Field | Description | Example |
|-------|-------------|---------|
| `vid` | Vendor ID (4-digit hex) | `"046D"` (Logitech) |
| `pid` | Product ID (4-digit hex) | `"C534"` |
| `serial` | Serial number (partial match supported) | `"ABC123"` |

Empty string (`""`) acts as a wildcard. See [examples/README.md](examples/README.md) for detailed configuration examples.

## Usage

### Service Commands

```batch
:: Start the service
sc start ExfilShield

:: Stop the service
sc stop ExfilShield

:: Check service status
sc query ExfilShield

:: View service configuration
sc qc ExfilShield
```

### Logs

**File Logs:**
```
C:\ProgramData\ExfilShield\Logs\exfilshield_YYYYMMDD.log
```

Log format:
```
[2024/01/15 14:30:45.123] [INFO] Device connected: Kingston DataTraveler (VID:0951, PID:1666) - Action: BLOCK
```

**Windows Event Viewer:**
- Open Event Viewer
- Navigate to: Windows Logs > Application
- Filter by Source: "ExfilShield"

Event IDs:
| ID | Description |
|----|-------------|
| 1000 | Service started |
| 1001 | Service stopped |
| 2100 | Device connected (includes action taken) |
| 2101 | Device disconnected |
| 9000 | Error occurred |

### Uninstallation

Run the uninstallation script as Administrator:
```batch
scripts\uninstall.bat
```

Or manually:
```batch
sc stop ExfilShield
sc delete ExfilShield
```

## Project Structure

```
ExfilShield/
├── ExfilShield/
│   ├── ExfilShield.cpp/.h      # Service main, event handling, notifications
│   ├── DeviceIdentity.cpp/.h   # Device detection and classification
│   ├── DeviceControl.cpp/.h    # Device enable/disable operations
│   ├── PolicyManager.cpp/.h    # Policy evaluation engine
│   ├── Logger.cpp/.h           # File-based logging with rotation
│   └── EventLog.cpp/.h         # Windows Event Viewer integration
├── examples/
│   ├── config.example.json     # Example configuration
│   └── README.md               # Configuration documentation
├── scripts/
│   ├── install.bat             # Service installation script
│   └── uninstall.bat           # Service removal script
├── vendor/
│   └── include/nlohmann/       # Third-party headers (json.hpp)
├── ExfilShield.sln             # Visual Studio solution
└── README.md                   # This file
```

## Technical Details

### Supported Device Classes

| Device Class | GUID |
|--------------|------|
| USB Devices | `GUID_DEVINTERFACE_USB_DEVICE` |
| Disk Drives | `GUID_DEVINTERFACE_DISK` |
| Volumes | `GUID_DEVINTERFACE_VOLUME` |
| HID Devices | `GUID_DEVINTERFACE_HID` |
| COM Ports | `GUID_DEVINTERFACE_COMPORT` |

### Device Categories

ExfilShield classifies devices into these categories based on their interface class:

- USB - Generic USB devices
- Disk - Mass storage devices
- Volume - Mounted volumes/partitions
- HID - Human Interface Devices (keyboards, mice)
- Network - Network adapters
- Serial - Serial/COM port devices
- MTP/PTP - Media Transfer Protocol devices
- Unknown - Unclassified devices

### How Device Control Works

1. Device connects to the system
2. Windows sends `DBT_DEVICEARRIVAL` notification
3. ExfilShield extracts VID, PID, and serial from the device path
4. PolicyManager evaluates the device against whitelist/blacklist
5. If blocked, `CM_Disable_DevNode` is called on the container root device
6. Event is logged to file and Event Viewer

### Container-aware Blocking

USB devices often expose multiple interfaces (e.g., a USB drive may appear as both a disk and a volume). ExfilShield uses Windows Container IDs to identify the physical device and applies policy decisions at the container level, ensuring consistent behavior across all interfaces.

## Roadmap

- [ ] Remote policy synchronization
- [ ] Real-time policy reload without service restart
- [ ] Web-based management console
- [ ] Device connection history and analytics
- [ ] Email/webhook notifications for blocked devices
- [ ] Integration with SIEM systems

## Acknowledgments

- [nlohmann/json](https://github.com/nlohmann/json) - JSON for Modern C++
