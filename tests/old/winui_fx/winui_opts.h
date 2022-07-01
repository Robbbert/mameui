// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#pragma once

#ifndef MUI_OPTS_H
#define MUI_OPTS_H

#define GLOBAL_OPTIONS -1
#define FOLDER_OPTIONS -2

#define OPTIONS_TYPE_GLOBAL -1
#define OPTIONS_TYPE_FOLDER -2

#define UNKNOWN -1

#define MUIOPTION_VERSION						"version"
#define MUIOPTION_EXIT_DIALOG					"confirm_exit"
#define MUIOPTION_NOROMS_GAMES					"display_no_roms_games"
#define MUIOPTION_TRAY_ICON						"minimize_tray_icon"
#define MUIOPTION_LIST_MODE						"list_mode"
#define MUIOPTION_JOYSTICK_IN_INTERFACE			"joystick_in_interface"
#define MUIOPTION_CYCLE_SCREENSHOT				"cycle_screenshot"
#define MUIOPTION_STRETCH_SCREENSHOT_LARGER		"stretch_screenshot_larger"
#define MUIOPTION_SCREENSHOT_BORDER_SIZE		"screenshot_bordersize"
#define MUIOPTION_SCREENSHOT_BORDER_COLOR		"screenshot_bordercolor"
#define MUIOPTION_INHERIT_FILTER				"inherit_filter"
#define MUIOPTION_DEFAULT_FOLDER_ID				"default_folder_id"
#define MUIOPTION_SHOW_IMAGE_SECTION			"show_image_section"
#define MUIOPTION_SHOW_FOLDER_SECTION			"show_folder_section"
#define MUIOPTION_HIDE_FOLDERS					"hide_folders"
#define MUIOPTION_SHOW_STATUS_BAR				"show_status_bar"
#define MUIOPTION_SHOW_TABS						"show_tabs"
#define MUIOPTION_SHOW_TOOLBAR					"show_tool_bar"
#define MUIOPTION_CURRENT_TAB					"current_tab"
#define MUIOPTION_WINDOW_X						"window_x"
#define MUIOPTION_WINDOW_Y						"window_y"
#define MUIOPTION_WINDOW_WIDTH					"window_width"
#define MUIOPTION_WINDOW_HEIGHT					"window_height"
#define MUIOPTION_WINDOW_STATE					"window_state"
#define MUIOPTION_CUSTOM_COLOR					"custom_color"
#define MUIOPTION_USE_BROKEN_ICON				"use_broken_icon"
#define MUIOPTION_ENABLE_INDENT					"enable_clones_indentation"
#define MUIOPTION_ENABLE_FASTAUDIT				"enable_fastaudit"
#define MUIOPTION_ENABLE_SEVENZIP				"enable_sevenzip"
#define MUIOPTION_GUI_FONT						"gui_font"
#define MUIOPTION_LIST_FONT						"list_font"
#define MUIOPTION_HISTORY_FONT					"history_font"
#define MUIOPTION_TREE_FONT						"tree_font"
#define MUIOPTION_LIST_COLOR					"list_color"
#define MUIOPTION_HISTORY_COLOR					"history_color"
#define MUIOPTION_TREE_COLOR					"tree_color"
#define MUIOPTION_TREEBG_COLOR					"tree_background_color"
#define MUIOPTION_LISTBG_COLOR					"list_background_color"
#define MUIOPTION_HISTORYBG_COLOR				"history_background_color"
#define MUIOPTION_HIDE_TABS						"hide_tabs"
#define MUIOPTION_HISTORY_TAB					"history_tab"
#define MUIOPTION_COLUMN_WIDTHS					"column_widths"
#define MUIOPTION_COLUMN_ORDER					"column_order"
#define MUIOPTION_COLUMN_SHOWN					"column_shown"
#define MUIOPTION_SPLITTERS						"splitters"
#define MUIOPTION_SORT_COLUMN					"sort_column"
#define MUIOPTION_SORT_REVERSED					"sort_reversed"
#define MUIOPTION_FLYER_DIRECTORY				"flyer_directory"
#define MUIOPTION_CABINET_DIRECTORY				"cabinet_directory"
#define MUIOPTION_MARQUEE_DIRECTORY				"marquee_directory"
#define MUIOPTION_TITLE_DIRECTORY				"title_directory"
#define MUIOPTION_CPANEL_DIRECTORY				"cpanel_directory"
#define MUIOPTION_PCB_DIRECTORY					"pcb_directory"
#define MUIOPTION_SCORES_DIRECTORY				"scores_directory"
#define MUIOPTION_BOSSES_DIRECTORY				"bosses_directory"
#define MUIOPTION_VERSUS_DIRECTORY				"versus_directory"
#define MUIOPTION_ENDS_DIRECTORY				"ends_directory"
#define MUIOPTION_GAMEOVER_DIRECTORY			"gameover_directory"
#define MUIOPTION_HOWTO_DIRECTORY				"howto_directory"
#define MUIOPTION_SELECT_DIRECTORY				"select_directory"
#define MUIOPTION_LOGO_DIRECTORY				"logo_directory"
#define MUIOPTION_ARTWORK_DIRECTORY				"artwork_directory"
#define MUIOPTION_ICONS_DIRECTORY				"icons_directory"
#define MUIOPTION_FOLDER_DIRECTORY				"folder_directory"
#define MUIOPTION_MOVIES_DIRECTORY				"movies_directory"
#define MUIOPTION_AUDIO_DIRECTORY				"audio_directory"
#define MUIOPTION_GUI_DIRECTORY					"gui_ini_directory"
#define MUIOPTION_DATS_DIRECTORY				"datafile_directory"
#define MUIOPTION_UI_JOY_UP						"ui_joy_up"
#define MUIOPTION_UI_JOY_DOWN					"ui_joy_down"
#define MUIOPTION_UI_JOY_LEFT					"ui_joy_left"
#define MUIOPTION_UI_JOY_RIGHT					"ui_joy_right"
#define MUIOPTION_UI_JOY_START					"ui_joy_start"
#define MUIOPTION_UI_JOY_PGUP					"ui_joy_pgup"
#define MUIOPTION_UI_JOY_PGDWN					"ui_joy_pgdwn"
#define MUIOPTION_UI_JOY_HOME					"ui_joy_home"
#define MUIOPTION_UI_JOY_END					"ui_joy_end"
#define MUIOPTION_UI_JOY_SS_CHANGE				"ui_joy_ss_change"
#define MUIOPTION_UI_JOY_HISTORY_UP				"ui_joy_history_up"
#define MUIOPTION_UI_JOY_HISTORY_DOWN			"ui_joy_history_down"
#define MUIOPTION_DEFAULT_GAME					"default_game"

