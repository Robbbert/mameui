// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include "winui.h"

/***************************************************************************
    Internal structures
 ***************************************************************************/

typedef struct
{
	TCHAR m_Directories[MAX_DIRS][MAX_PATH];
	int m_NumDirectories;
	bool m_bModified;
} tPath;

typedef struct
{
	tPath *m_Path;
	TCHAR *m_tDirectory;
} tDirInfo;

typedef struct
{
	const char *lpName;
	const char* (*pfnGetTheseDirs)(void);
	void (*pfnSetTheseDirs)(const char *lpDirs);
	bool bMulti;
	int nDirDlgFlags;
} DIRECTORYINFO;

static const DIRECTORYINFO g_directoryInfo[] =
{
	{ "ROMs",                  	GetRomDirs,      SetRomDirs,      true,  DIRDLG_ROM },
	{ "Samples",               	GetSampleDirs,   SetSampleDirs,   true,  0 },
	{ "Config",		           	GetCfgDir,       SetCfgDir,       false, 0 },
	{ "Snapshots",             	GetImgDir,       SetImgDir,       false, 0 },
	{ "Input Recordings",       GetInpDir,       SetInpDir,       false, 0 },
	{ "SaveStates",             GetStateDir,     SetStateDir,     false, 0 },
	{ "Artworks",         		GetArtDir,       SetArtDir,       false, 0 },
	{ "NVRAMs",           		GetNvramDir,     SetNvramDir,     false, 0 },
	{ "Controllers",      		GetCtrlrDir,     SetCtrlrDir,     false, 0 },
	{ "Crosshairs",       		GetCrosshairDir, SetCrosshairDir, false, 0 },
	{ "CHD Diffs",        		GetDiffDir, 	 SetDiffDir, 	  false, 0 },
	{ "HLSL files",            	GetHLSLDir, 	 SetHLSLDir, 	  false, 0 },
	{ "GLSL shaders",    		GetGLSLDir, 	 SetGLSLDir, 	  false, 0 },
	{ "BGFX chains",    		GetBGFXDir, 	 SetBGFXDir, 	  false, 0 },
	{ "LUA plugins",     		GetPluginsDir, 	 SetPluginsDir,	  false, 0 },
	{ "Fonts",            		GetFontDir,      SetFontDir,      false, 0 },
	{ "Videos",           		GetVideoDir,     SetVideoDir,     false, 0 },
	{ "ProgettoSnaps movies",  	GetMoviesDir,    SetMoviesDir,    false, 0 },
	{ "Audios",           		GetAudioDir,     SetAudioDir,     false, 0 },
	{ "Flyers",                	GetFlyerDir,     SetFlyerDir,     false, 0 },
	{ "Cabinets",              	GetCabinetDir,   SetCabinetDir,   false, 0 },
	{ "Marquees",              	GetMarqueeDir,   SetMarqueeDir,   false, 0 },
	{ "Titles",                	GetTitlesDir,    SetTitlesDir,    false, 0 },
	{ "Control Panels",        	GetControlPanelDir,SetControlPanelDir, false, 0 },
	{ "Scores",      			GetScoresDir,    SetScoresDir,    false, 0 },
	{ "Bosses",      			GetBossesDir,    SetBossesDir,    false, 0 },
	{ "Versus",      			GetVersusDir,    SetVersusDir,    false, 0 },
	{ "Ends",      				GetEndsDir,      SetEndsDir,      false, 0 },
	{ "Game Overs",      		GetGameOverDir,  SetGameOverDir,  false, 0 },
	{ "How Tos",      			GetHowToDir,     SetHowToDir,     false, 0 },
	{ "Selects",      			GetSelectDir,    SetSelectDir,    false, 0 },
	{ "Logos",      			GetLogoDir,      SetLogoDir,      false, 0 },
	{ "Artwork Previews",      	GetArtworkDir,   SetArtworkDir,   false, 0 },
	{ "PCBs",                  	GetPcbDir,       SetPcbDir,       false, 0 },
	{ "Folders",               	GetFolderDir,    SetFolderDir,    false, 0 },
	{ "Icons",                 	GetIconsDir,     SetIconsDir,     false, 0 },
	{ "External Datafiles",    	GetDatsDir,      SetDatsDir,      false, 0 },
	{ "Languages",             	GetLanguageDir,  SetLanguageDir,  false, 0 },
	{ NULL }
};

