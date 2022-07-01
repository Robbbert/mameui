// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include "winui.h"

struct PickerInfo
{
	const struct PickerCallbacks *pCallbacks;
	WNDPROC pfnParentWndProc;
	int nCurrentViewID;
	int nLastItem;
	int nColumnCount;
	int *pnColumnsShown;
	int *pnColumnsOrder;
	UINT_PTR nTimer;
	const TCHAR* const *ppszColumnNames;
};

struct CompareProcParams
{
	HWND hWndPicker;
	struct PickerInfo *pPickerInfo;
	int nSortColumn;
	int nViewMode;
	bool bReverse;
};

static struct PickerInfo *GetPickerInfo(HWND hWnd)
{
	LONG_PTR l= GetWindowLongPtr(hWnd, GWLP_USERDATA);
	
	return (struct PickerInfo *) l;
}

static LRESULT CallParentWndProc(WNDPROC pfnParentWndProc, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (!pfnParentWndProc)
		pfnParentWndProc = GetPickerInfo(hWnd)->pfnParentWndProc;

	LRESULT rc = CallWindowProc(pfnParentWndProc, hWnd, message, wParam, lParam);

	return rc;
}

static bool ListViewNotify(HWND hWnd, LPNMHDR lpNmHdr)
{
	RECT rcClient;
	POINT pt;

	if (lpNmHdr->code == HDN_ITEMCHANGINGA || lpNmHdr->code == HDN_ITEMCHANGINGW)
	{
		DWORD dwPos = GetMessagePos();
		pt.x = LOWORD(dwPos);
		pt.y = HIWORD(dwPos);
		GetClientRect(hWnd, &rcClient);
		ScreenToClient(hWnd, &pt);
		rcClient.left = pt.x;
		InvalidateRect(hWnd, &rcClient, false);
	}

	return false;
}

static bool ListViewContextMenu(HWND hWndPicker, LPARAM lParam)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);
	POINT pt, headerPt;
	int nColumn = -1;
	RECT rcCol;

	// Extract the point out of the lparam
	pt.x = GET_X_LPARAM(lParam);
	pt.y = GET_Y_LPARAM(lParam);

	if (pt.x < 0 && pt.y < 0)
		GetCursorPos(&pt);

	HWND hwndHeader = ListView_GetHeader(hWndPicker);
	headerPt = pt;
	ScreenToClient(hwndHeader, &headerPt);

	for (int i = 0; Header_GetItemRect(hwndHeader, i, &rcCol); i++)
	{
		if (PtInRect(&rcCol, headerPt))
		{
			nColumn = i;
			break;
		}
	}

	if (nColumn >= 0)
	{
		// A column header was clicked
		if (pPickerInfo->pCallbacks->pfnOnHeaderContextMenu)
			pPickerInfo->pCallbacks->pfnOnHeaderContextMenu(pt, nColumn);
	}
	else
	{
		// The body was clicked
		if (pPickerInfo->pCallbacks->pfnOnBodyContextMenu)
			pPickerInfo->pCallbacks->pfnOnBodyContextMenu(pt);
	}

	return true;
}

static void Picker_Free(struct PickerInfo *pPickerInfo)
{
	// Free up all resources associated with this picker structure
	if (pPickerInfo->pnColumnsShown)
		free(pPickerInfo->pnColumnsShown);

	if (pPickerInfo->pnColumnsOrder)
		free(pPickerInfo->pnColumnsOrder);

	free(pPickerInfo);
}

static LRESULT CALLBACK ListViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWnd);
	LRESULT rc = 0;
	bool bHandled = false;
	WNDPROC pfnParentWndProc = pPickerInfo->pfnParentWndProc;
	HWND hWndHeaderCtrl  = NULL;
	HFONT hHeaderCtrlFont = NULL;

	switch(message)
	{
		case WM_NOTIFY:
			bHandled = ListViewNotify(hWnd, (LPNMHDR) lParam);
			break;

		case WM_SETFONT:
			hWndHeaderCtrl = ListView_GetHeader(hWnd);
			if (hWndHeaderCtrl)
				hHeaderCtrlFont = GetWindowFont(hWndHeaderCtrl);
			break;

		case WM_CONTEXTMENU:
			bHandled = ListViewContextMenu(hWnd, lParam);
			break;

		case WM_DESTROY:
			// Received WM_DESTROY; time to clean up
			if (pPickerInfo->pCallbacks->pfnSetViewMode)
				pPickerInfo->pCallbacks->pfnSetViewMode(pPickerInfo->nCurrentViewID);

			Picker_Free(pPickerInfo);
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR) pfnParentWndProc);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			break;
	}

	if (!bHandled)
		rc = CallParentWndProc(pfnParentWndProc, hWnd, message, wParam, lParam);

	 // If we received WM_SETFONT, reset header ctrl font back to original font
	if (hWndHeaderCtrl)
		SetWindowFont(hWndHeaderCtrl, hHeaderCtrlFont, true);

	return rc;
}

