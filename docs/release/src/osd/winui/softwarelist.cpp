// For licensing and usage information, read docs/winui_license.txt
//****************************************************************************
//============================================================
//
//  softwarelist.cpp - MESS's softwarelist picker
//
//============================================================

#include <windows.h>
#include <commctrl.h>
#include <tchar.h>

#include "picker.h"
#include "softwarelist.h"
#include "corestr.h"



//============================================================
//  TYPE DEFINITIONS
//============================================================

typedef struct _file_info file_info;
struct _file_info
{
	char list_name[512];
	char description[1024];
	char file_name[128];
	char publisher[1024];
	char year[32];
	char full_name[640];
	char usage[1024];
	char device[512];
};

typedef struct _software_list_info software_list_info;
struct _software_list_info
{
	WNDPROC old_window_proc;
	file_info **file_index;
	int file_index_length;
	const software_config *config;
};



//============================================================
//  CONSTANTS
//============================================================

static const TCHAR software_list_property_name[] = TEXT("SWLIST");
static int software_numberofitems = 0;



static software_list_info *GetSoftwareListInfo(HWND hwndPicker)
{
	HANDLE h = GetProp(hwndPicker, software_list_property_name);
	//assert(h);
	return (software_list_info *) h;
}

#if 0
// return just the filename, to run a software in the software list - THIS IS NO LONGER USED
LPCSTR SoftwareList_LookupFilename(HWND hwndPicker, int nIndex)
{
	software_list_info *pPickerInfo;
	pPickerInfo = GetSoftwareListInfo(hwndPicker);
	if ((nIndex < 0) || (nIndex >= pPickerInfo->file_index_length))
		return NULL;
	return pPickerInfo->file_index[nIndex]->file_name;
}
#endif

// return the list:file, for screenshot / history / inifile
LPCSTR SoftwareList_LookupFullname(HWND hwndPicker, int nIndex)
{
	software_list_info *pPickerInfo;
	pPickerInfo = GetSoftwareListInfo(hwndPicker);
	if ((nIndex < 0) || (nIndex >= pPickerInfo->file_index_length))
		return NULL;
	return pPickerInfo->file_index[nIndex]->full_name;
}

// return the media slot in which the software is mounted
LPCSTR SoftwareList_LookupDevice(HWND hwndPicker, int nIndex)
{
	software_list_info *pPickerInfo;
	pPickerInfo = GetSoftwareListInfo(hwndPicker);
	if ((nIndex < 0) || (nIndex >= pPickerInfo->file_index_length))
		return NULL;
	return pPickerInfo->file_index[nIndex]->device;
}


int SoftwareList_LookupIndex(HWND hwndPicker, LPCSTR pszFilename)
{
	software_list_info *pPickerInfo;
	pPickerInfo = GetSoftwareListInfo(hwndPicker);

	for (int i = 0; i < pPickerInfo->file_index_length; i++)
	{
		if (core_stricmp(pPickerInfo->file_index[i]->file_name, pszFilename)==0)
			return i;
	}
	return -1;
}


#if 0
// not used, swlist items don't have icons
iodevice_//t SoftwareList_GetImageType(HWND hwndPicker, int nIndex)
{
	return IO_UNKNOWN;
}
#endif


void SoftwareList_SetDriver(HWND hwndPicker, const software_config *config)
{
	software_list_info *pPickerInfo;

	pPickerInfo = GetSoftwareListInfo(hwndPicker);
	pPickerInfo->config = config;
}


BOOL SoftwareList_AddFile(HWND hwndPicker, string pszName, string pszListname, string pszDescription, string pszPublisher, string pszYear, string pszUsage, string pszDevice)
{
	Picker_ResetIdle(hwndPicker);

	software_list_info *pPickerInfo;
	file_info **ppNewIndex;
	file_info *pInfo;
	int nIndex;

	pPickerInfo = GetSoftwareListInfo(hwndPicker);

	// create the FileInfo structure
	int nSize = sizeof(file_info);
	pInfo = (file_info *) malloc(nSize);
	if (!pInfo)
		goto error;
	memset(pInfo, 0, nSize);

	// copy the filename
	strcpy(pInfo->file_name, pszName.c_str());
	strcpy(pInfo->list_name, pszListname.c_str());
	if (!pszDescription.empty()) strcpy(pInfo->description, longdots(pszDescription,200).c_str());
	if (!pszPublisher.empty()) strcpy(pInfo->publisher, longdots(pszPublisher,200).c_str());
	if (!pszYear.empty()) strcpy(pInfo->year, longdots(pszYear, 8).c_str());
	if (!pszUsage.empty()) strcpy(pInfo->usage, longdots(pszUsage,200).c_str());
	if (!pszDevice.empty()) strcpy(pInfo->device, pszDevice.c_str());
	sprintf(pInfo->full_name,"%s:%s", pInfo->list_name,pInfo->file_name);

	ppNewIndex = (file_info**)malloc((pPickerInfo->file_index_length + 1) * sizeof(*pPickerInfo->file_index));
	memcpy(ppNewIndex,pPickerInfo->file_index,pPickerInfo->file_index_length * sizeof(*pPickerInfo->file_index));
	if (pPickerInfo->file_index) free(pPickerInfo->file_index);
	if (!ppNewIndex)
		goto error;

	nIndex = pPickerInfo->file_index_length++;
	pPickerInfo->file_index = ppNewIndex;
	pPickerInfo->file_index[nIndex] = pInfo;

	// Actually insert the item into the picker
	Picker_InsertItemSorted(hwndPicker, nIndex);
	software_numberofitems++;
	return true;

error:
	if (pInfo)
		free(pInfo);
	return false;
}


