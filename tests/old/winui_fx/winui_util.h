// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#pragma once 

#ifndef MUI_UTIL_H
#define MUI_UTIL_H

// string iterator helper class
class string_iterator
{
public:
	// simple construction/destruction
	string_iterator();
	string_iterator(const char *str);

	// character searching helpers
	bool next(int separator);

	// C string conversion operators and helpers
	operator const char *() const { return m_str.c_str(); }
	const char *c_str() const { return m_str.c_str(); }

private:
	std::string  m_str;
	const char * m_base;
	const char * m_cur;
	int          m_index;
};

void ErrorMessageBox(const char *fmt, ...);
void dprintf(const char *fmt, ...);
void ShellExecuteCommon(HWND hWnd, const char *cName);
char * MyStrStrI(const char *pFirst, const char *pSrch);
char * ConvertToWindowsNewlines(const char *source);
const char * GetVersionString(void);
const char * GetDriverGameTitle(int nIndex);
const char * GetDriverGameName(int nIndex);
const char * GetDriverGameManufacturer(int nIndex);
const char * GetDriverGameYear(int nIndex);
const char * GetDriverFileName(int nIndex);
int GetGameNameIndex(const char *name);
int DriverNumScreens(int driver_index);
bool DriverIsClone(int driver_index);
bool DriverIsBroken(int driver_index);
bool DriverIsHarddisk(int driver_index);
bool DriverHasOptionalBIOS(int driver_index);
bool DriverIsVector(int driver_index);
bool DriverIsBios(int driver_index);
bool DriverUsesRoms(int driver_index);
bool DriverUsesSamples(int driver_index);
bool DriverUsesLightGun(int driver_index);
bool DriverUsesTrackball(int driver_index);
bool DriverSupportsSaveState(int driver_index);
bool DriverIsVertical(int driver_index);
bool DriverIsMechanical(int driver_index);
bool DriverIsImperfect(int driver_index);
WCHAR *win_wstring_from_utf8(const char *utf8string);
char *win_utf8_from_wstring(const WCHAR *wstring);
HICON win_extract_icon_utf8(HINSTANCE inst, const char* exefilename, UINT iconindex);
HANDLE win_find_first_file_utf8(const char* filename, WIN32_FIND_DATA *findfiledata);
BOOL win_move_file_utf8(const char* existingfilename, const char* newfilename);
void CenterWindow(HWND hWnd);
bool IsWindowsSevenOrHigher(void);

#endif
