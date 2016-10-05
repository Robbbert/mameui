// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#pragma once
 
#ifndef DIALOGS_H
#define DIALOGS_H

#define FILTERTEXT_LEN  256
#define NUM_EXCLUSIONS  10
#define NUMHISTORYTAB   WINUI_ARRAY_LENGTH(g_ComboBoxHistoryTab)

const char * GetFilterText(void);
INT_PTR CALLBACK ResetDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK InterfaceDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK FilterDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AddCustomFileDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);

#endif
