// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include "winui.h"

/***************************************************************************
    Internal function prototypes
 ***************************************************************************/

static void LoadInterfaceFile(winui_options &opts, const std::string &filename);
static void SaveInterfaceFile(winui_options &opts, const std::string &filename);
static void LoadInternalUIFile(ui_options &opts, const std::string &filename);
static void SaveInternalUIFile(ui_options &opts, const std::string &filename);
static void LoadPluginsFile(plugin_options &opts, const std::string &filename);
static void SavePluginsFile(plugin_options &opts, const std::string &filename);
static void LoadOptionsFile(windows_options &opts, const std::string &filename);
static void LoadOptionsStartup(windows_options &opts, const std::string &filename);
static void SaveOptionsFile(windows_options &opts, const std::string &filename);
static void LoadOptionsAndInterface(void);
static void CusColorEncodeString(const COLORREF *value, char* str);
static void CusColorDecodeString(const char* str, COLORREF *value);
static void SplitterEncodeString(const int *value, char* str);
static void SplitterDecodeString(const char *str, int *value);
static void FontEncodeString(const LOGFONT *f, char *str);
static void FontDecodeString(const char* str, LOGFONT *f);
static void TabFlagsEncodeString(int data, char *str);
static void TabFlagsDecodeString(const char *str, int *data);
static DWORD DecodeFolderFlags(const char *buf);
static const char * EncodeFolderFlags(DWORD value);
static void ParseIniFile(windows_options &opts, const char *name);

/***************************************************************************
    Internal variables
 ***************************************************************************/

static bool RequiredDriverCacheStatus = false;
static winui_options winui_opts;			// INTERFACE.INI options
static gamelist_options game_opts;			// GAMELIST.INI options
static windows_options core_opts;			// MAME.INI default options
static windows_options save_opts;			// MAME.INI current options
static ui_options ui_opts;					// UI.INI options
static plugin_options plugin_opts;			// PLUGIN.INI options

// UI options in INTERFACE.INI
const options_entry winui_options::s_option_entries[] =
{
	// UI options
	{ NULL,									NULL,       OPTION_HEADER, "APPLICATION VERSION" },
	{ MUIOPTION_VERSION,					"",         OPTION_STRING, NULL },

	{ NULL,									NULL,       OPTION_HEADER, "DISPLAY STATE OPTIONS" },
	{ MUIOPTION_DEFAULT_GAME,				"puckman",  OPTION_STRING, NULL },
	{ MUIOPTION_DEFAULT_FOLDER_ID,			"0",        OPTION_INTEGER, NULL },
	{ MUIOPTION_SHOW_IMAGE_SECTION,			"1",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_CURRENT_TAB,				"0",        OPTION_STRING, NULL },
	{ MUIOPTION_SHOW_TOOLBAR,				"1",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_SHOW_STATUS_BAR,			"1",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_HIDE_FOLDERS,				"",         OPTION_STRING, NULL },
	{ MUIOPTION_SHOW_FOLDER_SECTION,		"1",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_SHOW_TABS,					"1",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_HIDE_TABS,					"",         OPTION_STRING, NULL },
	{ MUIOPTION_HISTORY_TAB,				"0",        OPTION_INTEGER, NULL },
	{ MUIOPTION_SORT_COLUMN,				"0",        OPTION_INTEGER, NULL },
	{ MUIOPTION_SORT_REVERSED,				"0",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_WINDOW_X,					"0",        OPTION_INTEGER, NULL },
	{ MUIOPTION_WINDOW_Y,					"0",        OPTION_INTEGER, NULL },
	{ MUIOPTION_WINDOW_WIDTH,				"1150",     OPTION_INTEGER, NULL },
	{ MUIOPTION_WINDOW_HEIGHT,				"639",      OPTION_INTEGER, NULL },
	{ MUIOPTION_WINDOW_STATE,				"1",        OPTION_INTEGER, NULL },
	{ MUIOPTION_LIST_COLOR,					"0,0,0",    OPTION_INTEGER, NULL },
	{ MUIOPTION_HISTORY_COLOR,				"0,0,0",    OPTION_INTEGER, NULL },
	{ MUIOPTION_TREE_COLOR,					"0,0,0",    OPTION_INTEGER, NULL },
	{ MUIOPTION_TREEBG_COLOR,				"255,255,255", OPTION_INTEGER, NULL },
	{ MUIOPTION_LISTBG_COLOR,				"255,255,255", OPTION_INTEGER, NULL },
	{ MUIOPTION_HISTORYBG_COLOR,			"255,255,255", OPTION_INTEGER, NULL },
	{ MUIOPTION_CUSTOM_COLOR,				"0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0", OPTION_STRING, NULL },
	{ MUIOPTION_LIST_MODE,					"0",        OPTION_INTEGER, NULL },
	{ MUIOPTION_SPLITTERS,					"150,498",  OPTION_STRING, NULL },
	{ MUIOPTION_GUI_FONT,					"-11,0,0,0,400,0,0,0,0,3,2,1,34,Tahoma", OPTION_STRING, NULL },
	{ MUIOPTION_LIST_FONT,					"-11,0,0,0,400,0,0,0,0,3,2,1,34,Tahoma", OPTION_STRING, NULL },
	{ MUIOPTION_HISTORY_FONT,				"-11,0,0,0,400,0,0,0,0,3,2,1,34,Verdana", OPTION_STRING, NULL },
	{ MUIOPTION_TREE_FONT,					"-11,0,0,0,400,0,0,0,0,3,2,1,34,Tahoma", OPTION_STRING, NULL },
	{ MUIOPTION_COLUMN_WIDTHS,				"320,80,110,140,50,160,60,70", OPTION_STRING, NULL },
	{ MUIOPTION_COLUMN_ORDER,				"0,3,4,6,7,5,1,2", OPTION_STRING, NULL },
	{ MUIOPTION_COLUMN_SHOWN,				"1,1,1,1,1,1,1,1", OPTION_STRING, NULL },

	{ NULL,									NULL,       OPTION_HEADER, "INTERFACE OPTIONS" },
	{ MUIOPTION_NOROMS_GAMES,				"1",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_TRAY_ICON,					"0",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_EXIT_DIALOG,				"1",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_JOYSTICK_IN_INTERFACE,		"1",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_INHERIT_FILTER,				"0",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_USE_BROKEN_ICON,			"0",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_ENABLE_INDENT,				"0",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_ENABLE_FASTAUDIT,			"0",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_ENABLE_SEVENZIP,			"0",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_STRETCH_SCREENSHOT_LARGER,	"0",        OPTION_BOOLEAN, NULL },
	{ MUIOPTION_CYCLE_SCREENSHOT,			"0",        OPTION_INTEGER, NULL },
	{ MUIOPTION_SCREENSHOT_BORDER_SIZE,		"0",        OPTION_INTEGER, NULL },
	{ MUIOPTION_SCREENSHOT_BORDER_COLOR,	"-1",       OPTION_INTEGER, NULL },

	{ NULL,									NULL,       OPTION_HEADER, "SEARCH PATH OPTIONS" },
	{ MUIOPTION_FLYER_DIRECTORY,			"flyers",   OPTION_STRING, NULL },
	{ MUIOPTION_CABINET_DIRECTORY,			"cabinets", OPTION_STRING, NULL },
	{ MUIOPTION_MARQUEE_DIRECTORY,			"marquees", OPTION_STRING, NULL },
	{ MUIOPTION_TITLE_DIRECTORY,			"titles",   OPTION_STRING, NULL },
	{ MUIOPTION_CPANEL_DIRECTORY,			"cpanel",   OPTION_STRING, NULL },
	{ MUIOPTION_PCB_DIRECTORY,				"pcb",      OPTION_STRING, NULL },
	{ MUIOPTION_SCORES_DIRECTORY,			"scores",   OPTION_STRING, NULL },
	{ MUIOPTION_BOSSES_DIRECTORY,			"bosses",   OPTION_STRING, NULL },
	{ MUIOPTION_VERSUS_DIRECTORY,			"versus",   OPTION_STRING, NULL },
	{ MUIOPTION_ENDS_DIRECTORY,				"ends",     OPTION_STRING, NULL },
	{ MUIOPTION_GAMEOVER_DIRECTORY,			"gameover", OPTION_STRING, NULL },
	{ MUIOPTION_HOWTO_DIRECTORY,			"howto",    OPTION_STRING, NULL },
	{ MUIOPTION_SELECT_DIRECTORY,			"select",   OPTION_STRING, NULL },
	{ MUIOPTION_LOGO_DIRECTORY,				"logo",     OPTION_STRING, NULL },
	{ MUIOPTION_ARTWORK_DIRECTORY,			"artpreview", OPTION_STRING, NULL },
	{ MUIOPTION_FOLDER_DIRECTORY,			"folders",  OPTION_STRING, NULL },
	{ MUIOPTION_ICONS_DIRECTORY,			"icons",    OPTION_STRING, NULL },
	{ MUIOPTION_MOVIES_DIRECTORY,			"movies",   OPTION_STRING, NULL },
	{ MUIOPTION_AUDIO_DIRECTORY,			"audio",    OPTION_STRING, NULL },
	{ MUIOPTION_GUI_DIRECTORY,				"gui",      OPTION_STRING, NULL },
	{ MUIOPTION_DATS_DIRECTORY,				"dats",     OPTION_STRING, NULL },

	{ NULL,									NULL,       OPTION_HEADER, "NAVIGATION JOYSTICK CODES" },
	{ MUIOPTION_UI_JOY_UP,					"1,1,1,1",  OPTION_STRING, NULL },
	{ MUIOPTION_UI_JOY_DOWN,				"1,1,1,2",  OPTION_STRING, NULL },
	{ MUIOPTION_UI_JOY_LEFT,				"1,1,2,1",  OPTION_STRING, NULL },
	{ MUIOPTION_UI_JOY_RIGHT,				"1,1,2,2",  OPTION_STRING, NULL },
	{ MUIOPTION_UI_JOY_START,				"1,0,1,0",  OPTION_STRING, NULL },
	{ MUIOPTION_UI_JOY_PGUP,				"2,1,2,1",  OPTION_STRING, NULL },
	{ MUIOPTION_UI_JOY_PGDWN,				"2,1,2,2",  OPTION_STRING, NULL },
	{ MUIOPTION_UI_JOY_HOME,				"0,0,0,0",  OPTION_STRING, NULL },
	{ MUIOPTION_UI_JOY_END,					"0,0,0,0",  OPTION_STRING, NULL },
	{ MUIOPTION_UI_JOY_SS_CHANGE,			"2,0,3,0",  OPTION_STRING, NULL },
	{ MUIOPTION_UI_JOY_HISTORY_UP,			"2,0,4,0",  OPTION_STRING, NULL },
	{ MUIOPTION_UI_JOY_HISTORY_DOWN,		"2,0,1,0",  OPTION_STRING, NULL },
	{ NULL }
};

