// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

/***************************************************************************

MSH - 20070809
--
Notes on properties and ini files, reset and reset to default.
----------------------------------------------------------------------------
Each ini contains a complete option set.

Priority order for option sets (Lowest to Highest):

built-in defaults
program     ini (executable root filename ini)
debug       ini (if running a debug build)
vector      ini (really is vector.ini!)
vertical    ini (really is vertical.ini!)
horizont    ini (really is horizont.ini!)
driver      ini (source code root filename in which this driver is found)
grandparent ini (grandparent, not sure these exist, but it is possible)
parent      ini (where parent is the name of the parent driver)
game        ini (where game is the driver name for this game)

To determine which option set to use, start at the top level (lowest
priority), and overlay all higher priority ini's until the desired level
is reached.

The 'default' option set is the next priority higher up the list from
the desired level. For the default (program.ini) level, it is also the
default.

When MAME is run, the desired level is game ini.

Expected Code behavior:
----------------------------------------------------------------------------
This approach requires 3 option sets, 'current', 'original' and 'default'.

'current': used to populate the property pages, and to initialize the
'original' set.

'original': used to evaluate if the 'Reset' button is enabled.
If 'current' matches 'original', the 'Reset' button is disabled,
otherwise it is enabled.

'default': used to evaluate if the 'Restore to Defaults' button is enabled.
If 'current' matches 'default', the 'Restore to Defaults' button is disabled,
otherwise it is enabled.

When editing any option set, the desired level is set to the one being
edited, the default set for that level, is the next lower priority set found.

Upon entering the properties dialog:
a) 'current' is initialized
b) 'original' is initialized by 'current'
c) 'default' is initialized
d) Populate Property pages with 'current'
e) 'Reset' and 'Restore to Defaults' buttons are evaluated.

After any change:
a) 'current' is updated
b) 'Reset' and 'Restore to Defaults' buttons are re-evaluated.

Reset:
a) 'current' is reinitialized to 'original'
b) Re-populate Property pages with 'current'
c) 'Reset' and 'Restore to Defaults' buttons are re-evaluated.

Restore to Defaults:
a) 'current' is reinitialized to 'default'
b) Re-populate Property pages with 'current'
b) 'Reset' and 'Restore to Defaults' buttons are re-evaluated.

Apply:
a) 'original' is reinitialized to 'current'
b) 'Reset' and 'Restore to defaults' are re-evaluated.
c) If they 'current' matches 'default', remove the ini from disk.
   Otherwise, write the ini to disk.

Cancel:
a) Exit the dialog.

OK:
a) If they 'current' matches 'default', remove the ini from disk.
   Otherwise, write the ini to disk.
b) Exit the dialog.


***************************************************************************/

#include "winui.h"

/**************************************************************
 * Local function prototypes
 **************************************************************/

static INT_PTR CALLBACK GameOptionsDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
static void InitializeOptions(HWND hDlg);
static void OptOnHScroll(HWND hWnd, HWND hWndCtl, UINT code, int pos);
static void NumScreensSelectionChange(HWND hWnd);
static void RefreshSelectionChange(HWND hWnd, HWND hWndCtrl);
static void InitializeSampleRateUI(HWND hWnd);
static void InitializeSoundModeUI(HWND hWnd);
static void InitializeSkippingUI(HWND hWnd);
static void InitializeRotateUI(HWND hWnd);
static void UpdateSelectScreenUI(HWND hWnd);
static void InitializeSelectScreenUI(HWND hWnd);
static void InitializeVideoUI(HWND hWnd);
static void InitializeSnapViewUI(HWND hWnd);
static void InitializeSnapNameUI(HWND hWnd);
static void InitializeBIOSUI(HWND hWnd);
static void InitializeControllerMappingUI(HWND hWnd);
static void InitializeLanguageUI(HWND hWnd);
static void InitializePluginsUI(HWND hWnd);
static void UpdateOptions(HWND hDlg, datamap *map, windows_options &opts);
static void UpdateProperties(HWND hDlg, datamap *map, windows_options &opts);
static void PropToOptions(HWND hWnd, windows_options &opts);
static void OptionsToProp(HWND hWnd, windows_options &opts);
static void SetPropEnabledControls(HWND hWnd);
static bool SelectEffect(HWND hWnd);
static bool ResetEffect(HWND hWnd);
static bool SelectMameShader(HWND hWnd, int slot);
static bool ResetMameShader(HWND hWnd, int slot);
static void UpdateMameShader(HWND hWnd, int slot, windows_options &opts);
static bool SelectScreenShader(HWND hWnd, int slot);
static bool ResetScreenShader(HWND hWnd, int slot);
static void UpdateScreenShader(HWND hWnd, int slot, windows_options &opts);
static bool SelectCheatFile(HWND hWnd);
static bool ResetCheatFile(HWND hWnd);
static bool ChangeJoystickMap(HWND hWnd);
static bool ResetJoystickMap(HWND hWnd);
static bool SelectLUAScript(HWND hWnd);
static bool ResetLUAScript(HWND hWnd);
static bool SelectPlugins(HWND hWnd);
static bool ResetPlugins(HWND hWnd);
static bool SelectBGFXChains(HWND hWnd);
static bool ResetBGFXChains(HWND hWnd);
static void BuildDataMap(void);
static void ResetDataMap(HWND hWnd);
//mamefx: for coloring of changed elements
static bool IsControlOptionValue(HWND hDlg, HWND hWnd_ctrl, windows_options &opts, windows_options &ref);
static void DisableVisualStyles(HWND hDlg);
static void MovePropertySheetChildWindows(HWND hWnd, int nDx, int nDy);
static HTREEITEM GetSheetPageTreeItem(int nPage);
static int GetSheetPageTreeCurSelText(LPWSTR lpszText, int iBufSize);
static void ModifyPropertySheetForTreeSheet(HWND hPageDlg);

/**************************************************************
 * Local private variables
 **************************************************************/

static windows_options pDefaultOpts;
static windows_options pOrigOpts;
static windows_options pCurrentOpts;
static datamap *properties_datamap;
static int g_nGame = 0;
static int g_nFolder = 0;
static int g_nFolderGame = 0;
static int g_nPropertyMode = 0;
static bool g_bUseDefaults = false;
static bool g_bReset = false;
static bool g_bAutoAspect[MAX_SCREENS] = {false, false, false, false};
static bool g_bAutoSnapSize = false;
static HICON hIcon = NULL;
static HBRUSH hBrush = NULL;
static HBRUSH hBrushDlg = NULL;
static HDC hDC = NULL;
static int status_color = 0;
static int g_nFirstInitPropertySheet = 0;
static RECT rcTabCtrl;
static RECT rcTabCaption;
static RECT rcChild;
static int nCaptionHeight = 0;
static HWND hSheetTreeCtrl = NULL;
static HINSTANCE hSheetInstance = 0;
static WNDPROC pfnOldSheetProc = NULL;
static bool bPageTreeSelChangedActive = false;
//mamefx: for coloring of changed elements
static windows_options pOptsGlobal;
static windows_options pOptsHorizontal;
static windows_options pOptsVertical;
static windows_options pOptsRaster;
static windows_options pOptsVector;
static windows_options pOptsSource;

static struct PropSheets
{
	DWORD dwDlgID;
	DLGPROC pfnDlgProc;
} g_PropSheets[] = 
{
	{ IDD_PROP_DISPLAY,		GameOptionsDialogProc },
	{ IDD_PROP_ADVANCED,	GameOptionsDialogProc },
	{ IDD_PROP_SCREEN,		GameOptionsDialogProc },
	{ IDD_PROP_OPENGL,		GameOptionsDialogProc },
	{ IDD_PROP_SHADER,		GameOptionsDialogProc },
	{ IDD_PROP_VECTOR,		GameOptionsDialogProc },
	{ IDD_PROP_SOUND,		GameOptionsDialogProc },
	{ IDD_PROP_INPUT,		GameOptionsDialogProc },
	{ IDD_PROP_CONTROLLER,	GameOptionsDialogProc },
	{ IDD_PROP_MISC,		GameOptionsDialogProc },
	{ IDD_PROP_MISC2,		GameOptionsDialogProc },
	{ IDD_PROP_SNAP,		GameOptionsDialogProc }
};

static struct ComboBoxVideo
{
	const TCHAR*	m_pText;
	const char*		m_pData;
} g_ComboBoxVideo[] =
{
	{ TEXT("Auto"),         "auto"   },
	{ TEXT("GDI"),			"gdi"    },
	{ TEXT("Direct3D"),     "d3d"    },
	{ TEXT("OpenGL"),       "opengl" },
	{ TEXT("BGFX"),         "bgfx"   }
//	{ TEXT("None"),         "none"   }
};

static struct ComboBoxSound
{
	const TCHAR*	m_pText;
	const char*		m_pData;
} g_ComboBoxSound[] =
{
	{ TEXT("Auto"),         "auto"   },
	{ TEXT("DirectSound"),  "dsound" },
	{ TEXT("None"),         "none"   }
};

static struct ComboBoxSampleRate
{
	const TCHAR*	m_pText;
	const int		m_pData;
} g_ComboBoxSampleRate[] =
{
	{ TEXT("11025"),    	11025 },
	{ TEXT("22050"),    	22050 },
	{ TEXT("44100"),    	44100 },
	{ TEXT("48000"),    	48000 }
};

static struct ComboBoxSelectScreen
{
	const TCHAR*	m_pText;
	const int		m_pData;
} g_ComboBoxSelectScreen[] =
{
	{ TEXT("Screen 0"),    	0 },
	{ TEXT("Screen 1"),    	1 },
	{ TEXT("Screen 2"),    	2 },
	{ TEXT("Screen 3"),    	3 }
};

static struct ComboBoxView
{
	const TCHAR*	m_pText;
	const char*		m_pData;
} g_ComboBoxView[] =
{
	{ TEXT("Auto"),		   	"auto"     },
	{ TEXT("Standard"),   	"standard" },
	{ TEXT("Pixel aspect"),	"pixel"    },
	{ TEXT("Cocktail"),    	"cocktail" }
};

static struct ComboBoxFrameSkip
{
	const TCHAR*	m_pText;
	const int		m_pData;
} g_ComboBoxFrameSkip[] = 
{
	{ TEXT("Draw every frame"),	0  },
	{ TEXT("Skip 1 frame"),		1  },
	{ TEXT("Skip 2 frames"),	2  },
	{ TEXT("Skip 3 frames"), 	3  },
	{ TEXT("Skip 4 frames"), 	4  },
	{ TEXT("Skip 5 frames"), 	5  },
	{ TEXT("Skip 6 frames"), 	6  },
	{ TEXT("Skip 7 frames"), 	7  },
	{ TEXT("Skip 8 frames"), 	8  },
	{ TEXT("Skip 9 frames"), 	9  },
	{ TEXT("Skip 10 frames"), 	10 }
};

static struct ComboBoxDevices
{
	const TCHAR*	m_pText;
	const char* 	m_pData;
} g_ComboBoxDevice[] =
{
	{ TEXT("None"),        	"none"     },
	{ TEXT("Keyboard"),    	"keyboard" },
	{ TEXT("Mouse"),		"mouse"    },
	{ TEXT("Joystick"),   	"joystick" },
	{ TEXT("Lightgun"),   	"lightgun" }
};

static struct ComboBoxSnapName
{
	const TCHAR*	m_pText;
	const char*		m_pData;
} g_ComboBoxSnapName[] =
{
	{ TEXT("Gamename"),		   					"%g" 	  },
	{ TEXT("Gamename + increment"),	   			"%g%i" 	  },
	{ TEXT("Gamename/gamename"),    			"%g/%g"   },
	{ TEXT("Gamename/gamename + increment"),	"%g/%g%i" },
	{ TEXT("Gamename/increment"),    			"%g/%i"   }
};

static struct ComboBoxSnapView
{
	const TCHAR*	m_pText;
	const char*		m_pData;
} g_ComboBoxSnapView[] =
{
	{ TEXT("Auto"),		   	"auto"     },
	{ TEXT("Internal"),	   	"internal" },
	{ TEXT("Standard"),    	"standard" },
	{ TEXT("Pixel aspect"),	"pixel"    },
	{ TEXT("Cocktail"),    	"cocktail" }
};

/***************************************************************
 * Public functions
 ***************************************************************/

// This function (and the code that use it) is a gross hack - but at least the vile
// and disgusting global variables are gone, making it less gross than what came before
static int GetSelectedScreen(HWND hWnd)
{
	int nSelectedScreen = 0;
	HWND hCtrl = GetDlgItem(hWnd, IDC_SCREENSELECT);

	if (hCtrl)
		nSelectedScreen = ComboBox_GetCurSel(hCtrl);

	if ((nSelectedScreen < 0) || (nSelectedScreen >= NUMSELECTSCREEN))
		nSelectedScreen = 0;

	return nSelectedScreen;
}

static PROPSHEETPAGE *CreatePropSheetPages(HINSTANCE hInst, UINT *pnMaxPropSheets)
{
	PROPSHEETPAGE *pspages = (PROPSHEETPAGE *)malloc(sizeof(PROPSHEETPAGE) * NUMPAGES);

	if (!pspages)
		return NULL;

	memset(pspages, 0, sizeof(PROPSHEETPAGE) * NUMPAGES);

	for (int i = 0; i < NUMPAGES; i++)
	{
		pspages[i].dwSize = sizeof(PROPSHEETPAGE);
		pspages[i].dwFlags = 0;
		pspages[i].hInstance = hInst;
		pspages[i].pszTemplate = MAKEINTRESOURCE(g_PropSheets[i].dwDlgID);
		pspages[i].pfnCallback = NULL;
		pspages[i].lParam = 0;
		pspages[i].pfnDlgProc = g_PropSheets[i].pfnDlgProc;
	}

	if (pnMaxPropSheets)
		*pnMaxPropSheets = NUMPAGES;

	return pspages;
}

/* Initialize the property pages */
void InitPropertyPage(HINSTANCE hInst, HWND hWnd, OPTIONS_TYPE opt_type, int folder_id, int game_num)
{
	PROPSHEETHEADER pshead;
	OPTIONS_TYPE default_type = opt_type;
	char tmp[512];

	// Load the current options, this will pickup the highest priority option set.
	LoadOptions(pCurrentOpts, opt_type, game_num);

	// Load the default options, pickup the next lower options set than the current level.
	if (opt_type > OPTIONS_GLOBAL)
	{
		default_type = (OPTIONS_TYPE)(default_type - 1);

		if (OPTIONS_VERTICAL == opt_type) 
			//since VERTICAL and HORIZONTAL are equally ranked
			//we need to subtract 2 from vertical to also get to correct default
			default_type = (OPTIONS_TYPE)(default_type - 1);

		if (OPTIONS_VECTOR == opt_type) 
			//since VECTOR and RASTER are equally ranked
			//we need to subtract 2 from vector to also get to correct default
			default_type = (OPTIONS_TYPE)(default_type - 1);	
	}

	LoadOptions(pDefaultOpts, default_type, game_num);
	//mamefx: for coloring of changed elements
	LoadOptions(pOptsGlobal, OPTIONS_GLOBAL, game_num);
	LoadOptions(pOptsHorizontal, OPTIONS_HORIZONTAL, game_num);
	LoadOptions(pOptsVertical, OPTIONS_VERTICAL, game_num);
	LoadOptions(pOptsRaster, OPTIONS_RASTER, game_num);
	LoadOptions(pOptsVector, OPTIONS_VECTOR, game_num);
	LoadOptions(pOptsSource, OPTIONS_SOURCE, game_num);
	// Copy current_options to original options
	pOrigOpts = pCurrentOpts;
	// These MUST be valid, they are used as indicies
	g_nGame = game_num;
	g_nFolder = folder_id;
	// Keep track of OPTIONS_TYPE that was passed in.
	g_nPropertyMode = opt_type;
	// Evaluate if the current set uses the Default set
	g_bUseDefaults = (pCurrentOpts == pDefaultOpts);
	g_bReset = false;
	BuildDataMap();
	// Create the property sheets
	memset(&pshead, 0, sizeof(PROPSHEETHEADER));
	PROPSHEETPAGE *pspage = CreatePropSheetPages(hInst, &pshead.nPages);

	if (!pspage)
		return;

	// Get the description use as the dialog caption.
	switch(opt_type)
	{
		case OPTIONS_GAME:
			snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "Properties for %s", GetDriverGameTitle(g_nGame));
			break;

		case OPTIONS_RASTER:
		case OPTIONS_VECTOR:
		case OPTIONS_VERTICAL:
		case OPTIONS_HORIZONTAL:
			snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "Properties for %s games", GetFolderNameByID(g_nFolder));
			break;

		case OPTIONS_SOURCE:
			snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "Properties for %s driver games", GetDriverFileName(g_nGame));
			break;

		case OPTIONS_GLOBAL:
			strcpy(tmp, "Properties for all games");
			break;

		default:
			free(pspage);
			return;
	}

	TCHAR *t_description = win_wstring_from_utf8(tmp);

	// If we have no descrption, return.
	if(!t_description)
	{
		free(pspage);
		return;
	}

	/* Fill in the property sheet header */
	pshead.pszCaption = t_description;
	pshead.hwndParent = hWnd;
	pshead.dwSize = sizeof(PROPSHEETHEADER);
	pshead.dwFlags = PSH_PROPSHEETPAGE | PSH_USEICONID | PSH_DEFAULT | PSH_NOCONTEXTHELP;
	pshead.hInstance = hInst;
	pshead.nStartPage = 0;
	pshead.pszIcon = MAKEINTRESOURCE(IDI_MAMEUI_ICON);
	pshead.ppsp = pspage;

	g_nFirstInitPropertySheet = 1;
	hSheetInstance = hInst;

	/* Create the Property sheet and display it */
	if (PropertySheet(&pshead) == -1)
	{
		DWORD dwError = GetLastError();
		ErrorMessageBox("PropertySheet creation error %d %X", (int)dwError, (int)dwError);
	}

	free(t_description);
	free(pspage);
}

/*********************************************************************
 * Local Functions
 *********************************************************************/

/* Build CPU info string */
static char *GameInfoCPU(int nIndex)
{
	machine_config config(driver_list::driver(nIndex), MameUIGlobal());
	execute_interface_iterator cpuiter(config.root_device());
	std::unordered_set<std::string> exectags;
	static char buffer[1024];

	memset(&buffer, 0, sizeof(buffer));

	for (device_execute_interface &exec : cpuiter)
	{
		if (!exectags.insert(exec.device().tag()).second)
				continue;

		char temp[300];
		int count = 1;
		int clock = exec.device().clock();
		const char *name = exec.device().name();

		for (device_execute_interface &scan : cpuiter)
		{
			if (exec.device().type() == scan.device().type() && strcmp(name, scan.device().name()) == 0 && clock == scan.device().clock())
				if (exectags.insert(scan.device().tag()).second)
					count++;
		}

		if (count > 1)
		{
			snprintf(temp, WINUI_ARRAY_LENGTH(temp), "%d x ", count);
			strcat(buffer, temp);
		}

		if (clock >= 1000000)
			snprintf(temp, WINUI_ARRAY_LENGTH(temp), "%s %d.%06d MHz\r\n", name, clock / 1000000, clock % 1000000);
		else
			snprintf(temp, WINUI_ARRAY_LENGTH(temp), "%s %d.%03d kHz\r\n", name, clock / 1000, clock % 1000);

		strcat(buffer, temp);
	}

	return buffer;
}

