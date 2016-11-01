// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include "winui.h"

static struct ComboBoxHistoryTab
{
	const TCHAR*	m_pText;
	const int		m_pData;
} g_ComboBoxHistoryTab[] = 
{
	{ TEXT("In Game"),			TAB_SCREENSHOT },
	{ TEXT("Title"),			TAB_TITLE },
	{ TEXT("Scores"),			TAB_SCORES },
	{ TEXT("How To"),			TAB_HOWTO },
	{ TEXT("Select"),			TAB_SELECT },
	{ TEXT("Versus"),			TAB_VERSUS },
	{ TEXT("Boss"),				TAB_BOSSES },
	{ TEXT("End"),				TAB_ENDS },
	{ TEXT("Game Over"),		TAB_GAMEOVER },
	{ TEXT("Logo"),				TAB_LOGO },
	{ TEXT("Artwork"),			TAB_ARTWORK },
	{ TEXT("Flyer"),			TAB_FLYER },
	{ TEXT("Cabinet"),			TAB_CABINET },
	{ TEXT("Marquee"),			TAB_MARQUEE },
	{ TEXT("Control Panel"),	TAB_CONTROL_PANEL },
	{ TEXT("PCB"),				TAB_PCB },
	{ TEXT("All"),				TAB_ALL },
	{ TEXT("None"),				TAB_NONE }
};

static char g_FilterText[FILTERTEXT_LEN];

/* Pairs of filters that exclude each other */
static DWORD filterExclusion[NUM_EXCLUSIONS] =
{
	IDC_FILTER_CLONES,      IDC_FILTER_ORIGINALS,
	IDC_FILTER_NONWORKING,  IDC_FILTER_WORKING,
	IDC_FILTER_UNAVAILABLE, IDC_FILTER_AVAILABLE,
	IDC_FILTER_RASTER,      IDC_FILTER_VECTOR,
	IDC_FILTER_HORIZONTAL,  IDC_FILTER_VERTICAL
};

static void DisableFilterControls(HWND hWnd, LPCFOLDERDATA lpFilterRecord, LPCFILTER_ITEM lpFilterItem, DWORD dwFlags);
static void EnableFilterExclusions(HWND hWnd, DWORD dwCtrlID);
static DWORD ValidateFilters(LPCFOLDERDATA lpFilterRecord, DWORD dwFlags);
static void OnHScroll(HWND hWnd, HWND hWndCtl, UINT code, int pos);
static UINT_PTR CALLBACK CCHookProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
static void DisableVisualStylesInterface(HWND hDlg);
static void DisableVisualStylesReset(HWND hDlg);
static void DisableVisualStylesFilters(HWND hDlg);

static HICON hIcon = NULL;
static HBRUSH hBrush = NULL;
static HFONT hFont = NULL;
static HFONT hFontFX = NULL;
static HDC hDC = NULL;
static DWORD dwFilters = 0;
static DWORD dwpFilters = 0;
static LPCFOLDERDATA lpFilterRecord;
static LPTREEFOLDER default_selection = NULL;
static int driver_index = 0;

/***************************************************************************/

const char * GetFilterText(void)
{
	return g_FilterText;
}

