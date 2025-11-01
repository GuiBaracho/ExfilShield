#include "ExfilShield.h"
#include "Logger.h"
#include "EventLog.h"

#include <dbt.h>
#include <initguid.h>
#include <usbiodef.h>
#include <hidclass.h>
#include <ntddndis.h>
#include <ShlObj.h>
#include <KnownFolders.h>
#include <iostream>
#include <fstream>
#include <iomanip>

// --- Global Variables ---
SERVICE_STATUS g_ServiceStatus = {};                //NOSONAR: Can't be const due to Windows API
SERVICE_STATUS_HANDLE g_StatusHandle = nullptr;     //NOSONAR: Can't be const due to Windows API
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;   //NOSONAR: Can't be const due to Windows API
std::vector<HDEVNOTIFY> g_DeviceNotifyHandles;      //NOSONAR: Can't be const due to Windows API

// --- Device Interface GUIDs ---
const std::array<GUID, 5>& GetInterfaceGuids() noexcept {
    static const std::array<GUID, 5> guids = {  // NOSONAR: local static is intentional to ensure safe initialization
        GUID_DEVINTERFACE_USB_DEVICE,   // USB devices
        GUID_DEVINTERFACE_DISK,         // Disk drives
        GUID_DEVINTERFACE_VOLUME,       // Volumes
        GUID_DEVINTERFACE_HID,          // Human Interface Devices
        GUID_DEVINTERFACE_COMPORT       // COM ports
    };
    return guids;
}

// --- Helper: ProgramData Path ---
std::filesystem::path ProgramDataPath() {
    PWSTR rawPath = nullptr;
    std::filesystem::path result = LR"(C:\ProgramData)";

    if (SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &rawPath) == S_OK && rawPath) {
        result = rawPath;
        CoTaskMemFree(rawPath);
    }

    return result / L"ExfilShield" / L"Logs";
}

// --- Helper: Extract VID/PID from device path ---
std::wstring ExtractVidPid(const std::wstring& path) {
    std::wstring info;
    const auto vidPos = path.find(L"VID_");
    if (vidPos != std::wstring::npos && path.size() >= vidPos + 8)
        info += L"VID=" + path.substr(vidPos, 8);

    const auto pidPos = path.find(L"PID_");
    if (pidPos != std::wstring::npos && path.size() >= pidPos + 8) {
        if (!info.empty()) info += L" ";
        info += L"PID=" + path.substr(pidPos + 4, 4);
    }

    return info;
}

// --- Worker Thread ---
DWORD WINAPI ServiceWorkerThread(LPVOID) {
    Logger::Instance().Info(L"Service worker thread started.");
    while (WaitForSingleObject(g_ServiceStopEvent, EVT_SERVICE_STARTED) != WAIT_OBJECT_0) {
        // Optional: background monitoring work here
    }
    Logger::Instance().Info(L"Service worker thread stopping...");
    return ERROR_SUCCESS;
}

// --- Service Control Handler ---
DWORD WINAPI ServiceCtrlHandler(DWORD CtrlCode, DWORD EventType, LPVOID EventData, LPVOID) {
    switch (CtrlCode) {
    case SERVICE_CONTROL_STOP:
        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            break;
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        SetEvent(g_ServiceStopEvent);
        break;

    case SERVICE_CONTROL_DEVICEEVENT: {
        const auto* hdr = static_cast<const DEV_BROADCAST_HDR*>(EventData);
        if (!hdr) break;

        if (hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
            const auto* devInfo = static_cast<const DEV_BROADCAST_DEVICEINTERFACE*>(EventData);
            std::wstring deviceName(devInfo->dbcc_name ? devInfo->dbcc_name : L"(unknown)");

            switch (EventType) {
            case DBT_DEVICEARRIVAL: {
                const std::wstring msg = L"USB Device Connected: " + deviceName;
                Logger::Instance().Info(msg);
                EventWriter::Instance().Info(EVT_DEVICE_CONNECTED, msg);

                const auto vidpid = ExtractVidPid(deviceName);
                if (!vidpid.empty()) {
                    Logger::Instance().Info(L"Device Info: " + vidpid);
                    EventWriter::Instance().Info(EVT_DEVICE_CONNECTED, vidpid);
                }
                break;
            }
            case DBT_DEVICEREMOVECOMPLETE: {
                const std::wstring msg = L"USB Device Disconnected: " + deviceName;
                Logger::Instance().Info(msg);
                EventWriter::Instance().Info(EVT_DEVICE_DISCONNECTED, msg);
                break;
            }
            default:
                break;
            }
        }
        break;
    }
    default:
        break;
    }
    return NO_ERROR;
}

// --- Register for Device Notifications ---
void RegisterForDeviceNotifications(SERVICE_STATUS_HANDLE serviceStatus) {
    for (const auto& guid : GetInterfaceGuids()) {
        DEV_BROADCAST_DEVICEINTERFACE filter = {};
        filter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
        filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        filter.dbcc_classguid = guid;

        if (HDEVNOTIFY hNotify = RegisterDeviceNotification(
            serviceStatus,
            &filter,
            DEVICE_NOTIFY_SERVICE_HANDLE
        )) {
            g_DeviceNotifyHandles.push_back(hNotify);
        }
        else {
            Logger::Instance().Error(L"RegisterDeviceNotification failed.");
            EventWriter::Instance().Error(EVT_ERROR, L"RegisterDeviceNotification failed.");
        }
    }
}

// --- Service Main Entry ---
void WINAPI ServiceMain() {
    Logger::Instance().Init(ProgramDataPath());
    EventWriter::Instance().Init(L"ExfilShield");

    g_StatusHandle = RegisterServiceCtrlHandlerEx(SERVICE_NAME, ServiceCtrlHandler, nullptr);
    if (!g_StatusHandle) return;

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    g_ServiceStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!g_ServiceStopEvent) return;

    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    Logger::Instance().Info(L"Service started successfully.");
    EventWriter::Instance().Info(EVT_SERVICE_STARTED, L"Service started successfully.");

    RegisterForDeviceNotifications(g_StatusHandle);

    HANDLE hThread = CreateThread(nullptr, 0, ServiceWorkerThread, nullptr, 0, nullptr);
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }

    for (auto& handle : g_DeviceNotifyHandles) {
        if (handle) UnregisterDeviceNotification(handle);
    }
    g_DeviceNotifyHandles.clear();

    if (g_ServiceStopEvent) {
        CloseHandle(g_ServiceStopEvent);
        g_ServiceStopEvent = nullptr;
    }

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    Logger::Instance().Info(L"Service stopped cleanly.");
    EventWriter::Instance().Info(EVT_SERVICE_STOPPED, L"Service stopped cleanly.");
}

// --- Entry Point ---
int wmain(int, wchar_t* []) {
    std::array<SERVICE_TABLE_ENTRYW, 2> ServiceTable = {
		SERVICE_TABLE_ENTRYW{ const_cast<LPWSTR>(SERVICE_NAME), (LPSERVICE_MAIN_FUNCTION)ServiceMain }, //NOSONAR: cast is required by Windows API
        SERVICE_TABLE_ENTRYW{ nullptr, nullptr }
    };

    if (!StartServiceCtrlDispatcher(ServiceTable.data())) {
        Logger::Instance().Error(L"Service failed to start.");
        EventWriter::Instance().Error(EVT_ERROR, L"Service failed to start.");
        return static_cast<int>(GetLastError());
    }
    return 0;
}