// Re/initialize the ListControl Columns
static void Picker_InternalResetColumnDisplay(HWND hWnd, bool bFirstTime)
{
	LVCOLUMN lvc;
	int i = 0;
	int nColumn = 0;
	LVCOLUMN col;
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWnd);

	int *widths = (int*)malloc(pPickerInfo->nColumnCount * sizeof(*widths));
	int *order = (int*)malloc(pPickerInfo->nColumnCount * sizeof(*order));
	int *shown = (int*)malloc(pPickerInfo->nColumnCount * sizeof(*shown));

	if (!widths || !order || !shown)
		goto done;

	memset(widths, 0, pPickerInfo->nColumnCount * sizeof(*widths));
	memset(order, 0, pPickerInfo->nColumnCount * sizeof(*order));
	memset(shown, 0, pPickerInfo->nColumnCount * sizeof(*shown));
	pPickerInfo->pCallbacks->pfnGetColumnWidths(widths);
	pPickerInfo->pCallbacks->pfnGetColumnOrder(order);
	pPickerInfo->pCallbacks->pfnGetColumnShown(shown);

	if (!bFirstTime)
	{
		// Retrieve each of the column widths
		i = 0;
		memset(&col, 0, sizeof(LVCOLUMN));
		col.mask = LVCF_WIDTH;

		while(ListView_GetColumn(hWnd, 0, &col))
		{
			nColumn = Picker_GetRealColumnFromViewColumn(hWnd, i++);
			widths[nColumn] = col.cx;
			(void)ListView_DeleteColumn(hWnd, 0);
		}

		pPickerInfo->pCallbacks->pfnSetColumnWidths(widths);
	}

	nColumn = 0;

	for (i = 0; i < pPickerInfo->nColumnCount; i++)
	{
		if (shown[order[i]])
		{
			lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_TEXT;
			lvc.pszText = (TCHAR*) pPickerInfo->ppszColumnNames[order[i]];
			lvc.iSubItem = nColumn;
			lvc.cx = widths[order[i]];
			lvc.fmt = LVCFMT_LEFT;
			(void)ListView_InsertColumn(hWnd, nColumn, &lvc);
			pPickerInfo->pnColumnsOrder[nColumn] = order[i];
			nColumn++;
		}
	}

	/* Fill this in so we can still sort on columns NOT shown */
	for (i = 0; i < pPickerInfo->nColumnCount && nColumn < pPickerInfo->nColumnCount; i++)
	{
		if (!shown[order[i]])
		{
			pPickerInfo->pnColumnsOrder[nColumn] = order[i];
			nColumn++;
		}
	}

	(void)ListView_SetBkColor(hWnd, GetListBgColor());
	(void)ListView_SetTextColor(hWnd, GetListFontColor());

done:
	if (widths)
		free(widths);

	if (order)
		free(order);

	if (shown)
		free(shown);
}

void Picker_ResetColumnDisplay(HWND hWnd)
{
	Picker_InternalResetColumnDisplay(hWnd, false);
}

void Picker_ClearIdle(HWND hWndPicker)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);

	if (pPickerInfo->nTimer)
	{
		KillTimer(hWndPicker, 0);
		pPickerInfo->nTimer = 0;
	}
}

