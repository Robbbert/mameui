// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include "winui.h"

struct TabViewInfo
{
	const struct TabViewCallbacks *pCallbacks;
	int nTabCount;
	WNDPROC pfnParentWndProc;
};

static struct TabViewInfo *GetTabViewInfo(HWND hWnd)
{
	LONG_PTR l = GetWindowLongPtr(hWnd, GWLP_USERDATA);
	return (struct TabViewInfo *) l;
}

static LRESULT CallParentWndProc(WNDPROC pfnParentWndProc, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (!pfnParentWndProc)
		pfnParentWndProc = GetTabViewInfo(hWnd)->pfnParentWndProc;

	LRESULT	rc = CallWindowProc(pfnParentWndProc, hWnd, message, wParam, lParam);

	return rc;
}

static LRESULT CALLBACK TabViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	struct TabViewInfo *pTabViewInfo = GetTabViewInfo(hWnd);
	WNDPROC pfnParentWndProc = pTabViewInfo->pfnParentWndProc;
	bool bHandled = false;
	LRESULT rc = 0;

	switch(message)
	{
		case WM_DESTROY:
			free(pTabViewInfo);
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR) pfnParentWndProc);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			break;
	}

	if (!bHandled)
		rc = CallParentWndProc(pfnParentWndProc, hWnd, message, wParam, lParam);

	switch(message)
	{
		case WM_MOVE:
		case WM_SIZE:
			if (pTabViewInfo->pCallbacks->pfnOnMoveSize)
				pTabViewInfo->pCallbacks->pfnOnMoveSize();
			break;
	}

	return rc;
}

static int TabView_GetTabFromTabIndex(HWND hWndTabView, int tab_index)
{
	int shown_tabs = -1;
	struct TabViewInfo *pTabViewInfo = GetTabViewInfo(hWndTabView);

	for (int i = 0; i < pTabViewInfo->nTabCount; i++)
	{
		if (!pTabViewInfo->pCallbacks->pfnGetShowTab || pTabViewInfo->pCallbacks->pfnGetShowTab(i))
		{
			shown_tabs++;

			if (shown_tabs == tab_index)
				return i;
		}
	}

	return 0;
}

int TabView_GetCurrentTab(HWND hWndTabView)
{
	struct TabViewInfo *pTabViewInfo = GetTabViewInfo(hWndTabView);
	const char *pszTab = NULL;
	int nTab = -1;

	if (pTabViewInfo->pCallbacks->pfnGetCurrentTab)
		pszTab = pTabViewInfo->pCallbacks->pfnGetCurrentTab();

	if (pszTab)
	{
		if (pTabViewInfo->pCallbacks->pfnGetTabShortName)
		{
			for (int i = 0; i < pTabViewInfo->nTabCount; i++)
			{
				const char *pszThatTab = pTabViewInfo->pCallbacks->pfnGetTabShortName(i);

				if (pszThatTab && !core_stricmp(pszTab, pszThatTab))
				{
					nTab = i;
					break;
				}
			}
		}

		if (nTab < 0)
		{
			nTab = 0;
			sscanf(pszTab, "%d", &nTab);
		}
	}
	else
		nTab = 0;

	return nTab;
}

void TabView_SetCurrentTab(HWND hWndTabView, int nTab)
{
	struct TabViewInfo *pTabViewInfo = GetTabViewInfo(hWndTabView);
	const char *pszName = NULL;
	char szBuffer[16];

	if (pTabViewInfo->pCallbacks->pfnGetTabShortName)
		pszName = pTabViewInfo->pCallbacks->pfnGetTabShortName(nTab);
	else
	{
		snprintf(szBuffer, WINUI_ARRAY_LENGTH(szBuffer), "%d", nTab);
		pszName = szBuffer;
	}

	if (pTabViewInfo->pCallbacks->pfnSetCurrentTab)
		pTabViewInfo->pCallbacks->pfnSetCurrentTab(pszName);
}

static int TabView_GetCurrentTabIndex(HWND hWndTabView)
{
	int shown_tabs = 0;
	int nCurrentTab = TabView_GetCurrentTab(hWndTabView);
	struct TabViewInfo *pTabViewInfo = GetTabViewInfo(hWndTabView);

	for (int i = 0; i < pTabViewInfo->nTabCount; i++)
	{
		if (i == nCurrentTab)
			break;

		if (!pTabViewInfo->pCallbacks->pfnGetShowTab || pTabViewInfo->pCallbacks->pfnGetShowTab(i))
			shown_tabs++;
	}

	return shown_tabs;
}