INT_PTR CALLBACK ResetDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
		case WM_INITDIALOG:
			CenterWindow(hDlg);
			hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAMEUI_ICON));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			hBrush = CreateSolidBrush(RGB(240, 240, 240));
			DisableVisualStylesReset(hDlg);
			return true;

		case WM_CTLCOLORDLG:
			return (LRESULT) hBrush;	
		
		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
			return (LRESULT) hBrush;

		case WM_COMMAND:
			switch (GET_WM_COMMAND_ID(wParam, lParam))
			{
				case IDOK:
				{
					bool resetFilters = Button_GetCheck(GetDlgItem(hDlg, IDC_RESET_FILTERS));
					bool resetGames = Button_GetCheck(GetDlgItem(hDlg, IDC_RESET_GAMES));
					bool resetDefaults = Button_GetCheck(GetDlgItem(hDlg, IDC_RESET_DEFAULT));
					bool resetUI = Button_GetCheck(GetDlgItem(hDlg, IDC_RESET_UI));
					bool resetInternalUI = Button_GetCheck(GetDlgItem(hDlg, IDC_RESET_MAME_UI));

					if (resetFilters || resetGames || resetUI || resetDefaults || resetInternalUI)
					{
						char temp[512];
						strcpy(temp, MAMEUINAME);
						strcat(temp, " will now reset the following\n");
						strcat(temp, "to the default settings:\n\n");

						if (resetDefaults)
							strcat(temp, "Global games options\n");

						if (resetInternalUI)
							strcat(temp, "Internal MAME interface options\n");

						if (resetGames)
							strcat(temp, "Individual game options\n");

						if (resetFilters)
							strcat(temp, "Custom folder filters\n");

						if (resetUI)
						{
							strcat(temp, "User interface settings\n\n");
							strcat(temp, "Resetting the user interface options\n");
							strcat(temp, "requires exiting ");
							strcat(temp, MAMEUINAME);
							strcat(temp, ".\n");
						}

						strcat(temp, "\nDo you wish to continue?");

						if (win_message_box_utf8(hDlg, temp, "Restore settings", MB_ICONQUESTION | MB_YESNO) == IDYES)
						{
							DestroyIcon(hIcon);
							DeleteObject(hBrush);

							if (resetFilters)
								ResetFilters();

							if (resetGames)
								ResetAllGameOptions();

							if (resetDefaults)
								ResetGameDefaults();

							if (resetInternalUI)
								ResetInternalUI();

							// This is the only case we need to exit and restart for.
							if (resetUI)
							{
								ResetInterface();
								EndDialog(hDlg, 1);
								return true;
							}
							else 
							{
								EndDialog(hDlg, 0);
								return true;
							}
						}
						else 
							// Give the user a chance to change what they want to reset.
							break;
					}
				}
		
				// Nothing was selected but OK, just fall through
				case IDCANCEL:
					DeleteObject(hBrush);
					DestroyIcon(hIcon);
					EndDialog(hDlg, 0);
					return true;
			}

			break;
	}

	return false;
}

static UINT_PTR CALLBACK CCHookProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{

	switch (Msg)
	{
		case WM_INITDIALOG:
			CenterWindow(hDlg);
			hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAMEUI_ICON));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			hBrush = CreateSolidBrush(RGB(240, 240, 240));
			win_set_window_text_utf8(hDlg, "Choose a color");
			break;

		case WM_CTLCOLORDLG:
			return (LRESULT) hBrush;	

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			return (LRESULT) hBrush;

		case WM_DESTROY:
			DestroyIcon(hIcon);
			DeleteObject(hBrush);
			return true;
	}

	return false;
}

