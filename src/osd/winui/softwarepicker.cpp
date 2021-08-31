// For licensing and usage information, read docs/winui_license.txt
//****************************************************************************
//============================================================
//
//  softwarepicker.c - MESS's software picker
//
//  Note: other parts of this code
//  optionsms.c, optionsms.h,
//  messui.c at static const LPCTSTR mess_column_names[] =
//  messui.c - LOTS of stuff about software
//
//
//============================================================

#include <assert.h>
#include <string.h>
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wingdi.h>
#include <winuser.h>
#include <tchar.h>
#include <ctype.h>

#include "emu.h"
#include "mui_opts.h"
#include "unzip.h"
#include "strconv.h"
#include "picker.h"
#include "screenshot.h"
#include "bitmask.h"
#include "winui.h"
#include "mui_util.h"
#include "hash.h"
#include "softwarepicker.h"
#include "drivenum.h"
#include "corestr.h"


//============================================================
//  TYPE DEFINITIONS
//============================================================

typedef struct _file_info file_info;
struct _file_info
{
	const device_image_interface *device;

	//hash information
	//hash_collection hashes;
	BOOL hash_realized;
	//const hash_info *hashinfo;

	const char *zip_entry_name;
	const char *base_name;
	char file_name[1024];
};

typedef struct _directory_search_info directory_search_info;
struct _directory_search_info
{
	directory_search_info *next;
	HANDLE find_handle;
	WIN32_FIND_DATA fd;
	char directory_name[1024];
};

typedef struct _software_picker_info software_picker_info;
struct _software_picker_info
{
	WNDPROC old_window_proc;
	file_info **file_index;
	int file_index_length;
	int hashes_realized;
	int current_position;
	directory_search_info *first_search_info;
	directory_search_info *last_search_info;
	const software_config *config;
};



//============================================================
//  CONSTANTS
//============================================================

static const TCHAR software_picker_property_name[] = TEXT("SWPICKER");



//============================================================

static LPCSTR NormalizePath(LPCSTR pszPath, LPSTR pszBuffer, size_t nBufferSize)
{
	std::fill_n(pszBuffer,nBufferSize,0);
	nBufferSize--;
	BOOL bChanged = FALSE;
	LPSTR s;
	int i, j;

	if (pszPath[0] == '\\')
	{
		win_get_current_directory_utf8(nBufferSize, pszBuffer);
		pszBuffer[2] = '\0';
		bChanged = TRUE;
	}
	else
	if (!isalpha(pszPath[0]) || (pszPath[1] != ':'))
	{
		win_get_current_directory_utf8(nBufferSize, pszBuffer);
		bChanged = TRUE;
	}

	if (bChanged)
	{
		s = (LPSTR) alloca(strlen(pszBuffer) + 1);
		strcpy(s, pszBuffer);
		snprintf(pszBuffer, nBufferSize, "%s\\%s", s, pszPath);
		pszPath = pszBuffer;

		// Remove double path separators
		i = 0;
		j = 0;
		while(pszBuffer[i])
		{
			while ((pszBuffer[i] == '\\') && (pszBuffer[i+1] == '\\'))
				i++;
			pszBuffer[j++] = pszBuffer[i++];
		}
		pszBuffer[j] = '\0';
	}
	return pszPath;
}



static software_picker_info *GetSoftwarePickerInfo(HWND hwndPicker)
{
	HANDLE h;
	h = GetProp(hwndPicker, software_picker_property_name);
	assert(h);
	return (software_picker_info *) h;
}



LPCSTR SoftwarePicker_LookupFilename(HWND hwndPicker, int nIndex)
{
	software_picker_info *pPickerInfo;
	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	if ((nIndex < 0) || (nIndex >= pPickerInfo->file_index_length))
		return NULL;

	return pPickerInfo->file_index[nIndex]->file_name;
}



const device_image_interface *SoftwarePicker_LookupDevice(HWND hwndPicker, int nIndex)
{
	software_picker_info *pPickerInfo;
	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	if ((nIndex < 0) || (nIndex >= pPickerInfo->file_index_length))
		return NULL;

	return pPickerInfo->file_index[nIndex]->device;
}



int SoftwarePicker_LookupIndex(HWND hwndPicker, LPCSTR pszFilename)
{
	software_picker_info *pPickerInfo;
	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);

	for (int i = 0; i < pPickerInfo->file_index_length; i++)
	{
		if (core_stricmp(pPickerInfo->file_index[i]->file_name, pszFilename)==0)
			return i;
	}
	return -1;
}



