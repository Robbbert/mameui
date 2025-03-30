// For licensing and usage information, read docs/release/winui_license.txt
//****************************************************************************
//============================================================
//
//  softwarepicker.h - MESS's software picker
//
//============================================================

#include "swconfig.h"

LPCSTR SoftwarePicker_LookupFilename(HWND, int);
const device_image_interface *SoftwarePicker_LookupDevice(HWND, int);
int SoftwarePicker_LookupIndex(HWND, LPCSTR);
string SoftwarePicker_GetImageType(HWND, int);
BOOL SoftwarePicker_AddFile(HWND, LPCSTR, bool);
BOOL SoftwarePicker_AddDirectory(HWND, LPCSTR);
void SoftwarePicker_Clear(HWND);
void SoftwarePicker_SetDriver(HWND, const software_config *config);

// PickerOptions callbacks
LPCTSTR SoftwarePicker_GetItemString(HWND, int, int, TCHAR *pszBuffer, UINT);
BOOL SoftwarePicker_Idle(HWND);

BOOL SetupSoftwarePicker(HWND, const struct PickerOptions *pOptions);
bool uses_file_extension(device_image_interface &dev, const char *file_extension);