INT_PTR CALLBACK InterfaceDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{

	switch (Msg)
	{
		case WM_INITDIALOG:
		{
			char buffer[200];
			int value = 0;
			CenterWindow(hDlg);
			hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAMEUI_ICON));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			hBrush = CreateSolidBrush(RGB(240, 240, 240));
			DisableVisualStylesInterface(hDlg);
			Button_SetCheck(GetDlgItem(hDlg, IDC_JOY_GUI), GetJoyGUI());
			Button_SetCheck(GetDlgItem(hDlg, IDC_DISABLE_TRAY_ICON), GetMinimizeTrayIcon());
			Button_SetCheck(GetDlgItem(hDlg, IDC_DISPLAY_NO_ROMS), GetDisplayNoRomsGames());
			Button_SetCheck(GetDlgItem(hDlg, IDC_EXIT_DIALOG), GetExitDialog());
			Button_SetCheck(GetDlgItem(hDlg, IDC_USE_BROKEN_ICON), GetUseBrokenIcon());
			Button_SetCheck(GetDlgItem(hDlg, IDC_STRETCH_SCREENSHOT_LARGER), GetStretchScreenShotLarger());
			Button_SetCheck(GetDlgItem(hDlg, IDC_FILTER_INHERIT), GetFilterInherit());
			Button_SetCheck(GetDlgItem(hDlg, IDC_ENABLE_FASTAUDIT), GetEnableFastAudit());
			Button_SetCheck(GetDlgItem(hDlg, IDC_ENABLE_SEVENZIP), GetEnableSevenZip());
			Button_SetCheck(GetDlgItem(hDlg, IDC_ENABLE_DATAFILES), GetEnableDatafiles());
			Button_SetCheck(GetDlgItem(hDlg, IDC_SKIP_BIOS), GetSkipBiosMenu());
			// Get the current value of the control
			SendMessage(GetDlgItem(hDlg, IDC_CYCLETIMESEC), TBM_SETRANGE, true, MAKELPARAM(0, 60)); /* [0, 60] */
			SendMessage(GetDlgItem(hDlg, IDC_CYCLETIMESEC), TBM_SETTICFREQ, 5, 0);
			value = GetCycleScreenshot();
			SendMessage(GetDlgItem(hDlg, IDC_CYCLETIMESEC), TBM_SETPOS, true, value);
			snprintf(buffer, WINUI_ARRAY_LENGTH(buffer), "%d", value);		
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_CYCLETIMESECTXT), buffer);

			for (int i = 0; i < NUMHISTORYTAB; i++)
			{
				(void)ComboBox_InsertString(GetDlgItem(hDlg, IDC_HISTORY_TAB), i, g_ComboBoxHistoryTab[i].m_pText);
				(void)ComboBox_SetItemData(GetDlgItem(hDlg, IDC_HISTORY_TAB), i, g_ComboBoxHistoryTab[i].m_pData);
			}

			if (GetHistoryTab() < MAX_TAB_TYPES) 
				(void)ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_HISTORY_TAB), GetHistoryTab());
			else 
				(void)ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_HISTORY_TAB), GetHistoryTab() - TAB_SUBTRACT);

			SendMessage(GetDlgItem(hDlg, IDC_SCREENSHOT_BORDERSIZE), TBM_SETRANGE, true, MAKELPARAM(0, 50)); /* [0, 50] */
			SendMessage(GetDlgItem(hDlg, IDC_SCREENSHOT_BORDERSIZE), TBM_SETTICFREQ, 5, 0);
			value = GetScreenshotBorderSize();
			SendMessage(GetDlgItem(hDlg, IDC_SCREENSHOT_BORDERSIZE), TBM_SETPOS, true, value);
			snprintf(buffer, WINUI_ARRAY_LENGTH(buffer), "%d", value);		
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_SCREENSHOT_BORDERSIZETXT), buffer);
			EnableWindow(GetDlgItem(hDlg, IDC_ENABLE_SEVENZIP), GetEnableFastAudit() ? true : false);
			break;
		}

		case WM_CTLCOLORDLG:
			return (LRESULT) hBrush;	

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
			return (LRESULT) hBrush;

		case WM_HSCROLL:
			HANDLE_WM_HSCROLL(hDlg, wParam, lParam, OnHScroll);
			break;

		case WM_COMMAND:
			switch (GET_WM_COMMAND_ID(wParam, lParam))
			{
				case IDC_SCREENSHOT_BORDERCOLOR:
				{
					CHOOSECOLOR cc;
					COLORREF choice_colors[16];			

					for (int i = 0; i < 16; i++)
						choice_colors[i] = GetCustomColor(i);

					cc.lStructSize = sizeof(CHOOSECOLOR);
					cc.hwndOwner = hDlg;
					cc.lpfnHook = &CCHookProc;
					cc.rgbResult = GetScreenshotBorderColor();
					cc.lpCustColors = choice_colors;
					cc.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_FULLOPEN | CC_ENABLEHOOK;

					if (!ChooseColor(&cc))
						return true;

					for (int i = 0; i < 16; i++)
						SetCustomColor(i,choice_colors[i]);

					SetScreenshotBorderColor(cc.rgbResult);
					UpdateScreenShot();
					return true;
				}

				case IDC_ENABLE_FASTAUDIT:
				{
					bool enabled = Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLE_FASTAUDIT));
					EnableWindow(GetDlgItem(hDlg, IDC_ENABLE_SEVENZIP), enabled ? true : false);
					return true;
				}

				case IDOK:
				{
					bool bRedrawList = false;
					bool checked = false;
					int value = 0;

					SetJoyGUI(Button_GetCheck(GetDlgItem(hDlg, IDC_JOY_GUI)));
					SetMinimizeTrayIcon(Button_GetCheck(GetDlgItem(hDlg, IDC_DISABLE_TRAY_ICON)));
					SetExitDialog(Button_GetCheck(GetDlgItem(hDlg, IDC_EXIT_DIALOG)));
					SetEnableFastAudit(Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLE_FASTAUDIT)));
					SetEnableSevenZip(Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLE_SEVENZIP)));
					SetEnableDatafiles(Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLE_DATAFILES)));
					SetSkipBiosMenu(Button_GetCheck(GetDlgItem(hDlg, IDC_SKIP_BIOS)));

					if (Button_GetCheck(GetDlgItem(hDlg, IDC_RESET_PLAYCOUNT)))
					{
						for (int i = 0; i < driver_list::total(); i++)
							ResetPlayCount(i);

						bRedrawList = true;
					}

					if (Button_GetCheck(GetDlgItem(hDlg, IDC_RESET_PLAYTIME)))
					{
						for (int i = 0; i < driver_list::total(); i++)
							ResetPlayTime(i);

						bRedrawList = true;
					}

					value = SendDlgItemMessage(hDlg, IDC_CYCLETIMESEC, TBM_GETPOS, 0, 0);

					if (GetCycleScreenshot() != value)
						SetCycleScreenshot(value);

					value = SendDlgItemMessage(hDlg, IDC_SCREENSHOT_BORDERSIZE, TBM_GETPOS, 0, 0);

					if (GetScreenshotBorderSize() != value)
					{
						SetScreenshotBorderSize(value);
						UpdateScreenShot();
					}

					checked = Button_GetCheck(GetDlgItem(hDlg, IDC_STRETCH_SCREENSHOT_LARGER));

					if (checked != GetStretchScreenShotLarger())
					{
						SetStretchScreenShotLarger(checked);
						UpdateScreenShot();
					}

					checked = Button_GetCheck(GetDlgItem(hDlg, IDC_FILTER_INHERIT));

					if (checked != GetFilterInherit())
					{
						SetFilterInherit(checked);
						bRedrawList = true;
					}

					checked = Button_GetCheck(GetDlgItem(hDlg, IDC_USE_BROKEN_ICON));

					if (checked != GetUseBrokenIcon())
					{
						SetUseBrokenIcon(checked);
						bRedrawList = true;
					}

					checked = Button_GetCheck(GetDlgItem(hDlg, IDC_DISPLAY_NO_ROMS));

					if (checked != GetDisplayNoRomsGames())
					{
						SetDisplayNoRomsGames(checked);
						bRedrawList = true;
					}

					int nCurSelection = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_HISTORY_TAB));
					int nHistoryTab = 0;

					if (nCurSelection != CB_ERR)
						nHistoryTab = ComboBox_GetItemData(GetDlgItem(hDlg, IDC_HISTORY_TAB), nCurSelection);

					DestroyIcon(hIcon);
					DeleteObject(hBrush);
					EndDialog(hDlg, 0);

					if(GetHistoryTab() != nHistoryTab)
					{
						SetHistoryTab(nHistoryTab, true);
						ResizePickerControls(GetMainWindow());
						UpdateScreenShot();
					}

					if(bRedrawList)
						UpdateListView();

					SaveInternalUI();
					return true;
				}

				case IDCANCEL:
					DestroyIcon(hIcon);
					DeleteObject(hBrush);
					EndDialog(hDlg, 0);
					return true;
			}

			break;
	}

	return false;
}