#define INTERFACE_INI_FILENAME 					"interface"
#define GAMELIST_INI_FILENAME 					"gamelist"
#define DEFAULT_INI_FILENAME 					"mame"
#define INTERNAL_UI_INI_FILENAME 				"ui"
#define PLUGINS_INI_FILENAME 					"plugin"

// Because we have added the Options after MAX_TAB_TYPES, we have to subtract 2 here
// (that's how many options we have after MAX_TAB_TYPES)
#define TAB_SUBTRACT 2

// Various levels of ini's we can edit.
typedef enum 
{
	OPTIONS_GLOBAL = 0,
	OPTIONS_HORIZONTAL,
	OPTIONS_VERTICAL,
	OPTIONS_RASTER,
	OPTIONS_VECTOR,
	OPTIONS_SOURCE,
	OPTIONS_PARENT,
	OPTIONS_GAME,
	OPTIONS_MAX
} OPTIONS_TYPE;

enum
{
	COLUMN_GAMES = 0,
	COLUMN_ROMNAME,
	COLUMN_SOURCEFILE,
	COLUMN_MANUFACTURER,
	COLUMN_YEAR,
	COLUMN_CLONE,
	COLUMN_PLAYED,
	COLUMN_PLAYTIME,
	COLUMN_MAX
};

typedef struct
{
	int x, y, width, height;
} AREA;

typedef struct
{
	char* screen;
	char* aspect;
	char* resolution;
	char* view;
} ScreenParams;

