// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include "winui.h"

/***************************************************************************
    Internal structures
 ***************************************************************************/
struct DriversInfo
{
	int screenCount;
	bool isClone;
	bool isBroken;
	bool isHarddisk;
	bool hasOptionalBIOS;
	bool isVector;
	bool usesRoms;
	bool usesSamples;
	bool usesTrackball;
	bool usesLightGun;
	bool supportsSaveState;
	bool isVertical;
	bool isImperfect;
	bool isMechanical;
	bool isBIOS;
};

static std::vector<DriversInfo>	drivers_info;

enum
{
	DRIVER_CACHE_SCREEN     = 0x000F,
	DRIVER_CACHE_ROMS       = 0x0010,
	DRIVER_CACHE_CLONE      = 0x0020,
	DRIVER_CACHE_BIOS       = 0x0040,
	DRIVER_CACHE_HARDDISK   = 0x0080,
	DRIVER_CACHE_SAMPLES    = 0x0100,
	DRIVER_CACHE_VECTOR     = 0x0200,
	DRIVER_CACHE_LIGHTGUN   = 0x0400,
	DRIVER_CACHE_TRACKBALL  = 0x0800,
};

string_iterator::string_iterator()
{
}

string_iterator::string_iterator(const char *str)
{
	/* reset the structure */
	m_index = 0;
	m_str.clear();
	m_base = (str != NULL) ? str : "";
	m_cur = m_base;
}
 
bool string_iterator::next(int separator)
{
	const char *semi;

	/* if none left, return false to indicate we are done */
	if (m_index != 0 && *m_cur == 0)
		return false;

	/* ignore duplicates of the separator */
	while (m_index == 0 && *m_cur == separator)
		m_cur++;

	if (*m_cur == 0)
		return false;

	/* copy up to the next separator */
	semi = strchr(m_cur, separator);

	if (semi == NULL)
		semi = m_cur + strlen(m_cur);

	m_str.assign(m_cur, semi - m_cur);
	m_cur = (*semi == 0) ? semi : semi + 1;

	/* ignore duplicates of the separator */
	while (*m_cur && *m_cur == separator)
		m_cur++;

	/* bump the index and return true */
	m_index++;
	return true;
}

void ErrorMessageBox(const char *fmt, ...)
{
	char buf[1024];
	va_list ptr;

	va_start(ptr, fmt);
	vsnprintf(buf, WINUI_ARRAY_LENGTH(buf), fmt, ptr);
	win_message_box_utf8(GetMainWindow(), buf, MAMEUINAME, MB_ICONERROR | MB_OK);
	va_end(ptr);
}

/* for debugging */
void dprintf(const char *fmt, ...)
{
	char buf[1024];
	va_list ptr;
	va_start(ptr, fmt);
	vsnprintf(buf, WINUI_ARRAY_LENGTH(buf), fmt, ptr);
	win_output_debug_string_utf8(buf);
	va_end(ptr);
}

void ShellExecuteCommon(HWND hWnd, const char *cName)
{
	const char *msg = NULL;
	TCHAR *tName = win_wstring_from_utf8(cName);

	if(!tName)
		return;

	HINSTANCE hErr = ShellExecute(hWnd, NULL, tName, NULL, NULL, SW_SHOWNORMAL);

	if ((FPTR)hErr > 32)
	{
		free(tName);
		return;
	}

	switch((FPTR)hErr)
	{
	case 0:
		msg = "The Operating System is out of memory or resources.";
		break;

	case ERROR_FILE_NOT_FOUND:
		msg = "The specified file was not found.";
		break;

	case SE_ERR_NOASSOC :
		msg = "There is no application associated with the given filename extension.";
		break;

	case SE_ERR_OOM :
		msg = "There was not enough memory to complete the operation.";
		break;

	case SE_ERR_PNF :
		msg = "The specified path was not found.";
		break;

	case SE_ERR_SHARE :
		msg = "A sharing violation occurred.";
		break;

	default:
		msg = "Unknown error.";
	}

	ErrorMessageBox("%s\r\nPath: '%s'", msg, cName);
	free(tName);
}