/* Build Sound system info string */
static char *GameInfoSound(int nIndex)
{
	machine_config config(driver_list::driver(nIndex), MameUIGlobal());
	sound_interface_iterator sounditer(config.root_device());
	std::unordered_set<std::string> soundtags;
	static char buffer[1024];

	memset(&buffer, 0, sizeof(buffer));

	for (device_sound_interface &sound : sounditer)
	{
		if (!soundtags.insert(sound.device().tag()).second)
				continue;

		char temp[300];
		int count = 1;
		int clock = sound.device().clock();
		const char *name = sound.device().name();

		for (device_sound_interface &scan : sounditer)
		{
			if (sound.device().type() == scan.device().type() && strcmp(name, scan.device().name()) == 0 && clock == scan.device().clock())
				if (soundtags.insert(scan.device().tag()).second)
					count++;
		}

		if (count > 1)
		{
			snprintf(temp, WINUI_ARRAY_LENGTH(temp), "%d x ", count);
			strcat(buffer, temp);
		}

		strcat(buffer, name);

		if (clock)
		{
			if (clock >= 1000000)
				snprintf(temp, WINUI_ARRAY_LENGTH(temp), " %d.%06d MHz", clock / 1000000, clock % 1000000);
			else
				snprintf(temp, WINUI_ARRAY_LENGTH(temp), " %d.%03d kHz", clock / 1000, clock % 1000);

			strcat(buffer, temp);
		}

		strcat(buffer, "\r\n");
	}

	return buffer;
}

/* Build Display info string */
static char *GameInfoScreen(int nIndex)
{
	machine_config config(driver_list::driver(nIndex), MameUIGlobal());
	static char buffer[1024];

	memset(&buffer, 0, sizeof(buffer));

	if (DriverIsVector(nIndex))
	{
		if (DriverIsVertical(nIndex))
			strcpy(buffer, "Vector (V)");
		else
			strcpy(buffer, "Vector (H)");
	}
	else
	{
		screen_device_iterator screeniter(config.root_device());
		int scrcount = screeniter.count();

		if (scrcount == 0)
			strcpy(buffer, "Screenless");
		else
		{
			for (screen_device &screen : screeniter)
			{
				const rectangle &visarea = screen.visible_area();
				char tmpbuf[256];

				if (DriverIsVertical(nIndex))
					snprintf(tmpbuf, WINUI_ARRAY_LENGTH(tmpbuf), "%d x %d (V) %f Hz\r\n", visarea.width(), visarea.height(), ATTOSECONDS_TO_HZ(screen.refresh_attoseconds()));
				else
					snprintf(tmpbuf, WINUI_ARRAY_LENGTH(tmpbuf), "%d x %d (H) %f Hz\r\n", visarea.width(), visarea.height(), ATTOSECONDS_TO_HZ(screen.refresh_attoseconds()));

				strcat(buffer, tmpbuf);
			}
		}
	}

	return buffer;
}

/* Build game status string */
static char *GameInfoStatus(int driver_index)
{
	static char buffer[1024];

	memset(&buffer, 0, sizeof(buffer));

	//Just show the emulation flags
	if (DriverIsBroken(driver_index))
	{
		strcpy(buffer, "Not working");
		strcat(buffer, "\r\n");
		strcat(buffer, "Game doesn't work properly");

		if (driver_list::driver(driver_index).flags & MACHINE_UNEMULATED_PROTECTION)
		{
			strcat(buffer, "\r\n");
			strcat(buffer, "Game protection isn't fully emulated");
		}

		if (driver_list::driver(driver_index).flags & MACHINE_MECHANICAL)
		{
			strcat(buffer, "\r\n");
			strcat(buffer, "Game has mechanical parts");
		}

		status_color = 1;
		return buffer;
	}

	if (DriverIsImperfect(driver_index))	
	{
		strcpy(buffer, "Working with problems");
		status_color = 2;

		if (driver_list::driver(driver_index).flags & MACHINE_WRONG_COLORS)
		{
			strcat(buffer, "\r\n");
			strcat(buffer, "Colors are completely wrong");
		}

		if (driver_list::driver(driver_index).flags & MACHINE_IMPERFECT_COLORS)
		{
			strcat(buffer, "\r\n");
			strcat(buffer, "Colors aren't 100% accurate");
		}

		if (driver_list::driver(driver_index).flags & MACHINE_IMPERFECT_GRAPHICS)
		{
			strcat(buffer, "\r\n");
			strcat(buffer, "Video emulation isn't 100% accurate");
		}

		if (driver_list::driver(driver_index).flags & MACHINE_NO_SOUND)
		{
			strcat(buffer, "\r\n");
			strcat(buffer, "Game lacks sound");
		}

		if (driver_list::driver(driver_index).flags & MACHINE_IMPERFECT_SOUND)
		{
			strcat(buffer, "\r\n");
			strcat(buffer, "Sound emulation isn't 100% accurate");
		}

		if (driver_list::driver(driver_index).flags & MACHINE_IS_INCOMPLETE)
		{
			strcat(buffer, "\r\n");
			strcat(buffer, "Game was never completed");
		}

		if (driver_list::driver(driver_index).flags & MACHINE_NO_SOUND_HW)
		{
			strcat(buffer, "\r\n");
			strcat(buffer, "Game has no sound hardware");
		}

		return buffer;
	}
	else
	{
		strcpy(buffer, "Working");
		status_color = 0;
		return buffer;
	}
}

/* Build game manufacturer string */
static char *GameInfoManufactured(int nIndex)
{
	static char buffer[1024];

	memset(&buffer, 0, sizeof(buffer));
	snprintf(buffer, WINUI_ARRAY_LENGTH(buffer), "%s %s", GetDriverGameYear(nIndex), GetDriverGameManufacturer(nIndex));
	return buffer;
}

/* Build Game title string */
static char *GameInfoTitle(OPTIONS_TYPE opt_type, int nIndex)
{
	static char buffer[1024];

	memset(&buffer, 0, sizeof(buffer));

	if (OPTIONS_GLOBAL == opt_type)
		strcpy(buffer, "Global options\r\nDefault options used by all games");
	else if (OPTIONS_RASTER == opt_type)
		strcpy(buffer, "Raster options\r\nDefault options used by all raster games");
	else if (OPTIONS_VECTOR == opt_type)
		strcpy(buffer, "Vector options\r\nDefault options used by all vector games");	
	else if (OPTIONS_VERTICAL == opt_type)
		strcpy(buffer, "Vertical options\r\nDefault options used by all vertical games");
	else if (OPTIONS_HORIZONTAL == opt_type)
		strcpy(buffer, "Horizontal options\r\nDefault options used by all horizontal games");
	else if (OPTIONS_SOURCE == opt_type)
		strcpy(buffer, "Driver options\r\nDefault options used by all games in the driver");
	else
		snprintf(buffer, WINUI_ARRAY_LENGTH(buffer), "%s - \"%s\"", GetDriverGameTitle(nIndex), GetDriverGameName(nIndex));

	return buffer;
}

/* Build game clone infromation string */
static char *GameInfoCloneOf(int nIndex)
{
	static char buffer[1024];

	memset(&buffer, 0, sizeof(buffer));

	if (DriverIsClone(nIndex))
	{
		int nParentIndex = GetParentIndex(&driver_list::driver(nIndex));
		snprintf(buffer, WINUI_ARRAY_LENGTH(buffer), "%s - \"%s\"", GetDriverGameTitle(nParentIndex), GetDriverGameName(nParentIndex));
	}

	return buffer;
}

static char *GameInfoSaveState(int driver_index)
{
	static char buffer[1024];

	memset(&buffer, 0, sizeof(buffer));

	if (DriverSupportsSaveState(driver_index))
		strcpy(buffer, "Supported");
	else
		strcpy(buffer, "Unsupported");

	return buffer;
}

/* Display Tree Sheet */
static void UpdateSheetCaption(HWND hWnd)
{
	PAINTSTRUCT ps;
	HRGN hRgn;
	RECT rect, rc;
	TCHAR szText[256];

	memcpy(&rect, &rcTabCaption, sizeof(RECT));
	BeginPaint (hWnd, &ps);
	hDC = ps.hdc;
	hRgn = CreateRectRgn(rect.left, rect.top, rect.right - 2, rect.bottom);
	SelectClipRgn(hDC, hRgn);
	hBrush = CreateSolidBrush(RGB(127, 127, 127));
	FillRect(hDC, &rect, hBrush);
	DeleteObject(hBrush);
	int i = GetSheetPageTreeCurSelText(szText, WINUI_ARRAY_LENGTH(szText));

	if (i > 0)
	{
		HFONT hFontCaption = CreateFont(-16, 0,		// height, width
			0, 										// angle of escapement
			0,										// base-line orientation angle
			700,									// font weight
			0, 0, 0, 								// italic, underline, strikeout
			0,										// character set identifier
			3,										// output precision
			2,										// clipping precision
			2,										// output quality
			34,										// pitch and family
			TEXT("Arial"));							// typeface name

		HFONT hOldFont = (HFONT)SelectObject(hDC, hFontCaption);
		SetTextColor(hDC, RGB(255, 255, 255));
		SetBkMode(hDC, TRANSPARENT);
		memcpy(&rc, &rect, sizeof(RECT));
		rc.left += 4;
		DrawText(hDC, szText, _tcslen(szText), &rc, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
		SelectObject(hDC, hOldFont);
		DeleteObject(hFontCaption);
	}

	SelectClipRgn(hDC, NULL);
	DeleteObject(hRgn);
	rect.left = SHEET_TREE_WIDTH + 15;
	rect.top = 8;
	rect.right = rcTabCaption.right - 1;
	rect.bottom = rcTabCtrl.bottom + 4;
	hRgn = CreateRectRgn(rect.left, rect.top, rect.right, rect.bottom);
	SelectClipRgn(hDC, hRgn);
	hBrush = CreateSolidBrush(RGB(127, 127, 127));
	FrameRect(hDC, &rect, hBrush);
	DeleteObject(hBrush);
	SelectClipRgn(hDC, NULL);
	DeleteObject(hRgn);
	EndPaint (hWnd, &ps);
	return;
}

static LRESULT CALLBACK NewSheetWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	bool bHandled = false;
	TVITEM item;

	switch (Msg)
	{
		case WM_PAINT:
			UpdateSheetCaption(hWnd);
			bHandled = true;
			break;

		case WM_NOTIFY:
			switch (((NMHDR *)lParam)->code)
			{
				case TVN_SELCHANGING:
					if ((bPageTreeSelChangedActive == false) && (g_nFirstInitPropertySheet == 0))
					{
						NMTREEVIEW* pTvn = (NMTREEVIEW*)lParam;
						bPageTreeSelChangedActive = true;

						item.hItem = pTvn->itemNew.hItem;
						item.mask = TVIF_PARAM;

						(void)TreeView_GetItem(hSheetTreeCtrl, &item);
						int nPage = (int)item.lParam;

						if (nPage >= 0)
							PropSheet_SetCurSel(hWnd, 0, nPage);

						bPageTreeSelChangedActive = false;
						bHandled = true;
					}
					break;

				case TVN_SELCHANGED:
					InvalidateRect(hWnd, &rcTabCaption, false);
					bHandled = true;
					break;
			}

			break;

		case WM_DESTROY:
			if (hSheetTreeCtrl != NULL)
			{
				DestroyWindow(hSheetTreeCtrl);
				hSheetTreeCtrl = NULL;
			}

			if (pfnOldSheetProc)
				SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)pfnOldSheetProc);

			break;
	}

	if ((bHandled == false) && pfnOldSheetProc)
		return CallWindowProc(pfnOldSheetProc, hWnd, Msg, wParam, lParam);

	return false;
}

static void AdjustChildWindows(HWND hWnd)
{
	TCHAR szClass[128];

	GetClassName(hWnd, szClass, WINUI_ARRAY_LENGTH(szClass));

	if (!_tcscmp(szClass, WC_BUTTON))
	{
		DWORD dwStyle = GetWindowLong(hWnd, GWL_STYLE);

		if (((dwStyle & BS_GROUPBOX) == BS_GROUPBOX) && (dwStyle & WS_TABSTOP))
			SetWindowLong(hWnd, GWL_STYLE, (dwStyle & ~WS_TABSTOP));
	}
}

static void AdjustPropertySheetChildWindows(HWND hWnd)
{
	HWND hChild = GetWindow(hWnd, GW_CHILD);

	while (hChild)
	{
		hChild = GetNextWindow(hChild, GW_HWNDNEXT);
	}
}

static void MovePropertySheetChildWindows(HWND hWnd, int nDx, int nDy)
{
	HWND hChild = GetWindow(hWnd, GW_CHILD);

	while (hChild)
	{
		GetWindowRect(hChild, &rcChild);
		OffsetRect(&rcChild, nDx, nDy);
		ScreenToClient(hWnd, (LPPOINT)&rcChild);
		ScreenToClient(hWnd, ((LPPOINT)&rcChild) + 1);
		AdjustChildWindows(hChild);
		MoveWindow(hChild, rcChild.left, rcChild.top, rcChild.right - rcChild.left, rcChild.bottom - rcChild.top, true);
		hChild = GetNextWindow(hChild, GW_HWNDNEXT);
	}
}

static HTREEITEM GetSheetPageTreeItem(int nPage)
{
	TVITEM item;

	if (hSheetTreeCtrl == NULL)
		return NULL;

	HTREEITEM hItem = TreeView_GetRoot(hSheetTreeCtrl);

	while (hItem)
	{
		item.hItem = hItem;
		item.mask = TVIF_PARAM;
		(void)TreeView_GetItem(hSheetTreeCtrl, &item);
		int nTreePage = (int)item.lParam;

		if (nTreePage == nPage)
			return hItem;

		hItem = TreeView_GetNextSibling(hSheetTreeCtrl, hItem);
	}

	return NULL;
}

static int GetSheetPageTreeCurSelText(TCHAR* lpszText, int iBufSize)
{
	TVITEM item;

	lpszText[0] = 0;

	if (hSheetTreeCtrl == NULL)
		return -1;

	HTREEITEM hItem = TreeView_GetSelection(hSheetTreeCtrl);

	if (hItem == NULL)
		return -1;

	item.hItem = hItem;
	item.mask = TVIF_TEXT;
	item.pszText = lpszText;
	item.cchTextMax = iBufSize;

	(void)TreeView_GetItem(hSheetTreeCtrl, &item);
	return _tcslen(lpszText);
}

static void ModifyPropertySheetForTreeSheet(HWND hPageDlg)
{
	RECT rectSheet, rectTree;
	LONG_PTR prevProc;
	TCITEM item;
	HTREEITEM hItem;
	int nPage = 0;

	if (g_nFirstInitPropertySheet == 0)
	{
		AdjustPropertySheetChildWindows(hPageDlg);
		return;
	}

	HWND hWnd = GetParent(hPageDlg);

	if (!hWnd)
		return;

	prevProc = GetWindowLongPtr(hWnd, GWLP_WNDPROC);
	pfnOldSheetProc = (WNDPROC)prevProc;
	SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)NewSheetWndProc);
	HWND hTabWnd = PropSheet_GetTabControl(hWnd);

	if (!hTabWnd)
		return;

	DWORD tabStyle = (GetWindowLong(hTabWnd, GWL_STYLE) & ~TCS_MULTILINE);
	SetWindowLong(hTabWnd, GWL_STYLE, tabStyle | TCS_SINGLELINE);
	ShowWindow(hTabWnd, SW_HIDE);
	EnableWindow(hTabWnd, false);
	GetWindowRect(hTabWnd, &rcTabCtrl);
	ScreenToClient(hTabWnd, (LPPOINT)&rcTabCtrl);
	ScreenToClient(hTabWnd, ((LPPOINT)&rcTabCtrl) + 1);
	GetWindowRect(hWnd, &rectSheet);
	rectSheet.right += SHEET_TREE_WIDTH + 5;
	SetWindowPos(hWnd, HWND_TOP, 0, 0, rectSheet.right - rectSheet.left, rectSheet.bottom - rectSheet.top, SWP_NOZORDER | SWP_NOMOVE);
	CenterWindow(hWnd);
	MovePropertySheetChildWindows(hWnd, SHEET_TREE_WIDTH + 6, 0);

	if (hSheetTreeCtrl != NULL)
	{
		DestroyWindow(hSheetTreeCtrl);
		hSheetTreeCtrl = NULL;
	}

	memset(&rectTree, 0, sizeof(RECT));
	HWND hTempTab = CreateWindowEx(0, WC_TABCONTROL, NULL, WS_CHILD | WS_CLIPSIBLINGS,
		rectTree.left, rectTree.top, rectTree.right - rectTree.left, rectTree.bottom - rectTree.top,
		hWnd, (HMENU)0x1234, hSheetInstance, NULL);

	item.mask = TCIF_TEXT;
	item.iImage = 0;
	item.lParam = 0;
	item.pszText = (TCHAR*)TEXT("");

	(void)TabCtrl_InsertItem(hTempTab, 0, &item);
	(void)TabCtrl_GetItemRect(hTempTab, 0, &rcTabCaption);
	nCaptionHeight = (rcTabCaption.bottom - rcTabCaption.top);
	rcTabCaption.left = rcTabCtrl.left + SHEET_TREE_WIDTH + 16;
	rcTabCaption.top = 8;
	rcTabCaption.right = rcTabCaption.left + (rcTabCtrl.right - rcTabCtrl.left - 6);
	rcTabCaption.bottom = rcTabCaption.top + nCaptionHeight;
	DestroyWindow(hTempTab);
	rectTree.left = rcTabCtrl.left + 8;
	rectTree.top = rcTabCtrl.top  + 8;
	rectTree.right = rcTabCtrl.left + SHEET_TREE_WIDTH + 2;
	rectTree.bottom = rcTabCtrl.bottom + 4;
	hSheetTreeCtrl = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_NOPARENTNOTIFY, WC_TREEVIEW, NULL,
		WS_TABSTOP | WS_CHILD | WS_VISIBLE | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT | TVS_TRACKSELECT, 
		rectTree.left, rectTree.top, rectTree.right - rectTree.left, rectTree.bottom - rectTree.top,
		hWnd, (HMENU)0x7EEE, hSheetInstance, NULL);

	if (IsWindowsSevenOrHigher())
		SendMessage(hSheetTreeCtrl, TVM_SETEXTENDEDSTYLE, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

	SetWindowTheme(hSheetTreeCtrl, L"Explorer", NULL);
	(void)TreeView_SetItemHeight(hSheetTreeCtrl, 34);

	if (hSheetTreeCtrl == NULL)
	{
		DWORD dwError = GetLastError();
		ErrorMessageBox("PropertySheet TreeCtrl creation error %d %X", (int)dwError, (int)dwError);
	}

	HFONT hTreeSheetFont = CreateFont(-11, 0, 0, 0, 400, 0, 0, 0, 0, 3, 2, 1, 34, TEXT("Verdana"));
	SetWindowFont(hSheetTreeCtrl, hTreeSheetFont, true);
	(void)TreeView_DeleteAllItems(hSheetTreeCtrl);
	int nPageCount = TabCtrl_GetItemCount(hTabWnd);
	HIMAGELIST hTreeList = ImageList_Create(32, 32, ILC_COLORDDB | ILC_MASK, nPageCount, 0);

	for (int i = 0; i < nPageCount; i++)
	{
		HICON hIconList = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DISPLAYSHEET + i));
		ImageList_AddIcon(hTreeList, hIconList);
	}

	(void)TreeView_SetImageList(hSheetTreeCtrl, hTreeList, TVSIL_NORMAL);

	for (nPage = 0; nPage < nPageCount; nPage++)
	{
		TCHAR szText[256];
		TCITEM ti;
		TVINSERTSTRUCT tvis;
		LPTVITEM lpTvItem;

		// Get title and image of the page
		memset(&ti, 0, sizeof(TCITEM));
		ti.mask = TCIF_TEXT | TCIF_IMAGE;
		ti.cchTextMax = sizeof(szText);
		ti.pszText = szText;

		(void)TabCtrl_GetItem(hTabWnd, nPage, &ti);
		lpTvItem = &tvis.item;

		// Create an item in the tree for the page
		tvis.hParent = TVI_ROOT;
		tvis.hInsertAfter = TVI_LAST;
		lpTvItem->mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		lpTvItem->pszText = szText;
		lpTvItem->iImage = nPage;
		lpTvItem->iSelectedImage = nPage;
		lpTvItem->state = 0;
		lpTvItem->stateMask = 0;
		lpTvItem->lParam = (LPARAM)NULL;

		// insert Item
		hItem = TreeView_InsertItem(hSheetTreeCtrl, &tvis);

		if (hItem)
		{
			TVITEM item;

			item.hItem = hItem;
			item.mask = TVIF_PARAM;
			item.pszText = NULL;
			item.iImage = 0;
			item.iSelectedImage = 0;
			item.state = 0;
			item.stateMask = 0;
			item.lParam = nPage;

			(void)TreeView_SetItem(hSheetTreeCtrl, &item);
		}
	}

	nPage = TabCtrl_GetCurSel(hTabWnd);

	if (nPage != -1)
	{
		hItem = GetSheetPageTreeItem(nPage);

		if (hItem)
			(void)TreeView_SelectItem(hSheetTreeCtrl, hItem);
	}

	g_nFirstInitPropertySheet = 0;
}

