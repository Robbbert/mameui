// For licensing and usage information, read docs/winui_license.txt
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
extern LONG GetCommonControlVersion(void);

void ShellExecuteCommon(HWND hWnd, const char *cName);
extern char * MyStrStrI(const char* pFirst, const char* pSrch);
extern char * ConvertToWindowsNewlines(const char *source);

extern const char * GetDriverFilename(uint32_t nIndex);

BOOL DriverIsClone(uint32_t driver_index);
BOOL DriverIsBroken(uint32_t driver_index);
BOOL DriverIsHarddisk(uint32_t driver_index);
BOOL DriverHasOptionalBIOS(uint32_t driver_index);
BOOL DriverIsStereo(uint32_t driver_index);
BOOL DriverIsVector(uint32_t driver_index);
int DriverNumScreens(uint32_t driver_index);
BOOL DriverIsBios(uint32_t driver_index);
BOOL DriverUsesRoms(uint32_t driver_index);
BOOL DriverUsesSamples(uint32_t driver_index);
BOOL DriverUsesTrackball(uint32_t driver_index);
BOOL DriverUsesLightGun(uint32_t driver_index);
BOOL DriverUsesMouse(uint32_t driver_index);
BOOL DriverSupportsSaveState(uint32_t driver_index);
BOOL DriverIsVertical(uint32_t driver_index);
BOOL DriverIsMechanical(uint32_t driver_index);
BOOL DriverIsArcade(uint32_t driver_index);
BOOL DriverHasRam(uint32_t driver_index);

int isDriverVector(const machine_config *config);
int numberOfSpeakers(const machine_config *config);
int numberOfScreens(const machine_config *config);

void FlushFileCaches(void);

BOOL StringIsSuffixedBy(const char *s, const char *suffix);

BOOL SafeIsAppThemed(void);

// provides result of FormatMessage()
// resulting buffer must be free'd with LocalFree()
void GetSystemErrorMessage(DWORD dwErrorId, TCHAR **tErrorMessage);

HICON win_extract_icon_utf8(HINSTANCE inst, const char* exefilename, UINT iconindex);
TCHAR* win_tstring_strdup(LPCTSTR str);
HANDLE win_create_file_utf8(const char* filename, DWORD desiredmode, DWORD sharemode,
							LPSECURITY_ATTRIBUTES securityattributes, DWORD creationdisposition,
							DWORD flagsandattributes, HANDLE templatehandle);
DWORD win_get_current_directory_utf8(DWORD bufferlength, char* buffer);
HANDLE win_find_first_file_utf8(const char* filename, LPWIN32_FIND_DATA findfiledata);

#endif /* MUI_UTIL_H */

