// For licensing and usage information, read docs/release/winui_license.txt
// MASTER
//****************************************************************************

/***************************************************************************

  mui_util.cpp

 ***************************************************************************/

// standard windows headers
#include <windows.h>
#include <shellapi.h>

// standard C headers
#include <tchar.h>

// MAME/MAMEUI headers
#include "emu.h"
#include "screen.h"
#include "speaker.h"
#include "unzip.h"
#include "sound/samples.h"
#include "winutf8.h"
#include "winui.h"
#include "mui_util.h"
#include "mui_opts.h"
#include "emu_opts.h"
#include "drivenum.h"
#include "machine/ram.h"
#include <shlwapi.h>
#include "corestr.h"
#include "path.h"

/***************************************************************************
    function prototypes
 ***************************************************************************/

/***************************************************************************
    External variables
 ***************************************************************************/

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
	bool isStereo;
	bool isVector;
	bool usesRoms;
	bool usesSamples;
	bool usesTrackball;
	bool usesLightGun;
	bool usesMouse;
	bool supportsSaveState;
	bool isVertical;
	bool hasRam;
};

static std::vector<DriversInfo> drivers_info;
static bool bFirst = true;


enum
{
	DRIVER_CACHE_SCREEN		= 0x000F,
	DRIVER_CACHE_ROMS		= 0x0010,
	DRIVER_CACHE_CLONE		= 0x0020,
	DRIVER_CACHE_STEREO		= 0x0040,
	DRIVER_CACHE_BIOS		= 0x0080,
	DRIVER_CACHE_TRACKBALL	= 0x0100,
	DRIVER_CACHE_HARDDISK	= 0x0200,
	DRIVER_CACHE_SAMPLES	= 0x0400,
	DRIVER_CACHE_LIGHTGUN	= 0x0800,
	DRIVER_CACHE_VECTOR		= 0x1000,
	DRIVER_CACHE_MOUSE		= 0x2000,
	DRIVER_CACHE_RAM        = 0x4000,
};

/***************************************************************************
    External functions
 ***************************************************************************/

/*
    ErrorMsg
*/
void __cdecl ErrorMsg(const char* fmt, ...)
{
	static FILE* pFile = NULL;
	DWORD dwWritten;
	char buf[5000];
	char buf2[5000];
	va_list va;

	va_start(va, fmt);

	vsprintf(buf, fmt, va);

	win_message_box_utf8(GetActiveWindow(), buf, MAMEUINAME, MB_OK | MB_ICONERROR);

	strcpy(buf2, MAMEUINAME ": ");
	strcat(buf2,buf);
	strcat(buf2, "\n");

	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf2, strlen(buf2), &dwWritten, NULL);

	if (pFile == NULL)
		pFile = fopen("debug.txt", "wt");

	if (pFile != NULL)
	{
		fprintf(pFile, "%s", buf2);
		fflush(pFile);
	}

	va_end(va);
}

void __cdecl dprintf(const char* fmt, ...)
{
	char buf[5000];
	va_list va;

	va_start(va, fmt);

	_vsnprintf(buf,sizeof(buf),fmt,va);

	win_output_debug_string_utf8(buf);

	va_end(va);
}

//============================================================
//  winui_message_box_utf8
//============================================================

int winui_message_box_utf8(HWND hWnd, const char *text, const char *caption, UINT type)
{
	int result = IDCANCEL;
	wchar_t *t_text = ui_wstring_from_utf8(text);
	wchar_t *t_caption = ui_wstring_from_utf8(caption);

	if (!t_text)
		return result;

	if (!t_caption)
	{
		free(t_text);
		return result;
	}

	result = MessageBox(hWnd, t_text, t_caption, type);
	free(t_text);
	free(t_caption);
	return result;
}

void ErrorMessageBox(const char *fmt, ...)
{
	char buf[1024];
	va_list ptr;

	va_start(ptr, fmt);
	vsnprintf(buf, std::size(buf), fmt, ptr);
	winui_message_box_utf8(GetMainWindow(), buf, MAMEUINAME, MB_ICONERROR | MB_OK);
	va_end(ptr);
}