/* Handle the information property page */
INT_PTR CALLBACK GamePropertiesDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
		case WM_INITDIALOG:
		{
			char tmp[64];
			int index = lParam;
			CenterWindow(hDlg);
			hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAMEUI_ICON));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			hBrushDlg = CreateSolidBrush(RGB(240, 240, 240));
			snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "Information for \"%s\"", GetDriverGameName(index));
			win_set_window_text_utf8(hDlg, tmp);
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_PROP_TITLE), GetDriverGameTitle(index));
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_PROP_MANUFACTURED), GameInfoManufactured(index));
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_PROP_STATUS), GameInfoStatus(index));
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_PROP_CPU), GameInfoCPU(index));
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_PROP_SOUND), GameInfoSound(index));
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_PROP_SCREEN), GameInfoScreen(index));
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_PROP_CLONEOF), GameInfoCloneOf(index));
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_PROP_SOURCE), GetDriverFileName(index));
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_PROP_SAVESTATE), GameInfoSaveState(index));

			if (DriverIsClone(index))
				ShowWindow(GetDlgItem(hDlg, IDC_PROP_CLONEOF_TEXT), SW_SHOW);
			else
				ShowWindow(GetDlgItem(hDlg, IDC_PROP_CLONEOF_TEXT), SW_HIDE);

			ShowWindow(hDlg, SW_SHOW);
			return true;
		}

		case WM_CTLCOLORDLG:
			return (LRESULT) hBrushDlg;

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));

			if ((HWND)lParam == GetDlgItem(hDlg, IDC_PROP_STATUS))
			{
				if (status_color == 0)
					SetTextColor(hDC, RGB(34, 177, 76));
				else if (status_color == 1)
					SetTextColor(hDC, RGB(237, 28, 36));
				else
					SetTextColor(hDC, RGB(198, 188, 0));
			}

			if (((HWND)lParam == GetDlgItem(hDlg, IDC_PROP_TITLE)) ||
				((HWND)lParam == GetDlgItem(hDlg, IDC_PROP_MANUFACTURED)) ||
				((HWND)lParam == GetDlgItem(hDlg, IDC_PROP_CPU)) ||
				((HWND)lParam == GetDlgItem(hDlg, IDC_PROP_SOUND)) ||
				((HWND)lParam == GetDlgItem(hDlg, IDC_PROP_SCREEN)) ||
				((HWND)lParam == GetDlgItem(hDlg, IDC_PROP_CLONEOF)) ||
				((HWND)lParam == GetDlgItem(hDlg, IDC_PROP_SOURCE)) ||
				((HWND)lParam == GetDlgItem(hDlg, IDC_PROP_SAVESTATE)))
					SetTextColor(hDC, RGB(63, 72, 204));

			return (LRESULT) hBrushDlg;

		case WM_COMMAND:
			switch (GET_WM_COMMAND_ID(wParam, lParam))
			{
				case IDOK:
				case IDCANCEL:
					DestroyIcon(hIcon);
					DeleteObject(hBrushDlg);
					EndDialog(hDlg, 0);
					return true;
			}

			break;
	}

    return false;
}

/* Handle all options property pages */
static INT_PTR CALLBACK GameOptionsDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
		case WM_INITDIALOG:
			//mamefx: for coloring of changed elements
			DisableVisualStyles(hDlg);
			ModifyPropertySheetForTreeSheet(hDlg);
			hBrushDlg = CreateSolidBrush(RGB(255, 255, 255));
			/* Fill in the Game info at the top of the sheet */
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_PROP_TITLE), GameInfoTitle((OPTIONS_TYPE)g_nPropertyMode, g_nGame));
			InitializeOptions(hDlg);
			UpdateProperties(hDlg, properties_datamap, pCurrentOpts);
			g_bUseDefaults = (pCurrentOpts == pDefaultOpts);
			g_bReset = (pCurrentOpts == pOrigOpts) ? false : true;

			if (g_nGame == GLOBAL_OPTIONS)
				ShowWindow(GetDlgItem(hDlg, IDC_USE_DEFAULT), SW_HIDE);
			else
				EnableWindow(GetDlgItem(hDlg, IDC_USE_DEFAULT), (g_bUseDefaults) ? false : true);

			EnableWindow(GetDlgItem(hDlg, IDC_PROP_RESET), g_bReset);
			ShowWindow(hDlg, SW_SHOW);
			return true;

		case WM_HSCROLL:
			/* slider changed */
			HANDLE_WM_HSCROLL(hDlg, wParam, lParam, OptOnHScroll);
			EnableWindow(GetDlgItem(hDlg, IDC_USE_DEFAULT), true);
			PropSheet_Changed(GetParent(hDlg), hDlg);
			// make sure everything's copied over, to determine what's changed
			UpdateOptions(hDlg, properties_datamap, pCurrentOpts);
			// redraw it, it might be a new color now
			InvalidateRect((HWND)lParam, NULL, true);
			break;

		case WM_COMMAND:
		{
			/* Below, 'changed' is used to signify the 'Apply' button should be enabled. */
			WORD wID = GET_WM_COMMAND_ID(wParam, lParam);
			HWND hWndCtrl = GET_WM_COMMAND_HWND(wParam, lParam);
			WORD wNotifyCode = GET_WM_COMMAND_CMD(wParam, lParam);
			bool changed = false;
			bool nCurSelection = false;
			TCHAR szClass[256];

			switch (wID)
			{
				case IDC_REFRESH:
					if (wNotifyCode == LBN_SELCHANGE)
					{
						RefreshSelectionChange(hDlg, hWndCtrl);
						changed = true;
					}
					break;

				case IDC_AUTOFRAMESKIP:
					if (Button_GetCheck(GetDlgItem(hDlg, IDC_AUTOFRAMESKIP)))
						EnableWindow(GetDlgItem(hDlg, IDC_FRAMESKIP), false);
					else
						EnableWindow(GetDlgItem(hDlg, IDC_FRAMESKIP), true);

					changed = true;
					break;

				case IDC_ASPECT:
					nCurSelection = Button_GetCheck(GetDlgItem(hDlg, IDC_ASPECT));

					if(g_bAutoAspect[GetSelectedScreen(hDlg)] != nCurSelection)
					{
						changed = true;
						g_bAutoAspect[GetSelectedScreen(hDlg)] = nCurSelection;
					}

					EnableWindow(GetDlgItem(hDlg, IDC_ASPECTRATIOTEXT), !g_bAutoAspect[GetSelectedScreen(hDlg)]);
					EnableWindow(GetDlgItem(hDlg, IDC_ASPECTRATION), !g_bAutoAspect[GetSelectedScreen(hDlg)]);
					EnableWindow(GetDlgItem(hDlg, IDC_ASPECTRATIOD), !g_bAutoAspect[GetSelectedScreen(hDlg)]);
					EnableWindow(GetDlgItem(hDlg, IDC_ASPECTRATIOP), !g_bAutoAspect[GetSelectedScreen(hDlg)]);
					break;

				case IDC_SNAPSIZE:
					nCurSelection = Button_GetCheck(GetDlgItem(hDlg, IDC_SNAPSIZE));

					if(g_bAutoSnapSize != nCurSelection)
					{
						changed = true;
						g_bAutoSnapSize = nCurSelection;
					}

					EnableWindow(GetDlgItem(hDlg, IDC_SNAPSIZETEXT), !g_bAutoSnapSize);
					EnableWindow(GetDlgItem(hDlg, IDC_SNAPSIZEHEIGHT), !g_bAutoSnapSize);
					EnableWindow(GetDlgItem(hDlg, IDC_SNAPSIZEWIDTH), !g_bAutoSnapSize);
					EnableWindow(GetDlgItem(hDlg, IDC_SNAPSIZEX), !g_bAutoSnapSize);
					break;

				case IDC_SELECT_EFFECT:
					changed = SelectEffect(hDlg);
					break;

				case IDC_RESET_EFFECT:
					changed = ResetEffect(hDlg);
					break;

				case IDC_SELECT_SHADER0:
				case IDC_SELECT_SHADER1:
				case IDC_SELECT_SHADER2:
				case IDC_SELECT_SHADER3:
				case IDC_SELECT_SHADER4:
					changed = SelectMameShader(hDlg, (wID - IDC_SELECT_SHADER0));
					break;

				case IDC_RESET_SHADER0:
				case IDC_RESET_SHADER1:
				case IDC_RESET_SHADER2:
				case IDC_RESET_SHADER3:
				case IDC_RESET_SHADER4:
					changed = ResetMameShader(hDlg, (wID - IDC_RESET_SHADER0));
					break;

				case IDC_SELECT_SCR_SHADER0:
				case IDC_SELECT_SCR_SHADER1:
				case IDC_SELECT_SCR_SHADER2:
				case IDC_SELECT_SCR_SHADER3:
				case IDC_SELECT_SCR_SHADER4:
					changed = SelectScreenShader(hDlg, (wID - IDC_SELECT_SCR_SHADER0));
					break;

				case IDC_RESET_SCR_SHADER0:
				case IDC_RESET_SCR_SHADER1:
				case IDC_RESET_SCR_SHADER2:
				case IDC_RESET_SCR_SHADER3:
				case IDC_RESET_SCR_SHADER4:
					changed = ResetScreenShader(hDlg, (wID - IDC_RESET_SCR_SHADER0));
					break;

				case IDC_SELECT_CHEATFILE:
					changed = SelectCheatFile(hDlg);
					break;

				case IDC_RESET_CHEATFILE:
					changed = ResetCheatFile(hDlg);
					break;

				case IDC_JOYSTICKMAP:
					changed = ChangeJoystickMap(hDlg);
					break;

				case IDC_RESET_JOYSTICKMAP:
					changed = ResetJoystickMap(hDlg);
					break;

				case IDC_SELECT_LUASCRIPT:
					changed = SelectLUAScript(hDlg);
					break;

				case IDC_RESET_LUASCRIPT:
					changed = ResetLUAScript(hDlg);
					break;

				case IDC_SELECT_PLUGIN:
					changed = SelectPlugins(hDlg);
					break;

				case IDC_RESET_PLUGIN:
					changed = ResetPlugins(hDlg);
					break;

				case IDC_SELECT_BGFX:
					changed = SelectBGFXChains(hDlg);
					break;

				case IDC_RESET_BGFX:
					changed = ResetBGFXChains(hDlg);
					break;

				case IDC_PROP_RESET:
					if (wNotifyCode != BN_CLICKED)
					break;

					pCurrentOpts = pOrigOpts;
					UpdateProperties(hDlg, properties_datamap, pCurrentOpts);
					g_bUseDefaults = (pCurrentOpts == pDefaultOpts);
					g_bReset = false;
					PropSheet_UnChanged(GetParent(hDlg), hDlg);
					EnableWindow(GetDlgItem(hDlg, IDC_USE_DEFAULT), (g_bUseDefaults) ? false : true);
					break;

				case IDC_USE_DEFAULT:
					pCurrentOpts = pDefaultOpts;
					// repopulate the controls with the new data
					UpdateProperties(hDlg, properties_datamap, pCurrentOpts);
					g_bUseDefaults = (pCurrentOpts == pDefaultOpts);
					// This evaluates properly
					g_bReset = (pCurrentOpts == pOrigOpts) ? false : true;
					// Enable/Dispable the Reset to Defaults button
					EnableWindow(GetDlgItem(hDlg, IDC_USE_DEFAULT), (g_bUseDefaults) ? false : true);

					// Tell the dialog to enable/disable the apply button.
					if (g_nGame != GLOBAL_OPTIONS)
					{
						if (g_bReset)
							PropSheet_Changed(GetParent(hDlg), hDlg);
						else
							PropSheet_UnChanged(GetParent(hDlg), hDlg);
					}

					break;

				// MSH 20070813 - Update all related controls
				case IDC_SCREENSELECT:
				case IDC_SCREEN:
					// NPW 3-Apr-2007:  Ugh I'm only perpetuating the vile hacks in this code
					if ((wNotifyCode == CBN_SELCHANGE) || (wNotifyCode == CBN_SELENDOK))
					{
						datamap_read_control(properties_datamap, hDlg, pCurrentOpts, wID);
						datamap_populate_control(properties_datamap, hDlg, pCurrentOpts, IDC_SIZES);
						//MSH 20070814 - Hate to do this, but its either this, or update each individual
						// control on the SCREEN tab.
						UpdateProperties(hDlg, properties_datamap, pCurrentOpts);
						changed = true;
					}

					break;

				default:
					// use default behavior; try to get the result out of the datamap if
					// appropriate
					GetClassName(hWndCtrl, szClass, WINUI_ARRAY_LENGTH(szClass));

					if (!_tcscmp(szClass, WC_COMBOBOX))
					{
						// combo box
						if ((wNotifyCode == CBN_SELCHANGE) || (wNotifyCode == CBN_SELENDOK))
							changed = datamap_read_control(properties_datamap, hDlg, pCurrentOpts, wID);
					}
						else if (!_tcscmp(szClass, WC_BUTTON) && (GetWindowLong(hWndCtrl, GWL_STYLE) & BS_CHECKBOX))
						// check box
							changed = datamap_read_control(properties_datamap, hDlg, pCurrentOpts, wID);

					break;
			}

			if (changed == true)
			{
				// make sure everything's copied over, to determine what's changed
				UpdateOptions(hDlg, properties_datamap, pCurrentOpts);
				// enable the apply button
				PropSheet_Changed(GetParent(hDlg), hDlg);
				g_bUseDefaults = (pCurrentOpts == pDefaultOpts);
				g_bReset = (pCurrentOpts == pOrigOpts) ? false : true;
				EnableWindow(GetDlgItem(hDlg, IDC_USE_DEFAULT), (g_bUseDefaults) ? false : true);
			}

			break;
		}

		case WM_NOTIFY:
		{
			switch (((NMHDR*)lParam)->code)
			{
				//We'll need to use a CheckState Table
				//Because this one gets called for all kinds of other things too, and not only if a check is set
				case PSN_SETACTIVE:
					/* Initialize the controls. */
					UpdateProperties(hDlg, properties_datamap, pCurrentOpts);
					g_bUseDefaults = (pCurrentOpts == pDefaultOpts);
					g_bReset = (pCurrentOpts == pOrigOpts) ? false : true;
					// Sync RESET TO DEFAULTS buttons.
					EnableWindow(GetDlgItem(hDlg, IDC_USE_DEFAULT), (g_bUseDefaults) ? false : true);
					break;

				case PSN_APPLY:
					// Read the datamap
					UpdateOptions(hDlg, properties_datamap, pCurrentOpts);
					pOrigOpts = pCurrentOpts;
					// Repopulate the controls?  WTF?  We just read them, they should be fine.
					UpdateProperties(hDlg, properties_datamap, pCurrentOpts);
					// Determine button states.
					g_bUseDefaults = (pCurrentOpts == pDefaultOpts);
					g_bReset = false;
					// Sync RESET and RESET TO DEFAULTS buttons.
					EnableWindow(GetDlgItem(hDlg, IDC_USE_DEFAULT), (g_bUseDefaults) ? false : true);
					EnableWindow(GetDlgItem(hDlg, IDC_PROP_RESET), g_bReset);
					// Save or remove the current options
					SaveOptions((OPTIONS_TYPE)g_nPropertyMode, pCurrentOpts, g_nGame);
					// Disable apply button
					PropSheet_UnChanged(GetParent(hDlg), hDlg);
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
					return true;

				case PSN_KILLACTIVE:
					/* Save Changes to the options here. */
					UpdateOptions(hDlg, properties_datamap, pCurrentOpts);
					// Determine button states.
					g_bUseDefaults = (pCurrentOpts == pDefaultOpts);
					ResetDataMap(hDlg);
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT, false);
					return true;

				case PSN_RESET:
					// Reset to the original values. Disregard changes
					pCurrentOpts = pOrigOpts;
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT, false);
					break;
			}

			break;
		}

		//mamefx: for coloring of changed elements
		case WM_CTLCOLORDLG:
			return (LRESULT) hBrushDlg;

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
		case WM_CTLCOLORLISTBOX:
		case WM_CTLCOLOREDIT:
		{
			HDC hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));

			//Set the Coloring of the elements
			if (GetWindowLongPtr((HWND)lParam, GWL_ID) < 0)
				return (LRESULT) hBrushDlg;

			if (g_nPropertyMode == OPTIONS_GLOBAL)
				SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
			else if (IsControlOptionValue(hDlg,(HWND)lParam, pCurrentOpts, pOptsGlobal))
				SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
			else if (IsControlOptionValue(hDlg,(HWND)lParam, pCurrentOpts, pOptsHorizontal) && !DriverIsVertical(g_nGame))
				SetTextColor(hDC, RGB(163, 73, 164));	// purple
			else if (IsControlOptionValue(hDlg,(HWND)lParam, pCurrentOpts, pOptsVertical) && DriverIsVertical(g_nGame))
				SetTextColor(hDC, RGB(63, 72, 204));	// blue
			else if (IsControlOptionValue(hDlg,(HWND)lParam, pCurrentOpts, pOptsRaster) && !DriverIsVector(g_nGame))
				SetTextColor(hDC, RGB(136, 0, 21));		// dark red
			else if (IsControlOptionValue(hDlg,(HWND)lParam, pCurrentOpts, pOptsVector) && DriverIsVector(g_nGame))
				SetTextColor(hDC, RGB(255, 127, 39));	// orange
			else if (IsControlOptionValue(hDlg,(HWND)lParam, pCurrentOpts, pOptsSource))
				SetTextColor(hDC, RGB(237, 28, 36));	// red
			else if (IsControlOptionValue(hDlg,(HWND)lParam, pCurrentOpts, pDefaultOpts))
				SetTextColor(hDC, RGB(34, 177, 76));	// green
			else
			{
				switch (g_nPropertyMode)
				{
					case OPTIONS_GAME:
						SetTextColor(hDC, RGB(34, 177, 76));
						break;

					case OPTIONS_SOURCE:
						SetTextColor(hDC, RGB(237, 28, 36));
						break;

					case OPTIONS_RASTER:
						SetTextColor(hDC, RGB(136, 0, 21));
						break;

					case OPTIONS_VECTOR:
						SetTextColor(hDC, RGB(255, 127, 39));
						break;

					case OPTIONS_HORIZONTAL:
						SetTextColor(hDC, RGB(163, 73, 164));
						break;

					case OPTIONS_VERTICAL:
						SetTextColor(hDC, RGB(63, 72, 204));
						break;

					case OPTIONS_GLOBAL:
					default:
						SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
						break;
				}
			}

			if (Msg == WM_CTLCOLORLISTBOX || Msg == WM_CTLCOLOREDIT)
				return (LRESULT) GetStockObject(WHITE_BRUSH);
			else
				return (LRESULT) hBrushDlg;
		}
	}

	EnableWindow(GetDlgItem(hDlg, IDC_PROP_RESET), g_bReset);
	return false;
}

