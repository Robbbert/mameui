// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include "winui.h"

static int MIN_WIDTH  = DBU_MIN_WIDTH;
static int MIN_HEIGHT = DBU_MIN_HEIGHT;

typedef struct
{
	int resource;
	const char *icon_name;
} ICONDATA;

typedef struct
{
	const char *name;
	int index;
} srcdriver_data_type;

static const ICONDATA g_iconData[] =
{
	{ IDI_WIN_NOROMS,			"noroms" },
	{ IDI_WIN_ROMS,				"roms" },
	{ IDI_WIN_UNKNOWN,			"unknown" },
	{ IDI_WIN_CLONE,			"clone" },
	{ IDI_WIN_REDX,				"warning" },
	{ IDI_WIN_IMPERFECT,		"imperfect" },
	{ 0 }
};

typedef struct _play_options play_options;
struct _play_options
{
	const char *record;			// OPTION_RECORD
	const char *playback;		// OPTION_PLAYBACK
	const char *state;			// OPTION_STATE
	const char *wavwrite;		// OPTION_WAVWRITE
	const char *mngwrite;		// OPTION_MNGWRITE
	const char *aviwrite;		// OPTION_AVIWRITE
};

/***************************************************************************
    function prototypes
 ***************************************************************************/

static void Win32UI_init(void);
static void Win32UI_exit(void);
static void	SaveWindowArea(void);
static void	SaveWindowStatus(void);
static bool OnIdle(HWND hWnd);
static void OnSize(HWND hwnd, UINT state, int width, int height);
static void SetView(int menu_id);
static void ResetListView(void);
static void UpdateGameList(void);
static void DestroyIcons(void);
static void ReloadIcons(void);
static void PollGUIJoystick(void);
static bool MameCommand(HWND hwnd,int id, HWND hwndCtl, UINT codeNotify);
static void UpdateStatusBar(void);
static bool TreeViewNotify(NMHDR *nm);
static int CLIB_DECL SrcDriverDataCompareFunc(const void *arg1, const void *arg2);
static int GamePicker_Compare(HWND hwndPicker, int index1, int index2, int sort_subitem);
static void DisableSelection(void);
static void EnableSelection(int nGame);
static HICON GetSelectedPickItemIconSmall(void);
static void SetRandomPickItem(void);
static void	PickColor(COLORREF *cDefault);
static LPTREEFOLDER GetSelectedFolder(void);
static LRESULT CALLBACK PictureFrameWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK PictureWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static HBITMAP CreateBitmapTransparent(HBITMAP hSource);
static void MamePlayRecordGame(void);
static void MamePlayBackGame(void);
static void MamePlayRecordWave(void);
static void MamePlayRecordMNG(void);
static void MamePlayRecordAVI(void);
static void	MameLoadState(void);
static void MamePlayGameWithOptions(int nGame, const play_options *playopts);
static bool GameCheck(void);
static void FolderCheck(void);
static void ToggleScreenShot(void);
static void AdjustMetrics(void);
/* Icon routines */
static void CreateIcons(void);
static int GetIconForDriver(int nItem);
static void AddDriverIcon(int nItem, int default_icon_index);
// Context Menu handlers
static void UpdateMenu(HMENU hMenu);
static void InitMainMenu(HMENU hMainMenu);
static void InitTreeContextMenu(HMENU hTreeMenu);
static void InitBodyContextMenu(HMENU hBodyContextMenu);
static void ToggleShowFolder(int folder);
static bool HandleTreeContextMenu(HWND hWnd, WPARAM wParam, LPARAM lParam);
static bool HandleScreenShotContextMenu(HWND hWnd, WPARAM wParam, LPARAM lParam);
static void	GamePicker_OnHeaderContextMenu(POINT pt, int nColumn);
static void	GamePicker_OnBodyContextMenu(POINT pt);
static void InitListView(void);
/* Re/initialize the ListView header columns */
static void ResetColumnDisplay(void);
static void CopyToolTipText (LPTOOLTIPTEXT lpttt);
static void ProgressBarShow(void);
static void ProgressBarHide(void);
static void ResizeProgressBar(void);
static void InitProgressBar(void);
static void InitToolbar(void);
static void InitStatusBar(void);
static void InitTabView(void);
static void InitListTree(void);
static void	InitMenuIcons(void);
static void ResetFonts(void);
static void SetMainTitle(void);
static void UpdateHistory(void);
static void RemoveCurrentGameCustomFolder(void);
static void RemoveGameCustomFolder(int driver_index);
static void BeginListViewDrag(NM_LISTVIEW *pnmv);
static void MouseMoveListViewDrag(POINTS pt);
static void ButtonUpListViewDrag(POINTS p);
static void CalculateBestScreenShotRect(HWND hWnd, RECT *pRect, bool restrict_height);
static void SwitchFullScreenMode(void);
static LRESULT CALLBACK MameWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK StartupProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static UINT_PTR CALLBACK HookProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
static UINT_PTR CALLBACK OFNHookProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
static char* ConvertAmpersandString(const char *s);

enum
{
	FILETYPE_GAME_LIST = 1,
	FILETYPE_ROMS_LIST = 2,
};

static bool CommonListDialog(common_file_dialog_proc cfd, int filetype);
static void SaveGameListToFile(char *szFile, int filetype);

/***************************************************************************
    Internal structures
 ***************************************************************************/

/*
 * These next two structs represent how the icon information
 * is stored in an ICO file.
 */
typedef struct
{
	BYTE bWidth;               /* Width of the image */
	BYTE bHeight;              /* Height of the image (times 2) */
	BYTE bColorCount;          /* Number of colors in image (0 if >=8bpp) */
	BYTE bReserved;            /* Reserved */
	WORD wPlanes;              /* Color Planes */
	WORD wBitCount;            /* Bits per pixel */
	DWORD dwBytesInRes;        /* how many bytes in this resource? */
	DWORD dwImageOffset;       /* where in the file is this image */
} ICONDIRENTRY, *LPICONDIRENTRY;

typedef struct
{
	UINT Width, Height, Colors; 	/* Width, Height and bpp */
	LPBYTE lpBits;                	/* ptr to DIB bits */
	DWORD dwNumBytes;            	/* how many bytes? */
	LPBITMAPINFO lpbi;              /* ptr to header */
	LPBYTE lpXOR;                 	/* ptr to XOR image bits */
	LPBYTE lpAND;                 	/* ptr to AND image bits */
} ICONIMAGE, *LPICONIMAGE;

typedef struct
{
	int type;       		/* Either RA_ID or RA_HWND, to indicate which member of u is used; or RA_END to signify last entry */
	union                   /* Can identify a child window by control id or by handle */
	{
		int id;       		/* Window control id */
		HWND hwnd;     		/* Window handle */
	} u;
	int action;     		/* What to do when control is resized */
	void *subwindow; 		/* Points to a Resize structure for this subwindow; NULL if none */
} ResizeItem;

typedef struct
{
	RECT rect;       			/* Client rect of window; must be initialized before first resize */
	const ResizeItem* items;   	/* Array of subitems to be resized */
} Resize;

static void ResizeWindow(HWND hParent, Resize *r);

/***************************************************************************
    Internal variables
 ***************************************************************************/
 
static TCHAR g_szPlayGameString[] = TEXT("Play %s\tAlt+O");
static char g_szGameCountString[] = "%d Games";
static HWND hMain = NULL;
static HWND	hSplash = NULL;
static HWND hWndList = NULL;
static HWND hTreeView = NULL;
static HWND hProgWnd = NULL;
static HWND hTabCtrl = NULL;
static HWND hSearchWnd = NULL;
static HWND	hProgress = NULL;
static HACCEL hAccel = NULL;
static HINSTANCE hInst = NULL;
static HFONT hFontGui = NULL;     		/* Font for tab view and search window */
static HFONT hFontList = NULL;     		/* Font for list view */
static HFONT hFontHist = NULL;     		/* Font for history view */
static HFONT hFontTree = NULL;     		/* Font for folders view */
/* menu icons bitmaps */
static HBITMAP hAboutMenu = NULL;
static HBITMAP hCustom = NULL;
static HBITMAP hDirectories = NULL;
static HBITMAP hExit = NULL;
static HBITMAP hFullscreen = NULL;
static HBITMAP hInterface = NULL;
static HBITMAP hHelp = NULL;
static HBITMAP hMameHome = NULL;
static HBITMAP hPlay = NULL;
static HBITMAP hPlayM1 = NULL;
static HBITMAP hOptions = NULL;
static HBITMAP hRefresh = NULL;
static HBITMAP hZip = NULL;
static HBITMAP hSaveList = NULL;
static HBITMAP hSaveRoms = NULL;
static HBITMAP hPlayback = NULL;
static HBITMAP hProperties = NULL;
static HBITMAP hAuditMenu = NULL;
static HBITMAP hVideo = NULL;
static HBITMAP hFonts = NULL;
static HBITMAP hFolders = NULL;
static HBITMAP hSort = NULL;
static HBITMAP hDriver = NULL;
static HBITMAP hFaq = NULL;
static HBITMAP hTabs = NULL;
static HBITMAP hTrouble = NULL;
static HBITMAP hCount = NULL;
static HBITMAP hRelease = NULL;
static HBITMAP hTime = NULL;
static HBITMAP hDescription	= NULL;
static HBITMAP hRom	= NULL;
static HBITMAP hSource = NULL;
static HBITMAP hManufacturer = NULL;
static HBITMAP hYear = NULL;
static HBITMAP hPlaywav	= NULL;
static HBITMAP hFont1 = NULL;
static HBITMAP hFont2 = NULL;
static HBITMAP hInfoback = NULL;
static HBITMAP hListback = NULL;
static HBITMAP hTreeback = NULL;
static HBITMAP hAscending = NULL;
static HBITMAP hFields = NULL;
static HBITMAP hRecavi = NULL;
static HBITMAP hRecinput = NULL;
static HBITMAP hRecwav = NULL;
static HBITMAP hPlaymng	= NULL;
static HBITMAP hRandom = NULL;
static HBITMAP hRecmng = NULL;
static HBITMAP hSavestate = NULL;
static HBITMAP hFilters	= NULL;
static HBITMAP hRemove = NULL;
static HBITMAP hRename = NULL;
static HBITMAP hReset = NULL;
static int optionfolder_count = 0;
/* global data--know where to send messages */
static bool in_emulation = false;
static bool game_launched = false;
/* idle work at startup */
static bool idle_work = false;
/* object pool in use */
static object_pool *mameui_pool;
static int game_index = 0;
static int game_total = 0;
static int oldpercent = 0;
static bool bDoGameCheck = false;
static bool bFolderCheck = false;
static bool bChangedHook = false;
static bool bHookFont = false;
/* Tree control variables */
static bool bEnableIndent = false;
static bool bShowTree = false;
static bool bShowToolBar = false;
static bool bShowStatusBar = false;
static bool bShowTabCtrl = false;
static bool bProgressShown = false;
static bool bListReady = false;
static bool bFullScreen = false;
/* use a joystick subsystem in the gui? */
static const struct OSDJoystick* g_pJoyGUI = NULL;
/* search */
static char g_SearchText[256];
static UINT lastColumnClick = 0;
static WNDPROC g_lpPictureFrameWndProc = NULL;
static WNDPROC g_lpPictureWndProc = NULL;
/* Tool and Status bar variables */
static HWND hStatusBar = NULL;
static HWND hToolBar = NULL;
/* Used to recalculate the main window layout */
static int bottomMargin = 0;
static int topMargin = 0;
static bool have_history = false;
static bool have_selection = false;
static HBITMAP hMissing_bitmap = NULL;
static HBRUSH hBrush = NULL;
static HBRUSH hBrushDlg = NULL;
static HDC hDC = NULL;
/* Icon variables */
static HIMAGELIST hLarge = NULL;
static HIMAGELIST hSmall = NULL;
static HICON hIcon = NULL;
static int *icon_index = NULL; 	/* for custom per-game icons */

static const TBBUTTON tbb[] =
{
	{0, ID_VIEW_FOLDERS,    	TBSTATE_ENABLED, BTNS_CHECK,      {0, 0}, 0, 0},
	{1, ID_VIEW_PICTURE_AREA,	TBSTATE_ENABLED, BTNS_CHECK,      {0, 0}, 0, 1},
	{0, 0,                  	TBSTATE_ENABLED, BTNS_SEP,        {0, 0}, 0, 0},
	{2, ID_VIEW_ICONS_LARGE,  	TBSTATE_ENABLED, BTNS_CHECKGROUP, {0, 0}, 0, 2},
	{3, ID_VIEW_ICONS_SMALL, 	TBSTATE_ENABLED, BTNS_CHECKGROUP, {0, 0}, 0, 3},
	{0, 0,                  	TBSTATE_ENABLED, BTNS_SEP,        {0, 0}, 0, 0},
	{4, ID_ENABLE_INDENT,  		TBSTATE_ENABLED, BTNS_CHECK,      {0, 0}, 0, 12},
	{0, 0,                  	TBSTATE_ENABLED, BTNS_SEP,        {0, 0}, 0, 0},
	{6, ID_UPDATE_GAMELIST,  	TBSTATE_ENABLED, BTNS_BUTTON,     {0, 0}, 0, 4},
	{0, 0,                    	TBSTATE_ENABLED, BTNS_SEP,        {0, 0}, 0, 0},
	{7, ID_OPTIONS_INTERFACE,	TBSTATE_ENABLED, BTNS_BUTTON,     {0, 0}, 0, 5},
	{8, ID_OPTIONS_DEFAULTS, 	TBSTATE_ENABLED, BTNS_BUTTON,     {0, 0}, 0, 6},
	{0, 0,                   	TBSTATE_ENABLED, BTNS_SEP,        {0, 0}, 0, 0},
	{9, ID_VIDEO_SNAP,			TBSTATE_ENABLED, BTNS_BUTTON,     {0, 0}, 0, 7},
	{10,ID_PLAY_M1,   			TBSTATE_ENABLED, BTNS_BUTTON,     {0, 0}, 0, 8},
	{0, 0,                    	TBSTATE_ENABLED, BTNS_SEP,        {0, 0}, 0, 0},
	{11,ID_HELP_ABOUT,      	TBSTATE_ENABLED, BTNS_BUTTON,     {0, 0}, 0, 9},
	{12,ID_HELP_CONTENTS,   	TBSTATE_ENABLED, BTNS_BUTTON,     {0, 0}, 0, 10},
	{0, 0,                    	TBSTATE_ENABLED, BTNS_SEP,        {0, 0}, 0, 0},
	{13,ID_MAME_HOMEPAGE,     	TBSTATE_ENABLED, BTNS_BUTTON,     {0, 0}, 0, 11},
	{0, 0,                    	TBSTATE_ENABLED, BTNS_SEP,        {0, 0}, 0, 0}
};

static const TCHAR szTbStrings[NUM_TOOLTIPS][30] =
{
	TEXT("Toggle folders list"),
	TEXT("Toggle pictures area"),
	TEXT("Large icons"),
	TEXT("Small icons"),
	TEXT("Refresh"),
	TEXT("Interface setttings"),
	TEXT("Default games options"),
	TEXT("Play ProgettoSnaps movie"),
	TEXT("M1FX"),
	TEXT("About"),
	TEXT("Help"),
	TEXT("MAME homepage"),
	TEXT("Toggle grouped view")
};

static const int CommandToString[] =
{
	ID_VIEW_FOLDERS,
	ID_VIEW_PICTURE_AREA,
	ID_VIEW_ICONS_LARGE,
	ID_VIEW_ICONS_SMALL,
	ID_UPDATE_GAMELIST,
	ID_OPTIONS_INTERFACE,
	ID_OPTIONS_DEFAULTS,
	ID_VIDEO_SNAP,
	ID_PLAY_M1,
	ID_HELP_ABOUT,
	ID_HELP_CONTENTS,
	ID_MAME_HOMEPAGE,
	ID_ENABLE_INDENT,
	-1
};

static const int s_nPickers[] =
{
	IDC_LIST
};

/* How to resize toolbar sub window */
static ResizeItem toolbar_resize_items[] =
{
	{ RA_ID,   { ID_TOOLBAR_EDIT }, RA_LEFT | RA_TOP,     NULL },
	{ RA_END,  { 0 },               0,                    NULL }
};

static Resize toolbar_resize = { {0, 0, 0, 0}, toolbar_resize_items };

/* How to resize main window */
static ResizeItem main_resize_items[] =
{
	{ RA_HWND, { 0 },            RA_LEFT  | RA_RIGHT  | RA_TOP,     &toolbar_resize },
	{ RA_HWND, { 0 },            RA_LEFT  | RA_RIGHT  | RA_BOTTOM,  NULL },
	{ RA_ID,   { IDC_DIVIDER },  RA_LEFT  | RA_RIGHT  | RA_TOP,     NULL },
	{ RA_ID,   { IDC_TREE },     RA_LEFT  | RA_BOTTOM | RA_TOP,     NULL },
	{ RA_ID,   { IDC_SPLITTER }, RA_LEFT  | RA_BOTTOM | RA_TOP,     NULL },
	{ RA_ID,   { IDC_LIST },     RA_ALL,                            NULL },
	{ RA_ID,   { IDC_SPLITTER2 },RA_RIGHT | RA_BOTTOM | RA_TOP,     NULL },
	{ RA_ID,   { IDC_SSTAB },    RA_RIGHT | RA_TOP,                 NULL },
	{ RA_ID,   { IDC_SSPICTURE },RA_RIGHT | RA_BOTTOM | RA_TOP,     NULL },
	{ RA_ID,   { IDC_HISTORY },  RA_RIGHT | RA_BOTTOM | RA_TOP,     NULL },
	{ RA_ID,   { IDC_SSFRAME },  RA_RIGHT | RA_BOTTOM | RA_TOP,     NULL },
	{ RA_ID,   { IDC_SSBORDER }, RA_RIGHT | RA_BOTTOM | RA_TOP,     NULL },
	{ RA_END,  { 0 },            0,                                 NULL }
};

static Resize main_resize = { {0, 0, 0, 0}, main_resize_items };

/* last directory for common file dialogs */
static TCHAR last_directory[MAX_PATH] = TEXT(".");
/* Last directory for Save Game or ROMs List dialogs */
static TCHAR list_directory[MAX_PATH] = TEXT(".");
static bool g_listview_dragging = false;
static HIMAGELIST himl_drag = NULL;
static int game_dragged = 0; 					/* which game started the drag */
static HTREEITEM prev_drag_drop_target = NULL; 	/* which tree view item we're currently highlighting */
static bool g_in_treeview_edit = false;
static srcdriver_data_type *sorted_srcdrivers = NULL;

/***************************************************************************
    Global variables
 ***************************************************************************/

/* Icon displayed in system tray */
static NOTIFYICONDATA MameIcon;

/* List view Column text */
extern const TCHAR* const column_names[COLUMN_MAX] =
{
	TEXT("Description"),
	TEXT("ROM name"),
	TEXT("Source file"),
	TEXT("Manufacturer"),
	TEXT("Year"),
	TEXT("Clone of"),
	TEXT("Played"),
	TEXT("Play time")
};


/***************************************************************************
    External functions
 ***************************************************************************/
class mameui_output_error : public osd_output
{
public:
	virtual void output_callback(osd_output_channel channel, const char *msg, va_list args)
	{
		if (channel == OSD_OUTPUT_CHANNEL_ERROR)
		{
			char buffer[4096];

			// if we are in fullscreen mode, go to windowed mode
			if ((video_config.windowed == 0) && !osd_common_t::s_window_list.empty())
				winwindow_toggle_full_screen();

			vsnprintf(buffer, WINUI_ARRAY_LENGTH(buffer), msg, args);
			win_message_box_utf8(!osd_common_t::s_window_list.empty() ? osd_common_t::s_window_list.front()->platform_window<HWND>() : nullptr, buffer, MAMEUINAME, MB_ICONERROR | MB_OK);
		}
		else
			chain_output(channel, msg, args);
	}
};

static void RunMAME(int nGameIndex, const play_options *playopts)
{
	time_t start = 0;
	time_t end = 0;
	windows_options mame_opts;
	std::string error_string;

	// prepare MAMEUIFX to run the game
	ShowWindow(hMain, SW_HIDE);

	for (int i = 0; i < WINUI_ARRAY_LENGTH(s_nPickers); i++)
		Picker_ClearIdle(GetDlgItem(hMain, s_nPickers[i]));

	// revert options set to default values
	mame_opts.revert(OPTION_PRIORITY_CMDLINE, OPTION_PRIORITY_CMDLINE);

	// Time the game run.
	windows_osd_interface osd(mame_opts);
	mameui_output_error winerror;
	osd_output::push(&winerror);
	osd.register_options();
	mame_machine_manager *manager = mame_machine_manager::instance(mame_opts, osd);
	// set the new game name
	mame_options::set_system_name(mame_opts, GetDriverGameName(nGameIndex));
	// set all needed paths
	SetDirectories(mame_opts);
	// load internal UI options
	LoadInternalUI();
	// parse all INI files
	mame_options::parse_standard_inis(mame_opts, error_string);
	// load interface language
	load_translation(mame_opts);
	// start LUA engine
	manager->start_luaengine();

	// set any specified play options
	if (playopts != NULL)
	{
		if (playopts->record != NULL)
			mame_opts.set_value(OPTION_RECORD, playopts->record, OPTION_PRIORITY_CMDLINE, error_string);

		if (playopts->playback != NULL)
			mame_opts.set_value(OPTION_PLAYBACK, playopts->playback, OPTION_PRIORITY_CMDLINE, error_string);

		if (playopts->state != NULL)
			mame_opts.set_value(OPTION_STATE, playopts->state, OPTION_PRIORITY_CMDLINE, error_string);

		if (playopts->wavwrite != NULL)
			mame_opts.set_value(OPTION_WAVWRITE, playopts->wavwrite, OPTION_PRIORITY_CMDLINE, error_string);

		if (playopts->mngwrite != NULL)
			mame_opts.set_value(OPTION_MNGWRITE, playopts->mngwrite, OPTION_PRIORITY_CMDLINE, error_string);

		if (playopts->aviwrite != NULL)
			mame_opts.set_value(OPTION_AVIWRITE, playopts->aviwrite, OPTION_PRIORITY_CMDLINE, error_string);

		assert(error_string.empty());
	}

	// start played time
	time(&start);
	// run the game
	manager->execute();
	// end played time
	time(&end);
	// remove any existing device options because they leak memory
	mame_options::remove_device_options(mame_opts);
	osd_output::pop(&winerror);
	global_free(manager);
	// Calc the duration
	double elapsedtime = end - start;
	// Increment our playtime
	IncrementPlayTime(nGameIndex, elapsedtime);

	// the emulation is complete; continue
	for (int i = 0; i < WINUI_ARRAY_LENGTH(s_nPickers); i++)
		Picker_ResetIdle(GetDlgItem(hMain, s_nPickers[i]));
}

int MameUIMain(HINSTANCE hInstance, LPWSTR lpCmdLine)
{
	if (__argc != 1)
	{
		extern int utf8_main(int argc, char *argv[]);
		std::vector<std::string> argv_vectors(__argc);
		char **utf8_argv = (char **) alloca(__argc * sizeof(char *));

		/* convert arguments to UTF-8 */
		for (int i = 0; i < __argc; i++)
		{
			argv_vectors[i] = utf8_from_tstring(__targv[i]);
			utf8_argv[i] = (char *) argv_vectors[i].c_str();
		}

		/* run utf8_main */
		exit(utf8_main(__argc, utf8_argv));
	}

	WNDCLASS wndclass;
	MSG msg;
	hInst = hInstance;

	// set up window class
	memset(&wndclass, 0, sizeof(WNDCLASS));
	wndclass.style = 0; //CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = MameWindowProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = DLGWINDOWEXTRA;
	wndclass.hInstance = hInstance;
	wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAMEUI_ICON));
	wndclass.hCursor = NULL;
	wndclass.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wndclass.lpszMenuName = MAKEINTRESOURCE(IDR_UI_MENU);
	wndclass.lpszClassName = TEXT("MainClass");

	RegisterClass(&wndclass);
	hMain = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, NULL);
	InitToolbar();
	InitStatusBar();
	InitProgressBar();
	InitTabView();
	InitMenuIcons();
	SetMainTitle();

	memset (&MameIcon, 0, sizeof(NOTIFYICONDATA));
	MameIcon.cbSize = sizeof(NOTIFYICONDATA);
	MameIcon.hWnd = hMain;
	MameIcon.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SYSTRAY));
	MameIcon.uID = 0;
	MameIcon.uCallbackMessage = WM_USER;
	MameIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
	MameIcon.dwInfoFlags = NIIF_USER;
	MameIcon.uVersion = NOTIFYICON_VERSION;
	wcscpy(MameIcon.szInfoTitle, TEXT("ARCADE"));
	wcscpy(MameIcon.szInfo, TEXT("Still running...."));
	wcscpy(MameIcon.szTip, TEXT("ARCADE"));

	hSplash = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_STARTUP), hMain, StartupProc);
	SetActiveWindow(hSplash);
	SetForegroundWindow(hSplash);
	Win32UI_init();
	DestroyWindow(hSplash);

	while(GetMessage(&msg, NULL, 0, 0))
	{
		if (IsWindow(hMain))
		{
			if (!TranslateAccelerator(hMain, hAccel, &msg))
			{
				if (!IsDialogMessage(hMain, &msg))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
		}
	}

	return msg.wParam;
}