void ShellExecuteCommon(HWND hWnd, const char *cName)
{
	wchar_t *tName = ui_wstring_from_utf8(cName);

	if(!tName)
		return;

	HINSTANCE hErr = ShellExecute(hWnd, NULL, tName, NULL, NULL, SW_SHOWNORMAL);

	if ((uintptr_t)hErr > 32)
	{
		free(tName);
		return;
	}

	const char *msg = NULL;
	switch((uintptr_t)hErr)
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

UINT GetDepth(HWND hWnd)
{
	UINT nBPP;
	HDC hDC;

	hDC = GetDC(hWnd);

	nBPP = GetDeviceCaps(hDC, BITSPIXEL) * GetDeviceCaps(hDC, PLANES);

	ReleaseDC(hWnd, hDC);

	return nBPP;
}

/*
 * Return true if comctl32.dll is version 4.71 or greater
 * otherwise return false.
 */
LONG GetCommonControlVersion()
{
	HMODULE hModule = GetModuleHandle(TEXT("comctl32"));

	if (hModule)
	{
		FARPROC lpfnICCE = GetProcAddress(hModule, "InitCommonControlsEx");

		if (NULL != lpfnICCE)
		{
			FARPROC lpfnDLLI = GetProcAddress(hModule, "DllInstall");

			if (NULL != lpfnDLLI)
			{
				/* comctl 4.71 or greater */

				// see if we can find out exactly

				DLLGETVERSIONPROC pDllGetVersion;
				pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hModule, "DllGetVersion");

				/* Because some DLLs might not implement this function, you
                   must test for it explicitly. Depending on the particular
                   DLL, the lack of a DllGetVersion function can be a useful
                   indicator of the version. */

				if(pDllGetVersion)
				{
					DLLVERSIONINFO dvi;
					HRESULT hr;

					ZeroMemory(&dvi, sizeof(dvi));
					dvi.cbSize = sizeof(dvi);

					hr = (*pDllGetVersion)(&dvi);

					if (SUCCEEDED(hr))
					{
						return PACKVERSION(dvi.dwMajorVersion, dvi.dwMinorVersion);
					}
				}
				return PACKVERSION(4,71);
			}
			return PACKVERSION(4,7);
		}
		return PACKVERSION(4,0);
	}
	/* DLL not found */
	return PACKVERSION(0,0);
}

void DisplayTextFile(HWND hWnd, const char *cName)
{
	LPTSTR tName = ui_wstring_from_utf8(cName);
	if( !tName )
		return;

	HINSTANCE hErr = ShellExecute(hWnd, NULL, tName, NULL, NULL, SW_SHOWNORMAL);
	if ((uintptr_t)hErr > 32)
	{
		free(tName);
		return;
	}

	LPCTSTR msg = 0;
	switch((uintptr_t)hErr)
	{
	case 0:
		msg = TEXT("The operating system is out of memory or resources.");
		break;

	case ERROR_FILE_NOT_FOUND:
		msg = TEXT("The specified file was not found.");
		break;

	case SE_ERR_NOASSOC :
		msg = TEXT("There is no application associated with the given filename extension.");
		break;

	case SE_ERR_OOM :
		msg = TEXT("There was not enough memory to complete the operation.");
		break;

	case SE_ERR_PNF :
		msg = TEXT("The specified path was not found.");
		break;

	case SE_ERR_SHARE :
		msg = TEXT("A sharing violation occurred.");
		break;

	default:
		msg = TEXT("Unknown error.");
	}

	MessageBox(NULL, msg, tName, MB_OK);

	free(tName);
}