INT_PTR CALLBACK FilterDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
		case WM_INITDIALOG:
		{
			CenterWindow(hDlg);
			hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAMEUI_ICON));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			hBrush = CreateSolidBrush(RGB(240, 240, 240));
			DisableVisualStylesFilters(hDlg);
			LPTREEFOLDER folder = GetCurrentFolder();
			LPTREEFOLDER lpParent = NULL;
			LPCFILTER_ITEM g_lpFilterList = GetFilterList();
			dwFilters = 0;

			if (folder != NULL)
			{
				char tmp[80];
				win_set_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_EDIT), g_FilterText);
				Edit_SetSel(GetDlgItem(hDlg, IDC_FILTER_EDIT), 0, -1);
				// Mask out non filter flags
				dwFilters = folder->m_dwFlags & F_MASK;
				// Display current folder name in dialog titlebar
				snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "Filters for %s folder", folder->m_lpTitle);
				win_set_window_text_utf8(hDlg, tmp);

				if (GetFilterInherit())
				{
					lpParent = GetFolder(folder->m_nParent);
					bool bShowExplanation = false;

					if (lpParent)
					{
						char strText[24];
						/* Check the Parent Filters and inherit them on child,
						* No need to promote all games to parent folder, works as is */
						dwpFilters = lpParent->m_dwFlags & F_MASK;
						/*Check all possible Filters if inherited solely from parent, e.g. not being set explicitly on our folder*/

						if ((dwpFilters & F_CLONES) && !(dwFilters & F_CLONES))
						{
							/*Add a Specifier to the Checkbox to show it was inherited from the parent*/
							win_get_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_CLONES), strText, 20);
							strcat(strText, " (*)");
							win_set_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_CLONES), strText);
							bShowExplanation = true;
						}

						if ((dwpFilters & F_NONWORKING) && !(dwFilters & F_NONWORKING))
						{
							/*Add a Specifier to the Checkbox to show it was inherited from the parent*/
							win_get_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_NONWORKING), strText, 20);
							strcat(strText, " (*)");
							win_set_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_NONWORKING), strText);
							bShowExplanation = true;
						}

						if ((dwpFilters & F_UNAVAILABLE) && !(dwFilters & F_UNAVAILABLE))
						{
							/*Add a Specifier to the Checkbox to show it was inherited from the parent*/
							win_get_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_UNAVAILABLE), strText, 20);
							strcat(strText, " (*)");
							win_set_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_UNAVAILABLE), strText);
							bShowExplanation = true;
						}

						if ((dwpFilters & F_VECTOR) && !(dwFilters & F_VECTOR))
						{
							/*Add a Specifier to the Checkbox to show it was inherited from the parent*/
							win_get_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_VECTOR), strText, 20);
							strcat(strText, " (*)");
							win_set_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_VECTOR), strText);
							bShowExplanation = true;
						}

						if ((dwpFilters & F_RASTER) && !(dwFilters & F_RASTER))
						{
							/*Add a Specifier to the Checkbox to show it was inherited from the parent*/
							win_get_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_RASTER), strText, 20);
							strcat(strText, " (*)");
							win_set_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_RASTER), strText);
							bShowExplanation = true;
						}

						if ((dwpFilters & F_ORIGINALS) && !(dwFilters & F_ORIGINALS))
						{
							/*Add a Specifier to the Checkbox to show it was inherited from the parent*/
							win_get_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_ORIGINALS), strText, 20);
							strcat(strText, " (*)");
							win_set_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_ORIGINALS), strText);
							bShowExplanation = true;
						}

						if ((dwpFilters & F_WORKING) && !(dwFilters & F_WORKING))
						{
							/*Add a Specifier to the Checkbox to show it was inherited from the parent*/
							win_get_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_WORKING), strText, 20);
							strcat(strText, " (*)");
							win_set_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_WORKING), strText);
							bShowExplanation = true;
						}

						if ((dwpFilters & F_AVAILABLE) && !(dwFilters & F_AVAILABLE))
						{
							/*Add a Specifier to the Checkbox to show it was inherited from the parent*/
							win_get_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_AVAILABLE), strText, 20);
							strcat(strText, " (*)");
							win_set_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_AVAILABLE), strText);
							bShowExplanation = true;
						}

						if ((dwpFilters & F_HORIZONTAL) && !(dwFilters & F_HORIZONTAL))
						{
							/*Add a Specifier to the Checkbox to show it was inherited from the parent*/
							win_get_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_HORIZONTAL), strText, 20);
							strcat(strText, " (*)");
							win_set_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_HORIZONTAL), strText);
							bShowExplanation = true;
						}

						if ((dwpFilters & F_VERTICAL) && !(dwFilters & F_VERTICAL))
						{
							/*Add a Specifier to the Checkbox to show it was inherited from the parent*/
							win_get_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_VERTICAL), strText, 20);
							strcat(strText, " (*)");
							win_set_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_VERTICAL), strText);
							bShowExplanation = true;
						}
						/*Do not or in the Values of the parent, so that the values of the folder still can be set*/
						//dwFilters |= dwpFilters;
					}

					if(bShowExplanation)
					{
						ShowWindow(GetDlgItem(hDlg, IDC_INHERITED), true);
					}
				}
				else
				{
					ShowWindow(GetDlgItem(hDlg, IDC_INHERITED), false);
				}

				// Find the matching filter record if it exists
				lpFilterRecord = FindFilter(folder->m_nFolderId);

				// initialize and disable appropriate controls
				for (int i = 0; g_lpFilterList[i].m_dwFilterType; i++)
				{
					DisableFilterControls(hDlg, lpFilterRecord, &g_lpFilterList[i], dwFilters);
				}
			}

			SetFocus(GetDlgItem(hDlg, IDC_FILTER_EDIT));
			return false;
		}

		case WM_CTLCOLORDLG:
			return (LRESULT) hBrush;	

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
			return (LRESULT) hBrush;

		case WM_COMMAND:
		{
			WORD wID = GET_WM_COMMAND_ID(wParam, lParam);
			WORD wNotifyCode = GET_WM_COMMAND_CMD(wParam, lParam);
			LPTREEFOLDER folder = GetCurrentFolder();
			LPCFILTER_ITEM g_lpFilterList = GetFilterList();

			switch (wID)
			{
				case IDOK:
					dwFilters = 0;
					win_get_window_text_utf8(GetDlgItem(hDlg, IDC_FILTER_EDIT), g_FilterText, FILTERTEXT_LEN);

					// see which buttons are checked
					for (int i = 0; g_lpFilterList[i].m_dwFilterType; i++)
					{
						if (Button_GetCheck(GetDlgItem(hDlg, g_lpFilterList[i].m_dwCtrlID)))
							dwFilters |= g_lpFilterList[i].m_dwFilterType;
					}

					// Mask out invalid filters
					dwFilters = ValidateFilters(lpFilterRecord, dwFilters);
					// Keep non filter flags
					folder->m_dwFlags &= ~F_MASK;
					// put in the set filters
					folder->m_dwFlags |= dwFilters;
					DestroyIcon(hIcon);
					DeleteObject(hBrush);
					EndDialog(hDlg, 1);
					return true;

				case IDCANCEL:
					DestroyIcon(hIcon);
					DeleteObject(hBrush);
					EndDialog(hDlg, 0);
					return true;

				default:
					// Handle unchecking mutually exclusive filters
					if (wNotifyCode == BN_CLICKED)
						EnableFilterExclusions(hDlg, wID);
			}

			break;
		}
	}

	return false;
}