HWND GetMainWindow(void)
{
	return hMain;
}

HWND GetTreeView(void)
{
	return hTreeView;
}

HWND GetProgressBar(void)
{
	return hProgress;
}

object_pool *GetMameUIMemoryPool(void)
{
	return mameui_pool;
}

void GetRealColumnOrder(int order[])
{
	int tmpOrder[COLUMN_MAX];
	int nColumnMax = Picker_GetNumColumns(hWndList);

	/* Get the Column Order and save it */
	(void)ListView_GetColumnOrderArray(hWndList, nColumnMax, tmpOrder);

	for (int i = 0; i < nColumnMax; i++)
	{
		order[i] = Picker_GetRealColumnFromViewColumn(hWndList, tmpOrder[i]);
	}
}

/*
 * PURPOSE: Format raw data read from an ICO file to an HICON
 * PARAMS:  PBYTE ptrBuffer  - Raw data from an ICO file
 *          UINT nBufferSize - Size of buffer ptrBuffer
 * RETURNS: HICON - handle to the icon, NULL for failure
 * History: July '95 - Created
 *          March '00- Seriously butchered from MSDN for mine own
 *          purposes, sayeth H0ek.
 */
static HICON FormatICOInMemoryToHICON(PBYTE ptrBuffer, UINT nBufferSize)
{
	ICONIMAGE IconImage;
	UINT nBufferIndex = 0;
	HICON hIcon = NULL;

	/* Is there a WORD? */
	if (nBufferSize < sizeof(WORD))
		return NULL;

	/* Was it 'reserved' ?   (ie 0) */
	if ((WORD)(ptrBuffer[nBufferIndex]) != 0)
		return NULL;

	nBufferIndex += sizeof(WORD);

	/* Is there a WORD? */
	if (nBufferSize - nBufferIndex < sizeof(WORD))
		return NULL;

	/* Was it type 1? */
	if ((WORD)(ptrBuffer[nBufferIndex]) != 1)
		return NULL;

	nBufferIndex += sizeof(WORD);

	/* Is there a WORD? */
	if (nBufferSize - nBufferIndex < sizeof(WORD))
		return NULL;

	/* Then that's the number of images in the ICO file */
	int nNumImages = (WORD)(ptrBuffer[nBufferIndex]);

	/* Is there at least one icon in the file? */
	if (nNumImages < 1)
		return NULL;

	nBufferIndex += sizeof(WORD);

	/* Is there enough space for the icon directory entries? */
	if ((nBufferIndex + nNumImages * sizeof(ICONDIRENTRY)) > nBufferSize)
		return NULL;

	/* Assign icon directory entries from buffer */
	LPICONDIRENTRY lpIDE = (LPICONDIRENTRY)(&ptrBuffer[nBufferIndex]);
	nBufferIndex += nNumImages * sizeof (ICONDIRENTRY);

	IconImage.dwNumBytes = lpIDE->dwBytesInRes;

	/* Seek to beginning of this image */
	if (lpIDE->dwImageOffset > nBufferSize)
		return NULL;

	nBufferIndex = lpIDE->dwImageOffset;

	/* Read it in */
	if ((nBufferIndex + lpIDE->dwBytesInRes) > nBufferSize)
		return NULL;

	IconImage.lpBits = &ptrBuffer[nBufferIndex];

	/* We would break on NT if we try with a 16bpp image */
	if (((LPBITMAPINFO)IconImage.lpBits)->bmiHeader.biBitCount != 16)
		hIcon = CreateIconFromResourceEx(IconImage.lpBits, IconImage.dwNumBytes, true, 0x00030000, 0, 0, LR_DEFAULTSIZE);

	return hIcon;
}

HICON LoadIconFromFile(const char *iconname)
{
	HICON hIcon = NULL;
	WIN32_FIND_DATA FindFileData;
	std::string tmpStr;
	PBYTE bufferPtr;
	util::archive_file::error ziperr;
	util::archive_file::ptr zip;

	tmpStr = std::string(GetIconsDir()).append(PATH_SEPARATOR).append(iconname).append(".ico");
	HANDLE hFind = win_find_first_file_utf8(tmpStr.c_str(), &FindFileData);
	
	if (hFind == INVALID_HANDLE_VALUE || (hIcon = win_extract_icon_utf8(hInst, tmpStr.c_str(), 0)) == 0)
	{
		std::string tmpIcoName;

		tmpStr = std::string(GetIconsDir()).append(PATH_SEPARATOR).append("icons.zip");
		tmpIcoName = std::string(iconname).append(".ico");
		ziperr = util::archive_file::open_zip(tmpStr, zip);

		if (ziperr == util::archive_file::error::NONE)
		{
			int found = zip->search(tmpIcoName, false);

			if (found >= 0)
			{
				bufferPtr = (PBYTE)malloc(zip->current_uncompressed_length());

				if (bufferPtr)
				{
					ziperr = zip->decompress(bufferPtr, zip->current_uncompressed_length());

					if (ziperr == util::archive_file::error::NONE)
						hIcon = FormatICOInMemoryToHICON(bufferPtr, zip->current_uncompressed_length());

					free(bufferPtr);
				}
			}

			zip.reset();
		}
		else
		{
			tmpStr = std::string(GetIconsDir()).append(PATH_SEPARATOR).append("icons.7z");
			tmpIcoName = std::string(iconname).append(".ico");
			ziperr = util::archive_file::open_7z(tmpStr, zip);

			if (ziperr == util::archive_file::error::NONE)
			{
				int found = zip->search(tmpIcoName, false);

				if (found >= 0)
				{
					bufferPtr = (PBYTE)malloc(zip->current_uncompressed_length());

					if (bufferPtr)
					{
						ziperr = zip->decompress(bufferPtr, zip->current_uncompressed_length());

						if (ziperr == util::archive_file::error::NONE)
							hIcon = FormatICOInMemoryToHICON(bufferPtr, zip->current_uncompressed_length());

						free(bufferPtr);
					}
				}

				zip.reset();
			}
		}
	}

	return hIcon;
}

/* Return the number of folders with options */
void SetNumOptionFolders(int count)
{
	optionfolder_count = count;
}

/* search */
const char * GetSearchText(void)
{
	return g_SearchText;
}

/* Sets the treeview and listviews sizes in accordance with their visibility and the splitters */
static void ResizeTreeAndListViews(bool bResizeHidden)
{
	RECT rect;
	int nLastWidth = SPLITTER_WIDTH;
	int nLastWidth2 = 0;
	int nLeftWindowWidth = 0;

	/* Size the List Control in the Picker */
	GetClientRect(hMain, &rect);

	if (bShowStatusBar)
		rect.bottom -= bottomMargin;

	if (bShowToolBar)
		rect.top += topMargin;

	/* Tree control */
	ShowWindow(GetDlgItem(hMain, IDC_TREE), bShowTree ? SW_SHOW : SW_HIDE);

	for (int i = 0; g_splitterInfo[i].nSplitterWindow; i++)
	{
		bool bVisible = (GetWindowLong(GetDlgItem(hMain, g_splitterInfo[i].nLeftWindow), GWL_STYLE) & WS_VISIBLE) ? true : false;

		if (bResizeHidden || bVisible)
		{
			nLeftWindowWidth = nSplitterOffset[i] - SPLITTER_WIDTH / 2 - nLastWidth;

			/* special case for the rightmost pane when the screenshot is gone */
			if (!GetShowScreenShot() && !g_splitterInfo[i + 1].nSplitterWindow)
				nLeftWindowWidth = rect.right - nLastWidth - 4;

			/* woah?  are we overlapping ourselves? */
			if (nLeftWindowWidth < MIN_VIEW_WIDTH)
			{
				nLastWidth = nLastWidth2;
				nLeftWindowWidth = nSplitterOffset[i] - MIN_VIEW_WIDTH - (SPLITTER_WIDTH * 3 / 2) - nLastWidth;
				i--;
			}

			MoveWindow(GetDlgItem(hMain, g_splitterInfo[i].nLeftWindow), nLastWidth, rect.top + 3, nLeftWindowWidth, (rect.bottom - rect.top) - 8, true);
			MoveWindow(GetDlgItem(hMain, g_splitterInfo[i].nSplitterWindow), nSplitterOffset[i], rect.top + 3, SPLITTER_WIDTH, (rect.bottom - rect.top) - 8, true);
		}

		if (bVisible)
		{
			nLastWidth2 = nLastWidth;
			nLastWidth += nLeftWindowWidth + SPLITTER_WIDTH;
		}
	}
}

/* Adjust the list view and screenshot button based on GetShowScreenShot() */
void UpdateScreenShot(void)
{
	RECT rect;
	RECT fRect;
	POINT p = {0, 0};

	/* first time through can't do this stuff */
	if (hWndList == NULL)
		return;

	/* Size the List Control in the Picker */
	GetClientRect(hMain, &rect);

	if (bShowStatusBar)
		rect.bottom -= bottomMargin;

	if (bShowToolBar)
		rect.top += topMargin;

	if (GetShowScreenShot())
	{
		CheckMenuItem(GetMenu(hMain),ID_VIEW_PICTURE_AREA, MF_CHECKED);
		ToolBar_CheckButton(hToolBar, ID_VIEW_PICTURE_AREA, MF_CHECKED);
	}
	else
	{
		CheckMenuItem(GetMenu(hMain),ID_VIEW_PICTURE_AREA, MF_UNCHECKED);
		ToolBar_CheckButton(hToolBar, ID_VIEW_PICTURE_AREA, MF_UNCHECKED);
	}

	ResizeTreeAndListViews(false);
	FreeScreenShot();

	if (have_selection)
		LoadScreenShot(Picker_GetSelectedItem(hWndList), TabView_GetCurrentTab(hTabCtrl));

	// figure out if we have a history or not, to place our other windows properly
	UpdateHistory();

	// setup the picture area
	if (GetShowScreenShot())
	{
		ClientToScreen(hMain, &p);
		GetWindowRect(GetDlgItem(hMain, IDC_SSFRAME), &fRect);
		OffsetRect(&fRect, -p.x, -p.y);

		// show history on this tab IF
		// - we have history for the game
		// - we're on the first tab
		// - we DON'T have a separate history tab
		bool showing_history = (have_history && (TabView_GetCurrentTab(hTabCtrl) == GetHistoryTab() || GetHistoryTab() == TAB_ALL ) &&
			GetShowTab(TAB_HISTORY) == false);
		CalculateBestScreenShotRect(GetDlgItem(hMain, IDC_SSFRAME), &rect, showing_history);
		DWORD dwStyle = GetWindowLong(GetDlgItem(hMain, IDC_SSPICTURE), GWL_STYLE);
		DWORD dwStyleEx = GetWindowLong(GetDlgItem(hMain, IDC_SSPICTURE), GWL_EXSTYLE);
		AdjustWindowRectEx(&rect, dwStyle, false, dwStyleEx);
		MoveWindow(GetDlgItem(hMain, IDC_SSPICTURE), fRect.left + rect.left, fRect.top + rect.top, rect.right - rect.left, rect.bottom - rect.top, true);
		ShowWindow(GetDlgItem(hMain, IDC_SSPICTURE), (TabView_GetCurrentTab(hTabCtrl) != TAB_HISTORY) ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(hMain, IDC_SSFRAME), SW_SHOW);
		ShowWindow(GetDlgItem(hMain, IDC_SSTAB), bShowTabCtrl ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(hMain, IDC_SSBORDER), bShowTabCtrl ? SW_HIDE : SW_SHOW);
		InvalidateRect(GetDlgItem(hMain, IDC_SSBORDER), NULL, false);
		InvalidateRect(GetDlgItem(hMain, IDC_SSTAB), NULL, false);
		InvalidateRect(GetDlgItem(hMain, IDC_SSFRAME), NULL, false);
		InvalidateRect(GetDlgItem(hMain, IDC_SSPICTURE), NULL, false);
	}
	else
	{
		ShowWindow(GetDlgItem(hMain, IDC_SSPICTURE), SW_HIDE);
		ShowWindow(GetDlgItem(hMain, IDC_SSFRAME), SW_HIDE);
		ShowWindow(GetDlgItem(hMain, IDC_SSBORDER), SW_HIDE);
		ShowWindow(GetDlgItem(hMain, IDC_SSTAB), SW_HIDE);
	}
}

void ResizePickerControls(HWND hWnd)
{
	RECT frameRect;
	RECT rect, sRect;
	static bool firstTime = true;
	bool doSSControls = true;
	int nSplitterCount = GetSplitterCount();

	/* Size the List Control in the Picker */
	GetClientRect(hWnd, &rect);

	/* Calc the display sizes based on g_splitterInfo */
	if (firstTime)
	{
		RECT rWindow;

		for (int i = 0; i < nSplitterCount; i++)
			nSplitterOffset[i] = rect.right * g_splitterInfo[i].dPosition;

		GetWindowRect(hStatusBar, &rWindow);
		bottomMargin = rWindow.bottom - rWindow.top;
		GetWindowRect(hToolBar, &rWindow);
		topMargin = rWindow.bottom - rWindow.top;
		firstTime = false;
	}
	else
		doSSControls = GetShowScreenShot();

	if (bShowStatusBar)
		rect.bottom -= bottomMargin;

	if (bShowToolBar)
		rect.top += topMargin;

	MoveWindow(GetDlgItem(hWnd, IDC_DIVIDER), rect.left - 1, rect.top - 3, rect.right + 2, 1, true);
	ResizeTreeAndListViews(true);
	int nListWidth = nSplitterOffset[nSplitterCount - 1];
	int nScreenShotWidth = (rect.right - nListWidth);

	/* Screen shot Page tab control */
	if (bShowTabCtrl)
	{
		MoveWindow(GetDlgItem(hWnd, IDC_SSTAB), nListWidth + 2, rect.top + 3, nScreenShotWidth - 4, (rect.bottom - rect.top) - 7, doSSControls);
		rect.top += 21;
	}
	else
		MoveWindow(GetDlgItem(hWnd, IDC_SSBORDER), nListWidth + 2, rect.top + 3, nScreenShotWidth - 6, (rect.bottom - rect.top) - 8, doSSControls);

	/* resize the Screen shot frame */
	MoveWindow(GetDlgItem(hWnd, IDC_SSFRAME), nListWidth + 3, rect.top + 4, nScreenShotWidth - 8, (rect.bottom - rect.top) - 10, doSSControls);
	/* The screen shot controls */
	GetClientRect(GetDlgItem(hWnd, IDC_SSFRAME), &frameRect);
	/* Text control - game history */
	sRect.left = nListWidth + 12;
	sRect.right = sRect.left + (nScreenShotWidth - 26);

	if (GetShowTab(TAB_HISTORY))
	{
		// We're using the new mode, with the history filling the entire tab (almost)
		sRect.top = rect.top + 16;
		sRect.bottom = (rect.bottom - rect.top) - 30;
	}
	else
	{
		// We're using the original mode, with the history beneath the SS picture
		sRect.top = rect.top + 264;
		sRect.bottom = (rect.bottom - rect.top) - 278;
	}

	MoveWindow(GetDlgItem(hWnd, IDC_HISTORY), sRect.left, sRect.top, sRect.right - sRect.left, sRect.bottom, doSSControls);

	/* the other screen shot controls will be properly placed in UpdateScreenshot() */
}

int GetMinimumScreenShotWindowWidth(void)
{
	BITMAP bmp;

	GetObject(hMissing_bitmap, sizeof(BITMAP), &bmp);
	return bmp.bmWidth + 6; 	// 6 is for a little breathing room
}

int GetParentIndex(const game_driver *driver)
{
	return GetGameNameIndex(driver->parent);
}

int GetParentRomSetIndex(const game_driver *driver)
{
	int nParentIndex = GetGameNameIndex(driver->parent);

	if(nParentIndex >= 0)
	{
		if ((driver_list::driver(nParentIndex).flags & MACHINE_IS_BIOS_ROOT) == 0)
			return nParentIndex;
	}

	return -1;
}

int GetSrcDriverIndex(const char *name)
{
	srcdriver_data_type *srcdriver_index_info;
	srcdriver_data_type key;
	key.name = name;

	srcdriver_index_info = (srcdriver_data_type *)bsearch(&key, sorted_srcdrivers, driver_list::total(), sizeof(srcdriver_data_type), SrcDriverDataCompareFunc);

	if (srcdriver_index_info == NULL)
		return -1;

	return srcdriver_index_info->index;
}

/***************************************************************************
    Internal functions
 ***************************************************************************/

static int CLIB_DECL SrcDriverDataCompareFunc(const void *arg1, const void *arg2)
{
	return strcmp(((srcdriver_data_type *)arg1)->name, ((srcdriver_data_type *)arg2)->name);
}

static void SetMainTitle(void)
{
	char buffer[256];

	snprintf(buffer, WINUI_ARRAY_LENGTH(buffer), "%s %s", MAMEUINAME, GetVersionString());
	win_set_window_text_utf8(hMain, buffer);
}

static void memory_error(const char *message)
{
	ErrorMessageBox(message);
	exit(-1);
}

