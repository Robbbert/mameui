// For licensing and usage information, read docs/winui_license.txt
//****************************************************************************
//============================================================
//
//  messui.c - MESS extensions to src/osd/winui/winui.c
//
//============================================================

// standard windows headers
#include <assert.h>
#include <string.h>
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wingdi.h>
#include <winuser.h>
#include <tchar.h>

// MAME/MAMEOS/MAMEUI/MESS headers
#include "screenshot.h"
#include "bitmask.h"
#include "winui.h"
#include "resource.h"
#include "mui_opts.h"
#include "picker.h"
#include "tabview.h"
#include "mui_util.h"
#include "columnedit.h"
#include "softwarepicker.h"
#include "softwarelist.h"
#include "softlist_dev.h"
#include "devview.h"
#include "windows/window.h"
#include "strconv.h"
#include "messui.h"
#include "winutf8.h"
#include "swconfig.h"
#include "zippath.h"
#include "emuopts.h"
#include "drivenum.h"
#include "xmlfile.h"
#include "softlist.h"


//============================================================
//  PARAMETERS
//============================================================

#define LOG_SOFTWARE 0


//============================================================
//  TYPEDEFS
//============================================================

typedef struct _mess_image_type mess_image_type;
struct _mess_image_type
{
	const device_image_interface *dev;
	char *ext;
	const char *dlgname;
};


typedef struct _device_entry device_entry;
struct _device_entry
{
	iodevice_t dev_type;
	const char *icon_name;
	const char *dlgname;
};



//============================================================
//  GLOBAL VARIABLES
//============================================================

char g_szSelectedItem[MAX_PATH];

char g_szSelectedSoftware[MAX_PATH];

char g_szSelectedDevice[26];

//============================================================
//  LOCAL VARIABLES
//============================================================

static software_config *s_config;
static BOOL s_bIgnoreSoftwarePickerNotifies = 0;

static int *mess_icon_index;

// TODO - We need to make icons for Cylinders, Punch Cards, and Punch Tape!
static const device_entry s_devices[] =
{
	{ IO_CARTSLOT,	"roms",		"Cartridge images" },
	{ IO_FLOPPY,	"floppy",	"Floppy disk images" },
	{ IO_HARDDISK,	"hard",		"Hard disk images" },
	{ IO_CYLINDER,	NULL,		"Cylinders" },
	{ IO_CASSETTE,	NULL,		"Cassette images" },
	{ IO_PUNCHCARD,	NULL,		"Punchcard images" },
	{ IO_PUNCHTAPE,	NULL,		"Punchtape images" },
	{ IO_PRINTER,	NULL,		"Printer Output" },
	{ IO_SERIAL,	NULL,		"Serial Output" },
	{ IO_PARALLEL,	NULL,		"Parallel Output" },
	{ IO_SNAPSHOT,	"snapshot",	"Snapshots" },
	{ IO_QUICKLOAD,	"snapshot",	"Quickloads" },
	{ IO_MEMCARD,	NULL,		"Memory cards" },
	{ IO_CDROM,	NULL,		"CD-ROM images" },
	{ IO_MAGTAPE,	NULL,		"Magnetic tapes" }
};


// columns for software picker
static const LPCTSTR mess_column_names[] =
{
	TEXT("Filename"),
};

// columns for software list
static const LPCTSTR softlist_column_names[] =
{
	TEXT("Name"),
	TEXT("List"),
	TEXT("Description"),
	TEXT("Year"),
	TEXT("Publisher"),
	TEXT("Usage"),
};



//============================================================
//  PROTOTYPES
//============================================================

static void SoftwarePicker_OnHeaderContextMenu(POINT pt, int nColumn);
static void SoftwareList_OnHeaderContextMenu(POINT pt, int nColumn);

static LPCSTR SoftwareTabView_GetTabShortName(int tab);
static LPCSTR SoftwareTabView_GetTabLongName(int tab);
static void SoftwareTabView_OnSelectionChanged(void);
static void SoftwareTabView_OnMoveSize(void);
static void SetupSoftwareTabView(void);

static void MessOpenOtherSoftware(const device_image_interface *dev);
static void MessRefreshPicker(void);

static int SoftwarePicker_GetItemImage(HWND hwndPicker, int nItem);
static void SoftwarePicker_LeavingItem(HWND hwndSoftwarePicker, int nItem);
static void SoftwarePicker_EnteringItem(HWND hwndSoftwarePicker, int nItem);

static int SoftwareList_GetItemImage(HWND hwndPicker, int nItem);
static void SoftwareList_LeavingItem(HWND hwndSoftwareList, int nItem);
static void SoftwareList_EnteringItem(HWND hwndSoftwareList, int nItem);

