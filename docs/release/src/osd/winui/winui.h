// For licensing and usage information, read docs/winui_license.txt
//****************************************************************************

#ifndef WINUI_H
#define WINUI_H

#include <commctrl.h>
#include <commdlg.h>
#include "emu.h"
#include "pool.h"
#include "screenshot.h"
#include "drivenum.h"
#include "romload.h"

// Make sure all MESS features are included
#define MESS

#define MAMENAME "MAME"
#define UI_INI_FILENAME "MAMEUI.ini"

#ifdef PTR64
#define MAMEUINAME MAMENAME "UI64"
#else
#define MAMEUINAME MAMENAME "UI32"
#endif

#define SEARCH_PROMPT "<search here>"

enum
{
	UNKNOWN = -1,
	TAB_PICKER = 0,
	TAB_DISPLAY,
	TAB_MISC,
	NUM_TABS
};

enum
{
	FILETYPE_INPUT_FILES = 1,
	FILETYPE_SAVESTATE_FILES,
	FILETYPE_WAVE_FILES,
	FILETYPE_AVI_FILES,
	FILETYPE_MNG_FILES,
	FILETYPE_EFFECT_FILES,
	FILETYPE_JOYMAP_FILES,
	FILETYPE_DEBUGSCRIPT_FILES,
	FILETYPE_LUASCRIPT_FILES
};


typedef struct
{
	INT resource;
	const char *icon_name;
} ICONDATA;

typedef std::string string;

typedef BOOL (WINAPI *common_file_dialog_proc)(LPOPENFILENAME lpofn);
BOOL CommonFileDialog(common_file_dialog_proc cfd,char *filename, int filetype);

HWND GetMainWindow(void);
HWND GetTreeView(void);
HIMAGELIST GetLargeImageList(void);
HIMAGELIST GetSmallImageList(void);
void SetNumOptionFolders(int count);
void GetRealColumnOrder(int order[]);
HICON LoadIconFromFile(const char *iconname);
void UpdateScreenShot(void);
void ResizePickerControls(HWND hWnd);
void MamePlayGame(void);
int FindIconIndex(int nIconResource);
int FindIconIndexByName(const char *icon_name);
int GetSelectedPick(void);
object_pool *GetMameUIMemoryPool(void);

void UpdateListView(void);

// Move The in "The Title (notes)" to "Title, The (notes)"
char * ModifyThe(const char *str);

// Convert Ampersand so it can display in a static control
char * ConvertAmpersandString(const char *s);

// globalized for painting tree control
HBITMAP GetBackgroundBitmap(void);
HPALETTE GetBackgroundPalette(void);
MYBITMAPINFO* GetBackgroundInfo(void);
BOOL GetUseOldControl(void);
BOOL GetUseXPControl(void);

int GetMinimumScreenShotWindowWidth(void);

// we maintain an array of drivers sorted by name, useful all around
int GetParentIndex(const game_driver *driver);
int GetCompatIndex(const game_driver *driver);
int GetParentRomSetIndex(const game_driver *driver);
int GetGameNameIndex(const char *name);

// sets text in part of the status bar on the main window
void SetStatusBarText(int part_index, const char *message);
void SetStatusBarTextF(int part_index, const char *fmt, ...) ATTR_PRINTF(2,3);

int MameUIMain(HINSTANCE hInstance, LPWSTR lpCmdLine, int nCmdShow);

BOOL MouseHasBeenMoved(void);

const char * GetSearchText(void);

// This function truncates a long string, replacing the end with ...
string longdots(string incoming, uint16_t howmany)
{
	// Firstly, find out if it's multi-line text
	size_t i = incoming.find_first_of("\n");
	// If so, truncate at first newline
	if (i != string::npos)
		incoming = incoming.substr(0, i);
	// Now assume all is ok
	string outgoing = incoming;
	// But if it's too long, replace the excess with dots
	if ((howmany > 5) && (incoming.length() > howmany))
		outgoing = incoming.substr(0, howmany) + "...";
	return outgoing;
}
#endif