INT_PTR CALLBACK AboutDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
		case WM_INITDIALOG:
		{
			char tmp[256];
			CenterWindow(hDlg);
			hBrush = CreateSolidBrush(RGB(235, 233, 237));
			HBITMAP hBmp = (HBITMAP)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_SPLASH), IMAGE_BITMAP, 0, 0, LR_SHARED);
			SendMessage(GetDlgItem(hDlg, IDC_ABOUT), STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
			hFont = CreateFont(-11, 0, 0, 0, 400, 0, 0, 0, 0, 3, 2, 1, 34, TEXT("Verdana"));
			hFontFX = CreateFont(-12, 0, 0, 0, 400, 0, 0, 0, 0, 3, 2, 1, 34, TEXT("Verdana"));
			SetWindowFont(GetDlgItem(hDlg, IDC_TEXT1), hFont, true);
			SetWindowFont(GetDlgItem(hDlg, IDC_TEXT2), hFont, true);
			SetWindowFont(GetDlgItem(hDlg, IDC_TEXT3), hFont, true);
			SetWindowFont(GetDlgItem(hDlg, IDC_TEXT4), hFont, true);
			SetWindowFont(GetDlgItem(hDlg, IDC_SICKFX), hFontFX, true);
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_BUILD), "Build time: " __DATE__" - " __TIME__"");
			snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "Version: %s", MAME_VERSION);
			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_BUILDVER), tmp);
			return true;
		}

		case WM_CTLCOLORDLG:
			return (LRESULT) hBrush;

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));

			if ((HWND)lParam == GetDlgItem(hDlg, IDC_SICKFX))
				SetTextColor(hDC, RGB(63, 72, 204));

			return (LRESULT) hBrush;

		case WM_COMMAND:
			switch (GET_WM_COMMAND_ID(wParam, lParam))
			{
				case IDOK:
				case IDCANCEL:
					DeleteObject(hFont);
					DeleteObject(hFontFX);
					DeleteObject(hBrush);
					EndDialog(hDlg, 0);
					return true;
			}

			break;
	}

	return false;
}