static BOOL DevView_GetOpenFileName(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
static BOOL DevView_GetOpenItemName(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
static BOOL DevView_GetCreateFileName(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
static BOOL DevView_Unmount(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
static void DevView_SetSelectedSoftware(HWND hwndDevView, int nDriverIndex, const machine_config *config, const device_image_interface *dev, LPCTSTR pszFilename);
static LPCTSTR DevView_GetSelectedSoftware(HWND hwndDevView, int nDriverIndex, const machine_config *config, const device_image_interface *dev, LPTSTR pszBuffer, UINT nBufferLength);



//============================================================
//  PICKER/TABVIEW CALLBACKS
//============================================================

// picker
static const struct PickerCallbacks s_softwarePickerCallbacks =
{
	SetSWSortColumn,					// pfnSetSortColumn
	GetSWSortColumn,					// pfnGetSortColumn
	SetSWSortReverse,					// pfnSetSortReverse
	GetSWSortReverse,					// pfnGetSortReverse
	NULL,								// pfnSetViewMode
	GetViewMode,						// pfnGetViewMode
	SetSWColumnWidths,					// pfnSetColumnWidths
	GetSWColumnWidths,					// pfnGetColumnWidths
	SetSWColumnOrder,					// pfnSetColumnOrder
	GetSWColumnOrder,					// pfnGetColumnOrder
	SetSWColumnShown,					// pfnSetColumnShown
	GetSWColumnShown,					// pfnGetColumnShown
	NULL,								// pfnGetOffsetChildren

	NULL,								// pfnCompare
	MamePlayGame,						// pfnDoubleClick
	SoftwarePicker_GetItemString,		// pfnGetItemString
	SoftwarePicker_GetItemImage,		// pfnGetItemImage
	SoftwarePicker_LeavingItem,			// pfnLeavingItem
	SoftwarePicker_EnteringItem,		// pfnEnteringItem
	NULL,								// pfnBeginListViewDrag
	NULL,								// pfnFindItemParent
	SoftwarePicker_Idle,				// pfnIdle
	SoftwarePicker_OnHeaderContextMenu,	// pfnOnHeaderContextMenu
	NULL								// pfnOnBodyContextMenu
};

// swlist
static const struct PickerCallbacks s_softwareListCallbacks =
{
	SetSLSortColumn,					// pfnSetSortColumn
	GetSLSortColumn,					// pfnGetSortColumn
	SetSLSortReverse,					// pfnSetSortReverse
	GetSLSortReverse,					// pfnGetSortReverse
	NULL,								// pfnSetViewMode
	GetViewMode,						// pfnGetViewMode
	SetSLColumnWidths,					// pfnSetColumnWidths
	GetSLColumnWidths,					// pfnGetColumnWidths
	SetSLColumnOrder,					// pfnSetColumnOrder
	GetSLColumnOrder,					// pfnGetColumnOrder
	SetSLColumnShown,					// pfnSetColumnShown
	GetSLColumnShown,					// pfnGetColumnShown
	NULL,								// pfnGetOffsetChildren

	NULL,								// pfnCompare
	MamePlayGame,						// pfnDoubleClick
	SoftwareList_GetItemString,			// pfnGetItemString
	SoftwareList_GetItemImage,			// pfnGetItemImage
	SoftwareList_LeavingItem,			// pfnLeavingItem
	SoftwareList_EnteringItem,			// pfnEnteringItem
	NULL,								// pfnBeginListViewDrag
	NULL,								// pfnFindItemParent
	SoftwareList_Idle,					// pfnIdle
	SoftwareList_OnHeaderContextMenu,	// pfnOnHeaderContextMenu
	NULL								// pfnOnBodyContextMenu
};


static const struct TabViewCallbacks s_softwareTabViewCallbacks =
{
	NULL,								// pfnGetShowTabCtrl
	SetCurrentSoftwareTab,				// pfnSetCurrentTab
	GetCurrentSoftwareTab,				// pfnGetCurrentTab
	NULL,								// pfnSetShowTab
	NULL,								// pfnGetShowTab

	SoftwareTabView_GetTabShortName,	// pfnGetTabShortName
	SoftwareTabView_GetTabLongName,		// pfnGetTabLongName

	SoftwareTabView_OnSelectionChanged,	// pfnOnSelectionChanged
	SoftwareTabView_OnMoveSize			// pfnOnMoveSize
};



//============================================================
//  IMPLEMENTATION
//============================================================
static char *strncpyz(char *dest, const char *source, size_t len)
{
	char *s;
	if (len) {
		s = strncpy(dest, source, len - 1);
		dest[len-1] = '\0';
	}
	else {
		s = dest;
	}
	return s;
}

static const device_entry *lookupdevice(iodevice_t d)
{
	int i;
	for (i = 0; i < ARRAY_LENGTH(s_devices); i++)
	{
		if (s_devices[i].dev_type == d)
			return &s_devices[i];
	}
	return NULL;
}



BOOL CreateMessIcons(void)
{
	int i = 0;
	HWND hwndSoftwareList;
	HIMAGELIST hSmall;
	HIMAGELIST hLarge;

	// create the icon index, if we haven't already
	if (!mess_icon_index)
	{
		mess_icon_index = (int*)pool_malloc_lib(GetMameUIMemoryPool(), driver_list::total() * IO_COUNT * sizeof(*mess_icon_index));
	}

	for (i = 0; i < (driver_list::total() * IO_COUNT); i++)
		mess_icon_index[i] = 0;

	// Associate the image lists with the list view control.
	hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SWLIST);
	hSmall = GetSmallImageList();
	hLarge = GetLargeImageList();
	(void)ListView_SetImageList(hwndSoftwareList, hSmall, LVSIL_SMALL);
	(void)ListView_SetImageList(hwndSoftwareList, hLarge, LVSIL_NORMAL);
	return TRUE;
}



static int GetMessIcon(int drvindex, int nSoftwareType)
{
	int the_index = 0;
	int nIconPos = 0;
	HICON hIcon = 0;
	const game_driver *drv;
	char buffer[256];
	const char *iconname;

	assert(drvindex >= 0);
	assert(drvindex < driver_list::total());

	if ((nSoftwareType >= 0) && (nSoftwareType < IO_COUNT))
	{
		iconname = device_image_interface::device_brieftypename((iodevice_t)nSoftwareType);
		the_index = (drvindex * IO_COUNT) + nSoftwareType;

		nIconPos = mess_icon_index[the_index];
		if (nIconPos >= 0)
		{
			drv = &driver_list::driver(drvindex);
			while (drv)
			{
				_snprintf(buffer, ARRAY_LENGTH(buffer), "%s/%s", drv->name, iconname);
				hIcon = LoadIconFromFile(buffer);
				if (hIcon)
					break;

				int cl = driver_list::clone(*drv);
				if (cl!=-1) drv = &driver_list::driver(cl); else drv = NULL;
			}

			if (hIcon)
			{
				nIconPos = ImageList_AddIcon(GetSmallImageList(), hIcon);
				ImageList_AddIcon(GetLargeImageList(), hIcon);
				if (nIconPos != -1)
					mess_icon_index[the_index] = nIconPos;
			}
		}
	}
	return nIconPos;
}

// Split multi-directory for SW Files into separate directories. SubDir path not used.
// This could be replaced by strtok.
static BOOL AddSoftwarePickerDirs(HWND hwndPicker, LPCSTR pszDirectories, LPCSTR pszSubDir)
{
	LPCSTR s;
	LPSTR pszNewString;
	char cSeparator = ';';
	int nLength = 0;

	do
	{
		s = pszDirectories;
		while(*s && (*s != cSeparator))
			s++;

		nLength = s - pszDirectories;
		if (nLength > 0)
		{
			pszNewString = (LPSTR) alloca((nLength + 1 + (pszSubDir ? strlen(pszSubDir) + 1 : 0)));
			memcpy(pszNewString, pszDirectories, nLength);
			pszNewString[nLength] = '\0';

			if (pszSubDir)
			{
				pszNewString[nLength++] = '\\';
				strcpy(&pszNewString[nLength], pszSubDir);
			}

			if (!SoftwarePicker_AddDirectory(hwndPicker, pszNewString))
				return FALSE;
		}
		pszDirectories = s + 1;
	}
	while(*s);
	return TRUE;
}

void MySoftwareListClose(void)
{
	// free the machine config, if necessary
	if (s_config)
	{
		software_config_free(s_config);
		s_config = NULL;
	}
}

void MyFillSoftwareList(int drvindex, BOOL bForce)
{
	BOOL is_same = 0;
	HWND hwndSoftwarePicker;
	HWND hwndSoftwareList;
	HWND hwndSoftwareDevView;

	// do we have to do anything?
	if (!bForce)
	{
		if (s_config != NULL)
			is_same = (drvindex == s_config->driver_index);
		else
			is_same = (drvindex < 0);
		if (is_same)
			return;
	}

	// free the machine config, if necessary
	MySoftwareListClose();

	// allocate the machine config, if necessary
	if (drvindex >= 0)
		s_config = software_config_alloc(drvindex);

	// locate key widgets
	hwndSoftwarePicker = GetDlgItem(GetMainWindow(), IDC_SWLIST);
	hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);
	hwndSoftwareDevView = GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW);

	// set up the device view
	DevView_SetDriver(hwndSoftwareDevView, s_config);

	// set up the software picker
	SoftwarePicker_Clear(hwndSoftwarePicker);
	SoftwarePicker_SetDriver(hwndSoftwarePicker, s_config);

	// Get the game's software path
	int driver_index = drvindex;
	windows_options o;
	load_options(o, OPTIONS_GAME, driver_index);
	const char* paths = o.value(OPTION_SWPATH);
	if (paths && (paths[0] > 64)) 
	{} else
	// search deeper when looking for software
	{
		// not specified in driver, try parent if it has one
		int nParentIndex = -1;
		if (DriverIsClone(driver_index) == TRUE)
		{
			nParentIndex = GetParentIndex(&driver_list::driver(driver_index));
			if (nParentIndex >= 0)
			{
				load_options(o, OPTIONS_PARENT, nParentIndex);
				paths = o.value(OPTION_SWPATH);
			}
		}
		if (paths && (paths[0] > 64))
		{} else
		{

			// still nothing, try for a system in the 'compat' field
			if (nParentIndex >= 0)
				driver_index = nParentIndex;

			// now recycle variable as a compat system number
			nParentIndex = GetCompatIndex(&driver_list::driver(driver_index));
			if (nParentIndex >= 0)
			{
				load_options(o, OPTIONS_PARENT, nParentIndex);
				paths = o.value(OPTION_SWPATH);
			}
		}
	}

	// These are the only paths that matter
	AddSoftwarePickerDirs(hwndSoftwarePicker, paths, NULL);
	paths = 0;
	// set up the software picker
	SoftwareList_Clear(hwndSoftwareList);
	SoftwareList_SetDriver(hwndSoftwareList, s_config);

	/* allocate the machine config */
	machine_config config(driver_list::driver(drvindex),MameUIGlobal());

	for (software_list_device &swlistdev : software_list_device_iterator(config.root_device()))
	{
		for (const software_info &swinfo : swlistdev.get_info())
		{
			const software_part &swpart = swinfo.parts().front();

			// search for a device with the right interface
			for (device_image_interface &image : image_interface_iterator(config.root_device()))
			{
				const char *interface = image.image_interface();
				if (interface)
				{
					if (swpart.matches_interface(interface))
					{
						// Extract the Usage data from the "info" fields.
						const char* usage = NULL;
						for (const feature_list_item &flist : swinfo.other_info())
							if (flist.name() == "usage")
								usage = flist.value().c_str();
						// Now actually add the item
						SoftwareList_AddFile(hwndSoftwareList, swinfo.shortname().c_str(), swlistdev.list_name().c_str(), swinfo.longname().c_str(),
							swinfo.publisher().c_str(), swinfo.year().c_str(), usage, image.brief_instance_name().c_str());
						break;
					}
				}
			}
		}
	}
}



void MessUpdateSoftwareList(void)
{
	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	MyFillSoftwareList(Picker_GetSelectedItem(hwndList), TRUE);
}



BOOL MessApproveImageList(HWND hParent, int drvindex)
{
	char szMessage[256];
	BOOL bResult = FALSE;
	windows_options o;

	if (g_szSelectedSoftware[0] && g_szSelectedDevice[0])
		return TRUE;

	bResult = TRUE;

	if (!bResult)
	{
		win_message_box_utf8(hParent, szMessage, MAMEUINAME, MB_OK);
	}

	return bResult;
}



// Places the specified image in the specified slot - MUST be a valid filename, not blank
static void MessSpecifyImage(int drvindex, const device_image_interface *device, LPCSTR pszFilename)
{
	const char *file_extension;
	std::string opt_name;
	device_image_interface* img = 0;

	if (LOG_SOFTWARE)
		dprintf("MessSpecifyImage(): device=%p pszFilename='%s'\n", device, pszFilename);

	// identify the file extension
	file_extension = strrchr(pszFilename, '.'); // find last period
	file_extension = file_extension ? file_extension + 1 : NULL; // if found bump pointer to first letter of the extension; if not found return NULL

	if (file_extension)
	{
		for (device_image_interface &dev : image_interface_iterator(s_config->mconfig->root_device()))
		{
			if (dev.uses_file_extension(file_extension))
			{
				opt_name = dev.instance_name();
				img = &dev;
				break;
			}
		}
	}

	if (img)
	{
		// place the image
		SetSelectedSoftware(drvindex, img, pszFilename);
		strcpy(g_szSelectedSoftware, pszFilename);
		strcpy(g_szSelectedDevice, opt_name.c_str());
	}
	else
	{
		// could not place the image
		if (LOG_SOFTWARE)
			dprintf("MessSpecifyImage(): Failed to place image '%s'\n", pszFilename);
	}
}


// This is pointless because clicking on a new item overwrites the old one anyway
static void MessRemoveImage(int drvindex, const char *pszFilename)
{
#if 0
	const char *s;
	windows_options o;
	load_options(o, OPTIONS_GAME, drvindex);
	device_image_interface* img = 0;

	for (device_image_interface &dev : image_interface_iterator(s_config->mconfig->root_device()))
	{
		// search through all the slots looking for a matching software name and unload it
		std::string opt_name = dev.instance_name();
		s = o.value(opt_name.c_str());
		if (s && (strcmp(pszFilename, s)==0))
		{
			img = &dev;
			SetSelectedSoftware(drvindex, img, NULL);
		}
	}
#endif
}



void MessReadMountedSoftware(int drvindex)
{
	// First read stuff into device view
	DevView_Refresh(GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW));

	// Now read stuff into picker
	MessRefreshPicker();
}