static void CALLBACK Picker_TimerProc(HWND hWndPicker, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);
	bool bContinueIdle = false;
	DWORD nTickCount = 0;
	DWORD nBaseTickCount = GetTickCount();
	DWORD nMaxIdleTicks = 50;

	// This idle procedure will loop until either idling is over, or until
	// a specified amount of time elapses (in this case, 50ms).  This frees
	// idle callbacks of any responsibility for balancing their workloads; the
	// picker code will
	do
	{
		if (pPickerInfo->pCallbacks->pfnOnIdle)
			bContinueIdle = pPickerInfo->pCallbacks->pfnOnIdle(hWndPicker);

		nTickCount = GetTickCount();
	} while(bContinueIdle && ((nTickCount - nBaseTickCount) < nMaxIdleTicks));

	if (!bContinueIdle)
		Picker_ClearIdle(hWndPicker);
}

// Instructs this picker to reset idling; idling will continue until the
// idle function returns false
void Picker_ResetIdle(HWND hWndPicker)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);

	Picker_ClearIdle(hWndPicker);

	if (pPickerInfo->pCallbacks->pfnOnIdle)
		pPickerInfo->nTimer = SetTimer(hWndPicker, 0, 0, Picker_TimerProc);
}

bool Picker_IsIdling(HWND hWndPicker)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);

	return pPickerInfo->nTimer != 0;
}

bool SetupPicker(HWND hWndPicker, const struct PickerOptions *pOptions)
{
	LONG_PTR l;

	assert(hWndPicker);
	// Allocate the list view struct
	struct PickerInfo *pPickerInfo = (struct PickerInfo *) malloc(sizeof(struct PickerInfo));

	if (!pPickerInfo)
		return false;

	// And fill it out
	memset(pPickerInfo, 0, sizeof(*pPickerInfo));
	pPickerInfo->pCallbacks = pOptions->pCallbacks;
	pPickerInfo->nColumnCount = pOptions->nColumnCount;
	pPickerInfo->ppszColumnNames = pOptions->ppszColumnNames;
	pPickerInfo->nLastItem = -1;

	if (pPickerInfo->nColumnCount)
	{
		// Allocate space for the column order and columns shown array
		pPickerInfo->pnColumnsOrder = (int*)malloc(pPickerInfo->nColumnCount * sizeof(*pPickerInfo->pnColumnsOrder));
		pPickerInfo->pnColumnsShown = (int*)malloc(pPickerInfo->nColumnCount * sizeof(*pPickerInfo->pnColumnsShown));

		if (!pPickerInfo->pnColumnsOrder || !pPickerInfo->pnColumnsShown)
			goto error;

		// set up initial values
		for (int i = 0; i < pPickerInfo->nColumnCount; i++)
		{
			pPickerInfo->pnColumnsOrder[i] = i;
			pPickerInfo->pnColumnsShown[i] = true;
		}

		if (pPickerInfo->pCallbacks->pfnGetColumnOrder)
			pPickerInfo->pCallbacks->pfnGetColumnOrder(pPickerInfo->pnColumnsOrder);

		if (pPickerInfo->pCallbacks->pfnGetColumnShown)
			pPickerInfo->pCallbacks->pfnGetColumnShown(pPickerInfo->pnColumnsShown);
	}

	// Hook in our wndproc and userdata pointer
	l = GetWindowLongPtr(hWndPicker, GWLP_WNDPROC);
	pPickerInfo->pfnParentWndProc = (WNDPROC) l;
	SetWindowLongPtr(hWndPicker, GWLP_USERDATA, (LONG_PTR) pPickerInfo);
	SetWindowLongPtr(hWndPicker, GWLP_WNDPROC, (LONG_PTR) ListViewWndProc);
	Picker_InternalResetColumnDisplay(hWndPicker, true);
	Picker_ResetIdle(hWndPicker);
	return true;

error:
	if (pPickerInfo)
		Picker_Free(pPickerInfo);

	return false;
}

int Picker_GetViewID(HWND hWndPicker)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);

	return pPickerInfo->nCurrentViewID;
}

void Picker_SetViewID(HWND hWndPicker, int nViewID)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);

	// Change the nCurrentViewID member
	pPickerInfo->nCurrentViewID = nViewID;

	if (pPickerInfo->pCallbacks->pfnSetViewMode)
		pPickerInfo->pCallbacks->pfnSetViewMode(pPickerInfo->nCurrentViewID);

	DWORD dwStyle = GetWindowLong(hWndPicker, GWL_STYLE);
	dwStyle &= ~LVS_TYPEMASK;
	dwStyle |= LVS_REPORT;
	SetWindowLong(hWndPicker, GWL_STYLE, dwStyle);
	RedrawWindow(hWndPicker, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME);
}