iodevice_t SoftwarePicker_GetImageType(HWND hwndPicker, int nIndex)
{
	iodevice_t type;
	const device_image_interface *device = SoftwarePicker_LookupDevice(hwndPicker, nIndex);

	if (device)
		type = device->image_type();
	else
		type = IO_UNKNOWN;
	return type;
}



void SoftwarePicker_SetDriver(HWND hwndPicker, const software_config *config)
{
	int i;
	software_picker_info *pPickerInfo;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	pPickerInfo->config = config;

	// invalidate the hash "realization"
	for (i = 0; i < pPickerInfo->file_index_length; i++)
	{
		//pPickerInfo->file_index[i]->hashinfo = NULL;
		pPickerInfo->file_index[i]->hash_realized = FALSE;
	}
	pPickerInfo->hashes_realized = 0;
}


/*
static void ComputeFileHash(software_picker_info *pPickerInfo, file_info *pFileInfo, const unsigned char *pBuffer, unsigned int nLength)
{
	const char *functions;

	 determine which functions to use
	functions = hashfile_functions_used(pPickerInfo->config->hashfile, pFileInfo->device->image_type());

	 compute the hash
	pFileInfo->device->device_compute_hash(pFileInfo->hashes, (const void *)pBuffer, (size_t)nLength, functions);
}



static BOOL SoftwarePicker_CalculateHash(HWND hwndPicker, int nIndex)
{
	software_picker_info *pPickerInfo;
	file_info *pFileInfo;
	LPSTR pszZipName;
	BOOL rc = FALSE;
	unsigned char *pBuffer;
	unsigned int nLength;
	HANDLE hFile, hFileMapping;
	LVFINDINFO lvfi;
	BOOL res;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	assert((nIndex >= 0) && (nIndex < pPickerInfo->file_index_length));
	pFileInfo = pPickerInfo->file_index[nIndex];

	if (pFileInfo->zip_entry_name)
	{
		// this is in a ZIP file
		util::archive_file *zip;
		zip_error ziperr;
		const zip_file_header *zipent;

		// open the ZIP file
		nLength = pFileInfo->zip_entry_name - pFileInfo->file_name;
		pszZipName = (LPSTR) alloca(nLength);
		memcpy(pszZipName, pFileInfo->file_name, nLength);
		pszZipName[nLength - 1] = '\0';

		// get the entry name
		ziperr = zip_file_open(pszZipName, &zip);
		if (ziperr == ZIPERR_NONE)
		{
			zipent = zip_file_first_file(zip);
			while(!rc && zipent)
			{
				if (core_stricmp(zipent->filename, pFileInfo->zip_entry_name)==0)
				{
					pBuffer = (unsigned char *)malloc(zipent->uncompressed_length);
					if (pBuffer)
					{
						ziperr = zip_file_decompress(zip, pBuffer, zipent->uncompressed_length);
						if (ziperr == ZIPERR_NONE)
						{
							ComputeFileHash(pPickerInfo, pFileInfo, pBuffer, zipent->uncompressed_length);
							rc = TRUE;
						}
						free(pBuffer);
					}
				}
				zipent = zip_file_next_file(zip);
			}
			zip_file_close(zip);
		}
	}
	else
	{
		// plain open file; map it into memory and calculate the hash
		hFile = win_create_file_utf8(pFileInfo->file_name, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, 0, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			nLength = GetFileSize(hFile, NULL);
			hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
			if (hFileMapping)
			{
				pBuffer = (unsigned char *)MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
				if (pBuffer)
				{
					ComputeFileHash(pPickerInfo, pFileInfo, pBuffer, nLength);
					UnmapViewOfFile(pBuffer);
					rc = TRUE;
				}
				CloseHandle(hFileMapping);
			}
			CloseHandle(hFile);
		}
	}

	if (rc)
	{
		memset(&lvfi, 0, sizeof(lvfi));
		lvfi.flags = LVFI_PARAM;
		lvfi.lParam = nIndex;
		nIndex = ListView_FindItem(hwndPicker, -1, &lvfi);
		if (nIndex > 0)
			res = ListView_RedrawItems(hwndPicker, nIndex, nIndex);
	}

	return rc;
}

*/
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
static void SoftwarePicker_RealizeHash(HWND hwndPicker, int nIndex)
{
	software_picker_info *pPickerInfo;
	file_info *pFileInfo;
	//const char *nHashFunctionsUsed = NULL;
	//unsigned int nCalculatedHashes = 0;
	//iodevice_t type;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	assert((nIndex >= 0) && (nIndex < pPickerInfo->file_index_length));
	pFileInfo = pPickerInfo->file_index[nIndex];

/*
	// Determine which hash functions we need to use for this file, and which hashes
	// have already been calculated
	if ((pPickerInfo->config->hashfile != NULL) && (pFileInfo->device != NULL))
	{
		type = pFileInfo->device->image_type();
		if (type < IO_COUNT)
			nHashFunctionsUsed = hashfile_functions_used(pPickerInfo->config->hashfile, type);
		nCalculatedHashes = hash_data_used_functions(pFileInfo->hash_string);
	}

	// Did we fully compute all hashes?
	if ((nHashFunctionsUsed & ~nCalculatedHashes) == 0)
	{
		// We have calculated all hashs for this file; mark it as realized
		pPickerInfo->file_index[nIndex]->hash_realized = TRUE;
		pPickerInfo->hashes_realized++;

		if (pPickerInfo->config->hashfile)
		{
			pPickerInfo->file_index[nIndex]->hashinfo = hashfile_lookup(pPickerInfo->config->hashfile,
				pPickerInfo->file_index[nIndex]->hashes);
		}
	}
*/
}
#pragma GCC diagnostic error "-Wunused-but-set-variable"