void TabView_UpdateSelection(HWND hWndTabView)
{
	(void)TabCtrl_SetCurSel(hWndTabView, TabView_GetCurrentTabIndex(hWndTabView));
}

bool TabView_HandleNotify(LPNMHDR lpNmHdr)
{
	HWND hWndTabView = lpNmHdr->hwndFrom;
	struct TabViewInfo *pTabViewInfo = GetTabViewInfo(hWndTabView);
	bool bResult = false;

	switch (lpNmHdr->code)
	{
		case TCN_SELCHANGE:
			int nTabIndex = TabCtrl_GetCurSel(hWndTabView);
			int nTab = TabView_GetTabFromTabIndex(hWndTabView, nTabIndex);
			TabView_SetCurrentTab(hWndTabView, nTab);

			if (pTabViewInfo->pCallbacks->pfnOnSelectionChanged)
				pTabViewInfo->pCallbacks->pfnOnSelectionChanged();

			bResult = true;
			break;
	}

	return bResult;
}

void TabView_CalculateNextTab(HWND hWndTabView)
{
	struct TabViewInfo *pTabViewInfo = GetTabViewInfo(hWndTabView);

	// at most loop once through all options
	for (int i = 0; i < pTabViewInfo->nTabCount; i++)
	{
		int nCurrentTab = TabView_GetCurrentTab(hWndTabView);
		TabView_SetCurrentTab(hWndTabView, (nCurrentTab + 1) % pTabViewInfo->nTabCount);
		nCurrentTab = TabView_GetCurrentTab(hWndTabView);

		if (!pTabViewInfo->pCallbacks->pfnGetShowTab || pTabViewInfo->pCallbacks->pfnGetShowTab(nCurrentTab))
			// this tab is being shown, so we're all set
			return;
	}
}

void TabView_Reset(HWND hWndTabView)
{
	struct TabViewInfo *pTabViewInfo = GetTabViewInfo(hWndTabView);
	TCITEM tci;

	(void)TabCtrl_DeleteAllItems(hWndTabView);

	memset(&tci, 0, sizeof(TCITEM));
	tci.mask = TCIF_TEXT;
	tci.cchTextMax = 20;

	for (int i = 0; i < pTabViewInfo->nTabCount; i++)
	{
		if (!pTabViewInfo->pCallbacks->pfnGetShowTab || pTabViewInfo->pCallbacks->pfnGetShowTab(i))
		{
			TCHAR *t_text = win_wstring_from_utf8(pTabViewInfo->pCallbacks->pfnGetTabLongName(i));

			if(!t_text)
				return;

			tci.pszText = t_text;
			(void)TabCtrl_InsertItem(hWndTabView, i, &tci);
			free(t_text);
		}
	}

	TabView_UpdateSelection(hWndTabView);
}

bool SetupTabView(HWND hWndTabView, const struct TabViewOptions *pOptions)
{
	LONG_PTR l;

	assert(hWndTabView);
	// Allocate the list view struct
	struct TabViewInfo *pTabViewInfo = (struct TabViewInfo *) malloc(sizeof(struct TabViewInfo));

	if (!pTabViewInfo)
		return false;

	// And fill it out
	memset(pTabViewInfo, 0, sizeof(*pTabViewInfo));
	pTabViewInfo->pCallbacks = pOptions->pCallbacks;
	pTabViewInfo->nTabCount = pOptions->nTabCount;
	// Hook in our wndproc and userdata pointer
	l = GetWindowLongPtr(hWndTabView, GWLP_WNDPROC);
	pTabViewInfo->pfnParentWndProc = (WNDPROC) l;
	SetWindowLongPtr(hWndTabView, GWLP_USERDATA, (LONG_PTR) pTabViewInfo);
	SetWindowLongPtr(hWndTabView, GWLP_WNDPROC, (LONG_PTR) TabViewWndProc);
	bool bShowTabView = pTabViewInfo->pCallbacks->pfnGetShowTabCtrl ? pTabViewInfo->pCallbacks->pfnGetShowTabCtrl() : true;
	ShowWindow(hWndTabView, bShowTabView ? SW_SHOW : SW_HIDE);
	TabView_Reset(hWndTabView);

	if (pTabViewInfo->pCallbacks->pfnOnSelectionChanged)
		pTabViewInfo->pCallbacks->pfnOnSelectionChanged();

	return true;
}