static void Win32UI_init(void)
{
	RECT rect;
	extern const FOLDERDATA g_folderData[];
	extern const FILTER_ITEM g_filterList[];
	LONG_PTR l;

	/* Init DirectInput */
	DirectInputInitialize();
	OptionsInit();

	if (GetRequiredDriverCacheStatus())
		win_set_window_text_utf8(GetDlgItem(hSplash, IDC_PROGBAR), "Building folders structure...");
	else
		win_set_window_text_utf8(GetDlgItem(hSplash, IDC_PROGBAR), "Loading folders structure...");

	srand((unsigned)time(NULL));
	// create the memory pool
	mameui_pool = pool_alloc_lib(memory_error);
	// custom per-game icons
	icon_index = (int*)pool_malloc_lib(mameui_pool, sizeof(int) * driver_list::total());
	memset(icon_index, 0, sizeof(int) * driver_list::total());
	// sorted list of source drivers by name
	sorted_srcdrivers = (srcdriver_data_type *) pool_malloc_lib(mameui_pool, sizeof(srcdriver_data_type) * driver_list::total());
	memset(sorted_srcdrivers, 0, sizeof(srcdriver_data_type) * driver_list::total());

	for (int i = 0; i < driver_list::total(); i++)
	{
		const char *driver_name = core_strdup(GetDriverFileName(i));
		sorted_srcdrivers[i].name = driver_name;
		sorted_srcdrivers[i].index = i;
		driver_name = NULL;
	}

	qsort(sorted_srcdrivers, driver_list::total(), sizeof(srcdriver_data_type), SrcDriverDataCompareFunc);

	{
		struct TabViewOptions opts;

		static const struct TabViewCallbacks s_tabviewCallbacks =
		{
			GetShowTabCtrl,				// pfnGetShowTabCtrl
			SetCurrentTab,				// pfnSetCurrentTab
			GetCurrentTab,				// pfnGetCurrentTab
			SetShowTab,					// pfnSetShowTab
			GetShowTab,					// pfnGetShowTab
			GetImageTabShortName,		// pfnGetTabShortName
			GetImageTabLongName,		// pfnGetTabLongName
			UpdateScreenShot			// pfnOnSelectionChanged
		};

		memset(&opts, 0, sizeof(opts));
		opts.pCallbacks = &s_tabviewCallbacks;
		opts.nTabCount = MAX_TAB_TYPES;
		SetupTabView(hTabCtrl, &opts);
	}

	/* subclass picture frame area */
	l = GetWindowLongPtr(GetDlgItem(hMain, IDC_SSFRAME), GWLP_WNDPROC);
	g_lpPictureFrameWndProc = (WNDPROC)l;
	SetWindowLongPtr(GetDlgItem(hMain, IDC_SSFRAME), GWLP_WNDPROC, (LONG_PTR)PictureFrameWndProc);
	/* subclass picture area */
	l = GetWindowLongPtr(GetDlgItem(hMain, IDC_SSPICTURE), GWLP_WNDPROC);
	g_lpPictureWndProc = (WNDPROC)l;
	SetWindowLongPtr(GetDlgItem(hMain, IDC_SSPICTURE), GWLP_WNDPROC, (LONG_PTR)PictureWndProc);
	/* Load the pic for the default screenshot. */
	hMissing_bitmap = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_SNAPSHOT));
	main_resize_items[0].u.hwnd = hToolBar;
	main_resize_items[1].u.hwnd = hStatusBar;
	GetClientRect(hMain, &rect);
	InitListTree();
	InitSplitters();
	int nSplitterCount = GetSplitterCount();

	for (int i = 0; i < nSplitterCount; i++)
	{
		HWND hWnd = GetDlgItem(hMain, g_splitterInfo[i].nSplitterWindow);
		HWND hWndLeft = GetDlgItem(hMain, g_splitterInfo[i].nLeftWindow);
		HWND hWndRight = GetDlgItem(hMain, g_splitterInfo[i].nRightWindow);
		AddSplitter(hWnd, hWndLeft, hWndRight, g_splitterInfo[i].pfnAdjust);
	}

	/* Initial adjustment of controls on the Picker window */
	ResizePickerControls(hMain);
	TabView_UpdateSelection(hTabCtrl);
	bShowTree = GetShowFolderList();
	bShowToolBar = GetShowToolBar();
	bShowStatusBar = GetShowStatusBar();
	bShowTabCtrl = GetShowTabCtrl();
	bEnableIndent = GetEnableIndent();
	CheckMenuItem(GetMenu(hMain), ID_VIEW_FOLDERS, (bShowTree) ? MF_CHECKED : MF_UNCHECKED);
	ToolBar_CheckButton(hToolBar, ID_VIEW_FOLDERS, (bShowTree) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(GetMenu(hMain), ID_VIEW_TOOLBARS, (bShowToolBar) ? MF_CHECKED : MF_UNCHECKED);
	ShowWindow(hToolBar, (bShowToolBar) ? SW_SHOW : SW_HIDE);
	CheckMenuItem(GetMenu(hMain), ID_VIEW_STATUS, (bShowStatusBar) ? MF_CHECKED : MF_UNCHECKED);
	ShowWindow(hStatusBar, (bShowStatusBar) ? SW_SHOW : SW_HIDE);
	CheckMenuItem(GetMenu(hMain), ID_VIEW_PAGETAB, (bShowTabCtrl) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(GetMenu(hMain), ID_ENABLE_INDENT, (bEnableIndent) ? MF_CHECKED : MF_UNCHECKED);
	ToolBar_CheckButton(hToolBar, ID_ENABLE_INDENT, (bEnableIndent) ? MF_CHECKED : MF_UNCHECKED);
	InitTree(g_folderData, g_filterList);
	win_set_window_text_utf8(GetDlgItem(hSplash, IDC_PROGBAR), "Building list structure...");
	/* Initialize listview columns */
	InitListView();
	SendMessage(hProgress, PBM_SETPOS, 120, 0);
	win_set_window_text_utf8(GetDlgItem(hSplash, IDC_PROGBAR), "Initializing window...");
	ResetFonts();
	AdjustMetrics();
	UpdateScreenShot();
	InitMainMenu(GetMenu(hMain));
	hAccel = LoadAccelerators(hInst, MAKEINTRESOURCE(IDA_TAB_KEYS));

	if (GetJoyGUI() == true)
	{
		g_pJoyGUI = &DIJoystick;

		if (g_pJoyGUI->init() != 0)
			g_pJoyGUI = NULL;
		else
			SetTimer(hMain, JOYGUI_TIMER, JOYGUI_MS, NULL);
	}
	else
		g_pJoyGUI = NULL;

	game_index = 0;
	game_total = driver_list::total();
	oldpercent = -1;
	bDoGameCheck = false;
	bFolderCheck = false;
	idle_work = true;
	bFullScreen = false;

	switch (GetViewMode())
	{
		case VIEW_ICONS_LARGE :
			SetView(ID_VIEW_ICONS_LARGE);
			break;

		case VIEW_ICONS_SMALL :
		default :
			SetView(ID_VIEW_ICONS_SMALL);
			break;
	}

	UpdateListView();
	ShowWindow(hSplash, SW_HIDE);
	CenterWindow(hMain);
	ShowWindow(hMain, GetWindowState());
	SetActiveWindow(hMain);
	SetForegroundWindow(hMain);
	SetFocus(hWndList);

	if (GetCycleScreenshot() > 0)
		SetTimer(hMain, SCREENSHOT_TIMER, GetCycleScreenshot() * 1000, NULL); 	//scale to Seconds
}

static void Win32UI_exit(void)
{
	SaveWindowStatus();
	ShowWindow(hMain, SW_HIDE);

	if (GetMinimizeTrayIcon())
		Shell_NotifyIcon(NIM_DELETE, &MameIcon);

	if (g_pJoyGUI != NULL)
		g_pJoyGUI->exit();

	DeleteObject(hBrush);
	DeleteObject(hBrushDlg);
	DeleteBitmap(hAboutMenu);
	DeleteBitmap(hCustom);
	DeleteBitmap(hDirectories);
	DeleteBitmap(hExit);
	DeleteBitmap(hFullscreen);
	DeleteBitmap(hInterface);
	DeleteBitmap(hHelp);
	DeleteBitmap(hMameHome);
	DeleteBitmap(hPlay);
	DeleteBitmap(hPlayM1);
	DeleteBitmap(hOptions);
	DeleteBitmap(hRefresh);
	DeleteBitmap(hZip);
	DeleteBitmap(hSaveList);
	DeleteBitmap(hSaveRoms);
	DeleteBitmap(hPlayback);
	DeleteBitmap(hProperties);
	DeleteBitmap(hAuditMenu);
	DeleteBitmap(hVideo);
	DeleteBitmap(hFonts);
	DeleteBitmap(hFolders);
	DeleteBitmap(hSort);
	DeleteBitmap(hDriver);
	DeleteBitmap(hFaq);
	DeleteBitmap(hTabs);
	DeleteBitmap(hTrouble);
	DeleteBitmap(hCount);
	DeleteBitmap(hRelease);
	DeleteBitmap(hTime);
	DeleteBitmap(hDescription);
	DeleteBitmap(hRom);
	DeleteBitmap(hSource);
	DeleteBitmap(hManufacturer);
	DeleteBitmap(hYear);
	DeleteBitmap(hPlaywav);
	DeleteBitmap(hFont1);
	DeleteBitmap(hFont2);
	DeleteBitmap(hInfoback);
	DeleteBitmap(hListback);
	DeleteBitmap(hTreeback);
	DeleteBitmap(hAscending);
	DeleteBitmap(hFields);
	DeleteBitmap(hRecavi);
	DeleteBitmap(hRecinput);
	DeleteBitmap(hRecwav);
	DeleteBitmap(hPlaymng);
	DeleteBitmap(hRandom);
	DeleteBitmap(hRecmng);
	DeleteBitmap(hSavestate);
	DeleteBitmap(hFilters);
	DeleteBitmap(hRemove);
	DeleteBitmap(hRename);
	DeleteBitmap(hReset);
	DeleteBitmap(hMissing_bitmap);
	DeleteFont(hFontGui);
	DeleteFont(hFontList);
	DeleteFont(hFontHist);
	DeleteFont(hFontTree);
	DestroyIcons();
	DestroyAcceleratorTable(hAccel);
	DirectInputClose();
	SetSavedFolderID(GetCurrentFolderID());
	SaveInterface();
	SaveGameList();
	SaveInternalUI();
	SavePlugins();
	SaveGameDefaults();
	FreeFolders();
	FreeScreenShot();
	pool_free_lib(mameui_pool);
	mameui_pool = NULL;
	DestroyWindow(hMain);
}

static LRESULT CALLBACK MameWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_CTLCOLORSTATIC:
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);

			if ((HWND)lParam == GetDlgItem(hMain, IDC_HISTORY))
				SetTextColor(hDC, GetHistoryFontColor());

			return (LRESULT) hBrushDlg;

		case WM_INITDIALOG:
			/* Initialize info for resizing subitems */
			GetClientRect(hWnd, &main_resize.rect);
			return true;

		case WM_SETFOCUS:
			UpdateWindow(hMain);
			SetFocus(hWndList);
			break;

		case WM_SETTINGCHANGE:
			AdjustMetrics();
			return false;

		case WM_SIZE:
			OnSize(hWnd, wParam, LOWORD(lParam), HIWORD(wParam));
			return true;

		case MM_PLAY_GAME:
			MamePlayGame();
			return true;

		case WM_INITMENUPOPUP:
			UpdateMenu(GetMenu(hWnd));
			break;

		case WM_CONTEXTMENU:
			if (HandleTreeContextMenu(hWnd, wParam, lParam) || HandleScreenShotContextMenu(hWnd, wParam, lParam))
				return false;
			break;

		case WM_COMMAND:
			return MameCommand(hMain,(int)(LOWORD(wParam)),(HWND)(lParam),(UINT)HIWORD(wParam));

		case WM_GETMINMAXINFO:
		{
			MINMAXINFO *mminfo;		
			/* Don't let the window get too small; it can break resizing */
			mminfo = (MINMAXINFO *) lParam;
			mminfo->ptMinTrackSize.x = MIN_WIDTH;
			mminfo->ptMinTrackSize.y = MIN_HEIGHT;
			return false;
		}

		case WM_TIMER:
			switch (wParam)
			{
				case JOYGUI_TIMER:
					PollGUIJoystick();
					break;

				case SCREENSHOT_TIMER:
					TabView_CalculateNextTab(hTabCtrl);
					UpdateScreenShot();
					TabView_UpdateSelection(hTabCtrl);
					break;
			}

			return true;

		case WM_CLOSE:
			if (GetExitDialog())
			{
				if (win_message_box_utf8(hMain, "Are you sure you want to quit?", MAMEUINAME, MB_ICONQUESTION | MB_YESNO) == IDNO)
				{
					SetFocus(hWndList);
					return true;
				}
			}

			Win32UI_exit();
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			return false;

		case WM_LBUTTONDOWN:
			OnLButtonDown(hWnd, (UINT)wParam, MAKEPOINTS(lParam));
			break;

		case WM_MOUSEMOVE:
			if (g_listview_dragging)
				MouseMoveListViewDrag(MAKEPOINTS(lParam));
			else
				/* for splitters */
				OnMouseMove(hWnd, (UINT)wParam, MAKEPOINTS(lParam));

			break;

		case WM_LBUTTONUP:
			if (g_listview_dragging)
				ButtonUpListViewDrag(MAKEPOINTS(lParam));
			else
				/* for splitters */
				OnLButtonUp(hWnd, (UINT)wParam, MAKEPOINTS(lParam));

			break;

		case WM_NOTIFY:
			/* Where is this message intended to go */
			{
				LPNMHDR lpNmHdr = (LPNMHDR)lParam;
				TCHAR szClass[128];

				/* Fetch tooltip text */
				if (lpNmHdr->code == TTN_NEEDTEXT)
				{
					LPTOOLTIPTEXT lpttt = (LPTOOLTIPTEXT)lParam;
					CopyToolTipText(lpttt);
				}

				if (lpNmHdr->hwndFrom == hTreeView)
					return TreeViewNotify(lpNmHdr);

				GetClassName(lpNmHdr->hwndFrom, szClass, WINUI_ARRAY_LENGTH(szClass));

				if (!_tcscmp(szClass, WC_LISTVIEW))
					return Picker_HandleNotify(lpNmHdr);

				if (!_tcscmp(szClass, WC_TABCONTROL))
					return TabView_HandleNotify(lpNmHdr);

				break;
			}

		case WM_USER:
			if (lParam == WM_LBUTTONDBLCLK)
			{
				Shell_NotifyIcon(NIM_DELETE, &MameIcon);
				ShowWindow(hMain, SW_RESTORE);
				SetActiveWindow(hMain);
				SetForegroundWindow(hMain);
				SetFocus(hWndList);
			}

			break;

		case WM_SYSCOMMAND:
			if (wParam == SC_MINIMIZE)
			{
				if (!IsMaximized(hMain))
					SaveWindowArea();

				if (GetMinimizeTrayIcon())
				{
					ShowWindow(hMain, SW_MINIMIZE);
					ShowWindow(hMain, SW_HIDE);
					Shell_NotifyIcon(NIM_ADD, &MameIcon);
					Shell_NotifyIcon(NIM_SETVERSION, &MameIcon);
				}
			}
			else if (wParam == SC_MAXIMIZE)
				SaveWindowArea();

			break;

	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static void SaveWindowArea(void)
{
	/* save main window size */
	RECT rect;
	AREA area;

	GetWindowRect(hMain, &rect);
	area.x = rect.left;
	area.y = rect.top;
	area.width = rect.right  - rect.left;
	area.height = rect.bottom - rect.top;
	SetWindowArea(&area);
}

static void SaveWindowStatus(void)
{
	WINDOWPLACEMENT wndpl;

	wndpl.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(hMain, &wndpl);
	UINT state = wndpl.showCmd;

	if (state == SW_MINIMIZE || state == SW_SHOWMINIMIZED)
		state = SW_RESTORE;
	else if(state == SW_MAXIMIZE)
	{
		state = SW_MAXIMIZE;
		ShowWindow(hMain, SW_RESTORE);
	}
	else
	{
		state = SW_SHOWNORMAL;
		SaveWindowArea();
	}

	SetWindowState(state);

	for (int i = 0; i < GetSplitterCount(); i++)
		SetSplitterPos(i, nSplitterOffset[i]);
			
	for (int i = 0; i < sizeof(s_nPickers) / sizeof(s_nPickers[0]); i++)
		Picker_SaveColumnWidths(GetDlgItem(hMain, s_nPickers[i]));

	int nItem = Picker_GetSelectedItem(hWndList);
	SetDefaultGame(GetDriverGameName(nItem));
}

static void FolderCheck(void)
{
	int counter = ListView_GetItemCount(hWndList);

	for (int i = 0; i < counter; i++)
	{
		LVITEM lvi;

		lvi.iItem = i;
		lvi.iSubItem = 0;
		lvi.mask = LVIF_PARAM;
		(void)ListView_GetItem(hWndList, &lvi);
		SetRomAuditResults(lvi.lParam, UNKNOWN);
	}

	game_index = 0;
	game_total = counter;
	oldpercent = -1;
	bDoGameCheck = false;
	bFolderCheck = true;
	idle_work = true;
	ReloadIcons();
	Picker_ResetIdle(hWndList);
}

static bool GameCheck(void)
{
	LVFINDINFO lvfi;
	int percentage = ((game_index + 1) * 100) / game_total;
	bool changed = false;

	AuditRefresh();

	if (game_index == 0)
		ProgressBarShow();

	if (bFolderCheck == true)
	{
		LVITEM lvi;

		lvi.iItem = game_index;
		lvi.iSubItem = 0;
		lvi.mask = LVIF_PARAM;
		(void)ListView_GetItem(hWndList, &lvi);
		MameUIVerifyRomSet(lvi.lParam, true);
		changed = true;
		lvfi.flags	= LVFI_PARAM;
		lvfi.lParam = lvi.lParam;
	}
	else
	{
		MameUIVerifyRomSet(game_index, true);
		changed = true;
		lvfi.flags	= LVFI_PARAM;
		lvfi.lParam = game_index;
	}

	int i = ListView_FindItem(hWndList, -1, &lvfi);

	if (changed && i != -1)
		(void)ListView_RedrawItems(hWndList, i, i);

	if (percentage != oldpercent)
	{
		SetStatusBarTextF(0, "Game search %02d%% completed", percentage);
		oldpercent = percentage;
	}

	SendMessage(hProgWnd, PBM_SETPOS, game_index, 0);
	game_index++;

	if (game_index >= game_total)
	{
		bDoGameCheck = false;
		bFolderCheck = false;
		ProgressBarHide();
		ResetWhichGamesInFolders();
		return false;
	}

	return changed;
}

static bool OnIdle(HWND hWnd)
{
	static bool bFirstTime = true;

	if (bFirstTime)
		bFirstTime = false;

	if ((bDoGameCheck) || (bFolderCheck))
	{
		GameCheck();
		return idle_work;
	}

	// in case it's not found, get it back
	int driver_index = Picker_GetSelectedItem(hWndList);
	const char *pDescription = GetDriverGameTitle(driver_index);
	SetStatusBarText(0, pDescription);
	const char *pName = GetDriverGameName(driver_index);
	SetStatusBarText(1, pName);
	idle_work = false;
	UpdateStatusBar();
	bFirstTime = true;

	if (game_launched)
	{
		game_launched = false;
		return idle_work;
	}

	if (!idle_work)
		UpdateListView();

	return idle_work;
}

static void OnSize(HWND hWnd, UINT nState, int nWidth, int nHeight)
{
	static bool firstTime = true;

	if (nState != SIZE_MAXIMIZED && nState != SIZE_RESTORED)
		return;

	ResizeWindow(hWnd, &main_resize);
	ResizeProgressBar();

	if (firstTime == false)
		OnSizeSplitter(hMain);

	/* Update the splitters structures as appropriate */
	RecalcSplitters();

	if (firstTime == false)
		ResizePickerControls(hMain);

	firstTime = false;
	UpdateScreenShot();
}

static void ResizeWindow(HWND hParent, Resize *r)
{
	int cmkindex = 0;
	HWND hControl = NULL;
	RECT parent_rect, rect;
	POINT p = {0, 0};

	if (hParent == NULL)
		return;

	/* Calculate change in width and height of parent window */
	GetClientRect(hParent, &parent_rect);
	int dy = parent_rect.bottom - r->rect.bottom;
	int dx = parent_rect.right - r->rect.right;
	ClientToScreen(hParent, &p);

	while (r->items[cmkindex].type != RA_END)
	{
		const ResizeItem *ri = &r->items[cmkindex];

		if (ri->type == RA_ID)
			hControl = GetDlgItem(hParent, ri->u.id);
		else
			hControl = ri->u.hwnd;

		if (hControl == NULL)
		{
			cmkindex++;
			continue;
		}

		/* Get control's rectangle relative to parent */
		GetWindowRect(hControl, &rect);
		OffsetRect(&rect, -p.x, -p.y);
		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;

		if (!(ri->action & RA_LEFT))
			rect.left += dx;

		if (!(ri->action & RA_TOP))
			rect.top += dy;

		if (ri->action & RA_RIGHT)
			rect.right += dx;

		if (ri->action & RA_BOTTOM)
			rect.bottom += dy;
		//Sanity Check the child rect
		if (parent_rect.top > rect.top)
			rect.top = parent_rect.top;

		if (parent_rect.left > rect.left)
			rect.left = parent_rect.left;

		if (parent_rect.bottom < rect.bottom) 
		{
			rect.bottom = parent_rect.bottom;
			//ensure we have at least a minimal height
			rect.top = rect.bottom - height;

			if (rect.top < parent_rect.top) 
				rect.top = parent_rect.top;
		}

		if (parent_rect.right < rect.right) 
		{
			rect.right = parent_rect.right;
			//ensure we have at least a minimal width
			rect.left = rect.right - width;

			if (rect.left < parent_rect.left) 
				rect.left = parent_rect.left;
		}

		MoveWindow(hControl, rect.left, rect.top, (rect.right - rect.left), (rect.bottom - rect.top), true);

		/* Take care of subcontrols, if appropriate */
		if (ri->subwindow != NULL)
			ResizeWindow(hControl, (Resize*)ri->subwindow);

		cmkindex++;
	}

	/* Record parent window's new location */
	memcpy(&r->rect, &parent_rect, sizeof(RECT));
}

static void ProgressBarShow()
{
	RECT rect;
	int widths[2] = {160, -1};

	SendMessage(hStatusBar, SB_SETPARTS, 2, (LPARAM)widths);
	SendMessage(hProgWnd, PBM_SETRANGE, 0, MAKELPARAM(0, game_total));
	SendMessage(hProgWnd, PBM_SETPOS, 0, 0);
	StatusBar_GetItemRect(hStatusBar, 1, &rect);
	MoveWindow(hProgWnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, true);
	ShowWindow(hProgWnd, SW_SHOW);
	bProgressShown = true;
}

static void ProgressBarHide()
{
	RECT rect;
	int widths[6];
	int numParts = 6;

	if (hProgWnd == NULL)
		return;

	ShowWindow(hProgWnd, SW_HIDE);
	widths[5] = 96;
	widths[4] = 80;
	widths[3] = 160;
	widths[2] = 120;
	widths[1] = 88;
	widths[0] = -1;
	SendMessage(hStatusBar, SB_SETPARTS, 1, (LPARAM)widths);
	StatusBar_GetItemRect(hStatusBar, 0, &rect);
	widths[0] = (rect.right - rect.left) - (widths[1] + widths[2] + widths[3] + widths[4] + widths[5]);
	widths[1] += widths[0];
	widths[2] += widths[1];
	widths[3] += widths[2];
	widths[4] += widths[3];
	widths[5] += widths[4];
	SendMessage(hStatusBar, SB_SETPARTS, numParts, (LPARAM)widths);
	UpdateStatusBar();
	bProgressShown = false;
}

static void ResizeProgressBar()
{
	if (bProgressShown)
	{
		RECT rect;
		int  widths[2] = {160, -1};

		SendMessage(hStatusBar, SB_SETPARTS, 2, (LPARAM)widths);
		StatusBar_GetItemRect(hStatusBar, 1, &rect);
		MoveWindow(hProgWnd, rect.left, rect.top, rect.right  - rect.left, rect.bottom - rect.top, true);
	}
	else
		ProgressBarHide();
}

static void InitProgressBar(void)
{
	RECT rect;

	StatusBar_GetItemRect(hStatusBar, 0, &rect);
	rect.left += 160;
	hProgWnd = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_CLIPSIBLINGS, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, hStatusBar, NULL, hInst, NULL);
	SetWindowTheme(hProgWnd, L" ", L" ");
	SendMessage(hProgWnd, PBM_SETBARCOLOR, 0, RGB(34, 177, 76));
}

static void InitMenuIcons(void)
{
	HBITMAP hTemp = NULL;

	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_ABOUTMENU));
	hAboutMenu = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_CUSTOM));
	hCustom = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_DIRECTORIES));
	hDirectories = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_EXIT));
	hExit = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_FULLSCREEN));
	hFullscreen = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_INTERFACE));
	hInterface = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_HELP));
	hHelp = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_MAMEHOME));
	hMameHome = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_PLAY));
	hPlay = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_PLAYM1));
	hPlayM1 = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_OPTIONS));
	hOptions = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_REFRESH));
	hRefresh = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_ZIP));
	hZip = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_SAVELIST));
	hSaveList = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_SAVEROMS));
	hSaveRoms = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_PLAYBACK));
	hPlayback = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_PROPERTIES));
	hProperties = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_AUDIT));
	hAuditMenu = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_VIDEO));
	hVideo = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_FONTS));
	hFonts = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_FOLDERS));
	hFolders = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_SORT));
	hSort = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_DRIVER));
	hDriver = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_FAQ));
	hFaq = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_TABS));
	hTabs = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_TROUBLE));
	hTrouble = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_COUNT));
	hCount = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_RELEASE));
	hRelease = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_TIME));
	hTime = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_DESCRIPTION));
	hDescription = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_ROM));
	hRom = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_SOURCE));
	hSource = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_MANUFACTURER));
	hManufacturer = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_YEAR));
	hYear = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_PLAYWAV));
	hPlaywav = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_FONT1));
	hFont1 = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_FONT2));
	hFont2 = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_INFOBACK));
	hInfoback = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_LISTBACK));
	hListback = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_TREEBACK));
	hTreeback = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_ASCENDING));
	hAscending = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_FIELDS));
	hFields = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_RECAVI));
	hRecavi = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_RECINPUT));
	hRecinput = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_RECWAV));
	hRecwav = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_PLAYMNG));
	hPlaymng = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_RANDOM));
	hRandom = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_RECMNG));
	hRecmng = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_SAVESTATE));
	hSavestate = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_FILTERS));
	hFilters = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_REMOVE));
	hRemove = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_RENAME));
	hRename = CreateBitmapTransparent(hTemp);
	hTemp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_RESET));
	hReset = CreateBitmapTransparent(hTemp);
}

static void CopyToolTipText(LPTOOLTIPTEXT lpttt)
{
	int iButton = lpttt->hdr.idFrom;
	int game = Picker_GetSelectedItem(hWndList);
	static TCHAR String[1024];
	bool bConverted = false;

	/* Map command ID to string index */
	for (int i = 0; CommandToString[i] != -1; i++)
	{
		if (CommandToString[i] == iButton)
		{
			iButton = i;
			bConverted = true;
			break;
		}
	}

	if (bConverted)
	{
		/* Check for valid parameter */
		if (iButton > NUM_TOOLTIPS)
			_tcscpy(String, TEXT("Invalid button index"));
		else
			_tcscpy(String, szTbStrings[iButton]);
	}
	else
		_tcscpy(String, win_wstring_from_utf8(GetDriverGameTitle(game)));

	lpttt->lpszText = String;
}

static void InitToolbar(void)
{
	RECT rect;

	hToolBar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS, 0, 0, NUM_TOOLBUTTONS * 32, 32, hMain, NULL, hInst, NULL);
	HBITMAP hBitmap = (HBITMAP)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_TOOLBAR), IMAGE_BITMAP, 0, 0, LR_SHARED);
	HIMAGELIST hToolList = ImageList_Create(32, 32, ILC_COLORDDB | ILC_MASK, NUM_TOOLBUTTONS, 0);	
	ImageList_AddMasked(hToolList, hBitmap, RGB(0, 0, 0));
	DeleteObject(hBitmap);
	SendMessage(hToolBar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DOUBLEBUFFER); 	
	SendMessage(hToolBar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	SendMessage(hToolBar, TB_SETIMAGELIST, 0, (LPARAM)hToolList);
	SendMessage(hToolBar, TB_ADDBUTTONS, NUM_TOOLBUTTONS, (LPARAM)&tbb);
	SendMessage(hToolBar, TB_AUTOSIZE, 0, 0); 	
	// get Edit Control position
	int idx = SendMessage(hToolBar, TB_BUTTONCOUNT, 0, 0) - 1;
	SendMessage(hToolBar, TB_GETITEMRECT, idx, (LPARAM)&rect);
	int iPosX = rect.right + 8;
	int iPosY = (rect.bottom - rect.top) / 4;
	int iHeight = rect.bottom - rect.top - 17;
	// create Search Edit Control
	hSearchWnd = CreateWindowEx(0, WC_EDIT, TEXT(SEARCH_PROMPT), ES_LEFT | WS_CHILD | WS_CLIPSIBLINGS | WS_BORDER | WS_VISIBLE, iPosX, iPosY, 200, iHeight, hToolBar, (HMENU)ID_TOOLBAR_EDIT, hInst, NULL );
}

static void InitTabView(void)
{
	hTabCtrl = CreateWindowEx(0, WC_TABCONTROL, NULL, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | WS_TABSTOP | TCS_HOTTRACK, 0, 0, 0, 0, hMain, (HMENU)IDC_SSTAB, hInst, NULL);
}

static void InitStatusBar(void)
{
	hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | SBARS_SIZEGRIP | SBARS_TOOLTIPS, 0, 0, 0, 0, hMain, NULL, hInst, NULL);
}

static char *GameInfoStatusBar(int driver_index)
{
	static char status[64];

	memset(&status, 0, sizeof(status));

	if (DriverIsBroken(driver_index))
		return strcpy(status, "Not working");
	else if (DriverIsImperfect(driver_index))
		return strcpy(status, "Working with problems");
	else
		return strcpy(status, "Working");
}