static void MessRefreshPicker(void)
{
	HWND hwndSoftware;
	int i = 0;
	LVFINDINFO lvfi;
	const char *s;
	windows_options o;

	hwndSoftware = GetDlgItem(GetMainWindow(), IDC_SWLIST);

	s_bIgnoreSoftwarePickerNotifies = TRUE;

	// Now clear everything out; this may call back into us but it should not
	// be problematic
	ListView_SetItemState(hwndSoftware, -1, 0, LVIS_SELECTED);

	for (device_image_interface &dev : image_interface_iterator(s_config->mconfig->root_device()))
	{
		std::string opt_name = dev.instance_name(); // get name of device slot
		load_options(o, OPTIONS_GAME, s_config->driver_index);
		s = o.value(opt_name.c_str()); // get name of software in the slot

		if (s[0]) // if software is loaded
		{
			i = SoftwarePicker_LookupIndex(hwndSoftware, s); // see if its in the picker
			if (i < 0) // not there
			{
				// add already loaded software to picker, but not if its already there
				SoftwarePicker_AddFile(hwndSoftware, s, 1);
				i = SoftwarePicker_LookupIndex(hwndSoftware, s); // refresh pointer
			}
			if (i >= 0) // is there
			{
				memset(&lvfi, 0, sizeof(lvfi));
				lvfi.flags = LVFI_PARAM;
				lvfi.lParam = i;
				i = ListView_FindItem(hwndSoftware, -1, &lvfi);
				ListView_SetItemState(hwndSoftware, i, LVIS_SELECTED, LVIS_SELECTED); // highlight it
			}
		}
	}

	s_bIgnoreSoftwarePickerNotifies = FALSE;
}



