// For licensing and usage information, read docs/release/winui_license.txt
//****************************************************************************
#ifndef WINUI_MESSUI_H
#define WINUI_MESSUI_H

extern string g_szSelectedItem;

void InitMessPicker();
void MessUpdateSoftwareList();
BOOL MyFillSoftwareList(int, BOOL);
BOOL MessCommand(HWND hwnd,int, HWND, UINT);
void MessReadMountedSoftware(int);
void SoftwareTabView_OnSelectionChanged();
BOOL CreateMessIcons();
void MySoftwareListClose();
void MView_RegisterClass();
void MView_Refresh(HWND);

#endif // __MESSUI_H__