static char *GameInfoScreen(int driver_index)
{
	machine_config config(driver_list::driver(driver_index), MameUIGlobal());
	static char scrtxt[256];

	memset(&scrtxt, 0, sizeof(scrtxt));

	if (DriverIsVector(driver_index))
	{
		if (DriverIsVertical(driver_index))
			strcpy(scrtxt, "Vector (V)");
		else
			strcpy(scrtxt, "Vector (H)");
	}
	else
	{
		const screen_device *screen = config.first_screen();

		if (screen == nullptr)
			strcpy(scrtxt, "Screenless");
		else
		{
			const rectangle &visarea = screen->visible_area();
			char tmpbuf[256];

			if (DriverIsVertical(driver_index))
				snprintf(tmpbuf, WINUI_ARRAY_LENGTH(tmpbuf), "%d x %d (V) %f Hz", visarea.width(), visarea.height(), ATTOSECONDS_TO_HZ(screen->refresh_attoseconds()));
			else
				snprintf(tmpbuf, WINUI_ARRAY_LENGTH(tmpbuf), "%d x %d (H) %f Hz", visarea.width(), visarea.height(), ATTOSECONDS_TO_HZ(screen->refresh_attoseconds()));

			strcat(scrtxt, tmpbuf);
		}
	}

	return scrtxt;
}

static void UpdateStatusBar(void)
{
	LPTREEFOLDER lpFolder = GetCurrentFolder();
	int games_shown = 0;
	int i = -1;
	HICON hIconFX = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAMEUI_ICON), IMAGE_ICON, 16, 16, LR_SHARED);

	if (!lpFolder)
		return;

	while (1)
	{
		i = FindGame(lpFolder, i + 1);

		if (i == -1)
			break;

		if (!GameFiltered(i, lpFolder->m_dwFlags))
			games_shown++;
	}

	/* Show number of games in the current 'View' in the status bar */
	SetStatusBarTextF(4, g_szGameCountString, games_shown);
	i = Picker_GetSelectedItem(hWndList);

	if (games_shown == 0)
		DisableSelection();
	else
	{
		const char *pText = GetDriverGameTitle(i);
		char *pStatus = GameInfoStatusBar(i);
		char *pScreen = GameInfoScreen(i);
		const char *pName = GetDriverGameName(i);
		SetStatusBarText(0, pText);
		SetStatusBarText(1, pName);
		SendMessage(hStatusBar, SB_SETICON, 1, (LPARAM)GetSelectedPickItemIconSmall());
		SetStatusBarText(2, pStatus);
		SetStatusBarText(3, pScreen);
		SetStatusBarText(5, MAMEUINAME);
		SendMessage(hStatusBar, SB_SETICON, 5, (LPARAM)hIconFX);
	}
}

static void ResetFonts(void)
{
	LOGFONT font;
	LOGFONT font1;
	LOGFONT font2;
	LOGFONT font3;

	GetGuiFont(&font);

	if (hFontGui != NULL)
		DeleteFont(hFontGui);

	hFontGui = CreateFontIndirect(&font);

	if (hFontGui != NULL)
	{
		SetWindowFont(hSearchWnd, hFontGui, true);
		SetWindowFont(hTabCtrl, hFontGui, true);
		SetWindowFont(hStatusBar, hFontGui, true);
	}

	GetListFont(&font1);

	if (hFontList != NULL)
		DeleteFont(hFontList);

	hFontList = CreateFontIndirect(&font1);

	if (hFontList != NULL)
		SetWindowFont(hWndList, hFontList, true);

	GetHistoryFont(&font2);

	if (hFontHist != NULL)
		DeleteFont(hFontHist);

	hFontHist = CreateFontIndirect(&font2);

	if (hFontHist != NULL)
		SetWindowFont(GetDlgItem(hMain, IDC_HISTORY), hFontHist, true);

	GetTreeFont(&font3);

	if (hFontTree != NULL)
		DeleteFont(hFontTree);

	hFontTree = CreateFontIndirect(&font3);

	if (hFontTree != NULL)
		SetWindowFont(hTreeView, hFontTree, true);
}

static void InitListTree(void)
{
	hTreeView = GetDlgItem(hMain, IDC_TREE);
	hWndList = GetDlgItem(hMain, IDC_LIST);
	SetWindowTheme(hWndList, L"Explorer", NULL);
	SetWindowTheme(hTreeView, L"Explorer", NULL);

	if (IsWindowsSevenOrHigher())
	{
		(void)ListView_SetExtendedListViewStyle(hWndList, LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP | LVS_EX_LABELTIP | LVS_EX_ONECLICKACTIVATE | LVS_EX_DOUBLEBUFFER);
		SendMessage(hTreeView, TVM_SETEXTENDEDSTYLE, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
	}
	else
		(void)ListView_SetExtendedListViewStyle(hWndList, LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP | LVS_EX_LABELTIP | LVS_EX_UNDERLINEHOT | LVS_EX_ONECLICKACTIVATE | LVS_EX_DOUBLEBUFFER);
}

static void UpdateHistory(void)
{
	have_history = false;

	hBrushDlg = CreateSolidBrush(GetHistoryBgColor());

	if (GetSelectedPick() >= 0)
	{
		char *histText = GetGameHistory(Picker_GetSelectedItem(hWndList));
		have_history = (histText && histText[0]) ? true : false;
		win_set_window_text_utf8(GetDlgItem(hMain, IDC_HISTORY), histText);
	}

	if (have_history && GetShowScreenShot() && ((TabView_GetCurrentTab(hTabCtrl) == TAB_HISTORY) ||
		(TabView_GetCurrentTab(hTabCtrl) == GetHistoryTab() && GetShowTab(TAB_HISTORY) == false) ||
		(TAB_ALL == GetHistoryTab() && GetShowTab(TAB_HISTORY) == false) ))
		ShowWindow(GetDlgItem(hMain, IDC_HISTORY), SW_SHOW);
	else
		ShowWindow(GetDlgItem(hMain, IDC_HISTORY), SW_HIDE);
}

static void DisableSelection(void)
{
	MENUITEMINFO mmi;
	HMENU hMenu = GetMenu(hMain);
	bool prev_have_selection = have_selection;

	mmi.cbSize = sizeof(mmi);
	mmi.fMask = MIIM_TYPE;
	mmi.fType = MFT_STRING;
	mmi.dwTypeData = (TCHAR *)TEXT("Play\tAlt+O");
	mmi.cch = _tcslen(mmi.dwTypeData);

	SetMenuItemInfo(hMenu, ID_FILE_PLAY, false, &mmi);
	EnableMenuItem(hMenu, ID_FILE_PLAY, 		MF_GRAYED);
	EnableMenuItem(hMenu, ID_FILE_PLAY_RECORD,	MF_GRAYED);
	EnableMenuItem(hMenu, ID_GAME_PROPERTIES,	MF_GRAYED);
	SetStatusBarText(0, "No selection");
	SetStatusBarText(1, "");
	SendMessage(hStatusBar, SB_SETICON, 1, (LPARAM)NULL);
	SetStatusBarText(2, "");
	SetStatusBarText(3, "");
	SetStatusBarText(4, "");
	have_selection = false;

	if (prev_have_selection != have_selection)
		UpdateScreenShot();
}

static void EnableSelection(int nGame)
{
	TCHAR buf[200];
	MENUITEMINFO mmi;
	HMENU hMenu = GetMenu(hMain);
	TCHAR *t_description = win_wstring_from_utf8(ConvertAmpersandString(GetDriverGameTitle(nGame)));

	if( !t_description )
		return;

	_sntprintf(buf, WINUI_ARRAY_LENGTH(buf), g_szPlayGameString, t_description);

	mmi.cbSize = sizeof(mmi);
	mmi.fMask = MIIM_TYPE;
	mmi.fType = MFT_STRING;
	mmi.dwTypeData = buf;
	mmi.cch = _tcslen(mmi.dwTypeData);

	SetMenuItemInfo(hMenu, ID_FILE_PLAY, false, &mmi);
	const char *pText = GetDriverGameTitle(nGame);
	SetStatusBarText(0, pText);
	const char *pName = GetDriverGameName(nGame);
	SetStatusBarText(1, pName);
	SendMessage(hStatusBar, SB_SETICON, 1, (LPARAM)GetSelectedPickItemIconSmall());
	char *pStatus = GameInfoStatusBar(nGame);
	SetStatusBarText(2, pStatus);
	char *pScreen = GameInfoScreen(nGame);
	SetStatusBarText(3, pScreen);
	EnableMenuItem(hMenu, ID_FILE_PLAY, 		MF_ENABLED);
	EnableMenuItem(hMenu, ID_FILE_PLAY_RECORD,	MF_ENABLED);
	EnableMenuItem(hMenu, ID_GAME_PROPERTIES, 	MF_ENABLED);

	if (bProgressShown && bListReady == true)
		SetDefaultGame(GetDriverGameName(nGame));

	have_selection = true;
	UpdateScreenShot();
	free(t_description);
}

static const char* GetCloneParentName(int nItem)
{
	if (DriverIsClone(nItem))
	{
		int nParentIndex = GetParentIndex(&driver_list::driver(nItem));

		if( nParentIndex >= 0)
			return GetDriverGameTitle(nParentIndex);
	}

	return "";
}

static bool TreeViewNotify(LPNMHDR nm)
{
	switch (nm->code)
	{
		case TVN_SELCHANGED :
		{
			HTREEITEM hti = TreeView_GetSelection(hTreeView);
			TVITEM tvi;

			tvi.mask = TVIF_PARAM | TVIF_HANDLE;
			tvi.hItem = hti;

			if (TreeView_GetItem(hTreeView, &tvi))
			{
				SetCurrentFolder((LPTREEFOLDER)tvi.lParam);

				if (bListReady)
				{
					UpdateListView();
					UpdateScreenShot();
					SetFocus(hTreeView);
				}
			}

			return true;
		}

		case TVN_BEGINLABELEDIT :
		{
			TV_DISPINFO *ptvdi = (TV_DISPINFO *)nm;
			LPTREEFOLDER folder = (LPTREEFOLDER)ptvdi->item.lParam;

			if (folder->m_dwFlags & F_CUSTOM)
			{
				// user can edit custom folder names
				g_in_treeview_edit = true;
				return false;
			}

			// user can't edit built in folder names
			return true;
		}

		case TVN_ENDLABELEDIT :
		{
			TV_DISPINFO *ptvdi = (TV_DISPINFO *)nm;
			LPTREEFOLDER folder = (LPTREEFOLDER)ptvdi->item.lParam;

			g_in_treeview_edit = false;

			if (ptvdi->item.pszText == NULL || _tcslen(ptvdi->item.pszText) == 0)
				return false;

			char *utf8_szText = win_utf8_from_wstring(ptvdi->item.pszText);

			if (!utf8_szText)
				return false;

			bool result = TryRenameCustomFolder(folder, utf8_szText);
			free(utf8_szText);
			return result;
		}
	}

	return false;
}

static void GamePicker_OnHeaderContextMenu(POINT pt, int nColumn)
{
	// Right button was clicked on header
	MENUINFO mi;
	HMENU hMenuLoad = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_HEADER));
	HMENU hMenu = GetSubMenu(hMenuLoad, 0);

	memset(&mi, 0, sizeof(MENUINFO));
	mi.cbSize = sizeof(MENUINFO);
	mi.fMask = MIM_BACKGROUND | MIM_STYLE;
	mi.dwStyle = MNS_CHECKORBMP;
	mi.hbrBack = GetSysColorBrush(COLOR_WINDOW);

	SetMenuInfo(hMenu, &mi);
	SetMenuItemBitmaps(hMenu, ID_SORT_ASCENDING, MF_BYCOMMAND, hAscending, hAscending);
	SetMenuItemBitmaps(hMenu, ID_SORT_DESCENDING, MF_BYCOMMAND, hSort, hSort);
	SetMenuItemBitmaps(hMenu, ID_CUSTOMIZE_FIELDS, MF_BYCOMMAND, hFields, hFields);
	lastColumnClick = nColumn;
	TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hMain, NULL);
	DestroyMenu(hMenuLoad);
}

static char* ConvertAmpersandString(const char *s)
{
	/* takes a string and changes any ampersands to double ampersands,
       for setting text of window controls that don't allow us to disable
       the ampersand underlining.
      */
	/* returns a static buffer--use before calling again */

	static char buf[200];

	char *ptr = buf;

	while (*s)
	{
		if (*s == '&')
			*ptr++ = *s;

		*ptr++ = *s++;
	}

	*ptr = 0;

	return buf;
}

static void PollGUIJoystick()
{
	if (in_emulation)
		return;

	if (g_pJoyGUI == NULL)
		return;

	g_pJoyGUI->poll_joysticks();

	// User pressed UP
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyUp(0), GetUIJoyUp(1), GetUIJoyUp(2), GetUIJoyUp(3))))
		SendMessage(hMain, WM_COMMAND, ID_UI_UP, 0);

	// User pressed DOWN
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyDown(0), GetUIJoyDown(1), GetUIJoyDown(2), GetUIJoyDown(3))))
		SendMessage(hMain, WM_COMMAND, ID_UI_DOWN, 0);

	// User pressed LEFT
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyLeft(0), GetUIJoyLeft(1), GetUIJoyLeft(2), GetUIJoyLeft(3))))
		SendMessage(hMain, WM_COMMAND, ID_UI_LEFT, 0);

	// User pressed RIGHT
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyRight(0), GetUIJoyRight(1), GetUIJoyRight(2), GetUIJoyRight(3))))
		SendMessage(hMain, WM_COMMAND, ID_UI_RIGHT, 0);

	// User pressed START GAME
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyStart(0), GetUIJoyStart(1), GetUIJoyStart(2), GetUIJoyStart(3))))
		SendMessage(hMain, WM_COMMAND, ID_UI_START, 0);

	// User pressed PAGE UP
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyPageUp(0), GetUIJoyPageUp(1), GetUIJoyPageUp(2), GetUIJoyPageUp(3))))
		SendMessage(hMain, WM_COMMAND, ID_UI_PGUP, 0);

	// User pressed PAGE DOWN
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyPageDown(0), GetUIJoyPageDown(1), GetUIJoyPageDown(2), GetUIJoyPageDown(3))))
		SendMessage(hMain, WM_COMMAND, ID_UI_PGDOWN, 0);

	// User pressed HOME
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyHome(0), GetUIJoyHome(1), GetUIJoyHome(2), GetUIJoyHome(3))))
		SendMessage(hMain, WM_COMMAND, ID_UI_HOME, 0);

	// User pressed END
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyEnd(0), GetUIJoyEnd(1), GetUIJoyEnd(2), GetUIJoyEnd(3))))
		SendMessage(hMain, WM_COMMAND, ID_UI_END, 0);

	// User pressed CHANGE SCREENSHOT
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoySSChange(0), GetUIJoySSChange(1), GetUIJoySSChange(2), GetUIJoySSChange(3))))
		SendMessage(hMain, WM_COMMAND, IDC_SSFRAME, 0);

	// User pressed SCROLL HISTORY UP
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyHistoryUp(0), GetUIJoyHistoryUp(1), GetUIJoyHistoryUp(2), GetUIJoyHistoryUp(3))))
		SendMessage(hMain, WM_COMMAND, ID_UI_HISTORY_UP, 0);

	// User pressed SCROLL HISTORY DOWN
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyHistoryDown(0), GetUIJoyHistoryDown(1), GetUIJoyHistoryDown(2), GetUIJoyHistoryDown(3))))
		SendMessage(hMain, WM_COMMAND, ID_UI_HISTORY_DOWN, 0);
}

static void SetView(int menu_id)
{
	// first uncheck previous menu item, check new one
	CheckMenuRadioItem(GetMenu(hMain), ID_VIEW_ICONS_LARGE, ID_VIEW_ICONS_SMALL, menu_id, MF_CHECKED);
	ToolBar_CheckButton(hToolBar, menu_id, MF_CHECKED);

	// Associate the image lists with the list view control.
	if (menu_id == ID_VIEW_ICONS_LARGE)
		(void)ListView_SetImageList(hWndList, hLarge, LVSIL_SMALL);
	else
		(void)ListView_SetImageList(hWndList, hSmall, LVSIL_SMALL);

	for (int i = 0; i < sizeof(s_nPickers) / sizeof(s_nPickers[0]); i++)
		Picker_SetViewID(GetDlgItem(hMain, s_nPickers[i]), menu_id - ID_VIEW_ICONS_LARGE);

	for (int i = 0; i < sizeof(s_nPickers) / sizeof(s_nPickers[0]); i++)
		Picker_Sort(GetDlgItem(hMain, s_nPickers[i]));
}

static void ResetListView()
{
	int i = 0;
	LVITEM lvi;
	bool no_selection = false;
	LPTREEFOLDER lpFolder = GetCurrentFolder();

	if (!lpFolder)
		return;
 
	/* If the last folder was empty, no_selection is true */
	if (have_selection == false)
		no_selection = true;
 
	int current_game = Picker_GetSelectedItem(hWndList);
	SetWindowRedraw(hWndList, false);
	(void)ListView_DeleteAllItems(hWndList);
	// hint to have it allocate it all at once
	ListView_SetItemCount(hWndList, driver_list::total());

	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_INDENT;
	lvi.stateMask = 0;

	i = -1;

	do
	{
		/* Add the games that are in this folder */
		if ((i = FindGame(lpFolder, i + 1)) != -1)
		{
			if (GameFiltered(i, lpFolder->m_dwFlags))
				continue;

			lvi.iItem = i;
			lvi.iSubItem = 0;
			lvi.lParam = i;
			lvi.pszText = LPSTR_TEXTCALLBACK;
			lvi.iImage = I_IMAGECALLBACK;
			lvi.iIndent = 0;

			if (GetEnableIndent())
			{
				if (GetParentFound(i) && DriverIsClone(i))
					lvi.iIndent = 1;
				else
					lvi.iIndent = 0;
			}

			(void)ListView_InsertItem(hWndList, &lvi);
		}
	} while (i != -1);

	Picker_Sort(hWndList);

	if (bListReady)
	{
	/* If last folder was empty, select the first item in this folder */
		if (no_selection)
			Picker_SetSelectedPick(hWndList, 0);
		else
			Picker_SetSelectedItem(hWndList, current_game);
	}

	SetWindowRedraw(hWndList, true);
	UpdateStatusBar();
}

static void UpdateGameList(void)
{
	for (int i = 0; i < driver_list::total(); i++)
	{
		SetRomAuditResults(i, UNKNOWN);
	}

	game_index = 0;
	game_total = driver_list::total();
	oldpercent = -1;
	bDoGameCheck = true;
	bFolderCheck = false;
	idle_work = true;
	ReloadIcons();
	Picker_ResetIdle(hWndList);
}

static UINT_PTR CALLBACK HookProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
		case WM_INITDIALOG:
			CenterWindow(hDlg);
			hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAMEUI_ICON));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			hBrush = CreateSolidBrush(RGB(240, 240, 240));
			if (bHookFont)
				win_set_window_text_utf8(hDlg, "Choose a font");
			else
				win_set_window_text_utf8(hDlg, "Choose a color");
			break;

		case WM_CTLCOLORDLG:
			return (LRESULT) hBrush;	

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			return (LRESULT) hBrush;

		case WM_COMMAND:
			switch (GET_WM_COMMAND_ID(wParam, lParam))
			{
				case IDOK:
					bChangedHook = true;
					break;

				case IDCANCEL:
					bChangedHook = false;
					break;
			}

			break;

		case WM_DESTROY:
			DestroyIcon(hIcon);
			DeleteObject(hBrush);
			return true;	
	}

	return false;
}

static void PickFont(LOGFONT *font, COLORREF *color)
{
	CHOOSEFONT cf;
	bChangedHook = false;
	bHookFont = true;

	cf.lStructSize = sizeof(CHOOSEFONT);
	cf.hwndOwner = hMain;
	cf.lpLogFont = font;
	cf.lpfnHook = &HookProc;
	cf.rgbColors = *color;
	cf.Flags = CF_BOTH | CF_INITTOLOGFONTSTRUCT | CF_EFFECTS | CF_ENABLEHOOK;

	if (!ChooseFont(&cf))
		return;

	*color = cf.rgbColors;
}

static void PickListFont(void)
{
	LOGFONT FontList;

	GetListFont(&FontList); 
	COLORREF ColorList = GetListFontColor();
	PickFont(&FontList, &ColorList);

	if (bChangedHook)
	{
		SetListFont(&FontList);
		SetListFontColor(ColorList);

		if (hFontList != NULL)
			DeleteFont(hFontList);

		hFontList = CreateFontIndirect(&FontList);

		if (hFontList != NULL)
		{
			SetWindowFont(hWndList, hFontList, true);
			(void)ListView_SetTextColor(hWndList, ColorList);
			UpdateListView();
		}
	}
}

static void PickHistoryFont(void)
{
	LOGFONT FontHist;

	GetHistoryFont(&FontHist); 
	COLORREF ColorHist = GetHistoryFontColor();
	PickFont(&FontHist, &ColorHist);

	if (bChangedHook)
	{
		SetHistoryFont(&FontHist);
		SetHistoryFontColor(ColorHist);

		if (hFontHist != NULL)
			DeleteFont(hFontHist);

		hFontHist = CreateFontIndirect(&FontHist);

		if (hFontHist != NULL)
			SetWindowFont(GetDlgItem(hMain, IDC_HISTORY), hFontHist, true);
	}
}

static void PickFoldersFont(void)
{
	LOGFONT FontTree;

	GetTreeFont(&FontTree); 
	COLORREF ColorTree = GetTreeFontColor();
	PickFont(&FontTree, &ColorTree);

	if (bChangedHook)
	{
		SetTreeFont(&FontTree);
		SetTreeFontColor(ColorTree);

		if (hFontTree != NULL)
			DeleteFont(hFontTree);

		hFontTree = CreateFontIndirect(&FontTree);

		if (hFontTree != NULL)
		{
			SetWindowFont(hTreeView, hFontTree, true);
			(void)TreeView_SetTextColor(hTreeView, ColorTree);
		}
	}
}

static void PickColor(COLORREF *cDefault)
{
	CHOOSECOLOR cc;
	COLORREF choice_colors[16];
	bChangedHook = false;
	bHookFont = false;

	for (int i = 0; i < 16; i++)
		choice_colors[i] = GetCustomColor(i);

	cc.lStructSize = sizeof(CHOOSECOLOR);
	cc.hwndOwner = hMain;
	cc.lpfnHook = &HookProc;
	cc.rgbResult = *cDefault;
	cc.lpCustColors = choice_colors;
	cc.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_FULLOPEN | CC_ENABLEHOOK;

	if (!ChooseColor(&cc))
		return;

	for (int i = 0; i < 16; i++)
		SetCustomColor(i,choice_colors[i]);

	*cDefault = cc.rgbResult;
}

static void PickTreeBgColor(void)
{
	COLORREF cTreeColor = GetFolderBgColor();
	PickColor(&cTreeColor);

	if (bChangedHook)
	{
		SetFolderBgColor(cTreeColor);
		(void)TreeView_SetBkColor(hTreeView, GetFolderBgColor());
	}
}
 
static void PickHistoryBgColor(void)
{
	COLORREF cHistoryColor = GetHistoryBgColor();
	PickColor(&cHistoryColor);

	if (bChangedHook)
	{
		SetHistoryBgColor(cHistoryColor);
		UpdateScreenShot();
	}
}

static void PickListBgColor(void)
{
	COLORREF cListColor = GetListBgColor();
	PickColor(&cListColor);

	if (bChangedHook)
	{
		SetListBgColor(cListColor);
		(void)ListView_SetBkColor(hWndList, GetListBgColor());
		UpdateListView();
	}
}