void InitMessPicker(void)
{
	struct PickerOptions opts;
	HWND hwndSoftware;

	s_config = NULL;
	hwndSoftware = GetDlgItem(GetMainWindow(), IDC_SWLIST);

	memset(&opts, 0, sizeof(opts));
	opts.pCallbacks = &s_softwarePickerCallbacks;
	opts.nColumnCount = SW_COLUMN_MAX; // number of columns in picker
	opts.ppszColumnNames = mess_column_names; // get picker column names
	SetupSoftwarePicker(hwndSoftware, &opts); // display them

	SetWindowLong(hwndSoftware, GWL_STYLE, GetWindowLong(hwndSoftware, GWL_STYLE)
		| LVS_REPORT | LVS_SHOWSELALWAYS | LVS_OWNERDRAWFIXED);

	SetupSoftwareTabView();

	{
		static const struct DevViewCallbacks s_devViewCallbacks =
		{
			DevView_GetOpenFileName,
			DevView_GetCreateFileName,
			DevView_SetSelectedSoftware,
			DevView_GetSelectedSoftware,
			DevView_GetOpenItemName,
			DevView_Unmount
		};
		DevView_SetCallbacks(GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW), &s_devViewCallbacks);
	}

	HWND hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);

	memset(&opts, 0, sizeof(opts));
	opts.pCallbacks = &s_softwareListCallbacks;
	opts.nColumnCount = SL_COLUMN_MAX; // number of columns in sw-list
	opts.ppszColumnNames = softlist_column_names; // columns for sw-list
	SetupSoftwareList(hwndSoftwareList, &opts); // show them

	SetWindowLong(hwndSoftwareList, GWL_STYLE, GetWindowLong(hwndSoftwareList, GWL_STYLE)
		| LVS_REPORT | LVS_SHOWSELALWAYS | LVS_OWNERDRAWFIXED);
}



// ------------------------------------------------------------------------
// Open others dialog
// ------------------------------------------------------------------------

static BOOL CommonFileImageDialog(LPTSTR the_last_directory, common_file_dialog_proc cfd, LPTSTR filename, const machine_config *config, mess_image_type *imagetypes)
{
	BOOL success = 0;
	OPENFILENAME of;
	char szFilter[2048];
	LPSTR s;
	const char *typname;
	int i = 0;
	TCHAR* t_filter;
	TCHAR* t_buffer;

	s = szFilter;
	*filename = 0;

	// Common image types
	strcpy(s, "Common image types");
	s += strlen(s);
	*(s++) = '|';
	for (i = 0; imagetypes[i].ext; i++)
	{
		*(s++) = '*';
		*(s++) = '.';
		strcpy(s, imagetypes[i].ext);
		s += strlen(s);
		*(s++) = ';';
	}
	*(s++) = '|';

	// All files
	strcpy(s, "All files (*.*)");
	s += strlen(s);
	*(s++) = '|';
	strcpy(s, "*.*");
	s += strlen(s);
	*(s++) = '|';

	// The others
	for (i = 0; imagetypes[i].ext; i++)
	{
		if (!imagetypes[i].dev)
			typname = "Compressed images";
		else
			typname = imagetypes[i].dlgname;

		strcpy(s, typname);
		s += strlen(s);
		strcpy(s, " (*.");
		s += strlen(s);
		strcpy(s, imagetypes[i].ext);
		s += strlen(s);
		*(s++) = ')';
		*(s++) = '|';
		*(s++) = '*';
		*(s++) = '.';
		strcpy(s, imagetypes[i].ext);
		s += strlen(s);
		*(s++) = '|';
	}
	*(s++) = '|';

	t_buffer = ui_wstring_from_utf8(szFilter);
	if( !t_buffer )
		return FALSE;

	// convert a pipe-char delimited string into a NUL delimited string
	t_filter = (LPTSTR) alloca((_tcslen(t_buffer) + 2) * sizeof(*t_filter));
	for (i = 0; t_buffer[i] != '\0'; i++)
		t_filter[i] = (t_buffer[i] != '|') ? t_buffer[i] : '\0';
	t_filter[i++] = '\0';
	t_filter[i++] = '\0';
	free(t_buffer);

	of.lStructSize = sizeof(of);
	of.hwndOwner = GetMainWindow();
	of.hInstance = NULL;
	of.lpstrFilter = t_filter;
	of.lpstrCustomFilter = NULL;
	of.nMaxCustFilter = 0;
	of.nFilterIndex = 1;
	of.lpstrFile = filename;
	of.nMaxFile = MAX_PATH;
	of.lpstrFileTitle = NULL;
	of.nMaxFileTitle = 0;
	of.lpstrInitialDir = the_last_directory;
	of.lpstrTitle = NULL;
	of.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	of.nFileOffset = 0;
	of.nFileExtension = 0;
	of.lpstrDefExt = TEXT("rom");
	of.lCustData = 0;
	of.lpfnHook = NULL;
	of.lpTemplateName = NULL;

	success = cfd(&of);
	if (success)
	{
		//GetDirectory(filename,last_directory,sizeof(last_directory));
	}

	return success;
}



/* Specify IO_COUNT for type if you want all types */
static void SetupImageTypes(const machine_config *config, mess_image_type *types, int count, BOOL bZip, const device_image_interface *dev)
{
	int num_extensions = 0;

	memset(types, 0, sizeof(*types) * count);
	count--;

	if (bZip)
	{
		/* add the ZIP extension */
		types[num_extensions].ext = core_strdup("zip");
		types[num_extensions].dev = NULL;
		types[num_extensions].dlgname = NULL;
		num_extensions++;
		types[num_extensions].ext = core_strdup("7z");
		types[num_extensions].dev = NULL;
		types[num_extensions].dlgname = NULL;
		num_extensions++;
	}

	if (dev == NULL)
	{
		/* special case; all non-printer devices */
		for (device_image_interface &device : image_interface_iterator(s_config->mconfig->root_device()))
		{
			if (device.image_type() != IO_PRINTER)
				SetupImageTypes(config, &types[num_extensions], count - num_extensions, FALSE, &device);
		}

	}
	else
	{
		std::string extensions((char*)dev->file_extensions());
		char *ext = strtok((char*)extensions.c_str(),",");
		while (ext)
		{
			if (num_extensions < count)
			{
				types[num_extensions].dev = dev;
				types[num_extensions].ext = core_strdup(ext);
				types[num_extensions].dlgname = lookupdevice(dev->image_type())->dlgname;
				num_extensions++;
			}
			ext = strtok (NULL, ",");
		}
	}
}


static void CleanupImageTypes(mess_image_type *types, int count)
{
	for (int i = 0; i < count; ++i)
		if (types[i].ext)
			free(types[i].ext);
}