char * MyStrStrI(const char* pFirst, const char* pSrch)
{
	char *cp = (char*)pFirst;

	while (*cp)
	{
		char *s1 = cp;
		char *s2 = (char*)pSrch;

		while (*s1 && *s2 && !core_strnicmp(s1, s2, 1))
			s1++, s2++;

		if (!*s2)
			return cp;

		cp++;
	}

	return NULL;
}

char * ConvertToWindowsNewlines(const char *source)
{
	static char buf[2048 * 2048];

	memset(&buf, 0, sizeof(buf));
	char *dest = buf;

	while (*source != 0)
	{
		if (*source == '\n')
		{
			*dest++ = '\r';
			*dest++ = '\n';
		}
		else
			*dest++ = *source;

		source++;
	}

	*dest = 0;
	return buf;
}

const char * GetVersionString(void)
{
	return MAMEUIFX_VERSION;
}

const char * GetDriverGameTitle(int nIndex)
{
	return driver_list::driver(nIndex).description;
}

const char * GetDriverGameName(int nIndex)
{
	return driver_list::driver(nIndex).name;
}

const char * GetDriverGameManufacturer(int nIndex)
{
	return driver_list::driver(nIndex).manufacturer;
}

const char * GetDriverGameYear(int nIndex)
{
	return driver_list::driver(nIndex).year;
}

const char * GetDriverFileName(int nIndex)
{
	static char tmp[40];

	std::string driver = core_filename_extract_base(driver_list::driver(nIndex).source_file, false);
	strcpy(tmp, driver.c_str());
	return tmp;
}

int GetGameNameIndex(const char *name)
{
	return driver_list::find(name);
}

static int NumberOfScreens(const machine_config &config)
{
	screen_device_iterator scriter(config.root_device());
	return scriter.count();
}

static bool isDriverVector(const machine_config &config)
{
	const screen_device *screen  = config.first_screen();

	if (screen != nullptr) 
	{
		if (SCREEN_TYPE_VECTOR == screen->screen_type())
			return true;
	}

	return false;
}

static void SetDriversInfo(void)
{
	for (int ndriver = 0; ndriver < driver_list::total(); ndriver++)
	{
		struct DriversInfo *gameinfo = &drivers_info[ndriver];
		int cache = (gameinfo->screenCount & DRIVER_CACHE_SCREEN);

		if (gameinfo->isClone)
			cache += DRIVER_CACHE_CLONE;

		if (gameinfo->isHarddisk)
			cache += DRIVER_CACHE_HARDDISK;

		if (gameinfo->hasOptionalBIOS)
			cache += DRIVER_CACHE_BIOS;

		if (gameinfo->isVector)
			cache += DRIVER_CACHE_VECTOR;

		if (gameinfo->usesRoms)
			cache += DRIVER_CACHE_ROMS;

		if (gameinfo->usesSamples)
			cache += DRIVER_CACHE_SAMPLES;

		if (gameinfo->usesTrackball)
			cache += DRIVER_CACHE_TRACKBALL;

		if (gameinfo->usesLightGun)
			cache += DRIVER_CACHE_LIGHTGUN;

		SetDriverCache(ndriver, cache);
	}
}