static bool MameCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
	LPTREEFOLDER folder;

	switch (id)
	{
		case ID_FILE_PLAY:
			MamePlayGame();
			SetFocus(hWndList);
			return true;

		case ID_FILE_PLAY_RECORD:
			MamePlayRecordGame();
			SetFocus(hWndList);
			return true;

		case ID_FILE_PLAY_BACK:
			MamePlayBackGame();
			SetFocus(hWndList);
			return true;

		case ID_FILE_PLAY_RECORD_WAVE:
			MamePlayRecordWave();
			SetFocus(hWndList);
			return true;

		case ID_FILE_PLAY_RECORD_MNG:
			MamePlayRecordMNG();
			SetFocus(hWndList);
			return true;

		case ID_FILE_PLAY_RECORD_AVI:
			MamePlayRecordAVI();
			SetFocus(hWndList);
			return true;

		case ID_FILE_LOADSTATE :
			MameLoadState();
			SetFocus(hWndList);
			return true;

		case ID_FILE_AUDIT:
			AuditDialog();
			DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_AUDIT), hMain, AuditWindowProc);
			UpdateListView();
			return true;

		case ID_FILE_GAMELIST:
			if (CommonListDialog(GetOpenFileName, FILETYPE_GAME_LIST))
				return true;
			else
				break;

		case ID_FILE_ROMSLIST:
			if (CommonListDialog(GetOpenFileName, FILETYPE_ROMS_LIST))
				return true;
			else
				break;

		case ID_FILE_EXIT:
			PostMessage(hMain, WM_CLOSE, 0, 0);
			return true;

		case ID_VIEW_ICONS_LARGE:
			SetView(ID_VIEW_ICONS_LARGE);
			UpdateListView();
			return true;

		case ID_VIEW_ICONS_SMALL:
			SetView(ID_VIEW_ICONS_SMALL);
			UpdateListView();
			return true;

		/* Arrange Icons submenu */
		case ID_VIEW_BYGAME:
			SetSortReverse(false);
			SetSortColumn(COLUMN_GAMES);
			Picker_Sort(hWndList);
			break;

		case ID_VIEW_BYDIRECTORY:
			SetSortReverse(false);
			SetSortColumn(COLUMN_ROMNAME);
			Picker_Sort(hWndList);
			break;

		case ID_VIEW_BYMANUFACTURER:
			SetSortReverse(false);
			SetSortColumn(COLUMN_MANUFACTURER);
			Picker_Sort(hWndList);
			break;

		case ID_VIEW_BYYEAR:
			SetSortReverse(false);
			SetSortColumn(COLUMN_YEAR);
			Picker_Sort(hWndList);
			break;

		case ID_VIEW_BYSOURCE:
			SetSortReverse(false);
			SetSortColumn(COLUMN_SOURCEFILE);
			Picker_Sort(hWndList);
			break;

		case ID_VIEW_BYTIMESPLAYED:
			SetSortReverse(false);
			SetSortColumn(COLUMN_PLAYED);
			Picker_Sort(hWndList);
			break;

		case ID_ENABLE_INDENT:
			bEnableIndent = !bEnableIndent;
			SetEnableIndent(bEnableIndent);
			CheckMenuItem(GetMenu(hMain), ID_ENABLE_INDENT, (bEnableIndent) ? MF_CHECKED : MF_UNCHECKED);
			ToolBar_CheckButton(hToolBar, ID_ENABLE_INDENT, (bEnableIndent) ? MF_CHECKED : MF_UNCHECKED);
			UpdateListView();
			break;

		case ID_VIEW_FOLDERS:
			bShowTree = !bShowTree;
			SetShowFolderList(bShowTree);
			CheckMenuItem(GetMenu(hMain), ID_VIEW_FOLDERS, (bShowTree) ? MF_CHECKED : MF_UNCHECKED);
			ToolBar_CheckButton(hToolBar, ID_VIEW_FOLDERS, (bShowTree) ? MF_CHECKED : MF_UNCHECKED);
			UpdateScreenShot();
			break;

		case ID_VIEW_TOOLBARS:
			bShowToolBar = !bShowToolBar;
			SetShowToolBar(bShowToolBar);
			CheckMenuItem(GetMenu(hMain), ID_VIEW_TOOLBARS, (bShowToolBar) ? MF_CHECKED : MF_UNCHECKED);
			ToolBar_CheckButton(hToolBar, ID_VIEW_TOOLBARS, (bShowToolBar) ? MF_CHECKED : MF_UNCHECKED);
			ShowWindow(hToolBar, (bShowToolBar) ? SW_SHOW : SW_HIDE);
			ResizePickerControls(hMain);
			UpdateScreenShot();
			break;

		case ID_VIEW_STATUS:
			bShowStatusBar = !bShowStatusBar;
			SetShowStatusBar(bShowStatusBar);
			CheckMenuItem(GetMenu(hMain), ID_VIEW_STATUS, (bShowStatusBar) ? MF_CHECKED : MF_UNCHECKED);
			ShowWindow(hStatusBar, (bShowStatusBar) ? SW_SHOW : SW_HIDE);
			ResizePickerControls(hMain);
			UpdateScreenShot();
			break;

		case ID_VIEW_PAGETAB:
			bShowTabCtrl = !bShowTabCtrl;
			SetShowTabCtrl(bShowTabCtrl);
			ShowWindow(hTabCtrl, (bShowTabCtrl) ? SW_SHOW : SW_HIDE);
			ResizePickerControls(hMain);
			UpdateScreenShot();
			InvalidateRect(hMain, NULL, true);
			break;

		case ID_VIEW_FULLSCREEN:
			SwitchFullScreenMode();
			break;

		case ID_TOOLBAR_EDIT:
		{
			char buf[256];
			win_get_window_text_utf8(hWndCtl, buf, WINUI_ARRAY_LENGTH(buf));

			switch (codeNotify)
			{
				case TOOLBAR_EDIT_ACCELERATOR_PRESSED:
				{
					HWND hToolbarEdit = GetDlgItem(hToolBar, ID_TOOLBAR_EDIT);
					SetFocus(hToolbarEdit);
					break;
				}

				case EN_CHANGE:
					//put search routine here first, add a 200ms timer later.
					if ((!_stricmp(buf, SEARCH_PROMPT) && !_stricmp(g_SearchText, "")) ||
					(!_stricmp(g_SearchText, SEARCH_PROMPT) && !_stricmp(buf, "")))
						strcpy(g_SearchText, buf);
					else
					{
						strcpy(g_SearchText, buf);
						ResetListView();
					}

					break;

				case EN_SETFOCUS:
					if (!_stricmp(buf, SEARCH_PROMPT))
					win_set_window_text_utf8(hWndCtl, "");

					break;

				case EN_KILLFOCUS:
					if (*buf == 0)
						win_set_window_text_utf8(hWndCtl, SEARCH_PROMPT);

					break;
			}

			break;
		}

		case ID_GAME_INFO:
			(void)DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_GAME_INFO), hMain, GamePropertiesDialogProc, Picker_GetSelectedItem(hWndList));
			SetFocus(hWndList);
			break;

		case ID_GAME_AUDIT:
			(void)DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_GAME_AUDIT), hMain, GameAuditDialogProc, Picker_GetSelectedItem(hWndList));
			UpdateStatusBar();
			SetFocus(hWndList);
			break;

		/* ListView Context Menu */
		case ID_CONTEXT_ADD_CUSTOM:
			(void)DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_CUSTOM_FILE), hMain, AddCustomFileDialogProc, Picker_GetSelectedItem(hWndList));
			SetFocus(hWndList);
			break;

		case ID_CONTEXT_REMOVE_CUSTOM:
			RemoveCurrentGameCustomFolder();
			SetFocus(hWndList);
			break;

		/* Tree Context Menu */
		case ID_CONTEXT_FILTERS:
			if (DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_FILTERS), hMain, FilterDialogProc) == true)
				UpdateListView();
			return true;

		// ScreenShot Context Menu
		// select current tab
		case ID_VIEW_TAB_SCREENSHOT:
		case ID_VIEW_TAB_TITLE:
		case ID_VIEW_TAB_SCORES:
		case ID_VIEW_TAB_HOWTO:
		case ID_VIEW_TAB_SELECT:
		case ID_VIEW_TAB_VERSUS:
		case ID_VIEW_TAB_BOSSES:
		case ID_VIEW_TAB_ENDS:
		case ID_VIEW_TAB_GAMEOVER:
		case ID_VIEW_TAB_LOGO:
		case ID_VIEW_TAB_ARTWORK:
		case ID_VIEW_TAB_FLYER:
		case ID_VIEW_TAB_CABINET:
		case ID_VIEW_TAB_MARQUEE:
		case ID_VIEW_TAB_CONTROL_PANEL:
		case ID_VIEW_TAB_PCB:
		case ID_VIEW_TAB_HISTORY:
			if (id == ID_VIEW_TAB_HISTORY && GetShowTab(TAB_HISTORY) == false)
				break;

			TabView_SetCurrentTab(hTabCtrl, id - ID_VIEW_TAB_SCREENSHOT);
			UpdateScreenShot();
			TabView_UpdateSelection(hTabCtrl);
			break;

		// toggle tab's existence
		case ID_TOGGLE_TAB_SCREENSHOT:
		case ID_TOGGLE_TAB_TITLE:
		case ID_TOGGLE_TAB_SCORES:
		case ID_TOGGLE_TAB_HOWTO:
		case ID_TOGGLE_TAB_SELECT:
		case ID_TOGGLE_TAB_VERSUS:
		case ID_TOGGLE_TAB_BOSSES:
		case ID_TOGGLE_TAB_ENDS:
		case ID_TOGGLE_TAB_GAMEOVER:
		case ID_TOGGLE_TAB_LOGO:
		case ID_TOGGLE_TAB_ARTWORK:
		case ID_TOGGLE_TAB_FLYER:
		case ID_TOGGLE_TAB_CABINET:
		case ID_TOGGLE_TAB_MARQUEE:
		case ID_TOGGLE_TAB_CONTROL_PANEL:
		case ID_TOGGLE_TAB_PCB:
		case ID_TOGGLE_TAB_HISTORY:
		{
			int toggle_flag = id - ID_TOGGLE_TAB_SCREENSHOT;

			if (AllowedToSetShowTab(toggle_flag,!GetShowTab(toggle_flag)) == false)
				// attempt to hide the last tab
				// should show error dialog? hide picture area? or ignore?
				break;

			SetShowTab(toggle_flag,!GetShowTab(toggle_flag));
			TabView_Reset(hTabCtrl);

			if (TabView_GetCurrentTab(hTabCtrl) == toggle_flag && GetShowTab(toggle_flag) == false)
				// we're deleting the tab we're on, so go to the next one
				TabView_CalculateNextTab(hTabCtrl);

			// Resize the controls in case we toggled to another history
			// mode (and the history control needs resizing).
			ResizePickerControls(hMain);
			UpdateScreenShot();
			TabView_UpdateSelection(hTabCtrl);
			break;
		}

		/* Header Context Menu */
		case ID_SORT_ASCENDING:
			SetSortReverse(false);
			SetSortColumn(Picker_GetRealColumnFromViewColumn(hWndList, lastColumnClick));
			Picker_Sort(hWndList);
			break;

		case ID_SORT_DESCENDING:
			SetSortReverse(true);
			SetSortColumn(Picker_GetRealColumnFromViewColumn(hWndList, lastColumnClick));
			Picker_Sort(hWndList);
			break;

		case ID_CUSTOMIZE_FIELDS:
			if (DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_COLUMNS), hMain, ColumnDialogProc) == true)
				ResetColumnDisplay();
			SetFocus(hWndList);
			return true;

		case ID_GAME_PROPERTIES:
		{
			int game = Picker_GetSelectedItem(hWndList);
			folder = GetFolderByName(FOLDER_SOURCE, GetDriverFileName(game));
			InitPropertyPage(hInst, hWnd, OPTIONS_GAME, folder->m_nFolderId, game);
			UpdateStatusBar();
			SetFocus(hWndList);
			return true;
		}

		case ID_FOLDER_PROPERTIES:
		{
			OPTIONS_TYPE curOptType = OPTIONS_SOURCE;
			folder = GetSelectedFolder();

			if(folder->m_nFolderId == FOLDER_RASTER) 
				curOptType = OPTIONS_RASTER;
			else if(folder->m_nFolderId == FOLDER_VECTOR) 
				curOptType = OPTIONS_VECTOR;
			else if(folder->m_nFolderId == FOLDER_HORIZONTAL) 
				curOptType = OPTIONS_HORIZONTAL;
			else if(folder->m_nFolderId == FOLDER_VERTICAL) 
				curOptType = OPTIONS_VERTICAL;

			InitPropertyPage(hInst, hWnd, curOptType, folder->m_nFolderId, Picker_GetSelectedItem(hWndList));
			UpdateStatusBar();
			SetFocus(hWndList);
			return true;
		}

		case ID_FOLDER_SOURCEPROPERTIES:
		{
			int game = Picker_GetSelectedItem(hWndList);
			folder = GetFolderByName(FOLDER_SOURCE, GetDriverFileName(game));
			InitPropertyPage(hInst, hWnd, OPTIONS_SOURCE, folder->m_nFolderId, game);
			UpdateStatusBar();
			SetFocus(hWndList);
			return true;
		}

		case ID_FOLDER_AUDIT:
			FolderCheck();
			UpdateListView();
			UpdateStatusBar();
			break;

		case ID_VIEW_PICTURE_AREA :
			ToggleScreenShot();
			break;

		case ID_UPDATE_GAMELIST:
			UpdateGameList();
			UpdateListView();
			UpdateStatusBar();
			break;

		case ID_OPTIONS_FONT:
			PickListFont();
			SetFocus(hWndList);
			return true;

		case ID_OPTIONS_HISTORY_FONT:
			PickHistoryFont();
			SetFocus(hWndList);
			return true;

		case ID_OPTIONS_TREE_FONT:
			PickFoldersFont();
			SetFocus(hWndList);
			return true;

		case ID_OPTIONS_FOLDERS_COLOR:
			PickTreeBgColor();
			SetFocus(hWndList);
			return true;

		case ID_OPTIONS_HISTORY_COLOR:
			PickHistoryBgColor();
			SetFocus(hWndList);
			return true;

		case ID_OPTIONS_LIST_COLOR:
			PickListBgColor();
			SetFocus(hWndList);
			return true;

		case ID_OPTIONS_DEFAULTS:
			InitPropertyPage(hInst, hWnd, OPTIONS_GLOBAL, -1, GLOBAL_OPTIONS);
			UpdateStatusBar();
			SetFocus(hWndList);
			return true;

		case ID_OPTIONS_DIR:
		{
			int nResult = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIRECTORIES), hMain, DirectoriesDialogProc);
			bool bUpdateRoms = ((nResult & DIRDLG_ROM) == DIRDLG_ROM) ? true : false;

			SaveInternalUI();

			/* update game list */
			if (bUpdateRoms == true)
				UpdateGameList();

			SetFocus(hWndList);
			return true;
		}

		case ID_OPTIONS_RESET_DEFAULTS:
			if (DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_RESET), hMain, ResetDialogProc) == true)
				PostMessage(hMain, WM_CLOSE, 0, 0);
			else 
				UpdateListView();
			return true;

		case ID_OPTIONS_INTERFACE:
			DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_INTERFACE_OPTIONS), hMain, InterfaceDialogProc);
			KillTimer(hMain, SCREENSHOT_TIMER);

			if( GetCycleScreenshot() > 0)
				SetTimer(hMain, SCREENSHOT_TIMER, GetCycleScreenshot()*1000, NULL ); 	// Scale to seconds

			SetFocus(hWndList);
			return true;

		case ID_MAME_HOMEPAGE:
			ShellExecuteCommon(hMain, "http://www.mamedev.org");
			SetFocus(hWndList);
			return true;

		case ID_MAME_FAQ:
			ShellExecuteCommon(hMain, "http://mamedev.org/devwiki/index.php?title=Frequently_Asked_Questions"); 
			SetFocus(hWndList);
			return true;

		case ID_PLAY_VIDEO:
		{
			char videoplay[MAX_PATH];
			*videoplay = 0;
			if (CommonFileDialog(GetOpenFileName, videoplay, FILETYPE_AVI_FILES, false))
				ShellExecuteCommon(hMain, videoplay);
			SetFocus(hWndList);
			return true;
		}

		case ID_PLAY_AUDIO:
		{
			char audioplay[MAX_PATH];
			*audioplay = 0;
			if (CommonFileDialog(GetOpenFileName, audioplay, FILETYPE_WAVE_FILES, false))
				ShellExecuteCommon(hMain, audioplay);
			SetFocus(hWndList);
			return true;
		}

		case ID_PLAY_MNG:
		{
			char mngplay[MAX_PATH];
			*mngplay = 0;
			if (CommonFileDialog(GetOpenFileName, mngplay, FILETYPE_MNG_FILES, false))
				ShellExecuteCommon(hMain, mngplay);
			SetFocus(hWndList);
			return true;
		}

		case ID_VIEW_ZIP:
		{
			char viewzip[MAX_PATH];
			int nGame = Picker_GetSelectedItem(hWndList);
			snprintf(viewzip, WINUI_ARRAY_LENGTH(viewzip), "%s\\%s.zip", GetRomDirs(), GetDriverGameName(nGame));
			ShellExecuteCommon(hMain, viewzip);
			SetFocus(hWndList);
			return true;
		}
		
		case ID_VIDEO_SNAP:
		{
			char videosnap[MAX_PATH];
			int nGame = Picker_GetSelectedItem(hWndList);
			snprintf(videosnap, WINUI_ARRAY_LENGTH(videosnap), "%s\\%s.mp4", GetMoviesDir(), GetDriverGameName(nGame));
			ShellExecuteCommon(hMain, videosnap);
			SetFocus(hWndList);
			return true;
		}

		case ID_PLAY_M1:
		{
			char command[MAX_PATH];
			int nGame = Picker_GetSelectedItem(hWndList);
			const char *game = GetDriverGameName(nGame);
			int audit_result = GetRomAuditResults(nGame);
			snprintf(command, WINUI_ARRAY_LENGTH(command), "m1fx.exe %s", game);

			if (IsAuditResultYes(audit_result))
			{
				TCHAR* t_command = win_wstring_from_utf8(command);
				STARTUPINFO siStartupInfo;
				PROCESS_INFORMATION piProcessInfo;
				memset(&siStartupInfo, 0, sizeof(STARTUPINFO));
				memset(&piProcessInfo, 0, sizeof(PROCESS_INFORMATION));
				siStartupInfo.cb = sizeof(STARTUPINFO);
				CreateProcess(NULL, t_command, NULL, NULL, false, CREATE_DEFAULT_ERROR_MODE, NULL, NULL, &siStartupInfo, &piProcessInfo);				
				free(t_command);
				SetFocus(hWndList);
				return true;
			}
			else
			{
				ErrorMessageBox("Game '%s' is missing ROMs!\r\nM1FX cannot be executed!", game);
				SetFocus(hWndList);
				return true;
			}
		}

		case ID_HELP_ABOUT:
			DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT), hMain, AboutDialogProc);
			SetFocus(hWndList);
			return true;

		case IDOK:
			/* cmk -- might need to check more codes here, not sure */
			if (codeNotify != EN_CHANGE && codeNotify != EN_UPDATE)
			{
				/* enter key */
				if (g_in_treeview_edit)
				{
					(void)TreeView_EndEditLabelNow(hTreeView, false);
					return true;
				}
				else if (have_selection)
					MamePlayGame();
			}

			break;

		case IDCANCEL: /* esc key */
			if (g_in_treeview_edit)
				(void)TreeView_EndEditLabelNow(hTreeView, true);

			break;

		case IDC_PLAY_GAME:
			if (have_selection)
				MamePlayGame();

			break;

		case ID_UI_START:
			SetFocus(hWndList);
			MamePlayGame();
			break;

		case ID_UI_UP:
			Picker_SetSelectedPick(hWndList, GetSelectedPick() - 1);
			break;

		case ID_UI_DOWN:
			Picker_SetSelectedPick(hWndList, GetSelectedPick() + 1);
			break;

		case ID_UI_PGUP:
			Picker_SetSelectedPick(hWndList, GetSelectedPick() - ListView_GetCountPerPage(hWndList));
			break;

		case ID_UI_PGDOWN:
			if ((GetSelectedPick() + ListView_GetCountPerPage(hWndList)) < ListView_GetItemCount(hWndList))
				Picker_SetSelectedPick(hWndList, GetSelectedPick() + ListView_GetCountPerPage(hWndList));
			else
				Picker_SetSelectedPick(hWndList, ListView_GetItemCount(hWndList) - 1);
			break;

		case ID_UI_HOME:
			Picker_SetSelectedPick(hWndList, 0);
			break;

		case ID_UI_END:
			Picker_SetSelectedPick(hWndList, ListView_GetItemCount(hWndList) - 1);
			break;

		case ID_UI_LEFT:
			/* hmmmmm..... */
			SendMessage(hWndList, WM_HSCROLL, SB_LINELEFT, 0);
			break;

		case ID_UI_RIGHT:
			/* hmmmmm..... */
			SendMessage(hWndList, WM_HSCROLL, SB_LINERIGHT, 0);
			break;

		case ID_UI_HISTORY_UP:
		{
			/* hmmmmm..... */
			HWND hHistory = GetDlgItem(hMain, IDC_HISTORY);
			SendMessage(hHistory, EM_SCROLL, SB_PAGEUP, 0);
			break;
		}

		case ID_UI_HISTORY_DOWN:
		{
			/* hmmmmm..... */
			HWND hHistory = GetDlgItem(hMain, IDC_HISTORY);
			SendMessage(hHistory, EM_SCROLL, SB_PAGEDOWN, 0);
			break;
		}

		case IDC_SSFRAME:
			TabView_CalculateNextTab(hTabCtrl);
			UpdateScreenShot();
			TabView_UpdateSelection(hTabCtrl);
			break;

		case ID_CONTEXT_SELECT_RANDOM:
			SetRandomPickItem();
			break;

		case ID_CONTEXT_RESET_PLAYCOUNT:
			ResetPlayCount(Picker_GetSelectedItem(hWndList));
			(void)ListView_RedrawItems(hWndList, GetSelectedPick(), GetSelectedPick());
			break;

		case ID_CONTEXT_RESET_PLAYTIME:
			ResetPlayTime(Picker_GetSelectedItem(hWndList));
			(void)ListView_RedrawItems(hWndList, GetSelectedPick(), GetSelectedPick());
			break;

		case ID_CONTEXT_RENAME_CUSTOM:
			(void)TreeView_EditLabel(hTreeView, TreeView_GetSelection(hTreeView));		
			break;

		case ID_HELP_CONTENTS :
			ShellExecuteCommon(hMain, "http://docs.mamedev.org/");
			SetFocus(hWndList);
			return true;

		case ID_HELP_TROUBLE:
			ShellExecuteCommon(hMain, "http://www.1emulation.com/forums/forum/127-arcade/");
			SetFocus(hWndList);
			return true;

		case ID_HELP_WHATS_NEW :
			ShellExecuteCommon(hMain, ".\\docs\\whatsnew.txt");
			SetFocus(hWndList);
			return true;

		default:
			if (id >= ID_SHOW_FOLDER_START1 && id <= ID_SHOW_FOLDER_START28)
			{
				ToggleShowFolder((id - ID_SHOW_FOLDER_START1) + 1);
				break;
			}
			else if (id >= ID_CONTEXT_SHOW_FOLDER_START && id < ID_CONTEXT_SHOW_FOLDER_END)
			{
				ToggleShowFolder(id - ID_CONTEXT_SHOW_FOLDER_START);
				break;
			}
	}

	return false;
}

static void ResetColumnDisplay(void)
{
	int driver_index = GetGameNameIndex(GetDefaultGame());

	Picker_ResetColumnDisplay(hWndList);
	UpdateListView();
	Picker_SetSelectedItem(hWndList, driver_index);
}

static int GamePicker_GetItemImage(HWND hwndPicker, int nItem)
{
	return GetIconForDriver(nItem);
}

