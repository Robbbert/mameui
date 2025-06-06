// For licensing and usage information, read docs/release/winui_license.txt
// MASTER
//****************************************************************************

 /***************************************************************************

  mui_opts.cpp

  Stores global options and per-game options;

***************************************************************************/

// standard windows headers
#include <windows.h>
#include <windowsx.h>

// standard C headers
#include <tchar.h>

// MAME/MAMEUI headers
#include "emu.h"
#include "main.h"
#include "ui/info.h"
#include "drivenum.h"
#include "mui_opts.h"
#include <fstream>      // for *_opts.h (below)
#include "game_opts.h"
#include "ui_opts.h"
#include "mui_util.h"
#include "treeview.h"
#include "splitters.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

typedef std::string string;

/***************************************************************************
    Internal function prototypes
 ***************************************************************************/

// static void LoadFolderFilter(int folder_index,int filters);

static string CusColorEncodeString(const COLORREF *value);
static void CusColorDecodeString(string ss, COLORREF *value);

static string SplitterEncodeString(const int *value);
static void SplitterDecodeString(string ss, int *value);

static string FontEncodeString(const LOGFONT *f);
static void FontDecodeString(string ss, LOGFONT *f);

static string TabFlagsEncodeString(int data);
static void TabFlagsDecodeString(string ss, int *data);

static string ColumnEncodeStringWithCount(const int *value, int count);
static void ColumnDecodeStringWithCount(string ss, int *value, int count);



/***************************************************************************
    Internal defines
 ***************************************************************************/

static string gameinfo_ini_filename, mui_ini_filename;


/***************************************************************************
    Internal structures
 ***************************************************************************/

 /***************************************************************************
    Internal variables
 ***************************************************************************/

static winui_ui_options settings; // mameui.ini
static winui_game_options game_opts;    // game stats


// Screen shot Page tab control text
// these must match the order of the options flags in options.h
// (TAB_...)
static const char *const image_tabs_long_name[MAX_TAB_TYPES] =
{
	"Artwork",
	"Boss",
	"Cabinet",
	"Control Panel",
	"Cover",
	"End",
	"Flyer",
	"Game Over",
	"How To",
	"Logo",
	"Marquee",
	"PCB",
	"Scores",
	"Select",
	"Snapshot",
	"Title",
	"Versus",
	"History"
};

static const char *const image_tabs_short_name[MAX_TAB_TYPES] =
{
	"artpreview",
	"boss",
	"cabinet",
	"cpanel",
	"cover",
	"end",
	"flyer",
	"gameover",
	"howto",
	"logo",
	"marquee",
	"pcb",
	"scores",
	"select",
	"snap",
	"title",
	"versus",
	"history"
};


/***************************************************************************
    External functions
 ***************************************************************************/
string GetGameName(int drvindex)
{
	if ((drvindex >= 0) && (drvindex < driver_list::total()))
		return driver_list::driver(drvindex).name;
	else
		return "0";
}

void OptionsInit()
{
	// set up global options
	gameinfo_ini_filename = GetEmuPath() + PATH_SEPARATOR + "MAME_g.ini";
	mui_ini_filename = GetEmuPath() + PATH_SEPARATOR + "MAMEUI.ini";
	printf("OptionsInit: About to load %s\n",mui_ini_filename.c_str());fflush(stdout);
	settings.load_file(mui_ini_filename.c_str());                    // parse MAMEUI.ini
	printf("OptionsInit: About to load %s\n",gameinfo_ini_filename.c_str());fflush(stdout);
	game_opts.load_file(gameinfo_ini_filename.c_str());             // parse MAME_g.ini
	printf("OptionsInit: Finished\n");fflush(stdout);
	return;
}

// Restore ui settings to factory
void ResetGUI()
{
	settings.reset_and_save(mui_ini_filename.c_str());
}

const char * GetImageTabLongName(int tab_index)
{
	return image_tabs_long_name[tab_index];
}

const char * GetImageTabShortName(int tab_index)
{
	return image_tabs_short_name[tab_index];
}

//============================================================
//  OPTIONS WRAPPERS
//============================================================

static COLORREF options_get_color(const char *name)
{
	unsigned int r = 0, g = 0, b = 0;
	COLORREF value;
	const string val = settings.getter(name);

	if (sscanf(val.c_str(), "%u,%u,%u", &r, &g, &b) == 3)
		value = RGB(r,g,b);
	else
		value = (COLORREF) - 1;
	return value;
}

static void options_set_color(const char *name, COLORREF value)
{
	char value_str[32];

	if (value == (COLORREF) -1)
		snprintf(value_str, std::size(value_str), "%d", (int) value);
	else
		snprintf(value_str, std::size(value_str), "%d,%d,%d", (((int) value) >>  0) & 0xFF, (((int) value) >>  8) & 0xFF, (((int) value) >> 16) & 0xFF);

	settings.setter(name, string(value_str));
}

static COLORREF options_get_color_default(const char *name, int default_color)
{
	COLORREF value = options_get_color(name);
	if (value == (COLORREF) -1)
		value = GetSysColor(default_color);

	return value;
}