/* Read controls that are not handled in the DataMap */
static void PropToOptions(HWND hWnd, windows_options &opts)
{
	HWND hCtrl = NULL;
	HWND hCtrl2 = NULL;
	HWND hCtrl3 = NULL;
	std::string error_string;
	TCHAR buffer[200];
	char buffer2[200];

	/* aspect ratio */
	hCtrl  = GetDlgItem(hWnd, IDC_ASPECTRATION);
	hCtrl2 = GetDlgItem(hWnd, IDC_ASPECTRATIOD);
	hCtrl3 = GetDlgItem(hWnd, IDC_ASPECT);

	if (hCtrl && hCtrl2 && hCtrl3)
	{
		int n = 0;
		int d = 0;
		char aspect_option[32];

		snprintf(aspect_option, WINUI_ARRAY_LENGTH(aspect_option), "aspect%d", GetSelectedScreen(hWnd));

		if (Button_GetCheck(hCtrl3))
			opts.set_value(aspect_option, "auto", OPTION_PRIORITY_CMDLINE, error_string);
		else
		{
			Edit_GetText(hCtrl, buffer, WINUI_ARRAY_LENGTH(buffer));
			_stscanf(buffer, TEXT("%d"), &n);
			Edit_GetText(hCtrl2, buffer, WINUI_ARRAY_LENGTH(buffer));
			_stscanf(buffer, TEXT("%d"), &d);

			if (n == 0 || d == 0)
			{
				n = 4;
				d = 3;
			}

			snprintf(buffer2, WINUI_ARRAY_LENGTH(buffer2), "%d:%d", n, d);
			opts.set_value(aspect_option, buffer2, OPTION_PRIORITY_CMDLINE, error_string);
		}
	}

	/* snapshot size */
	hCtrl  = GetDlgItem(hWnd, IDC_SNAPSIZEWIDTH);
	hCtrl2 = GetDlgItem(hWnd, IDC_SNAPSIZEHEIGHT);
	hCtrl3 = GetDlgItem(hWnd, IDC_SNAPSIZE);

	if (hCtrl && hCtrl2 && hCtrl3)
	{
		int width = 0;
		int height = 0;

		if (Button_GetCheck(hCtrl3))
			opts.set_value(OPTION_SNAPSIZE, "auto", OPTION_PRIORITY_CMDLINE, error_string);
		else
		{
			Edit_GetText(hCtrl, buffer, WINUI_ARRAY_LENGTH(buffer));
			_stscanf(buffer, TEXT("%d"), &width);
			Edit_GetText(hCtrl2, buffer, WINUI_ARRAY_LENGTH(buffer));
			_stscanf(buffer, TEXT("%d"), &height);

			if (width == 0 || height == 0)
			{
				width = 640;
				height = 480;
			}

			snprintf(buffer2, WINUI_ARRAY_LENGTH(buffer2), "%dx%d", width, height);
			opts.set_value(OPTION_SNAPSIZE, buffer2, OPTION_PRIORITY_CMDLINE, error_string);
		}
	}
}

/* Update options from the dialog */
static void UpdateOptions(HWND hDlg, datamap *map, windows_options &opts)
{
	/* These are always called together, so make one convenience function. */
	datamap_read_all_controls(map, hDlg, opts);
	PropToOptions(hDlg, opts);
}

/* Update the dialog from the options */
static void UpdateProperties(HWND hDlg, datamap *map, windows_options &opts)
{
	/* set ticks frequency in variours sliders */
	SendMessage(GetDlgItem(hDlg, IDC_GAMMA), 		TBM_SETTICFREQ, 10, 0);
	SendMessage(GetDlgItem(hDlg, IDC_CONTRAST), 	TBM_SETTICFREQ, 5, 0);
	SendMessage(GetDlgItem(hDlg, IDC_BRIGHTCORRECT),TBM_SETTICFREQ, 20, 0);
	SendMessage(GetDlgItem(hDlg, IDC_PAUSEBRIGHT), 	TBM_SETTICFREQ, 20, 0);
	SendMessage(GetDlgItem(hDlg, IDC_FSGAMMA),		TBM_SETTICFREQ, 10, 0);
	SendMessage(GetDlgItem(hDlg, IDC_FSCONTRAST), 	TBM_SETTICFREQ, 5, 0);
	SendMessage(GetDlgItem(hDlg, IDC_FSBRIGHTNESS), TBM_SETTICFREQ, 20, 0);
	SendMessage(GetDlgItem(hDlg, IDC_SECONDSTORUN), TBM_SETTICFREQ, 10, 0);
	SendMessage(GetDlgItem(hDlg, IDC_JDZ), 			TBM_SETTICFREQ, 20, 0);
	SendMessage(GetDlgItem(hDlg, IDC_JSAT), 		TBM_SETTICFREQ, 20, 0);
	SendMessage(GetDlgItem(hDlg, IDC_VOLUME), 		TBM_SETTICFREQ, 4, 0);
	SendMessage(GetDlgItem(hDlg, IDC_HIGH_PRIORITY),TBM_SETTICFREQ, 2, 0);
	SendMessage(GetDlgItem(hDlg, IDC_SPEED), 		TBM_SETTICFREQ, 50, 0);
	SendMessage(GetDlgItem(hDlg, IDC_BEAM_MIN), 	TBM_SETTICFREQ, 5, 0);
	SendMessage(GetDlgItem(hDlg, IDC_BEAM_MAX), 	TBM_SETTICFREQ, 50, 0);
	SendMessage(GetDlgItem(hDlg, IDC_BEAM_INTEN), 	TBM_SETTICFREQ, 100, 0);
	SendMessage(GetDlgItem(hDlg, IDC_FLICKER), 		TBM_SETTICFREQ, 5, 0);
	/* These are always called together, so make one convenience function. */
	datamap_populate_all_controls(map, hDlg, opts);
	OptionsToProp(hDlg, opts);
	SetPropEnabledControls(hDlg);
}

/* Populate controls that are not handled in the DataMap */
static void OptionsToProp(HWND hWnd, windows_options &opts)
{
	HWND hCtrl = NULL;
	HWND hCtrl2 = NULL;
	TCHAR buf[100];
	char aspect_option[32];
	char buffer[MAX_PATH];

	/* Setup refresh list based on depth. */
	datamap_update_control(properties_datamap, hWnd, pCurrentOpts, IDC_REFRESH);
	/* Setup Select screen*/
	UpdateSelectScreenUI(hWnd );

	hCtrl = GetDlgItem(hWnd, IDC_ASPECT);

	if (hCtrl)
		Button_SetCheck(hCtrl, g_bAutoAspect[GetSelectedScreen(hWnd)]);

	hCtrl = GetDlgItem(hWnd, IDC_SNAPSIZE);

	if (hCtrl)
		Button_SetCheck(hCtrl, g_bAutoSnapSize);

	/* Bios select list */
	hCtrl = GetDlgItem(hWnd, IDC_BIOS);

	if (hCtrl)
	{
		int iCount = ComboBox_GetCount(hCtrl);

		for (int i = 0; i < iCount; i++)
		{
			const char *cBuffer = (const char*)ComboBox_GetItemData(hCtrl, i);

			if (strcmp(cBuffer, pCurrentOpts.value(OPTION_BIOS)) == 0)
			{
				(void)ComboBox_SetCurSel(hCtrl, i);
				break;
			}
		}
	}

	hCtrl = GetDlgItem(hWnd, IDC_ASPECT);

	if (hCtrl)
	{
		snprintf(aspect_option, WINUI_ARRAY_LENGTH(aspect_option), "aspect%d", GetSelectedScreen(hWnd));

		if( strcmp(opts.value(aspect_option), "auto") == 0)
		{
			Button_SetCheck(hCtrl, true);
			g_bAutoAspect[GetSelectedScreen(hWnd)] = true;
		}
		else
		{
			Button_SetCheck(hCtrl, false);
			g_bAutoAspect[GetSelectedScreen(hWnd)] = false;
		}
	}

	/* aspect ratio */
	hCtrl  = GetDlgItem(hWnd, IDC_ASPECTRATION);
	hCtrl2 = GetDlgItem(hWnd, IDC_ASPECTRATIOD);

	if (hCtrl && hCtrl2)
	{
		int n = 0;
		int d = 0;

		snprintf(aspect_option, WINUI_ARRAY_LENGTH(aspect_option), "aspect%d", GetSelectedScreen(hWnd));

		if (sscanf(opts.value(aspect_option), "%d:%d", &n, &d) == 2 && n != 0 && d != 0)
		{
			_sntprintf(buf, WINUI_ARRAY_LENGTH(buf), TEXT("%d"), n);
			Edit_SetText(hCtrl, buf);
			_sntprintf(buf, WINUI_ARRAY_LENGTH(buf), TEXT("%d"), d);
			Edit_SetText(hCtrl2, buf);
		}
		else
		{
			Edit_SetText(hCtrl,  TEXT("4"));
			Edit_SetText(hCtrl2, TEXT("3"));
		}
	}

	hCtrl = GetDlgItem(hWnd, IDC_EFFECT);

	if (hCtrl) 
	{
		const char* effect = opts.value(OPTION_EFFECT);

		if (strcmp(effect, "none") == 0)
			win_set_window_text_utf8(hCtrl, "None");
		else
			win_set_window_text_utf8(hCtrl, effect);
	}

	for (int i = 0; i < 5; i++)
	{
		UpdateMameShader(hWnd, i, opts);
		UpdateScreenShader(hWnd, i, opts);
	}

	hCtrl = GetDlgItem(hWnd, IDC_CHEATFILE);

	if (hCtrl) 
	{
		const char* cheatfile = opts.value(OPTION_CHEATPATH);

		if (strcmp(cheatfile, "cheat") == 0)
			win_set_window_text_utf8(hCtrl, "Default");
		else
		{
			char *cheatname = strrchr(cheatfile, '\\');

			if (cheatname != NULL)
			{
				strcpy(buffer, cheatname + 1);
				win_set_window_text_utf8(hCtrl, buffer);
			}
			else
				win_set_window_text_utf8(hCtrl, cheatfile);
		}
	}

	hCtrl = GetDlgItem(hWnd, IDC_JOYSTICKMAP);

	if (hCtrl) 
	{
		const char* joymap = opts.value(OPTION_JOYSTICK_MAP);

		win_set_window_text_utf8(hCtrl, joymap);
	}

	hCtrl = GetDlgItem(hWnd, IDC_LUASCRIPT);

	if (hCtrl) 
	{
		const char* script = opts.value(OPTION_AUTOBOOT_SCRIPT);

		if (strcmp(script, "") == 0)
			win_set_window_text_utf8(hCtrl, "None");
		else
		{
			TCHAR *t_filename = win_wstring_from_utf8(script);
			TCHAR *tempname = PathFindFileName(t_filename);
			PathRemoveExtension(tempname);
			char *optname = win_utf8_from_wstring(tempname);
			strcpy(buffer, optname);
			free(t_filename);
			free(optname);
			win_set_window_text_utf8(hCtrl, buffer);
		}
	}

	hCtrl = GetDlgItem(hWnd, IDC_PLUGIN);

	if (hCtrl) 
	{
		const char* plugin = opts.value(OPTION_PLUGIN);

		if (strcmp(plugin, "") == 0)
			win_set_window_text_utf8(hCtrl, "None");
		else
			win_set_window_text_utf8(hCtrl, plugin);
	}

	hCtrl = GetDlgItem(hWnd, IDC_BGFX_CHAINS);

	if (hCtrl) 
	{
		const char* chains = opts.value(OSDOPTION_BGFX_SCREEN_CHAINS);

		if (strcmp(chains, "default") == 0)
			win_set_window_text_utf8(hCtrl, "Default");
		else
			win_set_window_text_utf8(hCtrl, chains);
	}

	hCtrl = GetDlgItem(hWnd, IDC_SNAPSIZE);

	if (hCtrl)
	{
		if(strcmp(opts.value(OPTION_SNAPSIZE), "auto") == 0)
		{
			Button_SetCheck(hCtrl, true);
			g_bAutoSnapSize = true;
		}
		else
		{
			Button_SetCheck(hCtrl, false);
			g_bAutoSnapSize = false;
		}
	}

	/* snapshot size */
	hCtrl  = GetDlgItem(hWnd, IDC_SNAPSIZEWIDTH);
	hCtrl2 = GetDlgItem(hWnd, IDC_SNAPSIZEHEIGHT);

	if (hCtrl && hCtrl2)
	{
		int width = 0;
		int height = 0;

		if (sscanf(opts.value(OPTION_SNAPSIZE), "%dx%d", &width, &height) == 2 && width != 0 && height != 0)
		{
			_sntprintf(buf, WINUI_ARRAY_LENGTH(buf), TEXT("%d"), width);
			Edit_SetText(hCtrl, buf);
			_sntprintf(buf, WINUI_ARRAY_LENGTH(buf), TEXT("%d"), height);
			Edit_SetText(hCtrl2, buf);
		}
		else
		{
			Edit_SetText(hCtrl,  TEXT("640"));
			Edit_SetText(hCtrl2, TEXT("480"));
		}
	}
}

