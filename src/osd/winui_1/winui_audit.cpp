// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include "winui.h"

/***************************************************************************
    function prototypes
 ***************************************************************************/

static DWORD WINAPI AuditThreadProc(LPVOID hDlg);
static int MameUIVerifySampleSet(int game);
static int MameUIVerifyRomSetFull(int game);
static void ProcessNextRom(void);
static void DetailsPrintf(const char *fmt, ...);
static const char * StatusString(int iStatus);
static bool RomSetFound(int index);
static void RetrievePaths(void);
static const char * RetrieveCHDName(int romset);

/***************************************************************************
    Internal variables
 ***************************************************************************/

static int rom_index = 0;
static int roms_correct = 0;
static int roms_incorrect = 0;
static int roms_notfound = 0;
static int audit_color = 0;
static int audit_samples = 0;
static char rom_path[MAX_DIRS][MAX_PATH];
static int num_path = 0;
static HWND hAudit = NULL;
static HICON audit_icon = NULL;
static HICON hIcon = NULL;
static HBRUSH hBrush = NULL;
static HDC hDC = NULL;
static HANDLE hThread = NULL;
static HFONT hFont = NULL;

/***************************************************************************
    External functions
 ***************************************************************************/

void AuditDialog(void)
{
	for (int i = 0; i < driver_list::total(); i++)
	{
		SetRomAuditResults(i, UNKNOWN);
	}

	rom_index         = 0;
	roms_correct      = 0;
	roms_incorrect    = 0;
	roms_notfound	  = 0;
	RetrievePaths();
}

void AuditRefresh(void)
{
	RetrievePaths();
}

bool IsAuditResultYes(int audit_result)
{
	return audit_result == media_auditor::CORRECT || audit_result == media_auditor::BEST_AVAILABLE || audit_result == media_auditor::NONE_NEEDED;
}

bool IsAuditResultNo(int audit_result)
{
	return audit_result == media_auditor::NOTFOUND || audit_result == media_auditor::INCORRECT;
}

int MameUIVerifyRomSet(int game, bool refresh)
{
	if (!RomSetFound(game))
	{
		SetRomAuditResults(game, media_auditor::NOTFOUND);
		return media_auditor::NOTFOUND;
	}

	driver_enumerator enumerator(MameUIGlobal(), driver_list::driver(game));
	enumerator.next();
	media_auditor auditor(enumerator);
	media_auditor::summary summary = auditor.audit_media(AUDIT_VALIDATE_FAST);
//	util::ovectorstream buffer;
//	buffer.clear();
//	buffer.seekp(0);
//	auditor.winui_summarize(GetDriverGameName(game), &buffer);
//	buffer.put('\0');
	std::string summary_string;
	auditor.winui_summarize(driver_list::driver(game).name, &summary_string); // audit all games

	if (!refresh)
		DetailsPrintf("%s", summary_string.c_str());
//		DetailsPrintf("%s", &buffer.vec()[0]);

	SetRomAuditResults(game, summary);
	return summary;
}

static int MameUIVerifyRomSetFull(int game)
{
	driver_enumerator enumerator(MameUIGlobal(), driver_list::driver(game));
	enumerator.next();
	media_auditor auditor(enumerator);
	media_auditor::summary summary = auditor.audit_media(AUDIT_VALIDATE_FAST);
	util::ovectorstream buffer;
	buffer.clear();
	buffer.seekp(0);
	auditor.summarize(GetDriverGameName(game), &buffer);
	buffer.put('\0');
	DetailsPrintf("%s", &buffer.vec()[0]);
	SetRomAuditResults(game, summary);
	return summary;
}

int MameUIVerifySampleSet(int game)
{
	driver_enumerator enumerator(MameUIGlobal(), driver_list::driver(game));
	enumerator.next();
	media_auditor auditor(enumerator);
	media_auditor::summary summary = auditor.audit_samples();

	if (summary != media_auditor::NONE_NEEDED)
	{
		util::ovectorstream buffer;
		buffer.clear();
		buffer.seekp(0);
		auditor.summarize(GetDriverGameName(game), &buffer);
		buffer.put('\0');
		DetailsPrintf("%s", &buffer.vec()[0]);
	}

	return summary;
}