INT_PTR CALLBACK AddCustomFileDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
		case WM_INITDIALOG:
		{
			CenterWindow(hDlg);
			hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAMEUI_ICON));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			hBrush = CreateSolidBrush(RGB(240, 240, 240));

			if (IsWindowsSevenOrHigher())
				SendMessage(GetDlgItem(hDlg, IDC_CUSTOM_TREE), TVM_SETEXTENDEDSTYLE, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

			SetWindowTheme(GetDlgItem(hDlg, IDC_CUSTOM_TREE), L"Explorer", NULL);

			TREEFOLDER **folders;
			int num_folders = 0;
			TVINSERTSTRUCT tvis;
			TVITEM tvi;
			bool first_entry = true;
			HIMAGELIST treeview_icons = GetTreeViewIconList();

			// current game passed in using DialogBoxParam()
			driver_index = lParam;
			(void)TreeView_SetImageList(GetDlgItem(hDlg, IDC_CUSTOM_TREE), treeview_icons, LVSIL_NORMAL);
			GetFolders(&folders, &num_folders);

			// insert custom folders into our tree view
			for (int i = 0; i < num_folders; i++)
			{
				if (folders[i]->m_dwFlags & F_CUSTOM)
				{
					HTREEITEM hti;

					if (folders[i]->m_nParent == -1)
					{
						memset(&tvi, 0, sizeof(TVITEM));
						tvis.hParent = TVI_ROOT;
						tvis.hInsertAfter = TVI_SORT;
						tvi.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
						tvi.pszText = folders[i]->m_lptTitle;
						tvi.lParam = (LPARAM)folders[i];
						tvi.iImage = GetTreeViewIconIndex(folders[i]->m_nIconId);
						tvi.iSelectedImage = 0;
						tvis.item = tvi;

						hti = TreeView_InsertItem(GetDlgItem(hDlg, IDC_CUSTOM_TREE), &tvis);

						/* look for children of this custom folder */
						for (int jj = 0; jj < num_folders; jj++)
						{
							if (folders[jj]->m_nParent == i)
							{
								HTREEITEM hti_child;
								tvis.hParent = hti;
								tvis.hInsertAfter = TVI_SORT;
								tvi.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
								tvi.pszText = folders[jj]->m_lptTitle;
								tvi.lParam = (LPARAM)folders[jj];
								tvi.iImage = GetTreeViewIconIndex(folders[jj]->m_nIconId);
								tvi.iSelectedImage = 0;
								tvis.item = tvi;

								hti_child = TreeView_InsertItem(GetDlgItem(hDlg, IDC_CUSTOM_TREE), &tvis);

								if (folders[jj] == default_selection)
									(void)TreeView_SelectItem(GetDlgItem(hDlg, IDC_CUSTOM_TREE), hti_child);
							}
						}

						/*TreeView_Expand(GetDlgItem(hDlg,IDC_CUSTOM_TREE),hti,TVE_EXPAND);*/
						if (first_entry || folders[i] == default_selection)
						{
							(void)TreeView_SelectItem(GetDlgItem(hDlg, IDC_CUSTOM_TREE),hti);
							first_entry = false;
						}
					}
				}
			}

			win_set_window_text_utf8(GetDlgItem(hDlg, IDC_CUSTOMFILE_GAME), GetDriverGameTitle(driver_index));
			return true;
		}

		case WM_CTLCOLORDLG:
			return (LRESULT) hBrush;

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
			return (LRESULT) hBrush;

		case WM_COMMAND:
			switch (GET_WM_COMMAND_ID(wParam, lParam))
			{
				case IDOK:
				{
					TVITEM tvi;
					tvi.hItem = TreeView_GetSelection(GetDlgItem(hDlg, IDC_CUSTOM_TREE));
					tvi.mask = TVIF_PARAM;

					if (TreeView_GetItem(GetDlgItem(hDlg, IDC_CUSTOM_TREE),&tvi) == true)
					{
						/* should look for New... */
						default_selection = (LPTREEFOLDER)tvi.lParam; 	/* start here next time */
						AddToCustomFolder((LPTREEFOLDER)tvi.lParam, driver_index);
					}

					DestroyIcon(hIcon);
					DeleteObject(hBrush);
					EndDialog(hDlg, 0);
					return true;
				}

				case IDCANCEL:
					DestroyIcon(hIcon);
					DeleteObject(hBrush);
					EndDialog(hDlg, 0);
					return true;
			}

			break;
	}

	return false;
}

