// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#pragma once
 
#ifndef PICKER_H
#define PICKER_H

struct PickerCallbacks
{
	// Options retrieval
	void (*pfnSetSortColumn)(int column);
	int (*pfnGetSortColumn)(void);
	void (*pfnSetSortReverse)(bool reverse);
	bool (*pfnGetSortReverse)(void);
	void (*pfnSetViewMode)(int val);
	int (*pfnGetViewMode)(void);
	void (*pfnSetColumnWidths)(int widths[]);
	void (*pfnGetColumnWidths)(int widths[]);
	void (*pfnSetColumnOrder)(int order[]);
	void (*pfnGetColumnOrder)(int order[]);
	void (*pfnSetColumnShown)(int shown[]);
	void (*pfnGetColumnShown)(int shown[]);
	int (*pfnCompare)(HWND hWndPicker, int nIndex1, int nIndex2, int nSortSubItem);
	void (*pfnDoubleClick)(void);
	const TCHAR *(*pfnGetItemString)(HWND hWndPicker, int nItem, int nColumn, TCHAR *pszBuffer, UINT nBufferLength);
	int (*pfnGetItemImage)(HWND hWndPicker, int nItem);
	void (*pfnLeavingItem)(HWND hWndPicker, int nItem);
	void (*pfnEnteringItem)(HWND hWndPicker, int nItem);
	void (*pfnBeginListViewDrag)(NM_LISTVIEW *pnlv);
	int (*pfnFindItemParent)(HWND hWndPicker, int nItem);
	bool (*pfnCheckNotWorkingItem)(HWND hWndPicker, int nItem);
	bool (*pfnOnIdle)(HWND hWndPicker);
	void (*pfnOnHeaderContextMenu)(POINT pt, int nColumn);
	void (*pfnOnBodyContextMenu)(POINT pt);
};

struct PickerOptions
{
	const struct PickerCallbacks *pCallbacks;
	int nColumnCount;
	const TCHAR* const *ppszColumnNames;
};

enum
{
	VIEW_ICONS_LARGE = 0,
	VIEW_ICONS_SMALL,
	VIEW_MAX
};

bool SetupPicker(HWND hWndPicker, const struct PickerOptions *pOptions);
int Picker_GetViewID(HWND hWndPicker);
void Picker_SetViewID(HWND hWndPicker, int nViewID);
int Picker_GetRealColumnFromViewColumn(HWND hWndPicker, int nViewColumn);
int Picker_GetViewColumnFromRealColumn(HWND hWndPicker, int nRealColumn);
void Picker_Sort(HWND hWndPicker);
void Picker_ResetColumnDisplay(HWND hWndPicker);
int Picker_GetSelectedItem(HWND hWndPicker);
void Picker_SetSelectedItem(HWND hWndPicker, int nItem);
void Picker_SetSelectedPick(HWND hWndPicker, int nIndex);
int Picker_GetNumColumns(HWND hWnd);
void Picker_ClearIdle(HWND hWndPicker);
void Picker_ResetIdle(HWND hWndPicker);
bool Picker_IsIdling(HWND hWndPicker);
int Picker_InsertItemSorted(HWND hWndPicker, int nParam);
bool Picker_SaveColumnWidths(HWND hWndPicker);
// These are used to handle events received by the parent regarding
// picker controls
bool Picker_HandleNotify(LPNMHDR lpNmHdr);
// Accessors
const struct PickerCallbacks *Picker_GetCallbacks(HWND hWndPicker);
int Picker_GetColumnCount(HWND hWndPicker);
const TCHAR* const *Picker_GetColumnNames(HWND hWndPicker);

#endif