static void MessSetupDevice(common_file_dialog_proc cfd, const device_image_interface *dev)
{
	TCHAR filename[MAX_PATH];
	mess_image_type imagetypes[256];
	int drvindex = 0;
	HWND hwndList;
	char* utf8_filename;
	BOOL bResult = 0;
	std::string dst = GetSWDir();
	// We only want the first path; throw out the rest
	size_t i = dst.find(';');
	if (i > 0) dst.substr(0, i);
	wchar_t* t_s = ui_wstring_from_utf8(dst.c_str());

	//  begin_resource_tracking();
	hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	drvindex = Picker_GetSelectedItem(hwndList);

	// allocate the machine config
	machine_config config(driver_list::driver(drvindex),MameUIGlobal());

	SetupImageTypes(&config, imagetypes, ARRAY_LENGTH(imagetypes), TRUE, dev);
	bResult = CommonFileImageDialog(t_s, cfd, filename, &config, imagetypes);
	free(t_s);
	CleanupImageTypes(imagetypes, ARRAY_LENGTH(imagetypes));

	if (bResult)
	{
		utf8_filename = ui_utf8_from_wstring(filename);
		if( !utf8_filename )
			return;

		SoftwarePicker_AddFile(GetDlgItem(GetMainWindow(), IDC_SWLIST), utf8_filename, 0);
		free(utf8_filename);
	}
}



static void MessOpenOtherSoftware(const device_image_interface *dev)
{
	MessSetupDevice(GetOpenFileName, dev);
}



static BOOL DevView_Unmount(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength)
{
	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	int drvindex = Picker_GetSelectedItem(hwndList);

	SetSelectedSoftware(drvindex, dev, "");

	return true;
}



/* This is used to Mount a software File in the device view of MESSUI.
    Since the emulation is not running at this time,
    we cannot do the same as the NEWUI does, that is, "initial_dir = image_working_directory(dev);"
    because a crash occurs (the image system isn't set up yet).

    Order of priority:
    1. Directory where existing image is already loaded from
    2. First directory specified in game-specific "swpath"
    3. First directory specified in the global software "swpath"
    4. mess-folder */

static BOOL DevView_GetOpenFileName(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength)
{
	BOOL bResult = 0;
	TCHAR *t_s;
	int i = 0;
	mess_image_type imagetypes[256];
	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	int drvindex = Picker_GetSelectedItem(hwndList);
	std::string as, dst, opt_name = dev->instance_name();
	windows_options o;
	load_options(o, OPTIONS_GAME, drvindex);
	auto iter = o.image_options().find(opt_name.c_str());
	std::string temp = std::move(iter->second);
	const char *s = temp.c_str();

	/* Get the path to the currently mounted image */
	util::zippath_parent(as, s);
	dst = as;

	/* See if an image was loaded, and that the path still exists */
	if ((!osd::directory::open(as.c_str())) || (as.find(':') == std::string::npos))
	{
		/* Get the path from the software tab */
		int driver_index = drvindex;
		windows_options o;
		load_options(o, OPTIONS_GAME, driver_index);
		const char* paths = o.value(OPTION_SWPATH);
		if (paths && (paths[0] > 64)) 
		{} else
		// search deeper when looking for software
		{
			// not specified in driver, try parent if it has one
			int nParentIndex = -1;
			if (DriverIsClone(driver_index) == TRUE)
			{
				nParentIndex = GetParentIndex(&driver_list::driver(driver_index));
				if (nParentIndex >= 0)
				{
					load_options(o, OPTIONS_PARENT, nParentIndex);
					paths = o.value(OPTION_SWPATH);
				}
			}
			if (paths && (paths[0] > 64))
			{} else
			{
				// still nothing, try for a system in the 'compat' field
				if (nParentIndex >= 0)
					driver_index = nParentIndex;

				// now recycle variable as a compat system number
				nParentIndex = GetCompatIndex(&driver_list::driver(driver_index));
				if (nParentIndex >= 0)
				{
					load_options(o, OPTIONS_PARENT, nParentIndex);
					paths = o.value(OPTION_SWPATH);
				}
			}
		}

		as = paths;
		/* We only want the first path; throw out the rest */
		i = as.find(';');
		if (i > 0) as.substr(0, i);
		dst = as;

		/* Make sure a folder was specified in the tab, and that it exists */
		if ((!osd::directory::open(as.c_str())) || (as.find(':') == std::string::npos))
		{
			// Get the global loose software path
			as = GetSWDir();

			/* We only want the first path; throw out the rest */
			i = as.find(';');
			if (i > 0) as.substr(0, i);
			dst = as;

			/* Make sure a folder was specified in the tab, and that it exists */
			if ((!osd::directory::open(as.c_str())) || (as.find(':') == std::string::npos))
			{
				/* Default to emu directory */
				osd_get_full_path(dst,".");
			}
		}
	}

	SetupImageTypes(config, imagetypes, ARRAY_LENGTH(imagetypes), TRUE, dev);
	t_s = ui_wstring_from_utf8(dst.c_str());
	bResult = CommonFileImageDialog(t_s, GetOpenFileName, pszFilename, config, imagetypes);
	free(t_s);
	CleanupImageTypes(imagetypes, ARRAY_LENGTH(imagetypes));

	if (bResult)
	{
		char t2[nFilenameLength];
		wcstombs(t2, pszFilename, nFilenameLength-1); // convert wide string to a normal one
		SetSelectedSoftware(drvindex, dev, t2);
	}

	return bResult;
}


/* This is used to Mount a software Item in the device view of MESSUI.

    Order of priority:
    1. Directory where existing image is already loaded from
    2. The software directory root
    3. mess-folder */

static BOOL DevView_GetOpenItemName(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength)
{
	BOOL bResult = 0;
	TCHAR *t_s;
	int i = 0;
	mess_image_type imagetypes[256];
	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	int drvindex = Picker_GetSelectedItem(hwndList);
	const char *s;
	std::string as, dst, opt_name = dev->instance_name();
	windows_options o;
	load_options(o, OPTIONS_GAME, drvindex);
	s = o.value(opt_name.c_str());

	/* Get the path to the currently mounted image, chop off any trailing backslash */
	util::zippath_parent(as, s);
	size_t t1 = as.length()-1;
	if (as[t1] == '\\') as[t1]='\0';
	dst = as;

	/* See if an image was loaded, and that the path still exists */
	if ((!osd::directory::open(as.c_str())) || (as.find(':') == std::string::npos))
	{
		/* Get the path from the SL base */
		as = GetSLDir();

		/* We only want the first path; throw out the rest */
		i = as.find(';');
		if (i > 0) as.substr(0, i);

		// Get the path to suitable software
		i = 0;
		for (software_list_device &swlist : software_list_device_iterator(config->root_device()))
		{
			for (const software_info &swinfo : swlist.get_info())
			{
				const software_part &part = swinfo.parts().front();
				if (swlist.is_compatible(part) == SOFTWARE_IS_COMPATIBLE)
				{
					for (device_image_interface &image : image_interface_iterator(config->root_device()))
					{
						if ((i == 0) && (opt_name == image.instance_name()))
						{
							const char *interface = image.image_interface();
							if (interface != nullptr && part.matches_interface(interface))
							{
								as.append("\\").append(swlist.list_name());
								i++;
							}
						}
					}
				}
			}
		}

		dst = as;

		/* Make sure a folder was specified in the tab, and that it exists */
		if ((!osd::directory::open(as.c_str())) || (as.find(':') == std::string::npos))
		{
			/* Default to emu directory */
			osd_get_full_path(dst,".");
		}
	}

	SetupImageTypes(config, imagetypes, ARRAY_LENGTH(imagetypes), TRUE, dev);
	t_s = ui_wstring_from_utf8(dst.c_str());
	bResult = CommonFileImageDialog(t_s, GetOpenFileName, pszFilename, config, imagetypes);
	free(t_s);
	CleanupImageTypes(imagetypes, ARRAY_LENGTH(imagetypes));

	if (bResult)
	{
		// This crappy code is typical of what you get with strings in c++
		// All we want to do is get the Item name out of the full path
		char t2[nFilenameLength];
		wcstombs(t2, pszFilename, nFilenameLength-1); // convert wide string to a normal one
		std::string t3 = t2; // then convert to a c++ string so we can manipulate it
		t1 = t3.find(".zip"); // get rid of zip name and anything after
		if (t1) t3[t1] = '\0';
		t1 = t3.find(".7z"); // get rid of 7zip name and anything after
		if (t1) t3[t1] = '\0';
		t1 = t3.find_last_of("\\");   // put the swlist name in
		t3[t1] = ':';
		t1 = t3.find_last_of("\\"); // get rid of path; we only want the item name
		t3.erase(0, t1+1);

		// set up editbox display text
		mbstowcs(pszFilename, t3.c_str(), nFilenameLength-1); // convert it back to a wide string

		// set up inifile text to signify to MAME that a SW ITEM is to be used
		SetSelectedSoftware(drvindex, dev, t3.c_str());
		strcpy(g_szSelectedSoftware, t3.c_str()); // store to global item name
		strcpy(g_szSelectedDevice, opt_name.c_str()); // get media-device name (brief_instance_name is ok too)
	}

	return bResult;
}