static DWORD WINAPI AuditThreadProc(LPVOID hDlg)
{
	while (rom_index != -1)
	{
		ProcessNextRom();
	}

	ExitThread(1);
	return 0;
}

INT_PTR CALLBACK AuditWindowProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
		case WM_INITDIALOG:
			hAudit = hDlg;
			CenterWindow(hAudit);
			hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAMEUI_ICON));
			SendMessage(hAudit, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			hBrush = CreateSolidBrush(RGB(240, 240, 240));
			hFont = CreateFont(-11, 0, 0, 0, 400, 0, 0, 0, 0, 3, 2, 1, 34, TEXT("Lucida Console"));
			SetWindowFont(GetDlgItem(hAudit, IDC_AUDIT_DETAILS), hFont, true);
			SetWindowTheme(GetDlgItem(hAudit, IDC_AUDIT_DETAILS), L" ", L" ");
			SetWindowTheme(GetDlgItem(hAudit, IDC_ROMS_PROGRESS), L" ", L" ");
			SendMessage(GetDlgItem(hAudit, IDC_ROMS_PROGRESS), PBM_SETBARCOLOR, 0, RGB(85, 191, 132));
			SendMessage(GetDlgItem(hAudit, IDC_ROMS_PROGRESS), PBM_SETBKCOLOR, 0, RGB(224, 224, 224));
			SendMessage(GetDlgItem(hAudit, IDC_ROMS_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, driver_list::total()));
			win_set_window_text_utf8(hAudit, "Checking games... Please wait...");
			hThread = CreateThread(NULL, 0, AuditThreadProc, hAudit, 0, 0);
			return true;

		case WM_CTLCOLORDLG:
			return (LRESULT) hBrush;	

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));

			if ((HWND)lParam == GetDlgItem(hAudit, IDC_ROMS_CORRECT))
				SetTextColor(hDC, RGB(34, 177, 76));

			if ((HWND)lParam == GetDlgItem(hAudit, IDC_ROMS_INCORRECT))
				SetTextColor(hDC, RGB(198, 188, 0));

			if ((HWND)lParam == GetDlgItem(hAudit, IDC_ROMS_NOTFOUND))
				SetTextColor(hDC, RGB(237, 28, 36));

			if ((HWND)lParam == GetDlgItem(hAudit, IDC_ROMS_TOTAL))
				SetTextColor(hDC, RGB(63, 72, 204));

			return (LRESULT) hBrush;

		case WM_CTLCOLOREDIT:
			hDC = (HDC)wParam;
			SetTextColor(hDC, RGB(136, 0, 21));
			return (LRESULT) GetStockObject(WHITE_BRUSH);

		case WM_COMMAND:
			switch (GET_WM_COMMAND_ID(wParam, lParam))
			{
				case IDCANCEL:
					DWORD ExitCode = 0;

					if (hThread)
					{
						rom_index = -1;

						if (GetExitCodeThread(hThread, &ExitCode) && (ExitCode == STILL_ACTIVE))
						{
							PostMessage(hAudit, WM_COMMAND, wParam, lParam);
							return true;
						}
					}

					DeleteObject(hBrush);
					DeleteObject(hFont);
					DestroyIcon(hIcon);
					EndDialog(hAudit, 0);
					break;
			}

			break;
	}

	return false;
}