static void InitDriversInfo(void)
{
	for (int ndriver = 0; ndriver < driver_list::total(); ndriver++)
	{
		const game_driver *gamedrv = &driver_list::driver(ndriver);
		struct DriversInfo *gameinfo = &drivers_info[ndriver];
		machine_config config(*gamedrv, MameUIGlobal());
		samples_device_iterator sampiter(config.root_device());
		
		gameinfo->isClone = (GetParentRomSetIndex(gamedrv) != -1);
		gameinfo->isBroken = (gamedrv->flags & (MACHINE_NOT_WORKING | MACHINE_UNEMULATED_PROTECTION | MACHINE_MECHANICAL)) ? true : false;
		gameinfo->isImperfect = (gamedrv->flags & (MACHINE_WRONG_COLORS | MACHINE_IMPERFECT_COLORS | MACHINE_IMPERFECT_GRAPHICS
			| MACHINE_NO_SOUND | MACHINE_IMPERFECT_SOUND | MACHINE_IS_INCOMPLETE | MACHINE_NO_SOUND_HW)) ? true : false;
 		gameinfo->supportsSaveState = (gamedrv->flags & MACHINE_SUPPORTS_SAVE) ? true : false;
		gameinfo->isVertical = (gamedrv->flags & ORIENTATION_SWAP_XY) ? true : false;
		gameinfo->isMechanical = (gamedrv->flags & MACHINE_MECHANICAL) ? true : false;
		gameinfo->isBIOS = (gamedrv->flags & MACHINE_IS_BIOS_ROOT) ? true : false;
		gameinfo->screenCount = NumberOfScreens(config);
		gameinfo->isVector = isDriverVector(config);
		gameinfo->isHarddisk = false;
		gameinfo->usesRoms = false;
		gameinfo->hasOptionalBIOS = false;
		gameinfo->usesSamples = false;
		gameinfo->usesTrackball = false;
		gameinfo->usesLightGun = false;

		for (device_t &device : device_iterator(config.root_device()))
		{
			for (const rom_entry *region = rom_first_region(device); region != nullptr; region = rom_next_region(region))
			{
				for (const rom_entry *rom = rom_first_file(region); rom != nullptr; rom = rom_next_file(rom))
				{
					if (ROMREGION_ISDISKDATA(region))
						gameinfo->isHarddisk = true;

					gameinfo->usesRoms = true;
				}
			}
		}

		if (gamedrv->rom != nullptr)
		{
			auto rom_entries = rom_build_entries(gamedrv->rom);

			for (const rom_entry *rom = rom_entries.data(); !ROMENTRY_ISEND(rom); rom++)
			{
				if (ROMENTRY_ISSYSTEM_BIOS(rom))
					gameinfo->hasOptionalBIOS = true;
			}
		}
		
		if (sampiter.first() != nullptr)
			gameinfo->usesSamples = true;

		if (gamedrv->ipt != nullptr)
		{
			ioport_list portlist;
			std::string errors;

			for (device_t &cfg : device_iterator(config.root_device()))
			{
				if (cfg.input_ports())
					portlist.append(cfg, errors);
			}

			for (auto &port : portlist)
			{
				for (ioport_field &field : port.second->fields())
				{
					UINT32 type = field.type();

					if (type == IPT_END)
						break;

					if (type == IPT_DIAL || type == IPT_PADDLE || type == IPT_TRACKBALL_X || type == IPT_TRACKBALL_Y)
						gameinfo->usesTrackball = true;

					if (type == IPT_LIGHTGUN_X || type == IPT_LIGHTGUN_Y || type == IPT_AD_STICK_X || type == IPT_AD_STICK_Y)
						gameinfo->usesLightGun = true;
				}
			}
		}
	}

	SetDriversInfo();
}

