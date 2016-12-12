// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#pragma once

#ifndef WINUI_H
#define WINUI_H

// standard Windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <uxtheme.h>

// standard C headers
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <time.h>

// MAME headers
#include "emu.h"
#include "options.h"
#include "pool.h"
#include "unzip.h"
#include "winutf8.h"
#include "strconv.h"
#include "drivenum.h"
#include "winmain.h"
#include "png.h"
#include "window.h"
#include "sound\samples.h"
#include "..\frontend\mame\audit.h"
#include "..\frontend\mame\mame.h"
#include "..\frontend\mame\mameopts.h"
#include "..\frontend\mame\language.h"
#include "..\frontend\mame\pluginopts.h"
#include "..\frontend\mame\ui\moptions.h"

// special Windows headers, after MAME ones
#include <dinput.h>
#include <shlwapi.h>
#include <shlobj.h>

// MAMEUIFX headers
#include "resource.h"
#include "winui_util.h"
#include "winui_opts.h"
#include "properties.h"
#include "winui_audit.h"
#include "directories.h"
#include "datafile.h"
#include "datamap.h"
#include "columnedit.h"
#include "picker.h"
#include "tabview.h"
#include "bitmask.h"
#include "treeview.h"
#include "splitters.h"
#include "history.h"
#include "dialogs.h"
#include "dinputjoy.h"
#include "dxdecode.h"   
#include "screenshot.h"

#ifndef TVS_EX_DOUBLEBUFFER
#define TVS_EX_DOUBLEBUFFER		0x0004
#endif

#ifndef TVM_SETEXTENDEDSTYLE
#define TVM_SETEXTENDEDSTYLE		(TV_FIRST + 44)
#endif

#ifdef PTR64
#define MAMEUINAME			"MAMEUI64"
#else
#define MAMEUINAME			"MAMEUI32"
#endif
#define MAMENAME			"MAME"

#define MAMEUIFX_VERSION	"0.177 (" __DATE__")"
#define MAME_VERSION		"0.177"

#define SEARCH_PROMPT		"<search here>"

/* Highly useful macro for compile-time knowledge of an array size */
#define WINUI_ARRAY_LENGTH(x)		(sizeof(x) / sizeof(x[0]))

/* For future use? though here is the best place to define them */
#define COLOR_WINXP			RGB(236, 233, 216)
#define COLOR_SILVER		RGB(224, 223, 227)
#define COLOR_ZUNE			RGB(226, 226, 226)
#define COLOR_ROYALE		RGB(235, 233, 237)
#define COLOR_WIN7			RGB(240, 240, 240)
#define COLOR_WHITE			RGB(255, 255, 255)

#define MM_PLAY_GAME		(WM_APP + 15000)

#define JOYGUI_MS 			100

#define JOYGUI_TIMER 		1
#define SCREENSHOT_TIMER	2

/* Max size of a sub-menu */
#define DBU_MIN_WIDTH		512
#define DBU_MIN_HEIGHT		250

//I could not find a predefined value for this event and docs just say it has 1 for the parameter
#define TOOLBAR_EDIT_ACCELERATOR_PRESSED		1

#ifndef StatusBar_GetItemRect
#define StatusBar_GetItemRect(hWnd, iPart, lpRect) \
    SendMessage(hWnd, SB_GETRECT, iPart, (LPARAM)lpRect)
#endif

#ifndef ToolBar_CheckButton
#define ToolBar_CheckButton(hWnd, idButton, fCheck) \
    SendMessage(hWnd, TB_CHECKBUTTON, idButton, MAKELPARAM(fCheck, 0))
#endif

/* Which edges of a control are anchored to the corresponding side of the parent window */
#define RA_LEFT				0x01
#define RA_RIGHT			0x02
#define RA_TOP				0x04
#define RA_BOTTOM			0x08
#define RA_ALL				0x0F
#define RA_END				0
#define RA_ID				1
#define RA_HWND				2

#define SPLITTER_WIDTH		4
#define MIN_VIEW_WIDTH		10

#define NUM_TOOLBUTTONS     WINUI_ARRAY_LENGTH(tbb)
#define NUM_TOOLTIPS 		(13)

enum
{
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
	FILETYPE_SHADER_FILES,
	FILETYPE_CHEAT_FILES,
	FILETYPE_BGFX_FILES,
	FILETYPE_LUASCRIPT_FILES
};

int MameUIMain(HINSTANCE hInstance, LPWSTR lpCmdLine);
typedef int (WINAPI *common_file_dialog_proc)(LPOPENFILENAME lpofn);
bool CommonFileDialog(common_file_dialog_proc cfd, char *filename, int filetype, bool saving);
HWND GetMainWindow(void);
HWND GetTreeView(void);
HWND GetProgressBar(void);
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
int GetMinimumScreenShotWindowWidth(void);
// we maintain an array of drivers sorted by name, useful all around
int GetParentIndex(const game_driver *driver);
int GetParentRomSetIndex(const game_driver *driver);
int GetSrcDriverIndex(const char *name);
// sets text in part of the status bar on the main window
void SetStatusBarText(int part_index, const char *message);
void SetStatusBarTextF(int part_index, const char *fmt, ...);
const char * GetSearchText(void);

#endif