INT_PTR CALLBACK GameAuditDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
		case WM_INITDIALOG:
		{
			char tmp[64];
			rom_index = lParam;
			const game_driver *game = &driver_list::driver(rom_index);
			machine_config config(*game, MameUIGlobal());
			char buffer[4096];
			char details[4096];
			UINT32 crctext = 0;

			memset(&buffer, 0, sizeof(buffer));
			memset(&details, 0, sizeof(details));
			hAudit = hDlg;
			CenterWindow(hAudit);
			hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAMEUI_ICON));
			SendMessage(hAudit, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			hBrush = CreateSolidBrush(RGB(240, 240, 240));
			hFont = CreateFont(-11, 0, 0, 0, 400, 0, 0, 0, 0, 3, 2, 1, 34, TEXT("Lucida Console"));
			SetWindowFont(GetDlgItem(hAudit, IDC_ROM_DETAILS), hFont, true);
			SetWindowFont(GetDlgItem(hAudit, IDC_AUDIT_DETAILS_PROP), hFont, true);
			SetWindowTheme(GetDlgItem(hAudit, IDC_AUDIT_DETAILS_PROP), L" ", L" ");
			SetWindowTheme(GetDlgItem(hAudit, IDC_ROM_DETAILS), L" ", L" ");
			snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "Audit results for \"%s\"", GetDriverGameName(rom_index));
			win_set_window_text_utf8(hAudit, tmp);
			int iStatus = MameUIVerifyRomSetFull(rom_index);

			switch (iStatus)
			{
				case media_auditor::CORRECT:
				case media_auditor::BEST_AVAILABLE:
				case media_auditor::NONE_NEEDED:
					audit_icon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_AUDIT_PASS));
					break;

				case media_auditor::NOTFOUND:
				case media_auditor::INCORRECT:
					audit_icon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_AUDIT_FAIL));
					break;
			}

			SendMessage(GetDlgItem(hAudit, IDC_AUDIT_ICON), STM_SETICON, (WPARAM)audit_icon, 0);
			const char *lpStatus = StatusString(iStatus);
			win_set_window_text_utf8(GetDlgItem(hAudit, IDC_PROP_ROMS), lpStatus);

			if (DriverUsesSamples(rom_index))
			{
				iStatus = MameUIVerifySampleSet(rom_index);
				lpStatus = StatusString(iStatus);
			}
			else
			{
				lpStatus = "None required";
				audit_samples = 2;
			}

			win_set_window_text_utf8(GetDlgItem(hAudit, IDC_PROP_SAMPLES), lpStatus);
			strcpy(buffer, "NAME                SIZE      CRC\n");
			strcat(buffer, "--------------------------------------\n");
			strcat(details, buffer);

			for (device_t &device : device_iterator(config.root_device()))
			{
				for (const rom_entry *region = rom_first_region(device); region != nullptr; region = rom_next_region(region))
				{
					for (const rom_entry *rom = rom_first_file(region); rom != nullptr; rom = rom_next_file(rom))
					{
						UINT32 crc = 0;

						if (util::hash_collection(ROM_GETHASHDATA(rom)).crc(crc))
							crctext = crc;
						else
							crctext = 0;

						snprintf(buffer, WINUI_ARRAY_LENGTH(buffer), "%-18s  %09d %08x\n", ROM_GETNAME(rom), ROM_GETLENGTH(rom), crctext);
						strcat(details, buffer);
					}
				}
			}

			win_set_window_text_utf8(GetDlgItem(hAudit, IDC_ROM_DETAILS), ConvertToWindowsNewlines(details));
			ShowWindow(hAudit, SW_SHOW);
			return true;
		}

		case WM_CTLCOLORDLG:
			return (LRESULT) hBrush;

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
		{
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));

			if ((HWND)lParam == GetDlgItem(hAudit, IDC_PROP_ROMS))
			{
				if (audit_color == 0)
					SetTextColor(hDC, RGB(34, 177, 76));
				else if (audit_color == 1)
					SetTextColor(hDC, RGB(237, 28, 36));
				else
					SetTextColor(hDC, RGB(63, 72, 204));
			}

			if ((HWND)lParam == GetDlgItem(hAudit, IDC_PROP_SAMPLES))
			{
				if (audit_samples == 0)
					SetTextColor(hDC, RGB(34, 177, 76));
				else if (audit_samples == 1)
					SetTextColor(hDC, RGB(237, 28, 36));
				else
					SetTextColor(hDC, RGB(63, 72, 204));
			}

			return (LRESULT) hBrush;
		}

		case WM_CTLCOLOREDIT:
		{
			hDC = (HDC)wParam;

			if ((HWND)lParam == GetDlgItem(hAudit, IDC_ROM_DETAILS))
				SetTextColor(hDC, RGB(59, 59, 59));

			if ((HWND)lParam == GetDlgItem(hAudit, IDC_AUDIT_DETAILS_PROP))
				SetTextColor(hDC, RGB(136, 0, 21));

			return (LRESULT) GetStockObject(WHITE_BRUSH);
		}

		case WM_COMMAND:
			switch (GET_WM_COMMAND_ID(wParam, lParam))
			{
				case IDOK:
				case IDCANCEL:
					DeleteObject(hFont);
					DestroyIcon(hIcon);
					DestroyIcon(audit_icon);
					DeleteObject(hBrush);
					EndDialog(hDlg, 0);
					return true;
			}

			break;
	}

	return false;
}