/* Adjust controls - tune them to the currently selected game */
static void SetPropEnabledControls(HWND hWnd)
{
	int nIndex = g_nGame;
	bool d3d = (!core_stricmp(pCurrentOpts.value(OSDOPTION_VIDEO), "d3d") || !core_stricmp(pCurrentOpts.value(OSDOPTION_VIDEO), "auto"));
	bool opengl = !core_stricmp(pCurrentOpts.value(OSDOPTION_VIDEO), "opengl");
	bool bgfx = !core_stricmp(pCurrentOpts.value(OSDOPTION_VIDEO), "bgfx");
	bool in_window = pCurrentOpts.bool_value(OSDOPTION_WINDOW);

	/* Video options */
	EnableWindow(GetDlgItem(hWnd, IDC_REFRESH),         !in_window);
	EnableWindow(GetDlgItem(hWnd, IDC_SIZES),         	!in_window);
	EnableWindow(GetDlgItem(hWnd, IDC_FSGAMMA),      	!in_window);
	EnableWindow(GetDlgItem(hWnd, IDC_FSGAMMATEXT),  	!in_window);
	EnableWindow(GetDlgItem(hWnd, IDC_FSGAMMADISP),  	!in_window);
	EnableWindow(GetDlgItem(hWnd, IDC_FSBRIGHTNESS),    !in_window);
	EnableWindow(GetDlgItem(hWnd, IDC_FSBRIGHTNESSTEXT),!in_window);
	EnableWindow(GetDlgItem(hWnd, IDC_FSBRIGHTNESSDISP),!in_window);
	EnableWindow(GetDlgItem(hWnd, IDC_FSCONTRAST),      !in_window);
	EnableWindow(GetDlgItem(hWnd, IDC_FSCONTRASTTEXT),  !in_window);
	EnableWindow(GetDlgItem(hWnd, IDC_FSCONTRASTDISP),  !in_window);
	EnableWindow(GetDlgItem(hWnd, IDC_ASPECTRATIOTEXT), !g_bAutoAspect[GetSelectedScreen(hWnd)]);
	EnableWindow(GetDlgItem(hWnd, IDC_ASPECTRATION), 	!g_bAutoAspect[GetSelectedScreen(hWnd)]);
	EnableWindow(GetDlgItem(hWnd, IDC_ASPECTRATIOD), 	!g_bAutoAspect[GetSelectedScreen(hWnd)]);
	EnableWindow(GetDlgItem(hWnd, IDC_ASPECTRATIOP), 	!g_bAutoAspect[GetSelectedScreen(hWnd)]);
	EnableWindow(GetDlgItem(hWnd, IDC_HLSL_ON), 		d3d);
	EnableWindow(GetDlgItem(hWnd, IDC_GLSL), 			opengl);	
	EnableWindow(GetDlgItem(hWnd, IDC_GLSLFILTER), 		opengl);	
	EnableWindow(GetDlgItem(hWnd, IDC_GLSLPOW), 		opengl);	
	EnableWindow(GetDlgItem(hWnd, IDC_GLSLTEXTURE), 	opengl);	
	EnableWindow(GetDlgItem(hWnd, IDC_GLSLVBO), 		opengl);	
	EnableWindow(GetDlgItem(hWnd, IDC_GLSLPBO), 		opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_GLSLSYNC), 		opengl);	
	EnableWindow(GetDlgItem(hWnd, IDC_SELECT_SHADER0), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_SELECT_SHADER1), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_SELECT_SHADER2), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_SELECT_SHADER3), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_SELECT_SHADER4), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_SELECT_SCR_SHADER0), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_SELECT_SCR_SHADER1), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_SELECT_SCR_SHADER2), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_SELECT_SCR_SHADER3), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_SELECT_SCR_SHADER4), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_RESET_SHADER0), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_RESET_SHADER1), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_RESET_SHADER2), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_RESET_SHADER3), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_RESET_SHADER4), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_RESET_SCR_SHADER0), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_RESET_SCR_SHADER1), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_RESET_SCR_SHADER2), 	opengl);
	EnableWindow(GetDlgItem(hWnd, IDC_RESET_SCR_SHADER3), 	opengl);	
	EnableWindow(GetDlgItem(hWnd, IDC_RESET_SCR_SHADER4), 	opengl);	
	EnableWindow(GetDlgItem(hWnd, IDC_SELECT_BGFX), 	bgfx);	
	EnableWindow(GetDlgItem(hWnd, IDC_RESET_BGFX), 		bgfx);	
	/* Snapshot options */
	EnableWindow(GetDlgItem(hWnd, IDC_SNAPSIZETEXT), 	!g_bAutoSnapSize);
	EnableWindow(GetDlgItem(hWnd, IDC_SNAPSIZEHEIGHT), 	!g_bAutoSnapSize);
	EnableWindow(GetDlgItem(hWnd, IDC_SNAPSIZEWIDTH), 	!g_bAutoSnapSize);
	EnableWindow(GetDlgItem(hWnd, IDC_SNAPSIZEX), 		!g_bAutoSnapSize);
	/* Misc options */
	if (Button_GetCheck(GetDlgItem(hWnd, IDC_AUTOFRAMESKIP)))
		EnableWindow(GetDlgItem(hWnd, IDC_FRAMESKIP), 	false);
	else
		EnableWindow(GetDlgItem(hWnd, IDC_FRAMESKIP), 	true);

	if (nIndex <= -1 || DriverHasOptionalBIOS(nIndex))
		EnableWindow(GetDlgItem(hWnd, IDC_BIOS),		true);
	else
		EnableWindow(GetDlgItem(hWnd, IDC_BIOS),		false);

	if (nIndex <= -1 || DriverSupportsSaveState(nIndex))
		EnableWindow(GetDlgItem(hWnd, IDC_ENABLE_AUTOSAVE), true);
	else
		EnableWindow(GetDlgItem(hWnd, IDC_ENABLE_AUTOSAVE), false);
}

//============================================================
//  CONTROL HELPER FUNCTIONS FOR DATA EXCHANGE
//============================================================