static bool PickerHitTest(HWND hWnd)
{
	RECT rect;
	DWORD res = GetMessagePos();
	LVHITTESTINFO htInfo;

	memset(&htInfo, 0, sizeof(LVHITTESTINFO));
	POINTS p = MAKEPOINTS(res);
	GetWindowRect(hWnd, &rect);
	htInfo.pt.x = p.x - rect.left;
	htInfo.pt.y = p.y - rect.top;
	(void)ListView_HitTest(hWnd, &htInfo);
	return (!(htInfo.flags & LVHT_NOWHERE));
}

int Picker_GetSelectedItem(HWND hWnd)
{
	LVITEM lvi;

	int nItem = ListView_GetNextItem(hWnd, -1, LVIS_SELECTED | LVIS_FOCUSED);

	if (nItem < 0)
		return 0;

	memset(&lvi, 0, sizeof(LVITEM));
	lvi.iItem = nItem;
	lvi.mask = LVIF_PARAM;
	(void)ListView_GetItem(hWnd, &lvi);
	return lvi.lParam;
}

void Picker_SetSelectedPick(HWND hWnd, int nIndex)
{
	if (nIndex < 0)
		nIndex = 0;

	ListView_SetItemState(hWnd, nIndex, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
	(void)ListView_EnsureVisible(hWnd, nIndex, false);
}

void Picker_SetSelectedItem(HWND hWnd, int nItem)
{
	LVFINDINFO lvfi;

	if (nItem < 0)
		return;

	lvfi.flags = LVFI_PARAM;
	lvfi.lParam = nItem;
	int i = ListView_FindItem(hWnd, -1, &lvfi);
	Picker_SetSelectedPick(hWnd, (i == -1) ? 0 : i);
}

static const TCHAR *Picker_CallGetItemString(HWND hWndPicker, int nItem, int nColumn, TCHAR *pszBuffer, UINT nBufferLength)
{
	// this call wraps the pfnGetItemString callback to properly set up the
	// buffers, and normalize the results
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);
	const TCHAR *s = NULL;

	pszBuffer[0] = '\0';

	if (pPickerInfo->pCallbacks->pfnGetItemString)
		s = pPickerInfo->pCallbacks->pfnGetItemString(hWndPicker, nItem, nColumn, pszBuffer, nBufferLength);

	if (!s)
		s = pszBuffer;

	return s;
}

// put the arrow on the new sort column
static void Picker_ResetHeaderSortIcon(HWND hWndPicker)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);
	HWND hwndHeader = ListView_GetHeader(hWndPicker);
	HDITEM hdi;

	// take arrow off non-current columns
	hdi.mask = HDI_FORMAT;
	hdi.fmt = HDF_STRING;

	for (int i = 0; i < pPickerInfo->nColumnCount; i++)
	{
		if (i != pPickerInfo->pCallbacks->pfnGetSortColumn())
			(void)Header_SetItem(hwndHeader, Picker_GetViewColumnFromRealColumn(hWndPicker, i), &hdi);
	}

	// use built in sort arrows
	hdi.mask = HDI_FORMAT;
	hdi.fmt = HDF_STRING | (pPickerInfo->pCallbacks->pfnGetSortReverse() ? HDF_SORTDOWN : HDF_SORTUP);
	int nViewColumn = Picker_GetViewColumnFromRealColumn(hWndPicker, pPickerInfo->pCallbacks->pfnGetSortColumn());
	(void)Header_SetItem(hwndHeader, nViewColumn, &hdi);
}

static void Picker_PopulateCompareProcParams(HWND hWndPicker, struct CompareProcParams *pParams)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);

	// populate the CompareProcParams structure
	memset(pParams, 0, sizeof(*pParams));
	pParams->hWndPicker = hWndPicker;
	pParams->pPickerInfo = pPickerInfo;
	pParams->nViewMode = pPickerInfo->pCallbacks->pfnGetViewMode();

	if (pPickerInfo->pCallbacks->pfnGetSortColumn)
		pParams->nSortColumn = pPickerInfo->pCallbacks->pfnGetSortColumn();

	if (pPickerInfo->pCallbacks->pfnGetSortReverse)
		pParams->bReverse = pPickerInfo->pCallbacks->pfnGetSortReverse();
}