/***************************************************************************
    Function prototypes
 ***************************************************************************/

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData);
static bool BrowseForDirectory(HWND hwnd, const TCHAR* pStartDir, TCHAR* pResult);
static void DirInfo_SetDir(tDirInfo *pInfo, int nType, int nItem, const TCHAR* pText);
static TCHAR* DirInfo_Dir(tDirInfo *pInfo, int nType);
static TCHAR* DirInfo_Path(tDirInfo *pInfo, int nType, int nItem);
static void DirInfo_SetModified(tDirInfo *pInfo, int nType, bool bModified);
static bool DirInfo_Modified(tDirInfo *pInfo, int nType);
static TCHAR* FixSlash(TCHAR *s);
static void UpdateDirectoryList(HWND hDlg);
static void Directories_OnSelChange(HWND hDlg);
static bool Directories_OnInitDialog(HWND hDlg, HWND hwndFocus, LPARAM lParam);
static void Directories_OnDestroy(HWND hDlg);
static void Directories_OnClose(HWND hDlg);
static void Directories_OnOk(HWND hDlg);
static void Directories_OnCancel(HWND hDlg);
static void Directories_OnInsert(HWND hDlg);
static void Directories_OnBrowse(HWND hDlg);
static void Directories_OnDelete(HWND hDlg);
static void Directories_OnCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify);

/***************************************************************************
    Internal variables
 ***************************************************************************/

static tDirInfo *g_pDirInfo;
static HBRUSH hBrush = NULL;
static HDC hDC = NULL;
static HICON hIcon = NULL;

/***************************************************************************
    External function definitions
 ***************************************************************************/

INT_PTR CALLBACK DirectoriesDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
		case WM_INITDIALOG:
			CenterWindow(hDlg);
			hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAMEUI_ICON));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			hBrush = CreateSolidBrush(RGB(240, 240, 240));

			if(IsWindowsSevenOrHigher())
				(void)ListView_SetExtendedListViewStyle(GetDlgItem(hDlg, IDC_DIR_LIST), LVS_EX_FULLROWSELECT | LVS_EX_ONECLICKACTIVATE | LVS_EX_DOUBLEBUFFER);
			else
				(void)ListView_SetExtendedListViewStyle(GetDlgItem(hDlg, IDC_DIR_LIST), LVS_EX_FULLROWSELECT | LVS_EX_UNDERLINEHOT | LVS_EX_ONECLICKACTIVATE | LVS_EX_DOUBLEBUFFER);

			SetWindowTheme(GetDlgItem(hDlg, IDC_DIR_LIST), L"Explorer", NULL);
			SetWindowTheme(GetDlgItem(hDlg, IDC_DIR_COMBO), L" ", L" ");
			return (BOOL)HANDLE_WM_INITDIALOG(hDlg, wParam, lParam, Directories_OnInitDialog);

		case WM_CTLCOLORDLG:
			return (LRESULT) hBrush;

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
			return (LRESULT) hBrush;

		case WM_COMMAND:
			HANDLE_WM_COMMAND(hDlg, wParam, lParam, Directories_OnCommand);
			return true;

		case WM_CLOSE:
			HANDLE_WM_CLOSE(hDlg, wParam, lParam, Directories_OnClose);
			break;

		case WM_DESTROY:
			HANDLE_WM_DESTROY(hDlg, wParam, lParam, Directories_OnDestroy);
			break;

	}
	
	return false;
}