static void options_set_color_default(const char *name, COLORREF value, int default_color)
{
	if (value == GetSysColor(default_color))
		options_set_color(name, (COLORREF) -1);
	else
		options_set_color(name, value);
}

static input_seq *options_get_input_seq(const char *name)
{
/*
	static input_seq seq;
	string val = settings.getter(name);
	input_seq_from_tokens(NULL, seq_string.c_str(), &seq);  // HACK
	return &seq;*/
	return NULL;
}



//============================================================
//  OPTIONS CALLS
//============================================================

// ***************************************************************** MAMEUI.INI settings **************************************************************************
void SetViewMode(int val)
{
	settings.setter(MUIOPTION_LIST_MODE, val);
}

int GetViewMode()
{
	return settings.int_value(MUIOPTION_LIST_MODE);
}

void SetGameCheck(BOOL game_check)
{
	settings.setter(MUIOPTION_CHECK_GAME, game_check);
}

BOOL GetGameCheck()
{
	return settings.bool_value(MUIOPTION_CHECK_GAME);
}

void SetEnableIndent(bool value)
{
	settings.setter(MUIOPTION_VIEW_INDENT, value);
}

bool GetEnableIndent()
{
	return settings.bool_value(MUIOPTION_VIEW_INDENT);
}

void SetJoyGUI(BOOL use_joygui)
{
	settings.setter(MUIOPTION_JOYSTICK_IN_INTERFACE, use_joygui);
}

BOOL GetJoyGUI()
{
	return settings.bool_value( MUIOPTION_JOYSTICK_IN_INTERFACE);
}

void SetKeyGUI(BOOL use_keygui)
{
	settings.setter(MUIOPTION_KEYBOARD_IN_INTERFACE, use_keygui);
}

BOOL GetKeyGUI()
{
	return settings.bool_value(MUIOPTION_KEYBOARD_IN_INTERFACE);
}

void SetCycleScreenshot(int cycle_screenshot)
{
	settings.setter(MUIOPTION_CYCLE_SCREENSHOT, cycle_screenshot);
}

int GetCycleScreenshot()
{
	return settings.int_value(MUIOPTION_CYCLE_SCREENSHOT);
}

void SetStretchScreenShotLarger(BOOL stretch)
{
	settings.setter(MUIOPTION_STRETCH_SCREENSHOT_LARGER, stretch);
}

BOOL GetStretchScreenShotLarger()
{
	return settings.bool_value(MUIOPTION_STRETCH_SCREENSHOT_LARGER);
}

void SetScreenshotBorderSize(int size)
{
	settings.setter(MUIOPTION_SCREENSHOT_BORDER_SIZE, size);
}

int GetScreenshotBorderSize()
{
	return settings.int_value(MUIOPTION_SCREENSHOT_BORDER_SIZE);
}

void SetScreenshotBorderColor(COLORREF uColor)
{
	options_set_color_default(MUIOPTION_SCREENSHOT_BORDER_COLOR, uColor, COLOR_3DFACE);
}

COLORREF GetScreenshotBorderColor()
{
	return options_get_color_default(MUIOPTION_SCREENSHOT_BORDER_COLOR, COLOR_3DFACE);
}

void SetFilterInherit(BOOL inherit)
{
	settings.setter(MUIOPTION_INHERIT_FILTER, inherit);
}

BOOL GetFilterInherit()
{
	return settings.bool_value( MUIOPTION_INHERIT_FILTER);
}

void SetOffsetClones(BOOL offset)
{
	settings.setter(MUIOPTION_OFFSET_CLONES, offset);
}

BOOL GetOffsetClones()
{
	return settings.bool_value( MUIOPTION_OFFSET_CLONES);
}

void SetSavedFolderID(UINT val)
{
	settings.setter(MUIOPTION_DEFAULT_FOLDER_ID, (int) val);
}

UINT GetSavedFolderID()
{
	return (UINT) settings.int_value(MUIOPTION_DEFAULT_FOLDER_ID);
}

void SetOverrideRedX(BOOL val)
{
	settings.setter(MUIOPTION_OVERRIDE_REDX, val);
}

BOOL GetOverrideRedX()
{
	return settings.bool_value(MUIOPTION_OVERRIDE_REDX);
}

static LPBITS GetShowFolderFlags(LPBITS bits)
{
	SetAllBits(bits, TRUE);

	string val = settings.getter(MUIOPTION_HIDE_FOLDERS);
	if (val.empty())
		return bits;

	extern const FOLDERDATA g_folderData[];
	char s[val.size()+1];
	snprintf(s, val.size()+1, "%s", val.c_str());
	char *token = strtok(s, ",");
	int j;
	while (token)
	{
		for (j=0; g_folderData[j].m_lpTitle; j++)
		{
			if (strcmp(g_folderData[j].short_name,token) == 0)
			{
				ClearBit(bits, g_folderData[j].m_nFolderId);
				break;
			}
		}
		token = strtok(NULL,",");
	}
	return bits;
}