static BOOL SoftwarePicker_AddFileEntry(HWND hwndPicker, LPCSTR pszFilename, UINT nZipEntryNameLength, UINT32 nCrc, BOOL bForce, bool check)
{
	software_picker_info *pPickerInfo;
	file_info **ppNewIndex;
	file_info *pInfo;
	int nIndex, nSize;
	LPCSTR pszExtension = NULL;
	const device_image_interface *device = NULL;

	// first check to see if it is already here - FIXME - only check when it could be a duplicate
	if (check && (SoftwarePicker_LookupIndex(hwndPicker, pszFilename) >= 0))
		return TRUE;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);

	// look up the device
	if (strrchr(pszFilename, '.'))
		pszExtension = strrchr(pszFilename, '.');
	if ((pszExtension) && (pPickerInfo->config))
	{
		for (device_image_interface &dev : image_interface_enumerator(pPickerInfo->config->mconfig->root_device()))
		{
			if (!dev.user_loadable())
				continue;
			if (dev.uses_file_extension(pszExtension))
			{
				device = &dev;
				break;
			}
		}
	}

	// no device?  cop out unless bForce is on
	if ((device == NULL) && !bForce)
		return TRUE;

	// create the FileInfo structure
	nSize = sizeof(file_info) + strlen(pszFilename);
	pInfo = (file_info *) malloc(nSize);
	if (!pInfo)
		goto error;
	memset(pInfo, 0, nSize);

	// copy the filename
	strcpy(pInfo->file_name, pszFilename);

	// set up device and CRC, if specified
	pInfo->device = device;
	if ((device != NULL))
		nCrc = 0;
	//if (nCrc != 0)
		//snprintf(pInfo->hash_string, std::size(pInfo->hash_string), "c:%08x#", nCrc);

	// set up zip entry name length, if specified
	if (nZipEntryNameLength > 0)
		pInfo->zip_entry_name = pInfo->file_name + strlen(pInfo->file_name) - nZipEntryNameLength;

	// calculate the subname
	pInfo->base_name = strrchr(pInfo->file_name, '\\');
	if (pInfo->base_name)
		pInfo->base_name++;
	else
		pInfo->base_name = pInfo->file_name;

	ppNewIndex = (file_info**)malloc((pPickerInfo->file_index_length + 1) * sizeof(*pPickerInfo->file_index));
	memcpy(ppNewIndex,pPickerInfo->file_index,pPickerInfo->file_index_length * sizeof(*pPickerInfo->file_index));
	if (pPickerInfo->file_index) free(pPickerInfo->file_index);
	if (!ppNewIndex)
		goto error;

	nIndex = pPickerInfo->file_index_length++;
	pPickerInfo->file_index = ppNewIndex;
	pPickerInfo->file_index[nIndex] = pInfo;

	// Realize the hash
	SoftwarePicker_RealizeHash(hwndPicker, nIndex);

	// Actually insert the item into the picker
	Picker_InsertItemSorted(hwndPicker, nIndex);
	return TRUE;