/* This is used to Create an image in the device view of MESSUI.

    Order of priority:
    1. First directory specified in game-specific "swpath"
    2. First directory specified in the global software "swpath"
    3. mess-folder */

static BOOL DevView_GetCreateFileName(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength)
{
	BOOL bResult;
	TCHAR *t_s;
	int i = 0;
	mess_image_type imagetypes[256];
	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	int drvindex = Picker_GetSelectedItem(hwndList);
	std::string as;

	/* Get the path from the software tab */
	//as = GetExtraSoftwarePaths(drvindex, 1);
	// Get the game's software path
	int driver_index = drvindex;
	windows_options o;
	load_options(o, OPTIONS_GAME, driver_index);
	const char* paths = o.value(OPTION_SWPATH);
	if (paths && (paths[0] > 64)) 
	{} else
	// search deeper when looking for software
	{
		// not specified in driver, try parent if it has one
		int nParentIndex = -1;
		if (DriverIsClone(driver_index) == TRUE)
		{
			nParentIndex = GetParentIndex(&driver_list::driver(driver_index));
			if (nParentIndex >= 0)
			{
				load_options(o, OPTIONS_PARENT, nParentIndex);
				paths = o.value(OPTION_SWPATH);
			}
		}
		if (paths && (paths[0] > 64))
		{} else
		{

			// still nothing, try for a system in the 'compat' field
			if (nParentIndex >= 0)
				driver_index = nParentIndex;

			// now recycle variable as a compat system number
			nParentIndex = GetCompatIndex(&driver_list::driver(driver_index));
			if (nParentIndex >= 0)
			{
				load_options(o, OPTIONS_PARENT, nParentIndex);
				paths = o.value(OPTION_SWPATH);
			}
		}
	}

	as = paths;
	/* We only want the first path; throw out the rest */
	i = as.find(';');
	if (i > 0) as.substr(0, i);
	t_s = ui_wstring_from_utf8(as.c_str());

	/* Make sure a folder was specified in the tab, and that it exists */
	if ((!osd::directory::open(as.c_str())) || (as.find(':') == std::string::npos))
	{
		// Get the global loose software path
		as = GetSWDir();

		/* We only want the first path; throw out the rest */
		i = as.find(';');
		if (i > 0) as.substr(0, i);
		t_s = ui_wstring_from_utf8(as.c_str());

		/* Make sure a folder was specified in the tab, and that it exists */
		if ((!osd::directory::open(as.c_str())) || (as.find(':') == std::string::npos))
		{
			std::string dst;
			osd_get_full_path(dst,".");
			/* Default to emu directory */
			t_s = ui_wstring_from_utf8(dst.c_str());
		}
	}

	SetupImageTypes(config, imagetypes, ARRAY_LENGTH(imagetypes), TRUE, dev);
	bResult = CommonFileImageDialog(t_s, GetSaveFileName, pszFilename, config, imagetypes);
	free(t_s);
	CleanupImageTypes(imagetypes, ARRAY_LENGTH(imagetypes));

	if (bResult)
	{
		char t2[nFilenameLength];
		wcstombs(t2, pszFilename, nFilenameLength-1); // convert wide string to a normal one
		SetSelectedSoftware(drvindex, dev, t2);
	}

	return bResult;
}



static void DevView_SetSelectedSoftware(HWND hwndDevView, int drvindex,
	const machine_config *config, const device_image_interface *dev, LPCTSTR pszFilename)
{
	char* utf8_filename = ui_utf8_from_wstring(pszFilename);
	if( !utf8_filename )
		return;
	MessSpecifyImage(drvindex, dev, utf8_filename);
	MessRefreshPicker();
	free(utf8_filename);
}



static LPCTSTR DevView_GetSelectedSoftware(HWND hwndDevView, int nDriverIndex,
	const machine_config *config, const device_image_interface *dev, LPTSTR pszBuffer, UINT nBufferLength)
{
	// can't get loaded image from dev->basename because the machine isn't running.
	windows_options o;
	load_options(o, OPTIONS_GAME, nDriverIndex);
	auto iter = o.image_options().find(dev->instance_name().c_str());
	std::string temp = std::move(iter->second);

	if (!temp.empty())
	{
		TCHAR* t_s = ui_wstring_from_utf8(temp.c_str());
		if( t_s )
		{
			_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_s);
			free(t_s);
			LPCTSTR t_buffer = pszBuffer;
			return t_buffer;
		}
	}

	return ui_wstring_from_utf8(""); // nothing loaded or error occurred
}



// ------------------------------------------------------------------------
// Software Picker Class
// ------------------------------------------------------------------------

static int SoftwarePicker_GetItemImage(HWND hwndPicker, int nItem)
{
	iodevice_t nType;
	int nIcon = 0;
	int drvindex = 0;
	const char *icon_name;
	HWND hwndGamePicker = GetDlgItem(GetMainWindow(), IDC_LIST);
	HWND hwndSoftwarePicker = GetDlgItem(GetMainWindow(), IDC_SWLIST);
	drvindex = Picker_GetSelectedItem(hwndGamePicker);
	nType = SoftwarePicker_GetImageType(hwndSoftwarePicker, nItem);
	nIcon = GetMessIcon(drvindex, nType);
	if (!nIcon)
	{
		switch(nType)
		{
			case IO_UNKNOWN:
				nIcon = FindIconIndex(IDI_WIN_REDX);
				break;

			default:
				icon_name = lookupdevice(nType)->icon_name;
				if (!icon_name)
					icon_name = device_image_interface::device_typename(nType);
				nIcon = FindIconIndexByName(icon_name);
				if (nIcon < 0)
					nIcon = FindIconIndex(IDI_WIN_UNKNOWN);
				break;
		}
	}
	return nIcon;
}



