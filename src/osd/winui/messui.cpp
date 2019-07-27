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

#define MVIEW_PADDING 10
#define MVIEW_SPACING 21
#define LOG_SOFTWARE 1


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


//============================================================
//  LOCAL VARIABLES
//============================================================

static software_config *s_config;
static BOOL s_bIgnoreSoftwarePickerNotifies = 0;
static HWND MyColumnDialogProc_hwndPicker;
static int *MyColumnDialogProc_order;
static int *MyColumnDialogProc_shown;
static int *mess_icon_index;
static std::map<string,string> slmap; // store folder for Media View Mount Item
static std::map<string,int> mvmap;  // store indicator if Media View Unmount should be enabled

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

static BOOL MVstate = 0;
static BOOL has_software = 0;

struct MViewCallbacks
{
	BOOL (*pfnGetOpenFileName)(HWND hwndMView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
	BOOL (*pfnGetCreateFileName)(HWND hwndMView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
	void (*pfnSetSelectedSoftware)(HWND hwndMView, int nGame, const machine_config *config, const device_image_interface *dev, LPCTSTR pszFilename);
	LPCTSTR (*pfnGetSelectedSoftware)(HWND hwndMView, int nGame, const machine_config *config, const device_image_interface *dev, LPTSTR pszBuffer, UINT nBufferLength);
	BOOL (*pfnGetOpenItemName)(HWND hwndMView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
	BOOL (*pfnUnmount)(HWND hwndMView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
};

struct MViewInfo
{
	HFONT hFont;
	UINT nWidth;
	UINT slots;
	BOOL bSurpressFilenameChanged;
	const software_config *config;
	const struct MViewCallbacks *pCallbacks;
	struct MViewEntry *pEntries;
};



struct MViewEntry
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
static void SoftwareTabView_OnMoveSize(void);
static void SetupSoftwareTabView(void);

static void MessRefreshPicker(void);

static BOOL MView_GetOpenFileName(HWND hwndMView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
static BOOL MView_GetOpenItemName(HWND hwndMView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
static BOOL MView_GetCreateFileName(HWND hwndMView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
static BOOL MView_Unmount(HWND hwndMView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength);
static void MView_SetSelectedSoftware(HWND hwndMView, int nDriverIndex, const machine_config *config, const device_image_interface *dev, LPCTSTR pszFilename);
static LPCTSTR MView_GetSelectedSoftware(HWND hwndMView, int nDriverIndex, const machine_config *config, const device_image_interface *dev, LPTSTR pszBuffer, UINT nBufferLength);



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


static struct MViewInfo *GetMViewInfo(HWND hwndMView)
{
	LONG_PTR l = GetWindowLongPtr(hwndMView, GWLP_USERDATA);
	return (struct MViewInfo *) l;
}


static void MView_SetCallbacks(HWND hwndMView, const struct MViewCallbacks *pCallbacks)
{
	struct MViewInfo *pMViewInfo;
	pMViewInfo = GetMViewInfo(hwndMView);
	pMViewInfo->pCallbacks = pCallbacks;
}


void InitMessPicker(void)
{
	struct PickerOptions opts;

	s_config = NULL;
	HWND hwndSoftware = GetDlgItem(GetMainWindow(), IDC_SWLIST);

	printf("InitMessPicker: A\n");fflush(stdout);
	memset(&opts, 0, sizeof(opts));
	opts.pCallbacks = &s_softwarePickerCallbacks;
	opts.nColumnCount = SW_COLUMN_MAX; // number of columns in picker
	opts.ppszColumnNames = mess_column_names; // get picker column names
	printf("InitMessPicker: B\n");fflush(stdout);
	SetupSoftwarePicker(hwndSoftware, &opts); // display them

	printf("InitMessPicker: C\n");fflush(stdout);
	SetWindowLong(hwndSoftware, GWL_STYLE, GetWindowLong(hwndSoftware, GWL_STYLE) | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_OWNERDRAWFIXED);

	printf("InitMessPicker: D\n");fflush(stdout);
	SetupSoftwareTabView();

	{
		static const struct MViewCallbacks s_MViewCallbacks =
		{
			MView_GetOpenFileName,
			MView_GetCreateFileName,
			MView_SetSelectedSoftware,
			MView_GetSelectedSoftware,
			MView_GetOpenItemName,
			MView_Unmount
		};
		MView_SetCallbacks(GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW), &s_MViewCallbacks);
	}

	HWND hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);

	printf("InitMessPicker: H\n");fflush(stdout);
	memset(&opts, 0, sizeof(opts));
	opts.pCallbacks = &s_softwareListCallbacks;
	opts.nColumnCount = SL_COLUMN_MAX; // number of columns in sw-list
	opts.ppszColumnNames = softlist_column_names; // columns for sw-list
	printf("InitMessPicker: I\n");fflush(stdout);
	SetupSoftwareList(hwndSoftwareList, &opts); // show them

	printf("InitMessPicker: J\n");fflush(stdout);
	SetWindowLong(hwndSoftwareList, GWL_STYLE, GetWindowLong(hwndSoftwareList, GWL_STYLE) | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_OWNERDRAWFIXED);
	printf("InitMessPicker: Finished\n");fflush(stdout);

	BOOL bShowSoftware = BIT(GetWindowPanes(), 2);
	int swtab = GetCurrentSoftwareTab();
	if (!bShowSoftware)
		swtab = -1;
	ShowWindow(GetDlgItem(GetMainWindow(), IDC_SWLIST), (swtab == 0) ? SW_SHOW : SW_HIDE);
	ShowWindow(GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW), (swtab == 1) ? SW_SHOW : SW_HIDE);
	ShowWindow(GetDlgItem(GetMainWindow(), IDC_SOFTLIST), (swtab == 2) ? SW_SHOW : SW_HIDE);
	ShowWindow(GetDlgItem(GetMainWindow(), IDC_SWTAB), bShowSoftware ? SW_SHOW : SW_HIDE);
	CheckMenuItem(GetMenu(GetMainWindow()), ID_VIEW_SOFTWARE_AREA, bShowSoftware ? MF_CHECKED : MF_UNCHECKED);
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
static string ProcessSWDir(int drvindex)
{
	if (drvindex < 0)
	{
		string dst;
		osd_get_full_path(dst,".");
		return string("1") + dst;
	}

	BOOL b_dir = false;
	char dir0[2048];
	char *t0 = 0;
	printf("ProcessSWDir: A\n");fflush(stdout);
	string t = GetSWDir();
	if (!t.empty())
	{
		printf("ProcessSWDir: B\n");fflush(stdout);
		strcpy(dir0, t.c_str()); // global SW
		t0 = strtok(dir0, ";");
		if (t0 && osd::directory::open(t0))  // make sure its valid
			b_dir = true;
	}

	// Get the game's software path
	printf("ProcessSWDir: C\n");fflush(stdout);
	windows_options o;
	load_options(o, OPTIONS_GAME, drvindex, 0);
	char dir1[2048];
	strcpy(dir1, o.value(OPTION_SWPATH));
	char* t1 = strtok(dir1, ";");
	printf("ProcessSWDir: D\n");fflush(stdout);
	if (t1 && osd::directory::open(t1))  // make sure its valid
		if (b_dir && (strcmp(t0, t1) != 0))
			return string("1") + string(dir1);

	// not specified in driver, try parent if it has one
	printf("ProcessSWDir: E\n");fflush(stdout);
	int nParentIndex = drvindex;
	if (DriverIsClone(drvindex))
	{
		printf("ProcessSWDir: F\n");fflush(stdout);
		nParentIndex = GetParentIndex(&driver_list::driver(drvindex));
		if (nParentIndex >= 0)
		{
			printf("ProcessSWDir: G\n");fflush(stdout);
			load_options(o, OPTIONS_PARENT, nParentIndex, 0);
			strcpy(dir1, o.value(OPTION_SWPATH));
			t1 = strtok(dir1, ";");
			printf("ProcessSWDir: GA = %s\n",dir1);fflush(stdout);
			if (t1 && osd::directory::open(t1))  // make sure its valid
			{
				printf("ProcessSWDir: GB\n");fflush(stdout);
				if (b_dir && (strcmp(t0, t1) != 0))
				{
					printf("ProcessSWDir: GC\n");fflush(stdout);
					return string("1") + string(dir1);
				}
			}
		}
		else
			nParentIndex = drvindex; // don't pass -1 to compat check
	}

	// Try compat if it has one
	printf("ProcessSWDir: H = %d\n",nParentIndex);fflush(stdout);
	int nCloneIndex = GetCompatIndex(&driver_list::driver(nParentIndex));
	printf("ProcessSWDir: HA = %d\n",nCloneIndex);fflush(stdout);
	if (nCloneIndex >= 0)
	{
		printf("ProcessSWDir: I\n");fflush(stdout);
		load_options(o, OPTIONS_PARENT, nCloneIndex, 0);
		strcpy(dir1, o.value(OPTION_SWPATH));
		t1 = strtok(dir1, ";");
		if (t1 && osd::directory::open(t1))  // make sure its valid
			if (b_dir && (strcmp(t0, t1) != 0))
				return string("1") + string(dir1);
	}

	// Try the global root
	printf("ProcessSWDir: J\n");fflush(stdout);
	if (b_dir)
		return string("0") + string(dir0);

	// nothing valid, drop to default emu directory
	printf("ProcessSWDir: K\n");fflush(stdout);
	string dst;
	osd_get_full_path(dst,".");
	printf("ProcessSWDir: L\n");fflush(stdout);
	return string("1") + dst;
}


// Split multi-directory for SW Files into separate directories, and ask the picker to add the files from each.
// pszSubDir path not used by any caller.
static BOOL AddSoftwarePickerDirs(HWND hwndPicker, LPCSTR pszDirectories, LPCSTR pszSubDir)
{
	if (!pszDirectories)
		return false;

	char s[2048];
	string pszNewString;
	strcpy(s, pszDirectories);
	LPSTR t1 = strtok(s,";");
	while (t1)
	{
		if (pszSubDir)
			pszNewString = t1 + string("\\") + pszSubDir;
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


void MView_Clear(HWND hwndMView)
{
	if (!hwndMView)
		return;
	struct MViewInfo *pMViewInfo;
	pMViewInfo = GetMViewInfo(hwndMView);

	if (pMViewInfo->pEntries)
	{
		for (int i = 0; pMViewInfo->pEntries[i].dev; i++)
		{
			DestroyWindow(pMViewInfo->pEntries[i].hwndStatic);
			DestroyWindow(pMViewInfo->pEntries[i].hwndEdit);
			DestroyWindow(pMViewInfo->pEntries[i].hwndBrowseButton);
		}
		free(pMViewInfo->pEntries);
		pMViewInfo->pEntries = NULL;
	}

	pMViewInfo->config = NULL;
	MVstate = 0;
	pMViewInfo->slots = 0;
}


static void MView_GetColumns(HWND hwndMView, int *pnStaticPos, int *pnStaticWidth, int *pnEditPos, int *pnEditWidth, int *pnButtonPos, int *pnButtonWidth)
{
	struct MViewInfo *pMViewInfo;

	pMViewInfo = GetMViewInfo(hwndMView);

	RECT r;
	GetClientRect(hwndMView, &r);

	*pnStaticPos = MVIEW_PADDING;
	*pnStaticWidth = pMViewInfo->nWidth;

	*pnButtonWidth = 25;
	*pnButtonPos = (r.right - r.left) - *pnButtonWidth - MVIEW_PADDING;

//	*pnEditPos = *pnStaticPos + *pnStaticWidth + MVIEW_PADDING;
	*pnEditPos = MVIEW_PADDING;
	*pnEditWidth = *pnButtonPos - *pnEditPos - MVIEW_PADDING;
	if (*pnEditWidth < 0)
		*pnEditWidth = 0;
}


static void MView_TextChanged(HWND hwndMView, int nChangedEntry, LPCTSTR pszFilename)
{
	struct MViewInfo *pMViewInfo;
	pMViewInfo = GetMViewInfo(hwndMView);
	if (!pMViewInfo->bSurpressFilenameChanged && pMViewInfo->pCallbacks->pfnSetSelectedSoftware)
		pMViewInfo->pCallbacks->pfnSetSelectedSoftware(hwndMView, pMViewInfo->config->driver_index,
			pMViewInfo->config->mconfig, pMViewInfo->pEntries[nChangedEntry].dev, pszFilename);
}


static LRESULT CALLBACK MView_EditWndProc(HWND hwndEdit, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	LONG_PTR l = GetWindowLongPtr(hwndEdit, GWLP_USERDATA);
	struct MViewEntry *pEnt;
	pEnt = (struct MViewEntry *) l;
	LRESULT rc;
	if (IsWindowUnicode(hwndEdit))
		rc = CallWindowProcW(pEnt->pfnEditWndProc, hwndEdit, nMessage, wParam, lParam);
	else
		rc = CallWindowProcA(pEnt->pfnEditWndProc, hwndEdit, nMessage, wParam, lParam);

	if (nMessage == WM_SETTEXT)
	{
		HWND hwndMView = GetParent(hwndEdit);
		struct MViewInfo *pMViewInfo;
		pMViewInfo = GetMViewInfo(hwndMView);
		MView_TextChanged(hwndMView, pEnt - pMViewInfo->pEntries, (LPCTSTR) lParam);
	}
	return rc;
}


void MView_Refresh(HWND hwndMView)
{
	if (!MVstate)
		return;

	struct MViewInfo *pMViewInfo;
	LPCTSTR pszSelection;
	TCHAR szBuffer[MAX_PATH];

	pMViewInfo = GetMViewInfo(hwndMView);
	printf("MView_Refresh: %s\n", driver_list::driver(pMViewInfo->config->driver_index).name);fflush(stdout);

	if (pMViewInfo->slots)
	{
		for (int i = 0; i < pMViewInfo->slots; i++)
		{
			pszSelection = pMViewInfo->pCallbacks->pfnGetSelectedSoftware( hwndMView, pMViewInfo->config->driver_index,
				pMViewInfo->config->mconfig, pMViewInfo->pEntries[i].dev, szBuffer, MAX_PATH);
			printf("MView_Refresh: Finished GetSelectSoftware\n");fflush(stdout);
			if (!pszSelection)
				pszSelection = TEXT("");

			pMViewInfo->bSurpressFilenameChanged = true;
			SetWindowText(pMViewInfo->pEntries[i].hwndEdit, pszSelection);
			pMViewInfo->bSurpressFilenameChanged = false;
		}
	}
}


//#ifdef __GNUC__
//#pragma GCC diagnostic ignored "-Wunused-variable"
//#endif
static BOOL MView_SetDriver(HWND hwndMView, const software_config *sconfig)
{
	struct MViewInfo *pMViewInfo;
	int i, y = 0, nHeight = 0, nStaticPos = 0, nStaticWidth = 0, nEditPos = 0, nEditWidth = 0, nButtonPos = 0, nButtonWidth = 0;
	pMViewInfo = GetMViewInfo(hwndMView);

	// clear out
	printf("MView_SetDriver: A\n");fflush(stdout);
	MView_Clear(hwndMView);

	// copy the config
	printf("MView_SetDriver: B\n");fflush(stdout);
	pMViewInfo->config = sconfig;

	if (!sconfig || !has_software)
		return false;

	// count total amount of devices
	printf("MView_SetDriver: C\n");fflush(stdout);
	for (device_image_interface &dev : image_interface_iterator(pMViewInfo->config->mconfig->root_device()))
		if (dev.user_loadable())
			pMViewInfo->slots++;

	printf("MView_SetDriver: Number of slots = %d\n", pMViewInfo->slots);fflush(stdout);

	if (pMViewInfo->slots)
	{
		// get the names of all of the media-slots so we can then work out how much space is needed to display them
		string instance;
		LPTSTR *ppszDevices;
		ppszDevices = (LPTSTR *) alloca(pMViewInfo->slots * sizeof(*ppszDevices));
		i = 0;
		for (device_image_interface &dev : image_interface_iterator(pMViewInfo->config->mconfig->root_device()))
		{
			if (!dev.user_loadable())
				continue;
			instance = dev.instance_name() + string(" (") + dev.brief_instance_name() + string(")");
			LPTSTR t_s = ui_wstring_from_utf8(instance.c_str());
			ppszDevices[i] = (TCHAR*)alloca((_tcslen(t_s) + 1) * sizeof(TCHAR));
			_tcscpy(ppszDevices[i], t_s);
			free(t_s);
			i++;
		}
		printf("MView_SetDriver: Number of media slots = %d\n", i);fflush(stdout);

		// Calculate the requisite size for the device column
		pMViewInfo->nWidth = 0;
		HDC hDc = GetDC(hwndMView);
		SIZE sz;
		for (i = 0; i < pMViewInfo->slots; i++)
		{
			GetTextExtentPoint32(hDc, ppszDevices[i], _tcslen(ppszDevices[i]), &sz);
			if (sz.cx > pMViewInfo->nWidth)
				pMViewInfo->nWidth = sz.cx;
		}
		ReleaseDC(hwndMView, hDc);

		struct MViewEntry *pEnt;
		pEnt = (struct MViewEntry *) malloc(sizeof(struct MViewEntry) * (pMViewInfo->slots + 1));
		if (!pEnt)
			return false;
		memset(pEnt, 0, sizeof(struct MViewEntry) * (pMViewInfo->slots + 1));
		pMViewInfo->pEntries = pEnt;

		y = MVIEW_PADDING;
		nHeight = MVIEW_SPACING;
		MView_GetColumns(hwndMView, &nStaticPos, &nStaticWidth, &nEditPos, &nEditWidth, &nButtonPos, &nButtonWidth);

		// Now actually display the media-slot names, and show the empty boxes and the browse button
		LONG_PTR l = 0;
		for (device_image_interface &dev : image_interface_iterator(pMViewInfo->config->mconfig->root_device()))
		{
			if (!dev.user_loadable())
				continue;
			pEnt->dev = &dev;
			instance = dev.instance_name() + string(" (") + dev.brief_instance_name() + string(")"); // get name of the slot (long and short)
			std::transform(instance.begin(), instance.begin()+1, instance.begin(), ::toupper); // turn first char to uppercase
			pEnt->hwndStatic = win_create_window_ex_utf8(0, "STATIC", instance.c_str(), // display it
				WS_VISIBLE | WS_CHILD, nStaticPos, y, nStaticWidth, nHeight, hwndMView, NULL, NULL, NULL);
			y += nHeight;

			pEnt->hwndEdit = win_create_window_ex_utf8(0, "EDIT", "",
				WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, nEditPos,
				y, nEditWidth, nHeight, hwndMView, NULL, NULL, NULL); // display blank edit box

			pEnt->hwndBrowseButton = win_create_window_ex_utf8(0, "BUTTON", "...",
				WS_VISIBLE | WS_CHILD, nButtonPos, y, nButtonWidth, nHeight, hwndMView, NULL, NULL, NULL); // display browse button

			if (pEnt->hwndStatic)
				SendMessage(pEnt->hwndStatic, WM_SETFONT, (WPARAM) pMViewInfo->hFont, true);

			if (pEnt->hwndEdit)
			{
				SendMessage(pEnt->hwndEdit, WM_SETFONT, (WPARAM) pMViewInfo->hFont, true);
				l = (LONG_PTR) MView_EditWndProc;
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

	if (!pMViewInfo->slots)
		return false;

	MVstate = 1;
	printf("MView_SetDriver: Calling MView_Refresh\n");fflush(stdout);
	MView_Refresh(hwndMView); // show names of already-loaded software
	printf("MView_SetDriver: Finished\n");fflush(stdout);
	return true;
}
//#ifdef __GNUC__
//#pragma GCC diagnostic error "-Wunused-variable"
//#endif


BOOL MyFillSoftwareList(int drvindex, BOOL bForce)
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
			return has_software;
	}

	mvmap.clear();
	slmap.clear();
	has_software = false;

	// free the machine config, if necessary
	MySoftwareListClose();

	// allocate the machine config, if necessary
	if (drvindex >= 0)
	{
		s_config = software_config_alloc(drvindex);
		has_software = DriverHasSoftware(drvindex);
	}

	// locate key widgets
	HWND hwndSoftwarePicker = GetDlgItem(GetMainWindow(), IDC_SWLIST);
	HWND hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);
	HWND hwndSoftwareMView = GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW);

	printf("MyFillSoftwareList: Calling SoftwarePicker_Clear\n");fflush(stdout);
	SoftwareList_Clear(hwndSoftwareList);
	printf("MyFillSoftwareList: Calling SoftwarePicker_Clear\n");fflush(stdout);
	SoftwarePicker_Clear(hwndSoftwarePicker);

	// set up the device view
	printf("MyFillSoftwareList: Calling MView_SetDriver\n");fflush(stdout);
	MView_SetDriver(hwndSoftwareMView, s_config);

	// set up the software picker
	printf("MyFillSoftwareList: Calling SoftwarePicker_SetDriver\n");fflush(stdout);
	SoftwarePicker_SetDriver(hwndSoftwarePicker, s_config);

	if (!s_config || !has_software)
		return has_software;

	// set up the Software Files by using swpath (can handle multiple paths)
	printf("MyFillSoftwareList: Processing SWDir\n");fflush(stdout);
	string paths = ProcessSWDir(drvindex);
	printf("MyFillSoftwareList: Finished SWDir = %s\n", paths.c_str());fflush(stdout);
	const char* t1 = paths.c_str();
	if (t1[0] == '1')
	{
		paths.erase(0,1);
		AddSoftwarePickerDirs(hwndSoftwarePicker, paths.c_str(), NULL);
	}

	// set up the Software List
	printf("MyFillSoftwareList: Calling SoftwarePicker_SetDriver\n");fflush(stdout);
	SoftwareList_SetDriver(hwndSoftwareList, s_config);

	printf("MyFillSoftwareList: Getting Softlist Information\n");fflush(stdout);
	for (software_list_device &swlistdev : software_list_device_iterator(s_config->mconfig->root_device()))
	{
		for (const software_info &swinfo : swlistdev.get_info())
		{
			const software_part &swpart = swinfo.parts().front();

			// search for a device with the right interface
			for (const device_image_interface &image : image_interface_iterator(s_config->mconfig->root_device()))
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
	printf("MyFillSoftwareList: Finished\n");fflush(stdout);
	return has_software;
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
		SetSelectedSoftware(drvindex, dev->instance_name(), pszFilename);
		return;
	}

	string opt_name;
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
		SetSelectedSoftware(drvindex, img->instance_name(), pszFilename);
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
	//o.set_value(OPTION_SYSTEMNAME, driver_list::driver(drvindex).name, OPTION_PRIORITY_CMDLINE);
	load_options(o, OPTIONS_GAME, drvindex, 1);

	for (device_image_interface &dev : image_interface_iterator(s_config->mconfig->root_device()))
	{
		if (!dev.user_loadable())
			continue;
		// search through all the slots looking for a matching software name and unload it
		string opt_name = dev.instance_name();
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
		MView_Refresh(GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW));

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

	// Get the game's options including slots & software
	windows_options o;
	load_options(o, OPTIONS_GAME, s_config->driver_index, 1);
	/* allocate the machine config */
	machine_config config(driver_list::driver(s_config->driver_index), o);

	for (device_image_interface &dev : image_interface_iterator(config.root_device()))
	{
		if (!dev.user_loadable())
			continue;
		string opt_name = dev.instance_name(); // get name of device slot
		s = o.value(opt_name.c_str()); // get name of software in the slot

		if (s[0]) // if software is loaded
		{
			i = SoftwarePicker_LookupIndex(hwndSoftware, s); // see if its in the picker
			if (i < 0) // not there
			{
				// add already loaded software to picker, but not if its already there
//				SoftwarePicker_AddFile(hwndSoftware, s, 1);    // this adds the 'extra' loaded software item into the list - we don't need to see this
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

	string name = "Common image types (", ext = "|";
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


char *core_strdup(const char *str)
{
	char *cpy = nullptr;
	if (str != nullptr)
	{
		cpy = (char*) malloc(strlen(str)+1);
		if (cpy != nullptr)
			strcpy(cpy, str);
	}
	return cpy;
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
		return; // used by MView_MountItem
	else
	{
		string exts = dev->file_extensions();
		char t1[exts.size()+1];
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
	string dst = GetSWDir();
	// We only want the first path; throw out the rest
	size_t i = dst.find(';');
	if (i != string::npos)
		dst.substr(0, i);
	wchar_t* t_s = ui_wstring_from_utf8(dst.c_str());

	//  begin_resource_tracking();
	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	int drvindex = Picker_GetSelectedItem(hwndList);
	if (drvindex < 0)
		return;

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
// Unused fields: hwndMView, config, pszFilename, nFilenameLength
static BOOL MView_Unmount(HWND hwndMView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength)
{
	int drvindex = Picker_GetSelectedItem(GetDlgItem(GetMainWindow(), IDC_LIST));

	if (drvindex < 0)
		return false;
	SetSelectedSoftware(drvindex, dev->instance_name(), "");
	mvmap[dev->instance_name()] = 0;
	return true;
}


// This is used to Mount an existing software File in the Media View
static BOOL MView_GetOpenFileName(HWND hwndMView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength)
{
	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	int drvindex = Picker_GetSelectedItem(hwndList);
	if (drvindex < 0)
		return false;
	string dst, opt_name = dev->instance_name();
	windows_options o;
	load_options(o, OPTIONS_GAME, drvindex, 1);
	const char* s = o.value(opt_name.c_str());

	/* Get the path to the currently mounted image */
	util::zippath_parent(dst, s);
	if ((!osd::directory::open(dst.c_str())) || (dst.find(':') == string::npos))
	{
		// no image loaded, use swpath
		dst = ProcessSWDir(drvindex);
		dst.erase(0,1);
		/* We only want the first path; throw out the rest */
		size_t i = dst.find(';');
		if (i != string::npos)
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
		SetSelectedSoftware(drvindex, dev->instance_name(), t2);
		mvmap[opt_name] = 1;
	}

	return bResult;
}


// This is used to Mount a software-list Item in the Media View.
static BOOL MView_GetOpenItemName(HWND hwndMView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength)
{
	string opt_name = dev->instance_name();
	// sanity check - should never happen
	if (slmap.find(opt_name) == slmap.end())
	{
		printf ("MView_GetOpenItemName used invalid device of %s\n", opt_name.c_str());
		return false;
	}

	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	int drvindex = Picker_GetSelectedItem(hwndList);
	if (drvindex < 0)
		return false;

	string dst = slmap.find(opt_name)->second;

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
		string t3 = t2; // then convert to a c++ string so we can manipulate it
		size_t i = t3.find(".zip"); // get rid of zip name and anything after
		if (i != string::npos)
			t3.erase(i);
		else
		{
			i = t3.find(".7z"); // get rid of 7zip name and anything after
			if (i != string::npos)
				t3.erase(i);
		}
		i = t3.find_last_of("\\");   // put the swlist name in
		t3[i] = ':';
		i = t3.find_last_of("\\"); // get rid of path; we only want the item name
		t3.erase(0, i+1);

		// set up editbox display text
		mbstowcs(pszFilename, t3.c_str(), nFilenameLength-1); // convert it back to a wide string

		// set up inifile text to signify to MAME that a SW ITEM is to be used
		SetSelectedSoftware(drvindex, dev->instance_name(), t3.c_str());
		mvmap[opt_name] = 1;
	}

	return bResult;
}


// This is used to Create an image in the Media View.
static BOOL MView_GetCreateFileName(HWND hwndMView, const machine_config *config, const device_image_interface *dev, LPTSTR pszFilename, UINT nFilenameLength)
{
	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);
	int drvindex = Picker_GetSelectedItem(hwndList);
	if (drvindex < 0)
		return false;

	string dst = ProcessSWDir(drvindex);
	dst.erase(0,1);
	/* We only want the first path; throw out the rest */
	size_t i = dst.find(';');
	if (i != string::npos)
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
		SetSelectedSoftware(drvindex, dev->instance_name(), t2);
		mvmap[dev->instance_name()] = 1;
	}

	return bResult;
}


// Unused fields: hwndMView, config
static void MView_SetSelectedSoftware(HWND hwndMView, int drvindex, const machine_config *config, const device_image_interface *dev, LPCTSTR pszFilename)
{
	char* utf8_filename = ui_utf8_from_wstring(pszFilename);
	if( !utf8_filename )
		return;
	MessSpecifyImage(drvindex, dev, utf8_filename);
	free(utf8_filename);
	MessRefreshPicker();
}


// Unused fields: config
static LPCTSTR MView_GetSelectedSoftware(HWND hwndMView, int nDriverIndex, const machine_config *config, const device_image_interface *dev, LPTSTR pszBuffer, UINT nBufferLength)
{
	BOOL res = false;
	string opt_name, opt_value;
	if (dev && dev->user_loadable())
	{
		windows_options o;
		load_options(o, OPTIONS_GAME, nDriverIndex, 1);
		printf("MView_GetSelectedSoftware: Got options\n");fflush(stdout);
		opt_name = dev->instance_name();
		//const char* temp = o.value(opt_name.c_str());
		if (o.has_image_option(opt_name))
			opt_value = o.image_option(opt_name).value().empty() ? "" : o.image_option(opt_name).value();
		printf("MView_GetSelectedSoftware: == %s : %s\n", opt_name.c_str(), opt_value.c_str());fflush(stdout);
		if (!opt_value.empty())
			res = true;
	}
	printf("MView_GetSelectedSoftware: Got values\n");fflush(stdout);
	if (res)
	{
		TCHAR* t_s = ui_wstring_from_utf8(opt_value.c_str());
		if( t_s )
		{
			_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_s);
			free(t_s);
			LPCTSTR t_buffer = pszBuffer;
			mvmap[opt_name] = 1;
			printf("MView_GetSelectedSoftware: Got %s\n", opt_value.c_str());fflush(stdout);
			return t_buffer;
		}
	}
	printf("MView_GetSelectedSoftware: Got nothing\n");fflush(stdout);
	if (!opt_name.empty())
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
	if (drvindex < 0)
		return -1;
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
		if (drvindex < 0)
			return;

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
		if (drvindex < 0)
		{
			g_szSelectedItem[0] = 0;
			return;
		}

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
	if (drvindex < 0)
		return -1;

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
		g_szSelectedItem[0] = 0;
}



static void SoftwareList_EnteringItem(HWND hwndSoftwareList, int nItem)
{
	HWND hwndList = GetDlgItem(GetMainWindow(), IDC_LIST);

	if (!s_bIgnoreSoftwarePickerNotifies)
	{
		int drvindex = Picker_GetSelectedItem(hwndList);
		if (drvindex < 0)
		{
			g_szSelectedItem[0] = 0;
			return;
		}

		// Get the fullname for this file
		LPCSTR pszFullName = SoftwareList_LookupFullname(hwndSoftwareList, nItem); // for the screenshot and SetSoftware.

		char t[100];
		strncpyz(t, SoftwareList_LookupDevice(hwndSoftwareList, nItem), ARRAY_LENGTH(t));
		string opt_name = t[0] ? t : "";

		// For UpdateScreenShot()
		strncpyz(g_szSelectedItem, pszFullName, ARRAY_LENGTH(g_szSelectedItem));
		UpdateScreenShot();
		SetSelectedSoftware(drvindex, opt_name, pszFullName);
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
	"MVIEW\0Media View",
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


void SoftwareTabView_OnSelectionChanged(void)
{
	HWND hwndSoftwarePicker = GetDlgItem(GetMainWindow(), IDC_SWLIST);
	HWND hwndSoftwareMView = GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW);
	HWND hwndSoftwareList = GetDlgItem(GetMainWindow(), IDC_SOFTLIST);

	int nTab = TabView_GetCurrentTab(GetDlgItem(GetMainWindow(), IDC_SWTAB));

	switch(nTab)
	{
		case 0:
			ShowWindow(hwndSoftwarePicker, SW_SHOW);
			ShowWindow(hwndSoftwareMView, SW_HIDE);
			ShowWindow(hwndSoftwareList, SW_HIDE);
			//MessRefreshPicker(); // crashes MESSUI at start
			break;

		case 1:
			ShowWindow(hwndSoftwarePicker, SW_HIDE);
			ShowWindow(hwndSoftwareMView, SW_SHOW);
			ShowWindow(hwndSoftwareList, SW_HIDE);
			MView_Refresh(GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW));
			break;
		case 2:
			ShowWindow(hwndSoftwarePicker, SW_HIDE);
			ShowWindow(hwndSoftwareMView, SW_HIDE);
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
	HWND hwndSoftwareMView = GetDlgItem(GetMainWindow(), IDC_SWDEVVIEW);
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
	MoveWindow(hwndSoftwareMView, rClient.left + 3, rClient.top + 2, rClient.right - rClient.left - 6, rClient.bottom - rClient.top -4, true);
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


static void MView_ButtonClick(HWND hwndMView, struct MViewEntry *pEnt, HWND hwndButton)
{
	struct MViewInfo *pMViewInfo;
	RECT r;
	BOOL software = false, passes_tests = false;
	TCHAR szPath[MAX_PATH];
	string opt_name = pEnt->dev->instance_name();
	pMViewInfo = GetMViewInfo(hwndMView);

	HMENU hMenu = CreatePopupMenu();

	for (software_list_device &swlist : software_list_device_iterator(pMViewInfo->config->mconfig->root_device()))
	{
		for (const software_info &swinfo : swlist.get_info())
		{
			const software_part &part = swinfo.parts().front();
			if (swlist.is_compatible(part) == SOFTWARE_IS_COMPATIBLE)
			{
				for (device_image_interface &image : image_interface_iterator(pMViewInfo->config->mconfig->root_device()))
				{
					if (!image.user_loadable())
						continue;
					if (!software && (opt_name == image.instance_name()))
					{
						const char *interface = image.image_interface();
						if (interface && part.matches_interface(interface))
						{
							slmap[opt_name] = swlist.list_name();
							software = true;
						}
					}
				}
			}
		}
	}

	if (software)
	{
		string as, dst;
		string t = GetRomDirs();
		char rompath[t.size()+1];
		strcpy(rompath, t.c_str());
		char* sl_root = strtok(rompath, ";");
		while (sl_root && !passes_tests)
		{
			as = sl_root + string("\\") + slmap.find(opt_name)->second;
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

	if (pMViewInfo->pCallbacks->pfnGetOpenFileName)
		AppendMenu(hMenu, MF_STRING, 1, TEXT("Mount File..."));

	if (passes_tests && pMViewInfo->pCallbacks->pfnGetOpenItemName)
		AppendMenu(hMenu, MF_STRING, 4, TEXT("Mount Item..."));

	if (pEnt->dev->is_creatable())
	{
		if (pMViewInfo->pCallbacks->pfnGetCreateFileName)
			AppendMenu(hMenu, MF_STRING, 2, TEXT("Create..."));
	}

	if (mvmap.find(opt_name)->second == 1)
		AppendMenu(hMenu, MF_STRING, 3, TEXT("Unmount"));

	GetWindowRect(hwndButton, &r);
	int rc = TrackPopupMenu(hMenu, TPM_TOPALIGN | TPM_NONOTIFY | TPM_RETURNCMD, r.left, r.bottom, 0, hwndMView, NULL);
	BOOL res = false;
	switch(rc)
	{
		case 1:
			res = pMViewInfo->pCallbacks->pfnGetOpenFileName(hwndMView, pMViewInfo->config->mconfig, pEnt->dev, szPath, ARRAY_LENGTH(szPath));
			break;
		case 2:
			res = pMViewInfo->pCallbacks->pfnGetCreateFileName(hwndMView, pMViewInfo->config->mconfig, pEnt->dev, szPath, ARRAY_LENGTH(szPath));
			break;
		case 3:
			res = pMViewInfo->pCallbacks->pfnUnmount(hwndMView, pMViewInfo->config->mconfig, pEnt->dev, szPath, ARRAY_LENGTH(szPath));
			memset(szPath, 0, sizeof(szPath));
			break;
		case 4:
			res = pMViewInfo->pCallbacks->pfnGetOpenItemName(hwndMView, pMViewInfo->config->mconfig, pEnt->dev, szPath, ARRAY_LENGTH(szPath));
			break;
		default:
			break;
	}
	if (res)
		SetWindowText(pEnt->hwndEdit, szPath);
}


static BOOL MView_Setup(HWND hwndMView)
{
	struct MViewInfo *pMViewInfo;

	// allocate the device view info
	pMViewInfo = (MViewInfo*)malloc(sizeof(struct MViewInfo));
	if (!pMViewInfo)
		return false;
	memset(pMViewInfo, 0, sizeof(*pMViewInfo));

	SetWindowLongPtr(hwndMView, GWLP_USERDATA, (LONG_PTR) pMViewInfo);

	// create and specify the font
	pMViewInfo->hFont = CreateFont(10, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("MS Sans Serif"));
	SendMessage(hwndMView, WM_SETFONT, (WPARAM) pMViewInfo->hFont, false);
	return true;
}


static void MView_Free(HWND hwndMView)
{
	struct MViewInfo *pMViewInfo;

	MView_Clear(hwndMView);
	pMViewInfo = GetMViewInfo(hwndMView);
	DeleteObject(pMViewInfo->hFont);
	free(pMViewInfo);
}


static LRESULT CALLBACK MView_WndProc(HWND hwndMView, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	struct MViewInfo *pMViewInfo;
	struct MViewEntry *pEnt;
	int nStaticPos, nStaticWidth, nEditPos, nEditWidth, nButtonPos, nButtonWidth;
	RECT r;
	LONG_PTR l = 0;
	LRESULT rc = 0;
	BOOL bHandled = false;
	HWND hwndButton;

	switch(nMessage)
	{
		case WM_DESTROY:
			MView_Free(hwndMView);
			break;

		case WM_SHOWWINDOW:
			if (wParam)
			{
				pMViewInfo = GetMViewInfo(hwndMView);
				MView_Refresh(hwndMView);
			}
			break;

		case WM_COMMAND:
			switch(HIWORD(wParam))
			{
				case BN_CLICKED:
					hwndButton = (HWND) lParam;
					l = GetWindowLongPtr(hwndButton, GWLP_USERDATA);
					pEnt = (struct MViewEntry *) l;
					MView_ButtonClick(hwndMView, pEnt, hwndButton);
					break;
			}
			break;
	}

	if (!bHandled)
		rc = DefWindowProc(hwndMView, nMessage, wParam, lParam);

	switch(nMessage)
	{
		case WM_CREATE:
			if (!MView_Setup(hwndMView))
				return -1;
			break;

		case WM_MOVE:
		case WM_SIZE:
			pMViewInfo = GetMViewInfo(hwndMView);
			pEnt = pMViewInfo->pEntries;
			if (pEnt)
			{
				MView_GetColumns(hwndMView, &nStaticPos, &nStaticWidth,
					&nEditPos, &nEditWidth, &nButtonPos, &nButtonWidth);
				while(pEnt->dev)
				{
					GetClientRect(pEnt->hwndStatic, &r);
					MapWindowPoints(pEnt->hwndStatic, hwndMView, ((POINT *) &r), 2);
					//MoveWindow(pEnt->hwndStatic, nStaticPos, r.top, nStaticWidth, r.bottom - r.top, false); // has its own line, no need to move
					// On next line, so need MVIEW_SPACING to put them down there.
					MoveWindow(pEnt->hwndEdit, nEditPos, r.top+MVIEW_SPACING, nEditWidth, r.bottom - r.top, false);
					MoveWindow(pEnt->hwndBrowseButton, nButtonPos, r.top+MVIEW_SPACING, nButtonWidth, r.bottom - r.top, false);
					pEnt++;
				}
			}
			break;
	}
	return rc;
}


void MView_RegisterClass(void)
{
	WNDCLASS wc;
	memset(&wc, 0, sizeof(wc));
	wc.lpszClassName = TEXT("MessSoftwareMView");
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpfnWndProc = MView_WndProc;
	wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
	RegisterClass(&wc);
}