static const TCHAR *GamePicker_GetItemString(HWND hwndPicker, int nItem, int nColumn, TCHAR *pszBuffer, UINT nBufferLength)
{
	const TCHAR *s = NULL;
	const char* utf8_s = NULL;
	char playtime_buf[256];
	char playcount_buf[256];

	switch(nColumn)
	{
		case COLUMN_GAMES:
			/* Driver description */
			utf8_s = GetDriverGameTitle(nItem);
			break;

		case COLUMN_ROMNAME:
			/* Driver name (directory) */
			utf8_s = GetDriverGameName(nItem);
			break;

		case COLUMN_MANUFACTURER:
			/* Manufacturer */
			utf8_s = GetDriverGameManufacturer(nItem);
			break;

		case COLUMN_PLAYED:
			/* played count */
			snprintf(playcount_buf, WINUI_ARRAY_LENGTH(playcount_buf), "%d",  GetPlayCount(nItem));
			utf8_s = playcount_buf;
			break;

		case COLUMN_PLAYTIME:
			/* played time */
			GetTextPlayTime(nItem, playtime_buf);
			utf8_s = playtime_buf;
			break;

		case COLUMN_YEAR:
			/* Year */
			utf8_s = GetDriverGameYear(nItem);
			break;

		case COLUMN_SOURCEFILE:
			/* Source drivers */
			utf8_s = GetDriverFileName(nItem);
			break;

		case COLUMN_CLONE:
			utf8_s = GetCloneParentName(nItem);
			break;
	}

	if(utf8_s)
	{
		TCHAR* t_s = win_wstring_from_utf8(utf8_s);

		if(!t_s)
			return s;

		_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_s);
		free(t_s);
		s = pszBuffer;
	}

	return s;
}

static void GamePicker_LeavingItem(HWND hwndPicker, int nItem)
{
	// leaving item...
}

static void GamePicker_EnteringItem(HWND hwndPicker, int nItem)
{
	EnableSelection(nItem);
}

static int GamePicker_FindItemParent(HWND hwndPicker, int nItem)
{
	return GetParentRomSetIndex(&driver_list::driver(nItem));
}

static bool GamePicker_CheckNotWorkingItem(HWND hwndPicker, int nItem)
{
	return DriverIsBroken(nItem);
}

/* Initialize the Picker and List controls */
static void InitListView(void)
{
	static const struct PickerCallbacks s_gameListCallbacks =
	{
		SetSortColumn,					/* pfnSetSortColumn */
		GetSortColumn,					/* pfnGetSortColumn */
		SetSortReverse,					/* pfnSetSortReverse */
		GetSortReverse,					/* pfnGetSortReverse */
		SetViewMode,					/* pfnSetViewMode */
		GetViewMode,					/* pfnGetViewMode */
		SetColumnWidths,				/* pfnSetColumnWidths */
		GetColumnWidths,				/* pfnGetColumnWidths */
		SetColumnOrder,					/* pfnSetColumnOrder */
		GetColumnOrder,					/* pfnGetColumnOrder */
		SetColumnShown,					/* pfnSetColumnShown */
		GetColumnShown,					/* pfnGetColumnShown */
		GamePicker_Compare,				/* pfnCompare */
		MamePlayGame,					/* pfnDoubleClick */
		GamePicker_GetItemString,		/* pfnGetItemString */
		GamePicker_GetItemImage,		/* pfnGetItemImage */
		GamePicker_LeavingItem,			/* pfnLeavingItem */
		GamePicker_EnteringItem,		/* pfnEnteringItem */
		BeginListViewDrag,				/* pfnBeginListViewDrag */
		GamePicker_FindItemParent,		/* pfnFindItemParent */
		GamePicker_CheckNotWorkingItem,	/* pfnCheckNotWorkingItem */
		OnIdle,							/* pfnIdle */
		GamePicker_OnHeaderContextMenu,	/* pfnOnHeaderContextMenu */
		GamePicker_OnBodyContextMenu	/* pfnOnBodyContextMenu */
	};

	struct PickerOptions opts;

	// subclass the list view
	memset(&opts, 0, sizeof(opts));
	opts.pCallbacks = &s_gameListCallbacks;
	opts.nColumnCount = COLUMN_MAX;
	opts.ppszColumnNames = column_names;

	SetupPicker(hWndList, &opts);
	(void)ListView_SetTextBkColor(hWndList, CLR_NONE);
	(void)ListView_SetBkColor(hWndList, CLR_NONE);
	CreateIcons();
	ResetWhichGamesInFolders();
	ResetColumnDisplay();
	// Allow selection to change the default saved game
	bListReady = true;
}

static void AddDriverIcon(int nItem, int default_icon_index)
{
	/* if already set to rom or clone icon, we've been here before */
	if (icon_index[nItem] == 1 || icon_index[nItem] == 3)
		return;

	HICON hIcon = LoadIconFromFile((char *)GetDriverGameName(nItem));

	if (hIcon == NULL)
	{
		int nParentIndex = GetParentIndex(&driver_list::driver(nItem));

		if( nParentIndex >= 0)
		{
			hIcon = LoadIconFromFile((char *)GetDriverGameName(nParentIndex));
			nParentIndex = GetParentIndex(&driver_list::driver(nParentIndex));

			if (hIcon == NULL && nParentIndex >= 0)
				hIcon = LoadIconFromFile((char *)GetDriverGameName(nParentIndex));
		}
	}

	if (hIcon != NULL)
	{
		int nIconPos = ImageList_AddIcon(hSmall, hIcon);
		ImageList_AddIcon(hLarge, hIcon);

		if (nIconPos != -1)
			icon_index[nItem] = nIconPos;

		DestroyIcon(hIcon);
	}

	if (icon_index[nItem] == 0)
		icon_index[nItem] = default_icon_index;
}

static void DestroyIcons(void)
{
	if (hSmall != NULL)
	{
		ImageList_Destroy(hSmall);
		hSmall = NULL;
	}

	if (icon_index != NULL)
	{
		for (int i = 0; i < driver_list::total(); i++)
			icon_index[i] = 0; 	// these are indices into hSmall
	}

	if (hLarge != NULL)
	{
		ImageList_Destroy(hLarge);
		hLarge = NULL;
	}
}

static void ReloadIcons(void)
{
	// clear out all the images
	ImageList_RemoveAll(hSmall);
	ImageList_RemoveAll(hLarge);

	if (icon_index != NULL)
	{
		for (int i = 0; i < driver_list::total(); i++)
			icon_index[i] = 0; 	// these are indices into hSmall
	}

	for (int i = 0; g_iconData[i].icon_name; i++)
	{
		HICON hIcon = LoadIconFromFile((char *) g_iconData[i].icon_name);

		if (hIcon == NULL)
			hIcon = LoadIcon(hInst, MAKEINTRESOURCE(g_iconData[i].resource));

		ImageList_AddIcon(hSmall, hIcon);
		ImageList_AddIcon(hLarge, hIcon);
		DestroyIcon(hIcon);
	}
}

// create iconlist for Listview control
static void CreateIcons(void)
{
	int icon_count = 0;
	int grow = 1000;

	while(g_iconData[icon_count].icon_name)
		icon_count++;

	hSmall = ImageList_Create(16, 16, ILC_COLORDDB | ILC_MASK, icon_count, icon_count + grow);

	if (hSmall == NULL) 
	{
		ErrorMessageBox("Cannot allocate small icon image list!");
		PostQuitMessage(0);
	}

	hLarge = ImageList_Create(32, 32, ILC_COLORDDB | ILC_MASK, icon_count, icon_count + grow);

	if (hLarge == NULL) 
	{
		ErrorMessageBox("Cannot allocate large icon image list!");
		PostQuitMessage(0);
	}

	ReloadIcons();
}


static int GamePicker_Compare(HWND hwndPicker, int index1, int index2, int sort_subitem)
{
	int value = 0;  /* Default to 0, for unknown case */

	switch (sort_subitem)
	{
		case COLUMN_GAMES:
			value = core_stricmp(GetDriverGameTitle(index1), GetDriverGameTitle(index2));
			break;

		case COLUMN_ROMNAME:
			value = core_stricmp(GetDriverGameName(index1), GetDriverGameName(index2));
			break;

		case COLUMN_MANUFACTURER:
			value = core_stricmp(GetDriverGameManufacturer(index1), GetDriverGameManufacturer(index2));
			break;

		case COLUMN_PLAYED:
			value = GetPlayCount(index1) - GetPlayCount(index2);
			break;

		case COLUMN_PLAYTIME:
			value = GetPlayTime(index1) - GetPlayTime(index2);
			break;

		case COLUMN_YEAR:
			value = core_stricmp(GetDriverGameYear(index1), GetDriverGameYear(index2));
			break;

		case COLUMN_SOURCEFILE: // don't try to "improve" this, it will break
			char file1[32];
			char file2[32];
			strcpy(file1, GetDriverFileName(index1));
			strcpy(file2, GetDriverFileName(index2));
			value = core_stricmp(file1, file2);
			break;

		case COLUMN_CLONE:
		{
			const char *name1 = GetCloneParentName(index1);
			const char *name2 = GetCloneParentName(index2);

			if (*name1 == '\0')
				name1 = NULL;

			if (*name2 == '\0')
				name2 = NULL;

			if (NULL == name1 && NULL == name2)
				value = 0;
			else if (name2 == NULL)
				value = -1;
			else if (name1 == NULL)
				value = 1;
			else
				value = core_stricmp(name1, name2);

			break;
		}
	}

	// Handle same comparisons here
	if (value == 0 && COLUMN_GAMES != sort_subitem)
		value = GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);

	return value;
}

int GetSelectedPick()
{
	/* returns index of listview selected item */
	/* This will return -1 if not found */
	return ListView_GetNextItem(hWndList, -1, LVIS_SELECTED | LVIS_FOCUSED);
}

static HICON GetSelectedPickItemIconSmall()
{
	LVITEM lvi;

	lvi.iItem = GetSelectedPick();
	lvi.iSubItem = 0;
	lvi.mask = LVIF_IMAGE;
	(void)ListView_GetItem(hWndList, &lvi);

	return ImageList_GetIcon(hSmall, lvi.iImage, ILD_TRANSPARENT);
}

static void SetRandomPickItem()
{
	int nListCount = ListView_GetItemCount(hWndList);

	if (nListCount > 0)
		Picker_SetSelectedPick(hWndList, rand() % nListCount);
}

static UINT_PTR CALLBACK OFNHookProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
		case WM_INITDIALOG:
			CenterWindow(GetParent(hWnd));
			hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAMEUI_ICON));
			SendMessage(GetParent(hWnd), WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			break;
	}

	return false;
}

bool CommonFileDialog(common_file_dialog_proc cfd, char *filename, int filetype, bool saving)
{
	bool success = false;
	OPENFILENAME of;
	const char *path = NULL;
	TCHAR t_filename_buffer[MAX_PATH];
	TCHAR fCurDir[MAX_PATH];

	// convert the filename to UTF-8 and copy into buffer
	TCHAR *t_filename = win_wstring_from_utf8(filename);

	if (t_filename != NULL)
	{
		_sntprintf(t_filename_buffer, WINUI_ARRAY_LENGTH(t_filename_buffer), TEXT("%s"), t_filename);
		free(t_filename);
	}

	if (GetCurrentDirectory(MAX_PATH, fCurDir) > MAX_PATH)
		fCurDir[0] = 0;

	of.lStructSize = sizeof(OPENFILENAME);
	of.hwndOwner = hMain;
	of.hInstance = NULL;
	of.lpstrCustomFilter = NULL;
	of.nMaxCustFilter = 0;
	of.nFilterIndex = 1;
	of.lpstrFile = t_filename_buffer;
	of.nMaxFile = WINUI_ARRAY_LENGTH(t_filename_buffer);
	of.lpstrFileTitle = NULL;
	of.nMaxFileTitle = 0;
	of.Flags  = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLEHOOK;
	of.nFileOffset = 0;
	of.nFileExtension = 0;
	of.lCustData = 0;
	of.lpfnHook = &OFNHookProc;
	of.lpTemplateName = NULL;

	switch (filetype)
	{
		case FILETYPE_INPUT_FILES :
			path = GetInpDir();
			of.lpstrInitialDir = win_wstring_from_utf8(path);
			of.lpstrFilter = TEXT("inputs (*.inp,*.zip)\0*.inp;*.zip\0");
			of.lpstrDefExt = TEXT("inp");

			if (!saving)
				of.lpstrTitle  = TEXT("Select an INP playback file");
			else
				of.lpstrTitle  = TEXT("Enter a name for the INP playback file");

			break;

		case FILETYPE_SAVESTATE_FILES :
			path = GetStateDir();
			of.lpstrInitialDir = win_wstring_from_utf8(path);
			of.lpstrFilter = TEXT("savestates (*.sta)\0*.sta;\0");
			of.lpstrDefExt = TEXT("sta");
			of.lpstrTitle  = TEXT("Select a STA savestate file");
			break;

		case FILETYPE_WAVE_FILES :
			path = GetAudioDir();
			of.lpstrInitialDir = win_wstring_from_utf8(path);
			of.lpstrFilter = TEXT("sounds (*.wav)\0*.wav;\0");
			of.lpstrDefExt = TEXT("wav");

			if (!saving)
				of.lpstrTitle  = TEXT("Select a WAV audio file");
			else
				of.lpstrTitle  = TEXT("Enter a name for the WAV audio file");

			break;

		case FILETYPE_MNG_FILES :
			path = GetVideoDir();
			of.lpstrInitialDir = win_wstring_from_utf8(path);
			of.lpstrFilter = TEXT("videos (*.mng)\0*.mng;\0");
			of.lpstrDefExt = TEXT("mng");

			if (!saving)
				of.lpstrTitle  = TEXT("Select a MNG image file");
			else
				of.lpstrTitle  = TEXT("Enter a name for the MNG image file");

			break;

		case FILETYPE_AVI_FILES :
			path = GetVideoDir();
			of.lpstrInitialDir = win_wstring_from_utf8(path);
			of.lpstrFilter = TEXT("videos (*.avi)\0*.avi;\0");
			of.lpstrDefExt = TEXT("avi");

			if (!saving)
				of.lpstrTitle  = TEXT("Select an AVI video file");
			else
				of.lpstrTitle  = TEXT("Enter a name for the AVI video file");

			break;

		case FILETYPE_EFFECT_FILES :
			path = GetArtDir();
			of.lpstrInitialDir = win_wstring_from_utf8(path);
			of.lpstrFilter = TEXT("effects (*.png)\0*.png;\0");
			of.lpstrDefExt = TEXT("png");
			of.lpstrTitle  = TEXT("Select an overlay PNG effect file");
			break;

		case FILETYPE_SHADER_FILES :
			path = GetGLSLDir();
			of.lpstrInitialDir = win_wstring_from_utf8(path);
			of.lpstrFilter = TEXT("shaders (*.vsh)\0*.vsh;\0");
			of.lpstrDefExt = TEXT("vsh");
			of.lpstrTitle  = TEXT("Select a GLSL shader file");
			break;

		case FILETYPE_CHEAT_FILES :
			of.lpstrInitialDir = last_directory;
			of.lpstrFilter = TEXT("cheats (*.7z,*.zip)\0*.7z;*.zip;\0");
			of.lpstrDefExt = TEXT("7z");
			of.lpstrTitle  = TEXT("Select a cheats archive file");
			break;

		case FILETYPE_BGFX_FILES :
			char temp[MAX_PATH];
			snprintf(temp, WINUI_ARRAY_LENGTH(temp), "%s\\chains", GetBGFXDir());
			of.lpstrInitialDir = win_wstring_from_utf8(temp);
			of.lpstrFilter = TEXT("chains (*.json)\0*.json;\0");
			of.lpstrDefExt = TEXT("json");
			of.lpstrTitle  = TEXT("Select a BGFX chain file");
			break;

		case FILETYPE_LUASCRIPT_FILES :
			of.lpstrInitialDir = last_directory;
			of.lpstrFilter = TEXT("scripts (*.lua)\0*.lua;\0");
			of.lpstrDefExt = TEXT("lua");
			of.lpstrTitle  = TEXT("Select a LUA script file");
			break;
	}

	success = cfd(&of);

	if (success)
	{
		GetCurrentDirectory(MAX_PATH, last_directory);

		if (fCurDir[0] != 0)
			SetCurrentDirectory(fCurDir);
	}

	char *utf8_filename = win_utf8_from_wstring(t_filename_buffer);

	if (utf8_filename != NULL)
	{
		snprintf(filename, MAX_PATH, "%s", utf8_filename);
		free(utf8_filename);
	}

	return success;
}

void SetStatusBarText(int part_index, const char *message)
{
	TCHAR *t_message = win_wstring_from_utf8(message);

	if(!t_message)
		return;

	SendMessage(hStatusBar, SB_SETTEXT, part_index, (LPARAM)t_message);
	free(t_message);
}

void SetStatusBarTextF(int part_index, const char *fmt, ...)
{
	char buf[256];
	va_list va;

	va_start(va, fmt);
	vsnprintf(buf, WINUI_ARRAY_LENGTH(buf), fmt, va);
	va_end(va);
	SetStatusBarText(part_index, buf);
}

static void MamePlayBackGame(void)
{
	int nGame = Picker_GetSelectedItem(hWndList);
	char filename[MAX_PATH];
	play_options playopts;

	memset(&playopts, 0, sizeof(playopts));
	*filename = 0;

	if (CommonFileDialog(GetOpenFileName, filename, FILETYPE_INPUT_FILES, false))
	{
		osd_file::error filerr;
		TCHAR *t_filename = win_wstring_from_utf8(filename);
		TCHAR *tempname = PathFindFileName(t_filename);
		char *fname = win_utf8_from_wstring(tempname);
		std::string const name = fname;
		free(t_filename);
		free(fname);

		emu_file check(GetInpDir(), OPEN_FLAG_READ);
		filerr = check.open(name);

		if (filerr != osd_file::error::NONE)
		{
			ErrorMessageBox("Could not open '%s' as a valid input file.", name);
			return;
		}

		inp_header header;

		// read the header and verify that it is a modern version; if not, print an error
		if (!header.read(check))
		{
			ErrorMessageBox("Input file is corrupt or invalid (missing header).");
			return;
		}

		// find game and play it
		std::string const sysname = header.get_sysname();

		for (int i = 0; i < driver_list::total(); i++)
		{
			if (sysname == GetDriverGameName(i))
			{
				nGame = i;
				break;
			}
		}

		playopts.playback = name.c_str();
		MamePlayGameWithOptions(nGame, &playopts);
	}
}

static void MameLoadState(void)
{
	int nGame = Picker_GetSelectedItem(hWndList);
	char filename[MAX_PATH];
	play_options playopts;

	memset(&playopts, 0, sizeof(playopts));
	*filename = 0;

	if (CommonFileDialog(GetOpenFileName, filename, FILETYPE_SAVESTATE_FILES, false))
	{
		char name[MAX_PATH];
		TCHAR *t_filename = win_wstring_from_utf8(filename);
		TCHAR *tempname = PathFindFileName(t_filename);
		PathRemoveExtension(tempname);
		char *fname = win_utf8_from_wstring(tempname);
		strcpy(name, fname);
		free(t_filename);
		free(fname);
		playopts.state = name;
		MamePlayGameWithOptions(nGame, &playopts);
	}
}

static void MamePlayRecordGame(void)
{
	int nGame = Picker_GetSelectedItem(hWndList);
	char filename[MAX_PATH];
	play_options playopts;

	memset(&playopts, 0, sizeof(playopts));
	*filename = 0;
	strcpy(filename, GetDriverGameName(nGame));

	if (CommonFileDialog(GetSaveFileName, filename, FILETYPE_INPUT_FILES, true))
	{
		char name[MAX_PATH];
		TCHAR *t_filename = win_wstring_from_utf8(filename);
		TCHAR *tempname = PathFindFileName(t_filename);
		char *fname = win_utf8_from_wstring(tempname);
		strcpy(name, fname);
		free(t_filename);
		free(fname);
		playopts.record = name;
		MamePlayGameWithOptions(nGame, &playopts);
	}
}

void MamePlayGame(void)
{
	int nGame = Picker_GetSelectedItem(hWndList);
	play_options playopts;

	memset(&playopts, 0, sizeof(playopts));
	MamePlayGameWithOptions(nGame, &playopts);
}

static void MamePlayRecordWave(void)
{
	int nGame = Picker_GetSelectedItem(hWndList);
	char filename[MAX_PATH];
	play_options playopts;

	memset(&playopts, 0, sizeof(playopts));
	*filename = 0;
	strcpy(filename, GetDriverGameName(nGame));

	if (CommonFileDialog(GetSaveFileName, filename, FILETYPE_WAVE_FILES, true))
	{
		playopts.wavwrite = filename;
		MamePlayGameWithOptions(nGame, &playopts);
	}
}

static void MamePlayRecordMNG(void)
{
	int nGame = Picker_GetSelectedItem(hWndList);
	char filename[MAX_PATH];
	play_options playopts;

	memset(&playopts, 0, sizeof(playopts));
	*filename = 0;
	strcpy(filename, GetDriverGameName(nGame));

	if (CommonFileDialog(GetSaveFileName, filename, FILETYPE_MNG_FILES, true))
	{
		char name[MAX_PATH];
		TCHAR *t_filename = win_wstring_from_utf8(filename);
		TCHAR *tempname = PathFindFileName(t_filename);
		char *fname = win_utf8_from_wstring(tempname);
		strcpy(name, fname);
		free(t_filename);
		free(fname);
		playopts.mngwrite = name;
		MamePlayGameWithOptions(nGame, &playopts);
	}
}

static void MamePlayRecordAVI(void)
{
	int nGame = Picker_GetSelectedItem(hWndList);
	char filename[MAX_PATH];
	play_options playopts;

	memset(&playopts, 0, sizeof(playopts));
	*filename = 0;
	strcpy(filename, GetDriverGameName(nGame));

	if (CommonFileDialog(GetSaveFileName, filename, FILETYPE_AVI_FILES, true))
	{
		char name[MAX_PATH];
		TCHAR *t_filename = win_wstring_from_utf8(filename);
		TCHAR *tempname = PathFindFileName(t_filename);
		char *fname = win_utf8_from_wstring(tempname);
		strcpy(name, fname);
		free(t_filename);
		free(fname);
		playopts.aviwrite = name;
		MamePlayGameWithOptions(nGame, &playopts);
	}
}

static void MamePlayGameWithOptions(int nGame, const play_options *playopts)
{
	if (g_pJoyGUI != NULL)
		KillTimer(hMain, JOYGUI_TIMER);

	if (GetCycleScreenshot() > 0)
		KillTimer(hMain, SCREENSHOT_TIMER);

	in_emulation = true;
	RunMAME(nGame, playopts);
	IncrementPlayCount(nGame);
	(void)ListView_RedrawItems(hWndList, GetSelectedPick(), GetSelectedPick());
	in_emulation = false;
	game_launched = true;

	// re-sort if sorting on # of times played
	if (GetSortColumn() == COLUMN_PLAYED)
		Picker_Sort(hWndList);

	UpdateStatusBar();
	UpdateWindow(hMain);
	ShowWindow(hMain, SW_SHOW);
	SetActiveWindow(hMain);
	SetForegroundWindow(hMain);
	SetFocus(hWndList);

	if (g_pJoyGUI != NULL)
		SetTimer(hMain, JOYGUI_TIMER, JOYGUI_MS, NULL);

	if (GetCycleScreenshot() > 0)
		SetTimer(hMain, SCREENSHOT_TIMER, GetCycleScreenshot()*1000, NULL); //scale to seconds
}

/* Toggle ScreenShot ON/OFF */
static void ToggleScreenShot(void)
{
	bool showScreenShot = GetShowScreenShot();

	SetShowScreenShot((showScreenShot) ? false : true);
	UpdateScreenShot();

	/* Redraw list view */
	if (showScreenShot)
		InvalidateRect(hWndList, NULL, false);
}