static void InitDriversCache(void)
{
	SetRequiredDriverCacheStatus();

	if (RequiredDriverCache())
	{
		InitDriversInfo();
		return;
	}

	for (int ndriver = 0; ndriver < driver_list::total(); ndriver++)
	{
		const game_driver *gamedrv = &driver_list::driver(ndriver);
		struct DriversInfo *gameinfo = &drivers_info[ndriver];
		int cache = GetDriverCache(ndriver);

		if (cache == -1)
		{
			InitDriversInfo();
			break;
		}

		gameinfo->isBroken  = (gamedrv->flags & (MACHINE_NOT_WORKING | MACHINE_UNEMULATED_PROTECTION | MACHINE_MECHANICAL)) ? true : false;
		gameinfo->supportsSaveState = (gamedrv->flags & MACHINE_SUPPORTS_SAVE) ? true : false;
		gameinfo->isVertical = (gamedrv->flags & ORIENTATION_SWAP_XY) ? true : false;
		gameinfo->screenCount = (cache & DRIVER_CACHE_SCREEN);
		gameinfo->isClone = (cache & DRIVER_CACHE_CLONE) ? true : false;
		gameinfo->isHarddisk = (cache & DRIVER_CACHE_HARDDISK) ? true : false;
		gameinfo->hasOptionalBIOS = (cache & DRIVER_CACHE_BIOS) ? true : false;
		gameinfo->isVector = (cache & DRIVER_CACHE_VECTOR) ? true : false;
		gameinfo->usesRoms = (cache & DRIVER_CACHE_ROMS) ? true : false;
		gameinfo->usesSamples = (cache & DRIVER_CACHE_SAMPLES) ? true : false;
		gameinfo->usesTrackball = (cache & DRIVER_CACHE_TRACKBALL) ? true : false;
		gameinfo->usesLightGun = (cache & DRIVER_CACHE_LIGHTGUN) ? true : false;
		gameinfo->isImperfect = (gamedrv->flags & (MACHINE_WRONG_COLORS | MACHINE_IMPERFECT_COLORS | MACHINE_IMPERFECT_GRAPHICS	
			| MACHINE_NO_SOUND | MACHINE_IMPERFECT_SOUND | MACHINE_IS_INCOMPLETE | MACHINE_NO_SOUND_HW)) ? true : false;
		gameinfo->isMechanical = (gamedrv->flags & MACHINE_MECHANICAL) ? true : false;
		gameinfo->isBIOS = (gamedrv->flags & MACHINE_IS_BIOS_ROOT) ? true : false;
	}
}

static struct DriversInfo* GetDriversInfo(int driver_index)
{
	static bool bFirst = true;

	if (bFirst)
	{
		bFirst = false;
		drivers_info.clear();
		drivers_info.reserve(driver_list::total());
		InitDriversCache();
	}

	return &drivers_info[driver_index];
}

bool DriverIsClone(int driver_index)
{
	return GetDriversInfo(driver_index)->isClone;
}

bool DriverIsBroken(int driver_index)
{
	return GetDriversInfo(driver_index)->isBroken;
}

bool DriverIsHarddisk(int driver_index)
{
	return GetDriversInfo(driver_index)->isHarddisk;
}

bool DriverIsBios(int driver_index)
{
	return GetDriversInfo(driver_index)->isBIOS;
}

bool DriverIsMechanical(int driver_index)
{
	return GetDriversInfo(driver_index)->isMechanical;
}

bool DriverHasOptionalBIOS(int driver_index)
{
	return GetDriversInfo(driver_index)->hasOptionalBIOS;
}

int DriverNumScreens(int driver_index)
{
	return GetDriversInfo(driver_index)->screenCount;
}

bool DriverIsVector(int driver_index)
{
	return GetDriversInfo(driver_index)->isVector;
}

bool DriverUsesRoms(int driver_index)
{
	return GetDriversInfo(driver_index)->usesRoms;
}

bool DriverUsesSamples(int driver_index)
{
	return GetDriversInfo(driver_index)->usesSamples;
}

bool DriverUsesTrackball(int driver_index)
{
	return GetDriversInfo(driver_index)->usesTrackball;
}

bool DriverUsesLightGun(int driver_index)
{
	return GetDriversInfo(driver_index)->usesLightGun;
}

bool DriverSupportsSaveState(int driver_index)
{
	return GetDriversInfo(driver_index)->supportsSaveState;
}

bool DriverIsVertical(int driver_index)
{
	return GetDriversInfo(driver_index)->isVertical;
}

bool DriverIsImperfect(int driver_index)
{
	return GetDriversInfo(driver_index)->isImperfect;
}

//============================================================
//  win_wstring_from_utf8
//============================================================