// List of artwork types to display in the screen shot area
enum
{
	// these must match array of strings image_tabs_long_name in mui_opts.c
	// if you add new Tabs, be sure to also add them to the ComboBox init in dialogs.c
	TAB_SCREENSHOT = 0,
	TAB_TITLE,
	TAB_SCORES,
	TAB_HOWTO,
	TAB_SELECT,
	TAB_VERSUS,
	TAB_BOSSES,
	TAB_ENDS,
	TAB_GAMEOVER,
	TAB_LOGO,
	TAB_ARTWORK,
	TAB_FLYER,
	TAB_CABINET,
	TAB_MARQUEE,
	TAB_CONTROL_PANEL,
	TAB_PCB,
	TAB_HISTORY,
	MAX_TAB_TYPES,
	TAB_ALL,
	TAB_NONE
};

class winui_options : public core_options
{
public:
	// construction/destruction
	winui_options();

private:
	static const options_entry s_option_entries[];
};

class gamelist_options
{
public:
	// construction/destruction
	gamelist_options();

	int  rom(int index)                 { assert(0 <= index && index < driver_list::total()); return m_list[index].rom; }
	void rom(int index, int val)        { assert(0 <= index && index < driver_list::total()); m_list[index].rom = val; }
	int  cache(int index)               { assert(0 <= index && index < driver_list::total()); return m_list[index].cache; }
	void cache(int index, int val)      { assert(0 <= index && index < driver_list::total()); m_list[index].cache = val; }
	int  play_count(int index)          { assert(0 <= index && index < driver_list::total()); return m_list[index].play_count; }
	void play_count(int index, int val) { assert(0 <= index && index < driver_list::total()); m_list[index].play_count = val; }
	int  play_time(int index)           { assert(0 <= index && index < driver_list::total()); return m_list[index].play_time; }
	void play_time(int index, int val)  { assert(0 <= index && index < driver_list::total()); m_list[index].play_time = val; }

	void add_entries(void);
	osd_file::error load_options(const std::string &filename);
	osd_file::error save_options(const std::string &filename);
	void load_settings(void);
	void save_settings(void);
	void load_settings(const char *str, int index);
private:
	core_options m_info;
	int          m_total;

	struct driver_options
	{
		int	rom;
		int	cache;
		int	play_count;		
		int	play_time;
	};

	std::vector<driver_options>	m_list;
};

windows_options & MameUIGlobal(void);