BOOL GetShowFolder(int folder)
{
	LPBITS show_folder_flags = NewBits(MAX_FOLDERS);
	show_folder_flags = GetShowFolderFlags(show_folder_flags);
	BOOL result = TestBit(show_folder_flags, folder);
	DeleteBits(show_folder_flags);
	return result;
}

void SetShowFolder(int folder, BOOL show)
{
	LPBITS show_folder_flags = NewBits(MAX_FOLDERS);
	int i = 0, j = 0;
	int num_saved = 0;
	string str;
	extern const FOLDERDATA g_folderData[];

	show_folder_flags = GetShowFolderFlags(show_folder_flags);

	if (show)
		SetBit(show_folder_flags, folder);
	else
		ClearBit(show_folder_flags, folder);

	// we save the ones that are NOT displayed, so we can add new ones
	// and upgraders will see them
	for (i=0; i<MAX_FOLDERS; i++)
	{
		if (TestBit(show_folder_flags, i) == FALSE)
		{
			if (num_saved != 0)
				str.append(",");

			for (j=0; g_folderData[j].m_lpTitle; j++)
			{
				if (g_folderData[j].m_nFolderId == i)
				{
					str.append(g_folderData[j].short_name);
					num_saved++;
					break;
				}
			}
		}
	}
	settings.setter(MUIOPTION_HIDE_FOLDERS, str);
	DeleteBits(show_folder_flags);
}

void SetShowStatusBar(BOOL val)
{
	settings.setter(MUIOPTION_SHOW_STATUS_BAR, val);
}

BOOL GetShowStatusBar()
{
	return settings.bool_value( MUIOPTION_SHOW_STATUS_BAR);
}

void SetShowTabCtrl (BOOL val)
{
	settings.setter(MUIOPTION_SHOW_TABS, val);
}

BOOL GetShowTabCtrl ()
{
	return settings.bool_value( MUIOPTION_SHOW_TABS);
}

void SetShowToolBar(BOOL val)
{
	settings.setter(MUIOPTION_SHOW_TOOLBAR, val);
}

BOOL GetShowToolBar()
{
	return settings.bool_value( MUIOPTION_SHOW_TOOLBAR);
}

void SetCurrentTab(int val)
{
	settings.setter(MUIOPTION_CURRENT_TAB, val);
}

int GetCurrentTab()
{
	return settings.int_value(MUIOPTION_CURRENT_TAB);
}

// Need int here in case no games were in the list at exit
void SetDefaultGame(int val)
{
	if ((val < 0) || (val > driver_list::total()))
		settings.setter(MUIOPTION_DEFAULT_GAME, "");
	else
		settings.setter(MUIOPTION_DEFAULT_GAME, driver_list::driver(val).name);
}

uint32_t GetDefaultGame()
{
	string t = settings.getter(MUIOPTION_DEFAULT_GAME);
	if (t.empty())
		return 0;
	int val = driver_list::find(t.c_str());
	if (val < 0)
		val = 0;
	return val;
}

void SetWindowArea(const AREA *area)
{
	settings.setter(MUIOPTION_WINDOW_X, area->x);
	settings.setter(MUIOPTION_WINDOW_Y, area->y);
	settings.setter(MUIOPTION_WINDOW_WIDTH, area->width);
	settings.setter(MUIOPTION_WINDOW_HEIGHT, area->height);
}

void GetWindowArea(AREA *area)
{
	area->x = settings.int_value(MUIOPTION_WINDOW_X);
	area->y = settings.int_value(MUIOPTION_WINDOW_Y);
	area->width  = settings.int_value(MUIOPTION_WINDOW_WIDTH);
	area->height = settings.int_value(MUIOPTION_WINDOW_HEIGHT);
}

void SetWindowState(UINT state)
{
	settings.setter(MUIOPTION_WINDOW_STATE, (int)state);
}

UINT GetWindowState()
{
	return settings.int_value(MUIOPTION_WINDOW_STATE);
}

void SetWindowPanes(int val)
{
	settings.setter(MUIOPTION_WINDOW_PANES, val & 15);
}

UINT GetWindowPanes()
{
	return settings.int_value(MUIOPTION_WINDOW_PANES) & 15;
}

void SetCustomColor(int iIndex, COLORREF uColor)
{
	if ((iIndex < 0) || (iIndex > 15))
		return;

	COLORREF custom_color[16];
	CusColorDecodeString(settings.getter(MUIOPTION_CUSTOM_COLOR), custom_color);
	custom_color[iIndex] = uColor;
	settings.setter(MUIOPTION_CUSTOM_COLOR, CusColorEncodeString(custom_color));
}

COLORREF GetCustomColor(int iIndex)
{
	if ((iIndex < 0) || (iIndex > 15))
		return (COLORREF)RGB(0,0,0);

	COLORREF custom_color[16];

	CusColorDecodeString(settings.getter(MUIOPTION_CUSTOM_COLOR), custom_color);

	if (custom_color[iIndex] == (COLORREF)-1)
		return (COLORREF)RGB(0,0,0);

	return custom_color[iIndex];
}

void SetListFont(const LOGFONT *font)
{
	settings.setter(MUIOPTION_LIST_FONT, FontEncodeString(font));
}

