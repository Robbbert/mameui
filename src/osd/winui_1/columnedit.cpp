// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include "winui.h"

static HWND hShown = NULL;
static HWND hAvailable = NULL;
static HBRUSH hBrush = NULL;
static HDC hDC = NULL;
static HICON hIcon = NULL;
static bool showMsg = false;

// Returns true if successful
static int DoExchangeItem(HWND hFrom, HWND hTo, int nMinItem)
{
	LVITEM lvi;
	TCHAR buf[80];

	lvi.iItem = ListView_GetNextItem(hFrom, -1, LVIS_SELECTED | LVIS_FOCUSED);

	if (lvi.iItem < nMinItem)
	{
		if (lvi.iItem != -1) 	// Can't remove the first column
			ErrorMessageBox("Cannot move selected item");

		SetFocus(hFrom);
		return 0;
	}

	lvi.iSubItem = 0;
	lvi.mask = LVIF_PARAM | LVIF_TEXT;
	lvi.pszText = buf;
	lvi.cchTextMax = WINUI_ARRAY_LENGTH(buf);

	if (ListView_GetItem(hFrom, &lvi))
	{
		// Add this item to the Show and delete it from Available
		(void)ListView_DeleteItem(hFrom, lvi.iItem);
		lvi.iItem = ListView_GetItemCount(hTo);
		(void)ListView_InsertItem(hTo, &lvi);
		ListView_SetItemState(hTo, lvi.iItem, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
		SetFocus(hTo);
		return lvi.iItem;
	}

	return 0;
}

static void DoMoveItem( HWND hWnd, bool bDown)
{
	LVITEM lvi;
	TCHAR buf[80];

	lvi.iItem = ListView_GetNextItem(hWnd, -1, LVIS_SELECTED | LVIS_FOCUSED);
	int nMaxpos = ListView_GetItemCount(hWnd);

	if (lvi.iItem == -1 ||
		(lvi.iItem <  2 && bDown == false) || 	// Disallow moving First column
		(lvi.iItem == 0 && bDown == true)  || 	// ""
		(lvi.iItem == nMaxpos - 1 && bDown == true))
	{
		SetFocus(hWnd);
		return;
	}

	lvi.iSubItem = 0;
	lvi.mask = LVIF_PARAM | LVIF_TEXT;
	lvi.pszText = buf;
	lvi.cchTextMax = WINUI_ARRAY_LENGTH(buf);

	if (ListView_GetItem(hWnd, &lvi))
	{
		// Add this item to the Show and delete it from Available
		(void)ListView_DeleteItem(hWnd, lvi.iItem);
		lvi.iItem += (bDown) ? 1 : -1;
		(void)ListView_InsertItem(hWnd,&lvi);
		ListView_SetItemState(hWnd, lvi.iItem, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);

		if (lvi.iItem == nMaxpos - 1)
			EnableWindow(GetDlgItem(GetParent(hWnd), IDC_BUTTONMOVEDOWN), false);
		else
			EnableWindow(GetDlgItem(GetParent(hWnd), IDC_BUTTONMOVEDOWN), true);

		if (lvi.iItem < 2)
			EnableWindow(GetDlgItem(GetParent(hWnd), IDC_BUTTONMOVEUP), false);
		else
			EnableWindow(GetDlgItem(GetParent(hWnd), IDC_BUTTONMOVEUP), true);

		SetFocus(hWnd);
	}
}

static INT_PTR InternalColumnDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam,
	int nColumnMax, int *shown, int *order,
	const TCHAR* const *names, void (*pfnGetRealColumnOrder)(int *),
	void (*pfnGetColumnInfo)(int *pnOrder, int *pnShown),
	void (*pfnSetColumnInfo)(int *pnOrder, int *pnShown))
{
	RECT rectClient;
	LVCOLUMN LVCol;
	int nShown = 0;
	int nAvail = 0;
	LVITEM lvi;

	switch (Msg)
	{
		case WM_INITDIALOG:
			CenterWindow(hDlg);
			hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAMEUI_ICON));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			hBrush = CreateSolidBrush(RGB(240, 240, 240));
			hShown = GetDlgItem(hDlg, IDC_LISTSHOWCOLUMNS);
			hAvailable = GetDlgItem(hDlg, IDC_LISTAVAILABLECOLUMNS);
			SetWindowTheme(hShown, L"Explorer", NULL);
			SetWindowTheme(hAvailable, L"Explorer", NULL);

			if(IsWindowsSevenOrHigher())
			{
				(void)ListView_SetExtendedListViewStyle(hShown, LVS_EX_FULLROWSELECT | LVS_EX_ONECLICKACTIVATE | LVS_EX_DOUBLEBUFFER);
				(void)ListView_SetExtendedListViewStyle(hAvailable, LVS_EX_FULLROWSELECT | LVS_EX_ONECLICKACTIVATE | LVS_EX_DOUBLEBUFFER);
			}
			else
			{
				(void)ListView_SetExtendedListViewStyle(hShown, LVS_EX_FULLROWSELECT | LVS_EX_UNDERLINEHOT | LVS_EX_ONECLICKACTIVATE | LVS_EX_DOUBLEBUFFER);
				(void)ListView_SetExtendedListViewStyle(hAvailable, LVS_EX_FULLROWSELECT | LVS_EX_UNDERLINEHOT | LVS_EX_ONECLICKACTIVATE | LVS_EX_DOUBLEBUFFER);
			}

			GetClientRect(hShown, &rectClient);

			memset(&LVCol, 0, sizeof(LVCOLUMN));
			LVCol.mask = LVCF_WIDTH;
			LVCol.cx = rectClient.right - rectClient.left;

			(void)ListView_InsertColumn(hShown, 0, &LVCol);
			(void)ListView_InsertColumn(hAvailable, 0, &LVCol);
			pfnGetColumnInfo(order, shown);
			showMsg = true;
			nShown = 0;
			nAvail = 0;

			lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
			lvi.stateMask = 0;
			lvi.iSubItem = 0;
			lvi.iImage = -1;

			/* Get the Column Order and save it */
			pfnGetRealColumnOrder(order);

			for (int i = 0 ; i < nColumnMax; i++)
			{
				lvi.pszText = (TCHAR*)names[order[i]];
				lvi.lParam = order[i];

				if (shown[order[i]])
				{
					lvi.iItem = nShown;
					(void)ListView_InsertItem(hShown, &lvi);
					nShown++;
				}
				else
				{
					lvi.iItem = nAvail;
					(void)ListView_InsertItem(hAvailable, &lvi);
					nAvail++;
				}
			}

			if(nShown > 0)
				/*Set to Second, because first is not allowed*/
				ListView_SetItemState(hShown, 1, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);

			if(nAvail > 0)
				ListView_SetItemState(hAvailable, 0, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);

			EnableWindow(GetDlgItem(hDlg, IDC_BUTTONADD), true);
			return true;

		case WM_CTLCOLORDLG:
			return (LRESULT) hBrush;	

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
			hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
			return (LRESULT) hBrush;

		case WM_NOTIFY:
		{
			NMHDR *nm = (NMHDR *)lParam;
			NM_LISTVIEW *pnmv;
			int nPos = 0;

			switch (nm->code)
			{
				case NM_DBLCLK:
					// Do Data Exchange here, which ListView was double clicked?
					switch (nm->idFrom)
					{
						case IDC_LISTAVAILABLECOLUMNS:
							// Move selected Item from Available to Shown column
							nPos = DoExchangeItem(hAvailable, hShown, 0);

							if (nPos)
							{
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONADD), false);
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONREMOVE), true);
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEUP), true);
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEDOWN), false);
							}

							break;

						case IDC_LISTSHOWCOLUMNS:
							// Move selected Item from Show to Available column
							if (DoExchangeItem(hShown, hAvailable, 1))
							{
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONADD), true);
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONREMOVE), false);
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEUP), false);
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEDOWN), false);
							}

							break;
					}

					return true;

				case LVN_ITEMCHANGED:
					// Don't handle this message for now
					pnmv = (NM_LISTVIEW *)nm;

					if (pnmv->uNewState & LVIS_SELECTED)
					{
						if (pnmv->iItem == 0 && pnmv->hdr.idFrom == IDC_LISTSHOWCOLUMNS)
						{
							// Don't allow selecting the first item
							ListView_SetItemState(hShown, pnmv->iItem, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);

							if (showMsg)
							{
								ErrorMessageBox("Changing this item is not permitted");
								showMsg = false;
							}

							EnableWindow(GetDlgItem(hDlg, IDC_BUTTONREMOVE), false);
							EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEUP), false);
							EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEDOWN), false);
							return true;
						}
						else
							showMsg = true;
					}

					if( pnmv->uOldState & LVIS_SELECTED && pnmv->iItem == 0 && pnmv->hdr.idFrom == IDC_LISTSHOWCOLUMNS)
					{
						/*we enable the buttons again, if the first Entry loses selection*/
						EnableWindow(GetDlgItem(hDlg, IDC_BUTTONREMOVE), true);
						EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEUP), true);
						EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEDOWN), true);
					}

					break;

				case NM_SETFOCUS:
					switch (nm->idFrom)
					{
						case IDC_LISTAVAILABLECOLUMNS:
							if (ListView_GetItemCount(nm->hwndFrom) != 0)
							{
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONADD), true);
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONREMOVE), false);
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEDOWN), false);
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEUP), false);
							}

							break;

						case IDC_LISTSHOWCOLUMNS:
							if (ListView_GetItemCount(nm->hwndFrom) != 0)
							{
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONADD), false);

								if (ListView_GetNextItem(hShown, -1, LVIS_SELECTED | LVIS_FOCUSED) == 0)
								{
									EnableWindow(GetDlgItem(hDlg, IDC_BUTTONREMOVE), false);
									EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEDOWN), false);
									EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEUP), false);
								}
								else
								{
									EnableWindow(GetDlgItem(hDlg, IDC_BUTTONREMOVE), true);
									EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEDOWN), true);
									EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEUP), true);
								}
							}

							break;
					}

					break;

				case LVN_KEYDOWN:
				case NM_CLICK:
					pnmv = (NM_LISTVIEW *)nm;

					if (pnmv->uNewState & LVIS_SELECTED)
					{
						if (pnmv->iItem == 0 && pnmv->hdr.idFrom == IDC_LISTSHOWCOLUMNS)
						{
							// nothing to do here...
						}
					}

					switch (nm->idFrom)
					{
						case IDC_LISTAVAILABLECOLUMNS:
							if (ListView_GetItemCount(nm->hwndFrom) != 0)
							{
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONADD), true);
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONREMOVE), false);
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEDOWN), false);
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEUP), false);
							}

							break;

						case IDC_LISTSHOWCOLUMNS:
							if (ListView_GetItemCount(nm->hwndFrom) != 0)
							{
								EnableWindow(GetDlgItem(hDlg, IDC_BUTTONADD), false);

								if (ListView_GetNextItem(hShown, -1, LVIS_SELECTED | LVIS_FOCUSED) == 0)
								{
									EnableWindow(GetDlgItem(hDlg, IDC_BUTTONREMOVE), false);
									EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEDOWN), false);
									EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEUP), false);
								}
								else
								{
									EnableWindow(GetDlgItem(hDlg, IDC_BUTTONREMOVE), true);
									EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEDOWN), true);
									EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEUP), true);
								}
							}

							break;
					}

					return true;
			}
		}

		return false;

		case WM_COMMAND:
		{
			WORD wID = GET_WM_COMMAND_ID(wParam, lParam);
			HWND hWndCtrl = GET_WM_COMMAND_HWND(wParam, lParam);
			int nPos = 0;

			switch (wID)
			{
				case IDC_LISTSHOWCOLUMNS:
					break;

				case IDC_BUTTONADD:
					// Move selected Item in Available to Shown
					nPos = DoExchangeItem(hAvailable, hShown, 0);

					if (nPos)
					{
						EnableWindow(hWndCtrl,false);
						EnableWindow(GetDlgItem(hDlg, IDC_BUTTONREMOVE), true);
						EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEUP), true);
						EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEDOWN), false);
					}

					break;

				case IDC_BUTTONREMOVE:
					// Move selected Item in Show to Available
					if (DoExchangeItem(hShown, hAvailable, 1))
					{
						EnableWindow(hWndCtrl,false);
						EnableWindow(GetDlgItem(hDlg, IDC_BUTTONADD), true);
						EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEUP), false);
						EnableWindow(GetDlgItem(hDlg, IDC_BUTTONMOVEDOWN), false);
					}

					break;

				case IDC_BUTTONMOVEDOWN:
					// Move selected item in the Show window up 1 item
					DoMoveItem(hShown, true);
					break;

				case IDC_BUTTONMOVEUP:
					// Move selected item in the Show window down 1 item
					DoMoveItem(hShown, false);
					break;

				case IDOK:
				{
					// Save users choices
					nShown = ListView_GetItemCount(hShown);
					nAvail = ListView_GetItemCount(hAvailable);
					int nCount = 0;

					for (int i = 0; i < nShown; i++)
					{
						lvi.iSubItem = 0;
						lvi.mask = LVIF_PARAM;
						lvi.pszText = 0;
						lvi.iItem  = i;

						(void)ListView_GetItem(hShown, &lvi);
						order[nCount++] = lvi.lParam;
						shown[lvi.lParam] = true;
					}

					for (int i = 0; i < nAvail; i++)
					{
						lvi.iSubItem = 0;
						lvi.mask  = LVIF_PARAM;
						lvi.pszText = 0;
						lvi.iItem  = i;

						(void)ListView_GetItem(hAvailable, &lvi);
						order[nCount++]   = lvi.lParam;
						shown[lvi.lParam] = false;
					}

					pfnSetColumnInfo(order, shown);
					DestroyIcon(hIcon);
					DeleteObject(hBrush);
					EndDialog(hDlg, 1);
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
	}

	return false;
}

static void GetColumnInfo(int *order, int *shown)
{
	GetColumnOrder(order);
	GetColumnShown(shown);
}

static void SetColumnInfo(int *order, int *shown)
{
	SetColumnOrder(order);
	SetColumnShown(shown);
}

INT_PTR CALLBACK ColumnDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static int shown[COLUMN_MAX];
	static int order[COLUMN_MAX];
	extern const TCHAR* const column_names[COLUMN_MAX]; // from winui.c, should improve

	return InternalColumnDialogProc(hDlg, Msg, wParam, lParam, COLUMN_MAX,
		shown, order, column_names, GetRealColumnOrder, GetColumnInfo, SetColumnInfo);
}