/***************************************************************************
    Internal function definitions
 ***************************************************************************/

static bool IsMultiDir(int nType)
{
	return g_directoryInfo[nType].bMulti;
}

static void DirInfo_SetDir(tDirInfo *pInfo, int nType, int nItem, const TCHAR* pText)
{
	if (IsMultiDir(nType))
	{
		assert(nItem >= 0);
		_tcscpy(DirInfo_Path(pInfo, nType, nItem), pText);
		DirInfo_SetModified(pInfo, nType, true);
	}
	else
	{
		const char *str = win_utf8_from_wstring(pText);
		TCHAR *t_str = win_wstring_from_utf8(str);
		pInfo[nType].m_tDirectory = t_str;
		t_str = NULL;
		str = NULL;
	}
}

static TCHAR* DirInfo_Dir(tDirInfo *pInfo, int nType)
{
	assert(!IsMultiDir(nType));
	return pInfo[nType].m_tDirectory;
}

static TCHAR* DirInfo_Path(tDirInfo *pInfo, int nType, int nItem)
{
	return pInfo[nType].m_Path->m_Directories[nItem];
}

static void DirInfo_SetModified(tDirInfo *pInfo, int nType, bool bModified)
{
	assert(IsMultiDir(nType));
	pInfo[nType].m_Path->m_bModified = bModified;
}

static bool DirInfo_Modified(tDirInfo *pInfo, int nType)
{
	assert(IsMultiDir(nType));
	return pInfo[nType].m_Path->m_bModified;
}

/* lop off trailing backslash if it exists */
static TCHAR * FixSlash(TCHAR *s)
{
	int len = 0;

	if (s)
		len = _tcslen(s);

	if (len > 3 && s[len - 1] == *PATH_SEPARATOR)
		s[len - 1] = '\0';

	return s;
}

static void UpdateDirectoryList(HWND hDlg)
{
	LVITEM Item;
	HWND hList  = GetDlgItem(hDlg, IDC_DIR_LIST);
	HWND hCombo = GetDlgItem(hDlg, IDC_DIR_COMBO);

	/* Remove previous */
	(void)ListView_DeleteAllItems(hList);

	/* Update list */
	memset(&Item, 0, sizeof(LVITEM));
	Item.mask = LVIF_TEXT | LVIF_IMAGE;
	Item.iImage = -1;

	int nType = ComboBox_GetCurSel(hCombo);

	if (IsMultiDir(nType))
	{
		Item.pszText = (TCHAR*) TEXT(DIRLIST_NEWENTRYTEXT);
		(void)ListView_InsertItem(hList, &Item);

		for (int i = DirInfo_NumDir(g_pDirInfo, nType) - 1; 0 <= i; i--)
		{
			Item.pszText = DirInfo_Path(g_pDirInfo, nType, i);
			(void)ListView_InsertItem(hList, &Item);
		}
	}
	else
	{
		Item.pszText = DirInfo_Dir(g_pDirInfo, nType);
		(void)ListView_InsertItem(hList, &Item);
	}

	/* select first one */
	ListView_SetItemState(hList, 0, LVIS_SELECTED, LVIS_SELECTED);
}

static void Directories_OnSelChange(HWND hDlg)
{
	UpdateDirectoryList(hDlg);
	int nType = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_DIR_COMBO));

	if (IsMultiDir(nType))
	{
		EnableWindow(GetDlgItem(hDlg, IDC_DIR_DELETE), true);
		EnableWindow(GetDlgItem(hDlg, IDC_DIR_INSERT), true);
	}
	else
	{
		EnableWindow(GetDlgItem(hDlg, IDC_DIR_DELETE), false);
		EnableWindow(GetDlgItem(hDlg, IDC_DIR_INSERT), false);
	}
}