void GetListFont(LOGFONT *font)
{
	FontDecodeString(settings.getter(MUIOPTION_LIST_FONT), font);
}

void SetListFontColor(COLORREF uColor)
{
	options_set_color_default(MUIOPTION_TEXT_COLOR, uColor, COLOR_WINDOWTEXT);
}

COLORREF GetListFontColor()
{
	return options_get_color_default(MUIOPTION_TEXT_COLOR, COLOR_WINDOWTEXT);
}

void SetListCloneColor(COLORREF uColor)
{
	options_set_color_default(MUIOPTION_CLONE_COLOR, uColor, COLOR_WINDOWTEXT);
}

COLORREF GetListCloneColor()
{
	return options_get_color_default(MUIOPTION_CLONE_COLOR, COLOR_WINDOWTEXT);
}

int GetShowTab(int tab)
{
	int show_tab_flags = 0;
	TabFlagsDecodeString(settings.getter(MUIOPTION_HIDE_TABS), &show_tab_flags);
	return (show_tab_flags & (1 << tab)) != 0;
}

void SetShowTab(int tab,BOOL show)
{
	int show_tab_flags = 0;
	TabFlagsDecodeString(settings.getter(MUIOPTION_HIDE_TABS), &show_tab_flags);

	if (show)
		show_tab_flags |= 1 << tab;
	else
		show_tab_flags &= ~(1 << tab);

	settings.setter(MUIOPTION_HIDE_TABS, TabFlagsEncodeString(show_tab_flags));
}

// don't delete the last one
BOOL AllowedToSetShowTab(int tab,BOOL show)
{
	int show_tab_flags = 0;

	if (show == TRUE)
		return TRUE;

	TabFlagsDecodeString(settings.getter(MUIOPTION_HIDE_TABS), &show_tab_flags);

	show_tab_flags &= ~(1 << tab);
	return show_tab_flags != 0;
}

int GetHistoryTab()
{
	return settings.int_value(MUIOPTION_HISTORY_TAB);
}

void SetHistoryTab(int tab, BOOL show)
{
	if (show)
		settings.setter(MUIOPTION_HISTORY_TAB, tab);
	else
		settings.setter(MUIOPTION_HISTORY_TAB, TAB_NONE);
}

void SetColumnWidths(int width[])
{
	settings.setter(MUIOPTION_COLUMN_WIDTHS, ColumnEncodeStringWithCount(width, COLUMN_MAX));
}

void GetColumnWidths(int width[])
{
	ColumnDecodeStringWithCount(settings.getter(MUIOPTION_COLUMN_WIDTHS), width, COLUMN_MAX);
}

void SetSplitterPos(int splitterId, int pos)
{
	int *splitter;

	if (splitterId < GetSplitterCount())
	{
		splitter = (int *) alloca(GetSplitterCount() * sizeof(*splitter));
		SplitterDecodeString(settings.getter(MUIOPTION_SPLITTERS), splitter);
		splitter[splitterId] = pos;
		settings.setter(MUIOPTION_SPLITTERS, SplitterEncodeString(splitter));
	}
}

int GetSplitterPos(int splitterId)
{
	int *splitter;
	splitter = (int *) alloca(GetSplitterCount() * sizeof(*splitter));
	SplitterDecodeString(settings.getter(MUIOPTION_SPLITTERS), splitter);

	if (splitterId < GetSplitterCount())
		return splitter[splitterId];

	return -1; /* Error */
}

void SetColumnOrder(int order[])
{
	settings.setter(MUIOPTION_COLUMN_ORDER, ColumnEncodeStringWithCount(order, COLUMN_MAX));
}

void GetColumnOrder(int order[])
{
	ColumnDecodeStringWithCount(settings.getter(MUIOPTION_COLUMN_ORDER), order, COLUMN_MAX);
}

void SetColumnShown(int shown[])
{
	settings.setter(MUIOPTION_COLUMN_SHOWN, ColumnEncodeStringWithCount(shown, COLUMN_MAX));
}

void GetColumnShown(int shown[])
{
	ColumnDecodeStringWithCount(settings.getter(MUIOPTION_COLUMN_SHOWN), shown, COLUMN_MAX);
}

void SetSortColumn(int column)
{
	settings.setter(MUIOPTION_SORT_COLUMN, column);
}

int GetSortColumn()
{
	return settings.int_value(MUIOPTION_SORT_COLUMN);
}

void SetSortReverse(BOOL reverse)
{
	settings.setter(MUIOPTION_SORT_REVERSED, reverse);
}

BOOL GetSortReverse()
{
	return settings.bool_value( MUIOPTION_SORT_REVERSED);
}

const string GetBgDir ()
{
	string t = settings.getter(MUIOPTION_BACKGROUND_DIRECTORY);
	if (t.empty())
		return "bkground\\bkground.png";
	else
		return settings.getter(MUIOPTION_BACKGROUND_DIRECTORY);
}

void SetBgDir (const char* path)
{
	settings.setter(MUIOPTION_BACKGROUND_DIRECTORY, path);
}

