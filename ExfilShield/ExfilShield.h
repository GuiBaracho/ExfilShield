#pragma once

// --- System Includes (minimal necessary for declarations) ---
#include <Windows.h>
#include <string>
#include <array>
#include <vector>
#include <filesystem>

// --- Constants ---
constexpr auto SERVICE_NAME = L"ExfilShieldService";
constexpr auto SERVICE_DISPLAY_NAME = L"ExfilShield USB Monitoring Service";

constexpr DWORD EVT_SERVICE_STARTED = 1000;
constexpr DWORD EVT_SERVICE_STOPPED = 1001;
constexpr DWORD EVT_DEVICE_CONNECTED = 2000;
constexpr DWORD EVT_DEVICE_DISCONNECTED = 2001;
constexpr DWORD EVT_ERROR = 9000;

enum class DeviceAction {
    Arrival,
    Removal,
    NodeChange,
    Unknown
};

struct DeviceEvent {
    DeviceAction action{};
    std::wstring devicePath;
    GUID classGuid{};
};

// --- Globals ---
extern SERVICE_STATUS g_ServiceStatus;					//NOSONAR: Can't be const due to Windows API
extern SERVICE_STATUS_HANDLE g_StatusHandle;			//NOSONAR: Can't be const due to Windows API
extern HANDLE g_ServiceStopEvent;						//NOSONAR: Can't be const due to Windows API
extern std::vector<HDEVNOTIFY> g_DeviceNotifyHandles;	//NOSONAR: Can't be const due to Windows API
extern HANDLE g_DeviceEventSignal;                      //NOSONAR: Can't be const due to Windows API

// --- Function Declarations ---
[[nodiscard]] const std::array<GUID, 5>& GetInterfaceGuids() noexcept;
[[nodiscard]] std::filesystem::path ProgramDataPath();
[[nodiscard]] DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);
[[nodiscard]] DWORD WINAPI ServiceCtrlHandler(DWORD CtrlCode, DWORD EventType, LPVOID EventData, LPVOID Context);
void WINAPI ServiceMain();
void RegisterForDeviceNotifications(SERVICE_STATUS_HANDLE hserviceStatus);
void EnqueueDeviceEvent(const DeviceEvent& evt);
void SetErrorStatus(const DWORD& err);
std::wstring ExtractVidPid(const std::wstring& path);