static bool Directories_OnInitDialog(HWND hDlg, HWND hwndFocus, LPARAM lParam)
{
	RECT rectClient;
	LVCOLUMN LVCol;
	TCHAR *token = NULL;
	TCHAR buf[MAX_PATH * MAX_DIRS];
	const TCHAR *t_s = NULL;

	/* count how many dirinfos there are */
	int nDirInfoCount = 0;

	while(g_directoryInfo[nDirInfoCount].lpName)
		nDirInfoCount++;

	g_pDirInfo = (tDirInfo *) malloc(sizeof(tDirInfo) * nDirInfoCount);

	if (!g_pDirInfo) /* bummer */
		goto error;

	memset(g_pDirInfo, 0, sizeof(tDirInfo) * nDirInfoCount);

	for (int i = nDirInfoCount - 1; i >= 0; i--)
	{
		t_s = win_wstring_from_utf8(g_directoryInfo[i].lpName);

		if(!t_s)
			return false;

		(void)ComboBox_InsertString(GetDlgItem(hDlg, IDC_DIR_COMBO), 0, t_s);
		t_s = NULL;
	}

	(void)ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_DIR_COMBO), 0);
	GetClientRect(GetDlgItem(hDlg, IDC_DIR_LIST), &rectClient);

	memset(&LVCol, 0, sizeof(LVCOLUMN));
	LVCol.mask = LVCF_WIDTH;
	LVCol.cx = rectClient.right - rectClient.left;

	(void)ListView_InsertColumn(GetDlgItem(hDlg, IDC_DIR_LIST), 0, &LVCol);

	/* Keep a temporary copy of the directory strings in g_pDirInfo. */
	for (int i = 0; i < nDirInfoCount; i++)
	{
		const char *s = g_directoryInfo[i].pfnGetTheseDirs();
		t_s = win_wstring_from_utf8(s);

		if(!t_s)
			return false;

		if (g_directoryInfo[i].bMulti)
		{
			/* Copy the string to our own buffer so that we can mutilate it */
			_tcscpy(buf, t_s);

			g_pDirInfo[i].m_Path = (tPath*)malloc(sizeof(tPath));

			if (!g_pDirInfo[i].m_Path)
				goto error;

			g_pDirInfo[i].m_Path->m_NumDirectories = 0;
			token = _tcstok(buf, TEXT(";"));

			while ((DirInfo_NumDir(g_pDirInfo, i) < MAX_DIRS) && token)
			{
				_tcscpy(DirInfo_Path(g_pDirInfo, i, DirInfo_NumDir(g_pDirInfo, i)), token);
				DirInfo_NumDir(g_pDirInfo, i)++;
				token = _tcstok(NULL, TEXT(";"));
			}

			DirInfo_SetModified(g_pDirInfo, i, false);
		}
		else
		{
			DirInfo_SetDir(g_pDirInfo, i, -1, t_s);
		}

		t_s = NULL;
	}

	UpdateDirectoryList(hDlg);
	return true;

error:
	if(t_s)
		t_s = NULL;

	Directories_OnDestroy(hDlg);
	DeleteObject(hBrush);
	DestroyIcon(hIcon);
	EndDialog(hDlg, -1);
	return false;
}

static void Directories_OnDestroy(HWND hDlg)
{
	if (g_pDirInfo)
	{
		/* count how many dirinfos there are */
		int nDirInfoCount = 0;

		while(g_directoryInfo[nDirInfoCount].lpName)
			nDirInfoCount++;

		for (int i = 0; i < nDirInfoCount; i++)
		{
			if (g_pDirInfo[i].m_Path)
				free(g_pDirInfo[i].m_Path);

			if (g_pDirInfo[i].m_tDirectory)
				free(g_pDirInfo[i].m_tDirectory);
		}

		free(g_pDirInfo);
		g_pDirInfo = NULL;
	}
}

static void Directories_OnClose(HWND hDlg)
{
	DeleteObject(hBrush);
	DestroyIcon(hIcon);
	EndDialog(hDlg, IDCANCEL);
}