static void SoftwarePicker_LeavingItem(HWND hwndSoftwarePicker, int nItem)
{
	int drvindex = 0;
	const char *pszFullName;

	if (!s_bIgnoreSoftwarePickerNotifies)
	{
		HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
		drvindex = Picker_GetSelectedItem(hwndList);
		pszFullName = SoftwarePicker_LookupFilename(hwndSoftwarePicker, nItem);

		MessRemoveImage(drvindex, pszFullName);
	}
}



static void SoftwarePicker_EnteringItem(HWND hwndSoftwarePicker, int nItem)
{
	LPCSTR pszFullName;
	LPCSTR pszName;
	const char* tmp;
	LPSTR s;
	int drvindex = 0;
	HWND hwndList;

	hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);

	if (!s_bIgnoreSoftwarePickerNotifies)
	{
		drvindex = Picker_GetSelectedItem(hwndList);

		// Get the fullname and partialname for this file
		pszFullName = SoftwarePicker_LookupFilename(hwndSoftwarePicker, nItem);
		tmp = strrchr(pszFullName, '\\');
		pszName = tmp ? tmp + 1 : pszFullName;

		// Do the dirty work
		MessSpecifyImage(drvindex, NULL, pszFullName);

		// Set up g_szSelectedItem, for the benefit of UpdateScreenShot()
		strncpyz(g_szSelectedItem, pszName, ARRAY_LENGTH(g_szSelectedItem));
		s = strrchr(g_szSelectedItem, '.');
		if (s)
			*s = '\0';

		UpdateScreenShot();
	}
}


// ------------------------------------------------------------------------
// Software List Class
// ------------------------------------------------------------------------

static int SoftwareList_GetItemImage(HWND hwndPicker, int nItem)
{
	iodevice_t nType;
	int nIcon = 0;
	int drvindex = 0;
	const char *icon_name;
	HWND hwndGamePicker = GetDlgItem(GetMainWindow(), IDC_LIST);
	HWND hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);
	drvindex = Picker_GetSelectedItem(hwndGamePicker);
	nType = SoftwareList_GetImageType(hwndSoftwareList, nItem);
	nIcon = GetMessIcon(drvindex, nType);
	if (!nIcon)
	{
		switch(nType)
		{
			case IO_UNKNOWN:
				nIcon = FindIconIndex(IDI_WIN_REDX);
				break;

			default:
				icon_name = lookupdevice(nType)->icon_name;
				if (!icon_name)
					icon_name = device_image_interface::device_typename(nType);
				nIcon = FindIconIndexByName(icon_name);
				if (nIcon < 0)
					nIcon = FindIconIndex(IDI_WIN_UNKNOWN);
				break;
		}
	}
	return nIcon;
}



static void SoftwareList_LeavingItem(HWND hwndSoftwareList, int nItem)
{
	if (!s_bIgnoreSoftwarePickerNotifies)
	{
		g_szSelectedSoftware[0] = 0;
		g_szSelectedDevice[0] = 0;
		g_szSelectedItem[0] = 0;
	}
}



static void SoftwareList_EnteringItem(HWND hwndSoftwareList, int nItem)
{
	LPCSTR pszFullName;
	LPCSTR pszFileName;
	int drvindex = 0;
	HWND hwndList;

	hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);

	if (!s_bIgnoreSoftwarePickerNotifies)
	{
		drvindex = Picker_GetSelectedItem(hwndList);

		// Get the fullname and partialname for this file
		pszFileName = SoftwareList_LookupFilename(hwndSoftwareList, nItem); // to run the software
		pszFullName = SoftwareList_LookupFullname(hwndSoftwareList, nItem); // for the screenshot

		strncpyz(g_szSelectedSoftware, pszFileName, ARRAY_LENGTH(g_szSelectedSoftware));

		strncpyz(g_szSelectedDevice, SoftwareList_LookupDevice(hwndSoftwareList, nItem), ARRAY_LENGTH(g_szSelectedDevice));

		// Set up s_szSelecteItem, for the benefit of UpdateScreenShot()
		strncpyz(g_szSelectedItem, pszFullName, ARRAY_LENGTH(g_szSelectedItem));

		UpdateScreenShot();
	}
	drvindex++;
}


// ------------------------------------------------------------------------
// Header context menu stuff
// ------------------------------------------------------------------------

static HWND MyColumnDialogProc_hwndPicker;
static int *MyColumnDialogProc_order;
static int *MyColumnDialogProc_shown;

static void MyGetRealColumnOrder(int *order)
{
	int nColumnCount = Picker_GetColumnCount(MyColumnDialogProc_hwndPicker);
	for (int i = 0; i < nColumnCount; i++)
		order[i] = Picker_GetRealColumnFromViewColumn(MyColumnDialogProc_hwndPicker, i);
}



static void MyGetColumnInfo(int *order, int *shown)
{
	const struct PickerCallbacks *pCallbacks;
	pCallbacks = Picker_GetCallbacks(MyColumnDialogProc_hwndPicker);
	pCallbacks->pfnGetColumnOrder(order);
	pCallbacks->pfnGetColumnShown(shown);
}



static void MySetColumnInfo(int *order, int *shown)
{
	const struct PickerCallbacks *pCallbacks;
	pCallbacks = Picker_GetCallbacks(MyColumnDialogProc_hwndPicker);
	pCallbacks->pfnSetColumnOrder(order);
	pCallbacks->pfnSetColumnShown(shown);
}


static INT_PTR CALLBACK MyColumnDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	INT_PTR result = 0;
	int nColumnCount = Picker_GetColumnCount(MyColumnDialogProc_hwndPicker);
	const LPCTSTR *ppszColumnNames = Picker_GetColumnNames(MyColumnDialogProc_hwndPicker);

	result = InternalColumnDialogProc(hDlg, Msg, wParam, lParam, nColumnCount,
		MyColumnDialogProc_shown, MyColumnDialogProc_order, ppszColumnNames,
		MyGetRealColumnOrder, MyGetColumnInfo, MySetColumnInfo);

	return result;
}



static BOOL RunColumnDialog(HWND hwndPicker)
{
	BOOL bResult = 0;

	MyColumnDialogProc_hwndPicker = hwndPicker;
	int nColumnCount = Picker_GetColumnCount(MyColumnDialogProc_hwndPicker);

	MyColumnDialogProc_order = (int*)alloca(nColumnCount * sizeof(int));
	MyColumnDialogProc_shown = (int*)alloca(nColumnCount * sizeof(int));
	bResult = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_COLUMNS), GetMainWindow(), MyColumnDialogProc);
	return bResult;
}