static bool RotateReadControl(datamap *map, HWND dialog, HWND control, windows_options &opts, const char *option_name)
{
	int selected_index = ComboBox_GetCurSel(control);
	int original_selection = 0;

	// Figure out what the original selection value is
	if (opts.bool_value(OPTION_ROR) && !opts.bool_value(OPTION_ROL))
		original_selection = 1;
	else if (!opts.bool_value(OPTION_ROR) && opts.bool_value(OPTION_ROL))
		original_selection = 2;
	else if (!opts.bool_value(OPTION_ROTATE))
		original_selection = 3;
	else if (opts.bool_value(OPTION_AUTOROR))
		original_selection = 4;
	else if (opts.bool_value(OPTION_AUTOROL))
		original_selection = 5;

	// Any work to do?  If so, make the changes and return true.
	if (selected_index != original_selection)
	{
		// Set the options based on the new selection.
		std::string error_string;
		opts.set_value(OPTION_ROR,		selected_index == 1, OPTION_PRIORITY_CMDLINE, error_string);
		opts.set_value(OPTION_ROL,		selected_index == 2, OPTION_PRIORITY_CMDLINE, error_string);
		opts.set_value(OPTION_ROTATE,	selected_index != 3, OPTION_PRIORITY_CMDLINE, error_string);
		opts.set_value(OPTION_AUTOROR,	selected_index == 4, OPTION_PRIORITY_CMDLINE, error_string);
		opts.set_value(OPTION_AUTOROL,	selected_index == 5, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
		return true;
	}

	// No changes
	return false;
}

static bool RotatePopulateControl(datamap *map, HWND dialog, HWND control, windows_options &opts, const char *option_name)
{
	int selected_index = 0;

	if (opts.bool_value(OPTION_ROR) && !opts.bool_value(OPTION_ROL))
		selected_index = 1;
	else if (!opts.bool_value(OPTION_ROR) && opts.bool_value(OPTION_ROL))
		selected_index = 2;
	else if (!opts.bool_value(OPTION_ROTATE))
		selected_index = 3;
	else if (opts.bool_value(OPTION_AUTOROR))
		selected_index = 4;
	else if (opts.bool_value(OPTION_AUTOROL))
		selected_index = 5;

	(void)ComboBox_SetCurSel(control, selected_index);
	return false;
}

static bool ScreenReadControl(datamap *map, HWND dialog, HWND control, windows_options &opts, const char *option_name)
{
	char screen_option_name[32];
	std::string error_string;

	int selected_screen = GetSelectedScreen(dialog);
	int screen_option_index = ComboBox_GetCurSel(control);
	const char *screen_option_value = (const char*)ComboBox_GetItemData(control, screen_option_index);
	snprintf(screen_option_name, WINUI_ARRAY_LENGTH(screen_option_name), "screen%d", selected_screen);
	opts.set_value(screen_option_name, screen_option_value, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
	return false;
}

static bool ScreenPopulateControl(datamap *map, HWND dialog, HWND control, windows_options &opts, const char *option_name)
{
	DISPLAY_DEVICE dd;
	int nSelection = 0;

	/* Remove all items in the list. */
	(void)ComboBox_ResetContent(control);
	(void)ComboBox_InsertString(control, 0, TEXT("Auto"));
	(void)ComboBox_SetItemData(control, 0, "auto");

	memset(&dd, 0, sizeof(DISPLAY_DEVICE));
	dd.cb = sizeof(DISPLAY_DEVICE);

	for (int i = 0; EnumDisplayDevices(NULL, i, &dd, 0); i++)
	{
		if (!(dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
		{
			//we have to add 1 to account for the "auto" entry
			char screen_option[32];
			const char *device = win_utf8_from_wstring(dd.DeviceName);
			TCHAR *t_device = win_wstring_from_utf8(device);
			(void)ComboBox_InsertString(control, i + 1, t_device);
			(void)ComboBox_SetItemData(control, i + 1, device);
			snprintf(screen_option, WINUI_ARRAY_LENGTH(screen_option), "screen%d", GetSelectedScreen(dialog));
			const char *option = opts.value(screen_option);

			if (strcmp(option, device) == 0)
				nSelection = i + 1;

			free(t_device);
			device = NULL;
		}
	}

	(void)ComboBox_SetCurSel(control, nSelection);
	return false;
}

static void ViewSetOptionName(datamap *map, HWND dialog, HWND control, char *buffer, size_t buffer_size)
{
	snprintf(buffer, buffer_size, "view%d", GetSelectedScreen(dialog));
}

static bool ViewPopulateControl(datamap *map, HWND dialog, HWND control, windows_options &opts, const char *option_name)
{
	int selected_index = 0;
	char view_option[32];

	// determine the view option value
	snprintf(view_option, WINUI_ARRAY_LENGTH(view_option), "view%d", GetSelectedScreen(dialog));
	const char *view = opts.value(view_option);

	(void)ComboBox_ResetContent(control);

	for (int i = 0; i < NUMVIEW; i++)
	{
		(void)ComboBox_InsertString(control, i, g_ComboBoxView[i].m_pText);
		(void)ComboBox_SetItemData(control, i, g_ComboBoxView[i].m_pData);

		if (!strcmp(view, g_ComboBoxView[i].m_pData))
			selected_index = i;
	}

	(void)ComboBox_SetCurSel(control, selected_index);
	return false;
}

static bool DefaultInputReadControl(datamap *map, HWND dialog, HWND control, windows_options &opts, const char *option_name)
{
	std::string error_string;

	int input_option_index = ComboBox_GetCurSel(control);
	const char *input_option_value = (const char*)ComboBox_GetItemData(control, input_option_index);
	opts.set_value(OPTION_CTRLR, input_option_index ? input_option_value : "", OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
	return false;
}

static bool DefaultInputPopulateControl(datamap *map, HWND dialog, HWND control, windows_options &opts, const char *option_name)
{
	WIN32_FIND_DATA FindFileData;
	char path[MAX_PATH];
	int selected = 0;
	int index = 0;

	// determine the ctrlr option
	const char *ctrlr_option = opts.value(OPTION_CTRLR);

	// reset the controllers dropdown
	(void)ComboBox_ResetContent(control);
	(void)ComboBox_InsertString(control, index, TEXT("Default"));
	(void)ComboBox_SetItemData(control, index, "");
	index++;
	snprintf(path, WINUI_ARRAY_LENGTH(path), "%s\\*.*", GetCtrlrDir());
	HANDLE hFind = win_find_first_file_utf8(path, &FindFileData);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		while (FindNextFile (hFind, &FindFileData) != 0)
		{
			// copy the filename
			const char *root = win_utf8_from_wstring(FindFileData.cFileName);
			// find the extension
			char *ext = strrchr(root, '.');

			if (ext)
			{
				// check if it's a cfg file
				if (strcmp (ext, ".cfg") == 0)
				{
					// and strip off the extension
					*ext = 0;

					// set the option?
					if (!strcmp(root, ctrlr_option))
						selected = index;

					// add it as an option
					TCHAR *t_root = win_wstring_from_utf8(root);
					(void)ComboBox_InsertString(control, index, t_root);
					(void)ComboBox_SetItemData(control, index, root);
					free(t_root);
					root = NULL;
					index++;
				}
			}
		}

		FindClose (hFind);
	}

	(void)ComboBox_SetCurSel(control, selected);

	return false;
}

static void ResolutionSetOptionName(datamap *map, HWND dialog, HWND control, char *buffer, size_t buffer_size)
{
	snprintf(buffer, buffer_size, "resolution%d", GetSelectedScreen(dialog));
}

static bool ResolutionReadControl(datamap *map, HWND dialog, HWND control, windows_options &opts, const char *option_name)
{
	HWND refresh_control = GetDlgItem(dialog, IDC_REFRESH);
	HWND sizes_control = GetDlgItem(dialog, IDC_SIZES);

	if (refresh_control && sizes_control)
	{
		char option_value[256];
		int width = 0; 
		int height = 0;
		TCHAR buffer[256];

		(void)ComboBox_GetText(sizes_control, buffer, WINUI_ARRAY_LENGTH(buffer) - 1);

		if (_stscanf(buffer, TEXT("%d x %d"), &width, &height) == 2)
		{
			int refresh_index = ComboBox_GetCurSel(refresh_control);
			int refresh_value = ComboBox_GetItemData(refresh_control, refresh_index);
			snprintf(option_value, WINUI_ARRAY_LENGTH(option_value), "%dx%d@%d", width, height, refresh_value);
		}
		else
			strcpy(option_value, "auto");
		
		std::string error_string;
		opts.set_value(option_name, option_value, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
	}

	return false;
}

static bool ResolutionPopulateControl(datamap *map, HWND dialog, HWND control_, windows_options &opts, const char *option_name)
{
	HWND sizes_control = GetDlgItem(dialog, IDC_SIZES);
	HWND refresh_control = GetDlgItem(dialog, IDC_REFRESH);
	DEVMODE devmode;

	if (sizes_control && refresh_control)
	{
		int sizes_index = 0;
		int refresh_index = 0;
		int sizes_selection = 0;
		int refresh_selection = 0;
		char screen_option[32];
		int width = 0; 
		int height = 0; 
		int refresh = 0;
		TCHAR buf[32];

		// determine the resolution
		const char *option_value = opts.value(option_name);

		if (sscanf(option_value, "%dx%d@%d", &width, &height, &refresh) != 3)
		{
			width = 0;
			height = 0;
			refresh = 0;
		}

		// reset sizes control
		(void)ComboBox_ResetContent(sizes_control);
		(void)ComboBox_InsertString(sizes_control, sizes_index, TEXT("Auto"));
		(void)ComboBox_SetItemData(sizes_control, sizes_index, 0);
		sizes_index++;
		// reset refresh control
		(void)ComboBox_ResetContent(refresh_control);
		(void)ComboBox_InsertString(refresh_control, refresh_index, TEXT("Auto"));
		(void)ComboBox_SetItemData(refresh_control, refresh_index, 0);
		refresh_index++;
		// determine which screen we're using
		snprintf(screen_option, WINUI_ARRAY_LENGTH(screen_option), "screen%d", GetSelectedScreen(dialog));
		const char *screen = opts.value(screen_option);
		TCHAR *t_screen = win_wstring_from_utf8(screen);
		// retrieve screen information
		devmode.dmSize = sizeof(DEVMODE);

		for (int i = 0; EnumDisplaySettings(t_screen, i, &devmode); i++)
		{
			if ((devmode.dmBitsPerPel == 32 ) // Only 32 bit depth supported by core
				&& (devmode.dmDisplayFrequency == refresh || refresh == 0))
			{
				_sntprintf(buf, WINUI_ARRAY_LENGTH(buf), TEXT("%d x %d"), devmode.dmPelsWidth, devmode.dmPelsHeight);

				if (ComboBox_FindString(sizes_control, 0, buf) == CB_ERR)
				{
					(void)ComboBox_InsertString(sizes_control, sizes_index, buf);

					if ((width == devmode.dmPelsWidth) && (height == devmode.dmPelsHeight))
						sizes_selection = sizes_index;

					sizes_index++;
				}
			}

			if (devmode.dmDisplayFrequency >= 10 )
			{
				// I have some devmode "vga" which specifes 1 Hz, which is probably bogus, so we filter it out
				_sntprintf(buf, WINUI_ARRAY_LENGTH(buf), TEXT("%d Hz"), devmode.dmDisplayFrequency);

				if (ComboBox_FindString(refresh_control, 0, buf) == CB_ERR)
				{
					(void)ComboBox_InsertString(refresh_control, refresh_index, buf);
					(void)ComboBox_SetItemData(refresh_control, refresh_index, devmode.dmDisplayFrequency);

					if (refresh == devmode.dmDisplayFrequency)
						refresh_selection = refresh_index;

					refresh_index++;
				}
			}
		}

		free(t_screen);
		(void)ComboBox_SetCurSel(sizes_control, sizes_selection);
		(void)ComboBox_SetCurSel(refresh_control, refresh_selection);
	}

	return false;
}

//============================================================

/************************************************************
 * DataMap initializers
 ************************************************************/

/* Initialize local helper variables */
static void ResetDataMap(HWND hWnd)
{
	char screen_option[32];

	snprintf(screen_option, WINUI_ARRAY_LENGTH(screen_option), "screen%d", GetSelectedScreen(hWnd));

	if (pCurrentOpts.value(screen_option) == NULL || (core_stricmp(pCurrentOpts.value(screen_option), "") == 0 )
		|| (core_stricmp(pCurrentOpts.value(screen_option), "auto") == 0 ) )
	{
		std::string error_string;
		pCurrentOpts.set_value(screen_option, "auto", OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
	}
}

/* Build the control mapping by adding all needed information to the DataMap */
static void BuildDataMap(void)
{
	properties_datamap = datamap_create();

	// core state options
	datamap_add(properties_datamap, IDC_ENABLE_AUTOSAVE,		DM_BOOL,	OPTION_AUTOSAVE);
	datamap_add(properties_datamap, IDC_SNAPVIEW,				DM_STRING,	OPTION_SNAPVIEW);
	datamap_add(properties_datamap, IDC_SNAPNAME,				DM_STRING,	OPTION_SNAPNAME);
	datamap_add(properties_datamap, IDC_SNAPBILINEAR,			DM_BOOL,	OPTION_SNAPBILINEAR);
	datamap_add(properties_datamap, IDC_SNAPBURNIN,				DM_BOOL,	OPTION_BURNIN);
	datamap_add(properties_datamap, IDC_EXIT_PLAYBACK,			DM_BOOL,	OPTION_EXIT_AFTER_PLAYBACK);
	datamap_add(properties_datamap, IDC_SNAPSIZEWIDTH,			DM_STRING,	NULL);
	datamap_add(properties_datamap, IDC_SNAPSIZEHEIGHT,			DM_STRING,	NULL);
	// core performance options
	datamap_add(properties_datamap, IDC_AUTOFRAMESKIP,			DM_BOOL,	OPTION_AUTOFRAMESKIP);
	datamap_add(properties_datamap, IDC_FRAMESKIP,				DM_INT,		OPTION_FRAMESKIP);
	datamap_add(properties_datamap, IDC_SECONDSTORUN,			DM_INT,		OPTION_SECONDS_TO_RUN);
	datamap_add(properties_datamap, IDC_SECONDSTORUNDISP,		DM_INT,		OPTION_SECONDS_TO_RUN);
	datamap_add(properties_datamap, IDC_THROTTLE,				DM_BOOL,	OPTION_THROTTLE);
	datamap_add(properties_datamap, IDC_SLEEP,					DM_BOOL,	OPTION_SLEEP);
	datamap_add(properties_datamap, IDC_SPEED,					DM_FLOAT,	OPTION_SPEED);
	datamap_add(properties_datamap, IDC_SPEEDDISP,				DM_FLOAT,	OPTION_SPEED);
	datamap_add(properties_datamap, IDC_REFRESHSPEED,			DM_BOOL,	OPTION_REFRESHSPEED);
	// core rotation options
	datamap_add(properties_datamap, IDC_ROTATE,					DM_INT,		NULL);
	// ror, rol, autoror, autorol handled by callback
	datamap_add(properties_datamap, IDC_FLIPX,					DM_BOOL,	OPTION_FLIPX);
	datamap_add(properties_datamap, IDC_FLIPY,					DM_BOOL,	OPTION_FLIPY);
	// core artwork options
	datamap_add(properties_datamap, IDC_ARTWORK_CROP,			DM_BOOL,	OPTION_ARTWORK_CROP);
	datamap_add(properties_datamap, IDC_BACKDROPS,				DM_BOOL,	OPTION_USE_BACKDROPS);
	datamap_add(properties_datamap, IDC_OVERLAYS,				DM_BOOL,	OPTION_USE_OVERLAYS);
	datamap_add(properties_datamap, IDC_BEZELS,					DM_BOOL,	OPTION_USE_BEZELS);
	datamap_add(properties_datamap, IDC_CPANELS,				DM_BOOL,	OPTION_USE_CPANELS);
	datamap_add(properties_datamap, IDC_MARQUEES,				DM_BOOL,	OPTION_USE_MARQUEES);
	// core screen options
	datamap_add(properties_datamap, IDC_BRIGHTCORRECT,			DM_FLOAT,	OPTION_BRIGHTNESS);
	datamap_add(properties_datamap, IDC_BRIGHTCORRECTDISP,		DM_FLOAT,	OPTION_BRIGHTNESS);
	datamap_add(properties_datamap, IDC_CONTRAST,				DM_FLOAT,	OPTION_CONTRAST);
	datamap_add(properties_datamap, IDC_CONTRASTDISP,			DM_FLOAT,	OPTION_CONTRAST);
	datamap_add(properties_datamap, IDC_GAMMA,					DM_FLOAT,	OPTION_GAMMA);
	datamap_add(properties_datamap, IDC_GAMMADISP,				DM_FLOAT,	OPTION_GAMMA);
	datamap_add(properties_datamap, IDC_PAUSEBRIGHT,			DM_FLOAT,	OPTION_PAUSE_BRIGHTNESS);
	datamap_add(properties_datamap, IDC_PAUSEBRIGHTDISP,		DM_FLOAT,	OPTION_PAUSE_BRIGHTNESS);
	datamap_add(properties_datamap, IDC_EFFECT,					DM_STRING,	OPTION_EFFECT);
//	datamap_add(properties_datamap, IDC_WIDESTRETCH,			DM_BOOL,	OPTION_WIDESTRETCH);
	datamap_add(properties_datamap, IDC_UNEVENSTRETCH,			DM_BOOL,	OPTION_UNEVENSTRETCH);
	datamap_add(properties_datamap, IDC_UNEVENSTRETCHX,			DM_BOOL,	OPTION_UNEVENSTRETCHX);
	datamap_add(properties_datamap, IDC_UNEVENSTRETCHY,			DM_BOOL,	OPTION_UNEVENSTRETCHY);
	datamap_add(properties_datamap, IDC_AUTOSTRETCHXY,			DM_BOOL,	OPTION_AUTOSTRETCHXY);
	datamap_add(properties_datamap, IDC_INTOVERSCAN,			DM_BOOL,	OPTION_INTOVERSCAN);
	datamap_add(properties_datamap, IDC_INTSCALEX,				DM_INT,		OPTION_INTSCALEX);
	datamap_add(properties_datamap, IDC_INTSCALEX_TXT,			DM_INT,		OPTION_INTSCALEX);
	datamap_add(properties_datamap, IDC_INTSCALEY,				DM_INT,		OPTION_INTSCALEY);
	datamap_add(properties_datamap, IDC_INTSCALEY_TXT,			DM_INT,		OPTION_INTSCALEY);
	// core opengl - bgfx options
	datamap_add(properties_datamap, IDC_GLSLPOW,				DM_BOOL,	OSDOPTION_GL_FORCEPOW2TEXTURE);
	datamap_add(properties_datamap, IDC_GLSLTEXTURE,			DM_BOOL,	OSDOPTION_GL_NOTEXTURERECT);
	datamap_add(properties_datamap, IDC_GLSLVBO,				DM_BOOL,	OSDOPTION_GL_VBO);
	datamap_add(properties_datamap, IDC_GLSLPBO,				DM_BOOL,	OSDOPTION_GL_PBO);
	datamap_add(properties_datamap, IDC_GLSL,					DM_BOOL,	OSDOPTION_GL_GLSL);
	datamap_add(properties_datamap, IDC_GLSLFILTER,				DM_BOOL,	OSDOPTION_GLSL_FILTER);
//	datamap_add(properties_datamap, IDC_GLSLSYNC,				DM_BOOL,	OSDOPTION_GLSL_SYNC);
	datamap_add(properties_datamap, IDC_BGFX_CHAINS,			DM_STRING,	OSDOPTION_BGFX_SCREEN_CHAINS);
	datamap_add(properties_datamap, IDC_MAME_SHADER0,			DM_STRING,	OSDOPTION_SHADER_MAME "0");
	datamap_add(properties_datamap, IDC_MAME_SHADER1,			DM_STRING,	OSDOPTION_SHADER_MAME "1");
	datamap_add(properties_datamap, IDC_MAME_SHADER2,			DM_STRING,	OSDOPTION_SHADER_MAME "2");
	datamap_add(properties_datamap, IDC_MAME_SHADER3,			DM_STRING,	OSDOPTION_SHADER_MAME "3");
	datamap_add(properties_datamap, IDC_MAME_SHADER4,			DM_STRING,	OSDOPTION_SHADER_MAME "4");
	datamap_add(properties_datamap, IDC_SCREEN_SHADER0,			DM_STRING,	OSDOPTION_SHADER_SCREEN "0");
	datamap_add(properties_datamap, IDC_SCREEN_SHADER1,			DM_STRING,	OSDOPTION_SHADER_SCREEN "1");
	datamap_add(properties_datamap, IDC_SCREEN_SHADER2,			DM_STRING,	OSDOPTION_SHADER_SCREEN "2");
	datamap_add(properties_datamap, IDC_SCREEN_SHADER3,			DM_STRING,	OSDOPTION_SHADER_SCREEN "3");
	datamap_add(properties_datamap, IDC_SCREEN_SHADER4,			DM_STRING,	OSDOPTION_SHADER_SCREEN "4");
	// core vector options
	datamap_add(properties_datamap, IDC_BEAM_MIN,				DM_FLOAT,	OPTION_BEAM_WIDTH_MIN);
	datamap_add(properties_datamap, IDC_BEAM_MINDISP,			DM_FLOAT,	OPTION_BEAM_WIDTH_MIN);
	datamap_add(properties_datamap, IDC_BEAM_MAX,				DM_FLOAT,	OPTION_BEAM_WIDTH_MAX);
	datamap_add(properties_datamap, IDC_BEAM_MAXDISP,			DM_FLOAT,	OPTION_BEAM_WIDTH_MAX);
	datamap_add(properties_datamap, IDC_BEAM_INTEN,				DM_FLOAT,	OPTION_BEAM_INTENSITY_WEIGHT);
	datamap_add(properties_datamap, IDC_BEAM_INTENDISP,			DM_FLOAT,	OPTION_BEAM_INTENSITY_WEIGHT);
	datamap_add(properties_datamap, IDC_FLICKER,				DM_FLOAT,	OPTION_FLICKER);
	datamap_add(properties_datamap, IDC_FLICKERDISP,			DM_FLOAT,	OPTION_FLICKER);
	// core sound options
	datamap_add(properties_datamap, IDC_SOUND_MODE,				DM_STRING,	OSDOPTION_SOUND);
	datamap_add(properties_datamap, IDC_SAMPLERATE,				DM_INT,		OPTION_SAMPLERATE);
	datamap_add(properties_datamap, IDC_SAMPLES,				DM_BOOL,	OPTION_SAMPLES);
	datamap_add(properties_datamap, IDC_VOLUME,					DM_INT,		OPTION_VOLUME);
	datamap_add(properties_datamap, IDC_VOLUMEDISP,				DM_INT,		OPTION_VOLUME);
	// core input options
	datamap_add(properties_datamap, IDC_COINLOCKOUT,			DM_BOOL,	OPTION_COIN_LOCKOUT);
	datamap_add(properties_datamap, IDC_DEFAULT_INPUT,			DM_STRING,	OPTION_CTRLR);
	datamap_add(properties_datamap, IDC_USE_MOUSE,				DM_BOOL,	OPTION_MOUSE);
	datamap_add(properties_datamap, IDC_JOYSTICK,				DM_BOOL,	OPTION_JOYSTICK);
	datamap_add(properties_datamap, IDC_LIGHTGUN,				DM_BOOL,	OPTION_LIGHTGUN);
	datamap_add(properties_datamap, IDC_STEADYKEY,				DM_BOOL,	OPTION_STEADYKEY);
	datamap_add(properties_datamap, IDC_MULTIKEYBOARD,			DM_BOOL,	OPTION_MULTIKEYBOARD);
	datamap_add(properties_datamap, IDC_MULTIMOUSE,				DM_BOOL,	OPTION_MULTIMOUSE);
	datamap_add(properties_datamap, IDC_RELOAD,					DM_BOOL,	OPTION_OFFSCREEN_RELOAD);
	datamap_add(properties_datamap, IDC_JDZ,					DM_FLOAT,	OPTION_JOYSTICK_DEADZONE);
	datamap_add(properties_datamap, IDC_JDZDISP,				DM_FLOAT,	OPTION_JOYSTICK_DEADZONE);
	datamap_add(properties_datamap, IDC_JSAT,					DM_FLOAT,	OPTION_JOYSTICK_SATURATION);
	datamap_add(properties_datamap, IDC_JSATDISP,				DM_FLOAT,	OPTION_JOYSTICK_SATURATION);
	datamap_add(properties_datamap, IDC_JOYSTICKMAP,			DM_STRING,	OPTION_JOYSTICK_MAP);
	// core input automatic enable options
	datamap_add(properties_datamap, IDC_PADDLE,					DM_STRING,	OPTION_PADDLE_DEVICE);
	datamap_add(properties_datamap, IDC_ADSTICK,				DM_STRING,	OPTION_ADSTICK_DEVICE);
	datamap_add(properties_datamap, IDC_PEDAL,					DM_STRING,	OPTION_PEDAL_DEVICE);
	datamap_add(properties_datamap, IDC_DIAL,					DM_STRING,	OPTION_DIAL_DEVICE);
	datamap_add(properties_datamap, IDC_TRACKBALL,				DM_STRING,	OPTION_TRACKBALL_DEVICE);
	datamap_add(properties_datamap, IDC_LIGHTGUNDEVICE,			DM_STRING,	OPTION_LIGHTGUN_DEVICE);
	datamap_add(properties_datamap, IDC_POSITIONAL,				DM_STRING,	OPTION_POSITIONAL_DEVICE);
	datamap_add(properties_datamap, IDC_MOUSE,					DM_STRING,	OPTION_MOUSE_DEVICE);
	// core misc options
	datamap_add(properties_datamap, IDC_BIOS,					DM_STRING,	OPTION_BIOS);
	datamap_add(properties_datamap, IDC_CHEAT,					DM_BOOL,	OPTION_CHEAT);
	datamap_add(properties_datamap, IDC_DEBUG,					DM_BOOL,	OPTION_DEBUG);
	datamap_add(properties_datamap, IDC_LOGERROR,				DM_BOOL,	OPTION_LOG);
	datamap_add(properties_datamap, IDC_SKIP_GAME_INFO,			DM_BOOL,	OPTION_SKIP_GAMEINFO);
	datamap_add(properties_datamap, IDC_CONFIRM_QUIT,			DM_BOOL,	OPTION_CONFIRM_QUIT);
	datamap_add(properties_datamap, IDC_ENABLE_MOUSE_UI,		DM_BOOL,	OPTION_UI_MOUSE);
	datamap_add(properties_datamap, IDC_CHEATFILE,				DM_STRING,	OPTION_CHEATPATH);
	datamap_add(properties_datamap, IDC_LANGUAGE,				DM_STRING,	OPTION_LANGUAGE);
	datamap_add(properties_datamap, IDC_LUASCRIPT,				DM_STRING,	OPTION_AUTOBOOT_SCRIPT);
	datamap_add(properties_datamap, IDC_BOOTDELAY,				DM_INT,		OPTION_AUTOBOOT_DELAY);
	datamap_add(properties_datamap, IDC_BOOTDELAYDISP,			DM_INT,		OPTION_AUTOBOOT_DELAY);
	datamap_add(properties_datamap, IDC_PLUGINS,				DM_STRING,	OPTION_PLUGINS);
	datamap_add(properties_datamap, IDC_PLUGIN,					DM_STRING,	OPTION_PLUGIN);
	// windows performance options
	datamap_add(properties_datamap, IDC_HIGH_PRIORITY,			DM_INT,		WINOPTION_PRIORITY);
	datamap_add(properties_datamap, IDC_HIGH_PRIORITYTXT,		DM_INT,		WINOPTION_PRIORITY);
	// windows video options
	datamap_add(properties_datamap, IDC_VIDEO_MODE,				DM_STRING,	OSDOPTION_VIDEO);
	datamap_add(properties_datamap, IDC_NUMSCREENS,				DM_INT,		OSDOPTION_NUMSCREENS);
	datamap_add(properties_datamap, IDC_NUMSCREENSDISP,			DM_INT,		OSDOPTION_NUMSCREENS);
	datamap_add(properties_datamap, IDC_WINDOWED,				DM_BOOL,	OSDOPTION_WINDOW);
	datamap_add(properties_datamap, IDC_MAXIMIZE,				DM_BOOL,	OSDOPTION_MAXIMIZE);
	datamap_add(properties_datamap, IDC_KEEPASPECT,				DM_BOOL,	OPTION_KEEPASPECT);
	datamap_add(properties_datamap, IDC_PRESCALE,				DM_INT,		OSDOPTION_PRESCALE);
	datamap_add(properties_datamap, IDC_PRESCALEDISP,			DM_INT,		OSDOPTION_PRESCALE);
	datamap_add(properties_datamap, IDC_WAITVSYNC,				DM_BOOL,	OSDOPTION_WAITVSYNC);
//	datamap_add(properties_datamap, IDC_SYNCREFRESH,			DM_BOOL,	OPTION_SYNCREFRESH);
	datamap_add(properties_datamap, IDC_D3D_FILTER,				DM_BOOL,	OSDOPTION_FILTER);
	// per window video options
	datamap_add(properties_datamap, IDC_SCREEN,					DM_STRING,	NULL);
	datamap_add(properties_datamap, IDC_SCREENSELECT,			DM_STRING,	NULL);
	datamap_add(properties_datamap, IDC_VIEW,					DM_STRING,	NULL);
	datamap_add(properties_datamap, IDC_ASPECTRATIOD,			DM_STRING,  NULL);
	datamap_add(properties_datamap, IDC_ASPECTRATION,			DM_STRING,  NULL);
	datamap_add(properties_datamap, IDC_REFRESH,				DM_STRING,  NULL);
	datamap_add(properties_datamap, IDC_SIZES,					DM_STRING,  NULL);
	// full screen options
	datamap_add(properties_datamap, IDC_TRIPLE_BUFFER,			DM_BOOL,	WINOPTION_TRIPLEBUFFER);
	datamap_add(properties_datamap, IDC_SWITCHRES,				DM_BOOL,	OSDOPTION_SWITCHRES);
	datamap_add(properties_datamap, IDC_FSBRIGHTNESS,			DM_FLOAT,	WINOPTION_FULLSCREENBRIGHTNESS);
	datamap_add(properties_datamap, IDC_FSBRIGHTNESSDISP,		DM_FLOAT,	WINOPTION_FULLSCREENBRIGHTNESS);
	datamap_add(properties_datamap, IDC_FSCONTRAST,				DM_FLOAT,	WINOPTION_FULLSCREENCONTRAST);
	datamap_add(properties_datamap, IDC_FSCONTRASTDISP,			DM_FLOAT,	WINOPTION_FULLSCREENCONTRAST);
	datamap_add(properties_datamap, IDC_FSGAMMA,				DM_FLOAT,	WINOPTION_FULLSCREENGAMMA);
	datamap_add(properties_datamap, IDC_FSGAMMADISP,			DM_FLOAT,	WINOPTION_FULLSCREENGAMMA);
	// windows sound options
	datamap_add(properties_datamap, IDC_AUDIO_LATENCY,			DM_INT,		OSDOPTION_AUDIO_LATENCY);
	datamap_add(properties_datamap, IDC_AUDIO_LATENCY_DISP,		DM_INT,		OSDOPTION_AUDIO_LATENCY);
	// input device options
	datamap_add(properties_datamap, IDC_DUAL_LIGHTGUN,			DM_BOOL,	WINOPTION_DUAL_LIGHTGUN);
	// hlsl
	datamap_add(properties_datamap, IDC_HLSL_ON,				DM_BOOL,	WINOPTION_HLSL_ENABLE);
	// set up callbacks
	datamap_set_callback(properties_datamap, IDC_ROTATE,		DCT_READ_CONTROL,		RotateReadControl);
	datamap_set_callback(properties_datamap, IDC_ROTATE,		DCT_POPULATE_CONTROL,	RotatePopulateControl);
	datamap_set_callback(properties_datamap, IDC_SCREEN,		DCT_READ_CONTROL,		ScreenReadControl);
	datamap_set_callback(properties_datamap, IDC_SCREEN,		DCT_POPULATE_CONTROL,	ScreenPopulateControl);
	datamap_set_callback(properties_datamap, IDC_VIEW,			DCT_POPULATE_CONTROL,	ViewPopulateControl);
	datamap_set_callback(properties_datamap, IDC_REFRESH,		DCT_READ_CONTROL,		ResolutionReadControl);
	datamap_set_callback(properties_datamap, IDC_REFRESH,		DCT_POPULATE_CONTROL,	ResolutionPopulateControl);
	datamap_set_callback(properties_datamap, IDC_SIZES,			DCT_READ_CONTROL,		ResolutionReadControl);
	datamap_set_callback(properties_datamap, IDC_SIZES,			DCT_POPULATE_CONTROL,	ResolutionPopulateControl);
	datamap_set_callback(properties_datamap, IDC_DEFAULT_INPUT,	DCT_READ_CONTROL,		DefaultInputReadControl);
	datamap_set_callback(properties_datamap, IDC_DEFAULT_INPUT,	DCT_POPULATE_CONTROL,	DefaultInputPopulateControl);
	datamap_set_option_name_callback(properties_datamap, IDC_VIEW,		ViewSetOptionName);
	//missing population of views with per game defined additional views
	datamap_set_option_name_callback(properties_datamap, IDC_REFRESH,	ResolutionSetOptionName);
	datamap_set_option_name_callback(properties_datamap, IDC_SIZES,		ResolutionSetOptionName);
	// formats
	datamap_set_int_format(properties_datamap, IDC_VOLUMEDISP,			"%ddB");
	datamap_set_int_format(properties_datamap, IDC_AUDIO_LATENCY_DISP,	"%d/5");
	datamap_set_float_format(properties_datamap, IDC_BEAM_MINDISP,		"%3.2f");
	datamap_set_float_format(properties_datamap, IDC_BEAM_MAXDISP,		"%3.2f");
	datamap_set_float_format(properties_datamap, IDC_BEAM_INTENDISP,	"%3.2f");
	datamap_set_float_format(properties_datamap, IDC_FLICKERDISP,		"%3.2f");
	datamap_set_float_format(properties_datamap, IDC_GAMMADISP,			"%3.2f");
	datamap_set_float_format(properties_datamap, IDC_BRIGHTCORRECTDISP,	"%3.2f");
	datamap_set_float_format(properties_datamap, IDC_CONTRASTDISP,		"%3.2f");
	datamap_set_float_format(properties_datamap, IDC_PAUSEBRIGHTDISP,	"%3.2f");
	datamap_set_float_format(properties_datamap, IDC_FSGAMMADISP,		"%3.2f");
	datamap_set_float_format(properties_datamap, IDC_FSBRIGHTNESSDISP,	"%3.2f");
	datamap_set_float_format(properties_datamap, IDC_FSCONTRASTDISP,	"%3.2f");
	datamap_set_float_format(properties_datamap, IDC_JDZDISP,			"%3.2f");
	datamap_set_float_format(properties_datamap, IDC_JSATDISP,			"%3.2f");
	datamap_set_float_format(properties_datamap, IDC_SPEEDDISP,			"%3.1f");
	// trackbar ranges - slider-name,start,end,step
	datamap_set_trackbar_range(properties_datamap, IDC_JDZ,         	0.00, 1.00, 0.01);
	datamap_set_trackbar_range(properties_datamap, IDC_JSAT,        	0.00, 1.00, 0.01);
	datamap_set_trackbar_range(properties_datamap, IDC_SPEED,       	0.00, 100.00, 0.1);
	datamap_set_trackbar_range(properties_datamap, IDC_BEAM_MIN,        0.00, 1.00, 0.01);
	datamap_set_trackbar_range(properties_datamap, IDC_BEAM_MAX,        1.00, 10.00, 0.01);
	datamap_set_trackbar_range(properties_datamap, IDC_BEAM_INTEN,      -10.00, 10.00, 0.01);
	datamap_set_trackbar_range(properties_datamap, IDC_FLICKER,       	0.00, 1.00, 0.01);
	datamap_set_trackbar_range(properties_datamap, IDC_AUDIO_LATENCY, 	1, 5, 1);
	datamap_set_trackbar_range(properties_datamap, IDC_VOLUME,      	-32, 0, 1);
	datamap_set_trackbar_range(properties_datamap, IDC_HIGH_PRIORITY, 	-15, 1, 1);
	datamap_set_trackbar_range(properties_datamap, IDC_SECONDSTORUN, 	0, 60, 1);
	datamap_set_trackbar_range(properties_datamap, IDC_NUMSCREENS, 		1, 4, 1);
	datamap_set_trackbar_range(properties_datamap, IDC_PRESCALE, 		1, 3, 1);
	datamap_set_trackbar_range(properties_datamap, IDC_FSGAMMA, 		0.00, 3.00, 0.05);
	datamap_set_trackbar_range(properties_datamap, IDC_FSBRIGHTNESS, 	0.00, 2.00, 0.01);
	datamap_set_trackbar_range(properties_datamap, IDC_FSCONTRAST, 		0.00, 2.00, 0.05);
	datamap_set_trackbar_range(properties_datamap, IDC_GAMMA, 			0.00, 3.00, 0.05);
	datamap_set_trackbar_range(properties_datamap, IDC_BRIGHTCORRECT, 	0.00, 2.00, 0.01);
	datamap_set_trackbar_range(properties_datamap, IDC_CONTRAST, 		0.00, 2.00, 0.05);
	datamap_set_trackbar_range(properties_datamap, IDC_PAUSEBRIGHT, 	0.00, 1.00, 0.01);
	datamap_set_trackbar_range(properties_datamap, IDC_BOOTDELAY, 		0, 5, 1);
	datamap_set_trackbar_range(properties_datamap, IDC_INTSCALEX, 		0, 4, 1);
	datamap_set_trackbar_range(properties_datamap, IDC_INTSCALEY, 		0, 4, 1);
}

//mamefx: for coloring of changed elements
static bool IsControlOptionValue(HWND hDlg, HWND hWnd_ctrl, windows_options &opts, windows_options &ref)
{
	const char *option_name = datamap_get_control_option_name(properties_datamap, hDlg, hWnd_ctrl);

	if (option_name == NULL)
		return true;

	const char *opts_value = opts.value(option_name);
	const char *ref_value = ref.value(option_name);

	if (opts_value == ref_value)
		return true;

	if (!opts_value || !ref_value)
		return false;

	return strcmp(opts_value, ref_value) == 0;
}

/* Moved here cause it's called in a few places */
static void InitializeOptions(HWND hDlg)
{
	InitializeSampleRateUI(hDlg);
	InitializeSoundModeUI(hDlg);
	InitializeSkippingUI(hDlg);
	InitializeRotateUI(hDlg);
	InitializeSelectScreenUI(hDlg);
	InitializeBIOSUI(hDlg);
	InitializeControllerMappingUI(hDlg);
	InitializeVideoUI(hDlg);
	InitializeSnapViewUI(hDlg);
	InitializeSnapNameUI(hDlg);
	InitializeLanguageUI(hDlg);
	InitializePluginsUI(hDlg);
}

static void OptOnHScroll(HWND hWnd, HWND hWndCtl, UINT code, int pos)
{
	if (hWndCtl == GetDlgItem(hWnd, IDC_NUMSCREENS))
		NumScreensSelectionChange(hWnd);
}

/* Handle changes to the Numscreens slider */
static void NumScreensSelectionChange(HWND hWnd)
{
	//Also Update the ScreenSelect Combo with the new number of screens
	UpdateSelectScreenUI(hWnd );
}

/* Handle changes to the Refresh drop down */
static void RefreshSelectionChange(HWND hWnd, HWND hWndCtrl)
{
	int nCurSelection = ComboBox_GetCurSel(hWndCtrl);

	if (nCurSelection != CB_ERR)
	{
		datamap_read_control(properties_datamap, hWnd, pCurrentOpts, IDC_SIZES);
		datamap_populate_control(properties_datamap, hWnd, pCurrentOpts, IDC_SIZES);
	}
}

/* Populate the Sample Rate drop down */
static void InitializeSampleRateUI(HWND hWnd)
{
	HWND hCtrl = GetDlgItem(hWnd, IDC_SAMPLERATE);

	if (hCtrl)
	{
		for (int i = 0; i < NUMSAMPLERATE; i++)
		{
			(void)ComboBox_InsertString(hCtrl, i, g_ComboBoxSampleRate[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl, i, g_ComboBoxSampleRate[i].m_pData);
		}
	}
}

/* Populate the Sound Mode drop down */
static void InitializeSoundModeUI(HWND hWnd)
{
	HWND hCtrl = GetDlgItem(hWnd, IDC_SOUND_MODE);

	if (hCtrl)
	{
		for (int i = 0; i < NUMSOUND; i++)
		{
			(void)ComboBox_InsertString(hCtrl, i, g_ComboBoxSound[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl, i, g_ComboBoxSound[i].m_pData);
		}
	}
}

/* Populate the Frame Skipping drop down */
static void InitializeSkippingUI(HWND hWnd)
{
	HWND hCtrl = GetDlgItem(hWnd, IDC_FRAMESKIP);

	if (hCtrl)
	{
		for (int i = 0; i < NUMFRAMESKIP; i++)
		{
			(void)ComboBox_InsertString(hCtrl, i, g_ComboBoxFrameSkip[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl, i, g_ComboBoxFrameSkip[i].m_pData);
		}
	}
}

/* Populate the Rotate drop down */
static void InitializeRotateUI(HWND hWnd)
{
	HWND hCtrl = GetDlgItem(hWnd, IDC_ROTATE);

	if (hCtrl)
	{
		(void)ComboBox_AddString(hCtrl, TEXT("Default"));             // 0
		(void)ComboBox_AddString(hCtrl, TEXT("Clockwise"));           // 1
		(void)ComboBox_AddString(hCtrl, TEXT("Anti-clockwise"));      // 2
		(void)ComboBox_AddString(hCtrl, TEXT("None"));                // 3
		(void)ComboBox_AddString(hCtrl, TEXT("Auto clockwise"));      // 4
		(void)ComboBox_AddString(hCtrl, TEXT("Auto anti-clockwise")); // 5
	}
}

/* Populate the Video Mode drop down */
static void InitializeVideoUI(HWND hWnd)
{
	HWND hCtrl = GetDlgItem(hWnd, IDC_VIDEO_MODE);

	if (hCtrl)
	{
		for (int i = 0; i < NUMVIDEO; i++)
		{
			(void)ComboBox_InsertString(hCtrl, i, g_ComboBoxVideo[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl, i, g_ComboBoxVideo[i].m_pData);
		}
	}
}

static void InitializeSnapNameUI(HWND hWnd)
{
	HWND hCtrl = GetDlgItem(hWnd, IDC_SNAPNAME);

	if (hCtrl)
	{
		for (int i = 0; i < NUMSNAPNAME; i++)
		{
			(void)ComboBox_InsertString(hCtrl, i, g_ComboBoxSnapName[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl, i, g_ComboBoxSnapName[i].m_pData);
		}
	}
}

static void InitializeSnapViewUI(HWND hWnd)
{
	HWND hCtrl = GetDlgItem(hWnd, IDC_SNAPVIEW);

	if (hCtrl)
	{
		for (int i = 0; i < NUMSNAPVIEW; i++)
		{
			(void)ComboBox_InsertString(hCtrl, i, g_ComboBoxSnapView[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl, i, g_ComboBoxSnapView[i].m_pData);
		}
	}
}

static void UpdateSelectScreenUI(HWND hWnd)
{
	HWND hCtrl = GetDlgItem(hWnd, IDC_SCREENSELECT);

	if (hCtrl)
	{
		int curSel = ComboBox_GetCurSel(hCtrl);
		int i = 0;

		if ((curSel < 0) || (curSel >= NUMSELECTSCREEN))
			curSel = 0;

		(void)ComboBox_ResetContent(hCtrl);

		for (i = 0; i < NUMSELECTSCREEN && i < pCurrentOpts.int_value(OSDOPTION_NUMSCREENS) ; i++)
		{
			(void)ComboBox_InsertString(hCtrl, i, g_ComboBoxSelectScreen[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl, i, g_ComboBoxSelectScreen[i].m_pData);
		}

		// Smaller Amount of screens was selected, so use 0
		if (i < curSel)
			(void)ComboBox_SetCurSel(hCtrl, 0);
		else
			(void)ComboBox_SetCurSel(hCtrl, curSel);
	}
}

/* Populate the Select Screen drop down */
static void InitializeSelectScreenUI(HWND hWnd)
{
	UpdateSelectScreenUI(hWnd);
}

static void InitializeControllerMappingUI(HWND hWnd)
{
	HWND hCtrl  = GetDlgItem(hWnd, IDC_PADDLE);
	HWND hCtrl1 = GetDlgItem(hWnd, IDC_ADSTICK);
	HWND hCtrl2 = GetDlgItem(hWnd, IDC_PEDAL);
	HWND hCtrl3 = GetDlgItem(hWnd, IDC_MOUSE);
	HWND hCtrl4 = GetDlgItem(hWnd, IDC_DIAL);
	HWND hCtrl5 = GetDlgItem(hWnd, IDC_TRACKBALL);
	HWND hCtrl6 = GetDlgItem(hWnd, IDC_LIGHTGUNDEVICE);
	HWND hCtrl7 = GetDlgItem(hWnd, IDC_POSITIONAL);

	if (hCtrl)
	{
		for (int i = 0; i < NUMDEVICES; i++)
		{
			(void)ComboBox_InsertString(hCtrl, i, g_ComboBoxDevice[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl, i, g_ComboBoxDevice[i].m_pData);
		}
	}

	if (hCtrl1)
	{
		for (int i = 0; i < NUMDEVICES; i++)
		{
			(void)ComboBox_InsertString(hCtrl1, i, g_ComboBoxDevice[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl1, i, g_ComboBoxDevice[i].m_pData);
		}
	}

	if (hCtrl2)
	{
		for (int i = 0; i < NUMDEVICES; i++)
		{
			(void)ComboBox_InsertString(hCtrl2, i, g_ComboBoxDevice[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl2, i, g_ComboBoxDevice[i].m_pData);
		}
	}

	if (hCtrl3)
	{
		for (int i = 0; i < NUMDEVICES; i++)
		{
			(void)ComboBox_InsertString(hCtrl3, i, g_ComboBoxDevice[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl3, i, g_ComboBoxDevice[i].m_pData);
		}
	}

	if (hCtrl4)
	{
		for (int i = 0; i < NUMDEVICES; i++)
		{
			(void)ComboBox_InsertString(hCtrl4, i, g_ComboBoxDevice[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl4, i, g_ComboBoxDevice[i].m_pData);
		}
	}

	if (hCtrl5)
	{
		for (int i = 0; i < NUMDEVICES; i++)
		{
			(void)ComboBox_InsertString(hCtrl5, i, g_ComboBoxDevice[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl5, i, g_ComboBoxDevice[i].m_pData);
		}
	}

	if (hCtrl6)
	{
		for (int i = 0; i < NUMDEVICES; i++)
		{
			(void)ComboBox_InsertString(hCtrl6, i, g_ComboBoxDevice[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl6, i, g_ComboBoxDevice[i].m_pData);
		}
	}

	if (hCtrl7)
	{
		for (int i = 0; i < NUMDEVICES; i++)
		{
			(void)ComboBox_InsertString(hCtrl7, i, g_ComboBoxDevice[i].m_pText);
			(void)ComboBox_SetItemData(hCtrl7, i, g_ComboBoxDevice[i].m_pData);
		}
	}
}

static void InitializeBIOSUI(HWND hWnd)
{
	HWND hCtrl = GetDlgItem(hWnd, IDC_BIOS);

	if (hCtrl)
	{
		const game_driver *gamedrv = &driver_list::driver(g_nGame);
		int i = 0;

		if (g_nGame == GLOBAL_OPTIONS)
		{
			(void)ComboBox_InsertString(hCtrl, i, TEXT("None"));
			(void)ComboBox_SetItemData(hCtrl, i++, "");
			return;
		}

		if (g_nGame == FOLDER_OPTIONS) 		//Folder Options
		{
			gamedrv = &driver_list::driver(g_nFolderGame);

			if (DriverHasOptionalBIOS(g_nFolderGame) == false)
			{
				(void)ComboBox_InsertString(hCtrl, i, TEXT("None"));
				(void)ComboBox_SetItemData(hCtrl, i++, "");
				return;
			}
			
			(void)ComboBox_InsertString(hCtrl, i, TEXT("Default"));
			(void)ComboBox_SetItemData(hCtrl, i++, "");

			if (gamedrv->rom != NULL)
			{
				auto rom_entries = rom_build_entries(gamedrv->rom);
				
				for (const rom_entry *rom = rom_entries.data(); !ROMENTRY_ISEND(rom); rom++)
				{
					if (ROMENTRY_ISSYSTEM_BIOS(rom))
					{
						const char *name = ROM_GETHASHDATA(rom);
						const char *biosname = ROM_GETNAME(rom);
						TCHAR *t_s = win_wstring_from_utf8(name);

						if(!t_s)
							return;

						(void)ComboBox_InsertString(hCtrl, i, t_s);
						(void)ComboBox_SetItemData(hCtrl, i++, biosname);
						free(t_s);
					}
				}
			}

			return;
		}

		if (DriverHasOptionalBIOS(g_nGame) == false)
		{
			(void)ComboBox_InsertString(hCtrl, i, TEXT("None"));
			(void)ComboBox_SetItemData(hCtrl, i++, "");
			return;
		}

		(void)ComboBox_InsertString(hCtrl, i, TEXT("Default"));
		(void)ComboBox_SetItemData(hCtrl, i++, "");

		if (gamedrv->rom != NULL)
		{
			auto rom_entries = rom_build_entries(gamedrv->rom);

			for (const rom_entry *rom = rom_entries.data(); !ROMENTRY_ISEND(rom); rom++)
			{
				if (ROMENTRY_ISSYSTEM_BIOS(rom))
				{
					const char *name = ROM_GETHASHDATA(rom);
					const char *biosname = ROM_GETNAME(rom);
					TCHAR *t_s = win_wstring_from_utf8(name);

					if(!t_s)
						return;

					(void)ComboBox_InsertString(hCtrl, i, t_s);
					(void)ComboBox_SetItemData(hCtrl, i++, biosname);
					free(t_s);
				}
			}
		}
	}
}

static void InitializeLanguageUI(HWND hWnd)
{
	HWND hCtrl = GetDlgItem(hWnd, IDC_LANGUAGE);

	if (hCtrl)
	{
		int count = 0;
		osd::directory::ptr directory = osd::directory::open(GetLanguageDir());

		if (directory == nullptr)
			return;

		for (const osd::directory::entry *entry = directory->read(); entry != nullptr; entry = directory->read())
		{
			if (entry->type == osd::directory::entry::entry_type::DIR)
			{
				std::string name = entry->name;
				
				if (!(name == "." || name == ".."))
				{
					const char *value = core_strdup(entry->name);
					TCHAR *text = win_wstring_from_utf8(entry->name);
					(void)ComboBox_InsertString(hCtrl, count, text);
					(void)ComboBox_SetItemData(hCtrl, count, value);
					count++;
					free(text);
					value = NULL;
				}
			}
		}

		directory.reset();
	}
}

static void InitializePluginsUI(HWND hWnd)
{
	HWND hCtrl = GetDlgItem(hWnd, IDC_SELECT_PLUGIN);

	if (hCtrl)
	{
		int count = 0;
		osd::directory::ptr directory = osd::directory::open(GetPluginsDir());
		
		if (directory == nullptr)
			return;

		for (const osd::directory::entry *entry = directory->read(); entry != nullptr; entry = directory->read())
		{
			if (entry->type == osd::directory::entry::entry_type::DIR)
			{
				std::string name = entry->name;

				if (!(name == "." || name == ".." || name == "json"))
				{
					const char *value = core_strdup(entry->name);
					TCHAR *text = win_wstring_from_utf8(entry->name);
					(void)ComboBox_InsertString(hCtrl, count, text);
					(void)ComboBox_SetItemData(hCtrl, count, value);
					count++;
					free(text);
					value = NULL;
				}
			}
		}

		directory.reset();
	}

	(void)ComboBox_SetCurSel(hCtrl, -1);
	(void)ComboBox_SetCueBannerText(hCtrl, TEXT("Select a plugin"));
}

static bool SelectEffect(HWND hWnd)
{
	char filename[MAX_PATH];
	bool changed = false;

	*filename = 0;

	if (CommonFileDialog(GetOpenFileName, filename, FILETYPE_EFFECT_FILES, false))
	{
		char option[MAX_PATH];
		TCHAR *t_filename = win_wstring_from_utf8(filename);
		TCHAR *tempname = PathFindFileName(t_filename);
		PathRemoveExtension(tempname);
		char *optname = win_utf8_from_wstring(tempname);
		strcpy(option, optname);
		free(t_filename);
		free(optname);

		if (strcmp(option, pCurrentOpts.value(OPTION_EFFECT)))
		{
			std::string error_string;
			pCurrentOpts.set_value(OPTION_EFFECT, option, OPTION_PRIORITY_CMDLINE, error_string);
			assert(error_string.empty());
			win_set_window_text_utf8(GetDlgItem(hWnd, IDC_EFFECT), option);
			changed = true;
		}
	}

	return changed;
}

static bool ResetEffect(HWND hWnd)
{
	bool changed = false;
	const char *new_value = "none";

	if (strcmp(new_value, pCurrentOpts.value(OPTION_EFFECT)))
	{
		std::string error_string;
		pCurrentOpts.set_value(OPTION_EFFECT, new_value, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
		win_set_window_text_utf8(GetDlgItem(hWnd, IDC_EFFECT), "None");
		changed = true;
	}

	return changed;
}

static bool SelectMameShader(HWND hWnd, int slot)
{
	char filename[MAX_PATH];
	bool changed = false;
	char shader[32];
	int dialog = IDC_MAME_SHADER0 + slot;

	*filename = 0;
	snprintf(shader, WINUI_ARRAY_LENGTH(shader), "glsl_shader_mame%d", slot);

	if (CommonFileDialog(GetOpenFileName, filename, FILETYPE_SHADER_FILES, false))
	{
		char option[MAX_PATH];
		TCHAR *t_filename = win_wstring_from_utf8(filename);
		TCHAR *tempname = PathFindFileName(t_filename);
		PathRemoveExtension(tempname);
		char *optname = win_utf8_from_wstring(tempname);
		strcpy(option, optname);
		free(t_filename);
		free(optname);

		if (strcmp(option, pCurrentOpts.value(shader)))
		{
			std::string error_string;
			pCurrentOpts.set_value(shader, option, OPTION_PRIORITY_CMDLINE, error_string);
			assert(error_string.empty());
			win_set_window_text_utf8(GetDlgItem(hWnd, dialog), option);
			changed = true;
		}
	}

	return changed;
}

static bool ResetMameShader(HWND hWnd, int slot)
{
	bool changed = false;
	const char *new_value = "none";
	char option[32];
	int dialog = IDC_MAME_SHADER0 + slot;

	snprintf(option, WINUI_ARRAY_LENGTH(option), "glsl_shader_mame%d", slot);

	if (strcmp(new_value, pCurrentOpts.value(option)))
	{
		std::string error_string;
		pCurrentOpts.set_value(option, new_value, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
		win_set_window_text_utf8(GetDlgItem(hWnd, dialog), "None");
		changed = true;
	}

	return changed;
}

static bool SelectScreenShader(HWND hWnd, int slot)
{
	char filename[MAX_PATH];
	bool changed = false;
	char shader[32];
	int dialog = IDC_SCREEN_SHADER0 + slot;

	*filename = 0;
	snprintf(shader, WINUI_ARRAY_LENGTH(shader), "glsl_shader_screen%d", slot);

	if (CommonFileDialog(GetOpenFileName, filename, FILETYPE_SHADER_FILES, false))
	{
		char option[MAX_PATH];
		TCHAR *t_filename = win_wstring_from_utf8(filename);
		TCHAR *tempname = PathFindFileName(t_filename);
		PathRemoveExtension(tempname);
		char *optname = win_utf8_from_wstring(tempname);
		strcpy(option, optname);
		free(t_filename);
		free(optname);

		if (strcmp(option, pCurrentOpts.value(shader)))
		{
			std::string error_string;
			pCurrentOpts.set_value(shader, option, OPTION_PRIORITY_CMDLINE, error_string);
			assert(error_string.empty());
			win_set_window_text_utf8(GetDlgItem(hWnd, dialog), option);
			changed = true;
		}
	}

	return changed;
}

static bool ResetScreenShader(HWND hWnd, int slot)
{
	bool changed = false;
	const char *new_value = "none";
	char option[32];
	int dialog = IDC_SCREEN_SHADER0 + slot;

	snprintf(option, WINUI_ARRAY_LENGTH(option), "glsl_shader_screen%d", slot);

	if (strcmp(new_value, pCurrentOpts.value(option)))
	{
		std::string error_string;
		pCurrentOpts.set_value(option, new_value, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
		win_set_window_text_utf8(GetDlgItem(hWnd, dialog), "None");
		changed = true;
	}

	return changed;
}

static void UpdateMameShader(HWND hWnd, int slot, windows_options &opts)
{
	HWND hCtrl = GetDlgItem(hWnd, IDC_MAME_SHADER0 + slot);

	if (hCtrl)
	{
		char option[32];
		snprintf(option, WINUI_ARRAY_LENGTH(option), "glsl_shader_mame%d", slot);
		const char* value = opts.value(option);

		if (strcmp(value, "none") == 0)
			win_set_window_text_utf8(hCtrl, "None");
		else
			win_set_window_text_utf8(hCtrl, value);
	}
}

static void UpdateScreenShader(HWND hWnd, int slot, windows_options &opts)
{
	HWND hCtrl = GetDlgItem(hWnd, IDC_SCREEN_SHADER0 + slot);

	if (hCtrl)
	{
		char option[32];
		snprintf(option, WINUI_ARRAY_LENGTH(option), "glsl_shader_screen%d", slot);
		const char* value = opts.value(option);

		if (strcmp(value, "none") == 0)
			win_set_window_text_utf8(hCtrl, "None");
		else
			win_set_window_text_utf8(hCtrl, value);
	}
}

static bool SelectCheatFile(HWND hWnd)
{
	char filename[MAX_PATH];
	bool changed = false;

	*filename = 0;

	if (CommonFileDialog(GetOpenFileName, filename, FILETYPE_CHEAT_FILES, false))
	{
		char option[MAX_PATH];
		char optvalue[MAX_PATH];
		TCHAR *t_filename = win_wstring_from_utf8(filename);
		TCHAR *t_cheatopt = win_wstring_from_utf8(filename);
		TCHAR *tempname = PathFindFileName(t_filename);
		PathRemoveExtension(tempname);
		PathRemoveExtension(t_cheatopt);
		char *optname = win_utf8_from_wstring(tempname);
		char *cheatopt = win_utf8_from_wstring(t_cheatopt);
		strcpy(option, optname);
		strcpy(optvalue, cheatopt);
		free(t_filename);
		free(t_cheatopt);
		free(optname);
		free(cheatopt);

		if (strcmp(optvalue, pCurrentOpts.value(OPTION_CHEATPATH)))
		{
			std::string error_string;
			pCurrentOpts.set_value(OPTION_CHEATPATH, optvalue, OPTION_PRIORITY_CMDLINE, error_string);
			assert(error_string.empty());
			win_set_window_text_utf8(GetDlgItem(hWnd, IDC_CHEATFILE), option);
			changed = true;
		}
	}

	return changed;
}

static bool ResetCheatFile(HWND hWnd)
{
	bool changed = false;
	const char *new_value = "cheat";

	if (strcmp(new_value, pCurrentOpts.value(OPTION_CHEATPATH)))
	{
		std::string error_string;
		pCurrentOpts.set_value(OPTION_CHEATPATH, new_value, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
		win_set_window_text_utf8(GetDlgItem(hWnd, IDC_CHEATFILE), "Default");
		changed = true;
	}

	return changed;
}

static bool ChangeJoystickMap(HWND hWnd)
{
	bool changed = false;
	char joymap[90];

	win_get_window_text_utf8(GetDlgItem(hWnd, IDC_JOYSTICKMAP), joymap, WINUI_ARRAY_LENGTH(joymap));

	if (strcmp(joymap, pCurrentOpts.value(OPTION_JOYSTICK_MAP)))
	{
		std::string error_string;
		pCurrentOpts.set_value(OPTION_JOYSTICK_MAP, joymap, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
		changed = true;
	}

	return changed;
}

static bool ResetJoystickMap(HWND hWnd)
{
	bool changed = false;
	const char *new_value = "auto";

	if (strcmp(new_value, pCurrentOpts.value(OPTION_JOYSTICK_MAP)))
	{
		std::string error_string;
		pCurrentOpts.set_value(OPTION_JOYSTICK_MAP, new_value, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
		win_set_window_text_utf8(GetDlgItem(hWnd, IDC_JOYSTICKMAP), new_value);
		changed = true;
	}

	return changed;
}

static bool SelectLUAScript(HWND hWnd)
{
	char filename[MAX_PATH];
	bool changed = false;

	*filename = 0;

	if (CommonFileDialog(GetOpenFileName, filename, FILETYPE_LUASCRIPT_FILES, false))
	{
		char option[MAX_PATH];
		char script[MAX_PATH];
		TCHAR *t_filename = win_wstring_from_utf8(filename);
		TCHAR *tempname = PathFindFileName(t_filename);
		char *optvalue = win_utf8_from_wstring(tempname);
		strcpy(script, optvalue);
		PathRemoveExtension(tempname);
		char *optname = win_utf8_from_wstring(tempname);
		strcpy(option, optname);
		free(t_filename);
		free(optname);
		free(optvalue);

		if (strcmp(script, pCurrentOpts.value(OPTION_AUTOBOOT_SCRIPT)))
		{
			std::string error_string;
			pCurrentOpts.set_value(OPTION_AUTOBOOT_SCRIPT, script, OPTION_PRIORITY_CMDLINE, error_string);
			assert(error_string.empty());
			win_set_window_text_utf8(GetDlgItem(hWnd, IDC_LUASCRIPT), option);
			changed = true;
		}
	}

	return changed;
}

static bool ResetLUAScript(HWND hWnd)
{
	bool changed = false;
	const char *new_value = "";

	if (strcmp(new_value, pCurrentOpts.value(OPTION_AUTOBOOT_SCRIPT)))
	{
		std::string error_string;
		pCurrentOpts.set_value(OPTION_AUTOBOOT_SCRIPT, new_value, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
		win_set_window_text_utf8(GetDlgItem(hWnd, IDC_LUASCRIPT), "None");
		changed = true;
	}

	return changed;
}

static bool SelectPlugins(HWND hWnd)
{
	bool changed = false;
	bool already_enabled = false;
	int index = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_SELECT_PLUGIN));

	if (index == CB_ERR)
		return changed;

	const char *value = pCurrentOpts.value(OPTION_PLUGIN);
	const char *new_value = (const char*)ComboBox_GetItemData(GetDlgItem(hWnd, IDC_SELECT_PLUGIN), index);
	char *token = NULL;
	char buffer[2048];
	char plugins[256][32];
	int num_plugins = 0;

	strcpy(buffer, value);
	token = strtok(buffer, ",");

	if (token == NULL)
	{
		strcpy(plugins[num_plugins], buffer);
		num_plugins = 1;
	}
	else
	{
		while (token != NULL)
		{
			strcpy(plugins[num_plugins], token);
			num_plugins++;
			token = strtok(NULL, ",");
		}
	}

	if (strcmp(value, "") == 0)
	{
		std::string error_string;
		pCurrentOpts.set_value(OPTION_PLUGIN, new_value, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
		win_set_window_text_utf8(GetDlgItem(hWnd, IDC_PLUGIN), new_value);
		changed = true;
		(void)ComboBox_SetCurSel(GetDlgItem(hWnd, IDC_SELECT_PLUGIN), -1);
		return changed;	
	}
	
	for (int i = 0; i < num_plugins; i++)
	{
		if (strcmp(new_value, plugins[i]) == 0)
		{
			already_enabled = true;
			break;
		}
	}

	if (!already_enabled)
	{
		char new_option[256];
		snprintf(new_option, WINUI_ARRAY_LENGTH(new_option), "%s,%s", value, new_value);
		std::string error_string;
		pCurrentOpts.set_value(OPTION_PLUGIN, new_option, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
		win_set_window_text_utf8(GetDlgItem(hWnd, IDC_PLUGIN), new_option);
		changed = true;
	}

	(void)ComboBox_SetCurSel(GetDlgItem(hWnd, IDC_SELECT_PLUGIN), -1);
	return changed;
}

static bool ResetPlugins(HWND hWnd)
{
	bool changed = false;
	const char *new_value = "";

	if (strcmp(new_value, pCurrentOpts.value(OPTION_PLUGIN)))
	{
		std::string error_string;
		pCurrentOpts.set_value(OPTION_PLUGIN, new_value, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
		win_set_window_text_utf8(GetDlgItem(hWnd, IDC_PLUGIN), "None");
		changed = true;
	}

	(void)ComboBox_SetCurSel(GetDlgItem(hWnd, IDC_SELECT_PLUGIN), -1);
	return changed;
}

static bool SelectBGFXChains(HWND hWnd)
{
	char filename[MAX_PATH];
	bool changed = false;

	*filename = 0;

	if (CommonFileDialog(GetOpenFileName, filename, FILETYPE_BGFX_FILES, false))
	{
		char option[MAX_PATH];
		TCHAR *t_filename = win_wstring_from_utf8(filename);
		TCHAR *tempname = PathFindFileName(t_filename);
		PathRemoveExtension(tempname);
		char *optname = win_utf8_from_wstring(tempname);
		strcpy(option, optname);
		free(t_filename);
		free(optname);

		if (strcmp(option, pCurrentOpts.value(OSDOPTION_BGFX_SCREEN_CHAINS)))
		{
			std::string error_string;
			pCurrentOpts.set_value(OSDOPTION_BGFX_SCREEN_CHAINS, option, OPTION_PRIORITY_CMDLINE, error_string);
			assert(error_string.empty());
			win_set_window_text_utf8(GetDlgItem(hWnd, IDC_BGFX_CHAINS), option);
			changed = true;
		}
	}

	return changed;
}

static bool ResetBGFXChains(HWND hWnd)
{
	bool changed = false;
	const char *new_value = "default";

	if (strcmp(new_value, pCurrentOpts.value(OSDOPTION_BGFX_SCREEN_CHAINS)))
	{
		std::string error_string;
		pCurrentOpts.set_value(OSDOPTION_BGFX_SCREEN_CHAINS, new_value, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
		win_set_window_text_utf8(GetDlgItem(hWnd, IDC_BGFX_CHAINS), "Default");
		changed = true;
	}

	return changed;
}

//mamefx: for coloring of changed elements
static void DisableVisualStyles(HWND hDlg)
{
	/* Display */
	SetWindowTheme(GetDlgItem(hDlg, IDC_WINDOWED), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_MAXIMIZE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_KEEPASPECT), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_THROTTLE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FLIPY), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FLIPX), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_VIDEO_MODE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_D3D_FILTER), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_WIDESTRETCH), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_UNEVENSTRETCH), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_UNEVENSTRETCHX), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_UNEVENSTRETCHY), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_AUTOSTRETCHXY), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_INTOVERSCAN), L" ", L" ");
	/* Advanced */
	SetWindowTheme(GetDlgItem(hDlg, IDC_TRIPLE_BUFFER), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SYNCREFRESH), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_WAITVSYNC), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_REFRESHSPEED), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_HLSL_ON), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_AUTOFRAMESKIP), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FRAMESKIP), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_EFFECT), L" ", L" ");
	/* Screen */
	SetWindowTheme(GetDlgItem(hDlg, IDC_VIEW), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SIZES), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_REFRESH), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SWITCHRES), L" ", L" ");		
	/* OpenGL - BGFX */
	SetWindowTheme(GetDlgItem(hDlg, IDC_GLSLPOW), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_GLSLTEXTURE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_GLSLVBO), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_GLSLPBO), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_GLSL), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_GLSLFILTER), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_GLSLSYNC), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_MAME_SHADER0), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_MAME_SHADER1), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_MAME_SHADER2), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_MAME_SHADER3), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_MAME_SHADER4), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SCREEN_SHADER0), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SCREEN_SHADER1), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SCREEN_SHADER2), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SCREEN_SHADER3), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SCREEN_SHADER4), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_BGFX_CHAINS), L" ", L" ");
	/* Sound */
	SetWindowTheme(GetDlgItem(hDlg, IDC_SOUND_MODE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SAMPLES), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SAMPLERATE), L" ", L" ");
	/* Controllers */
	SetWindowTheme(GetDlgItem(hDlg, IDC_USE_MOUSE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_STEADYKEY), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_JOYSTICK), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_DEFAULT_INPUT), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_LIGHTGUN), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_RELOAD), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_DUAL_LIGHTGUN), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_MULTIKEYBOARD), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_MULTIMOUSE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_JOYSTICKMAP), L" ", L" ");
	/* Controller Mapping */
	SetWindowTheme(GetDlgItem(hDlg, IDC_PADDLE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_ADSTICK), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_PEDAL), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_MOUSE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_DIAL), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_TRACKBALL), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_LIGHTGUNDEVICE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_POSITIONAL), L" ", L" ");
	/* Miscellaneous */
	SetWindowTheme(GetDlgItem(hDlg, IDC_CHEAT), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_DEBUG), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_LOGERROR), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SLEEP), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SKIP_GAME_INFO), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_ENABLE_AUTOSAVE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_COINLOCKOUT), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_CONFIRM_QUIT), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_ENABLE_MOUSE_UI), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_BIOS), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_BACKDROPS), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_BEZELS), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_OVERLAYS), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_CPANELS), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_MARQUEES), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_ARTWORK_CROP), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_CHEATFILE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_LANGUAGE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_LUASCRIPT), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_PLUGINS), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_PLUGIN), L" ", L" ");
	/* Snap/Movie/Playback */
	SetWindowTheme(GetDlgItem(hDlg, IDC_SNAPVIEW), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SNAPNAME), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SNAPBILINEAR), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SNAPBURNIN), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_EXIT_PLAYBACK), L" ", L" ");
	/* Not Working for now, disabled for future FIX!! */
	SetWindowTheme(GetDlgItem(hDlg, IDC_ROTATE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SCREEN), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_ASPECT), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_ASPECTRATION), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_ASPECTRATIOD), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SCREENSELECT), L" ", L" ");	
	SetWindowTheme(GetDlgItem(hDlg, IDC_SNAPSIZE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SNAPSIZEWIDTH), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SNAPSIZEHEIGHT), L" ", L" ");
}