const string GetVideoDir()
{
	string t = settings.getter(MUIOPTION_VIDEO_DIRECTORY);
	if (t.empty())
		return "video";
	else
		return settings.getter(MUIOPTION_VIDEO_DIRECTORY);
}

void SetVideoDir(const char *path)
{
	settings.setter(MUIOPTION_VIDEO_DIRECTORY, path);
}

const string GetManualsDir()
{
	string t = settings.getter(MUIOPTION_MANUALS_DIRECTORY);
	if (t.empty())
		return "manuals";
	else
		return settings.getter(MUIOPTION_MANUALS_DIRECTORY);
}

void SetManualsDir(const char *path)
{
	settings.setter(MUIOPTION_MANUALS_DIRECTORY, path);
}

// ***************************************************************** MAME_g.INI settings **************************************************************************
int GetRomAuditResults(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return game_opts.rom(drvindex);
}

void SetRomAuditResults(int drvindex, int audit_results)
{
	if (drvindex >= 0)
		game_opts.rom(drvindex, audit_results);
}

int GetSampleAuditResults(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return game_opts.sample(drvindex);
}

void SetSampleAuditResults(int drvindex, int audit_results)
{
	if (drvindex >= 0)
		game_opts.sample(drvindex, audit_results);
}

static void IncrementPlayVariable(int drvindex, const char *play_variable, uint32_t increment)
{
	if (drvindex >= 0)
	{
		if (strcmp(play_variable, "count") == 0)
			game_opts.play_count(drvindex, game_opts.play_count(drvindex) + increment);
		else
		if (strcmp(play_variable, "time") == 0)
			game_opts.play_time(drvindex, game_opts.play_time(drvindex) + increment);
	}
}

void IncrementPlayCount(int drvindex)
{
	if (drvindex > 0)
		IncrementPlayVariable(drvindex, "count", 1);
}

uint32_t GetPlayCount(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return game_opts.play_count(drvindex);
}

static void ResetPlayVariable(int drvindex, const char *play_variable)
{
	if (drvindex < 0)
		/* all games */
		for (uint32_t i = 0; i < driver_list::total(); i++)
			ResetPlayVariable(i, play_variable);
	else
	{
		if (strcmp(play_variable, "count") == 0)
			game_opts.play_count(drvindex, 0);
		else
		if (strcmp(play_variable, "time") == 0)
			game_opts.play_time(drvindex, 0);
	}
}

void ResetPlayCount(int drvindex)
{
	ResetPlayVariable(drvindex, "count");
}

void ResetPlayTime(int drvindex)
{
	ResetPlayVariable(drvindex, "time");
}

uint32_t GetPlayTime(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return game_opts.play_time(drvindex);
}

void IncrementPlayTime(int drvindex, uint32_t playtime)
{
	if (drvindex >= 0)
		IncrementPlayVariable(drvindex, "time", playtime);
}

void GetTextPlayTime(int drvindex, char *buf)
{
	if ((drvindex >= 0) && (drvindex < driver_list::total()))
	{
		uint32_t second = GetPlayTime(drvindex);
		uint32_t hour = second / 3600;
		second -= 3600*hour;
		uint8_t minute = second / 60; //Calc Minutes
		second -= 60*minute;

		if (hour == 0)
			sprintf(buf, "%d:%02d", minute, second );
		else
			sprintf(buf, "%d:%02d:%02d", hour, minute, second );
	}
}

input_seq* Get_ui_key_up()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_UP);
}

input_seq* Get_ui_key_down()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_DOWN);
}

input_seq* Get_ui_key_left()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_LEFT);
}

input_seq* Get_ui_key_right()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_RIGHT);
}

input_seq* Get_ui_key_start()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_START);
}

input_seq* Get_ui_key_pgup()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_PGUP);
}

input_seq* Get_ui_key_pgdwn()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_PGDWN);
}

input_seq* Get_ui_key_home()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_HOME);
}

input_seq* Get_ui_key_end()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_END);
}

input_seq* Get_ui_key_ss_change()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_SS_CHANGE);
}

input_seq* Get_ui_key_history_up()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_HISTORY_UP);
}

input_seq* Get_ui_key_history_down()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_HISTORY_DOWN);
}

input_seq* Get_ui_key_context_filters()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_CONTEXT_FILTERS);
}

input_seq* Get_ui_key_select_random()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_SELECT_RANDOM);
}

input_seq* Get_ui_key_game_audit()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_GAME_AUDIT);
}

input_seq* Get_ui_key_game_properties()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_GAME_PROPERTIES);
}

input_seq* Get_ui_key_help_contents()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_HELP_CONTENTS);
}

input_seq* Get_ui_key_update_gamelist()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_UPDATE_GAMELIST);
}

input_seq* Get_ui_key_view_folders()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_FOLDERS);
}

input_seq* Get_ui_key_view_fullscreen()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_FULLSCREEN);
}

input_seq* Get_ui_key_view_pagetab()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_PAGETAB);
}

input_seq* Get_ui_key_view_picture_area()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_PICTURE_AREA);
}

input_seq* Get_ui_key_view_software_area()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_SOFTWARE_AREA);
}

input_seq* Get_ui_key_view_status()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_STATUS);
}