/***************************************************************************
    private functions
 ***************************************************************************/

static void DisableFilterControls(HWND hWnd, LPCFOLDERDATA lpFilterRecord, LPCFILTER_ITEM lpFilterItem, DWORD dwFlags)
{
	HWND  hWndCtrl = GetDlgItem(hWnd, lpFilterItem->m_dwCtrlID);
	DWORD dwFilterType = lpFilterItem->m_dwFilterType;

	/* Check the appropriate control */
	if (dwFilterType & dwFlags)
		Button_SetCheck(hWndCtrl, MF_CHECKED);

	/* No special rules for this folder? */
	if (!lpFilterRecord)
		return;

	/* If this is an excluded filter */
	if (lpFilterRecord->m_dwUnset & dwFilterType)
	{
		/* uncheck it and disable the control */
		Button_SetCheck(hWndCtrl, MF_UNCHECKED);
		EnableWindow(hWndCtrl, false);
	}

	/* If this is an implied filter, check it and disable the control */
	if (lpFilterRecord->m_dwSet & dwFilterType)
	{
		Button_SetCheck(hWndCtrl, MF_CHECKED);
		EnableWindow(hWndCtrl, false);
	}
}

// Handle disabling mutually exclusive controls
static void EnableFilterExclusions(HWND hWnd, DWORD dwCtrlID)
{
	int i = 0;
	DWORD id = 0;

	for (i = 0; i < NUM_EXCLUSIONS; i++)
	{
		// is this control in the list?
		if (filterExclusion[i] == dwCtrlID)
			// found the control id
			break;
	}

	// if the control was found
	if (i < NUM_EXCLUSIONS)
	{
		// find the opposing control id
		if (i % 2)
			id = filterExclusion[i - 1];
		else
			id = filterExclusion[i + 1];

		// Uncheck the other control
		Button_SetCheck(GetDlgItem(hWnd, id), MF_UNCHECKED);
	}
}

