// For licensing and usage information, read docs/winui_license.txt
//****************************************************************************
//============================================================
//
//  messui.c - MESS extensions to src/osd/winui/winui.c
//
//============================================================

// standard windows headers
#include <windows.h>
#include <commdlg.h>
#include <wingdi.h>
#include <winuser.h>
#include <tchar.h>

// MAME/MAMEOS/MAMEUI/MESS headers
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
#include "messui.h"
#include "winutf8.h"
#include "zippath.h"


//============================================================
//  PARAMETERS
//============================================================

#define DEVVIEW_PADDING 10
#define DEVVIEW_SPACING 21
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
static HWND MyColumnDialogProc_hwndPicker;
static int *MyColumnDialogProc_order;
static int *MyColumnDialogProc_shown;
static int *mess_icon_index;
static std::map<std::string,std::string> slmap; // store folder for Media View Mount Item
static std::map<std::string,int> mvmap;  // store indicator if Media View Unmount should be enabled

// TODO - We need to make icons for Cylinders, Punch Cards, and Punch Tape!
static const device_entry s_devices[] =
{
	{ IO_CARTSLOT,  "roms",     "Cartridge images" },
	{ IO_FLOPPY,    "floppy",   "Floppy disk images" },
	{ IO_HARDDISK,  "hard",     "Hard disk images" },
	{ IO_CYLINDER,  NULL,       "Cylinders" },
	{ IO_CASSETTE,  NULL,       "Cassette images" },
	{ IO_PUNCHCARD, NULL,       "Punchcard images" },
	{ IO_PUNCHTAPE, NULL,       "Punchtape images" },
	{ IO_PRINTER,   NULL,       "Printer Output" },
	{ IO_SERIAL,    NULL,       "Serial Output" },
	{ IO_PARALLEL,  NULL,       "Parallel Output" },
	{ IO_SNAPSHOT,  "snapshot", "Snapshots" },
	{ IO_QUICKLOAD, "snapshot", "Quickloads" },
	{ IO_MEMCARD,   NULL,       "Memory cards" },
	{ IO_CDROM,     NULL,       "CD-ROM images" },
	{ IO_MAGTAPE,   NULL,       "Magnetic tapes" },
	{ IO_ROM,       "roms",     "Apps on roms" },
	{ IO_MIDIIN,    NULL,       "MIDI input" },
	{ IO_MIDIOUT,   NULL,       "MIDI output" }
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


struct DevViewCallbacks
{
	BOOL (*pfnGetOpenFileName)(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
	BOOL (*pfnGetCreateFileName)(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
	void (*pfnSetSelectedSoftware)(HWND hwndDevView, int nGame, const machine_config *config, const device_image_interface *dev, LPCTSTR pszFilename);
	LPCTSTR (*pfnGetSelectedSoftware)(HWND hwndDevView, int nGame, const machine_config *config, const device_image_interface *dev, LPTSTR pszBuffer, UINT nBufferLength);
	BOOL (*pfnGetOpenItemName)(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
	BOOL (*pfnUnmount)(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
};

struct DevViewInfo
{
	HFONT hFont;
	int nWidth;
	BOOL bSurpressFilenameChanged;
	const software_config *config;
	const struct DevViewCallbacks *pCallbacks;
	struct DevViewEntry *pEntries;
};



struct DevViewEntry
{
	const device_image_interface *dev;
	HWND hwndStatic;
	HWND hwndEdit;
	HWND hwndBrowseButton;
	WNDPROC pfnEditWndProc;
};



//============================================================
//  PROTOTYPES
//============================================================

static void SoftwarePicker_OnHeaderContextMenu(POINT pt, int nColumn);
static int SoftwarePicker_GetItemImage(HWND hwndPicker, int nItem);
static void SoftwarePicker_LeavingItem(HWND hwndSoftwarePicker, int nItem);
static void SoftwarePicker_EnteringItem(HWND hwndSoftwarePicker, int nItem);

static void SoftwareList_OnHeaderContextMenu(POINT pt, int nColumn);
static int SoftwareList_GetItemImage(HWND hwndPicker, int nItem);
static void SoftwareList_LeavingItem(HWND hwndSoftwareList, int nItem);
static void SoftwareList_EnteringItem(HWND hwndSoftwareList, int nItem);

static LPCSTR SoftwareTabView_GetTabShortName(int tab);
static LPCSTR SoftwareTabView_GetTabLongName(int tab);
static void SoftwareTabView_OnSelectionChanged(void);
static void SoftwareTabView_OnMoveSize(void);
static void SetupSoftwareTabView(void);

static void MessRefreshPicker(void);

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
	for (int i = 0; i < ARRAY_LENGTH(s_devices); i++)
	{
		if (s_devices[i].dev_type == d)
			return &s_devices[i];
	}
	return NULL;
}


static struct DevViewInfo *GetDevViewInfo(HWND hwndDevView)
{
	LONG_PTR l = GetWindowLongPtr(hwndDevView, GWLP_USERDATA);
	return (struct DevViewInfo *) l;
}


static void DevView_SetCallbacks(HWND hwndDevView, const struct DevViewCallbacks *pCallbacks)
{
	struct DevViewInfo *pDevViewInfo;
	pDevViewInfo = GetDevViewInfo(hwndDevView);
	pDevViewInfo->pCallbacks = pCallbacks;
}


void InitMessPicker(void)
{
	struct PickerOptions opts;

	s_config = NULL;
	HWND hwndSoftware = GetDlgItem(GetMainWindow(), IDC_SWLIST);

	memset(&opts, 0, sizeof(opts));
	opts.pCallbacks = &s_softwarePickerCallbacks;
	opts.nColumnCount = SW_COLUMN_MAX; // number of columns in picker
	opts.ppszColumnNames = mess_column_names; // get picker column names
	SetupSoftwarePicker(hwndSoftware, &opts); // display them

	SetWindowLong(hwndSoftware, GWL_STYLE, GetWindowLong(hwndSoftware, GWL_STYLE) | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_OWNERDRAWFIXED);

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

	SetWindowLong(hwndSoftwareList, GWL_STYLE, GetWindowLong(hwndSoftwareList, GWL_STYLE) | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_OWNERDRAWFIXED);
}


BOOL CreateMessIcons(void)
{
	// create the icon index, if we haven't already
	if (!mess_icon_index)
		mess_icon_index = (int*)pool_malloc_lib(GetMameUIMemoryPool(), driver_list::total() * IO_COUNT * sizeof(*mess_icon_index));

	for (int i = 0; i < (driver_list::total() * IO_COUNT); i++)
		mess_icon_index[i] = 0;

	// Associate the image lists with the list view control.
	HWND hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SWLIST);
	HIMAGELIST hSmall = GetSmallImageList();
	HIMAGELIST hLarge = GetLargeImageList();
	(void)ListView_SetImageList(hwndSoftwareList, hSmall, LVSIL_SMALL);
	(void)ListView_SetImageList(hwndSoftwareList, hLarge, LVSIL_NORMAL);
	return true;
}


static int GetMessIcon(int drvindex, int nSoftwareType)
{
	int the_index = 0;
	int nIconPos = 0;
	HICON hIcon = 0;
	const game_driver *drv;
	char buffer[256];
	const char *iconname;

	//assert(drvindex >= 0);
	//assert(drvindex < driver_list::total());

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
				if (cl != -1) drv = &driver_list::driver(cl); else drv = NULL;
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


// Rules: if driver's path valid and not same as global, that's ok. Otherwise try parent, then compat.
// If still no good, use global if it was valid. Lastly, default to emu folder.
// Some callers regard the defaults as invalid so prepend a character to indicate validity
static std::string ProcessSWDir(int drvindex)
{
	BOOL b_dir = false;
	char dir0[2048];
	strcpy(dir0, GetSWDir()); // global SW
	char* t0 = strtok(dir0, ";");
	if (t0 && osd::directory::open(t0))  // make sure its valid
		b_dir = true;

	// Get the game's software path
	windows_options o;
	load_options(o, OPTIONS_GAME, drvindex);
	char dir1[2048];
	strcpy(dir1, o.value(OPTION_SWPATH));
	char* t1 = strtok(dir1, ";");
	if (t1 && osd::directory::open(t1))  // make sure its valid
		if (b_dir && (strcmp(t0, t1) != 0))
			return std::string("1") + std::string(dir1);

	// not specified in driver, try parent if it has one
	int nParentIndex = drvindex;
	if (DriverIsClone(drvindex) == true)
	{
		nParentIndex = GetParentIndex(&driver_list::driver(drvindex));
		if (nParentIndex >= 0)
		{
			load_options(o, OPTIONS_PARENT, nParentIndex);
			strcpy(dir1, o.value(OPTION_SWPATH));
			t1 = strtok(dir1, ";");
			if (t1 && osd::directory::open(t1))  // make sure its valid
				if (b_dir && (strcmp(t0, t1) != 0))
					return std::string("1") + std::string(dir1);
		}
	}

	// Try compat if it has one
	int nCloneIndex = GetCompatIndex(&driver_list::driver(nParentIndex));
	if (nCloneIndex >= 0)
	{
		load_options(o, OPTIONS_PARENT, nCloneIndex);
		strcpy(dir1, o.value(OPTION_SWPATH));
		t1 = strtok(dir1, ";");
		if (t1 && osd::directory::open(t1))  // make sure its valid
			if (b_dir && (strcmp(t0, t1) != 0))
				return std::string("1") + std::string(dir1);
	}

	// Try the global root
	if (b_dir)
		return std::string("0") + std::string(dir0);

	// nothing valid, drop to default emu directory
	std::string dst;
	osd_get_full_path(dst,".");
	return std::string("1") + dst;
}


// Split multi-directory for SW Files into separate directories, and ask the picker to add the files from each.
// pszSubDir path not used by any caller.
static BOOL AddSoftwarePickerDirs(HWND hwndPicker, LPCSTR pszDirectories, LPCSTR pszSubDir)
{
	if (!pszDirectories)
		return false;

	char s[2048];
	std::string pszNewString;
	strcpy(s, pszDirectories);
	LPSTR t1 = strtok(s,";");
	while (t1)
	{
		if (pszSubDir)
			pszNewString = t1 + std::string("\\") + pszSubDir;
		else
			pszNewString = t1;

		if (!SoftwarePicker_AddDirectory(hwndPicker, pszNewString.c_str()))
			return false;

		t1 = strtok (NULL, ",");
	}
	return true;
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


static void DevView_Clear(HWND hwndDevView)
{
	struct DevViewInfo *pDevViewInfo;
	pDevViewInfo = GetDevViewInfo(hwndDevView);

	if (pDevViewInfo->pEntries)
	{
		for (int i = 0; pDevViewInfo->pEntries[i].dev; i++)
		{
			DestroyWindow(pDevViewInfo->pEntries[i].hwndStatic);
			DestroyWindow(pDevViewInfo->pEntries[i].hwndEdit);
			DestroyWindow(pDevViewInfo->pEntries[i].hwndBrowseButton);
		}
		free(pDevViewInfo->pEntries);
		pDevViewInfo->pEntries = NULL;
	}

	pDevViewInfo->config = NULL;
}


static void DevView_GetColumns(HWND hwndDevView, int *pnStaticPos, int *pnStaticWidth, int *pnEditPos, int *pnEditWidth, int *pnButtonPos, int *pnButtonWidth)
{
	struct DevViewInfo *pDevViewInfo;
	RECT r;

	pDevViewInfo = GetDevViewInfo(hwndDevView);

	GetClientRect(hwndDevView, &r);

	*pnStaticPos = DEVVIEW_PADDING;
	*pnStaticWidth = pDevViewInfo->nWidth;

	*pnButtonWidth = 25;
	*pnButtonPos = (r.right - r.left) - *pnButtonWidth - DEVVIEW_PADDING;

//	*pnEditPos = *pnStaticPos + *pnStaticWidth + DEVVIEW_PADDING;
	*pnEditPos = DEVVIEW_PADDING;
	*pnEditWidth = *pnButtonPos - *pnEditPos - DEVVIEW_PADDING;
	if (*pnEditWidth < 0)
		*pnEditWidth = 0;
}


static void DevView_TextChanged(HWND hwndDevView, int nChangedEntry, LPCTSTR pszFilename)
{
	struct DevViewInfo *pDevViewInfo;
	pDevViewInfo = GetDevViewInfo(hwndDevView);
	if (!pDevViewInfo->bSurpressFilenameChanged && pDevViewInfo->pCallbacks->pfnSetSelectedSoftware)
	{
		pDevViewInfo->pCallbacks->pfnSetSelectedSoftware(hwndDevView,
			pDevViewInfo->config->driver_index,
			pDevViewInfo->config->mconfig,
			pDevViewInfo->pEntries[nChangedEntry].dev,
			pszFilename);
	}
}


static LRESULT CALLBACK DevView_EditWndProc(HWND hwndEdit, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	struct DevViewInfo *pDevViewInfo;
	struct DevViewEntry *pEnt;
	LRESULT rc;
	HWND hwndDevView = 0;
	LONG_PTR l = GetWindowLongPtr(hwndEdit, GWLP_USERDATA);
	pEnt = (struct DevViewEntry *) l;
	if (IsWindowUnicode(hwndEdit))
		rc = CallWindowProcW(pEnt->pfnEditWndProc, hwndEdit, nMessage, wParam, lParam);
	else
		rc = CallWindowProcA(pEnt->pfnEditWndProc, hwndEdit, nMessage, wParam, lParam);

	if (nMessage == WM_SETTEXT)
	{
		hwndDevView = GetParent(hwndEdit);
		pDevViewInfo = GetDevViewInfo(hwndDevView);
		DevView_TextChanged(hwndDevView, pEnt - pDevViewInfo->pEntries, (LPCTSTR) lParam);
	}
	return rc;
}


void DevView_Refresh(HWND hwndDevView)
{
	struct DevViewInfo *pDevViewInfo;
	LPCTSTR pszSelection;
	TCHAR szBuffer[MAX_PATH];

	pDevViewInfo = GetDevViewInfo(hwndDevView);

	if (pDevViewInfo->pEntries)
	{
		for (int i = 0; pDevViewInfo->pEntries[i].dev; i++)
		{
			pszSelection = pDevViewInfo->pCallbacks->pfnGetSelectedSoftware(
				hwndDevView,
				pDevViewInfo->config->driver_index,
				pDevViewInfo->config->mconfig,
				pDevViewInfo->pEntries[i].dev,
				szBuffer, MAX_PATH);

			if (!pszSelection)
				pszSelection = TEXT("");

			pDevViewInfo->bSurpressFilenameChanged = true;
			SetWindowText(pDevViewInfo->pEntries[i].hwndEdit, pszSelection);
			pDevViewInfo->bSurpressFilenameChanged = false;
		}
	}
}


//#ifdef __GNUC__
//#pragma GCC diagnostic ignored "-Wunused-variable"
//#endif
static BOOL DevView_SetDriver(HWND hwndDevView, const software_config *config)
{
	struct DevViewInfo *pDevViewInfo;
	struct DevViewEntry *pEnt;
	int i, y = 0, nHeight = 0, nDevCount = 0;
	int nStaticPos = 0, nStaticWidth = 0, nEditPos = 0, nEditWidth = 0, nButtonPos = 0, nButtonWidth = 0;
	HDC hDc;
	LPTSTR *ppszDevices;
	SIZE sz;
	LONG_PTR l = 0;
	pDevViewInfo = GetDevViewInfo(hwndDevView);
	std::string instance;

	// clear out
	DevView_Clear(hwndDevView);

	// copy the config
	pDevViewInfo->config = config;

	// count total amount of devices
	for (device_image_interface &dev : image_interface_iterator(pDevViewInfo->config->mconfig->root_device()))
		if (dev.user_loadable())
			nDevCount++;

	if (nDevCount > 0)
	{
		// get the names of all of the media-slots so we can then work out how much space is needed to display them
		ppszDevices = (LPTSTR *) alloca(nDevCount * sizeof(*ppszDevices));
		i = 0;
		for (device_image_interface &dev : image_interface_iterator(pDevViewInfo->config->mconfig->root_device()))
		{
			if (!dev.user_loadable())
				continue;
			instance = string_format("%s (%s)", dev.instance_name(), dev.brief_instance_name());
			LPTSTR t_s = ui_wstring_from_utf8(instance.c_str());
			ppszDevices[i] = (TCHAR*)alloca((_tcslen(t_s) + 1) * sizeof(TCHAR));
			_tcscpy(ppszDevices[i], t_s);
			free(t_s);
			i++;
		}

		// Calculate the requisite size for the device column
		pDevViewInfo->nWidth = 0;
		hDc = GetDC(hwndDevView);
		for (i = 0; i < nDevCount; i++)
		{
			GetTextExtentPoint32(hDc, ppszDevices[i], _tcslen(ppszDevices[i]), &sz);
			if (sz.cx > pDevViewInfo->nWidth)
				pDevViewInfo->nWidth = sz.cx;
		}
		ReleaseDC(hwndDevView, hDc);

		pEnt = (struct DevViewEntry *) malloc(sizeof(struct DevViewEntry) * (nDevCount + 1));
		if (!pEnt)
			return false;
		memset(pEnt, 0, sizeof(struct DevViewEntry) * (nDevCount + 1));
		pDevViewInfo->pEntries = pEnt;

		y = DEVVIEW_PADDING;
		nHeight = DEVVIEW_SPACING;
		DevView_GetColumns(hwndDevView, &nStaticPos, &nStaticWidth, &nEditPos, &nEditWidth, &nButtonPos, &nButtonWidth);

		// Now actually display the media-slot names, and show the empty boxes and the browse button
		for (device_image_interface &dev : image_interface_iterator(pDevViewInfo->config->mconfig->root_device()))
		{
			if (!dev.user_loadable())
				continue;
			pEnt->dev = &dev;

			instance = string_format("%s (%s)", dev.instance_name(), dev.brief_instance_name()); // get name of the slot (long and short)
			std::transform(instance.begin(), instance.begin()+1, instance.begin(), ::toupper); // turn first char to uppercase
			pEnt->hwndStatic = win_create_window_ex_utf8(0, "STATIC", instance.c_str(), // display it
				WS_VISIBLE | WS_CHILD, nStaticPos,
				y, nStaticWidth, nHeight, hwndDevView, NULL, NULL, NULL);
			y += nHeight;

			pEnt->hwndEdit = win_create_window_ex_utf8(0, "EDIT", "",
				WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, nEditPos,
				y, nEditWidth, nHeight, hwndDevView, NULL, NULL, NULL); // display blank edit box

			pEnt->hwndBrowseButton = win_create_window_ex_utf8(0, "BUTTON", "...",
				WS_VISIBLE | WS_CHILD, nButtonPos,
				y, nButtonWidth, nHeight, hwndDevView, NULL, NULL, NULL); // display browse button

			if (pEnt->hwndStatic)
				SendMessage(pEnt->hwndStatic, WM_SETFONT, (WPARAM) pDevViewInfo->hFont, true);

			if (pEnt->hwndEdit)
			{
				SendMessage(pEnt->hwndEdit, WM_SETFONT, (WPARAM) pDevViewInfo->hFont, true);
				l = (LONG_PTR) DevView_EditWndProc;
				l = SetWindowLongPtr(pEnt->hwndEdit, GWLP_WNDPROC, l);
				pEnt->pfnEditWndProc = (WNDPROC) l;
				SetWindowLongPtr(pEnt->hwndEdit, GWLP_USERDATA, (LONG_PTR) pEnt);
			}

			if (pEnt->hwndBrowseButton)
				SetWindowLongPtr(pEnt->hwndBrowseButton, GWLP_USERDATA, (LONG_PTR) pEnt);

			y += nHeight;
			y += nHeight;

			pEnt++;
		}
	}

	DevView_Refresh(hwndDevView); // show names of already-loaded software
	return true;
}
//#ifdef __GNUC__
//#pragma GCC diagnostic error "-Wunused-variable"
//#endif


void MyFillSoftwareList(int drvindex, BOOL bForce)
{
	// do we have to do anything?
	if (!bForce)
	{
		BOOL is_same = false;
		if (s_config)
			is_same = (drvindex == s_config->driver_index);
		else
			is_same = (drvindex < 0);
		if (is_same)
			return;
	}

	mvmap.clear();
	slmap.clear();

	// free the machine config, if necessary
	MySoftwareListClose();

	// allocate the machine config, if necessary
	if (drvindex >= 0)
		s_config = software_config_alloc(drvindex);

	// locate key widgets
	HWND hwndSoftwarePicker = GetDlgItem(GetMainWindow(), IDC_SWLIST);
	HWND hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);
	HWND hwndSoftwareDevView = GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW);

	// set up the device view
	DevView_SetDriver(hwndSoftwareDevView, s_config);

	// set up the software picker
	SoftwarePicker_Clear(hwndSoftwarePicker);
	SoftwarePicker_SetDriver(hwndSoftwarePicker, s_config);

	// set up the Software Files by using swpath (can handle multiple paths)
	std::string paths = ProcessSWDir(drvindex);
	const char* t1 = paths.c_str();
	if (t1[0] == '1')
	{
		paths.erase(0,1);
		AddSoftwarePickerDirs(hwndSoftwarePicker, paths.c_str(), NULL);
	}

	// set up the Software List
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
			for (const device_image_interface &image : image_interface_iterator(config.root_device()))
			{
				if (!image.user_loadable())
					continue;
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
	MyFillSoftwareList(Picker_GetSelectedItem(hwndList), true);
}


// Places the specified image in the specified slot - MUST be a valid filename, not blank
static void MessSpecifyImage(int drvindex, const device_image_interface *dev, LPCSTR pszFilename)
{
	if (dev)
	{
		SetSelectedSoftware(drvindex, dev, pszFilename);
		return;
	}

	std::string opt_name;
	device_image_interface* img = 0;

	if (LOG_SOFTWARE)
		printf("MessSpecifyImage(): device=%p pszFilename='%s'\n", dev, pszFilename);

	// identify the file extension
	const char *file_extension = strrchr(pszFilename, '.'); // find last period
	file_extension = file_extension ? file_extension + 1 : NULL; // if found bump pointer to first letter of the extension; if not found return NULL

	if (file_extension)
	{
		for (device_image_interface &dev : image_interface_iterator(s_config->mconfig->root_device()))
		{
			if (!dev.user_loadable())
				continue;
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
	}
	else
	{
		// could not place the image
		if (LOG_SOFTWARE)
			printf("MessSpecifyImage(): Failed to place image '%s'\n", pszFilename);
	}
}


// This is pointless because clicking on a new item overwrites the old one anyway
static void MessRemoveImage(int drvindex, const char *pszFilename)
{
#if 0
	const char *s;
	windows_options o;
	const char* name = driver_list::driver(drvindex).name;
	o.set_value(OPTION_SYSTEMNAME, name, OPTION_PRIORITY_CMDLINE);
	load_options(o, OPTIONS_GAME, drvindex);

	for (device_image_interface &dev : image_interface_iterator(s_config->mconfig->root_device()))
	{
		if (!dev.user_loadable())
			continue;
		// search through all the slots looking for a matching software name and unload it
		std::string opt_name = dev.instance_name();
		s = o.value(opt_name.c_str());
		if (s && (strcmp(pszFilename, s)==0))
			SetSelectedSoftware(drvindex, dev, NULL);
	}
#endif
}


void MessReadMountedSoftware(int drvindex)
{
	// First read stuff into device view
	if (TabView_GetCurrentTab(GetDlgItem(GetMainWindow(), IDC_SWTAB))==1)
		DevView_Refresh(GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW));

	// Now read stuff into picker
	if (TabView_GetCurrentTab(GetDlgItem(GetMainWindow(), IDC_SWTAB))==0)
		MessRefreshPicker();
}


static void MessRefreshPicker(void)
{
	HWND hwndSoftware = GetDlgItem(GetMainWindow(), IDC_SWLIST);

	s_bIgnoreSoftwarePickerNotifies = true;

	// Now clear everything out; this may call back into us but it should not be problematic
	ListView_SetItemState(hwndSoftware, -1, 0, LVIS_SELECTED);

	int i = 0;
	LVFINDINFO lvfi;
	const char *s;
	windows_options o;
	const char* name = driver_list::driver(s_config->driver_index).name;
	o.set_value(OPTION_SYSTEMNAME, name, OPTION_PRIORITY_CMDLINE);
	load_options(o, OPTIONS_GAME, s_config->driver_index);
	for (device_image_interface &dev : image_interface_iterator(s_config->mconfig->root_device()))
	{
		if (!dev.user_loadable())
			continue;
		std::string opt_name = dev.instance_name(); // get name of device slot
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

	s_bIgnoreSoftwarePickerNotifies = false;
}


// ------------------------------------------------------------------------
// Open others dialog
// ------------------------------------------------------------------------

static BOOL CommonFileImageDialog(LPTSTR the_last_directory, common_file_dialog_proc cfd, LPTSTR filename, const machine_config *config, mess_image_type *imagetypes)
{
	int i;

	std::string name = "Common image types (", ext = "|";
	// Common image types
	for (i = 0; imagetypes[i].ext; i++)
	{
		ext.append("*.").append(imagetypes[i].ext).append(";");
		name.append(imagetypes[i].ext).append(",");
	}
	i = name.size();
	name.erase(i-1); // remove trailing comma
	name.append(")").append(ext);
	i = name.size();
	name.erase(i-1); // remove trailing semi-colon
	name.append("|");

	// All files
	name.append("All files (*.*)|*.*|");

	// The others
	for (i = 0; imagetypes[i].ext; i++)
	{
		if (!imagetypes[i].dev)
			name.append("Compressed images");
		else
			name.append(imagetypes[i].dlgname);

		name.append(" (").append(imagetypes[i].ext).append(")|*.").append(imagetypes[i].ext).append("|");
	}

	TCHAR* t_buffer = ui_wstring_from_utf8(name.c_str());
	if( !t_buffer )
		return false;

	// convert a pipe-char delimited string into a NUL delimited string
	TCHAR* t_filter = (LPTSTR) alloca((_tcslen(t_buffer) + 2) * sizeof(*t_filter));
	for (i = 0; t_buffer[i] != '\0'; i++)
		t_filter[i] = (t_buffer[i] != '|') ? t_buffer[i] : '\0';
	t_filter[i++] = '\0';
	t_filter[i++] = '\0';
	free(t_buffer);

	*filename = 0;
	OPENFILENAME of;
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

	BOOL success = cfd(&of);
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
#if 0
	// Nobody used this so we will usurp it to just exit instead
	{
		/* special case; all non-printer devices */
		for (device_image_interface &device : image_interface_iterator(s_config->mconfig->root_device()))
		{
			if (!device.user_loadable())
				continue;
			if (device.image_type() != IO_PRINTER)
				SetupImageTypes(config, &types[num_extensions], count - num_extensions, false, &device);
		}
	}
#endif
		return; // used by DevView_MountItem
	else
	{
		char t1[256];
		strcpy(t1, dev->file_extensions());
		char *ext = strtok(t1,",");
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
	std::string dst = GetSWDir();
	// We only want the first path; throw out the rest
	size_t i = dst.find(';');
	if (i != std::string::npos)
		dst.substr(0, i);
	wchar_t* t_s = ui_wstring_from_utf8(dst.c_str());

	//  begin_resource_tracking();
	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	int drvindex = Picker_GetSelectedItem(hwndList);

	// allocate the machine config
	machine_config config(driver_list::driver(drvindex), MameUIGlobal());

	mess_image_type imagetypes[256];
	SetupImageTypes(&config, imagetypes, ARRAY_LENGTH(imagetypes), true, dev);
	TCHAR filename[MAX_PATH];
	BOOL bResult = CommonFileImageDialog(t_s, cfd, filename, &config, imagetypes);
	free(t_s);
	CleanupImageTypes(imagetypes, ARRAY_LENGTH(imagetypes));

	if (bResult)
	{
		char* utf8_filename = ui_utf8_from_wstring(filename);
		if( !utf8_filename )
			return;

		SoftwarePicker_AddFile(GetDlgItem(GetMainWindow(), IDC_SWLIST), utf8_filename, 0);
		free(utf8_filename);
	}
}


// This is used to Unmount a file from the Media View.
// Unused fields: hwndDevView, config, pszFilename, nFilenameLength
static BOOL DevView_Unmount(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength)
{
	int drvindex = Picker_GetSelectedItem(GetDlgItem(GetMainWindow(), IDC_LIST));

	SetSelectedSoftware(drvindex, dev, "");
	mvmap[dev->instance_name()] = 0;
	return true;
}


// This is used to Mount an existing software File in the Media View
static BOOL DevView_GetOpenFileName(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength)
{
	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	int drvindex = Picker_GetSelectedItem(hwndList);
	std::string dst, opt_name = dev->instance_name();
	windows_options o;
	const char* name = driver_list::driver(drvindex).name;
	o.set_value(OPTION_SYSTEMNAME, name, OPTION_PRIORITY_CMDLINE); // required
	load_options(o, OPTIONS_GAME, drvindex);
	const char* s = o.value(opt_name.c_str());

	/* Get the path to the currently mounted image */
	util::zippath_parent(dst, s);
	if ((!osd::directory::open(dst.c_str())) || (dst.find(':') == std::string::npos))
	{
		// no image loaded, use swpath
		dst = ProcessSWDir(drvindex);
		dst.erase(0,1);
		/* We only want the first path; throw out the rest */
		size_t i = dst.find(';');
		if (i != std::string::npos)
			dst.substr(0, i);
	}

	mess_image_type imagetypes[256];
	SetupImageTypes(config, imagetypes, ARRAY_LENGTH(imagetypes), true, dev);
	TCHAR *t_s = ui_wstring_from_utf8(dst.c_str());
	BOOL bResult = CommonFileImageDialog(t_s, GetOpenFileName, pszFilename, config, imagetypes);
	free(t_s);
	CleanupImageTypes(imagetypes, ARRAY_LENGTH(imagetypes));

	if (bResult)
	{
		char t2[nFilenameLength];
		wcstombs(t2, pszFilename, nFilenameLength-1); // convert wide string to a normal one
		SetSelectedSoftware(drvindex, dev, t2);
		mvmap[opt_name] = 1;
	}

	return bResult;
}


// This is used to Mount a software-list Item in the Media View.
static BOOL DevView_GetOpenItemName(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength)
{
	std::string opt_name = dev->instance_name();
	// sanity check - should never happen
	if (slmap.find(opt_name) == slmap.end())
	{
		printf ("DevView_GetOpenItemName used invalid device of %s\n", opt_name.c_str());
		return false;
	}

	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	int drvindex = Picker_GetSelectedItem(hwndList);

	std::string dst = slmap.find(opt_name)->second;

	if (!osd::directory::open(dst.c_str()))
		// Default to emu directory
		osd_get_full_path(dst, ".");

	mess_image_type imagetypes[256];
	SetupImageTypes(config, imagetypes, ARRAY_LENGTH(imagetypes), true, NULL); // just get zip & 7z
	TCHAR *t_s = ui_wstring_from_utf8(dst.c_str());
	BOOL bResult = CommonFileImageDialog(t_s, GetOpenFileName, pszFilename, config, imagetypes);
	free(t_s);
	CleanupImageTypes(imagetypes, ARRAY_LENGTH(imagetypes));

	if (bResult)
	{
		// Get the Item name out of the full path
		char t2[nFilenameLength];
		wcstombs(t2, pszFilename, nFilenameLength-1); // convert wide string to a normal one
		std::string t3 = t2; // then convert to a c++ string so we can manipulate it
		size_t i = t3.find(".zip"); // get rid of zip name and anything after
		if (i != std::string::npos)
			t3.erase(i);
		else
		{
			i = t3.find(".7z"); // get rid of 7zip name and anything after
			if (i != std::string::npos)
				t3.erase(i);
		}
		i = t3.find_last_of("\\");   // put the swlist name in
		t3[i] = ':';
		i = t3.find_last_of("\\"); // get rid of path; we only want the item name
		t3.erase(0, i+1);

		// set up editbox display text
		mbstowcs(pszFilename, t3.c_str(), nFilenameLength-1); // convert it back to a wide string

		// set up inifile text to signify to MAME that a SW ITEM is to be used
		SetSelectedSoftware(drvindex, dev, t3.c_str());
		mvmap[opt_name] = 1;
	}

	return bResult;
}


// This is used to Create an image in the Media View.
static BOOL DevView_GetCreateFileName(HWND hwndDevView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength)
{
	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	int drvindex = Picker_GetSelectedItem(hwndList);

	std::string dst = ProcessSWDir(drvindex);
	dst.erase(0,1);
	/* We only want the first path; throw out the rest */
	size_t i = dst.find(';');
	if (i != std::string::npos)
		dst.substr(0, i);

	TCHAR *t_s = ui_wstring_from_utf8(dst.c_str());
	mess_image_type imagetypes[256];
	SetupImageTypes(config, imagetypes, ARRAY_LENGTH(imagetypes), true, dev);
	BOOL bResult = CommonFileImageDialog(t_s, GetSaveFileName, pszFilename, config, imagetypes);
	free(t_s);
	CleanupImageTypes(imagetypes, ARRAY_LENGTH(imagetypes));

	if (bResult)
	{
		char t2[nFilenameLength];
		wcstombs(t2, pszFilename, nFilenameLength-1); // convert wide string to a normal one
		SetSelectedSoftware(drvindex, dev, t2);
		mvmap[dev->instance_name()] = 1;
	}

	return bResult;
}


// Unused fields: hwndDevView, config
static void DevView_SetSelectedSoftware(HWND hwndDevView, int drvindex, const machine_config *config, const device_image_interface *dev, LPCTSTR pszFilename)
{
	char* utf8_filename = ui_utf8_from_wstring(pszFilename);
	if( !utf8_filename )
		return;
	MessSpecifyImage(drvindex, dev, utf8_filename);
	free(utf8_filename);
	MessRefreshPicker();
}


// Unused fields: config
static LPCTSTR DevView_GetSelectedSoftware(HWND hwndDevView, int nDriverIndex, const machine_config *config, const device_image_interface *dev, LPTSTR pszBuffer, UINT nBufferLength)
{
	windows_options o;
	std::string opt_name = dev->instance_name();
	const char* name = driver_list::driver(nDriverIndex).name;
	o.set_value(OPTION_SYSTEMNAME, name, OPTION_PRIORITY_CMDLINE);
	load_options(o, OPTIONS_GAME, nDriverIndex);
	//const char* temp = o.value(opt_name.c_str());
	const std::string temp = o.image_option(opt_name).value();

	if (!temp.empty())
	{
		TCHAR* t_s = ui_wstring_from_utf8(temp.c_str());
		if( t_s )
		{
			_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_s);
			free(t_s);
			LPCTSTR t_buffer = pszBuffer;
			mvmap[opt_name] = 1;
			return t_buffer;
		}
	}

	mvmap[opt_name] = 0;
	return ui_wstring_from_utf8(""); // nothing loaded or error occurred
}


// ------------------------------------------------------------------------
// Software Picker Class
// ------------------------------------------------------------------------

static int SoftwarePicker_GetItemImage(HWND hwndPicker, int nItem)
{
	HWND hwndGamePicker = GetDlgItem(GetMainWindow(), IDC_LIST);
	HWND hwndSoftwarePicker = GetDlgItem(GetMainWindow(), IDC_SWLIST);
	int drvindex = Picker_GetSelectedItem(hwndGamePicker);
	iodevice_t nType = SoftwarePicker_GetImageType(hwndSoftwarePicker, nItem);
	int nIcon = GetMessIcon(drvindex, nType);
	if (!nIcon)
	{
		switch(nType)
		{
			case IO_UNKNOWN:
				nIcon = FindIconIndex(IDI_WIN_REDX);
				break;

			default:
				const char *icon_name = lookupdevice(nType)->icon_name;
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
	if (!s_bIgnoreSoftwarePickerNotifies)
	{
		HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
		int drvindex = Picker_GetSelectedItem(hwndList);
		const char *pszFullName = SoftwarePicker_LookupFilename(hwndSoftwarePicker, nItem);

		MessRemoveImage(drvindex, pszFullName);
	}
}


static void SoftwarePicker_EnteringItem(HWND hwndSoftwarePicker, int nItem)
{
	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);

	if (!s_bIgnoreSoftwarePickerNotifies)
	{
		int drvindex = Picker_GetSelectedItem(hwndList);

		// Get the fullname and partialname for this file
		LPCSTR pszFullName = SoftwarePicker_LookupFilename(hwndSoftwarePicker, nItem);
		const char* tmp = strrchr(pszFullName, '\\');
		LPCSTR pszName = tmp ? tmp + 1 : pszFullName;

		// Do the dirty work
		MessSpecifyImage(drvindex, NULL, pszFullName);

		// Set up g_szSelectedItem, for the benefit of UpdateScreenShot()
		strncpyz(g_szSelectedItem, pszName, ARRAY_LENGTH(g_szSelectedItem));
		LPSTR s = strrchr(g_szSelectedItem, '.');
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
	HWND hwndGamePicker = GetDlgItem(GetMainWindow(), IDC_LIST);
	HWND hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);
	int drvindex = Picker_GetSelectedItem(hwndGamePicker);
	iodevice_t nType = SoftwareList_GetImageType(hwndSoftwareList, nItem);
	int nIcon = GetMessIcon(drvindex, nType);
	if (!nIcon)
	{
		switch(nType)
		{
			case IO_UNKNOWN:
				nIcon = FindIconIndex(IDI_WIN_REDX);
				break;

			default:
				const char *icon_name = lookupdevice(nType)->icon_name;
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


// what does drvindex do here?
static void SoftwareList_EnteringItem(HWND hwndSoftwareList, int nItem)
{
	//HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);

	if (!s_bIgnoreSoftwarePickerNotifies)
	{
		//int drvindex = Picker_GetSelectedItem(hwndList);

		// Get the fullname for this file
		LPCSTR pszFullName = SoftwareList_LookupFullname(hwndSoftwareList, nItem); // for the screenshot

		// These 2 over to winui to load the SL item
		strncpyz(g_szSelectedSoftware, pszFullName, ARRAY_LENGTH(g_szSelectedSoftware));
		strncpyz(g_szSelectedDevice, SoftwareList_LookupDevice(hwndSoftwareList, nItem), ARRAY_LENGTH(g_szSelectedDevice));

		// For UpdateScreenShot()
		strncpyz(g_szSelectedItem, pszFullName, ARRAY_LENGTH(g_szSelectedItem));
		UpdateScreenShot();
	}
}


// ------------------------------------------------------------------------
// Header context menu stuff
// ------------------------------------------------------------------------

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
	int nColumnCount = Picker_GetColumnCount(MyColumnDialogProc_hwndPicker);
	const LPCTSTR *ppszColumnNames = Picker_GetColumnNames(MyColumnDialogProc_hwndPicker);

	INT_PTR result = InternalColumnDialogProc(hDlg, Msg, wParam, lParam, nColumnCount,
		MyColumnDialogProc_shown, MyColumnDialogProc_order, ppszColumnNames,
		MyGetRealColumnOrder, MyGetColumnInfo, MySetColumnInfo);

	return result;
}


static BOOL RunColumnDialog(HWND hwndPicker)
{
	MyColumnDialogProc_hwndPicker = hwndPicker;
	int nColumnCount = Picker_GetColumnCount(MyColumnDialogProc_hwndPicker);

	MyColumnDialogProc_order = (int*)alloca(nColumnCount * sizeof(int));
	MyColumnDialogProc_shown = (int*)alloca(nColumnCount * sizeof(int));
	BOOL bResult = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_COLUMNS), GetMainWindow(), MyColumnDialogProc);
	return bResult;
}


static void SoftwarePicker_OnHeaderContextMenu(POINT pt, int nColumn)
{
	HMENU hMenuLoad = LoadMenu(NULL, MAKEINTRESOURCE(IDR_CONTEXT_HEADER));
	HMENU hMenu = GetSubMenu(hMenuLoad, 0);

	int nMenuItem = (int) TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, GetMainWindow(), NULL);

	DestroyMenu(hMenuLoad);

	HWND hwndPicker = GetDlgItem(GetMainWindow(), IDC_SWLIST);

	switch(nMenuItem)
	{
		case ID_SORT_ASCENDING:
			SetSWSortReverse(false);
			SetSWSortColumn(Picker_GetRealColumnFromViewColumn(hwndPicker, nColumn));
			Picker_Sort(hwndPicker);
			break;

		case ID_SORT_DESCENDING:
			SetSWSortReverse(true);
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
	HMENU hMenuLoad = LoadMenu(NULL, MAKEINTRESOURCE(IDR_CONTEXT_HEADER));
	HMENU hMenu = GetSubMenu(hMenuLoad, 0);

	int nMenuItem = (int) TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, GetMainWindow(), NULL);

	DestroyMenu(hMenuLoad);

	HWND hwndPicker = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);

	switch(nMenuItem)
	{
		case ID_SORT_ASCENDING:
			SetSLSortReverse(false);
			SetSLSortColumn(Picker_GetRealColumnFromViewColumn(hwndPicker, nColumn));
			Picker_Sort(hwndPicker);
			break;

		case ID_SORT_DESCENDING:
			SetSLSortReverse(true);
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
			MessSetupDevice(GetOpenFileName, NULL);
			break;

	}
	return false;
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
	HWND hwndSoftwarePicker = GetDlgItem(GetMainWindow(), IDC_SWLIST);
	HWND hwndSoftwareDevView = GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW);
	HWND hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);

	int nTab = TabView_GetCurrentTab(GetDlgItem(GetMainWindow(), IDC_SWTAB));

	switch(nTab)
	{
		case 0:
			ShowWindow(hwndSoftwarePicker, SW_SHOW);
			ShowWindow(hwndSoftwareDevView, SW_HIDE);
			ShowWindow(hwndSoftwareList, SW_HIDE);
			//MessRefreshPicker(); // crashes MESSUI at start
			break;

		case 1:
			ShowWindow(hwndSoftwarePicker, SW_HIDE);
			ShowWindow(hwndSoftwareDevView, SW_SHOW);
			ShowWindow(hwndSoftwareList, SW_HIDE);
			DevView_Refresh(GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW));
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
	RECT rMain, rSoftwareTabView, rClient, rTab;
	BOOL res = 0;

	HWND hwndSoftwareTabView = GetDlgItem(GetMainWindow(), IDC_SWTAB);
	HWND hwndSoftwarePicker = GetDlgItem(GetMainWindow(), IDC_SWLIST);
	HWND hwndSoftwareDevView = GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW);
	HWND hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);

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
	MoveWindow(hwndSoftwarePicker, rClient.left, rClient.top, rClient.right - rClient.left, rClient.bottom - rClient.top, true);
	MoveWindow(hwndSoftwareDevView, rClient.left + 3, rClient.top + 2, rClient.right - rClient.left - 6, rClient.bottom - rClient.top -4, true);
	MoveWindow(hwndSoftwareList, rClient.left, rClient.top, rClient.right - rClient.left, rClient.bottom - rClient.top, true);
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


static void DevView_ButtonClick(HWND hwndDevView, struct DevViewEntry *pEnt, HWND hwndButton)
{
	struct DevViewInfo *pDevViewInfo;
	RECT r;
	BOOL has_software = false, passes_tests = false;
	TCHAR szPath[MAX_PATH];
	std::string opt_name = pEnt->dev->instance_name();
	pDevViewInfo = GetDevViewInfo(hwndDevView);

	HMENU hMenu = CreatePopupMenu();

	for (software_list_device &swlist : software_list_device_iterator(pDevViewInfo->config->mconfig->root_device()))
	{
		for (const software_info &swinfo : swlist.get_info())
		{
			const software_part &part = swinfo.parts().front();
			if (swlist.is_compatible(part) == SOFTWARE_IS_COMPATIBLE)
			{
				for (device_image_interface &image : image_interface_iterator(pDevViewInfo->config->mconfig->root_device()))
				{
					if (!image.user_loadable())
						continue;
					if (!has_software && (opt_name == image.instance_name()))
					{
						const char *interface = image.image_interface();
						if (interface && part.matches_interface(interface))
						{
							slmap[opt_name] = swlist.list_name();
							has_software = true;
						}
					}
				}
			}
		}
	}

	if (has_software)
	{
		std::string as, dst;
		char rompath[512];
		strcpy(rompath, GetRomDirs());
		char* sl_root = strtok(rompath, ";");
		while (sl_root && !passes_tests)
		{
			as = sl_root + std::string("\\") + slmap.find(opt_name)->second;
			TCHAR *szPath = ui_wstring_from_utf8(as.c_str());
			DWORD dwAttrib = GetFileAttributes(szPath);
			if ((dwAttrib != INVALID_FILE_ATTRIBUTES) && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
				passes_tests = true;

			free(szPath);

			sl_root = strtok(NULL, ";");
		}
		if (passes_tests)
			slmap[opt_name] = as;
	}

	if (pDevViewInfo->pCallbacks->pfnGetOpenFileName)
		AppendMenu(hMenu, MF_STRING, 1, TEXT("Mount File..."));

	if (passes_tests && pDevViewInfo->pCallbacks->pfnGetOpenItemName)
		AppendMenu(hMenu, MF_STRING, 4, TEXT("Mount Item..."));

	if (pEnt->dev->is_creatable())
	{
		if (pDevViewInfo->pCallbacks->pfnGetCreateFileName)
			AppendMenu(hMenu, MF_STRING, 2, TEXT("Create..."));
	}

	if (mvmap.find(opt_name)->second == 1)
		AppendMenu(hMenu, MF_STRING, 3, TEXT("Unmount"));

	GetWindowRect(hwndButton, &r);
	int rc = TrackPopupMenu(hMenu, TPM_TOPALIGN | TPM_NONOTIFY | TPM_RETURNCMD, r.left, r.bottom, 0, hwndDevView, NULL);
	BOOL res = false;
	switch(rc)
	{
		case 1:
			res = pDevViewInfo->pCallbacks->pfnGetOpenFileName(hwndDevView, pDevViewInfo->config->mconfig, pEnt->dev, szPath, ARRAY_LENGTH(szPath));
			break;
		case 2:
			res = pDevViewInfo->pCallbacks->pfnGetCreateFileName(hwndDevView, pDevViewInfo->config->mconfig, pEnt->dev, szPath, ARRAY_LENGTH(szPath));
			break;
		case 3:
			res = pDevViewInfo->pCallbacks->pfnUnmount(hwndDevView, pDevViewInfo->config->mconfig, pEnt->dev, szPath, ARRAY_LENGTH(szPath));
			memset(szPath, 0, sizeof(szPath));
			break;
		case 4:
			res = pDevViewInfo->pCallbacks->pfnGetOpenItemName(hwndDevView, pDevViewInfo->config->mconfig, pEnt->dev, szPath, ARRAY_LENGTH(szPath));
			break;
		default:
			break;
	}
	if (res)
		SetWindowText(pEnt->hwndEdit, szPath);
}


static BOOL DevView_Setup(HWND hwndDevView)
{
	struct DevViewInfo *pDevViewInfo;

	// allocate the device view info
	pDevViewInfo = (DevViewInfo*)malloc(sizeof(struct DevViewInfo));
	if (!pDevViewInfo)
		return false;
	memset(pDevViewInfo, 0, sizeof(*pDevViewInfo));

	SetWindowLongPtr(hwndDevView, GWLP_USERDATA, (LONG_PTR) pDevViewInfo);

	// create and specify the font
	pDevViewInfo->hFont = CreateFont(10, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("MS Sans Serif"));
	SendMessage(hwndDevView, WM_SETFONT, (WPARAM) pDevViewInfo->hFont, false);
	return true;
}


static void DevView_Free(HWND hwndDevView)
{
	struct DevViewInfo *pDevViewInfo;

	DevView_Clear(hwndDevView);
	pDevViewInfo = GetDevViewInfo(hwndDevView);
	DeleteObject(pDevViewInfo->hFont);
	free(pDevViewInfo);
}


static LRESULT CALLBACK DevView_WndProc(HWND hwndDevView, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	struct DevViewInfo *pDevViewInfo;
	struct DevViewEntry *pEnt;
	int nStaticPos, nStaticWidth, nEditPos, nEditWidth, nButtonPos, nButtonWidth;
	RECT r;
	LONG_PTR l = 0;
	LRESULT rc = 0;
	BOOL bHandled = false;
	HWND hwndButton;

	switch(nMessage)
	{
		case WM_DESTROY:
			DevView_Free(hwndDevView);
			break;

		case WM_SHOWWINDOW:
			if (wParam)
			{
				pDevViewInfo = GetDevViewInfo(hwndDevView);
				DevView_Refresh(hwndDevView);
			}
			break;

		case WM_COMMAND:
			switch(HIWORD(wParam))
			{
				case BN_CLICKED:
					hwndButton = (HWND) lParam;
					l = GetWindowLongPtr(hwndButton, GWLP_USERDATA);
					pEnt = (struct DevViewEntry *) l;
					DevView_ButtonClick(hwndDevView, pEnt, hwndButton);
					break;
			}
			break;
	}

	if (!bHandled)
		rc = DefWindowProc(hwndDevView, nMessage, wParam, lParam);

	switch(nMessage)
	{
		case WM_CREATE:
			if (!DevView_Setup(hwndDevView))
				return -1;
			break;

		case WM_MOVE:
		case WM_SIZE:
			pDevViewInfo = GetDevViewInfo(hwndDevView);
			pEnt = pDevViewInfo->pEntries;
			if (pEnt)
			{
				DevView_GetColumns(hwndDevView, &nStaticPos, &nStaticWidth,
					&nEditPos, &nEditWidth, &nButtonPos, &nButtonWidth);
				while(pEnt->dev)
				{
					GetClientRect(pEnt->hwndStatic, &r);
					MapWindowPoints(pEnt->hwndStatic, hwndDevView, ((POINT *) &r), 2);
					//MoveWindow(pEnt->hwndStatic, nStaticPos, r.top, nStaticWidth, r.bottom - r.top, false); // has its own line, no need to move
					// On next line, so need DEVVIEW_SPACING to put them down there.
					MoveWindow(pEnt->hwndEdit, nEditPos, r.top+DEVVIEW_SPACING, nEditWidth, r.bottom - r.top, false);
					MoveWindow(pEnt->hwndBrowseButton, nButtonPos, r.top+DEVVIEW_SPACING, nButtonWidth, r.bottom - r.top, false);
					pEnt++;
				}
			}
			break;
	}
	return rc;
}


void DevView_RegisterClass(void)
{
	WNDCLASS wc;
	memset(&wc, 0, sizeof(wc));
	wc.lpszClassName = TEXT("MessSoftwareDevView");
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpfnWndProc = DevView_WndProc;
	wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
	RegisterClass(&wc);
}