input_seq* Get_ui_key_view_toolbars()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_TOOLBARS);
}

input_seq* Get_ui_key_view_tab_cabinet()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_TAB_CABINET);
}

input_seq* Get_ui_key_view_tab_cpanel()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_TAB_CPANEL);
}

input_seq* Get_ui_key_view_tab_flyer()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_TAB_FLYER);
}

input_seq* Get_ui_key_view_tab_history()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_TAB_HISTORY);
}

input_seq* Get_ui_key_view_tab_marquee()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_TAB_MARQUEE);
}

input_seq* Get_ui_key_view_tab_screenshot()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_TAB_SCREENSHOT);
}

input_seq* Get_ui_key_view_tab_title()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_TAB_TITLE);
}

input_seq* Get_ui_key_view_tab_pcb()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_VIEW_TAB_PCB);
}

input_seq* Get_ui_key_quit()
{
	return options_get_input_seq(MUIOPTION_UI_KEY_QUIT);
}

static int GetUIJoy(const char *option_name, int joycodeIndex)
{
	int joycodes[4];

	if ((joycodeIndex < 0) || (joycodeIndex > 3))
		joycodeIndex = 0;
	ColumnDecodeStringWithCount(settings.getter(option_name), joycodes, std::size(joycodes));
	return joycodes[joycodeIndex];
}

static void SetUIJoy(const char *option_name, int joycodeIndex, int val)
{
	int joycodes[4];

	if ((joycodeIndex < 0) || (joycodeIndex > 3))
		joycodeIndex = 0;
	ColumnDecodeStringWithCount(settings.getter(option_name), joycodes, std::size(joycodes));
	joycodes[joycodeIndex] = val;
	settings.setter(option_name, ColumnEncodeStringWithCount(joycodes, std::size(joycodes)));
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

// exec functions start: these are unsupported
void SetUIJoyExec(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_EXEC, joycodeIndex, val);
}

int GetUIJoyExec(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_EXEC, joycodeIndex);
}

const string GetExecCommand()
{
	return settings.getter(MUIOPTION_EXEC_COMMAND);
}

// not used
void SetExecCommand(char *cmd)
{
	settings.setter(MUIOPTION_EXEC_COMMAND, cmd);
}

int GetExecWait()
{
	return settings.int_value(MUIOPTION_EXEC_WAIT);
}

void SetExecWait(int wait)
{
	settings.setter(MUIOPTION_EXEC_WAIT, wait);
}
// exec functions end

BOOL GetHideMouseOnStartup()
{
	return settings.bool_value(MUIOPTION_HIDE_MOUSE);
}

void SetHideMouseOnStartup(BOOL hide)
{
	settings.setter(MUIOPTION_HIDE_MOUSE, hide);
}

BOOL GetRunFullScreen()
{
	return settings.bool_value( MUIOPTION_FULL_SCREEN);
}

void SetRunFullScreen(BOOL fullScreen)
{
	settings.setter(MUIOPTION_FULL_SCREEN, fullScreen);
}

/***************************************************************************
    Internal functions
 ***************************************************************************/

static string CusColorEncodeString(const COLORREF *value)
{
	string str = std::to_string(value[0]);

	for (int i = 1; i < 16; i++)
		str.append(",").append(std::to_string(value[i]));

	return str;
}

static void CusColorDecodeString(string ss, COLORREF *value)
{
	const char *str = ss.c_str();
	char *s, *p;
	char tmpStr[256];

	strcpy(tmpStr, str);
	p = tmpStr;

	for (int i = 0; p && i < 16; i++)
	{
		s = p;

		if ((p = strchr(s,',')) != NULL && *p == ',')
		{
			*p = '\0';
			p++;
		}
		value[i] = atoi(s);
	}
}


static string ColumnEncodeStringWithCount(const int *value, int count)
{
	string str = std::to_string(value[0]);

	for (int i = 1; i < count; i++)
		str.append(",").append(std::to_string(value[i]));

	return str;
}

static void ColumnDecodeStringWithCount(string ss, int *value, int count)
{
	const char *str = ss.c_str();
	char *s, *p;
	char tmpStr[256];

	if (str == NULL)
		return;

	strcpy(tmpStr, str);
	p = tmpStr;

	for (int i = 0; p && i < count; i++)
	{
		s = p;

		if ((p = strchr(s,',')) != NULL && *p == ',')
		{
			*p = '\0';
			p++;
		}
		value[i] = atoi(s);
	}
}

static string SplitterEncodeString(const int *value)
{
	string str = std::to_string(value[0]);

	for (int i = 1; i < GetSplitterCount(); i++)
		str.append(",").append(std::to_string(value[i]));

	return str;
}

static void SplitterDecodeString(string ss, int *value)
{
	const char *str = ss.c_str();
	char *s, *p;
	char tmpStr[256];

	strcpy(tmpStr, str);
	p = tmpStr;

	for (int i = 0; p && i < GetSplitterCount(); i++)
	{
		s = p;

		if ((p = strchr(s,',')) != NULL && *p == ',')
		{
			*p = '\0';
			p++;
		}
		value[i] = atoi(s);
	}
}