static void AdjustMetrics(void)
{
	TEXTMETRIC tm;
	AREA area;

	/* WM_SETTINGCHANGE also */
	int xtraX = GetSystemMetrics(SM_CXFIXEDFRAME); 	/* Dialog frame width */
	int xtraY = GetSystemMetrics(SM_CYFIXEDFRAME); 	/* Dialog frame height */
	xtraY += GetSystemMetrics(SM_CYMENUSIZE);		/* Menu height */
	xtraY += GetSystemMetrics(SM_CYCAPTION);		/* Caption Height */
	int maxX = GetSystemMetrics(SM_CXSCREEN); 		/* Screen Width */
	int maxY = GetSystemMetrics(SM_CYSCREEN); 		/* Screen Height */
	HDC hDC = GetDC(hMain);
	GetTextMetrics (hDC, &tm);
	/* Convert MIN Width/Height from Dialog Box Units to pixels. */
	MIN_WIDTH  = (int)((tm.tmAveCharWidth / 4.0) * DBU_MIN_WIDTH)  + xtraX;
	MIN_HEIGHT = (int)((tm.tmHeight / 8.0) * DBU_MIN_HEIGHT) + xtraY;
	ReleaseDC(hMain, hDC);

	HWND hWnd = GetWindow(hMain, GW_CHILD);

	while(hWnd)
	{
		TCHAR szClass[128];

		if (GetClassName(hWnd, szClass, WINUI_ARRAY_LENGTH(szClass)))
		{
			if (!_tcscmp(szClass, WC_LISTVIEW))
			{
				(void)ListView_SetBkColor(hWndList, GetListBgColor());
				(void)ListView_SetTextColor(hWndList, GetListFontColor());
			}
			else if (!_tcscmp(szClass, WC_TREEVIEW))
			{
				(void)TreeView_SetBkColor(hTreeView, GetFolderBgColor());
				(void)TreeView_SetTextColor(hTreeView, GetTreeFontColor());
			}
		}

		hWnd = GetWindow(hWnd, GW_HWNDNEXT);
	}

	GetWindowArea(&area);
	int offX = area.x + area.width;
	int offY = area.y + area.height;

	if (offX > maxX)
	{
		offX = maxX;
		area.x = (offX - area.width > 0) ? (offX - area.width) : 0;
	}

	if (offY > maxY)
	{
		offY = maxY;
		area.y = (offY - area.height > 0) ? (offY - area.height) : 0;
	}

	SetWindowArea(&area);
	SetWindowPos(hMain, HWND_TOP, area.x, area.y, area.width, area.height, 0);
}

int FindIconIndex(int nIconResource)
{
	for (int i = 0; g_iconData[i].icon_name; i++)
	{
		if (g_iconData[i].resource == nIconResource)
			return i;
	}

	return -1;
}

int FindIconIndexByName(const char *icon_name)
{
	for (int i = 0; g_iconData[i].icon_name; i++)
	{
		if (!strcmp(g_iconData[i].icon_name, icon_name))
			return i;
	}

	return -1;
}

static bool UseBrokenIcon(int type)
{
	if (type == 4 && !GetUseBrokenIcon())
		return false;

	return true;
}

static int GetIconForDriver(int nItem)
{
	int iconRoms = 0;

	if (DriverUsesRoms(nItem))
	{
		int audit_result = GetRomAuditResults(nItem);

		if (audit_result == -1)
			iconRoms = 2;
		else if (IsAuditResultYes(audit_result))
			iconRoms = 1;
		else
			iconRoms = 0;
	}
	else
		iconRoms = 1;

	// iconRoms is now either 0 (no roms), 1 (roms), or 2 (unknown)

	/* these are indices into icon_names, which maps into our image list
    * also must match IDI_WIN_NOROMS + iconRoms */

	// Show Red-X if the ROMs are present and flagged as NOT WORKING
	if (iconRoms == 1 && DriverIsBroken(nItem))
		iconRoms = FindIconIndex(IDI_WIN_REDX);

	// Show imperfect if the ROMs are present and flagged as imperfect
	if (iconRoms == 1 && DriverIsImperfect(nItem))
		iconRoms = FindIconIndex(IDI_WIN_IMPERFECT);

	// show clone icon if we have roms and game is working
	if (iconRoms == 1 && DriverIsClone(nItem))
		iconRoms = FindIconIndex(IDI_WIN_CLONE);

	// if we have the roms, then look for a custom per-game icon to override
	if (iconRoms == 1 || iconRoms == 3 || iconRoms == 5 || !UseBrokenIcon(iconRoms))
	{
		if (icon_index[nItem] == 0)
			AddDriverIcon(nItem,iconRoms);

		iconRoms = icon_index[nItem];
	}

	return iconRoms;
}

static bool HandleTreeContextMenu(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	TVHITTESTINFO hti;
	POINT pt;

	if ((HWND)wParam != GetDlgItem(hWnd, IDC_TREE))
		return false;

	pt.x = GET_X_LPARAM(lParam);
	pt.y = GET_Y_LPARAM(lParam);
	
	if (pt.x < 0 && pt.y < 0)
		GetCursorPos(&pt);

	/* select the item that was right clicked or shift-F10'ed */
	hti.pt = pt;
	ScreenToClient(hTreeView,&hti.pt);
	(void)TreeView_HitTest(hTreeView,&hti);

	if ((hti.flags & TVHT_ONITEM) != 0)
		(void)TreeView_SelectItem(hTreeView,hti.hItem);

	HMENU hTreeMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_TREE));
	InitTreeContextMenu(hTreeMenu);
	HMENU hMenu = GetSubMenu(hTreeMenu, 0);
	UpdateMenu(hMenu);
	TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
	DestroyMenu(hTreeMenu);
	return true;
}

static void GamePicker_OnBodyContextMenu(POINT pt)
{
	HMENU hMenuLoad = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_MENU));
	HMENU hMenu = GetSubMenu(hMenuLoad, 0);

	InitBodyContextMenu(hMenu);
	UpdateMenu(hMenu);
	TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hMain, NULL);
	DestroyMenu(hMenuLoad);
}

static bool HandleScreenShotContextMenu(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	POINT pt;
	MENUINFO mi;

	if ((HWND)wParam != GetDlgItem(hWnd, IDC_SSPICTURE) && (HWND)wParam != GetDlgItem(hWnd, IDC_SSFRAME))
		return false;

	pt.x = GET_X_LPARAM(lParam);
	pt.y = GET_Y_LPARAM(lParam);

	if (pt.x < 0 && pt.y < 0)
		GetCursorPos(&pt);

	HMENU hMenuLoad = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_SCREENSHOT));
	HMENU hMenu = GetSubMenu(hMenuLoad, 0);
	HMENU hSubMenu = GetSubMenu(hMenu, 2);

	memset(&mi, 0, sizeof(MENUINFO));
	mi.cbSize = sizeof(MENUINFO);
	mi.fMask = MIM_BACKGROUND | MIM_STYLE;
	mi.dwStyle = MNS_CHECKORBMP;
	mi.hbrBack = GetSysColorBrush(COLOR_WINDOW);

	SetMenuInfo(hMenu, &mi);
	SetMenuInfo(hSubMenu, &mi);
	SetMenuItemBitmaps(hMenu, 2, MF_BYPOSITION, hTabs, hTabs);
	UpdateMenu(hMenu);
	TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
	DestroyMenu(hMenuLoad);
	return true;
}

static void UpdateMenu(HMENU hMenu)
{
	MENUITEMINFO mItem;
	LPTREEFOLDER lpFolder = GetCurrentFolder();

	if (have_selection)
	{
		TCHAR buf[200];
		int nGame = Picker_GetSelectedItem(hWndList);
		
		TCHAR *t_description = win_wstring_from_utf8(ConvertAmpersandString(GetDriverGameTitle(nGame)));

		if( !t_description )
			return;

		_sntprintf(buf, WINUI_ARRAY_LENGTH(buf), g_szPlayGameString, t_description);
		memset(&mItem, 0, sizeof(MENUITEMINFO));
		mItem.cbSize = sizeof(MENUITEMINFO);
		mItem.fMask = MIIM_TYPE;
		mItem.fType = MFT_STRING;
		mItem.dwTypeData = buf;
		mItem.cch = _tcslen(mItem.dwTypeData);

		SetMenuItemInfo(hMenu, ID_FILE_PLAY, false, &mItem);
		EnableMenuItem(hMenu, ID_CONTEXT_SELECT_RANDOM, MF_ENABLED);
		free(t_description);
	}
	else
	{
		EnableMenuItem(hMenu, ID_FILE_PLAY, MF_GRAYED);
		EnableMenuItem(hMenu, ID_FILE_PLAY_RECORD, MF_GRAYED);
		EnableMenuItem(hMenu, ID_GAME_PROPERTIES, MF_GRAYED);
		EnableMenuItem(hMenu, ID_CONTEXT_SELECT_RANDOM, MF_GRAYED);
	}

	if (lpFolder->m_dwFlags & F_CUSTOM)
	{
		EnableMenuItem(hMenu, ID_CONTEXT_REMOVE_CUSTOM,	MF_ENABLED);
		EnableMenuItem(hMenu, ID_CONTEXT_RENAME_CUSTOM,	MF_ENABLED);
	}
	else
	{
		EnableMenuItem(hMenu, ID_CONTEXT_REMOVE_CUSTOM,	MF_GRAYED);
		EnableMenuItem(hMenu, ID_CONTEXT_RENAME_CUSTOM,	MF_GRAYED);
	}

	if (lpFolder->m_dwFlags & F_INIEDIT)
		EnableMenuItem(hMenu,ID_FOLDER_PROPERTIES,	MF_ENABLED);
	else
		EnableMenuItem(hMenu,ID_FOLDER_PROPERTIES,	MF_GRAYED);

	CheckMenuRadioItem(hMenu, ID_VIEW_TAB_SCREENSHOT, ID_VIEW_TAB_HISTORY, ID_VIEW_TAB_SCREENSHOT + TabView_GetCurrentTab(hTabCtrl), MF_BYCOMMAND);

	// set whether we're showing the tab control or not
	if (bShowTabCtrl)
		CheckMenuItem(hMenu, ID_VIEW_PAGETAB, MF_BYCOMMAND | MF_CHECKED);
	else
		CheckMenuItem(hMenu, ID_VIEW_PAGETAB, MF_BYCOMMAND | MF_UNCHECKED);


	for (int i = 0; i < MAX_TAB_TYPES; i++)
	{
		// disable menu items for tabs we're not currently showing
		if (GetShowTab(i))
			EnableMenuItem(hMenu, ID_VIEW_TAB_SCREENSHOT + i, MF_BYCOMMAND | MF_ENABLED);
		else
			EnableMenuItem(hMenu, ID_VIEW_TAB_SCREENSHOT + i, MF_BYCOMMAND | MF_GRAYED);

		// check toggle menu items
		if (GetShowTab(i))
			CheckMenuItem(hMenu, ID_TOGGLE_TAB_SCREENSHOT + i, MF_BYCOMMAND | MF_CHECKED);
		else
			CheckMenuItem(hMenu, ID_TOGGLE_TAB_SCREENSHOT + i, MF_BYCOMMAND | MF_UNCHECKED);
	}

	for (int i = 0; i < MAX_FOLDERS; i++)
	{
		if (GetShowFolder(i))
		{
			CheckMenuItem(hMenu, ID_CONTEXT_SHOW_FOLDER_START + i, MF_BYCOMMAND | MF_CHECKED);
			CheckMenuItem(hMenu, (ID_SHOW_FOLDER_START1 + i) - 1, MF_BYCOMMAND | MF_CHECKED);
		}
		else
		{
			CheckMenuItem(hMenu, ID_CONTEXT_SHOW_FOLDER_START + i, MF_BYCOMMAND | MF_UNCHECKED);
			CheckMenuItem(hMenu, (ID_SHOW_FOLDER_START1 + i) - 1, MF_BYCOMMAND | MF_UNCHECKED);
		}
	}
}

void InitMainMenu(HMENU hMainMenu)
{
	MENUINFO mi;
	MENUITEMINFO mif;
	extern const FOLDERDATA g_folderData[];
	HMENU hFile = GetSubMenu(hMainMenu, 0);
	HMENU hView = GetSubMenu(hMainMenu, 1);
	HMENU hOption = GetSubMenu(hMainMenu, 2);
	HMENU hTools = GetSubMenu(hMainMenu, 3);
	HMENU hAbout = GetSubMenu(hMainMenu, 4);
	HMENU hSubSort = GetSubMenu(hView, 11);
	HMENU hSubFold = GetSubMenu(hView, 13);
	HMENU hSubView = GetSubMenu(hView, 15);
	HMENU hSubFonts = GetSubMenu(hOption, 4);

	memset(&mi, 0, sizeof(MENUINFO));
	mi.cbSize = sizeof(MENUINFO);
	mi.fMask = MIM_BACKGROUND | MIM_STYLE;
	mi.dwStyle = MNS_CHECKORBMP;
	mi.hbrBack = GetSysColorBrush(COLOR_WINDOW);

	SetMenuInfo(hFile, &mi);
	SetMenuInfo(hView, &mi);
	SetMenuInfo(hOption, &mi);
	SetMenuInfo(hTools, &mi);
	SetMenuInfo(hAbout, &mi);
	SetMenuInfo(hSubSort, &mi);
	SetMenuInfo(hSubFold, &mi);
	SetMenuInfo(hSubView, &mi);
	SetMenuInfo(hSubFonts, &mi);

	memset(&mif, 0, sizeof(MENUITEMINFO));
	mif.cbSize = sizeof(MENUITEMINFO);

	for (int i = 0; g_folderData[i].m_lpTitle != NULL; i++)
	{
		TCHAR* t_title = win_wstring_from_utf8(g_folderData[i].m_lpTitle);
		
		if(!t_title)
			return;

		mif.fMask = MIIM_TYPE | MIIM_ID;
		mif.fType = MFT_STRING;
		mif.dwTypeData = t_title;
		mif.cch = _tcslen(mif.dwTypeData);
		mif.wID = ID_SHOW_FOLDER_START1 + i;

		SetMenuItemInfo(hMainMenu, ID_SHOW_FOLDER_START1 + i, false, &mif);
		free(t_title);
	}

	SetMenuItemBitmaps(hMainMenu, ID_HELP_ABOUT, MF_BYCOMMAND, hAboutMenu, hAboutMenu);
	SetMenuItemBitmaps(hMainMenu, ID_OPTIONS_DIR, MF_BYCOMMAND, hDirectories, hDirectories);
	SetMenuItemBitmaps(hMainMenu, ID_FILE_EXIT, MF_BYCOMMAND, hExit, hExit);
	SetMenuItemBitmaps(hMainMenu, ID_VIEW_FULLSCREEN, MF_BYCOMMAND, hFullscreen, hFullscreen);
	SetMenuItemBitmaps(hMainMenu, ID_OPTIONS_INTERFACE, MF_BYCOMMAND, hInterface, hInterface);
	SetMenuItemBitmaps(hMainMenu, ID_HELP_CONTENTS, MF_BYCOMMAND, hHelp, hHelp);
	SetMenuItemBitmaps(hMainMenu, ID_MAME_HOMEPAGE, MF_BYCOMMAND, hMameHome, hMameHome);
	SetMenuItemBitmaps(hMainMenu, ID_FILE_PLAY, MF_BYCOMMAND, hPlay, hPlay);
	SetMenuItemBitmaps(hMainMenu, ID_VIDEO_SNAP, MF_BYCOMMAND, hVideo, hVideo);
	SetMenuItemBitmaps(hMainMenu, ID_PLAY_M1, MF_BYCOMMAND, hPlayM1, hPlayM1);
	SetMenuItemBitmaps(hMainMenu, ID_OPTIONS_DEFAULTS, MF_BYCOMMAND, hOptions, hOptions);
	SetMenuItemBitmaps(hMainMenu, ID_UPDATE_GAMELIST, MF_BYCOMMAND, hRefresh, hRefresh);
	SetMenuItemBitmaps(hMainMenu, ID_FILE_GAMELIST, MF_BYCOMMAND, hSaveList, hSaveList);
	SetMenuItemBitmaps(hMainMenu, ID_FILE_ROMSLIST, MF_BYCOMMAND, hSaveRoms, hSaveRoms);
	SetMenuItemBitmaps(hMainMenu, ID_FILE_PLAY_BACK, MF_BYCOMMAND, hPlayback, hPlayback);
	SetMenuItemBitmaps(hMainMenu, ID_FILE_AUDIT, MF_BYCOMMAND, hAuditMenu, hAuditMenu);
	SetMenuItemBitmaps(hMainMenu, ID_PLAY_VIDEO, MF_BYCOMMAND, hVideo, hVideo);
	SetMenuItemBitmaps(hOption, 4, MF_BYPOSITION, hFonts, hFonts);
	SetMenuItemBitmaps(hView, 13, MF_BYPOSITION, hFolders, hFolders);
	SetMenuItemBitmaps(hView, 11, MF_BYPOSITION, hSort, hSort);
	SetMenuItemBitmaps(hMainMenu, ID_MAME_FAQ, MF_BYCOMMAND, hFaq, hFaq);
	SetMenuItemBitmaps(hView, 15, MF_BYPOSITION, hTabs, hTabs);
	SetMenuItemBitmaps(hMainMenu, ID_HELP_TROUBLE, MF_BYCOMMAND, hTrouble, hTrouble);
	SetMenuItemBitmaps(hMainMenu, ID_HELP_WHATS_NEW, MF_BYCOMMAND, hRelease, hRelease);
	SetMenuItemBitmaps(hSubSort, ID_VIEW_BYGAME, MF_BYCOMMAND, hDescription, hDescription);
	SetMenuItemBitmaps(hSubSort, ID_VIEW_BYDIRECTORY, MF_BYCOMMAND, hRom, hRom);
	SetMenuItemBitmaps(hSubSort, ID_VIEW_BYSOURCE, MF_BYCOMMAND, hSource, hSource);
	SetMenuItemBitmaps(hSubSort, ID_VIEW_BYMANUFACTURER, MF_BYCOMMAND, hManufacturer, hManufacturer);
	SetMenuItemBitmaps(hSubSort, ID_VIEW_BYYEAR, MF_BYCOMMAND, hYear, hYear);
	SetMenuItemBitmaps(hSubSort, ID_VIEW_BYTIMESPLAYED, MF_BYCOMMAND, hCount, hCount);
	SetMenuItemBitmaps(hMainMenu, ID_PLAY_AUDIO, MF_BYCOMMAND, hPlaywav, hPlaywav);
	SetMenuItemBitmaps(hSubFonts, ID_OPTIONS_TREE_FONT, MF_BYCOMMAND, hFont1, hFont1);
	SetMenuItemBitmaps(hSubFonts, ID_OPTIONS_HISTORY_FONT, MF_BYCOMMAND, hFont2, hFont2);
	SetMenuItemBitmaps(hSubFonts, ID_OPTIONS_FONT, MF_BYCOMMAND, hFont1, hFont1);
	SetMenuItemBitmaps(hSubFonts, ID_OPTIONS_HISTORY_COLOR, MF_BYCOMMAND, hInfoback, hInfoback);
	SetMenuItemBitmaps(hSubFonts, ID_OPTIONS_LIST_COLOR, MF_BYCOMMAND, hListback, hListback);
	SetMenuItemBitmaps(hSubFonts, ID_OPTIONS_FOLDERS_COLOR, MF_BYCOMMAND, hTreeback, hTreeback);
	SetMenuItemBitmaps(hMainMenu, ID_CUSTOMIZE_FIELDS, MF_BYCOMMAND, hFields, hFields);
	SetMenuItemBitmaps(hMainMenu, ID_FILE_PLAY_RECORD_AVI, MF_BYCOMMAND, hRecavi, hRecavi);
	SetMenuItemBitmaps(hMainMenu, ID_FILE_PLAY_RECORD, MF_BYCOMMAND, hRecinput, hRecinput);
	SetMenuItemBitmaps(hMainMenu, ID_FILE_PLAY_RECORD_WAVE, MF_BYCOMMAND, hRecwav, hRecwav);
	SetMenuItemBitmaps(hMainMenu, ID_PLAY_MNG, MF_BYCOMMAND, hPlaymng, hPlaymng);
	SetMenuItemBitmaps(hMainMenu, ID_CONTEXT_SELECT_RANDOM, MF_BYCOMMAND, hRandom, hRandom);
	SetMenuItemBitmaps(hMainMenu, ID_FILE_PLAY_RECORD_MNG, MF_BYCOMMAND, hRecmng, hRecmng);
	SetMenuItemBitmaps(hMainMenu, ID_FILE_LOADSTATE, MF_BYCOMMAND, hSavestate, hSavestate);
	SetMenuItemBitmaps(hMainMenu, ID_CONTEXT_FILTERS, MF_BYCOMMAND, hFilters, hFilters);
	SetMenuItemBitmaps(hMainMenu, ID_OPTIONS_RESET_DEFAULTS, MF_BYCOMMAND, hReset, hReset);
}

void InitTreeContextMenu(HMENU hTreeMenu)
{
	MENUINFO mi;
	MENUITEMINFO mii;
	extern const FOLDERDATA g_folderData[];
	HMENU hMenuTree = GetSubMenu(hTreeMenu, 0);

	memset(&mi, 0, sizeof(MENUINFO));
	mi.cbSize = sizeof(MENUINFO);
	mi.fMask = MIM_BACKGROUND | MIM_STYLE;
	mi.dwStyle = MNS_CHECKORBMP;
	mi.hbrBack = GetSysColorBrush(COLOR_WINDOW);

	SetMenuInfo(hMenuTree, &mi);

	memset(&mii, 0, sizeof(MENUITEMINFO));
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.wID = -1;
	mii.fMask = MIIM_SUBMENU | MIIM_ID;

	if (GetMenuItemInfo(hMenuTree, 3, true, &mii) == false)
		return;

	if (mii.hSubMenu == NULL)
		return;

	SetMenuItemBitmaps(hMenuTree, ID_FOLDER_PROPERTIES, MF_BYCOMMAND, hProperties, hProperties);
	SetMenuItemBitmaps(hMenuTree, ID_FOLDER_AUDIT, MF_BYCOMMAND, hAuditMenu, hAuditMenu);
	SetMenuItemBitmaps(hMenuTree, 3, MF_BYPOSITION, hFolders, hFolders);
	SetMenuItemBitmaps(hMenuTree, ID_CONTEXT_FILTERS, MF_BYCOMMAND, hFilters, hFilters);
	SetMenuItemBitmaps(hMenuTree, ID_CONTEXT_RENAME_CUSTOM, MF_BYCOMMAND, hRename, hRename);
	hMenuTree = mii.hSubMenu;
	SetMenuInfo(hMenuTree, &mi);

	for (int i = 0; g_folderData[i].m_lpTitle != NULL; i++)
	{
		TCHAR* t_title = win_wstring_from_utf8(g_folderData[i].m_lpTitle);

		if(!t_title)
			return;

		mii.fMask = MIIM_TYPE | MIIM_ID;
		mii.fType = MFT_STRING;
		mii.dwTypeData = t_title;
		mii.cch = _tcslen(mii.dwTypeData);
		mii.wID = ID_CONTEXT_SHOW_FOLDER_START + g_folderData[i].m_nFolderId;

		// menu in resources has one empty item (needed for the submenu to setup properly)
		// so overwrite this one, append after
		if (i == 0)
			SetMenuItemInfo(hMenuTree, ID_CONTEXT_SHOW_FOLDER_START, false, &mii);
		else
			InsertMenuItem(hMenuTree, i, false, &mii);

		free(t_title);
	}
}