static void SoftwarePicker_OnHeaderContextMenu(POINT pt, int nColumn)
{
	HMENU hMenu;
	int nMenuItem = 0;

	HMENU hMenuLoad = LoadMenu(NULL, MAKEINTRESOURCE(IDR_CONTEXT_HEADER));
	hMenu = GetSubMenu(hMenuLoad, 0);

	nMenuItem = (int) TrackPopupMenu(hMenu,
		TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
		pt.x,
		pt.y,
		0,
		GetMainWindow(),
		NULL);

	DestroyMenu(hMenuLoad);

	HWND hwndPicker = GetDlgItem(GetMainWindow(), IDC_SWLIST);

	switch(nMenuItem) {
	case ID_SORT_ASCENDING:
		SetSWSortReverse(FALSE);
		SetSWSortColumn(Picker_GetRealColumnFromViewColumn(hwndPicker, nColumn));
		Picker_Sort(hwndPicker);
		break;

	case ID_SORT_DESCENDING:
		SetSWSortReverse(TRUE);
		SetSWSortColumn(Picker_GetRealColumnFromViewColumn(hwndPicker, nColumn));
		Picker_Sort(hwndPicker);
		break;

	case ID_CUSTOMIZE_FIELDS:
		if (RunColumnDialog(hwndPicker))
			Picker_ResetColumnDisplay(hwndPicker);
		break;
	}
}

static void SoftwareList_OnHeaderContextMenu(POINT pt, int nColumn)
{
	HMENU hMenu;
	HMENU hMenuLoad;
	HWND hwndPicker;

	hMenuLoad = LoadMenu(NULL, MAKEINTRESOURCE(IDR_CONTEXT_HEADER));
	hMenu = GetSubMenu(hMenuLoad, 0);

	int nMenuItem = (int) TrackPopupMenu(hMenu,
		TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
		pt.x,
		pt.y,
		0,
		GetMainWindow(),
		NULL);

	DestroyMenu(hMenuLoad);

	hwndPicker = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);

	switch(nMenuItem) {
	case ID_SORT_ASCENDING:
		SetSLSortReverse(FALSE);
		SetSLSortColumn(Picker_GetRealColumnFromViewColumn(hwndPicker, nColumn));
		Picker_Sort(hwndPicker);
		break;

	case ID_SORT_DESCENDING:
		SetSLSortReverse(TRUE);
		SetSLSortColumn(Picker_GetRealColumnFromViewColumn(hwndPicker, nColumn));
		Picker_Sort(hwndPicker);
		break;

	case ID_CUSTOMIZE_FIELDS:
		if (RunColumnDialog(hwndPicker))
			Picker_ResetColumnDisplay(hwndPicker);
		break;
	}
}


// ------------------------------------------------------------------------
// MessCommand
// ------------------------------------------------------------------------

BOOL MessCommand(HWND hwnd,int id, HWND hwndCtl, UINT codeNotify)
{
	switch (id)
	{
		case ID_MESS_OPEN_SOFTWARE:
			MessOpenOtherSoftware(NULL);
			break;

	}
	return FALSE;
}



// ------------------------------------------------------------------------
// Software Tab View
// ------------------------------------------------------------------------

static LPCSTR s_tabs[] =
{
	"picker\0SW Files",
	"devview\0Media View",
	"softlist\0SW Items",
};



static LPCSTR SoftwareTabView_GetTabShortName(int tab)
{
	return s_tabs[tab];
}



static LPCSTR SoftwareTabView_GetTabLongName(int tab)
{
	return s_tabs[tab] + strlen(s_tabs[tab]) + 1;
}



static void SoftwareTabView_OnSelectionChanged(void)
{
	int nTab = 0;
	HWND hwndSoftwarePicker;
	HWND hwndSoftwareDevView;
	HWND hwndSoftwareList;

	hwndSoftwarePicker = GetDlgItem(GetMainWindow(), IDC_SWLIST);
	hwndSoftwareDevView = GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW);
	hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);

	nTab = TabView_GetCurrentTab(GetDlgItem(GetMainWindow(), IDC_SWTAB));

	switch(nTab)
	{
		case 0:
			ShowWindow(hwndSoftwarePicker, SW_SHOW);
			ShowWindow(hwndSoftwareDevView, SW_HIDE);
			ShowWindow(hwndSoftwareList, SW_HIDE);
			break;

		case 1:
			ShowWindow(hwndSoftwarePicker, SW_HIDE);
			ShowWindow(hwndSoftwareDevView, SW_SHOW);
			ShowWindow(hwndSoftwareList, SW_HIDE);
			break;
		case 2:
			ShowWindow(hwndSoftwarePicker, SW_HIDE);
			ShowWindow(hwndSoftwareDevView, SW_HIDE);
			ShowWindow(hwndSoftwareList, SW_SHOW);
			break;
	}
}



static void SoftwareTabView_OnMoveSize(void)
{
	HWND hwndSoftwareTabView;
	HWND hwndSoftwarePicker;
	HWND hwndSoftwareDevView;
	HWND hwndSoftwareList;
	RECT rMain, rSoftwareTabView, rClient, rTab;
	BOOL res = 0;

	hwndSoftwareTabView = GetDlgItem(GetMainWindow(), IDC_SWTAB);
	hwndSoftwarePicker = GetDlgItem(GetMainWindow(), IDC_SWLIST);
	hwndSoftwareDevView = GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW);
	hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);

	GetWindowRect(hwndSoftwareTabView, &rSoftwareTabView);
	GetClientRect(GetMainWindow(), &rMain);
	ClientToScreen(GetMainWindow(), &((POINT *) &rMain)[0]);
	ClientToScreen(GetMainWindow(), &((POINT *) &rMain)[1]);

	// Calculate rClient from rSoftwareTabView in terms of rMain coordinates
	rClient.left = rSoftwareTabView.left - rMain.left;
	rClient.top = rSoftwareTabView.top - rMain.top;
	rClient.right = rSoftwareTabView.right - rMain.left;
	rClient.bottom = rSoftwareTabView.bottom - rMain.top;

	// If the tabs are visible, then make sure that the tab view's tabs are
	// not being covered up
	if (GetWindowLong(hwndSoftwareTabView, GWL_STYLE) & WS_VISIBLE)
	{
		res = TabCtrl_GetItemRect(hwndSoftwareTabView, 0, &rTab);
		rClient.top += rTab.bottom - rTab.top + 2;
	}

	/* Now actually move the controls */
	MoveWindow(hwndSoftwarePicker, rClient.left, rClient.top,
		rClient.right - rClient.left, rClient.bottom - rClient.top,
		TRUE);
	MoveWindow(hwndSoftwareDevView, rClient.left + 3, rClient.top + 2,
		rClient.right - rClient.left - 6, rClient.bottom - rClient.top -4,
		TRUE);
	MoveWindow(hwndSoftwareList, rClient.left, rClient.top,
		rClient.right - rClient.left, rClient.bottom - rClient.top,
		TRUE);
	res++;
}



static void SetupSoftwareTabView(void)
{
	struct TabViewOptions opts;

	memset(&opts, 0, sizeof(opts));
	opts.pCallbacks = &s_softwareTabViewCallbacks;
	opts.nTabCount = ARRAY_LENGTH(s_tabs);

	SetupTabView(GetDlgItem(GetMainWindow(), IDC_SWTAB), &opts);
}

