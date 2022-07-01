// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#pragma once
 
#ifndef TABVIEW_H
#define TABVIEW_H

struct TabViewCallbacks
{
	// Options retrieval
	bool (*pfnGetShowTabCtrl)(void);
	void (*pfnSetCurrentTab)(const char *pszShortName);
	const char* (*pfnGetCurrentTab)(void);
	void (*pfnSetShowTab)(int nTab, bool show);
	int (*pfnGetShowTab)(int nTab);
	// Accessors
	const char* (*pfnGetTabShortName)(int nTab);
	const char* (*pfnGetTabLongName)(int nTab);
	// Callbacks
	void (*pfnOnSelectionChanged)(void);
	void (*pfnOnMoveSize)(void);
};

struct TabViewOptions
{
	const struct TabViewCallbacks *pCallbacks;
	int nTabCount;
};

bool SetupTabView(HWND hWndTabView, const struct TabViewOptions *pOptions);
void TabView_Reset(HWND hWndTabView);
void TabView_CalculateNextTab(HWND hWndTabView);
int TabView_GetCurrentTab(HWND hWndTabView);
void TabView_SetCurrentTab(HWND hWndTabView, int nTab);
void TabView_UpdateSelection(HWND hWndTabView);
// These are used to handle events received by the parent regarding tabview controls
bool TabView_HandleNotify(LPNMHDR lpNmHdr);

#endif
