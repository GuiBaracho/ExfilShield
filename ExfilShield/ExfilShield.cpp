#include "ExfilShield.h"
#include "Logger.h"
#include "EventLog.h"
#include "DeviceIdentity.h"

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
#include <queue>
#include <mutex>

// --- Global Variables ---
SERVICE_STATUS g_ServiceStatus = {};                //NOSONAR: Can't be const due to Windows API
SERVICE_STATUS_HANDLE g_StatusHandle = nullptr;     //NOSONAR: Can't be const due to Windows API

HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;   //NOSONAR: Can't be const due to Windows API
HANDLE g_DeviceEventSignal = nullptr;               //NOSONAR: Can't be const due to Windows API

std::vector<HDEVNOTIFY> g_DeviceNotifyHandles;      //NOSONAR: Can't be const due to Windows API

static std::queue<DeviceEvent> g_DeviceQueue;       //NOSONAR: Can't be const due to Windows API
static std::mutex g_DeviceQueueMutex;               //NOSONAR: Can't be const due to Windows API

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

// --- Process Device Events ---
void ProcessDeviceEvents() {
    while (true) {
        DeviceEvent evt{};
        {
            std::scoped_lock lk(g_DeviceQueueMutex);
            if (g_DeviceQueue.empty()) break;
            evt = std::move(g_DeviceQueue.front());
            g_DeviceQueue.pop();
        }

        // === Enriched device processing ===
        DeviceIdentity ident = BuildDeviceIdentity(evt.devicePath, evt.classGuid);
        std::wstring ss;

        switch (evt.action)
        {
		using enum DeviceAction;
		case Arrival:
            ss = L"Device Arrival: " + IdentToWString(ident);
            Logger::Instance().Info(ss);
            EventWriter::Instance().Info(2100, ss);
            break;
		case NodeChange:
            ss = L"Device Node Change: " + IdentToWString(ident);
            Logger::Instance().Info(ss);
            EventWriter::Instance().Info(2100, ss);
            break;
		case Removal:
            ss = L"Device Removal: " + evt.devicePath;
            Logger::Instance().Info(ss);
            EventWriter::Instance().Info(2101, ss);
            break;
        case Unknown:
            Logger::Instance().Warn(L"Received unknown device event.");
            EventWriter::Instance().Warn(EVT_ERROR, L"Received unknown device event.");
            break;
        default:
            break;
        }
        
    }
}

// --- Worker Thread ---
DWORD WINAPI ServiceWorkerThread(LPVOID)
{
    Logger::Instance().Info(L"Service worker thread started.");

	std::array<HANDLE, 2> waits = { g_ServiceStopEvent, g_DeviceEventSignal };

    bool running = true;
    while (running)
    {
        DWORD wait = WaitForMultipleObjects(2, waits.data(), FALSE, INFINITE);
        switch (wait)
        {
        case WAIT_OBJECT_0: // g_ServiceStopEvent
            running = false;
            break;

        case WAIT_OBJECT_0 + 1: // g_DeviceEventSignal
        {
			ProcessDeviceEvents();
            break;
        }

        default:
            // Shouldn't happen; log and continue
            Logger::Instance().Warn(L"Unexpected WaitForMultipleObjects result in worker.");
            break;
        }
    }

    Logger::Instance().Info(L"Service worker thread stopping...");
    return ERROR_SUCCESS;
}


// --- Service Control Handler ---
DWORD WINAPI ServiceCtrlHandler(DWORD CtrlCode, DWORD EventType, LPVOID EventData, LPVOID)
{
    switch (CtrlCode)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        if (g_ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
            g_ServiceStatus.dwControlsAccepted = 0;
            g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
            SetEvent(g_ServiceStopEvent);
        }
        break;

    case SERVICE_CONTROL_DEVICEEVENT:
    {
        const auto* hdr = static_cast<const DEV_BROADCAST_HDR*>(EventData);
        if (!hdr || hdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE) break;

        const auto* devInfo = static_cast<const DEV_BROADCAST_DEVICEINTERFACE*>(EventData);
        DeviceEvent evt{};
        evt.devicePath = devInfo->dbcc_name ? devInfo->dbcc_name : L"(unknown)";
        evt.classGuid = devInfo->dbcc_classguid;

        switch (EventType) {
        using enum DeviceAction;
        case DBT_DEVICEARRIVAL:         evt.action = Arrival;   break;
        case DBT_DEVICEREMOVECOMPLETE:  evt.action = Removal;   break;
        case DBT_DEVNODES_CHANGED:      evt.action = NodeChange; break;
        default:                        evt.action = Unknown;   break;
        }

        EnqueueDeviceEvent(evt);
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

void EnqueueDeviceEvent(const DeviceEvent& evt)
{
    {
        std::scoped_lock lk(g_DeviceQueueMutex);
        g_DeviceQueue.push(evt);
    }
    if (g_DeviceEventSignal) {
        SetEvent(g_DeviceEventSignal); // wake worker immediately
    }
}

void SetErrorStatus(const DWORD& err)
{
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = err;
	SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
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

    // Create the service stop event
    g_ServiceStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!g_ServiceStopEvent) {
        DWORD err = GetLastError();
        Logger::Instance().Error(L"CreateEvent(ServiceStopEvent) failed: " + Logger::LastErrorMessage(err));
		SetErrorStatus(err);
		return;
    }

    // create the device event signal (auto-reset, unsignaled)
    g_DeviceEventSignal = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_DeviceEventSignal) {
        DWORD err = GetLastError();
        Logger::Instance().Error(L"CreateEvent(DeviceEventSignal) failed: " + Logger::LastErrorMessage(err));
        SetErrorStatus(err);
        return;
    }

	// Service is running
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

    // === Cleanup ===
    for (auto& handle : g_DeviceNotifyHandles) {
        if (handle) UnregisterDeviceNotification(handle);
    }
    g_DeviceNotifyHandles.clear();

    if (g_DeviceEventSignal) {
        CloseHandle(g_DeviceEventSignal);
        g_DeviceEventSignal = nullptr;
    }

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
