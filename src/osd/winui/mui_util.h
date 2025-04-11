// For licensing and usage information, read docs/release/winui_license.txt
//****************************************************************************

#ifndef WINUI_MUI_UTIL_H
#define WINUI_MUI_UTIL_H

#include "emucore.h"

extern void __cdecl ErrorMsg(const char* fmt, ...);
extern void __cdecl dprintf(const char* fmt, ...);


extern UINT GetDepth(HWND hWnd);

/* Open a text file */
extern void DisplayTextFile(HWND hWnd, const char *cName);

#define PACKVERSION(major,minor) MAKELONG(minor,major)

/* Check for old version of comctl32.dll */
extern LONG GetCommonControlVersion();

void ShellExecuteCommon(HWND hWnd, const char *cName);
extern char * MyStrStrI(const char* pFirst, const char* pSrch);
extern char * ConvertToWindowsNewlines(const char *source);

extern const char * GetDriverFilename(int);

BOOL DriverIsClone(int);
BOOL DriverIsBroken(int);
BOOL DriverIsHarddisk(int);
BOOL DriverHasOptionalBIOS(int);
BOOL DriverIsStereo(int);
BOOL DriverIsVector(int);
int DriverNumScreens(int);
BOOL DriverIsBios(int);
BOOL DriverUsesRoms(int);
BOOL DriverUsesSamples(int);
BOOL DriverUsesTrackball(int);
BOOL DriverUsesLightGun(int);
BOOL DriverUsesMouse(int);
BOOL DriverSupportsSaveState(int);
BOOL DriverIsVertical(int);
BOOL DriverIsMechanical(int);
BOOL DriverIsArcade(int);
BOOL DriverHasRam(int);

int isDriverVector(const machine_config *config);
int numberOfSpeakers(const machine_config *config);
int numberOfScreens(const machine_config *config);

void FlushFileCaches();

BOOL StringIsSuffixedBy(const char *s, const char *suffix);

BOOL SafeIsAppThemed();

// provides result of FormatMessage()
// resulting buffer must be free'd with LocalFree()
void GetSystemErrorMessage(DWORD dwErrorId, TCHAR **tErrorMessage);

HICON win_extract_icon_utf8(HINSTANCE inst, const char* exefilename, UINT iconindex);
TCHAR* win_tstring_strdup(LPCTSTR str);
HANDLE win_create_file_utf8(const char* filename, DWORD desiredmode, DWORD sharemode,
							LPSECURITY_ATTRIBUTES securityattributes, DWORD creationdisposition,
							DWORD flagsandattributes, HANDLE templatehandle);
DWORD win_get_current_directory_utf8(size_t bufferlength, char* buffer);
HANDLE win_find_first_file_utf8(const char* filename, LPWIN32_FIND_DATA findfiledata);
void ErrorMessageBox(const char *fmt, ...);

#endif /* MUI_UTIL_H */

