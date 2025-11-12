#include "DeviceControl.h"

#include <SetupAPI.h>    // optional but safe; SetupDi* definitions
#include <initguid.h>    // ensure GUIDs are fully defined if needed
#include <string>
#pragma comment(lib, "Cfgmgr32.lib")

// --- Helper: get the ContainerId of a devnode ---
static bool GetNodeContainerId(DEVINST dn, GUID& out)
{
    DEVPROPTYPE propType = 0;
    ULONG cb = sizeof(GUID);
    CONFIGRET cr = CM_Get_DevNode_PropertyW(
        dn,
        &DEVPKEY_Device_ContainerId,
        &propType,
        reinterpret_cast<PBYTE>(&out),
        &cb,
        0
    );

    return (cr == CR_SUCCESS && propType == DEVPROP_TYPE_GUID);
}

// --- Helper: Find the root devnode (physical device) for a given container
DEVINST FindContainerRoot(DEVINST dn, const GUID& containerId)
{
    DEVINST current = dn;
    DEVINST parent = 0;
    GUID parentCid{};

    while (CM_Get_Parent(&parent, current, 0) == CR_SUCCESS)
    {
        if (!GetNodeContainerId(parent, parentCid) || parentCid != containerId)
            break;

        current = parent;
    }

    return current; // topmost node within same container
}

// --- Disable a devnode (and optionally persist the disable state across reboots)
CONFIGRET DisableDevnode(DEVINST dn, bool persist)
{
    ULONG flags = persist ? CM_DISABLE_PERSIST : 0;
    CONFIGRET cr = CM_Disable_DevNode(dn, flags);

    if (cr != CR_SUCCESS)
    {
        // Some devices (e.g., system devices) cannot be disabled; ignore those
        // Typical failures: CR_ACCESS, CR_NOT_DISABLEABLE
    }

    return cr;
}

// --- Enable a devnode and re-enumerate it so child interfaces return
CONFIGRET EnableDevnode(DEVINST dn)
{
    CONFIGRET cr = CM_Enable_DevNode(dn, 0);

    if (cr == CR_SUCCESS)
    {
        // Force re-enumeration so that all child interfaces show up again
        CM_Reenumerate_DevNode(dn, 0);
    }

    return cr;
}