void OptionsInit(void);
void SetDirectories(windows_options &opts);
void LoadOptions(windows_options &opts, OPTIONS_TYPE opt_type, int game_num);
void SaveOptions(OPTIONS_TYPE opt_type, windows_options &opts, int game_num);
void LoadFolderFlags(void);
const char* GetFolderNameByID(UINT nID);
void SaveInterface(void);
void LoadInternalUI(void);
void SaveInternalUI(void);
void SavePlugins(void);
void SaveGameDefaults(void);
void SaveGameList(void);
void ResetInterface(void);
void ResetInternalUI(void);
void ResetGameDefaults(void);
void ResetAllGameOptions(void);
const char* GetImageTabLongName(int tab_index);
const char* GetImageTabShortName(int tab_index);
void SetViewMode(int val);
int  GetViewMode(void);
void SetVersionCheck(bool version_check);
bool GetVersionCheck(void);
void SetJoyGUI(bool use_joygui);
bool GetJoyGUI(void);
void SetCycleScreenshot(int cycle_screenshot);
int GetCycleScreenshot(void);
void SetStretchScreenShotLarger(bool stretch);
bool GetStretchScreenShotLarger(void);
void SetScreenshotBorderSize(int size);
int GetScreenshotBorderSize(void);
void SetScreenshotBorderColor(COLORREF uColor);
COLORREF GetScreenshotBorderColor(void);
void SetFilterInherit(bool inherit);
bool GetFilterInherit(void);
void SetUseBrokenIcon(bool broken);
bool GetUseBrokenIcon(void);
void SetSavedFolderID(int val);
int GetSavedFolderID(void);
void SetShowScreenShot(bool val);
bool GetShowScreenShot(void);
void SetShowFolderList(bool val);
bool GetShowFolderList(void);
bool GetShowFolder(int folder);
void SetShowFolder(int folder,bool show);
void SetShowStatusBar(bool val);
bool GetShowStatusBar(void);
void SetShowToolBar(bool val);
bool GetShowToolBar(void);
void SetShowTabCtrl(bool val);
bool GetShowTabCtrl(void);
void SetCurrentTab(const char *shortname);
const char* GetCurrentTab(void);
void SetDefaultGame(const char *name);
const char* GetDefaultGame(void);
void SetWindowArea(const AREA *area);
void GetWindowArea(AREA *area);
void SetWindowState(int state);
int GetWindowState(void);
void SetColumnWidths(int widths[]);
void GetColumnWidths(int widths[]);
void SetColumnOrder(int order[]);
void GetColumnOrder(int order[]);
void SetColumnShown(int shown[]);
void GetColumnShown(int shown[]);
void SetSplitterPos(int splitterId, int pos);
int  GetSplitterPos(int splitterId);
void SetCustomColor(int iIndex, COLORREF uColor);
COLORREF GetCustomColor(int iIndex);
void GetGuiFont(LOGFONT *font);
void SetListFont(const LOGFONT *font);
void GetListFont(LOGFONT *font);
void SetHistoryFont(const LOGFONT *font);
void GetHistoryFont(LOGFONT *font);
void SetTreeFont(const LOGFONT *font);
void GetTreeFont(LOGFONT *font);
DWORD GetFolderFlags(int folder_index);
void SetListFontColor(COLORREF uColor);
COLORREF GetListFontColor(void);
void SetHistoryFontColor(COLORREF uColor);
COLORREF GetHistoryFontColor(void);
void SetTreeFontColor(COLORREF uColor);
COLORREF GetTreeFontColor(void);
void SetFolderBgColor(COLORREF uColor);
COLORREF GetFolderBgColor(void);
void SetHistoryBgColor(COLORREF uColor);
COLORREF GetHistoryBgColor(void);
void SetListBgColor(COLORREF uColor);
COLORREF GetListBgColor(void);
int GetHistoryTab(void);
void SetHistoryTab(int tab,bool show);
int GetShowTab(int tab);
void SetShowTab(int tab,bool show);
bool AllowedToSetShowTab(int tab,bool show);
void SetSortColumn(int column);
int  GetSortColumn(void);
void SetSortReverse(bool reverse);
bool GetSortReverse(void);
void SetDisplayNoRomsGames(bool value);
bool GetDisplayNoRomsGames(void);
void SetExitDialog(bool value);
bool GetExitDialog(void);
void SetEnableIndent(bool value);
bool GetEnableIndent(void);
void SetMinimizeTrayIcon(bool value);
bool GetMinimizeTrayIcon(void);
void SetEnableFastAudit(bool value);
bool GetEnableFastAudit(void);
void SetEnableSevenZip(bool value);
bool GetEnableSevenZip(void);
void SetEnableDatafiles(bool value);
bool GetEnableDatafiles(void);
void SetSkipBiosMenu(bool value);
bool GetSkipBiosMenu(void);
const char* GetRomDirs(void);
void SetRomDirs(const char* paths);
const char* GetSampleDirs(void);
void  SetSampleDirs(const char* paths);
const char* GetIniDir(void);
void SetIniDir(const char *path);
const char* GetCfgDir(void);
void SetCfgDir(const char* path);
const char* GetGLSLDir(void);
void SetGLSLDir(const char* path);
const char* GetBGFXDir(void);
void SetBGFXDir(const char* path);
const char* GetPluginsDir(void);
void SetPluginsDir(const char* path);
const char* GetNvramDir(void);
void SetNvramDir(const char* path);
const char* GetInpDir(void);
void SetInpDir(const char* path);
const char* GetImgDir(void);
void SetImgDir(const char* path);
const char* GetStateDir(void);
void SetStateDir(const char* path);
const char* GetArtDir(void);
void SetArtDir(const char* path);
const char* GetFlyerDir(void);
void SetFlyerDir(const char* path);
const char* GetCabinetDir(void);
void SetCabinetDir(const char* path);
const char* GetMarqueeDir(void);
void SetMarqueeDir(const char* path);
const char* GetTitlesDir(void);
void SetTitlesDir(const char* path);
const char* GetControlPanelDir(void);
void SetControlPanelDir(const char *path);
const char* GetPcbDir(void);
void SetPcbDir(const char *path);
const char* GetMoviesDir(void);
void SetMoviesDir(const char *path);
const char* GetVideoDir(void);
void SetVideoDir(const char *path);
const char* GetAudioDir(void);
void SetAudioDir(const char *path);
const char* GetGuiDir(void);
void SetGuiDir(const char *path);
const char* GetDatsDir(void);
void SetDatsDir(const char *path);
const char* GetScoresDir(void);
void SetScoresDir(const char *path);
const char* GetBossesDir(void);
void SetBossesDir(const char *path);
const char* GetVersusDir(void);
void SetVersusDir(const char *path);
const char* GetEndsDir(void);
void SetEndsDir(const char *path);
const char* GetGameOverDir(void);
void SetGameOverDir(const char *path);
const char* GetHowToDir(void);
void SetHowToDir(const char *path);
const char* GetSelectDir(void);
void SetSelectDir(const char *path);
const char* GetLogoDir(void);
void SetLogoDir(const char *path);
const char* GetArtworkDir(void);
void SetArtworkDir(const char *path);
const char* GetHLSLDir(void);
void SetHLSLDir(const char* path);
const char* GetDiffDir(void);
void SetDiffDir(const char* path);
const char* GetIconsDir(void);
void SetIconsDir(const char* path);
const char* GetCtrlrDir(void);
void SetCtrlrDir(const char* path);
const char* GetCommentDir(void);
void SetCommentDir(const char* path);
const char* GetFolderDir(void);
void SetFolderDir(const char* path);
const char* GetFontDir(void);
void SetFontDir(const char* path);
const char* GetCrosshairDir(void);
void SetCrosshairDir(const char* path);
const char* GetLanguageDir(void);
void SetLanguageDir(const char* path);
void ResetGameOptions(int driver_index);
int GetRomAuditResults(int driver_index);
void SetRomAuditResults(int driver_index, int audit_results);
void IncrementPlayCount(int driver_index);
int GetPlayCount(int driver_index);
void ResetPlayCount(int driver_index);
void IncrementPlayTime(int driver_index, int playtime);
int GetPlayTime(int driver_index);
void GetTextPlayTime(int driver_index, char *buf);
void ResetPlayTime(int driver_index);
/* joystick */
int GetUIJoyUp(int joycodeIndex);
void SetUIJoyUp(int joycodeIndex, int val);
int GetUIJoyDown(int joycodeIndex);
void SetUIJoyDown(int joycodeIndex, int val);
int GetUIJoyLeft(int joycodeIndex);
void SetUIJoyLeft(int joycodeIndex, int val);
int GetUIJoyRight(int joycodeIndex);
void SetUIJoyRight(int joycodeIndex, int val);
int GetUIJoyStart(int joycodeIndex);
void SetUIJoyStart(int joycodeIndex, int val);
int GetUIJoyPageUp(int joycodeIndex);
void SetUIJoyPageUp(int joycodeIndex, int val);
int GetUIJoyPageDown(int joycodeIndex);
void SetUIJoyPageDown(int joycodeIndex, int val);
int GetUIJoyHome(int joycodeIndex);
void SetUIJoyHome(int joycodeIndex, int val);
int GetUIJoyEnd(int joycodeIndex);
void SetUIJoyEnd(int joycodeIndex, int val);
int GetUIJoySSChange(int joycodeIndex);
void SetUIJoySSChange(int joycodeIndex, int val);
int GetUIJoyHistoryUp(int joycodeIndex);
void SetUIJoyHistoryUp(int joycodeIndex, int val);
int GetUIJoyHistoryDown(int joycodeIndex);
void SetUIJoyHistoryDown(int joycodeIndex, int val);
void ColumnEncodeStringWithCount(const int *value, char *str, int count);
void ColumnDecodeStringWithCount(const char* str, int *value, int count);
int GetDriverCache(int driver_index);
void SetDriverCache(int driver_index, int val);
bool RequiredDriverCache(bool check = false);
void SetRequiredDriverCacheStatus(void);
bool GetRequiredDriverCacheStatus(void);

#endif
