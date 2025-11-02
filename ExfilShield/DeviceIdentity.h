#pragma once
#include <string>
#include <windows.h>
#include <devpkey.h>
#include <cfgmgr32.h>
#include <SetupAPI.h>
#pragma comment(lib, "setupapi.lib")

struct DeviceIdentity
{
    std::wstring devicePath;
    std::wstring vid;
    std::wstring pid;
    std::wstring serial;
    std::wstring friendlyName;
    std::wstring manufacturer;
    std::wstring className;
    GUID classGuid{};
    std::wstring containerId;
    std::wstring category; // classified type
};

DeviceIdentity BuildDeviceIdentity(const std::wstring& devicePath, const GUID& classGuid);