error:
	if (pInfo)
		free(pInfo);
	return FALSE;
}



static BOOL SoftwarePicker_AddZipEntFile(HWND hwndPicker, LPCSTR pszZipPath, BOOL bForce, util::archive_file::ptr &pZip, bool check)
{
	LPSTR s;
	LPCSTR temp = pZip->current_name().c_str();

	// special case; skip first two characters if they are './'
	if ((temp[0] == '.') && (temp[1] == '/'))
	{
		while(*(++temp) == '/')
			;
	}

	int nZipEntryNameLength = strlen(pZip->current_name().c_str());
	int nLength = strlen(pszZipPath) + 1 + nZipEntryNameLength + 1;
	s = (LPSTR) alloca(nLength);
	snprintf(s, nLength, "%s\\%s", pszZipPath, pZip->current_name().c_str());

	return SoftwarePicker_AddFileEntry(hwndPicker, s, nZipEntryNameLength, pZip->current_crc(), bForce, check);
}

static BOOL SoftwarePicker_InternalAddFile(HWND hwndPicker, LPCSTR pszFilename, BOOL bForce, bool check)
{
	int res = 0;
	LPCSTR s;
	BOOL rc = TRUE;
	std::error_condition ziperr = std::errc::function_not_supported;
	util::archive_file::ptr pZip;

	s = strrchr(pszFilename, '.');
	if (s && (core_stricmp(s, ".zip")==0))
		ziperr = util::archive_file::open_zip(pszFilename, pZip);
	else
	if (s && (core_stricmp(s, ".7z")==0))
		ziperr = util::archive_file::open_7z(pszFilename, pZip);

	if (!ziperr)
	{
		res = pZip->first_file();
		while(rc && (res >= 0))
		{
			rc = SoftwarePicker_AddZipEntFile(hwndPicker, pszFilename, bForce, pZip, check);
			res = pZip->next_file();
		}
	pZip.reset();
	}
	else
		rc = SoftwarePicker_AddFileEntry(hwndPicker, pszFilename, 0, 0, bForce, check);

	return rc;
}



BOOL SoftwarePicker_AddFile(HWND hwndPicker, LPCSTR pszFilename, bool check)
{
	char szBuffer[2048] = {0,};

	Picker_ResetIdle(hwndPicker);
	pszFilename = NormalizePath(pszFilename, szBuffer, std::size(szBuffer));

	return SoftwarePicker_InternalAddFile(hwndPicker, pszFilename, TRUE, check);
}



BOOL SoftwarePicker_AddDirectory(HWND hwndPicker, LPCSTR pszDirectory)
{
	software_picker_info *pPickerInfo;
	directory_search_info *pSearchInfo;
	directory_search_info **ppLast;
	size_t nSearchInfoSize;
	char szBuffer[2048] = {0,};

	pszDirectory = NormalizePath(pszDirectory, szBuffer, std::size(szBuffer));

	Picker_ResetIdle(hwndPicker);
	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);

	nSearchInfoSize = sizeof(directory_search_info) + strlen(pszDirectory);
	pSearchInfo = (directory_search_info *)malloc(nSearchInfoSize);
	if (!pSearchInfo)
		return FALSE;
	memset(pSearchInfo, 0, nSearchInfoSize);
	pSearchInfo->find_handle = INVALID_HANDLE_VALUE;

	strcpy(pSearchInfo->directory_name, pszDirectory);

	// insert into linked list
	if (pPickerInfo->last_search_info)
		ppLast = &pPickerInfo->last_search_info->next;
	else
		ppLast = &pPickerInfo->first_search_info;
	*ppLast = pSearchInfo;
	pPickerInfo->last_search_info = pSearchInfo;
	return TRUE;
}



static void SoftwarePicker_FreeSearchInfo(directory_search_info *pSearchInfo)
{
	if (pSearchInfo->find_handle != INVALID_HANDLE_VALUE)
		FindClose(pSearchInfo->find_handle);
	free(pSearchInfo);
}



static void SoftwarePicker_InternalClear(software_picker_info *pPickerInfo)
{
	directory_search_info *p;
	int i;

	for (i = 0; i < pPickerInfo->file_index_length; i++)
		free(pPickerInfo->file_index[i]);

	while(pPickerInfo->first_search_info)
	{
		p = pPickerInfo->first_search_info->next;
		SoftwarePicker_FreeSearchInfo(pPickerInfo->first_search_info);
		pPickerInfo->first_search_info = p;
	}

	pPickerInfo->file_index = NULL;
	pPickerInfo->file_index_length = 0;
	pPickerInfo->current_position = 0;
	pPickerInfo->last_search_info = NULL;
}