void InitBodyContextMenu(HMENU hBodyContextMenu)
{
	TCHAR tmp[64];
	MENUINFO mi;
	MENUITEMINFO mii;

	memset(&mi, 0, sizeof(MENUINFO));
	mi.cbSize = sizeof(MENUINFO);
	mi.fMask = MIM_BACKGROUND | MIM_STYLE;
	mi.dwStyle = MNS_CHECKORBMP;
	mi.hbrBack = GetSysColorBrush(COLOR_WINDOW);

	SetMenuInfo(hBodyContextMenu, &mi);

	memset(&mii, 0, sizeof(MENUITEMINFO));
	mii.cbSize = sizeof(MENUITEMINFO);

	if (GetMenuItemInfo(hBodyContextMenu, ID_FOLDER_SOURCEPROPERTIES, false, &mii) == false)
		return;

	LPTREEFOLDER lpFolder = GetFolderByName(FOLDER_SOURCE, GetDriverFileName(Picker_GetSelectedItem(hWndList)));
	_sntprintf(tmp, WINUI_ARRAY_LENGTH(tmp), TEXT("Properties for %s\tAlt+D"), lpFolder->m_lptTitle);

	mii.fMask = MIIM_TYPE | MIIM_ID;
	mii.fType = MFT_STRING;
	mii.dwTypeData = tmp;
	mii.cch = _tcslen(mii.dwTypeData);
	mii.wID = ID_FOLDER_SOURCEPROPERTIES;

	// menu in resources has one default item
	// so overwrite this one
	SetMenuItemInfo(hBodyContextMenu, ID_FOLDER_SOURCEPROPERTIES, false, &mii);
	SetMenuItemBitmaps(hBodyContextMenu, ID_CONTEXT_ADD_CUSTOM, MF_BYCOMMAND, hCustom, hCustom);
	SetMenuItemBitmaps(hBodyContextMenu, ID_FILE_PLAY, MF_BYCOMMAND, hPlay, hPlay);
	SetMenuItemBitmaps(hBodyContextMenu, ID_VIDEO_SNAP, MF_BYCOMMAND, hVideo, hVideo);
	SetMenuItemBitmaps(hBodyContextMenu, ID_PLAY_M1, MF_BYCOMMAND, hPlayM1, hPlayM1);
	SetMenuItemBitmaps(hBodyContextMenu, ID_VIEW_ZIP, MF_BYCOMMAND, hZip, hZip);
	SetMenuItemBitmaps(hBodyContextMenu, ID_GAME_PROPERTIES, MF_BYCOMMAND, hProperties, hProperties);
	SetMenuItemBitmaps(hBodyContextMenu, ID_GAME_INFO, MF_BYCOMMAND, hRelease, hRelease);
	SetMenuItemBitmaps(hBodyContextMenu, ID_GAME_AUDIT, MF_BYCOMMAND, hAuditMenu, hAuditMenu);
	SetMenuItemBitmaps(hBodyContextMenu, ID_FOLDER_SOURCEPROPERTIES, MF_BYCOMMAND, hDriver, hDriver);
	SetMenuItemBitmaps(hBodyContextMenu, ID_CONTEXT_RESET_PLAYCOUNT, MF_BYCOMMAND, hCount, hCount);
	SetMenuItemBitmaps(hBodyContextMenu, ID_CONTEXT_RESET_PLAYTIME, MF_BYCOMMAND, hTime, hTime);
	SetMenuItemBitmaps(hBodyContextMenu, ID_FILE_PLAY_RECORD, MF_BYCOMMAND, hRecinput, hRecinput);
	SetMenuItemBitmaps(hBodyContextMenu, ID_CONTEXT_REMOVE_CUSTOM, MF_BYCOMMAND, hRemove, hRemove);
	SetMenuItemBitmaps(hBodyContextMenu, ID_FILE_LOADSTATE, MF_BYCOMMAND, hSavestate, hSavestate);
}

void ToggleShowFolder(int folder)
{
	int current_id = GetCurrentFolderID();

	SetWindowRedraw(hWndList, false);
	SetShowFolder(folder, !GetShowFolder(folder));
	ResetTreeViewFolders();
	SelectTreeViewFolder(current_id);
	SetWindowRedraw(hWndList, true);
}

static LRESULT CALLBACK PictureFrameWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_NCHITTEST :
		{
			POINT pt;
			RECT  rect;
			HWND hHistory = GetDlgItem(hMain, IDC_HISTORY);

			pt.x = LOWORD(lParam);
			pt.y = HIWORD(lParam);
			GetWindowRect(hHistory, &rect);
			// check if they clicked on the picture area (leave 6 pixel no man's land
			// by the history window to reduce mistaken clicks)
			// no more no man's land, the Cursor changes when Edit control is left, should be enough feedback
			if (have_history && ( ( (TabView_GetCurrentTab(hTabCtrl) == TAB_HISTORY) ||
				(TabView_GetCurrentTab(hTabCtrl) == GetHistoryTab() && GetShowTab(TAB_HISTORY) == false) ||
				(TAB_ALL == GetHistoryTab() && GetShowTab(TAB_HISTORY) == false) ) && PtInRect( &rect, pt ) ) )
				return HTTRANSPARENT;
			else
				return HTCLIENT;

			break;
		}

		case WM_CONTEXTMENU:
			if ( HandleScreenShotContextMenu(hWnd, wParam, lParam))
				return false;

			break;
	}

	return CallWindowProc(g_lpPictureFrameWndProc, hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK PictureWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC	hdc, hdc_temp;
			RECT rect;
			HBITMAP old_bitmap;
			int width = 0;
			int height = 0;
			RECT rect2;
			int nBordersize = GetScreenshotBorderSize();
			HBRUSH hBrush = CreateSolidBrush(GetScreenshotBorderColor());

			hdc = BeginPaint(hWnd, &ps);
			hdc_temp = CreateCompatibleDC(hdc);

			if (ScreenShotLoaded())
			{
				width = GetScreenShotWidth();
				height = GetScreenShotHeight();
				old_bitmap = (HBITMAP)SelectObject(hdc_temp,GetScreenShotHandle());
			}
			else
			{
				BITMAP bmp;

				GetObject(hMissing_bitmap, sizeof(BITMAP), &bmp);
				width = bmp.bmWidth;
				height = bmp.bmHeight;
				old_bitmap = (HBITMAP)SelectObject(hdc_temp,hMissing_bitmap);
			}

			GetClientRect(hWnd,&rect);
			rect2 = rect;
			//Configurable Borders around images
			rect.bottom -= nBordersize;
			
			if (rect.bottom < 0)
				rect.bottom = rect2.bottom;

			rect.right -= nBordersize;

			if (rect.right < 0)
				rect.right = rect2.right;

			rect.top += nBordersize;

			if (rect.top > rect.bottom)
				rect.top = rect2.top;

			rect.left += nBordersize;

			if (rect.left > rect.right)
				rect.left = rect2.left;

			HRGN region1 = CreateRectRgnIndirect(&rect);
			HRGN region2 = CreateRectRgnIndirect(&rect2);
			CombineRgn(region2, region2, region1, RGN_DIFF);
			HBRUSH holdBrush = (HBRUSH)SelectObject(hdc, hBrush);
			FillRgn(hdc,region2, hBrush);
			SelectObject(hdc, holdBrush);
			DeleteBrush(hBrush);
			SetStretchBltMode(hdc, STRETCH_HALFTONE);
			StretchBlt(hdc,nBordersize,nBordersize,rect.right-rect.left,rect.bottom-rect.top, hdc_temp, 0, 0, width, height, SRCCOPY);
			SelectObject(hdc_temp,old_bitmap);
			DeleteDC(hdc_temp);
			DeleteObject(region1);
			DeleteObject(region2);
			EndPaint(hWnd,&ps);
			return true;
		}
	}

	return CallWindowProc(g_lpPictureWndProc, hWnd, uMsg, wParam, lParam);
}

static void RemoveCurrentGameCustomFolder(void)
{
	RemoveGameCustomFolder(Picker_GetSelectedItem(hWndList));
}

static void RemoveGameCustomFolder(int driver_index)
{
	TREEFOLDER **folders;
	int num_folders = 0;

	GetFolders(&folders, &num_folders);

	for (int i = 0; i < num_folders; i++)
	{
		if (folders[i]->m_dwFlags & F_CUSTOM && folders[i]->m_nFolderId == GetCurrentFolderID())
		{
			RemoveFromCustomFolder(folders[i], driver_index);

			if (driver_index == Picker_GetSelectedItem(hWndList))
			{
				/* if we just removed the current game,
				move the current selection so that when we rebuild the listview it leaves the cursor on next or previous one */
				int current_pick_index = GetSelectedPick();
				Picker_SetSelectedPick(hWndList, GetSelectedPick() + 1);

				if (current_pick_index == GetSelectedPick()) /* we must have deleted the last item */
					Picker_SetSelectedPick(hWndList, GetSelectedPick() - 1);
			}

			UpdateListView();
			return;
		}
	}

	ErrorMessageBox("Error searching for custom folder");

}

static void BeginListViewDrag(NM_LISTVIEW *pnmv)
{
	LVITEM lvi;
	POINT pt;

	lvi.iItem = pnmv->iItem;
	lvi.mask = LVIF_PARAM;

	(void)ListView_GetItem(hWndList, &lvi);
	game_dragged = lvi.lParam;
	pt.x = 0;
	pt.y = 0;
	/* Tell the list view control to create an image to use for dragging. */
	himl_drag = ListView_CreateDragImage(hWndList, pnmv->iItem, &pt);
	/* Start the drag operation. */
	ImageList_BeginDrag(himl_drag, 0, 0, 0);
	pt = pnmv->ptAction;
	ClientToScreen(hWndList,&pt);
	ImageList_DragEnter(GetDesktopWindow(), pt.x, pt.y);
	/* Hide the mouse cursor, and direct mouse input to the parent window. */
	SetCapture(hMain);
	prev_drag_drop_target = NULL;
	g_listview_dragging = true;
}

static void MouseMoveListViewDrag(POINTS p)
{
	TV_HITTESTINFO tvht;
	POINT pt;
	
	pt.x = p.x;
	pt.y = p.y;
	ClientToScreen(hMain,&pt);
	ImageList_DragMove(pt.x,pt.y);
	MapWindowPoints(GetDesktopWindow(), hTreeView, &pt, 1);
	tvht.pt = pt;
	HTREEITEM htiTarget = TreeView_HitTest(hTreeView,&tvht);

	if (htiTarget != prev_drag_drop_target)
	{
		ImageList_DragShowNolock(false);

		if (htiTarget != NULL)
			(void)TreeView_SelectDropTarget(hTreeView,htiTarget);
		else
			(void)TreeView_SelectDropTarget(hTreeView,NULL);

		ImageList_DragShowNolock(true);
		prev_drag_drop_target = htiTarget;
	}
}

static void ButtonUpListViewDrag(POINTS p)
{
	POINT pt;
	TV_HITTESTINFO tvht;
	TVITEM tvi;

	ReleaseCapture();
	ImageList_DragLeave(hWndList);
	ImageList_EndDrag();
	ImageList_Destroy(himl_drag);
	(void)TreeView_SelectDropTarget(hTreeView,NULL);
	g_listview_dragging = false;
	/* see where the game was dragged */
	pt.x = p.x;
	pt.y = p.y;
	MapWindowPoints(hMain, hTreeView, &pt, 1);
	tvht.pt = pt;
	HTREEITEM htiTarget = TreeView_HitTest(hTreeView,&tvht);

	if (htiTarget == NULL)
	{
		LVHITTESTINFO lvhtti;
		RECT rcList;

		/* the user dragged a game onto something other than the treeview */
		/* try to remove if we're in a custom folder */
		/* see if it was dragged within the list view; if so, ignore */
		MapWindowPoints(hTreeView, hWndList, &pt, 1);
		lvhtti.pt = pt;
		GetWindowRect(hWndList, &rcList);
		ClientToScreen(hWndList, &pt);

		if( PtInRect(&rcList, pt) != 0 )
			return;

		LPTREEFOLDER folder = GetCurrentFolder();

		if (folder->m_dwFlags & F_CUSTOM)
		{
			/* dragged out of a custom folder, so let's remove it */
			RemoveCurrentGameCustomFolder();
		}

		return;
	}

	tvi.lParam = 0;
	tvi.mask = TVIF_PARAM | TVIF_HANDLE;
	tvi.hItem = htiTarget;

	if (TreeView_GetItem(hTreeView, &tvi))
	{
		LPTREEFOLDER folder = (LPTREEFOLDER)tvi.lParam;
		AddToCustomFolder(folder, game_dragged);
	}

}

static LPTREEFOLDER GetSelectedFolder(void)
{
	HTREEITEM htree = TreeView_GetSelection(hTreeView);
	TVITEM tvi;

	if(htree != NULL)
	{
		tvi.hItem = htree;
		tvi.mask = TVIF_PARAM;
		(void)TreeView_GetItem(hTreeView,&tvi);
		return (LPTREEFOLDER)tvi.lParam;
	}

	return NULL;
}

/* Updates all currently displayed Items in the List with the latest Data*/
void UpdateListView(void)
{
	ResetWhichGamesInFolders();
	ResetListView();
	(void)ListView_RedrawItems(hWndList, ListView_GetTopIndex(hWndList), ListView_GetTopIndex(hWndList) + ListView_GetCountPerPage(hWndList));
	SetFocus(hWndList);
}

static void CalculateBestScreenShotRect(HWND hWnd, RECT *pRect, bool restrict_height)
{
	RECT rect;
	/* for scaling */
	int destW = 0; 
	int destH = 0;
	int x = 0;
	int y = 0;
	double scale = 0;
	bool bReduce = false;

	GetClientRect(hWnd, &rect);

	// Scale the bitmap to the frame specified by the passed in hwnd
	if (ScreenShotLoaded())
	{
		x = GetScreenShotWidth();
		y = GetScreenShotHeight();
	}
	else
	{
		BITMAP bmp;
		
		GetObject(hMissing_bitmap,sizeof(BITMAP),&bmp);
		x = bmp.bmWidth;
		y = bmp.bmHeight;
	}
	
	int rWidth  = (rect.right  - rect.left);
	int rHeight = (rect.bottom - rect.top);

	/* Limit the screen shot to max height of 264 */
	if (restrict_height == true && rHeight > 264)
	{
		rect.bottom = rect.top + 264;
		rHeight = 264;
	}

	/* If the bitmap does NOT fit in the screenshot area */
	if (x > rWidth - 10 || y > rHeight - 10)
	{
		rect.right -= 10;
		rect.bottom -= 10;
		rWidth -= 10;
		rHeight -= 10;
		bReduce = true;
		
		/* Try to scale it properly */
		/*  assumes square pixels, doesn't consider aspect ratio */
		if (x > y)
			scale = (double)rWidth / x;
		else
			scale = (double)rHeight / y;

		destW = (int)(x * scale);
		destH = (int)(y * scale);

		/* If it's still too big, scale again */
		if (destW > rWidth || destH > rHeight)
		{
			if (destW > rWidth)
				scale = (double)rWidth / destW;
			else
				scale = (double)rHeight / destH;

			destW = (int)(destW * scale);
			destH = (int)(destH * scale);
		}
	}
	else
	{
		if (GetStretchScreenShotLarger())
		{
			rect.right -= 10;
			rect.bottom -= 10;
			rWidth -= 10;
			rHeight -= 10;
			bReduce = true;

			// Try to scale it properly
			// assumes square pixels, doesn't consider aspect ratio
			if (x < y)
				scale = (double)rWidth / x;
			else
				scale = (double)rHeight / y;

			destW = (int)(x * scale);
			destH = (int)(y * scale);

			// If it's too big, scale again
			if (destW > rWidth || destH > rHeight)
			{
				if (destW > rWidth)
					scale = (double)rWidth / destW;
				else
					scale = (double)rHeight / destH;

				destW = (int)(destW * scale);
				destH = (int)(destH * scale);
			}
		}
		else
		{
			// Use the bitmaps size if it fits
			destW = x;
			destH = y;
		}

	}

	int destX = ((rWidth  - destW) / 2);
	int destY = ((rHeight - destH) / 2);

	if (bReduce)
	{
		destX += 5;
		destY += 5;
	}

	int nBorder = GetScreenshotBorderSize();

	if( destX > nBorder+1)
		pRect->left = destX - nBorder;
	else
		pRect->left = 2;

	if( destY > nBorder+1)
		pRect->top = destY - nBorder;
	else
		pRect->top = 2;

	if( rWidth >= destX + destW + nBorder)
		pRect->right = destX + destW + nBorder;
	else
		pRect->right = rWidth - pRect->left;

	if( rHeight >= destY + destH + nBorder)
		pRect->bottom = destY + destH + nBorder;
	else
		pRect->bottom = rHeight - pRect->top;
}

/*
  Switches to either fullscreen or normal mode, based on the
  current mode.

  POSSIBLE BUGS:
  Removing the menu might cause problems later if some
  function tries to poll info stored in the menu. Don't
  know if you've done that, but this was the only way I
  knew to remove the menu dynamically.
*/

static void SwitchFullScreenMode(void)
{
	if (bFullScreen)
	{
		// Hide the window
		ShowWindow(hMain, SW_HIDE);
		// Restore the menu
		SetMenu(hMain, LoadMenu(hInst, MAKEINTRESOURCE(IDR_UI_MENU)));
		InitMainMenu(GetMenu(hMain));
		// Refresh the checkmarks
		CheckMenuItem(GetMenu(hMain), ID_VIEW_FOLDERS, GetShowFolderList() ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(GetMenu(hMain), ID_VIEW_TOOLBARS, GetShowToolBar() ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(GetMenu(hMain), ID_VIEW_STATUS, GetShowStatusBar() ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(GetMenu(hMain), ID_VIEW_PAGETAB, GetShowTabCtrl() ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(GetMenu(hMain), ID_ENABLE_INDENT, GetEnableIndent() ? MF_CHECKED : MF_UNCHECKED);
		// Add frame to dialog again
		SetWindowLong(hMain, GWL_STYLE, GetWindowLong(hMain, GWL_STYLE) | WS_OVERLAPPEDWINDOW);

		// Restore the window
		if (GetWindowState() == SW_MAXIMIZE)
			ShowWindow(hMain, SW_MAXIMIZE);
		else
			ShowWindow(hMain, SW_SHOWNORMAL);

		bFullScreen = !bFullScreen;
	}
	else
	{
		// Hide the window
		ShowWindow(hMain, SW_HIDE);
		// Remove menu
		SetMenu(hMain, NULL);
		// Frameless dialog
		SetWindowLong(hMain, GWL_STYLE, GetWindowLong(hMain, GWL_STYLE) & ~WS_OVERLAPPEDWINDOW);

		// Keep track if we're already maximized before fullscreen
		if (IsMaximized(hMain))
			SetWindowState(SW_MAXIMIZE);
		else
			SetWindowState(SW_SHOWNORMAL);

		// Maximize the window
		ShowWindow(hMain, SW_MAXIMIZE);
		bFullScreen = !bFullScreen;
	}
}

static INT_PTR CALLBACK StartupProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			HBITMAP hBmp = (HBITMAP)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_SPLASH), IMAGE_BITMAP, 0, 0, LR_SHARED);
			SendMessage(GetDlgItem(hDlg, IDC_SPLASH), STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
			hBrush = GetSysColorBrush(COLOR_3DFACE);
			hProgress = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE, 0, 136, 526, 18, hDlg, NULL, hInst, NULL);
			SetWindowTheme(hProgress, L" ", L" ");
			SendMessage(hProgress, PBM_SETBKCOLOR, 0, GetSysColor(COLOR_3DFACE));
			SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 120));
			SendMessage(hProgress, PBM_SETPOS, 0, 0);
			return true;
		}

		case WM_CTLCOLORDLG:
			return (LRESULT) hBrush;

		case WM_CTLCOLORSTATIC:
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, GetSysColor(COLOR_HIGHLIGHT));
			return (LRESULT) hBrush;
	}

	return false;
}

static bool CommonListDialog(common_file_dialog_proc cfd, int filetype)
{
	bool success = false;
	OPENFILENAME of;
	TCHAR szFile[MAX_PATH];
	TCHAR szCurDir[MAX_PATH];

	szFile[0] = 0;

	// Save current directory (avoids mame file creation further failure)
	if (GetCurrentDirectory(MAX_PATH, szCurDir) > MAX_PATH)
	{
		// Path too large
		szCurDir[0] = 0;
	}

	of.lStructSize = sizeof(OPENFILENAME);
	of.hwndOwner = hMain;
	of.hInstance = NULL;

	if (filetype == FILETYPE_GAME_LIST)
		of.lpstrTitle  = TEXT("Enter a name for the game list file");
	else
		of.lpstrTitle  = TEXT("Enter a name for the ROMs list file");

	of.lpstrFilter = TEXT("Standard text file (*.txt)\0*.txt\0");
	of.lpstrCustomFilter = NULL;
	of.nMaxCustFilter = 0;
	of.nFilterIndex = 1;
	of.lpstrFile = szFile;
	of.nMaxFile = sizeof(szFile);
	of.lpstrFileTitle = NULL;
	of.nMaxFileTitle = 0;
	of.lpstrInitialDir = list_directory;
	of.nFileOffset = 0;
	of.nFileExtension = 0;
	of.lpstrDefExt = TEXT("txt");
	of.lCustData = 0;
	of.lpfnHook = &OFNHookProc;
	of.lpTemplateName = NULL;
	of.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLEHOOK;

	while(!success)
	{
		if (GetSaveFileName(&of))
		{
			if (GetFileAttributes(szFile) != -1)
			{
				if (win_message_box_utf8(hMain, "File already exists, overwrite ?", MAMEUINAME, MB_ICONQUESTION | MB_YESNO) != IDYES )
					continue;
				else
					success = true;

				SetFileAttributes(szFile, FILE_ATTRIBUTE_NORMAL);
			}

			SaveGameListToFile(win_utf8_from_wstring(szFile), filetype);
			// Save current directory (avoids mame file creation further failure)
			GetCurrentDirectory(MAX_PATH, list_directory);
			// Restore current file path
			if (szCurDir[0] != 0)
				SetCurrentDirectory(szCurDir);

			success = true;
		}
		else
			break;
	}

	if (success)
		return true;
	else
		return false;
}

static void SaveGameListToFile(char *szFile, int filetype)
{
	int nListCount = ListView_GetItemCount(hWndList);
	const char *CrLf = "\n\n";
	LPTREEFOLDER lpFolder = GetCurrentFolder();
	LVITEM lvi;

	FILE *f = fopen(szFile, "w");

	if (f == NULL)
	{
		ErrorMessageBox("Error : unable to access file");
		return;
	}

	// Title
	fprintf(f, "%s %s.%s", MAMEUINAME, GetVersionString(), CrLf);

	if (filetype == FILETYPE_GAME_LIST)
		fprintf(f, "This is the current list of games.%s", CrLf);
	else
		fprintf(f, "This is the current list of ROMs.%s", CrLf);

	// Current folder
	fprintf(f, "Current folder : <");

	if (lpFolder->m_nParent != -1)
	{
		// Shows only 2 levels (last and previous)
		LPTREEFOLDER lpF = GetFolder(lpFolder->m_nParent);

		if (lpF->m_nParent == -1)
				fprintf(f, "\\");
 
		fprintf(f, "%s", lpF->m_lpTitle);
		fprintf(f, "\\");
	}
	else
		fprintf(f, "\\");
 
	fprintf(f, "%s>%s.%s", lpFolder->m_lpTitle, (lpFolder->m_dwFlags & F_CUSTOM) ? " (custom folder)" : "", CrLf);

	// Sorting
	if (GetSortColumn() > 0)

		fprintf(f, "Sorted by <%s> descending order", win_utf8_from_wstring(column_names[GetSortColumn()]));
	else
		fprintf(f, "Sorted by <%s> ascending order", win_utf8_from_wstring(column_names[-GetSortColumn()]));

	fprintf(f, ", %d game(s) found.%s", nListCount, CrLf);

	// Games
	for (int nIndex = 0; nIndex < nListCount; nIndex++)
	{
		lvi.iItem = nIndex;
		lvi.iSubItem = 0;
		lvi.mask = LVIF_PARAM;

		if (ListView_GetItem(hWndList, &lvi))
		{
			int nGameIndex  = lvi.lParam;

			if (filetype == FILETYPE_GAME_LIST)
				fprintf(f, "%s", GetDriverGameTitle(nGameIndex));
			else
				fprintf(f, "%s", GetDriverGameName(nGameIndex));

			fprintf(f, "\n");
		}
	}

	fclose(f);
	win_message_box_utf8(hMain, "File saved successfully.", MAMEUINAME, MB_ICONINFORMATION | MB_OK);
}

static HBITMAP CreateBitmapTransparent(HBITMAP hSource)
{
	BITMAP bm;

	HDC hSrc = CreateCompatibleDC(NULL);
	HDC hDst = CreateCompatibleDC(NULL);
	GetObject(hSource, sizeof(bm), &bm);
	SelectObject(hSrc, hSource);
	HBITMAP hNew = CreateBitmap(bm.bmWidth, bm.bmHeight, bm.bmPlanes, bm.bmBitsPixel, NULL);
	SelectObject(hDst, hNew);
	BitBlt(hDst, 0, 0, bm.bmWidth, bm.bmHeight, hSrc, 0, 0, SRCCOPY);
	COLORREF clrTP = RGB(239, 239, 239);
	COLORREF clrBK = GetSysColor(COLOR_MENU);

	for (int nRow = 0; nRow < bm.bmHeight; nRow++)
	{
		for (int nCol = 0; nCol < bm.bmWidth; nCol++)
		{
			if (GetPixel(hSrc, nCol, nRow) == clrTP)
				SetPixel(hDst, nCol, nRow, clrBK);
		}
	}

	DeleteDC(hDst);
	DeleteDC(hSrc);
	return hNew;
}
