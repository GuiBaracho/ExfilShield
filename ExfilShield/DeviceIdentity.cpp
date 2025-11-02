#include "DeviceIdentity.h"
#include "Logger.h"
#include <regex>
#include <sstream>

static std::wstring ExtractValue(const std::wstring& input, const std::wregex& pattern) {
    if (std::wsmatch match; std::regex_search(input, match, pattern) && match.size() > 1)
        return match[1].str();
    return L"";
}

static std::wstring GetDeviceProperty(HDEVINFO devInfo, SP_DEVINFO_DATA& data, const DEVPROPKEY& key) {
    DEVPROPTYPE type;
    WCHAR buffer[256];
    if (DWORD size = 0; SetupDiGetDevicePropertyW(devInfo, &data, &key, &type,
        (PBYTE)buffer, sizeof(buffer), &size, 0)) {
        return buffer;
    }
    return L"";
}

DeviceIdentity BuildDeviceIdentity(const std::wstring& devicePath, const GUID& classGuid)
{
    DeviceIdentity info{};
    info.devicePath = devicePath;
    info.classGuid = classGuid;

    // Parse VID/PID/Serial from the path
    info.vid = ExtractValue(devicePath, std::wregex(L"VID_([0-9A-Fa-f]{4})"));
    info.pid = ExtractValue(devicePath, std::wregex(L"PID_([0-9A-Fa-f]{4})"));
    info.serial = ExtractValue(devicePath, std::wregex(L"#([0-9A-Za-z]{8,})[{#]")); // after second '#'

    // Get device interface details directly for this specific path
    HDEVINFO devInfo = SetupDiGetClassDevsW(&classGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfo == INVALID_HANDLE_VALUE)
        return info;

    SP_DEVICE_INTERFACE_DATA ifData{};
    ifData.cbSize = sizeof(ifData);

    // Find the interface that matches this device path
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(devInfo, nullptr, &classGuid, i, &ifData); i++)
    {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, nullptr, 0, &requiredSize, nullptr);
        std::vector<BYTE> buffer(requiredSize);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA devData{};
        devData.cbSize = sizeof(devData);

        if (!SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, detail, requiredSize, nullptr, &devData))
            continue;

        if (_wcsicmp(detail->DevicePath, devicePath.c_str()) != 0)
            continue; // not the same instance

        // We found the matching device node, now query its properties
        info.friendlyName = GetDeviceProperty(devInfo, devData, DEVPKEY_Device_FriendlyName);
        info.manufacturer = GetDeviceProperty(devInfo, devData, DEVPKEY_Device_Manufacturer);
        info.className = GetDeviceProperty(devInfo, devData, DEVPKEY_Device_Class);
        info.containerId = GetDeviceProperty(devInfo, devData, DEVPKEY_Device_ContainerId);

        if (info.friendlyName.empty())
            info.friendlyName = GetDeviceProperty(devInfo, devData, DEVPKEY_Device_DeviceDesc);

        break;
    }

    SetupDiDestroyDeviceInfoList(devInfo);

    // --- Classification ---
    if (const std::wstring& cls = info.className; cls.find(L"USB") != std::wstring::npos)
		info.category = L"USB";
	else if (cls.find(L"Disk") != std::wstring::npos)
		info.category = L"Disk";
    else if(cls.find(L"Volume") != std::wstring::npos)
        info.category = L"Volume";
    else if (cls.find(L"HID") != std::wstring::npos)
        info.category = L"HID";
    else if (cls.find(L"Net") != std::wstring::npos)
        info.category = L"Network";
    else if (cls.find(L"COM") != std::wstring::npos)
        info.category = L"Serial";
    else if (cls.find(L"WPD") != std::wstring::npos)
        info.category = L"MTP/PTP";
    else
		info.category = cls.empty() ? L"Unknown" : cls;

    return info;
}