void SoftwarePicker_Clear(HWND hwndPicker)
{
	software_picker_info *pPickerInfo;
	BOOL res;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	SoftwarePicker_InternalClear(pPickerInfo);
	res = ListView_DeleteAllItems(hwndPicker);
	res++;
}



static BOOL SoftwarePicker_AddEntry(HWND hwndPicker, directory_search_info *pSearchInfo)
{
	//software_picker_info *pPickerInfo;
	LPSTR pszFilename;
	BOOL rc;
	char* utf8_FileName;

	//pPickerInfo = GetSoftwarePickerInfo(hwndPicker);

	utf8_FileName = ui_utf8_from_wstring(pSearchInfo->fd.cFileName);
	if( !utf8_FileName )
		return FALSE;

	if (!strcmp(utf8_FileName, ".") || !strcmp(utf8_FileName, ".."))
	{
		free(utf8_FileName);
		return TRUE;
	}

	pszFilename = (LPSTR)alloca(strlen(pSearchInfo->directory_name) + 1 + strlen(utf8_FileName) + 1);
	strcpy(pszFilename, pSearchInfo->directory_name);
	strcat(pszFilename, "\\");
	strcat(pszFilename, utf8_FileName);

	if (pSearchInfo->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		rc = SoftwarePicker_AddDirectory(hwndPicker, pszFilename);
	else
		rc = SoftwarePicker_InternalAddFile(hwndPicker, pszFilename, FALSE, 1); // must check for dup

	free(utf8_FileName);
	return rc;
}



BOOL SoftwarePicker_Idle(HWND hwndPicker)
{
	software_picker_info *pPickerInfo;
	file_info *pFileInfo;
	static const char szWildcards[] = "\\*.*";
	directory_search_info *pSearchInfo;
	LPSTR pszFilter;
	BOOL bSuccess;
	int nCount;
	BOOL bDone = FALSE;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);

	pSearchInfo = pPickerInfo->first_search_info;

	if (pSearchInfo)
	{
		// searching through directories
		if (pSearchInfo->find_handle != INVALID_HANDLE_VALUE)
			bSuccess = FindNextFile(pSearchInfo->find_handle, &pSearchInfo->fd);
		else
		{
			pszFilter = (LPSTR)alloca(strlen(pSearchInfo->directory_name) + strlen(szWildcards) + 1);
			strcpy(pszFilter, pSearchInfo->directory_name);
			strcat(pszFilter, szWildcards);
			pSearchInfo->find_handle = win_find_first_file_utf8(pszFilter, &pSearchInfo->fd);
			bSuccess = pSearchInfo->find_handle != INVALID_HANDLE_VALUE;
		}

		if (bSuccess)
			SoftwarePicker_AddEntry(hwndPicker, pSearchInfo);
		else
		{
			pPickerInfo->first_search_info = pSearchInfo->next;
			if (!pPickerInfo->first_search_info)
				pPickerInfo->last_search_info = NULL;
			SoftwarePicker_FreeSearchInfo(pSearchInfo);
		}
	}
	else
	if (pPickerInfo->config!=NULL  && (pPickerInfo->hashes_realized < pPickerInfo->file_index_length))
	{
		// time to realize some hashes
		nCount = 100;

		while((nCount > 0) && pPickerInfo->file_index[pPickerInfo->current_position]->hash_realized)
		{
			pPickerInfo->current_position++;
			pPickerInfo->current_position %= pPickerInfo->file_index_length;
			nCount--;
		}

		pFileInfo = pPickerInfo->file_index[pPickerInfo->current_position];
		if (!pFileInfo->hash_realized)
		{
/*
			if (hashfile_functions_used(pPickerInfo->config->hashfile, pFileInfo->device->image_type()))
			{
			// only calculate the hash if it is appropriate for this device
				if (!SoftwarePicker_CalculateHash(hwndPicker, pPickerInfo->current_position))
					return FALSE;
			}*/
			SoftwarePicker_RealizeHash(hwndPicker, pPickerInfo->current_position);

			// under normal circumstances this will be redundant, but in the unlikely
			// event of a failure, we do not want to keep running into a brick wall
			// by calculating this hash over and over
			if (!pPickerInfo->file_index[pPickerInfo->current_position]->hash_realized)
			{
				pPickerInfo->file_index[pPickerInfo->current_position]->hash_realized = TRUE;
				pPickerInfo->hashes_realized++;
			}
		}
	}
	else
	{
		// we are done!
		bDone = TRUE;
	}

	return !bDone;
}



