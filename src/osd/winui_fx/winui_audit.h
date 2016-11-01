// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#pragma once 

#ifndef MUI_AUDIT_H
#define MUI_AUDIT_H

void AuditDialog(void);
void AuditRefresh(void);
INT_PTR CALLBACK GameAuditDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AuditWindowProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
int MameUIVerifyRomSet(int game, bool refresh);
bool IsAuditResultYes(int audit_result);
bool IsAuditResultNo(int audit_result);

#endif