static void ProcessNextRom(void)
{
	char buffer[200];

	if (driver_list::driver(rom_index).name[0] == '_') // skip __empty driver
	{
		rom_index++;
		return;
	}

	int retval = MameUIVerifyRomSet(rom_index, false);

	switch (retval)
	{
		case media_auditor::BEST_AVAILABLE: 	/* correct, incorrect or separate count? */
		case media_auditor::CORRECT:
		case media_auditor::NONE_NEEDED:
			roms_correct++;
			snprintf(buffer, WINUI_ARRAY_LENGTH(buffer), "%d", roms_correct);
			win_set_window_text_utf8(GetDlgItem(hAudit, IDC_ROMS_CORRECT), buffer);
			break;

		case media_auditor::NOTFOUND:
			roms_notfound++;
			snprintf(buffer, WINUI_ARRAY_LENGTH(buffer), "%d", roms_notfound);
			win_set_window_text_utf8(GetDlgItem(hAudit, IDC_ROMS_NOTFOUND), buffer);
			break;

		case media_auditor::INCORRECT:
			roms_incorrect++;
			snprintf(buffer, WINUI_ARRAY_LENGTH(buffer), "%d", roms_incorrect);
			win_set_window_text_utf8(GetDlgItem(hAudit, IDC_ROMS_INCORRECT), buffer);
			break;
	}

	snprintf(buffer, WINUI_ARRAY_LENGTH(buffer), "%d", roms_correct + roms_incorrect + roms_notfound);
	win_set_window_text_utf8(GetDlgItem(hAudit, IDC_ROMS_TOTAL), buffer);
	rom_index++;
	SendMessage(GetDlgItem(hAudit, IDC_ROMS_PROGRESS), PBM_SETPOS, rom_index, 0);

	if (rom_index == driver_list::total())
	{
		win_set_window_text_utf8(hAudit, "Audit process completed");
		DetailsPrintf("Audit completed.\n");
		win_set_window_text_utf8(GetDlgItem(hAudit, IDCANCEL), "Close");
		rom_index = -1;
	}
}

static void DetailsPrintf(const char *fmt, ...)
{
	va_list marker;
	char buffer[8000];
	bool scroll = true;

	//RS 20030613 Different Ids for Property Page and Dialog
	// so see which one's currently instantiated
	HWND hEdit = GetDlgItem(hAudit, IDC_AUDIT_DETAILS);
	
	if (hEdit ==  NULL)
	{
		hEdit = GetDlgItem(hAudit, IDC_AUDIT_DETAILS_PROP);
		scroll = false;
	}

	if (hEdit == NULL)
		return;

	va_start(marker, fmt);
	vsnprintf(buffer, WINUI_ARRAY_LENGTH(buffer), fmt, marker);
	va_end(marker);
	TCHAR *t_s = win_wstring_from_utf8(ConvertToWindowsNewlines(buffer));

	if( !t_s || _tcscmp(TEXT(""), t_s) == 0)
		return;

	int textLength = Edit_GetTextLength(hEdit);
	Edit_SetSel(hEdit, textLength, textLength);
	Edit_ReplaceSel(hEdit, t_s);

	if (scroll)
		Edit_ScrollCaret(hEdit);

	free(t_s);
}