static int CALLBACK Picker_CompareProc(LPARAM index1, LPARAM index2, LPARAM nParamSort)
{
	struct CompareProcParams *pcpp = (struct CompareProcParams *) nParamSort;
	struct PickerInfo *pPickerInfo = pcpp->pPickerInfo;
	bool bCallCompare = true;
	int nResult = 0; 
	TCHAR szBuffer1[256]; 
	TCHAR szBuffer2[256];

	if (GetEnableIndent())
	{
		int nParent1 = 0; 
		int nParent2 = 0;
		
		// do our fancy compare, with clones grouped under parents
		// first thing we need to do is identify both item's parents
		if (pPickerInfo->pCallbacks->pfnFindItemParent)
		{
			nParent1 = pPickerInfo->pCallbacks->pfnFindItemParent(pcpp->hWndPicker, index1);
			nParent2 = pPickerInfo->pCallbacks->pfnFindItemParent(pcpp->hWndPicker, index2);
		}
		else
		{
			nParent1 = nParent2 = -1;
		}

		if ((nParent1 < 0) && (nParent2 < 0))
		{
			// if both entries are both parents, we just do a basic sort
		}
		else if ((nParent1 >= 0) && (nParent2 >= 0))
		{
			// if both entries are children and the parents are different,
			// sort on their parents
			if (nParent1 != nParent2)
			{
				index1 = nParent1;
				index2 = nParent2;
			}
		}
		else
		{
			// one parent, one child
			if (nParent1 >= 0)
			{
				// first one is a child
				if (nParent1 == index2)
				{
					// if this is a child and its parent, put child after
					nResult = 1;
					bCallCompare = false;
				}
				else
				{
					// sort on parent
					index1 = nParent1;
				}
			}
			else
			{
				// second one is a child
				if (nParent2 == index1)
				{
					// if this is a child and its parent, put child after
					nResult = -1;
					bCallCompare = false;
				}
				else
				{
					// sort on parent
					index2 = nParent2;
				}
			}
		}
	}

	if (bCallCompare)
	{
		if (pPickerInfo->pCallbacks->pfnCompare)
		{
			nResult = pPickerInfo->pCallbacks->pfnCompare(pcpp->hWndPicker, index1, index2, pcpp->nSortColumn);
		}
		else
		{
			// no default sort proc, just get the string and compare them
			const TCHAR *s1 = Picker_CallGetItemString(pcpp->hWndPicker, index1, pcpp->nSortColumn, szBuffer1, WINUI_ARRAY_LENGTH(szBuffer1));
			const TCHAR *s2 = Picker_CallGetItemString(pcpp->hWndPicker, index2, pcpp->nSortColumn, szBuffer2, WINUI_ARRAY_LENGTH(szBuffer2));
			nResult = _tcsicmp(s1, s2);
		}

		if (pcpp->bReverse)
			nResult = -nResult;
	}
	
	return nResult;
}

void Picker_Sort(HWND hWndPicker)
{
	LVFINDINFO lvfi;
	struct CompareProcParams params;

	// populate the CompareProcParams structure
	Picker_PopulateCompareProcParams(hWndPicker, &params);
	(void)ListView_SortItems(hWndPicker, Picker_CompareProc, (LPARAM) &params);
	Picker_ResetHeaderSortIcon(hWndPicker);

	memset(&lvfi, 0, sizeof(LVFINDINFO));
	lvfi.flags = LVFI_PARAM;
	lvfi.lParam = Picker_GetSelectedItem(hWndPicker);
	int nItem = ListView_FindItem(hWndPicker, -1, &lvfi);
	(void)ListView_EnsureVisible(hWndPicker, nItem, false);
}