LPCTSTR SoftwarePicker_GetItemString(HWND hwndPicker, int nRow, int nColumn, TCHAR *pszBuffer, UINT nBufferLength)
{
	software_picker_info *pPickerInfo;
	const file_info *pFileInfo;
	LPCTSTR s = NULL;
	//const char *pszUtf8 = NULL;
	unsigned int nHashFunction = 0;
	//char szBuffer[256];
	TCHAR* t_buf;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	if ((nRow < 0) || (nRow >= pPickerInfo->file_index_length))
		return NULL;

	pFileInfo = pPickerInfo->file_index[nRow];

	switch(nColumn)
	{
		case SW_COLUMN_IMAGES:
			t_buf = ui_wstring_from_utf8(pFileInfo->base_name);
			if( !t_buf )
				return s;
			_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_buf);
			s = pszBuffer;
			free(t_buf);
			break;

/*
		case SW_COLUMN_GOODNAME:
		case SW_COLUMN_MANUFACTURER:
		case SW_COLUMN_YEAR:
		case SW_COLUMN_PLAYABLE:
			if (pFileInfo->hashinfo)
			{
				switch(nColumn)
				{
					case SW_COLUMN_GOODNAME:
						pszUtf8 = pFileInfo->hashinfo->longname;
						break;
					case SW_COLUMN_MANUFACTURER:
						pszUtf8 = pFileInfo->hashinfo->manufacturer;
						break;
					case SW_COLUMN_YEAR:
						pszUtf8 = pFileInfo->hashinfo->year;
						break;
					case SW_COLUMN_PLAYABLE:
						pszUtf8 = pFileInfo->hashinfo->playable;
						break;
				}
				if (pszUtf8)
				{
					t_buf = tstring_from_utf8(pszUtf8);
					if( !t_buf )
						return s;
					_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_buf);
					s = pszBuffer;
					free(t_buf);
				}
			}
			break;

		case SW_COLUMN_CRC:
		case SW_COLUMN_SHA1:
			switch (nColumn)
			{
				case SW_COLUMN_CRC:
					nHashFunction = hash_collection::HASH_CRC;
					break;
				case SW_COLUMN_SHA1:
					nHashFunction = hash_collection::HASH_SHA1;
					break;
			}
			if (hash_data_extract_printable_checksum(pFileInfo->hash_string, nHashFunction, szBuffer))
			{
				t_buf = tstring_from_utf8(szBuffer);
				if( !t_buf )
					return s;
				_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_buf);
				s = pszBuffer;
				free(t_buf);
			}
			break;
*/
	}
	nHashFunction++;
	return s;
}



static LRESULT CALLBACK SoftwarePicker_WndProc(HWND hwndPicker, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	software_picker_info *pPickerInfo;
	LRESULT rc;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	rc = CallWindowProc(pPickerInfo->old_window_proc, hwndPicker, nMessage, wParam, lParam);

	if (nMessage == WM_DESTROY)
	{
		SoftwarePicker_InternalClear(pPickerInfo);
		SoftwarePicker_SetDriver(hwndPicker, NULL);
		free(pPickerInfo);
	}

	return rc;
}



BOOL SetupSoftwarePicker(HWND hwndPicker, const struct PickerOptions *pOptions)
{
	software_picker_info *pPickerInfo = NULL;
	LONG_PTR l;

	if (!SetupPicker(hwndPicker, pOptions))
		goto error;

	pPickerInfo = (software_picker_info *)malloc(sizeof(*pPickerInfo));
	if (!pPickerInfo)
		goto error;

	memset(pPickerInfo, 0, sizeof(*pPickerInfo));
	if (!SetProp(hwndPicker, software_picker_property_name, (HANDLE) pPickerInfo))
		goto error;

	l = (LONG_PTR) SoftwarePicker_WndProc;
	l = SetWindowLongPtr(hwndPicker, GWLP_WNDPROC, l);
	pPickerInfo->old_window_proc = (WNDPROC) l;
	return TRUE;

error:
	if (pPickerInfo)
		free(pPickerInfo);
	return FALSE;
}