char* MyStrStrI(const char* pFirst, const char* pSrch)
{
	char* cp = (char*)pFirst;
	char* s1;
	char* s2;

	while (*cp)
	{
		s1 = cp;
		s2 = (char*)pSrch;

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
	char *dest;

	dest = buf;
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

/* Lop off path and extention from a source file name
 * This assumes there is a pathname passed to the function
 * like src\drivers\blah.c
 */
const char * GetDriverFilename(int drvindex)
{
	static char tmp[2048] = { };
	if (drvindex >= 0)
	{
		string driver = string(core_filename_extract_base(driver_list::driver(drvindex).type.source()));
		strcpy(tmp, driver.c_str());
	}
	return tmp;
}

BOOL isDriverVector(const machine_config *config)
{
	const screen_device *screen = screen_device_enumerator(config->root_device()).first();

	if (screen)
		if (SCREEN_TYPE_VECTOR == screen->screen_type())
			return true;

	return false;
}

int numberOfScreens(const machine_config *config)
{
	screen_device_enumerator scriter(config->root_device());
	return scriter.count();
}

int numberOfSpeakers(const machine_config *config)
{
	speaker_device_enumerator iter(config->root_device());
	return iter.count();
}

static void SetDriversInfo()
{
	uint32_t cache;
	uint32_t total = driver_list::total();
	struct DriversInfo *gameinfo = NULL;

	for (uint32_t ndriver = 0; ndriver < total; ndriver++)
	{
		gameinfo = &drivers_info[ndriver];
		cache = gameinfo->screenCount & DRIVER_CACHE_SCREEN;

		if (gameinfo->isClone)
			cache += DRIVER_CACHE_CLONE;

		if (gameinfo->isHarddisk)
			cache += DRIVER_CACHE_HARDDISK;

		if (gameinfo->hasOptionalBIOS)
			cache += DRIVER_CACHE_BIOS;

		if (gameinfo->isStereo)
			cache += DRIVER_CACHE_STEREO;

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

		if (gameinfo->usesMouse)
			cache += DRIVER_CACHE_MOUSE;

		if (gameinfo->hasRam)
			cache += DRIVER_CACHE_RAM;

		SetDriverCache(ndriver, cache);
	}
}

static void InitDriversInfo()
{
	printf("InitDriversInfo: A\n");fflush(stdout);
	int num_speakers;
	uint32_t total = driver_list::total();
	const game_driver *gamedrv = NULL;
	struct DriversInfo *gameinfo = NULL;
	const rom_entry *region, *rom;

	for (uint32_t ndriver = 0; ndriver < total; ndriver++)
	{
		uint32_t cache = GetDriverCacheLower(ndriver);
		gamedrv = &driver_list::driver(ndriver);
		gameinfo = &drivers_info[ndriver];
		machine_config config(*gamedrv, MameUIGlobal());

		bool const have_parent(strcmp(gamedrv->parent, "0"));
		auto const parent_idx(have_parent ? driver_list::find(gamedrv->parent) : -1);
		gameinfo->isClone = ( !have_parent || (0 > parent_idx) || BIT(GetDriverCacheLower(parent_idx),9)) ? false : true;
		gameinfo->isBroken = (cache & 0x4040) ? true : false;  // (MACHINE_NOT_WORKING | MACHINE_MECHANICAL)
		gameinfo->supportsSaveState = BIT(cache, 7) ^ 1;  //MACHINE_SUPPORTS_SAVE
		gameinfo->isHarddisk = false;
		gameinfo->isVertical = BIT(cache, 2);  //ORIENTATION_SWAP_XY

		ram_device_enumerator iter1(config.root_device());
		gameinfo->hasRam = (iter1.first() );

		for (device_t &device : device_enumerator(config.root_device()))
			for (region = rom_first_region(device); region; region = rom_next_region(region))
				if (ROMREGION_ISDISKDATA(region))
					gameinfo->isHarddisk = true;

		gameinfo->hasOptionalBIOS = false;
		if (gamedrv->rom)
		{
			auto rom_entries = rom_build_entries(gamedrv->rom);
			for (rom = rom_entries.data(); !ROMENTRY_ISEND(rom); rom++)
				if (ROMENTRY_ISSYSTEM_BIOS(rom))
					gameinfo->hasOptionalBIOS = true;
		}

		num_speakers = numberOfSpeakers(&config);

		gameinfo->isStereo = (num_speakers > 1);
		gameinfo->screenCount = numberOfScreens(&config);
		gameinfo->isVector = isDriverVector(&config); // ((drv.video_attributes & VIDEO_TYPE_VECTOR) != 0);
		gameinfo->usesRoms = false;
		for (device_t &device : device_enumerator(config.root_device()))
			for (region = rom_first_region(device); region; region = rom_next_region(region))
				for (rom = rom_first_file(region); rom; rom = rom_next_file(rom))
					gameinfo->usesRoms = true;

		samples_device_enumerator iter(config.root_device());
		gameinfo->usesSamples = iter.count() ? true : false;

		gameinfo->usesTrackball = false;
		gameinfo->usesLightGun = false;
		gameinfo->usesMouse = false;

		if (gamedrv->ipt)
		{
			ioport_list portlist;
			std::ostringstream errors;
			for (device_t &cfg : device_enumerator(config.root_device()))
				if (cfg.input_ports())
					portlist.append(cfg, errors);

			for (auto &port : portlist)
			{
				for (ioport_field &field : port.second->fields())
				{
					UINT32 type;
					type = field.type();
					if (type == IPT_END)
						break;
					if (type == IPT_DIAL || type == IPT_PADDLE || type == IPT_TRACKBALL_X || type == IPT_TRACKBALL_Y || type == IPT_AD_STICK_X || type == IPT_AD_STICK_Y)
						gameinfo->usesTrackball = true;
					if (type == IPT_LIGHTGUN_X || type == IPT_LIGHTGUN_Y)
						gameinfo->usesLightGun = true;
					if (type == IPT_MOUSE_X || type == IPT_MOUSE_Y)
						gameinfo->usesMouse = true;
				}
			}
		}
	}

	SetDriversInfo();
	printf("InitDriversInfo: Finished\n");fflush(stdout);
}

static int InitDriversCache()
{
	printf("InitDriversCache: A\n");fflush(stdout);
	if (RequiredDriverCache())
	{
		printf("InitDriversCache: B\n");fflush(stdout);
		InitDriversInfo();
		return 0;
	}

	printf("InitDriversCache: C\n");fflush(stdout);
	uint32_t cache_lower, cache_upper;
	uint32_t total = driver_list::total();
	struct DriversInfo *gameinfo = NULL;

	printf("InitDriversCache: D\n");fflush(stdout);
	for (uint32_t ndriver = 0; ndriver < total; ndriver++)
	{
		gameinfo = &drivers_info[ndriver];
		cache_lower = GetDriverCacheLower(ndriver);
		cache_upper = GetDriverCacheUpper(ndriver);

		gameinfo->isBroken          =  (cache_lower & 0x4040) ? true : false; //MACHINE_NOT_WORKING | MACHINE_MECHANICAL
		gameinfo->supportsSaveState =  BIT(cache_lower, 7) ? false : true;  //MACHINE_SUPPORTS_SAVE
		gameinfo->isVertical        =  BIT(cache_lower, 2) ? true : false;  //ORIENTATION_XY
		gameinfo->screenCount       =   cache_upper & DRIVER_CACHE_SCREEN;
		gameinfo->isClone           = ((cache_upper & DRIVER_CACHE_CLONE)     != 0);
		gameinfo->isHarddisk        = ((cache_upper & DRIVER_CACHE_HARDDISK)  != 0);
		gameinfo->hasOptionalBIOS   = ((cache_upper & DRIVER_CACHE_BIOS)      != 0);
		gameinfo->isStereo          = ((cache_upper & DRIVER_CACHE_STEREO)    != 0);
		gameinfo->isVector          = ((cache_upper & DRIVER_CACHE_VECTOR)    != 0);
		gameinfo->usesRoms          = ((cache_upper & DRIVER_CACHE_ROMS)      != 0);
		gameinfo->usesSamples       = ((cache_upper & DRIVER_CACHE_SAMPLES)   != 0);
		gameinfo->usesTrackball     = ((cache_upper & DRIVER_CACHE_TRACKBALL) != 0);
		gameinfo->usesLightGun      = ((cache_upper & DRIVER_CACHE_LIGHTGUN)  != 0);
		gameinfo->usesMouse         = ((cache_upper & DRIVER_CACHE_MOUSE)     != 0);
		gameinfo->hasRam            = ((cache_upper & DRIVER_CACHE_RAM)       != 0);
	}

	printf("InitDriversCache: Finished\n");fflush(stdout);
	return 0;
}

static struct DriversInfo* GetDriversInfo(int drvindex)
{
	if (bFirst)
	{
		bFirst = false;

		drivers_info.clear();
		drivers_info.resize(driver_list::total());
		std::fill(drivers_info.begin(), drivers_info.end(), DriversInfo{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
		printf("DriversInfo: B\n");fflush(stdout);
		InitDriversCache();
	}

	return &drivers_info[drvindex];
}

BOOL DriverIsClone(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->isClone;
}

BOOL DriverIsBroken(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->isBroken;
}

BOOL DriverIsHarddisk(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->isHarddisk;
}

BOOL DriverIsBios(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return BIT(GetDriverCacheLower(drvindex), 9);
}

BOOL DriverIsMechanical(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return BIT(GetDriverCacheLower(drvindex), 14);
}

BOOL DriverIsArcade(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return ((GetDriverCacheLower(drvindex) & 3) == 0) ? true: false;  //TYPE_ARCADE
}

BOOL DriverHasOptionalBIOS(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->hasOptionalBIOS;
}

BOOL DriverIsStereo(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->isStereo;
}

int DriverNumScreens(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->screenCount;
}

BOOL DriverIsVector(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->isVector;
}

BOOL DriverUsesRoms(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->usesRoms;
}

BOOL DriverUsesSamples(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->usesSamples;
}

BOOL DriverUsesTrackball(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->usesTrackball;
}

BOOL DriverUsesLightGun(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->usesLightGun;
}

BOOL DriverUsesMouse(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->usesMouse;
}

BOOL DriverSupportsSaveState(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->supportsSaveState;
}

BOOL DriverIsVertical(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->isVertical;
}

BOOL DriverHasRam(int drvindex)
{
	if (drvindex < 0)
		return 0;
	else
		return GetDriversInfo(drvindex)->hasRam;
}

void FlushFileCaches()
{
	util::archive_file::cache_clear();
}

BOOL StringIsSuffixedBy(const char *s, const char *suffix)
{
	return (strlen(s) > strlen(suffix)) && (strcmp(s + strlen(s) - strlen(suffix), suffix) == 0);
}

/***************************************************************************
    Win32 wrappers
 ***************************************************************************/

BOOL SafeIsAppThemed()
{
	BOOL bResult = false;
	BOOL (WINAPI *pfnIsAppThemed)(void);

	HMODULE hThemes = LoadLibrary(TEXT("uxtheme.dll"));
	if (hThemes)
	{
		pfnIsAppThemed = (BOOL (WINAPI *)(void)) GetProcAddress(hThemes, "IsAppThemed");
		if (pfnIsAppThemed)
			bResult = pfnIsAppThemed();
		FreeLibrary(hThemes);
	}
	return bResult;

}


void GetSystemErrorMessage(DWORD dwErrorId, TCHAR **tErrorMessage)
{
	if( FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErrorId, 0, (LPTSTR)tErrorMessage, 0, NULL) == 0 )
	{
		*tErrorMessage = (LPTSTR)LocalAlloc(LPTR, MAX_PATH * sizeof(TCHAR));
		_tcscpy(*tErrorMessage, TEXT("Unknown Error"));
	}
}


//============================================================
//  win_extract_icon_utf8
//============================================================

HICON win_extract_icon_utf8(HINSTANCE inst, const char* exefilename, UINT iconindex)
{
	HICON icon = 0;
	TCHAR* t_exefilename = ui_wstring_from_utf8(exefilename);
	if( !t_exefilename )
		return icon;

	icon = ExtractIcon(inst, t_exefilename, iconindex);

	free(t_exefilename);

	return icon;
}



//============================================================
//  win_tstring_strdup
//============================================================

TCHAR* win_tstring_strdup(LPCTSTR str)
{
	TCHAR *cpy = NULL;
	if (str)
	{
		cpy = (TCHAR*)malloc((_tcslen(str) + 1) * sizeof(TCHAR));
		if (cpy)
			_tcscpy(cpy, str);
	}
	return cpy;
}

//============================================================
//  win_create_file_utf8
//============================================================

HANDLE win_create_file_utf8(const char* filename, DWORD desiredmode, DWORD sharemode, LPSECURITY_ATTRIBUTES securityattributes,
							DWORD creationdisposition, DWORD flagsandattributes, HANDLE templatehandle)
{
	HANDLE result = 0;
	TCHAR* t_filename = ui_wstring_from_utf8(filename);
	if( !t_filename )
		return result;

	result = CreateFile(t_filename, desiredmode, sharemode, securityattributes, creationdisposition, flagsandattributes, templatehandle);

	free(t_filename);

	return result;
}

//=================================================================
//  win_get_current_directory_utf8
//  return value: 0 for failure, otherwise size of returned string
//=================================================================

DWORD win_get_current_directory_utf8(size_t bufferlength, char* buffer)
{
	if (!bufferlength)
		return 0;

	TCHAR* t_buffer = NULL;
	t_buffer = (TCHAR*)malloc((bufferlength * sizeof(TCHAR)) + 1);
	if( !t_buffer )
		return 0;

	DWORD result = GetCurrentDirectory(bufferlength, t_buffer);

	if (result == 0)
	{
		printf("ERROR: win_get_current_directory_utf8: GetCurrentDirectory failed\n");
		free (t_buffer);
		return 0;
	}

	if (result > bufferlength)
	{
		printf("ERROR: win_get_current_directory_utf8: Need buffer size of %d\n",int(result));
		free (t_buffer);
		return 0;
	}

	char* utf8_buffer = NULL;
	utf8_buffer = ui_utf8_from_wstring(t_buffer);

	free(t_buffer);

	if( !utf8_buffer )
		return 0;

	strncpy(buffer, utf8_buffer, bufferlength);

	if( utf8_buffer )
		free(utf8_buffer);

	return result;
}

//============================================================
//  win_find_first_file_utf8
//============================================================

HANDLE win_find_first_file_utf8(const char* filename, LPWIN32_FIND_DATA findfiledata)
{
	HANDLE result = 0;
	TCHAR* t_filename = ui_wstring_from_utf8(filename);
	if( !t_filename )
		return result;

	result = FindFirstFile(t_filename, findfiledata);

	free(t_filename);

	return result;
}