// Validate filter setting, mask out inappropriate filters for this folder
static DWORD ValidateFilters(LPCFOLDERDATA lpFilterRecord, DWORD dwFlags)
{
	if (lpFilterRecord != (LPFOLDERDATA)0)
	{
		// Mask out implied and excluded filters
		DWORD dwFilters = lpFilterRecord->m_dwSet | lpFilterRecord->m_dwUnset;
		return dwFlags & ~dwFilters;
	}

	// No special cases - all filters apply
	return dwFlags;
}

static void OnHScroll(HWND hWnd, HWND hWndCtl, UINT code, int pos)
{
	int value = 0;
	char tmp[8];

	if (hWndCtl == GetDlgItem(hWnd, IDC_CYCLETIMESEC))
	{
		value = SendMessage(GetDlgItem(hWnd, IDC_CYCLETIMESEC), TBM_GETPOS, 0, 0);
		snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "%d", value);
		win_set_window_text_utf8(GetDlgItem(hWnd, IDC_CYCLETIMESECTXT), tmp);
	}
	else if (hWndCtl == GetDlgItem(hWnd, IDC_SCREENSHOT_BORDERSIZE))
	{
		value = SendMessage(GetDlgItem(hWnd, IDC_SCREENSHOT_BORDERSIZE), TBM_GETPOS, 0, 0);
		snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "%d", value);
		win_set_window_text_utf8(GetDlgItem(hWnd, IDC_SCREENSHOT_BORDERSIZETXT), tmp);
	}
}

static void DisableVisualStylesInterface(HWND hDlg)
{
	SetWindowTheme(GetDlgItem(hDlg, IDC_DISABLE_TRAY_ICON), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_EXIT_DIALOG), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_JOY_GUI), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_USE_BROKEN_ICON), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FILTER_INHERIT), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_DISPLAY_NO_ROMS), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_ENABLE_FASTAUDIT), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_ENABLE_SEVENZIP), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_ENABLE_DATAFILES), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_SKIP_BIOS), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_HISTORY_TAB), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_RESET_PLAYCOUNT), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_RESET_PLAYTIME), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_STRETCH_SCREENSHOT_LARGER), L" ", L" ");
}

static void DisableVisualStylesReset(HWND hDlg)
{
	SetWindowTheme(GetDlgItem(hDlg, IDC_RESET_UI), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_RESET_MAME_UI), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_RESET_DEFAULT), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_RESET_GAMES), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_RESET_FILTERS), L" ", L" ");
}

static void DisableVisualStylesFilters(HWND hDlg)
{
	SetWindowTheme(GetDlgItem(hDlg, IDC_FILTER_EDIT), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FILTER_VECTOR), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FILTER_CLONES), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FILTER_NONWORKING), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FILTER_HORIZONTAL), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FILTER_UNAVAILABLE), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FILTER_RASTER), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FILTER_ORIGINALS), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FILTER_WORKING), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FILTER_VERTICAL), L" ", L" ");
	SetWindowTheme(GetDlgItem(hDlg, IDC_FILTER_AVAILABLE), L" ", L" ");
}