int Picker_InsertItemSorted(HWND hWndPicker, int nParam)
{
	int nLow = 0;
	struct CompareProcParams params;
	LVITEM lvi;

	int nHigh = ListView_GetItemCount(hWndPicker);
	// populate the CompareProcParams structure
	Picker_PopulateCompareProcParams(hWndPicker, &params);

	while(nLow < nHigh)
	{
		int nMid = (nHigh + nLow) / 2;

		memset(&lvi, 0, sizeof(LVITEM));
		lvi.mask = LVIF_PARAM;
		lvi.iItem = nMid;
		
		(void)ListView_GetItem(hWndPicker, &lvi);
		int nCompareResult = Picker_CompareProc(nParam, lvi.lParam, (LPARAM) &params);

		if (nCompareResult > 0)
			nLow  = nMid + 1;
		else if (nCompareResult < 0)
			nHigh = nMid;
		else
		{
			nLow = nMid;
			break;
		}
	}

	memset(&lvi, 0, sizeof(LVITEM));
	lvi.mask     = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
	lvi.iItem = nLow;
	lvi.iSubItem = 0;
	lvi.lParam = nParam;
	lvi.pszText  = LPSTR_TEXTCALLBACK;
	lvi.iImage = I_IMAGECALLBACK;

	return ListView_InsertItem(hWndPicker, &lvi);
}

int Picker_GetRealColumnFromViewColumn(HWND hWnd, int nViewColumn)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWnd);
	int nRealColumn = 0;

	if (nViewColumn >= 0 && nViewColumn < pPickerInfo->nColumnCount)
		nRealColumn = pPickerInfo->pnColumnsOrder[nViewColumn];

	return nRealColumn;
}

int Picker_GetViewColumnFromRealColumn(HWND hWnd, int nRealColumn)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWnd);

	for (int i = 0; i < pPickerInfo->nColumnCount; i++)
	{
		if (pPickerInfo->pnColumnsOrder[i] == nRealColumn)
			return i;
	}

	// major error, shouldn't be possible, but no good way to warn
	return 0;
}

bool Picker_HandleNotify(LPNMHDR lpNmHdr)
{
	HWND hWnd = lpNmHdr->hwndFrom;
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWnd);
	bool bResult = false;
	bool bReverse = false;

	NMLISTVIEW *pnmv = (NM_LISTVIEW *) lpNmHdr;

	switch(lpNmHdr->code)
	{
		case NM_RCLICK:
		case NM_CLICK:
		case NM_DBLCLK:
			// don't allow selection of blank spaces in the listview
			if (!PickerHitTest(hWnd))
			{
				// we have no current item selected
				if (pPickerInfo->nLastItem != -1)
					Picker_SetSelectedItem(hWnd, pPickerInfo->nLastItem);

				bResult = true;
			}
			else if ((lpNmHdr->code == NM_DBLCLK) && (pPickerInfo->pCallbacks->pfnDoubleClick))
			{
				// double click!
				pPickerInfo->pCallbacks->pfnDoubleClick();
				bResult = true;
			}
			
			break;

		case LVN_GETDISPINFO:
		{
			LV_DISPINFO *pDispInfo = (LV_DISPINFO *) lpNmHdr;
			int nItem = (int) pDispInfo->item.lParam;

			if (pDispInfo->item.mask & LVIF_IMAGE)
			{
				// retrieve item image
				if (pPickerInfo->pCallbacks->pfnGetItemImage)
					pDispInfo->item.iImage = pPickerInfo->pCallbacks->pfnGetItemImage(hWnd, nItem);
				else
					pDispInfo->item.iImage = 0;

				bResult = true;
			}

			if (pDispInfo->item.mask & LVIF_STATE)
			{
				pDispInfo->item.state = 0;
				bResult = true;
			}

			if (pDispInfo->item.mask & LVIF_TEXT)
			{
				// retrieve item text
				int nColumn = Picker_GetRealColumnFromViewColumn(hWnd, pDispInfo->item.iSubItem);
				const TCHAR *s = Picker_CallGetItemString(hWnd, nItem, nColumn, pDispInfo->item.pszText, pDispInfo->item.cchTextMax);
				pDispInfo->item.pszText = (TCHAR *) s;
				bResult = true;
			}

			break;
		}

		case LVN_ITEMCHANGED:
			if ((pnmv->uOldState & LVIS_SELECTED) && !(pnmv->uNewState & LVIS_SELECTED))
			{
				if (pnmv->lParam != -1)
					pPickerInfo->nLastItem = pnmv->lParam;

				if (pPickerInfo->pCallbacks->pfnLeavingItem)
					pPickerInfo->pCallbacks->pfnLeavingItem(hWnd, pnmv->lParam);
			}

			if (!(pnmv->uOldState & LVIS_SELECTED) && (pnmv->uNewState & LVIS_SELECTED))
			{
				if (pPickerInfo->pCallbacks->pfnEnteringItem)
					pPickerInfo->pCallbacks->pfnEnteringItem(hWnd, pnmv->lParam);
			}

			bResult = true;
			break;

		case LVN_COLUMNCLICK:
			// if clicked on the same column we're sorting by, reverse it
			if (pPickerInfo->pCallbacks->pfnGetSortColumn() == Picker_GetRealColumnFromViewColumn(hWnd, pnmv->iSubItem))
				bReverse = !pPickerInfo->pCallbacks->pfnGetSortReverse();
			else
				bReverse = false;

			pPickerInfo->pCallbacks->pfnSetSortReverse(bReverse);
			pPickerInfo->pCallbacks->pfnSetSortColumn(Picker_GetRealColumnFromViewColumn(hWnd, pnmv->iSubItem));
			Picker_Sort(hWnd);
			bResult = true;
			break;

		case LVN_BEGINDRAG:
			if (pPickerInfo->pCallbacks->pfnBeginListViewDrag)
				pPickerInfo->pCallbacks->pfnBeginListViewDrag(pnmv);

			break;
	}

	return bResult;
}