static int RetrieveDirList(int nDir, int nFlagResult, void (*SetTheseDirs)(const char *s))
{
	int nResult = 0;

	if (DirInfo_Modified(g_pDirInfo, nDir))
	{
		TCHAR buf[MAX_PATH * MAX_DIRS];		
		memset(&buf, 0, sizeof(buf));
		int nPaths = DirInfo_NumDir(g_pDirInfo, nDir);

		for (int i = 0; i < nPaths; i++)
		{
			_tcscat(buf, FixSlash(DirInfo_Path(g_pDirInfo, nDir, i)));

			if (i < nPaths - 1)
				_tcscat(buf, TEXT(";"));
		}

		char *utf8_buf = win_utf8_from_wstring(buf);
		SetTheseDirs(utf8_buf);
		free(utf8_buf);
		nResult |= nFlagResult;
	}

	return nResult;
}

static void Directories_OnOk(HWND hDlg)
{
	int nResult = 0;

	for (int i = 0; g_directoryInfo[i].lpName; i++)
	{
		if (g_directoryInfo[i].bMulti)
			nResult |= RetrieveDirList(i, g_directoryInfo[i].nDirDlgFlags, g_directoryInfo[i].pfnSetTheseDirs);
		else
		{
			TCHAR *s = FixSlash(DirInfo_Dir(g_pDirInfo, i));
			char *utf8_s = win_utf8_from_wstring(s);
			g_directoryInfo[i].pfnSetTheseDirs(utf8_s);
			free(utf8_s);
		}
	}

	DeleteObject(hBrush);
	DestroyIcon(hIcon);
	EndDialog(hDlg, nResult);
}

static void Directories_OnCancel(HWND hDlg)
{
	DeleteObject(hBrush);
	DestroyIcon(hIcon);
	EndDialog(hDlg, IDCANCEL);
}

static void Directories_OnInsert(HWND hDlg)
{
	HWND hList = GetDlgItem(hDlg, IDC_DIR_LIST);
	int nItem = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
	TCHAR buf[MAX_PATH];

	if (BrowseForDirectory(hDlg, NULL, buf) == true)
	{
		/* list was empty */
		if (nItem == -1)
			nItem = 0;

		int nType = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_DIR_COMBO));

		if (IsMultiDir(nType))
		{
			if (MAX_DIRS <= DirInfo_NumDir(g_pDirInfo, nType))
				return;

			for (int i = DirInfo_NumDir(g_pDirInfo, nType); nItem < i; i--)
				_tcscpy(DirInfo_Path(g_pDirInfo, nType, i), DirInfo_Path(g_pDirInfo, nType, i - 1));

			_tcscpy(DirInfo_Path(g_pDirInfo, nType, nItem), buf);
			DirInfo_NumDir(g_pDirInfo, nType)++;
			DirInfo_SetModified(g_pDirInfo, nType, true);
		}

		UpdateDirectoryList(hDlg);
		ListView_SetItemState(hList, nItem, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
	}
}

static void Directories_OnBrowse(HWND hDlg)
{
	TCHAR inbuf[MAX_PATH];
	TCHAR outbuf[MAX_PATH];
	HWND hList = GetDlgItem(hDlg, IDC_DIR_LIST);
	int nItem = ListView_GetNextItem(hList, -1, LVNI_SELECTED);

	if (nItem == -1)
		return;

	int nType = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_DIR_COMBO));

	if (IsMultiDir(nType))
	{
		/* Last item is placeholder for append */
		if (nItem == ListView_GetItemCount(hList) - 1)
		{
			Directories_OnInsert(hDlg);
			return;
		}
	}

	ListView_GetItemText(hList, nItem, 0, inbuf, MAX_PATH);

	if (BrowseForDirectory(hDlg, inbuf, outbuf) == true)
	{
		nType = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_DIR_COMBO));
		DirInfo_SetDir(g_pDirInfo, nType, nItem, outbuf);
		UpdateDirectoryList(hDlg);
	}
}