static const options_entry filterOptions[] =
{
	{ "_filters",                           "0",        OPTION_INTEGER, NULL },
	{ NULL }
};

static const char *const image_tabs_long_name[MAX_TAB_TYPES] =
{
	"In Game",
	"Title",
	"Scores",
	"How To",
	"Select",
	"Versus",
	"Boss",
	"End",
	"Game Over",
	"Logo",
	"Artwork",
	"Flyer",
	"Cabinet",
	"Marquee",
	"Control Panel",
	"PCB",
	"Info"
};

static const char *const image_tabs_short_name[MAX_TAB_TYPES] =
{
	"ingame",
	"title",
	"scores",
	"howto",
	"select",
	"versus",
	"boss",
	"end",
	"gameover",
	"logo",
	"artpreview",
	"flyer",
	"cabinet",
	"marquee",
	"cpanel",
	"pcb",
	"history"
};

/***************************************************************************
    External functions
 ***************************************************************************/
winui_options::winui_options()
{
	add_entries(s_option_entries);
}

gamelist_options::gamelist_options()
{
	m_total = driver_list::total();
	m_list.reserve(m_total);
	driver_options option = { -1, -1, 0, 0 };

	for (int i = 0; i < m_total; i++)
		m_list[i] = option;
}

void gamelist_options::add_entries(void)
{
	options_entry entry[2] = { { 0 }, { 0 } };
	entry[0].defvalue    = "-1,-1";
	entry[0].flags       = OPTION_STRING;
	entry[0].description = nullptr;

	for (int i = 0; i < m_total; i++)
	{
		entry[0].name = GetDriverGameName(i);
		m_info.add_entries(entry);
	}
}

osd_file::error gamelist_options::load_options(const std::string &filename)
{
	osd_file::error filerr;
	emu_file file(OPEN_FLAG_READ);

	filerr = file.open(filename);

	if (filerr == osd_file::error::NONE)
	{
		std::string error_string;
		m_info.parse_ini_file((util::core_file&)file, OPTION_PRIORITY_CMDLINE, OPTION_PRIORITY_CMDLINE, error_string);
		file.close();
	}

	load_settings();
	return filerr;
}

osd_file::error gamelist_options::save_options(const std::string &filename)
{
	osd_file::error filerr;
	emu_file file(OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS);

	save_settings();
	std::string inistring = std::string("\n#\n# GAMELIST CACHED VALUES\n#\n").append(m_info.output_ini().c_str());
	filerr = file.open(filename);

	if (filerr == osd_file::error::NONE)
	{
		file.puts(inistring.c_str());
		file.close();
	}

	return filerr;
}

void gamelist_options::load_settings(void)
{
	std::string value_str;

	for (int i = 0; i < m_total; i++)
	{
		value_str.assign(m_info.value(GetDriverGameName(i)));

		if (!value_str.empty())
			load_settings(value_str.c_str(), i);
	}
}
	
void gamelist_options::save_settings(void)
{
	std::string value_str;
	std::string error_string;

	for (int i = 0; i < m_total; i++)
	{
		value_str = util::string_format("%d,%d,%d,%d", m_list[i].rom, m_list[i].cache, m_list[i].play_count, m_list[i].play_time);
		m_info.set_value(GetDriverGameName(i), value_str.c_str(), OPTION_PRIORITY_CMDLINE, error_string);
	}
}
	
void gamelist_options::load_settings(const char *str, int index)
{
	string_iterator value_str(str);
	int value_int;

	for (int i = 0; i < 4; i++)
	{
		if (value_str.next(','))
		{
			if (value_str && (sscanf(value_str.c_str(), "%d", &value_int) == 1))
			{
				switch (i)
				{
					case 0:  m_list[index].rom        = value_int;  break;
					case 1:  m_list[index].cache      = value_int;  break;
					case 2:  m_list[index].play_count = value_int;  break;
					case 3:  m_list[index].play_time  = value_int;  break;
				}
			}
		}
	}
}

windows_options & MameUIGlobal(void)
{
	return core_opts;
}

void OptionsInit(void)
{
	// setup our INI folder
	SetIniDir("ini");
	// gamelist creation
	game_opts.add_entries();
	// now load the options and interface settings
	LoadOptionsAndInterface();
}

const char * GetImageTabLongName(int tab_index)
{
	assert(tab_index >= 0);
	assert(tab_index < WINUI_ARRAY_LENGTH(image_tabs_long_name));
	return image_tabs_long_name[tab_index];
}

const char * GetImageTabShortName(int tab_index)
{
	assert(tab_index >= 0);
	assert(tab_index < WINUI_ARRAY_LENGTH(image_tabs_short_name));
	return image_tabs_short_name[tab_index];
}

//============================================================
//  OPTIONS WRAPPERS
//============================================================

static COLORREF options_get_color(winui_options &opts, const char *name)
{
	const char *value_str = opts.value(name);
	int r = 0;
	int g = 0;
	int b = 0;
	COLORREF value;

	if (sscanf(value_str, "%d,%d,%d", &r, &g, &b) == 3)
		value = RGB(r, g, b);
	else
		value = (COLORREF) - 1;

	return value;
}

