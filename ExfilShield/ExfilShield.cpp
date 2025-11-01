// ExfilShield.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>
#include <dbt.h>
#include <string>
#include <initguid.h>
#include <usbiodef.h>
#include <fstream>
#include <array>
#include <iomanip>
#include <hidclass.h>
#include <ntddndis.h>
#include <ShlObj.h>
#include <KnownFolders.h>
#include <vector>

#include "Logger.h"
#include "EventLog.h"

#define UNICODE
#define _UNICODE

#define SERVICE_NAME L"ExfilShieldService"
#define SERVICE_DISPLAY_NAME L"ExfilShield USB Monitoring Service"
#define LOG_FILE_PATH L"C:\\ExfilShield\\exfilshield_log.log"

#define EVT_SERVICE_STARTED 1000
#define EVT_SERVICE_STOPPED 1001
#define EVT_DEVICE_CONNECTED 2000
#define EVT_DEVICE_DISCONNECTED 2001
#define EVT_ERROR 9000

static SERVICE_STATUS g_ServiceStatus = { 0 };		// Current status of the service
SERVICE_STATUS_HANDLE g_StatusHandle = nullptr;		// Connection to the SCM for status updates
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;	// Event handle for stopping the service
static std::vector<HDEVNOTIFY> g_DeviceNotifyHandles;


inline const std::array<GUID, 5> guids = {
    GUID_DEVINTERFACE_USB_DEVICE,   // USB devices
    GUID_DEVINTERFACE_DISK,         // Disk drives
    GUID_DEVINTERFACE_VOLUME,       // Volumes
    GUID_DEVINTERFACE_HID,          // Human Interface Devices
    GUID_DEVINTERFACE_COMPORT		// COM ports
};

const std::array<GUID, 5>& GetInterfaceGuids()
{
    return guids;
}

static std::filesystem::path ProgramDataPath()
{
	PWSTR path = nullptr;
	std::filesystem::path out = L"C:\\ProgramData";

	if (SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &path) == S_OK && path)
	{
		out = path;
		CoTaskMemFree(path);
	}

	return out / L"ExfilShield" / L"Logs";
}

DWORD WINAPI ServiceWorkerThread(LPVOID)
{
	Logger::Instance().Info(L"Service worker thread started.");
	// Main service loop
	while (WaitForSingleObject(g_ServiceStopEvent, 1000) != WAIT_OBJECT_0){
		// Service is running, perform periodic tasks if needed
	}
	Logger::Instance().Info(L"Service worker thread stopping...");
	return ERROR_SUCCESS;
}

std::wstring ExtractVidPid(const std::wstring& path)
{
	std::wstring info;
	size_t vidPos = path.find(L"VID_");
	if (vidPos != std::wstring::npos && path.length() >= vidPos + 8)
	{
		info += L"VID=" + path.substr(vidPos, 8);
	}
	size_t pidPos = path.find(L"PID_");
	if (pidPos != std::wstring::npos && path.length() >= pidPos + 8)
	{
		if (!info.empty()) info += L" ";
		info += L"PID=" + path.substr(pidPos + 4, 4);
	}

	return info;
}

