#pragma once
#include <Windows.h>
#include <Cfgmgr32.h>
#include <Devpkey.h>
#include <string>


DEVINST FindContainerRoot(DEVINST dn, const GUID& containerId);
CONFIGRET DisableDevnode(DEVINST dn, bool persist);
CONFIGRET EnableDevnode(DEVINST dn);