static void options_set_color(winui_options &opts, const char *name, COLORREF value)
{
	char value_str[32];

	if (value == (COLORREF) - 1)
		snprintf(value_str, WINUI_ARRAY_LENGTH(value_str), "%d", (int)value);
	else
		snprintf(value_str, WINUI_ARRAY_LENGTH(value_str), "%d,%d,%d", (((int)value) >>  0) & 0xff,
			(((int)value) >>  8) & 0xff, (((int)value) >> 16) & 0xff);

	std::string error_string;
	opts.set_value(name, value_str, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

static COLORREF options_get_color_default(winui_options &opts, const char *name, int default_color)
{
	COLORREF value = options_get_color(opts, name);

	if (value == (COLORREF) - 1)
		value = GetSysColor(default_color);

	return value;
}

static void options_set_color_default(winui_options &opts, const char *name, COLORREF value, int default_color)
{
	if (value == GetSysColor(default_color))
		options_set_color(winui_opts, name, (COLORREF) - 1);
	else
		options_set_color(winui_opts, name, value);
}

//============================================================
//  OPTIONS CALLS
//============================================================

void SetEnableDatafiles(bool value)
{
	std::string error_string;
	ui_opts.set_value(OPTION_DATS_ENABLED, value, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetEnableDatafiles(void)
{
	return ui_opts.bool_value(OPTION_DATS_ENABLED);
}

void SetSkipBiosMenu(bool value)
{
	std::string error_string;
	ui_opts.set_value(OPTION_SKIP_BIOS_MENU, value, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetSkipBiosMenu(void)
{
	return ui_opts.bool_value(OPTION_SKIP_BIOS_MENU);
}

void SetViewMode(int val)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_LIST_MODE, val, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

int GetViewMode(void)
{
	return winui_opts.int_value(MUIOPTION_LIST_MODE);
}

void SetEnableIndent(bool value)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_ENABLE_INDENT, value, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetEnableIndent(void)
{
	return winui_opts.bool_value(MUIOPTION_ENABLE_INDENT);
}

void SetEnableFastAudit(bool value)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_ENABLE_FASTAUDIT, value, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetEnableFastAudit(void)
{
	return winui_opts.bool_value(MUIOPTION_ENABLE_FASTAUDIT);
}

void SetEnableSevenZip(bool value)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_ENABLE_SEVENZIP, value, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetEnableSevenZip(void)
{
	return winui_opts.bool_value(MUIOPTION_ENABLE_SEVENZIP);
}

void SetDisplayNoRomsGames(bool value)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_NOROMS_GAMES, value, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetDisplayNoRomsGames(void)
{
	return winui_opts.bool_value(MUIOPTION_NOROMS_GAMES);
}

void SetExitDialog(bool value)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_EXIT_DIALOG, value, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetExitDialog(void)
{
	return winui_opts.bool_value(MUIOPTION_EXIT_DIALOG);
}

void SetMinimizeTrayIcon(bool value)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_TRAY_ICON, value, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetMinimizeTrayIcon(void)
{
	return winui_opts.bool_value(MUIOPTION_TRAY_ICON);
}

void SetJoyGUI(bool use_joygui)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_JOYSTICK_IN_INTERFACE, use_joygui, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetJoyGUI(void)
{
	return winui_opts.bool_value( MUIOPTION_JOYSTICK_IN_INTERFACE);
}

void SetCycleScreenshot(int cycle_screenshot)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_CYCLE_SCREENSHOT, cycle_screenshot, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

int GetCycleScreenshot(void)
{
	return winui_opts.int_value(MUIOPTION_CYCLE_SCREENSHOT);
}

void SetStretchScreenShotLarger(bool stretch)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_STRETCH_SCREENSHOT_LARGER, stretch, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetStretchScreenShotLarger(void)
{
	return winui_opts.bool_value( MUIOPTION_STRETCH_SCREENSHOT_LARGER);
}

void SetScreenshotBorderSize(int size)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_SCREENSHOT_BORDER_SIZE, size, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

int GetScreenshotBorderSize(void)
{
	return winui_opts.int_value(MUIOPTION_SCREENSHOT_BORDER_SIZE);
}

void SetScreenshotBorderColor(COLORREF uColor)
{
	options_set_color_default(winui_opts, MUIOPTION_SCREENSHOT_BORDER_COLOR, uColor, COLOR_3DFACE);
}

COLORREF GetScreenshotBorderColor(void)
{
	return options_get_color_default(winui_opts, MUIOPTION_SCREENSHOT_BORDER_COLOR, COLOR_3DFACE);
}

void SetFilterInherit(bool inherit)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_INHERIT_FILTER, inherit, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetFilterInherit(void)
{
	return winui_opts.bool_value( MUIOPTION_INHERIT_FILTER);
}

void SetUseBrokenIcon(bool broken)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_USE_BROKEN_ICON, broken, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetUseBrokenIcon(void)
{
	return winui_opts.bool_value(MUIOPTION_USE_BROKEN_ICON);
}

void SetSavedFolderID(int val)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_DEFAULT_FOLDER_ID, val, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

int GetSavedFolderID(void)
{
	return winui_opts.int_value(MUIOPTION_DEFAULT_FOLDER_ID);
}

void SetShowScreenShot(bool val)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_SHOW_IMAGE_SECTION, val, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetShowScreenShot(void)
{
	return winui_opts.bool_value(MUIOPTION_SHOW_IMAGE_SECTION);
}

void SetShowFolderList(bool val)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_SHOW_FOLDER_SECTION, val, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetShowFolderList(void)
{
	return winui_opts.bool_value(MUIOPTION_SHOW_FOLDER_SECTION);
}

static void GetsShowFolderFlags(LPBITS bits)
{
	char s[1024];
	extern const FOLDERDATA g_folderData[];

	snprintf(s, WINUI_ARRAY_LENGTH(s), "%s", winui_opts.value(MUIOPTION_HIDE_FOLDERS));
	SetAllBits(bits, true);
	char *token = strtok(s,", \t");

	while (token != NULL)
	{
		for (int j = 0; g_folderData[j].m_lpTitle != NULL; j++)
		{
			if (strcmp(g_folderData[j].short_name,token) == 0)
			{
				ClearBit(bits, g_folderData[j].m_nFolderId);
				break;
			}
		}

		token = strtok(NULL, ", \t");
	}
}

bool GetShowFolder(int folder)
{
	LPBITS show_folder_flags = NewBits(MAX_FOLDERS);
	GetsShowFolderFlags(show_folder_flags);
	bool result = TestBit(show_folder_flags, folder);
	DeleteBits(show_folder_flags);
	return result;
}

void SetShowFolder(int folder, bool show)
{
	LPBITS show_folder_flags = NewBits(MAX_FOLDERS);
	int num_saved = 0;
	char str[1024];
	extern const FOLDERDATA g_folderData[];

	GetsShowFolderFlags(show_folder_flags);

	if (show)
		SetBit(show_folder_flags, folder);
	else
		ClearBit(show_folder_flags, folder);

	strcpy(str, "");

	// we save the ones that are NOT displayed, so we can add new ones
	// and upgraders will see them
	for (int i = 0; i < MAX_FOLDERS; i++)
	{
		if (TestBit(show_folder_flags, i) == false)
		{
			if (num_saved != 0)
				strcat(str,", ");

			for (int j = 0; g_folderData[j].m_lpTitle != NULL; j++)
			{
				if (g_folderData[j].m_nFolderId == i)
				{
					strcat(str,g_folderData[j].short_name);
					num_saved++;
					break;
				}
			}
		}
	}

	std::string error_string;
	winui_opts.set_value(MUIOPTION_HIDE_FOLDERS, str, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
	DeleteBits(show_folder_flags);
}

void SetShowStatusBar(bool val)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_SHOW_STATUS_BAR, val, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetShowStatusBar(void)
{
	return winui_opts.bool_value( MUIOPTION_SHOW_STATUS_BAR);
}

void SetShowTabCtrl (bool val)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_SHOW_TABS, val, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetShowTabCtrl (void)
{
	return winui_opts.bool_value( MUIOPTION_SHOW_TABS);
}

void SetShowToolBar(bool val)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_SHOW_TOOLBAR, val, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetShowToolBar(void)
{
	return winui_opts.bool_value( MUIOPTION_SHOW_TOOLBAR);
}

void SetCurrentTab(const char *shortname)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_CURRENT_TAB, shortname, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char *GetCurrentTab(void)
{
	return winui_opts.value( MUIOPTION_CURRENT_TAB);
}

void SetDefaultGame(const char *name)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_DEFAULT_GAME, name, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char *GetDefaultGame(void)
{
	return winui_opts.value( MUIOPTION_DEFAULT_GAME);
}

void SetWindowArea(const AREA *area)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_WINDOW_X, area->x, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
	winui_opts.set_value(MUIOPTION_WINDOW_Y, area->y, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
	winui_opts.set_value(MUIOPTION_WINDOW_WIDTH, area->width, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
	winui_opts.set_value(MUIOPTION_WINDOW_HEIGHT, area->height, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

void GetWindowArea(AREA *area)
{
	area->x = winui_opts.int_value(MUIOPTION_WINDOW_X);
	area->y = winui_opts.int_value(MUIOPTION_WINDOW_Y);
	area->width = winui_opts.int_value(MUIOPTION_WINDOW_WIDTH);
	area->height = winui_opts.int_value(MUIOPTION_WINDOW_HEIGHT);
}

void SetWindowState(int state)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_WINDOW_STATE, state, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

int GetWindowState(void)
{
	return winui_opts.int_value(MUIOPTION_WINDOW_STATE);
}

void SetCustomColor(int iIndex, COLORREF uColor)
{
	COLORREF custom_color[256];
	char buffer[256];
	const char *custom_color_string = winui_opts.value(MUIOPTION_CUSTOM_COLOR);

	CusColorDecodeString(custom_color_string, custom_color);
	custom_color[iIndex] = uColor;
	CusColorEncodeString(custom_color, buffer);
	std::string error_string;
	winui_opts.set_value(MUIOPTION_CUSTOM_COLOR, buffer, OPTION_PRIORITY_CMDLINE,error_string);
	assert(error_string.empty());
}

COLORREF GetCustomColor(int iIndex)
{
	COLORREF custom_color[256];
	const char *custom_color_string = winui_opts.value(MUIOPTION_CUSTOM_COLOR);

	CusColorDecodeString(custom_color_string, custom_color);

	if (custom_color[iIndex] == (COLORREF) - 1)
		return (COLORREF)RGB(0, 0, 0);

	return custom_color[iIndex];
}

void SetListFont(const LOGFONT *font)
{
	char font_string[256];

	FontEncodeString(font, font_string);
	std::string error_string;
	winui_opts.set_value(MUIOPTION_LIST_FONT, font_string, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

void GetGuiFont(LOGFONT *font)
{
	const char *font_string = winui_opts.value(MUIOPTION_GUI_FONT);

	FontDecodeString(font_string, font);
}

void GetListFont(LOGFONT *font)
{
	const char *font_string = winui_opts.value(MUIOPTION_LIST_FONT);

	FontDecodeString(font_string, font);
}

void SetHistoryFont(const LOGFONT *font)
{
	char font_string[256];

	FontEncodeString(font, font_string);
	std::string error_string;
	winui_opts.set_value(MUIOPTION_HISTORY_FONT, font_string, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

void GetHistoryFont(LOGFONT *font)
{
	const char *font_string = winui_opts.value(MUIOPTION_HISTORY_FONT);

	FontDecodeString(font_string, font);
}

void SetTreeFont(const LOGFONT *font)
{
	char font_string[256];

	FontEncodeString(font, font_string);
	std::string error_string;
	winui_opts.set_value(MUIOPTION_TREE_FONT, font_string, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

void GetTreeFont(LOGFONT *font)
{
	const char *font_string = winui_opts.value(MUIOPTION_TREE_FONT);

	FontDecodeString(font_string, font);
}

void SetListFontColor(COLORREF uColor)
{
	options_set_color_default(winui_opts, MUIOPTION_LIST_COLOR, uColor, COLOR_WINDOWTEXT);
}

COLORREF GetListFontColor(void)
{
	return options_get_color_default(winui_opts, MUIOPTION_LIST_COLOR, COLOR_WINDOWTEXT);
}

void SetHistoryFontColor(COLORREF uColor)
{
	options_set_color_default(winui_opts, MUIOPTION_HISTORY_COLOR, uColor, COLOR_WINDOWTEXT);
}

COLORREF GetHistoryFontColor(void)
{
	return options_get_color_default(winui_opts, MUIOPTION_HISTORY_COLOR, COLOR_WINDOWTEXT);
}

void SetTreeFontColor(COLORREF uColor)
{
	options_set_color_default(winui_opts, MUIOPTION_TREE_COLOR, uColor, COLOR_WINDOWTEXT);
}

COLORREF GetTreeFontColor(void)
{
	return options_get_color_default(winui_opts, MUIOPTION_TREE_COLOR, COLOR_WINDOWTEXT);
}

void SetFolderBgColor(COLORREF uColor)
{
	options_set_color_default(winui_opts, MUIOPTION_TREEBG_COLOR, uColor, COLOR_WINDOW);
}

COLORREF GetFolderBgColor(void)
{
	return options_get_color_default(winui_opts, MUIOPTION_TREEBG_COLOR, COLOR_WINDOW);
}

void SetHistoryBgColor(COLORREF uColor)
{
	options_set_color_default(winui_opts, MUIOPTION_HISTORYBG_COLOR, uColor, COLOR_WINDOW);
}

COLORREF GetHistoryBgColor(void)
{
	return options_get_color_default(winui_opts, MUIOPTION_HISTORYBG_COLOR, COLOR_WINDOW);
}

void SetListBgColor(COLORREF uColor)
{
	options_set_color_default(winui_opts, MUIOPTION_LISTBG_COLOR, uColor, COLOR_WINDOW);
}

COLORREF GetListBgColor(void)
{
	return options_get_color_default(winui_opts, MUIOPTION_LISTBG_COLOR, COLOR_WINDOW);
}

int GetShowTab(int tab)
{
	int show_tab_flags = 0;
	const char *show_tabs_string = winui_opts.value(MUIOPTION_HIDE_TABS);

	TabFlagsDecodeString(show_tabs_string, &show_tab_flags);

	return (show_tab_flags & (1 << tab)) != 0;
}

void SetShowTab(int tab, bool show)
{
	int show_tab_flags = 0;
	char buffer[256];
	const char *show_tabs_string = winui_opts.value(MUIOPTION_HIDE_TABS);

	TabFlagsDecodeString(show_tabs_string, &show_tab_flags);

	if (show)
		show_tab_flags |= 1 << tab;
	else
		show_tab_flags &= ~(1 << tab);

	TabFlagsEncodeString(show_tab_flags, buffer);
	std::string error_string;
	winui_opts.set_value(MUIOPTION_HIDE_TABS, buffer, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

// don't delete the last one
bool AllowedToSetShowTab(int tab, bool show)
{
	int show_tab_flags = 0;
	const char *show_tabs_string = winui_opts.value(MUIOPTION_HIDE_TABS);

	if (show == true)
		return true;

	TabFlagsDecodeString(show_tabs_string, &show_tab_flags);
	show_tab_flags &= ~(1 << tab);

	return show_tab_flags != 0;
}

int GetHistoryTab(void)
{
	return winui_opts.int_value(MUIOPTION_HISTORY_TAB);
}

void SetHistoryTab(int tab, bool show)
{
	std::string error_string;

	if (show)
		winui_opts.set_value(MUIOPTION_HISTORY_TAB, tab, OPTION_PRIORITY_CMDLINE, error_string);
	else
		winui_opts.set_value(MUIOPTION_HISTORY_TAB, TAB_NONE, OPTION_PRIORITY_CMDLINE, error_string);

	assert(error_string.empty());
}

void SetColumnWidths(int width[])
{
	char column_width_string[256];

	ColumnEncodeStringWithCount(width, column_width_string, COLUMN_MAX);
	std::string error_string;
	winui_opts.set_value(MUIOPTION_COLUMN_WIDTHS, column_width_string, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

void GetColumnWidths(int width[])
{
	const char *column_width_string = winui_opts.value(MUIOPTION_COLUMN_WIDTHS);

	ColumnDecodeStringWithCount(column_width_string, width, COLUMN_MAX);
}

void SetSplitterPos(int splitterId, int pos)
{
	if (splitterId < GetSplitterCount())
	{
		char buffer[256];
		const char *splitter_string = winui_opts.value(MUIOPTION_SPLITTERS);
		int *splitter = (int *)malloc(GetSplitterCount() * sizeof(*splitter));
		SplitterDecodeString(splitter_string, splitter);
		splitter[splitterId] = pos;
		SplitterEncodeString(splitter, buffer);
		std::string error_string;
		winui_opts.set_value(MUIOPTION_SPLITTERS, buffer, OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
		free(splitter);
	}
}

int GetSplitterPos(int splitterId)
{
	const char *splitter_string = winui_opts.value(MUIOPTION_SPLITTERS);

	int *splitter = (int *)malloc(GetSplitterCount() * sizeof(*splitter));
	SplitterDecodeString(splitter_string, splitter);

	if (splitterId < GetSplitterCount())
	{
		int value = splitter[splitterId];
		free(splitter);
		return value;
	}
	
	free(splitter);
	return -1; 	/* Error */
}

void SetColumnOrder(int order[])
{
	char column_order_string[256];

	ColumnEncodeStringWithCount(order, column_order_string, COLUMN_MAX);
	std::string error_string;
	winui_opts.set_value(MUIOPTION_COLUMN_ORDER, column_order_string, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

void GetColumnOrder(int order[])
{
	const char *column_order_string = winui_opts.value(MUIOPTION_COLUMN_ORDER);

	ColumnDecodeStringWithCount(column_order_string, order, COLUMN_MAX);
}

void SetColumnShown(int shown[])
{
	char column_shown_string[256];

	ColumnEncodeStringWithCount(shown, column_shown_string, COLUMN_MAX);
	std::string error_string;
	winui_opts.set_value(MUIOPTION_COLUMN_SHOWN, column_shown_string, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

void GetColumnShown(int shown[])
{
	const char *column_shown_string = winui_opts.value(MUIOPTION_COLUMN_SHOWN);

	ColumnDecodeStringWithCount(column_shown_string, shown, COLUMN_MAX);
}

void SetSortColumn(int column)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_SORT_COLUMN, column, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

int GetSortColumn(void)
{
	return winui_opts.int_value(MUIOPTION_SORT_COLUMN);
}

void SetSortReverse(bool reverse)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_SORT_REVERSED, reverse, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

bool GetSortReverse(void)
{
	return winui_opts.bool_value(MUIOPTION_SORT_REVERSED);
}

const char* GetRomDirs(void)
{
	return core_opts.value(OPTION_MEDIAPATH);
}

void SetRomDirs(const char* paths)
{
	std::string error_string;
	core_opts.set_value(OPTION_MEDIAPATH, paths, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetSampleDirs(void)
{
	return core_opts.value(OPTION_SAMPLEPATH);
}

void SetSampleDirs(const char* paths)
{
	std::string error_string;
	core_opts.set_value(OPTION_SAMPLEPATH, paths, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetIniDir(void)
{
	return core_opts.value(OPTION_INIPATH);
}

void SetIniDir(const char *path)
{
	std::string error_string;
	core_opts.set_value(OPTION_INIPATH, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetCtrlrDir(void)
{
	return core_opts.value(OPTION_CTRLRPATH);
}

void SetCtrlrDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OPTION_CTRLRPATH, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetCommentDir(void)
{
	return core_opts.value(OPTION_COMMENT_DIRECTORY);
}

void SetCommentDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OPTION_COMMENT_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetCfgDir(void)
{
	return core_opts.value(OPTION_CFG_DIRECTORY);
}

void SetCfgDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OPTION_CFG_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetGLSLDir(void)
{
	return 0;//core_opts.value(OSDOPTION_GLSLPATH);
}

void SetGLSLDir(const char* path)
{
//	std::string error_string;
//	core_opts.set_value(OSDOPTION_GLSLPATH, path, OPTION_PRIORITY_CMDLINE, error_string);
//	assert(error_string.empty());
}

const char* GetBGFXDir(void)
{
	return core_opts.value(OSDOPTION_BGFX_PATH);
}

void SetBGFXDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OSDOPTION_BGFX_PATH, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetPluginsDir(void)
{
	return core_opts.value(OPTION_PLUGINSPATH);
}

void SetPluginsDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OPTION_PLUGINSPATH, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetNvramDir(void)
{
	return core_opts.value(OPTION_NVRAM_DIRECTORY);
}

void SetNvramDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OPTION_NVRAM_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetInpDir(void)
{
	return core_opts.value(OPTION_INPUT_DIRECTORY);
}

void SetInpDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OPTION_INPUT_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetImgDir(void)
{
	return core_opts.value(OPTION_SNAPSHOT_DIRECTORY);
}

void SetImgDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OPTION_SNAPSHOT_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetStateDir(void)
{
	return core_opts.value(OPTION_STATE_DIRECTORY);
}

void SetStateDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OPTION_STATE_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetArtDir(void)
{
	return core_opts.value(OPTION_ARTPATH);
}

void SetArtDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OPTION_ARTPATH, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetFontDir(void)
{
	return core_opts.value(OPTION_FONTPATH);
}

void SetFontDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OPTION_FONTPATH, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetCrosshairDir(void)
{
	return core_opts.value(OPTION_CROSSHAIRPATH);
}

void SetCrosshairDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OPTION_CROSSHAIRPATH, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetLanguageDir(void)
{
	return core_opts.value(OPTION_LANGUAGEPATH);
}

void SetLanguageDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OPTION_LANGUAGEPATH, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetFlyerDir(void)
{
	return winui_opts.value(MUIOPTION_FLYER_DIRECTORY);
}

void SetFlyerDir(const char* path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_FLYER_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetCabinetDir(void)
{
	return winui_opts.value(MUIOPTION_CABINET_DIRECTORY);
}

void SetCabinetDir(const char* path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_CABINET_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetMarqueeDir(void)
{
	return winui_opts.value(MUIOPTION_MARQUEE_DIRECTORY);
}

void SetMarqueeDir(const char* path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_MARQUEE_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetTitlesDir(void)
{
	return winui_opts.value(MUIOPTION_TITLE_DIRECTORY);
}

void SetTitlesDir(const char* path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_TITLE_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetControlPanelDir(void)
{
	return winui_opts.value(MUIOPTION_CPANEL_DIRECTORY);
}

void SetControlPanelDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_CPANEL_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetPcbDir(void)
{
	return winui_opts.value(MUIOPTION_PCB_DIRECTORY);
}

void SetPcbDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_PCB_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetMoviesDir(void)
{
	return winui_opts.value(MUIOPTION_MOVIES_DIRECTORY);
}

void SetMoviesDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_MOVIES_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetVideoDir(void)
{
	return 0;//core_opts.value(OPTION_VIDEO_DIRECTORY);
}

void SetVideoDir(const char *path)
{
//	std::string error_string;
//	core_opts.set_value(OPTION_VIDEO_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
//	assert(error_string.empty());
}

const char * GetAudioDir(void)
{
	return winui_opts.value(MUIOPTION_AUDIO_DIRECTORY);
}

void SetAudioDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_AUDIO_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetGuiDir(void)
{
	return winui_opts.value(MUIOPTION_GUI_DIRECTORY);
}

void SetGuiDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_GUI_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetDatsDir(void)
{
	return winui_opts.value(MUIOPTION_DATS_DIRECTORY);
}

void SetDatsDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_DATS_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	ui_opts.set_value(OPTION_HISTORY_PATH, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetScoresDir(void)
{
	return winui_opts.value(MUIOPTION_SCORES_DIRECTORY);
}

void SetScoresDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_SCORES_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetBossesDir(void)
{
	return winui_opts.value(MUIOPTION_BOSSES_DIRECTORY);
}

void SetBossesDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_BOSSES_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetVersusDir(void)
{
	return winui_opts.value(MUIOPTION_VERSUS_DIRECTORY);
}

void SetVersusDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_VERSUS_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetEndsDir(void)
{
	return winui_opts.value(MUIOPTION_ENDS_DIRECTORY);
}

void SetEndsDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_ENDS_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetGameOverDir(void)
{
	return winui_opts.value(MUIOPTION_GAMEOVER_DIRECTORY);
}

void SetGameOverDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_GAMEOVER_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetHowToDir(void)
{
	return winui_opts.value(MUIOPTION_HOWTO_DIRECTORY);
}

void SetHowToDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_HOWTO_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetSelectDir(void)
{
	return winui_opts.value(MUIOPTION_SELECT_DIRECTORY);
}

void SetSelectDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_SELECT_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetLogoDir(void)
{
	return winui_opts.value(MUIOPTION_LOGO_DIRECTORY);
}

void SetLogoDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_LOGO_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetArtworkDir(void)
{
	return winui_opts.value(MUIOPTION_ARTWORK_DIRECTORY);
}

void SetArtworkDir(const char *path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_ARTWORK_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetHLSLDir(void)
{
	return core_opts.value(WINOPTION_HLSLPATH);
}

void SetHLSLDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(WINOPTION_HLSLPATH, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetDiffDir(void)
{
	return core_opts.value(OPTION_DIFF_DIRECTORY);
}

void SetDiffDir(const char* path)
{
	std::string error_string;
	core_opts.set_value(OPTION_DIFF_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetIconsDir(void)
{
	return winui_opts.value(MUIOPTION_ICONS_DIRECTORY);
}

void SetIconsDir(const char* path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_ICONS_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char* GetFolderDir(void)
{
	return winui_opts.value(MUIOPTION_FOLDER_DIRECTORY);
}

void SetFolderDir(const char* path)
{
	std::string error_string;
	winui_opts.set_value(MUIOPTION_FOLDER_DIRECTORY, path, OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

int GetRomAuditResults(int driver_index)
{
	return game_opts.rom(driver_index);
}

void SetRomAuditResults(int driver_index, int audit_results)
{
	game_opts.rom(driver_index, audit_results);
}

void IncrementPlayCount(int driver_index)
{
	int count = game_opts.play_count(driver_index);;

	game_opts.play_count(driver_index, count + 1);
}

int GetPlayCount(int driver_index)
{
	return game_opts.play_count(driver_index);
}

void ResetPlayCount(int driver_index)
{
	game_opts.play_count(driver_index, 0);
}

void IncrementPlayTime(int driver_index, int playtime)
{
	int time = game_opts.play_time(driver_index);

	game_opts.play_time(driver_index, time + playtime);
}

int GetPlayTime(int driver_index)
{
	return game_opts.play_time(driver_index);
}

void GetTextPlayTime(int driver_index, char *buf)
{
	char tmp[200];
	int temp = GetPlayTime(driver_index);

	int hour = temp / 3600;
	temp = temp - 3600 * hour;
	int minute = temp / 60;
	int second = temp - 60 * minute;
	snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "%d:%02d:%02d", hour, minute, second);
	strcpy(buf, tmp);
}

void ResetPlayTime(int driver_index)
{
	game_opts.play_time(driver_index, 0);
}

static int GetUIJoy(const char *option_name, int joycodeIndex)
{
	const char *joycodes_string = winui_opts.value(option_name);
	int joycodes[4];

	assert(0 <= joycodeIndex && joycodeIndex < 4);
	ColumnDecodeStringWithCount(joycodes_string, joycodes, WINUI_ARRAY_LENGTH(joycodes));
	return joycodes[joycodeIndex];
}

static void SetUIJoy(const char *option_name, int joycodeIndex, int val)
{
	const char *joycodes_string = winui_opts.value(option_name);
	int joycodes[4];
	char buffer[1024];

	assert(0 <= joycodeIndex && joycodeIndex < 4);
	ColumnDecodeStringWithCount(joycodes_string, joycodes, WINUI_ARRAY_LENGTH(joycodes));
	joycodes[joycodeIndex] = val;
	ColumnEncodeStringWithCount(joycodes, buffer, WINUI_ARRAY_LENGTH(joycodes));
	std::string error_string;
	winui_opts.set_value(option_name, buffer, OPTION_PRIORITY_CMDLINE,error_string);
	assert(error_string.empty());
}

int GetUIJoyUp(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_UP, joycodeIndex);
}

void SetUIJoyUp(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_UP, joycodeIndex, val);
}

int GetUIJoyDown(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_DOWN, joycodeIndex);
}

void SetUIJoyDown(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_DOWN, joycodeIndex, val);
}

int GetUIJoyLeft(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_LEFT, joycodeIndex);
}

void SetUIJoyLeft(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_LEFT, joycodeIndex, val);
}

int GetUIJoyRight(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_RIGHT, joycodeIndex);
}

void SetUIJoyRight(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_RIGHT, joycodeIndex, val);
}

int GetUIJoyStart(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_START, joycodeIndex);
}

void SetUIJoyStart(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_START, joycodeIndex, val);
}

int GetUIJoyPageUp(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_PGUP, joycodeIndex);
}

void SetUIJoyPageUp(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_PGUP, joycodeIndex, val);
}

int GetUIJoyPageDown(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_PGDWN, joycodeIndex);
}

void SetUIJoyPageDown(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_PGDWN, joycodeIndex, val);
}

int GetUIJoyHome(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_HOME, joycodeIndex);
}

void SetUIJoyHome(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_HOME, joycodeIndex, val);
}

int GetUIJoyEnd(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_END, joycodeIndex);
}

void SetUIJoyEnd(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_END, joycodeIndex, val);
}

int GetUIJoySSChange(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_SS_CHANGE, joycodeIndex);
}

void SetUIJoySSChange(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_SS_CHANGE, joycodeIndex, val);
}

int GetUIJoyHistoryUp(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_HISTORY_UP, joycodeIndex);
}

void SetUIJoyHistoryUp(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_HISTORY_UP, joycodeIndex, val);
}

int GetUIJoyHistoryDown(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_HISTORY_DOWN, joycodeIndex);
}

void SetUIJoyHistoryDown(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_HISTORY_DOWN, joycodeIndex, val);
}

/***************************************************************************
    Internal functions
 ***************************************************************************/

static void  CusColorEncodeString(const COLORREF *value, char* str)
{
	char tmpStr[256];

	snprintf(tmpStr, WINUI_ARRAY_LENGTH(tmpStr), "%u", (unsigned) value[0]);
	strcpy(str, tmpStr);

	for (int i = 1; i < 16; i++)
	{
		snprintf(tmpStr, WINUI_ARRAY_LENGTH(tmpStr), ",%u", (unsigned) value[i]);
		strcat(str, tmpStr);
	}
}

static void CusColorDecodeString(const char* str, COLORREF *value)
{
	char tmpStr[256];

	strcpy(tmpStr, str);
	char *p = tmpStr;

	for (int i = 0; p && i < 16; i++)
	{
		char *s = p;

		if ((p = strchr(s, ',')) != NULL && *p == ',')
		{
			*p = '\0';
			p++;
		}

		value[i] = atoi(s);
	}
}

void ColumnEncodeStringWithCount(const int *value, char *str, int count)
{
	char buffer[256];

	snprintf(buffer, WINUI_ARRAY_LENGTH(buffer),"%d", value[0]);
	strcpy(str, buffer);

    for (int i = 1; i < count; i++)
	{
		snprintf(buffer, WINUI_ARRAY_LENGTH(buffer),",%d", value[i]);
		strcat(str, buffer);
	}
}

void ColumnDecodeStringWithCount(const char* str, int *value, int count)
{
	char tmpStr[256];

	strcpy(tmpStr, str);
	char *p = tmpStr;

	for (int i = 0; p && i < count; i++)
	{
		char *s = p;

		if ((p = strchr(s, ',')) != NULL && *p == ',')
		{
			*p = '\0';
			p++;
		}

		value[i] = atoi(s);
	}
}

static void SplitterEncodeString(const int *value, char* str)
{
	char tmpStr[256];

	snprintf(tmpStr, WINUI_ARRAY_LENGTH(tmpStr), "%d", value[0]);
	strcpy(str, tmpStr);

	for (int i = 1; i < GetSplitterCount(); i++)
	{
		snprintf(tmpStr, WINUI_ARRAY_LENGTH(tmpStr), ",%d", value[i]);
		strcat(str, tmpStr);
	}
}

static void SplitterDecodeString(const char *str, int *value)
{
	char tmpStr[256];

	strcpy(tmpStr, str);
	char *p = tmpStr;

	for (int i = 0; p && i < GetSplitterCount(); i++)
	{
		char *s = p;

		if ((p = strchr(s, ',')) != NULL && *p == ',')
		{
			*p = '\0';
			p++;
		}

		value[i] = atoi(s);
	}
}

/* Parse the given comma-delimited string into a LOGFONT structure */
static void FontDecodeString(const char *str, LOGFONT *f)
{
	sscanf(str, "%li,%li,%li,%li,%li,%i,%i,%i,%i,%i,%i,%i,%i",
		&f->lfHeight,
		&f->lfWidth,
		&f->lfEscapement,
		&f->lfOrientation,
		&f->lfWeight,
		(int*)&f->lfItalic,
		(int*)&f->lfUnderline,
		(int*)&f->lfStrikeOut,
		(int*)&f->lfCharSet,
		(int*)&f->lfOutPrecision,
		(int*)&f->lfClipPrecision,
		(int*)&f->lfQuality,
		(int*)&f->lfPitchAndFamily);
	const char *ptr = strrchr(str, ',');

	if (ptr != NULL) 
	{
		TCHAR *t_ptr = win_wstring_from_utf8(ptr + 1);

		if(!t_ptr)
			return;

		_tcscpy(f->lfFaceName, t_ptr);
		free(t_ptr);
	}
}

/* Encode the given LOGFONT structure into a comma-delimited string */
static void FontEncodeString(const LOGFONT *f, char *str)
{
	char* utf8_FaceName = win_utf8_from_wstring(f->lfFaceName);
	char tmp[200];

	if(!utf8_FaceName)
		return;

	snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "%li,%li,%li,%li,%li,%i,%i,%i,%i,%i,%i,%i,%i,%s",
			f->lfHeight,
			f->lfWidth,
			f->lfEscapement,
			f->lfOrientation,
			f->lfWeight,
			f->lfItalic,
			f->lfUnderline,
			f->lfStrikeOut,
			f->lfCharSet,
			f->lfOutPrecision,
			f->lfClipPrecision,
			f->lfQuality,
			f->lfPitchAndFamily,
			utf8_FaceName);
	strcpy(str, tmp);
	free(utf8_FaceName);
}

static void TabFlagsEncodeString(int data, char *str)
{
	int num_saved = 0;

	strcpy(str,"");

	// we save the ones that are NOT displayed, so we can add new ones
	// and upgraders will see them
	for (int i = 0; i < MAX_TAB_TYPES; i++)
	{
		if (((data & (1 << i)) == 0) && GetImageTabShortName(i))
		{
			if (num_saved != 0)
				strcat(str, ", ");

			strcat(str,GetImageTabShortName(i));
			num_saved++;
		}
	}
}

static void TabFlagsDecodeString(const char *str, int *data)
{
	char s[256];

	snprintf(s, WINUI_ARRAY_LENGTH(s), "%s", str);
	// simple way to set all tab bits "on"
	*data = (1 << MAX_TAB_TYPES) - 1;
	char *token = strtok(s,", \t");

	while (token != NULL)
	{
		for (int j = 0; j < MAX_TAB_TYPES; j++)
		{
			if (!GetImageTabShortName(j) || (strcmp(GetImageTabShortName(j), token) == 0))
			{
				// turn off this bit
				*data &= ~(1 << j);
				break;
			}
		}

		token = strtok(NULL,", \t");
	}

	if (*data == 0)
		// not allowed to hide all tabs, because then why even show the area?
		*data = (1 << TAB_SCREENSHOT);
}

static void LoadInterfaceFile(winui_options &opts, const std::string &filename)
{
	osd_file::error filerr;
	emu_file file(OPEN_FLAG_READ);

	filerr = file.open(filename);

	if (filerr == osd_file::error::NONE)
	{
		std::string error_string;
		opts.parse_ini_file((util::core_file&)file, OPTION_PRIORITY_CMDLINE, OPTION_PRIORITY_CMDLINE, error_string);
		file.close();
	}
	else
		SaveInterfaceFile(opts, filename);
}

static void LoadInternalUIFile(ui_options &opts, const std::string &filename)
{
	osd_file::error filerr;
	emu_file file(OPEN_FLAG_READ);

	filerr = file.open(filename);

	if (filerr == osd_file::error::NONE)
	{
		std::string error_string;
		opts.parse_ini_file((util::core_file&)file, OPTION_PRIORITY_CMDLINE, OPTION_PRIORITY_CMDLINE, error_string);
		file.close();
	}
	else
		SaveInternalUIFile(opts, filename);
}

static void LoadPluginsFile(plugin_options &opts, const std::string &filename)
{
	osd_file::error filerr;
	emu_file file(OPEN_FLAG_READ);

	filerr = file.open(filename);

	if (filerr == osd_file::error::NONE)
	{
		std::string error_string;
		opts.parse_ini_file((util::core_file&)file, OPTION_PRIORITY_CMDLINE, OPTION_PRIORITY_CMDLINE, error_string);
		file.close();
	}
	else
		SavePluginsFile(opts, filename);
}

static void LoadOptionsFile(windows_options &opts, const std::string &filename)
{
	osd_file::error filerr;
	emu_file file(OPEN_FLAG_READ);

	filerr = file.open(filename);

	if (filerr == osd_file::error::NONE)
	{
		std::string error_string;
		opts.parse_ini_file((util::core_file&)file, OPTION_PRIORITY_CMDLINE, OPTION_PRIORITY_CMDLINE, error_string);
		file.close();
	}
}

static void LoadOptionsStartup(windows_options &opts, const std::string &filename)
{
	osd_file::error filerr;
	emu_file file(OPEN_FLAG_READ);

	filerr = file.open(filename);

	if (filerr == osd_file::error::NONE)
	{
		std::string error_string;
		opts.parse_ini_file((util::core_file&)file, OPTION_PRIORITY_CMDLINE, OPTION_PRIORITY_CMDLINE, error_string);
		file.close();
	}
	else
		SaveOptionsFile(opts, filename);
}

static void SaveInterfaceFile(winui_options &opts, const std::string &filename)
{
	osd_file::error filerr;
	emu_file file(OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS);

	filerr = file.open(filename);

	if (filerr == osd_file::error::NONE)
	{
		file.puts(opts.output_ini().c_str());
		file.close();
	}
}

static void SaveInternalUIFile(ui_options &opts, const std::string &filename)
{
	osd_file::error filerr;
	emu_file file(OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS);

	filerr = file.open(filename);

	if (filerr == osd_file::error::NONE)
	{
		file.puts(opts.output_ini().c_str());
		file.close();
	}
}

static void SavePluginsFile(plugin_options &opts, const std::string &filename)
{
	path_iterator iter(GetPluginsDir());
	std::string pluginpath;

	while (iter.next(pluginpath))
	{
		opts.parse_json(pluginpath);
	}

	osd_file::error filerr;
	emu_file file(OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS);

	filerr = file.open(filename);

	if (filerr == osd_file::error::NONE)
	{
		file.puts(opts.output_ini().c_str());
		file.close();
	}
}

static void SaveOptionsFile(windows_options &opts, const std::string &filename)
{
	osd_file::error filerr;
	emu_file file(OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS);

	filerr = file.open(filename);

	if (filerr == osd_file::error::NONE)
	{
		file.puts(opts.output_ini().c_str());
		file.close();
	}
}

static void LoadOptionsAndInterface(void)
{
	// parse GAMELIST.INI
	std::string gamename = std::string(GetGuiDir()).append(PATH_SEPARATOR).append(GAMELIST_INI_FILENAME).append(".ini");
	game_opts.load_options(gamename);
	// parse INTERFACE.INI
	std::string intername = std::string(GetGuiDir()).append(PATH_SEPARATOR).append(INTERFACE_INI_FILENAME).append(".ini");
	LoadInterfaceFile(winui_opts, intername);
	// parse MAME.INI
	std::string filename = std::string(GetIniDir()).append(PATH_SEPARATOR).append(DEFAULT_INI_FILENAME).append(".ini");
	LoadOptionsStartup(core_opts, filename);
	// parse UI.INI
	std::string uiname = std::string(GetIniDir()).append(PATH_SEPARATOR).append(INTERNAL_UI_INI_FILENAME).append(".ini");
	LoadInternalUIFile(ui_opts, uiname);
	// parse PLUGIN.INI
	std::string pluginname = std::string(GetIniDir()).append(PATH_SEPARATOR).append(PLUGINS_INI_FILENAME).append(".ini");
	LoadPluginsFile(plugin_opts, pluginname);
}

void SetDirectories(windows_options &opts)
{
	std::string error_string;
	opts.set_value(OPTION_MEDIAPATH, GetRomDirs(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OPTION_SAMPLEPATH, GetSampleDirs(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OPTION_INIPATH, GetIniDir(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OPTION_CFG_DIRECTORY, GetCfgDir(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OPTION_SNAPSHOT_DIRECTORY, GetImgDir(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OPTION_INPUT_DIRECTORY, GetInpDir(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OPTION_STATE_DIRECTORY, GetStateDir(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OPTION_ARTPATH, GetArtDir(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OPTION_NVRAM_DIRECTORY, GetNvramDir(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OPTION_CTRLRPATH, GetCtrlrDir(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OPTION_CROSSHAIRPATH, GetCrosshairDir(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(WINOPTION_HLSLPATH, GetHLSLDir(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OPTION_DIFF_DIRECTORY, GetDiffDir(), OPTION_PRIORITY_CMDLINE, error_string);
	//opts.set_value(OPTION_VIDEO_DIRECTORY, GetVideoDir(), OPTION_PRIORITY_CMDLINE, error_string);
	//opts.set_value(OSDOPTION_GLSLPATH, GetGLSLDir(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OSDOPTION_BGFX_PATH, GetBGFXDir(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OPTION_PLUGINSPATH, GetPluginsDir(), OPTION_PRIORITY_CMDLINE, error_string);
	opts.set_value(OPTION_LANGUAGEPATH, GetLanguageDir(), OPTION_PRIORITY_CMDLINE, error_string);
	assert(error_string.empty());
}

const char * GetFolderNameByID(UINT nID)
{
	UINT i = 0;
	extern const FOLDERDATA g_folderData[];
	extern const LPEXFOLDERDATA ExtraFolderData[];

	for (i = 0; i < MAX_EXTRA_FOLDERS * MAX_EXTRA_SUBFOLDERS; i++)
	{
		if( ExtraFolderData[i] )
		{
			if (ExtraFolderData[i]->m_nFolderId == nID)
				return ExtraFolderData[i]->m_szTitle;
		}
	}

	for (i = 0; i < MAX_FOLDERS; i++)
	{
		if (g_folderData[i].m_nFolderId == nID)
			return g_folderData[i].m_lpTitle;
	}

	return NULL;
}

DWORD GetFolderFlags(int folder_index)
{
	LPTREEFOLDER lpFolder = GetFolder(folder_index);

	if (lpFolder)
		return lpFolder->m_dwFlags & F_MASK;

	return 0;
}

/* Decode the flags into a DWORD */
static DWORD DecodeFolderFlags(const char *buf)
{
	DWORD flags = 0;
	int shift = 0;
	const char *ptr = buf;

	while (*ptr && (1 << shift) & F_MASK)
	{
		if (*ptr++ == '1')
			flags |= (1 << shift);

		shift++;
	}

	return flags;
}

/* Encode the flags into a string */
static const char * EncodeFolderFlags(DWORD value)
{
	int shift = 0;
	static char buf[80];

	memset(&buf, 0, sizeof(buf));

	while ((1 << shift) & F_MASK) 
	{
		buf[shift] = (value & (1 << shift)) ? '1' : '0';
		shift++;
	}

	return buf;
}

/* MSH 20080813
 * Read the folder filters from INTERFACE.ini.  This must only
 * be called AFTER the folders have all been created.
 */
void LoadFolderFlags(void)
{
	winui_options opts;
	int numFolders = 0;
	LPTREEFOLDER lpFolder;
	int i = 0;
	options_entry entries[2] = { { 0 }, { 0 } };
	char folder_name[80];

	memcpy(entries, filterOptions, sizeof(filterOptions));
	numFolders = GetNumFolders();

	for (i = 0; i < numFolders; i++)
	{
		lpFolder = GetFolder(i);

		if (lpFolder)
		{
			// Convert spaces to underscores
			strcpy(folder_name, lpFolder->m_lpTitle);
			char *ptr = folder_name;

			while (*ptr)
			{
				if (*ptr == ' ')
					*ptr = '_';

				ptr++;
			}

			std::string option_name = std::string(folder_name).append("_filters");
			// create entry
			entries[0].name = option_name.c_str();
			opts.add_entries(entries);
		}
	}

	// These are overlayed at the end of our UI ini
	// The normal read will skip them.
	std::string filename = std::string(GetGuiDir()).append(PATH_SEPARATOR).append(INTERFACE_INI_FILENAME).append(".ini");
    LoadInterfaceFile(opts, filename);

	// retrive the stored values
	for (i = 0; i < numFolders; i++)
	{
		lpFolder = GetFolder(i);

		if (lpFolder)
		{
			// Convert spaces to underscores
			strcpy(folder_name, lpFolder->m_lpTitle);
			char *ptr = folder_name;

			while (*ptr)
			{
				if (*ptr == ' ')
					*ptr = '_';

				ptr++;
			}

			std::string option_name = std::string(folder_name).append("_filters");
			// get entry and decode it
			const char *value = opts.value(option_name.c_str());

			if (value)
				lpFolder->m_dwFlags |= DecodeFolderFlags(value) & F_MASK;
		}
	}
}

// Adds our folder flags to a temporary winui_options, for saving.
static void AddFolderFlags(winui_options &opts)
{
	int numFolders = 0;
	int i = 0;
	int num_entries = 0;
	options_entry entries[2] = { { 0 }, { 0 } };
	char folder_name[256];

	entries[0].name = NULL;
	entries[0].defvalue = NULL;
	entries[0].flags = OPTION_HEADER;
	entries[0].description = "FOLDER FILTERS";
	opts.add_entries(entries);

	memcpy(entries, filterOptions, sizeof(filterOptions));
	numFolders = GetNumFolders();

	for (i = 0; i < numFolders; i++)
	{
		LPTREEFOLDER lpFolder = GetFolder(i);

		if (lpFolder && (lpFolder->m_dwFlags & F_MASK) != 0)
		{
			// Convert spaces to underscores
			strcpy(folder_name, lpFolder->m_lpTitle);
			char *ptr = folder_name;

			while (*ptr)
			{
				if (*ptr == ' ')
					*ptr = '_';

				ptr++;
			}

			std::string option_name = std::string(folder_name).append("_filters");
			// create entry
			entries[0].name = option_name.c_str();
			opts.add_entries(entries);
			// store entry
			std::string error_string;
			opts.set_value(option_name.c_str(), EncodeFolderFlags(lpFolder->m_dwFlags), OPTION_PRIORITY_CMDLINE, error_string);
			assert(error_string.empty());
			// increment counter
			num_entries++;
		}
	}
}

void SaveInterface(void)
{
	AddFolderFlags(winui_opts);
	std::string filename = std::string(GetGuiDir()).append(PATH_SEPARATOR).append(INTERFACE_INI_FILENAME).append(".ini");
	SaveInterfaceFile(winui_opts, filename);
}

void LoadInternalUI(void)
{
	std::string uiname = std::string(GetIniDir()).append(PATH_SEPARATOR).append(INTERNAL_UI_INI_FILENAME).append(".ini");
	LoadInternalUIFile(ui_opts, uiname);
}

void SaveInternalUI(void)
{
	std::string filename = std::string(GetIniDir()).append(PATH_SEPARATOR).append(INTERNAL_UI_INI_FILENAME).append(".ini");
	SaveInternalUIFile(ui_opts, filename);
}

void SavePlugins(void)
{
	std::string filename = std::string(GetIniDir()).append(PATH_SEPARATOR).append(PLUGINS_INI_FILENAME).append(".ini");
	SavePluginsFile(plugin_opts, filename);
}

void SaveGameDefaults(void)
{
	LoadOptions(save_opts, OPTIONS_GLOBAL, GLOBAL_OPTIONS);
	SaveOptions(OPTIONS_GLOBAL, save_opts, GLOBAL_OPTIONS);
}

void SaveGameList(void)
{
	std::string filename = std::string(GetGuiDir()).append(PATH_SEPARATOR).append(GAMELIST_INI_FILENAME).append(".ini");
	game_opts.save_options(filename);
}

void ResetInterface(void)
{
	winui_opts.revert(OPTION_PRIORITY_CMDLINE, OPTION_PRIORITY_CMDLINE);
	SaveInterface();
}

void ResetInternalUI(void)
{
	ui_opts.revert(OPTION_PRIORITY_CMDLINE, OPTION_PRIORITY_CMDLINE);
	SaveInternalUI();
}

void ResetGameDefaults(void)
{
	core_opts.revert(OPTION_PRIORITY_CMDLINE, OPTION_PRIORITY_CMDLINE);
	plugin_opts.revert(OPTION_PRIORITY_CMDLINE, OPTION_PRIORITY_CMDLINE);
	SaveOptions(OPTIONS_GLOBAL, core_opts, GLOBAL_OPTIONS);
	SavePlugins();
}

void ResetAllGameOptions(void)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFindFile;

	for (int i = 0; i < driver_list::total(); i++)
	{
		std::string filename = std::string(GetIniDir()).append(PATH_SEPARATOR).append(GetDriverGameName(i)).append(".ini");
		osd_file::remove(filename);
	}

    /* Easiest to just open the ini/source folder if it exists,
	then remove all the files in it that end in ini. */
	std::string pathname = std::string(GetIniDir()).append(PATH_SEPARATOR).append("source");
	std::string match = std::string(pathname.c_str()).append(PATH_SEPARATOR).append("*.ini");

	if ((hFindFile = win_find_first_file_utf8(match.c_str(), &FindFileData)) != INVALID_HANDLE_VALUE)
	{
		char *utf8_filename = win_utf8_from_wstring(FindFileData.cFileName);
		std::string match = std::string(pathname.c_str()).append(PATH_SEPARATOR).append(utf8_filename);
		free(utf8_filename);
		osd_file::remove(match);

		while (FindNextFile(hFindFile, &FindFileData) != 0)
		{
			char *utf8_filename = win_utf8_from_wstring(FindFileData.cFileName);
			std::string match = std::string(pathname.c_str()).append(PATH_SEPARATOR).append(utf8_filename);
			free(utf8_filename);
			osd_file::remove(match);
		}

		FindClose(hFindFile);
	}
}

/* ParseIniFile - parse a single INI file */
static void ParseIniFile(windows_options &opts, const char *name)
{
	/* open the file; if we fail, that's ok */
	std::string fname = std::string(GetIniDir()).append(PATH_SEPARATOR).append(name).append(".ini");
	LoadOptionsFile(opts, fname);
	SetDirectories(opts);
}

/*  get options, based on passed in option level. */
void LoadOptions(windows_options &opts, OPTIONS_TYPE opt_type, int game_num)
{
	const game_driver *driver = NULL;

	// Copy over the defaults
	ParseIniFile(opts, DEFAULT_INI_FILENAME);

	if (opt_type == OPTIONS_GLOBAL)
		return;

	if (game_num >= 0)
		driver = &driver_list::driver(game_num);

	// if we have a valid game driver, parse game-specific INI files
	if (driver != NULL)
	{
		const game_driver *parent = NULL;
		int cl = driver_list::clone(*driver);

		if (cl!= -1) 
			parent = &driver_list::driver(cl);

		int gp = -1;

		if (parent!=NULL) 
			gp = driver_list::clone(*parent);

		const game_driver *gparent = NULL;

		if (parent != NULL) 
		{
			if (gp!= -1) 
				gparent= &driver_list::driver(gp);
		}

		// parse "horizont.ini" for horizontal games
		if (!DriverIsVertical(game_num))
			ParseIniFile(opts, "horizont");

		if (opt_type == OPTIONS_HORIZONTAL)
			return;

		// parse "vertical.ini" for vertical games
		if (DriverIsVertical(game_num))
			ParseIniFile(opts, "vertical");

		if (opt_type == OPTIONS_VERTICAL)
			return;

		// parse "raster.ini" for raster games
		if (!DriverIsVector(game_num))
			ParseIniFile(opts, "raster");

		if (opt_type == OPTIONS_RASTER)
			return;

		// parse "vector.ini" for vector games
		if (DriverIsVector(game_num))
			ParseIniFile(opts, "vector");

		if (opt_type == OPTIONS_VECTOR)
			return;

		// then parse "<sourcefile>.ini"
		std::string basename = core_filename_extract_base(driver->source_file, true);
		std::string srcname = std::string("source").append(PATH_SEPARATOR).append(basename.c_str());
		ParseIniFile(opts, srcname.c_str());

		if (opt_type == OPTIONS_SOURCE)
			return;

		// then parent the grandparent, parent, and game-specific INIs
		if (gparent != NULL)
			ParseIniFile(opts, gparent->name);

		if (parent != NULL)
			ParseIniFile(opts, parent->name);

		if (opt_type == OPTIONS_PARENT)
			return;

		ParseIniFile(opts, driver->name);

		if (opt_type == OPTIONS_GAME)
			return;
	}
}

/*
 * Save ini file based on game_num and passed in opt_type.  If opts are
 * NULL, the ini will be removed.
 *
 * game_num must be valid or the driver cannot be expanded and anything
 * with a higher priority than OPTIONS_VECTOR will not be saved.
 */
void SaveOptions(OPTIONS_TYPE opt_type, windows_options &opts, int game_num)
{
	const game_driver *driver = NULL;
	std::string filename;

	if (game_num >= 0)
		driver = &driver_list::driver(game_num);

	if (opt_type == OPTIONS_GLOBAL)
		filename.assign(DEFAULT_INI_FILENAME);
	else if (opt_type == OPTIONS_RASTER)
		filename.assign("raster");
	else if (opt_type == OPTIONS_VECTOR)
		filename.assign("vector");
	else if (opt_type == OPTIONS_VERTICAL)
		filename.assign("vertical");
	else if (opt_type == OPTIONS_HORIZONTAL)
		filename.assign("horizont");
	else if (driver != NULL)
	{
		if (opt_type == OPTIONS_SOURCE)
		{
			// determine the <sourcefile>
			std::string basename = core_filename_extract_base(driver->source_file, true);
			std::string srcname = std::string("source").append(PATH_SEPARATOR).append(basename.c_str());
			filename.assign(srcname.c_str());
		}
		else if (opt_type == OPTIONS_GAME)
			filename.assign(driver->name);
	}

	if (!filename.empty())
	{
		std::string filepath = std::string(GetIniDir()).append(PATH_SEPARATOR).append(filename.c_str()).append(".ini");
		SetDirectories(opts);
		SaveOptionsFile(opts, filepath);
	}
}

int GetDriverCache(int driver_index)
{
	return game_opts.cache(driver_index);
}

void SetDriverCache(int driver_index, int val)
{
	game_opts.cache(driver_index, val);
}

void SetRequiredDriverCacheStatus(void)
{
	static bool bFirst = true;

	if (bFirst)
	{
		RequiredDriverCacheStatus = RequiredDriverCache(true);
		bFirst = false;
	}
}
bool GetRequiredDriverCacheStatus(void)
{
	SetRequiredDriverCacheStatus();

	return RequiredDriverCacheStatus;
}

bool RequiredDriverCache(bool check)
{
	bool ret = false;

	if (strcmp(winui_opts.value(MUIOPTION_VERSION), GetVersionString()) != 0)
		ret = true;

	if (!check)
	{
		std::string error_string;
		winui_opts.set_value(MUIOPTION_VERSION, GetVersionString(), OPTION_PRIORITY_CMDLINE, error_string);
		assert(error_string.empty());
	}

	return ret;
}