DWORD WINAPI ServiceCtrlHandler(
	DWORD CtrlCode,
	DWORD EventType,
	LPVOID EventData,
	LPVOID 
)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:
	{
		if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
			break;

		// Signal the service to stop
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			Logger::Instance().Error(L"SetServiceStatus failed.");
			EventWriter::Instance().Error(EVT_ERROR, L"SetServiceStatus failed.");
		}

		SetEvent(g_ServiceStopEvent);

		break;
	}

	case SERVICE_CONTROL_DEVICEEVENT:
	{
		auto pHdr = (PDEV_BROADCAST_HDR)EventData;
		if (!pHdr) break;

		if (pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
		{
			auto pDevInf = (PDEV_BROADCAST_DEVICEINTERFACE)pHdr;
			std::wstring deviceName(pDevInf->dbcc_name);
			switch (EventType)
			{
			case DBT_DEVICEARRIVAL: 
			{
				const std::wstring msg = std::wstring(L"USB Device Connected: ") + deviceName;
				Logger::Instance().Info(msg);
				EventWriter::Instance().Info(EVT_DEVICE_CONNECTED, msg.c_str());

				std::wstring deviceInfo = ExtractVidPid(deviceName);
				const std::wstring infoMsg = std::wstring(L"Device Info: ") + deviceInfo;
				Logger::Instance().Info(infoMsg);
				EventWriter::Instance().Info(EVT_DEVICE_CONNECTED, infoMsg.c_str());
				break;
			}

			case DBT_DEVICEREMOVECOMPLETE:
			{
				const std::wstring msg = std::wstring(L"USB Device Disconnected: ") + deviceName;
				Logger::Instance().Info(msg);
				EventWriter::Instance().Info(EVT_DEVICE_DISCONNECTED, msg.c_str());
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

VOID RegisterForDeviceNotifications(SERVICE_STATUS_HANDLE hserviceStatus)
{
	// Register for all device interface notifications in InterfaceGuids
	for (const auto& guid : GetInterfaceGuids())
	{
		DEV_BROADCAST_DEVICEINTERFACE NotificationFilter = { 0 };
		NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
		NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		NotificationFilter.dbcc_classguid = guid;

		HDEVNOTIFY hDevNotify = RegisterDeviceNotification( //NOSONAR: Windows API requires C-style cast
			hserviceStatus,
			&NotificationFilter,
			DEVICE_NOTIFY_SERVICE_HANDLE
		);

		if (hDevNotify)
		{
			g_DeviceNotifyHandles.push_back(hDevNotify);
			Logger::Instance().Info(L"Registered for device notifications for GUID.");
			EventWriter::Instance().Info(EVT_SERVICE_STARTED, L"Registered for device notifications.");
		}
		else
		{
			Logger::Instance().Error(L"RegisterDeviceNotification failed for GUID.");
			EventWriter::Instance().Error(EVT_ERROR, L"RegisterDeviceNotification failed for GUID.");
		}
	}
}

VOID WINAPI ServiceMain()
{
	Logger::Instance().Init(ProgramDataPath());
	EventWriter::Instance().Init(L"ExfilShield");

	Logger::Instance().Info(L"Initializing service...");
	EventWriter::Instance().Info(EVT_SERVICE_STARTED, L"Service start");

	// Register the service control handler
	g_StatusHandle = RegisterServiceCtrlHandlerEx(SERVICE_NAME, ServiceCtrlHandler, nullptr);
	if (g_StatusHandle == nullptr)
	{
		Logger::Instance().Error(L"RegisterServiceCtrlHandler failed.");
		EventWriter::Instance().Error(EVT_ERROR, L"RegisterServiceCtrlHandler failed.");
		return;
	}

	// Initialize service status
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwServiceSpecificExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	// Report initial status to SCM
	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		Logger::Instance().Error(L"SetServiceStatus failed.");
		EventWriter::Instance().Error(EVT_ERROR, L"SetServiceStatus failed.");
		return;
	}

	// Create stop event
	g_ServiceStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (g_ServiceStopEvent == nullptr)
	{
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		g_ServiceStatus.dwWin32ExitCode = GetLastError();
		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			Logger::Instance().Error(L"SetServiceStatus failed.");
			EventWriter::Instance().Error(EVT_ERROR, L"SetServiceStatus failed.");
		}
		Logger::Instance().Error(L"CreateEvent failed.");
		EventWriter::Instance().Error(EVT_ERROR, L"CreateEvent failed.");
		return;
	}

	// Service is running
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		Logger::Instance().Error(L"SetServiceStatus failed.");
		EventWriter::Instance().Error(EVT_ERROR, L"SetServiceStatus failed.");
		return;
	}
	Logger::Instance().Info(L"Service started successfully.");
	EventWriter::Instance().Info(EVT_SERVICE_STARTED, L"Service started successfully.");

	// Register for device notifications
	RegisterForDeviceNotifications(g_StatusHandle);

	// Main service loop
	if (HANDLE hThread = CreateThread(nullptr, 0, ServiceWorkerThread, nullptr, 0, nullptr); hThread == nullptr)
	{
		Logger::Instance().Error(L"CreateThread failed.");
		EventWriter::Instance().Error(EVT_ERROR, L"CreateThread failed.");
		return;
	}
	else {
		WaitForSingleObject(hThread, INFINITE);
	}
	
	// Cleanup and stop service
	for (HDEVNOTIFY hDevNotify : g_DeviceNotifyHandles)
	{
		if (hDevNotify) UnregisterDeviceNotification(hDevNotify);
	}
	g_DeviceNotifyHandles.clear();

	if (g_ServiceStopEvent != nullptr) {
		CloseHandle(g_ServiceStopEvent);
		g_ServiceStopEvent = nullptr;
	}

	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	Logger::Instance().Info(L"Service stopped.");
	EventWriter::Instance().Info(EVT_SERVICE_STOPPED, L"Service stopped.");

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		Logger::Instance().Error(L"SetServiceStatus failed.");
		EventWriter::Instance().Error(EVT_ERROR, L"SetServiceStatus failed.");
		return;
	}
	
}

// Replace the C-style cast with a const_cast to preserve const correctness
int wmain(int argc, wchar_t* argv[])
{
	std::array<SERVICE_TABLE_ENTRY, 2> ServiceTable = {
		SERVICE_TABLE_ENTRY{ const_cast<LPWSTR>(SERVICE_NAME), (LPSERVICE_MAIN_FUNCTION)ServiceMain}, //NOSONAR: cast is necessary for Windows API
		SERVICE_TABLE_ENTRY{ nullptr, nullptr } 
	};

	if (StartServiceCtrlDispatcher(ServiceTable.data()) == FALSE)
	{
		Logger::Instance().Error(L"Service failed to start.");
		EventWriter::Instance().Error(EVT_ERROR, L"Service failed to start.");
		return GetLastError();
	}
	return 0;
}