WCHAR *win_wstring_from_utf8(const char *utf8string)
{
	// convert MAME string (UTF-8) to UTF-16
	int char_count = MultiByteToWideChar(CP_UTF8, 0, utf8string, -1, nullptr, 0);
	WCHAR *result = (WCHAR *)osd_malloc_array(char_count * sizeof(*result));

	if (result != nullptr)
		MultiByteToWideChar(CP_UTF8, 0, utf8string, -1, result, char_count);

	return result;
}


//============================================================
//  win_utf8_from_wstring
//============================================================

char *win_utf8_from_wstring(const WCHAR *wstring)
{
	// convert UTF-16 to MAME string (UTF-8)
	int char_count = WideCharToMultiByte(CP_UTF8, 0, wstring, -1, nullptr, 0, nullptr, nullptr);
	char *result = (char *)osd_malloc_array(char_count * sizeof(*result));

	if (result != nullptr)
		WideCharToMultiByte(CP_UTF8, 0, wstring, -1, result, char_count, nullptr, nullptr);

	return result;
}

//============================================================
//  win_extract_icon_utf8
//============================================================

HICON win_extract_icon_utf8(HINSTANCE inst, const char* exefilename, UINT iconindex)
{
	TCHAR *t_exefilename = win_wstring_from_utf8(exefilename);

	if(!t_exefilename)
		return NULL;

	HICON icon = ExtractIcon(inst, t_exefilename, iconindex);
	free(t_exefilename);
	return icon;
}

//============================================================
//  win_find_first_file_utf8
//============================================================

HANDLE win_find_first_file_utf8(const char* filename, WIN32_FIND_DATA *findfiledata)
{
	TCHAR *t_filename = win_wstring_from_utf8(filename);

	if(!t_filename)
		return NULL;

	HANDLE result = FindFirstFile(t_filename, findfiledata);
	free(t_filename);
	return result;
}

//============================================================
//  win_move_file_utf8
//============================================================

BOOL win_move_file_utf8(const char* existingfilename, const char* newfilename)
{
	BOOL result = FALSE;

	TCHAR *t_existingfilename = win_wstring_from_utf8(existingfilename);

	if( !t_existingfilename )
		return result;

	TCHAR *t_newfilename = win_wstring_from_utf8(newfilename);

	if( !t_newfilename ) 
	{
		free(t_existingfilename);
		return result;
	}

	result = MoveFile(t_existingfilename, t_newfilename);
	osd_free(t_newfilename);
	osd_free(t_existingfilename);
	return result;
}

void CenterWindow(HWND hWnd)
{
	RECT rcCenter, rcWnd;
	HWND hWndParent = GetParent(hWnd);

	GetWindowRect(hWnd, &rcWnd);
	int iWndWidth  = rcWnd.right - rcWnd.left;
	int iWndHeight = rcWnd.bottom - rcWnd.top;

	if (hWndParent != NULL)
	{
		GetWindowRect(hWndParent, &rcCenter);
	}
	else
	{
		rcCenter.left = 0;
		rcCenter.top = 0;
		rcCenter.right = GetSystemMetrics(SM_CXFULLSCREEN);
		rcCenter.bottom = GetSystemMetrics(SM_CYFULLSCREEN);
	}

	int iScrWidth  = rcCenter.right - rcCenter.left;
	int iScrHeight = rcCenter.bottom - rcCenter.top;
	int xLeft = rcCenter.left;
	int yTop = rcCenter.top;

	if (iScrWidth > iWndWidth)
		xLeft += ((iScrWidth - iWndWidth) / 2);

	if (iScrHeight > iWndHeight)
		yTop += ((iScrHeight - iWndHeight) / 2);

	// map screen coordinates to child coordinates
	SetWindowPos(hWnd, HWND_TOP, xLeft, yTop, -1, -1, SWP_NOSIZE);
}

bool IsWindowsSevenOrHigher(void) 
{
	OSVERSIONINFO osvi;

	memset(&osvi, 0, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	GetVersionEx(&osvi);

	if ((osvi.dwMajorVersion >= 6) && (osvi.dwMinorVersion >= 1))
		return true;

	return false;
}
