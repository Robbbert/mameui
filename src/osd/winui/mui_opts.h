// For licensing and usage information, read docs/release/winui_license.txt
// MASTER
//****************************************************************************

#ifndef WINUI_MUI_OPTS_H
#define WINUI_MUI_OPTS_H

#include "winmain.h"
#include "winui.h"

// List of columns in the main game list
enum
{
	COLUMN_GAMES = 0,
	COLUMN_SRCDRIVERS,
	COLUMN_DIRECTORY,
	COLUMN_TYPE,
	COLUMN_ORIENTATION,
	COLUMN_MANUFACTURER,
	COLUMN_YEAR,
	COLUMN_PLAYED,
	COLUMN_PLAYTIME,
	COLUMN_CLONE,
	COLUMN_TRACKBALL,
	COLUMN_SAMPLES,
	COLUMN_ROMS,
	COLUMN_MAX
};

#define LOCAL_OPTIONS   -10

typedef struct
{
	int x, y, width, height;
} AREA;

typedef struct
{
	char *screen;
	char *aspect;
	char *resolution;
	char *view;
} ScreenParams;

// List of artwork types to display in the screen shot area
enum
{
	// these must match array of strings image_tabs_long_name in mui_opts.cpp
	// if you add new Tabs, be sure to also add them to the ComboBox init in dialogs.cpp
	TAB_ARTWORK = 0,
	TAB_BOSSES,
	TAB_CABINET,
	TAB_CONTROL_PANEL,
	TAB_COVER,
	TAB_ENDS,
	TAB_FLYER,
	TAB_GAMEOVER,
	TAB_HOWTO,
	TAB_LOGO,
	TAB_MARQUEE,
	TAB_PCB,
	TAB_SCORES,
	TAB_SELECT,
	TAB_SCREENSHOT,
	TAB_TITLE,
	TAB_VERSUS,
	TAB_HISTORY,
	MAX_TAB_TYPES,
	BACKGROUND,
	TAB_ALL,
	TAB_NONE
};
// Because we have added the Options after MAX_TAB_TYPES, we have to subtract 3 here
// (that's how many options we have after MAX_TAB_TYPES)
#define TAB_SUBTRACT 3

void OptionsInit();

#define OPTIONS_TYPE_GLOBAL -1
#define OPTIONS_TYPE_FOLDER -2

void LoadFolderFlags();

// Start interface to directories.h

const string GetManualsDir();
void SetManualsDir(const char* path);

const string GetVideoDir();
void SetVideoDir(const char *path);

// End interface to directories.h

void mui_save_ini();
void SaveGameListOptions();

void ResetGUI();


const char * GetImageTabLongName(int tab_index);
const char * GetImageTabShortName(int tab_index);

void SetViewMode(int val);
int  GetViewMode();

void SetGameCheck(BOOL game_check);
BOOL GetGameCheck();

void SetEnableIndent(bool value);
bool GetEnableIndent();

void SetJoyGUI(BOOL use_joygui);
BOOL GetJoyGUI();

void SetKeyGUI(BOOL use_keygui);
BOOL GetKeyGUI();

void SetCycleScreenshot(int cycle_screenshot);
int GetCycleScreenshot();

void SetStretchScreenShotLarger(BOOL stretch);
BOOL GetStretchScreenShotLarger();

void SetScreenshotBorderSize(int size);
int GetScreenshotBorderSize();

void SetScreenshotBorderColor(COLORREF uColor);
COLORREF GetScreenshotBorderColor();

void SetFilterInherit(BOOL inherit);
BOOL GetFilterInherit();

void SetOffsetClones(BOOL offset);
BOOL GetOffsetClones();

void SetSavedFolderID(UINT val);
UINT GetSavedFolderID();

void SetOverrideRedX(BOOL val);
BOOL GetOverrideRedX();

BOOL GetShowFolder(int folder);
void SetShowFolder(int folder,BOOL show);

void SetShowStatusBar(BOOL val);
BOOL GetShowStatusBar();

void SetShowToolBar(BOOL val);
BOOL GetShowToolBar();

void SetShowTabCtrl(BOOL val);
BOOL GetShowTabCtrl();

void SetCurrentTab(int val);
int GetCurrentTab();

void SetDefaultGame(int val);
uint32_t GetDefaultGame();

void SetWindowArea(const AREA *area);
void GetWindowArea(AREA *area);

void SetWindowState(UINT state);
UINT GetWindowState();

void SetWindowPanes(int val);
UINT GetWindowPanes();

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

void SetListFont(const LOGFONT *font);
void GetListFont(LOGFONT *font);

DWORD GetFolderFlags(int folder_index);

void SetListFontColor(COLORREF uColor);
COLORREF GetListFontColor();

void SetListCloneColor(COLORREF uColor);
COLORREF GetListCloneColor();