static void Directories_OnDelete(HWND hDlg)
{
	int nSelect = 0;
	HWND hList = GetDlgItem(hDlg, IDC_DIR_LIST);
	int nItem = ListView_GetNextItem(hList, -1, LVNI_SELECTED | LVNI_ALL);

	if (nItem == -1)
		return;

	/* Don't delete "Append" placeholder. */
	if (nItem == ListView_GetItemCount(hList) - 1)
		return;

	int nType = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_DIR_COMBO));

	if (IsMultiDir(nType))
	{
		for (int i = nItem; i < DirInfo_NumDir(g_pDirInfo, nType) - 1; i++)
			_tcscpy(DirInfo_Path(g_pDirInfo, nType, i), DirInfo_Path(g_pDirInfo, nType, i + 1));

		_tcscpy(DirInfo_Path(g_pDirInfo, nType, DirInfo_NumDir(g_pDirInfo, nType) - 1), TEXT(""));
		DirInfo_NumDir(g_pDirInfo, nType)--;
		DirInfo_SetModified(g_pDirInfo, nType, true);
	}

	UpdateDirectoryList(hDlg);
	int nCount = ListView_GetItemCount(hList);

	if (nCount <= 1)
		return;

	/* If the last item was removed, select the item above. */
	if (nItem == nCount - 1)
		nSelect = nCount - 2;
	else
		nSelect = nItem;

	ListView_SetItemState(hList, nSelect, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
}

static void Directories_OnCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify)
{
	switch (id)
	{
		case IDOK:
			if (codeNotify == BN_CLICKED)
				Directories_OnOk(hDlg);
			break;

		case IDCANCEL:
			if (codeNotify == BN_CLICKED)
				Directories_OnCancel(hDlg);
			break;

		case IDC_DIR_BROWSE:
			if (codeNotify == BN_CLICKED)
				Directories_OnBrowse(hDlg);
			break;

		case IDC_DIR_INSERT:
			if (codeNotify == BN_CLICKED)
				Directories_OnInsert(hDlg);
			break;

		case IDC_DIR_DELETE:
			if (codeNotify == BN_CLICKED)
				Directories_OnDelete(hDlg);
			break;

		case IDC_DIR_COMBO:
			switch (codeNotify)
			{
				case CBN_SELCHANGE:
					Directories_OnSelChange(hDlg);
					break;
			}
			break;
	}
}

/**************************************************************************

    Use the shell to select a Directory.

 **************************************************************************/

static int CALLBACK BrowseCallbackProc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	/* Called just after the dialog is initialized. Select the dir passed in BROWSEINFO.lParam */
	if (uMsg == BFFM_INITIALIZED)
	{
		if ((const char*)lpData != NULL)
			SendMessage(hWnd, BFFM_SETSELECTION, true, lpData);
	}

	return 0;
}

static bool BrowseForDirectory(HWND hWnd, const TCHAR* pStartDir, TCHAR* pResult)
{
	bool bResult = false;
	BROWSEINFO Info;
	LPITEMIDLIST pItemIDList = NULL;
	TCHAR buf[MAX_PATH];

	Info.hwndOwner = hWnd;
	Info.pidlRoot = NULL;
	Info.pszDisplayName = buf;
	Info.lpszTitle = TEXT("Select a directory:");
	Info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
	Info.lpfn = BrowseCallbackProc;
	Info.lParam = (LPARAM)pStartDir;

	pItemIDList = SHBrowseForFolder(&Info);

	if (pItemIDList != NULL)
	{
		if (SHGetPathFromIDList(pItemIDList, buf) == true)
		{
			_sntprintf(pResult, MAX_PATH, TEXT("%s"), buf);
			bResult = true;
		}
	}
	else
		bResult = false;

	return bResult;
}
