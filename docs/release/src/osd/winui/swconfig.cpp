// For licensing and usage information, read docs/winui_license.txt
//****************************************************************************
//============================================================
//
//  swconfig.c
//
//============================================================
// standard windows headers
#include <windows.h>

// standard C headers
#include <tchar.h>
#include "swconfig.h"
#include "mui_opts.h"
#include "drivenum.h"

//============================================================
//  IMPLEMENTATION
//============================================================

software_config *software_config_alloc(int driver_index) //, hashfile_error_func error_proc)
{
	software_config *config;

	// allocate the software_config
	config = (software_config *)malloc(sizeof(software_config));
	memset(config,0,sizeof(software_config));

	// allocate the machine config
	windows_options o;
	const char* name = driver_list::driver(driver_index).name;
	o.set_value(OPTION_SYSTEMNAME, name, OPTION_PRIORITY_CMDLINE);
	config->mconfig = global_alloc(machine_config(driver_list::driver(driver_index), o));

	// other stuff
	config->driver_index = driver_index;
	config->gamedrv = &driver_list::driver(driver_index);

	return config;
}



void software_config_free(software_config *config)
{
	if (config->mconfig)
	{
		global_free(config->mconfig);
		config->mconfig = NULL;
	}

	/*if (config->hashfile != NULL)
	{
		hashfile_close(config->hashfile);
		config->hashfile = NULL;
	}*/

	free(config);
}