static void SoftwareList_InternalClear(software_list_info *pPickerInfo)
{
	for (int i = 0; i < pPickerInfo->file_index_length; i++)
		free(pPickerInfo->file_index[i]);

	pPickerInfo->file_index = NULL;
	pPickerInfo->file_index_length = 0;
	software_numberofitems = 0;
}



void SoftwareList_Clear(HWND hwndPicker)
{
	software_list_info *pPickerInfo;

	pPickerInfo = GetSoftwareListInfo(hwndPicker);
	SoftwareList_InternalClear(pPickerInfo);
	BOOL res = ListView_DeleteAllItems(hwndPicker);
	res++;
}


BOOL SoftwareList_Idle(HWND hwndPicker)
{
	return false;
}



LPCTSTR SoftwareList_GetItemString(HWND hwndPicker, int nRow, int nColumn, TCHAR *pszBuffer, UINT nBufferLength)
{
	software_list_info *pPickerInfo;
	const file_info *pFileInfo;
	LPCTSTR s = NULL;
	TCHAR* t_buf;

	pPickerInfo = GetSoftwareListInfo(hwndPicker);
	if ((nRow < 0) || (nRow >= pPickerInfo->file_index_length))
		return NULL;

	pFileInfo = pPickerInfo->file_index[nRow];

	switch(nColumn)
	{
		case 0:
			t_buf = ui_wstring_from_utf8(pFileInfo->file_name);
			if( !t_buf )
				return s;
			_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_buf);
			s = pszBuffer;
			free(t_buf);
			break;
		case 1:
			t_buf = ui_wstring_from_utf8(pFileInfo->list_name);
			if( !t_buf )
				return s;
			_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_buf);
			s = pszBuffer;
			free(t_buf);
			break;
		case 2:
			t_buf = ui_wstring_from_utf8(pFileInfo->description);
			if( !t_buf )
				return s;
			_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_buf);
			s = pszBuffer;
			free(t_buf);
			break;
		case 3:
			t_buf = ui_wstring_from_utf8(pFileInfo->year);
			if( !t_buf )
				return s;
			_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_buf);
			s = pszBuffer;
			free(t_buf);
			break;
		case 4:
			t_buf = ui_wstring_from_utf8(pFileInfo->publisher);
			if( !t_buf )
				return s;
			_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_buf);
			s = pszBuffer;
			free(t_buf);
			break;
		case 5:
			t_buf = ui_wstring_from_utf8(pFileInfo->usage);
			if( !t_buf )
				return s;
			_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_buf);
			s = pszBuffer;
			free(t_buf);
			break;
	}
	return s;
}



static LRESULT CALLBACK SoftwareList_WndProc(HWND hwndPicker, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	software_list_info *pPickerInfo;
	pPickerInfo = GetSoftwareListInfo(hwndPicker);
	LRESULT rc = CallWindowProc(pPickerInfo->old_window_proc, hwndPicker, nMessage, wParam, lParam);

	if (nMessage == WM_DESTROY)
	{
		SoftwareList_InternalClear(pPickerInfo);
		SoftwareList_SetDriver(hwndPicker, NULL);
		free(pPickerInfo);
	}

	return rc;
}



BOOL SetupSoftwareList(HWND hwndPicker, const struct PickerOptions *pOptions)
{
	software_list_info *pPickerInfo = NULL;
	LONG_PTR l;

	if (!SetupPicker(hwndPicker, pOptions))
		goto error;

	pPickerInfo = (software_list_info *)malloc(sizeof(*pPickerInfo));
	if (!pPickerInfo)
		goto error;

	memset(pPickerInfo, 0, sizeof(*pPickerInfo));
	if (!SetProp(hwndPicker, software_list_property_name, (HANDLE) pPickerInfo))
		goto error;

	l = (LONG_PTR) SoftwareList_WndProc;
	l = SetWindowLongPtr(hwndPicker, GWLP_WNDPROC, l);
	pPickerInfo->old_window_proc = (WNDPROC) l;
	return true;

error:
	if (pPickerInfo)
		free(pPickerInfo);
	return false;
}

int SoftwareList_GetNumberOfItems()
{
	return software_numberofitems;
}
