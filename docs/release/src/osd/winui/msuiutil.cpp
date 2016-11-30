// For licensing and usage information, read docs/winui_license.txt
//****************************************************************************
#define WIN32_LEAN_AND_MEAN
#define _WIN32_IE 0x0501
#include <windows.h>

#include "emu.h"
#include "image.h"
#include "msuiutil.h"
#include "mui_opts.h"
#include "drivenum.h"

BOOL DriverIsComputer(int driver_index)
{
	return (driver_list::driver(driver_index).flags & MACHINE_TYPE_COMPUTER) != 0;
}

BOOL DriverIsConsole(int driver_index)
{
	return (driver_list::driver(driver_index).flags & MACHINE_TYPE_CONSOLE) != 0;
}

BOOL DriverIsModified(int driver_index)
{
	return (driver_list::driver(driver_index).flags & MACHINE_UNOFFICIAL) != 0;
}

BOOL DriverHasDevice(const game_driver *gamedrv, iodevice_t type)
{
	BOOL b = FALSE;
//	const device_image_interface *device;

	// allocate the machine config
	machine_config config(*gamedrv,MameUIGlobal());

	image_interface_iterator iter(config.root_device());
	for (device_image_interface *dev = iter.first(); dev != NULL; dev = iter.next())
//	for (bool gotone = config.devicelist().first(device); gotone; gotone = device->next(device))
	{
		if (dev->image_type() == type)
		{
			b = TRUE;
			break;
		}
	}
	return b;
}


