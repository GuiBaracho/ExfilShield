# Configuration Guide

ExfilShield uses a JSON configuration file to define device control policies.

## File Location

The configuration file must be placed at:
```
C:\ProgramData\ExfilShield\config.json
```

## Configuration Structure

### Actions

```json
"actions": {
  "default": "block",
  "blacklist": "block"
}
```

| Field | Description | Values |
|-------|-------------|--------|
| `default` | Action for devices not matching any rule | `allow`, `block`, `audit` |
| `blacklist` | Action for blacklisted devices | `allow`, `block`, `audit` |

### Policy Actions

| Action | Description |
|--------|-------------|
| `allow` | Device is permitted to function normally |
| `block` | Device is disabled at the OS level |
| `audit` | Device is allowed but logged for review |

### Whitelist

Devices in the whitelist are always allowed, regardless of the default action.

```json
"whitelist": [
  { "vid": "046D", "pid": "C534", "serial": "" }
]
```

### Blacklist

Devices in the blacklist are blocked (or audited based on `actions.blacklist`).

```json
"blacklist": [
  { "vid": "0781", "pid": "", "serial": "" }
]
```

## Device Matching

Each entry can specify:

| Field | Description | Example |
|-------|-------------|---------|
| `vid` | Vendor ID (4-digit hex) | `"046D"` (Logitech) |
| `pid` | Product ID (4-digit hex) | `"C534"` |
| `serial` | Serial number | `"ABC123"` |

**Matching Rules:**
- Empty string (`""`) acts as a wildcard (matches any value)
- All specified fields must match for a rule to apply
- Matching is case-insensitive
- Partial serial matching is supported

## Examples

### Allow only specific devices (strict mode)

```json
{
  "actions": {
    "default": "block",
    "blacklist": "block"
  },
  "whitelist": [
    { "vid": "046D", "pid": "C534", "serial": "" },
    { "vid": "8087", "pid": "", "serial": "" }
  ],
  "blacklist": []
}
```

### Block specific vendors

```json
{
  "actions": {
    "default": "allow",
    "blacklist": "block"
  },
  "whitelist": [],
  "blacklist": [
    { "vid": "0781", "pid": "", "serial": "" },
    { "vid": "0951", "pid": "", "serial": "" }
  ]
}
```

### Audit mode (monitor only)

```json
{
  "actions": {
    "default": "audit",
    "blacklist": "audit"
  },
  "whitelist": [],
  "blacklist": []
}
```

## Finding Device IDs

To find the VID and PID of a device:

1. Open Device Manager
2. Right-click the device > Properties
3. Go to Details tab
4. Select "Hardware Ids" from the dropdown
5. Look for `VID_XXXX&PID_XXXX` pattern

Alternatively, use PowerShell:
```powershell
Get-PnpDevice | Where-Object { $_.Class -eq 'USB' } | Select-Object FriendlyName, InstanceId
```