int Picker_GetNumColumns(HWND hWnd)
{
	int nColumnCount = 0;
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWnd);

	int *shown = (int*)malloc(pPickerInfo->nColumnCount * sizeof(*shown));

	if (!shown)
		return -1;

	pPickerInfo->pCallbacks->pfnGetColumnShown(shown);
	HWND hwndHeader = ListView_GetHeader(hWnd);

	if ((nColumnCount = Header_GetItemCount(hwndHeader)) < 1)
	{
		nColumnCount = 0;

		for (int i = 0; i < pPickerInfo->nColumnCount ; i++ )
		{
			if (shown[i])
				nColumnCount++;
		}
	}

	free(shown);
	return nColumnCount;
}

const struct PickerCallbacks *Picker_GetCallbacks(HWND hWndPicker)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);

	return pPickerInfo->pCallbacks;
}

int Picker_GetColumnCount(HWND hWndPicker)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);

	return pPickerInfo->nColumnCount;
}

const TCHAR* const *Picker_GetColumnNames(HWND hWndPicker)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);

	return pPickerInfo->ppszColumnNames;
}

bool Picker_SaveColumnWidths(HWND hWndPicker)
{
	struct PickerInfo *pPickerInfo = GetPickerInfo(hWndPicker);
	bool bSuccess = false;
	int nColumnMax = 0;

	/* allocate space for the column info */
	int *widths = (int*)malloc(pPickerInfo->nColumnCount * sizeof(*widths));
	int *order = (int*)malloc(pPickerInfo->nColumnCount * sizeof(*order));
	int *tmpOrder = (int*)malloc(pPickerInfo->nColumnCount * sizeof(*tmpOrder));

	if (!widths || !order || !tmpOrder)
		goto done;

	/* retrieve the values */
	memset(widths, 0, pPickerInfo->nColumnCount * sizeof(*widths));
	memset(order, 0, pPickerInfo->nColumnCount * sizeof(*order));
	pPickerInfo->pCallbacks->pfnGetColumnWidths(widths);
	pPickerInfo->pCallbacks->pfnGetColumnOrder(order);
	/* switch the list view to LVS_REPORT style so column widths reported correctly */
	SetWindowLong(hWndPicker, GWL_STYLE, (GetWindowLong(hWndPicker, GWL_STYLE) & ~LVS_TYPEMASK) | LVS_REPORT);
	nColumnMax = Picker_GetNumColumns(hWndPicker);
	/* Get the Column Order and save it */
	(void)ListView_GetColumnOrderArray(hWndPicker, nColumnMax, tmpOrder);

	for (int i = 0; i < nColumnMax; i++)
	{
		widths[Picker_GetRealColumnFromViewColumn(hWndPicker, i)] = ListView_GetColumnWidth(hWndPicker, i);
		order[i] = Picker_GetRealColumnFromViewColumn(hWndPicker, tmpOrder[i]);
	}

	pPickerInfo->pCallbacks->pfnSetColumnWidths(widths);
	pPickerInfo->pCallbacks->pfnSetColumnOrder(order);
	bSuccess = true;

done:
	if (widths)
		free(widths);

	if (order)
		free(order);

	if (tmpOrder)
		free(tmpOrder);

	return bSuccess;
}