/* Parse the given comma-delimited string into a LOGFONT structure */
static void FontDecodeString(string ss, LOGFONT *f)
{
	const char* str = ss.c_str();
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
	if (ptr)
	{
		TCHAR *t_s = ui_wstring_from_utf8(ptr + 1);
		if( !t_s )
			return;
		_tcscpy(f->lfFaceName, t_s);
		free(t_s);
	}
}

/* Encode the given LOGFONT structure into a comma-delimited string */
static string FontEncodeString(const LOGFONT *f)
{
	char* utf8_FaceName = ui_utf8_from_wstring(f->lfFaceName);
	if( !utf8_FaceName )
		return "";

	char s[200];
	sprintf(s, "%li,%li,%li,%li,%li,%i,%i,%i,%i,%i,%i,%i,%i,%s",
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

	free(utf8_FaceName);
	return string(s);
}

static string TabFlagsEncodeString(int data)
{
	int num_saved = 0;
	string str;

	// we save the ones that are NOT displayed, so we can add new ones
	// and upgraders will see them
	for ( int i=0; i<MAX_TAB_TYPES; i++)
	{
		if (((data & (1 << i)) == 0) && GetImageTabShortName(i))
		{
			if (num_saved > 0)
				str.append(",");

			str.append(GetImageTabShortName(i));
			num_saved++;
		}
	}
	return str;
}

static void TabFlagsDecodeString(string ss, int *data)
{
	const char *str = ss.c_str();
	int j = 0;
	char s[2000];
	char *token;

	snprintf(s, std::size(s), "%s", str);

	// simple way to set all tab bits "on"
	*data = (1 << MAX_TAB_TYPES) - 1;

	token = strtok(s,", \t");
	while (token)
	{
		for (j=0; j<MAX_TAB_TYPES; j++)
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
	{
		// not allowed to hide all tabs, because then why even show the area?
		*data = (1 << TAB_SCREENSHOT);
	}
}


// not used
#if 0
const char * GetFolderNameByID(UINT nID)
{
	UINT i;
	extern const FOLDERDATA g_folderData[];
	extern LPEXFOLDERDATA ExtraFolderData[];

	for (i = 0; i < MAX_EXTRA_FOLDERS * MAX_EXTRA_SUBFOLDERS; i++)
		if( ExtraFolderData[i] )
			if (ExtraFolderData[i]->m_nFolderId == nID)
				return ExtraFolderData[i]->m_szTitle;

	for( i = 0; i < MAX_FOLDERS; i++)
		if (g_folderData[i].m_nFolderId == nID)
			return g_folderData[i].m_lpTitle;

	return NULL;
}
#endif

DWORD GetFolderFlags(int folder_index)
{
	LPTREEFOLDER lpFolder = GetFolder(folder_index);

	if (lpFolder)
		return lpFolder->m_dwFlags & F_MASK;

	return 0;
}

/* MSH 20080813
 * Read the folder filters from MAMEui.ini.  This must only
 * be called AFTER the folders have all been created.
 */
void LoadFolderFlags()
{
	LPTREEFOLDER lpFolder;
	int i, numFolders = GetNumFolders();

	for (i = 0; i < numFolders; i++)
	{
		lpFolder = GetFolder(i);

		if (lpFolder)
		{
			char folder_name[400];
			char *ptr;

			// Convert spaces to underscores
			strcpy(folder_name, lpFolder->m_lpTitle);
			ptr = folder_name;
			while (*ptr && *ptr != '\0')
			{
				if ((*ptr == ' ') || (*ptr == '-'))
					*ptr = '_';

				ptr++;
			}

			string option_name = string(folder_name) + "_filters";
		}
	}

	// These are added to our UI ini
	// The normal read will skip them.

	// retrieve the stored values
	for (i = 0; i < numFolders; i++)
	{
		lpFolder = GetFolder(i);

		if (lpFolder)
		{
			char folder_name[400];

			// Convert spaces to underscores
			strcpy(folder_name, lpFolder->m_lpTitle);
			char *ptr = folder_name;
			while (*ptr && *ptr != '\0')
			{
				if ((*ptr == ' ') || (*ptr == '-'))
					*ptr = '_';

				ptr++;
			}
			string option_name = string(folder_name) + "_filters";

			// get entry and decode it
			lpFolder->m_dwFlags |= (settings.int_value(option_name.c_str()) & F_MASK);
		}
	}
}



// Adds our folder flags to winui_options, for saving.
static void AddFolderFlags()
{
	LPTREEFOLDER lpFolder;
	int num_entries = 0, numFolders = GetNumFolders();

	for (int i = 0; i < numFolders; i++)
	{
		lpFolder = GetFolder(i);
		if (lpFolder)
		{
			char folder_name[400];

			// Convert spaces to underscores
			strcpy(folder_name, lpFolder->m_lpTitle);
			char *ptr = folder_name;
			while (*ptr && *ptr != '\0')
			{
				if ((*ptr == ' ') || (*ptr == '-'))
					*ptr = '_';

				ptr++;
			}

			string option_name = string(folder_name) + "_filters";

			// store entry
			settings.setter(option_name.c_str(), lpFolder->m_dwFlags & F_MASK);

			// increment counter
			num_entries++;
		}
	}
}

// Save MAMEUI.ini
void mui_save_ini()
{
	// Add the folder flag to settings.
	AddFolderFlags();
	settings.save_file(mui_ini_filename.c_str());
}

void SaveGameListOptions()
{
	// Save GameInfo.ini - game options.
	game_opts.save_file(gameinfo_ini_filename.c_str());
}

const char * GetVersionString()
{
	return emulator_info::get_build_version();
}

uint32_t GetDriverCacheLower(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return game_opts.cache_lower(drvindex);
}

uint32_t GetDriverCacheUpper(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return game_opts.cache_upper(drvindex);
}

void SetDriverCache(int drvindex, uint32_t val)
{
	if (drvindex >= 0)
		game_opts.cache_upper(drvindex, val);
}

BOOL RequiredDriverCache()
{
	return game_opts.rebuild();
}

void ForceRebuild()
{
	game_opts.force_rebuild();
}

BOOL DriverIsModified(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return BIT(game_opts.cache_lower(drvindex), 12);
}

BOOL DriverIsImperfect(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return (game_opts.cache_lower(drvindex) & 0xff0000) ? true : false; // (NO|IMPERFECT) (CONTROLS|PALETTE|SOUND|GRAPHICS)
}

// from optionsms.cpp (MESSUI)


#define LOG_SOFTWARE 1

void SetSLColumnOrder(int order[])
{
	settings.setter(MESSUI_SL_COLUMN_ORDER, ColumnEncodeStringWithCount(order, SL_COLUMN_MAX));
}

void GetSLColumnOrder(int order[])
{
	ColumnDecodeStringWithCount(settings.getter(MESSUI_SL_COLUMN_ORDER), order, SL_COLUMN_MAX);
}

void SetSLColumnShown(int shown[])
{
	settings.setter(MESSUI_SL_COLUMN_SHOWN, ColumnEncodeStringWithCount(shown, SL_COLUMN_MAX));
}

void GetSLColumnShown(int shown[])
{
	ColumnDecodeStringWithCount(settings.getter(MESSUI_SL_COLUMN_SHOWN), shown, SL_COLUMN_MAX);
}

void SetSLColumnWidths(int width[])
{
	settings.setter(MESSUI_SL_COLUMN_WIDTHS, ColumnEncodeStringWithCount(width, SL_COLUMN_MAX));
}

void GetSLColumnWidths(int width[])
{
	ColumnDecodeStringWithCount(settings.getter(MESSUI_SL_COLUMN_WIDTHS), width, SL_COLUMN_MAX);
}

void SetSLSortColumn(int column)
{
	settings.setter(MESSUI_SL_SORT_COLUMN, column);
}

int GetSLSortColumn()
{
	return settings.int_value(MESSUI_SL_SORT_COLUMN);
}

void SetSLSortReverse(BOOL reverse)
{
	settings.setter(MESSUI_SL_SORT_REVERSED, reverse);
}

BOOL GetSLSortReverse()
{
	return settings.bool_value(MESSUI_SL_SORT_REVERSED);
}

void SetSWColumnOrder(int order[])
{
	settings.setter(MESSUI_SW_COLUMN_ORDER, ColumnEncodeStringWithCount(order, SW_COLUMN_MAX));
}

void GetSWColumnOrder(int order[])
{
	ColumnDecodeStringWithCount(settings.getter(MESSUI_SW_COLUMN_ORDER), order, SW_COLUMN_MAX);
}

void SetSWColumnShown(int shown[])
{
	settings.setter(MESSUI_SW_COLUMN_SHOWN, ColumnEncodeStringWithCount(shown, SW_COLUMN_MAX));
}

void GetSWColumnShown(int shown[])
{
	ColumnDecodeStringWithCount(settings.getter(MESSUI_SW_COLUMN_SHOWN), shown, SW_COLUMN_MAX);
}

void SetSWColumnWidths(int width[])
{
	settings.setter(MESSUI_SW_COLUMN_WIDTHS, ColumnEncodeStringWithCount(width, SW_COLUMN_MAX));
}

void GetSWColumnWidths(int width[])
{
	ColumnDecodeStringWithCount(settings.getter(MESSUI_SW_COLUMN_WIDTHS), width, SW_COLUMN_MAX);
}

void SetSWSortColumn(int column)
{
	settings.setter(MESSUI_SW_SORT_COLUMN, column);
}

int GetSWSortColumn()
{
	return settings.int_value(MESSUI_SW_SORT_COLUMN);
}

void SetSWSortReverse(BOOL reverse)
{
	settings.setter( MESSUI_SW_SORT_REVERSED, reverse);
}

BOOL GetSWSortReverse()
{
	return settings.bool_value(MESSUI_SW_SORT_REVERSED);
}


void SetCurrentSoftwareTab(int val)
{
	settings.setter(MESSUI_SOFTWARE_TAB, val);
}

int GetCurrentSoftwareTab()
{
	return settings.int_value(MESSUI_SOFTWARE_TAB);
}

