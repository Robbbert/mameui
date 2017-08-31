// For licensing and usage information, read docs/winui_license.txt
//****************************************************************************
#ifndef WINUI_MESSUI_H__
#define WINUI_MESSUI_H__

extern char g_szSelectedItem[MAX_PATH];

void InitMessPicker(void);
void MessUpdateSoftwareList(void);
BOOL MyFillSoftwareList(int nGame, BOOL bForce);
BOOL MessCommand(HWND hwnd,int id, HWND hwndCtl, UINT codeNotify);
void MessReadMountedSoftware(int nGame);
void SoftwareTabView_OnSelectionChanged(void);
BOOL CreateMessIcons(void);
void MySoftwareListClose(void);
void MView_RegisterClass(void);
void MView_Refresh(HWND hwndDevView);

#endif // __MESSUI_H__