static const char * StatusString(int iStatus)
{
	static const char *ptr = "None required";
	audit_color = 2;

	switch (iStatus)
	{
		case media_auditor::CORRECT:
			ptr = "Passed";
			audit_color = 0;
			audit_samples = 0;
			break;

		case media_auditor::BEST_AVAILABLE:
			ptr = "Best available";
			audit_color = 0;
			audit_samples = 0;
			break;

		case media_auditor::NOTFOUND:
			ptr = "Not found";
			audit_color = 1;
			audit_samples = 1;
			break;

		case media_auditor::INCORRECT:
			ptr = "Failed";
			audit_color = 1;
			audit_samples = 1;
			break;
	}

	return ptr;
}

static bool RomSetFound(int index)
{
	char filename[MAX_PATH];
	const char *gamename = GetDriverGameName(index);
	const char *chdname = NULL;
	bool found = false;

	// nothing to do if faster audit option is disabled
	if (!GetEnableFastAudit())
		return true;

	// obtain real CHD name from core if game has one
	if (DriverIsHarddisk(index))
		chdname = RetrieveCHDName(index);

	// don't search for empty romsets (e.g. pong, breakout)
	if (!DriverUsesRoms(index))
		return true;

	// parse all paths defined by the user
	for (int i = 0; i < num_path; i++)
	{
		FILE *f = NULL;

		// try to search a standard zip romset first
		snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\%s.zip", rom_path[i], gamename);
		f = fopen(filename, "r");

		// if it fails, try with 7zip extension if enabled by the user
		if (f == NULL && GetEnableSevenZip())
		{
			snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\%s.7z", rom_path[i], gamename);
			f = fopen(filename, "r");
		}

		// it could be a merged romset so check if we at least have the parent
		if (f == NULL && DriverIsClone(index))
		{
			int parent = GetParentIndex(&driver_list::driver(index));
			int parent_found = GetRomAuditResults(parent);

			// don't check clones of BIOSes but continue to next rompath
			if (DriverIsBios(parent))
				continue;

			if (IsAuditResultYes(parent_found)) // parent already found
			{
				found = true;
				break;
			}
			else // re-check if parent exists
			{
				const char *parentname = GetDriverGameName(parent);
				snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\%s.zip", rom_path[i], parentname);
				f = fopen(filename, "r");

				if (f == NULL && GetEnableSevenZip())
				{
					snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\%s.7z", rom_path[i], parentname);
					f = fopen(filename, "r");
				}

				if (f != NULL)
				{
					fclose(f);
					found = true;
					break;
				}
			}
		}

		// maybe is a game with chd and no romset (e.g. taito g-net games)
		if (f == NULL && DriverIsHarddisk(index))
		{
			snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\%s\\%s.chd", rom_path[i], gamename, chdname);
			f = fopen(filename, "r");
		}

		// let's try the last attempt in the root folder
		if (f == NULL && DriverIsHarddisk(index))
		{
			snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\%s.chd", rom_path[i], chdname);
			f = fopen(filename, "r");
		}

		// success, so close the file and call core to audit the rom
		if (f != NULL)
		{
			fclose(f);
			found = true;
			break;
		}
	}

	// return if we found something or not
	return found;
}

static void RetrievePaths(void)
{
	char *token = NULL;
	char buffer[MAX_DIRS * MAX_PATH];
	const char *dirs = GetRomDirs();
	num_path = 0;

	strcpy(buffer, dirs);
	token = strtok(buffer, ";");

	if (token == NULL)
	{
		strcpy(rom_path[num_path], buffer);
		num_path = 1;
		return;
	}

	while (token != NULL)
	{
		strcpy(rom_path[num_path], token);
		num_path++;
		token = strtok(NULL, ";");
	}
}

static const char * RetrieveCHDName(int romset)
{
	static const char *name = NULL;
	const game_driver *game = &driver_list::driver(romset);
	machine_config config(*game, MameUIGlobal());

	for (device_t &device : device_iterator(config.root_device()))
	{
		for (const rom_entry *region = rom_first_region(device); region != nullptr; region = rom_next_region(region))
		{
			for (const rom_entry *rom = rom_first_file(region); rom != nullptr; rom = rom_next_file(rom))
			{
				if (ROMREGION_ISDISKDATA(region))
				{
					name = ROM_GETNAME(rom);
					break;
				}
			}
		}
	}

	return name;
}