int GetHistoryTab();
void SetHistoryTab(int tab,BOOL show);

int GetShowTab(int tab);
void SetShowTab(int tab,BOOL show);
BOOL AllowedToSetShowTab(int tab,BOOL show);

void SetSortColumn(int column);
int  GetSortColumn();

void SetSortReverse(BOOL reverse);
BOOL GetSortReverse();

const string GetBgDir();
void SetBgDir(const char *path);

int GetRomAuditResults(int);
void SetRomAuditResults(int, int audit_results);

int GetSampleAuditResults(int);
void SetSampleAuditResults(int, int audit_results);

void IncrementPlayCount(int);
uint32_t GetPlayCount(int);
void ResetPlayCount(int);

void IncrementPlayTime(int, uint32_t playtime);
uint32_t GetPlayTime(int);
void GetTextPlayTime(int, char *buf);
void ResetPlayTime(int);

const char * GetVersionString();




// Keyboard control of ui
input_seq* Get_ui_key_up();
input_seq* Get_ui_key_down();
input_seq* Get_ui_key_left();
input_seq* Get_ui_key_right();
input_seq* Get_ui_key_start();
input_seq* Get_ui_key_pgup();
input_seq* Get_ui_key_pgdwn();
input_seq* Get_ui_key_home();
input_seq* Get_ui_key_end();
input_seq* Get_ui_key_ss_change();
input_seq* Get_ui_key_history_up();
input_seq* Get_ui_key_history_down();

input_seq* Get_ui_key_context_filters();
input_seq* Get_ui_key_select_random();
input_seq* Get_ui_key_game_audit();
input_seq* Get_ui_key_game_properties();
input_seq* Get_ui_key_help_contents();
input_seq* Get_ui_key_update_gamelist();
input_seq* Get_ui_key_view_folders();
input_seq* Get_ui_key_view_fullscreen();
input_seq* Get_ui_key_view_pagetab();
input_seq* Get_ui_key_view_picture_area();
input_seq* Get_ui_key_view_software_area();
input_seq* Get_ui_key_view_status();
input_seq* Get_ui_key_view_toolbars();

input_seq* Get_ui_key_view_tab_cabinet();
input_seq* Get_ui_key_view_tab_cpanel();
input_seq* Get_ui_key_view_tab_flyer();
input_seq* Get_ui_key_view_tab_history();
input_seq* Get_ui_key_view_tab_marquee();
input_seq* Get_ui_key_view_tab_screenshot();
input_seq* Get_ui_key_view_tab_title();
input_seq* Get_ui_key_view_tab_pcb();
input_seq* Get_ui_key_quit();


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

int GetUIJoyExec(int joycodeIndex);
void SetUIJoyExec(int joycodeIndex, int val);

const string GetExecCommand();
void SetExecCommand(char *cmd);

int GetExecWait();
void SetExecWait(int wait);

BOOL GetHideMouseOnStartup();
void SetHideMouseOnStartup(BOOL hide);

BOOL GetRunFullScreen();
void SetRunFullScreen(BOOL fullScreen);

uint32_t GetDriverCacheLower(int);
uint32_t GetDriverCacheUpper(int);
void SetDriverCache(int, uint32_t val);
BOOL RequiredDriverCache();
void ForceRebuild();
BOOL DriverIsModified(int);
BOOL DriverIsImperfect(int);
string GetGameName(int);

// from optionsms.h (MESSUI)

enum
{
	SW_COLUMN_IMAGES,
	SW_COLUMN_MAX
};

enum
{
	SL_COLUMN_IMAGES,
	SL_COLUMN_GOODNAME,
	SL_COLUMN_MANUFACTURER,
	SL_COLUMN_YEAR,
	SL_COLUMN_PLAYABLE,
	SL_COLUMN_USAGE,
	SL_COLUMN_MAX
};

void SetSWColumnWidths(int widths[]);
void GetSWColumnWidths(int widths[]);
void SetSWColumnOrder(int order[]);
void GetSWColumnOrder(int order[]);
void SetSWColumnShown(int shown[]);
void GetSWColumnShown(int shown[]);
void SetSWSortColumn(int column);
int  GetSWSortColumn();
void SetSWSortReverse(BOOL reverse);
BOOL GetSWSortReverse();

void SetSLColumnWidths(int widths[]);
void GetSLColumnWidths(int widths[]);
void SetSLColumnOrder(int order[]);
void GetSLColumnOrder(int order[]);
void SetSLColumnShown(int shown[]);
void GetSLColumnShown(int shown[]);
void SetSLSortColumn(int column);
int  GetSLSortColumn();
void SetSLSortReverse(BOOL reverse);
BOOL GetSLSortReverse();


void SetCurrentSoftwareTab(int val);
int GetCurrentSoftwareTab();


#endif

