// For licensing and usage information, read docs/winui_license.txt
//****************************************************************************
// ------------------------------------------------------------------------
// Software Device View class
// ------------------------------------------------------------------------

#include <string.h>
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wingdi.h>
#include <winuser.h>
#include <tchar.h>

#include "emu.h"
#include "mui_opts.h"
#include "devview.h"
#include "softlist_dev.h"
#include "strconv.h"
#include "winutf8.h"
#include "mui_util.h"
#include "drivenum.h"

#define DEVVIEW_PADDING 10
#define DEVVIEW_SPACING 21


struct DevViewInfo
{
	HFONT hFont;
	int nWidth;
	BOOL bSurpressFilenameChanged;
	const software_config *config;
	const struct DevViewCallbacks *pCallbacks;
	struct DevViewEntry *pEntries;
};



struct DevViewEntry
{
	const device_image_interface *dev;
	HWND hwndStatic;
	HWND hwndEdit;
	HWND hwndBrowseButton;
	WNDPROC pfnEditWndProc;
};



static struct DevViewInfo *GetDevViewInfo(HWND hwndDevView)
{
	LONG_PTR l = GetWindowLongPtr(hwndDevView, GWLP_USERDATA);
	return (struct DevViewInfo *) l;
}



static void DevView_Clear(HWND hwndDevView)
{
	struct DevViewInfo *pDevViewInfo;
	int i = 0;

	pDevViewInfo = GetDevViewInfo(hwndDevView);

	if (pDevViewInfo->pEntries != NULL)
	{
		for (i = 0; pDevViewInfo->pEntries[i].dev; i++)
		{
			DestroyWindow(pDevViewInfo->pEntries[i].hwndStatic);
			DestroyWindow(pDevViewInfo->pEntries[i].hwndEdit);
			DestroyWindow(pDevViewInfo->pEntries[i].hwndBrowseButton);
		}
		free(pDevViewInfo->pEntries);
		pDevViewInfo->pEntries = NULL;
	}

	pDevViewInfo->config = NULL;
}



static void DevView_GetColumns(HWND hwndDevView, int *pnStaticPos, int *pnStaticWidth,
	int *pnEditPos, int *pnEditWidth, int *pnButtonPos, int *pnButtonWidth)
{
	struct DevViewInfo *pDevViewInfo;
	RECT r;

	pDevViewInfo = GetDevViewInfo(hwndDevView);

	GetClientRect(hwndDevView, &r);

	*pnStaticPos = DEVVIEW_PADDING;
	*pnStaticWidth = pDevViewInfo->nWidth;

	*pnButtonWidth = 25;
	*pnButtonPos = (r.right - r.left) - *pnButtonWidth - DEVVIEW_PADDING;

//	*pnEditPos = *pnStaticPos + *pnStaticWidth + DEVVIEW_PADDING;
	*pnEditPos = DEVVIEW_PADDING;
	*pnEditWidth = *pnButtonPos - *pnEditPos - DEVVIEW_PADDING;
	if (*pnEditWidth < 0)
		*pnEditWidth = 0;
}



void DevView_Refresh(HWND hwndDevView)
{
	struct DevViewInfo *pDevViewInfo;
	int i = 0;
	LPCTSTR pszSelection;
	TCHAR szBuffer[MAX_PATH];

	pDevViewInfo = GetDevViewInfo(hwndDevView);

	if (pDevViewInfo->pEntries)
	{
		for (i = 0; pDevViewInfo->pEntries[i].dev; i++)
		{
			pszSelection = pDevViewInfo->pCallbacks->pfnGetSelectedSoftware(
				hwndDevView,
				pDevViewInfo->config->driver_index,
				pDevViewInfo->config->mconfig,
				pDevViewInfo->pEntries[i].dev,
				szBuffer, MAX_PATH);

			if (!pszSelection)
				pszSelection = TEXT("");

			pDevViewInfo->bSurpressFilenameChanged = TRUE;
			SetWindowText(pDevViewInfo->pEntries[i].hwndEdit, pszSelection);
			pDevViewInfo->bSurpressFilenameChanged = FALSE;
		}
	}
}



static void DevView_TextChanged(HWND hwndDevView, int nChangedEntry, LPCTSTR pszFilename)
{
	struct DevViewInfo *pDevViewInfo;

	pDevViewInfo = GetDevViewInfo(hwndDevView);
	if (!pDevViewInfo->bSurpressFilenameChanged && pDevViewInfo->pCallbacks->pfnSetSelectedSoftware)
	{
		pDevViewInfo->pCallbacks->pfnSetSelectedSoftware(hwndDevView,
			pDevViewInfo->config->driver_index,
			pDevViewInfo->config->mconfig,
			pDevViewInfo->pEntries[nChangedEntry].dev,
			pszFilename);
	}
}



static LRESULT CALLBACK DevView_EditWndProc(HWND hwndEdit, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	struct DevViewInfo *pDevViewInfo;
	struct DevViewEntry *pEnt;
	LRESULT rc;
	HWND hwndDevView = 0;
	LONG_PTR l = GetWindowLongPtr(hwndEdit, GWLP_USERDATA);
	pEnt = (struct DevViewEntry *) l;
	if (IsWindowUnicode(hwndEdit))
		rc = CallWindowProcW(pEnt->pfnEditWndProc, hwndEdit, nMessage, wParam, lParam);
	else
		rc = CallWindowProcA(pEnt->pfnEditWndProc, hwndEdit, nMessage, wParam, lParam);

	if (nMessage == WM_SETTEXT)
	{
		hwndDevView = GetParent(hwndEdit);
		pDevViewInfo = GetDevViewInfo(hwndDevView);
		DevView_TextChanged(hwndDevView, pEnt - pDevViewInfo->pEntries, (LPCTSTR) lParam);
	}
	return rc;
}



#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
BOOL DevView_SetDriver(HWND hwndDevView, const software_config *config)
{
	struct DevViewInfo *pDevViewInfo;
	struct DevViewEntry *pEnt;
	int i = 0;
	int y = 0, nHeight = 0, nDevCount = 0;
	int nStaticPos = 0, nStaticWidth = 0, nEditPos = 0, nEditWidth = 0, nButtonPos = 0, nButtonWidth = 0;
	HDC hDc;
	LPTSTR *ppszDevices;
	LPTSTR t_s;
	SIZE sz;
	LONG_PTR l = 0;
	pDevViewInfo = GetDevViewInfo(hwndDevView);
	std::string instance;

	// clear out
	DevView_Clear(hwndDevView);

	// copy the config
	pDevViewInfo->config = config;

	// count total amount of devices
	nDevCount = 0;

	// compiler says &dev is unused
	for (device_image_interface &dev : image_interface_iterator(pDevViewInfo->config->mconfig->root_device()))
		nDevCount++;

	if (nDevCount > 0)
	{
		// get the names of all of the media-slots so we can then work out how much space is needed to display them
		ppszDevices = (LPTSTR *) alloca(nDevCount * sizeof(*ppszDevices));
		i = 0;
		for (device_image_interface &dev : image_interface_iterator(pDevViewInfo->config->mconfig->root_device()))
		{
			instance = string_format("%s (%s)", dev.instance_name(), dev.brief_instance_name());
			t_s = ui_wstring_from_utf8(instance.c_str());
			ppszDevices[i] = (TCHAR*)alloca((_tcslen(t_s) + 1) * sizeof(TCHAR));
			_tcscpy(ppszDevices[i], t_s);
			free(t_s);
			i++;
		}

		// Calculate the requisite size for the device column
		pDevViewInfo->nWidth = 0;
		hDc = GetDC(hwndDevView);
		for (i = 0; i < nDevCount; i++)
		{
			GetTextExtentPoint32(hDc, ppszDevices[i], _tcslen(ppszDevices[i]), &sz);
			if (sz.cx > pDevViewInfo->nWidth)
				pDevViewInfo->nWidth = sz.cx;
		}
		ReleaseDC(hwndDevView, hDc);

		pEnt = (struct DevViewEntry *) malloc(sizeof(struct DevViewEntry) * (nDevCount + 1));
		if (!pEnt)
			return FALSE;
		memset(pEnt, 0, sizeof(struct DevViewEntry) * (nDevCount + 1));
		pDevViewInfo->pEntries = pEnt;

		y = DEVVIEW_PADDING;
		nHeight = DEVVIEW_SPACING;
		DevView_GetColumns(hwndDevView, &nStaticPos, &nStaticWidth, &nEditPos, &nEditWidth, &nButtonPos, &nButtonWidth);

		// Now actually display the media-slot names, and show the empty boxes and the browse button
		for (device_image_interface &dev : image_interface_iterator(pDevViewInfo->config->mconfig->root_device()))
		{
			pEnt->dev = &dev;

			instance = string_format("%s (%s)", dev.instance_name(), dev.brief_instance_name()); // get name of the slot (long and short)
			std::transform(instance.begin(), instance.begin()+1, instance.begin(), ::toupper); // turn first char to uppercase
			pEnt->hwndStatic = win_create_window_ex_utf8(0, "STATIC", instance.c_str(), // display it
				WS_VISIBLE | WS_CHILD, nStaticPos,
				y, nStaticWidth, nHeight, hwndDevView, NULL, NULL, NULL);
			y += nHeight;

			pEnt->hwndEdit = win_create_window_ex_utf8(0, "EDIT", "",
				WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, nEditPos,
				y, nEditWidth, nHeight, hwndDevView, NULL, NULL, NULL); // display blank edit box

			pEnt->hwndBrowseButton = win_create_window_ex_utf8(0, "BUTTON", "...",
				WS_VISIBLE | WS_CHILD, nButtonPos,
				y, nButtonWidth, nHeight, hwndDevView, NULL, NULL, NULL); // display browse button

			if (pEnt->hwndStatic)
				SendMessage(pEnt->hwndStatic, WM_SETFONT, (WPARAM) pDevViewInfo->hFont, TRUE);

			if (pEnt->hwndEdit)
			{
				SendMessage(pEnt->hwndEdit, WM_SETFONT, (WPARAM) pDevViewInfo->hFont, TRUE);
				l = (LONG_PTR) DevView_EditWndProc;
				l = SetWindowLongPtr(pEnt->hwndEdit, GWLP_WNDPROC, l);
				pEnt->pfnEditWndProc = (WNDPROC) l;
				SetWindowLongPtr(pEnt->hwndEdit, GWLP_USERDATA, (LONG_PTR) pEnt);
			}

			if (pEnt->hwndBrowseButton)
				SetWindowLongPtr(pEnt->hwndBrowseButton, GWLP_USERDATA, (LONG_PTR) pEnt);

			y += nHeight;
			y += nHeight;

			pEnt++;
		}
	}

	DevView_Refresh(hwndDevView); // show names of already-loaded software
	return TRUE;
}
#ifdef __GNUC__
#pragma GCC diagnostic error "-Wunused-variable"
#endif



static void DevView_ButtonClick(HWND hwndDevView, struct DevViewEntry *pEnt, HWND hwndButton)
{
	struct DevViewInfo *pDevViewInfo;
	HMENU hMenu;
	RECT r;
	int rc = 0;
	BOOL b = true, ShowItem = false;
	TCHAR szPath[MAX_PATH];

	pDevViewInfo = GetDevViewInfo(hwndDevView);

	hMenu = CreatePopupMenu();

	for (device_image_interface &dev : image_interface_iterator(pDevViewInfo->config->mconfig->root_device()))
	{
		for (software_list_device &swlist : software_list_device_iterator(pDevViewInfo->config->mconfig->root_device()))
		{
			for (const software_info &swinfo : swlist.get_info())
			{
				const software_part &part = swinfo.parts().front();
				if (swlist.is_compatible(part) == SOFTWARE_IS_COMPATIBLE)
				{
					for (device_image_interface &image : image_interface_iterator(pDevViewInfo->config->mconfig->root_device()))
					{
						if (b && (dev.instance_name() == image.instance_name()))
						{
							const char *interface = image.image_interface();
							if (interface != nullptr && part.matches_interface(interface))
							{
								ShowItem = true;
								b = false;
							}
						}
					}
				}
			}
		}
	}

	if (pDevViewInfo->pCallbacks->pfnGetOpenFileName)
		AppendMenu(hMenu, MF_STRING, 1, TEXT("Mount File..."));

	if (ShowItem && pDevViewInfo->pCallbacks->pfnGetOpenItemName)
		AppendMenu(hMenu, MF_STRING, 4, TEXT("Mount Item..."));

	if (pEnt->dev->is_creatable())
	{
		if (pDevViewInfo->pCallbacks->pfnGetCreateFileName)
			AppendMenu(hMenu, MF_STRING, 2, TEXT("Create..."));
	}

	AppendMenu(hMenu, MF_STRING, 3, TEXT("Unmount"));

	GetWindowRect(hwndButton, &r);
	rc = TrackPopupMenu(hMenu, TPM_TOPALIGN | TPM_NONOTIFY | TPM_RETURNCMD, r.left, r.bottom, 0, hwndDevView, NULL);

	switch(rc)
	{
		case 1:
			b = pDevViewInfo->pCallbacks->pfnGetOpenFileName(hwndDevView,
				pDevViewInfo->config->mconfig, pEnt->dev, szPath, ARRAY_LENGTH(szPath));
			break;
		case 2:
			b = pDevViewInfo->pCallbacks->pfnGetCreateFileName(hwndDevView,
				pDevViewInfo->config->mconfig, pEnt->dev, szPath, ARRAY_LENGTH(szPath));
			break;
		case 3:
			b = pDevViewInfo->pCallbacks->pfnUnmount(hwndDevView,
				pDevViewInfo->config->mconfig, pEnt->dev, szPath, ARRAY_LENGTH(szPath));
			break;
		case 4:
			b = pDevViewInfo->pCallbacks->pfnGetOpenItemName(hwndDevView,
				pDevViewInfo->config->mconfig, pEnt->dev, szPath, ARRAY_LENGTH(szPath));
			break;
		default:
			b = FALSE;
			break;
	}
	if (!b)
		return;
	SetWindowText(pEnt->hwndEdit, szPath);
}



static BOOL DevView_Setup(HWND hwndDevView)
{
	struct DevViewInfo *pDevViewInfo;

	// allocate the device view info
	pDevViewInfo = (DevViewInfo*)malloc(sizeof(struct DevViewInfo));
	if (!pDevViewInfo)
		return FALSE;
	memset(pDevViewInfo, 0, sizeof(*pDevViewInfo));

	SetWindowLongPtr(hwndDevView, GWLP_USERDATA, (LONG_PTR) pDevViewInfo);

	// create and specify the font
	pDevViewInfo->hFont = CreateFont(10, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("MS Sans Serif"));
	SendMessage(hwndDevView, WM_SETFONT, (WPARAM) pDevViewInfo->hFont, FALSE);
	return TRUE;
}



static void DevView_Free(HWND hwndDevView)
{
	struct DevViewInfo *pDevViewInfo;

	DevView_Clear(hwndDevView);
	pDevViewInfo = GetDevViewInfo(hwndDevView);
	DeleteObject(pDevViewInfo->hFont);
	free(pDevViewInfo);
}



static LRESULT CALLBACK DevView_WndProc(HWND hwndDevView, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	struct DevViewInfo *pDevViewInfo;
	struct DevViewEntry *pEnt;
	int nStaticPos, nStaticWidth, nEditPos, nEditWidth, nButtonPos, nButtonWidth;
	RECT r;
	LONG_PTR l = 0;
	LRESULT rc = 0;
	BOOL bHandled = FALSE;
	HWND hwndButton;

	switch(nMessage)
	{
		case WM_DESTROY:
			DevView_Free(hwndDevView);
			break;

		case WM_SHOWWINDOW:
			if (wParam)
			{
				pDevViewInfo = GetDevViewInfo(hwndDevView);
				DevView_Refresh(hwndDevView);
			}
			break;

		case WM_COMMAND:
			switch(HIWORD(wParam))
			{
				case BN_CLICKED:
					hwndButton = (HWND) lParam;
					l = GetWindowLongPtr(hwndButton, GWLP_USERDATA);
					pEnt = (struct DevViewEntry *) l;
					DevView_ButtonClick(hwndDevView, pEnt, hwndButton);
					break;
			}
			break;
	}

	if (!bHandled)
		rc = DefWindowProc(hwndDevView, nMessage, wParam, lParam);

	switch(nMessage)
	{
		case WM_CREATE:
			if (!DevView_Setup(hwndDevView))
				return -1;
			break;

		case WM_MOVE:
		case WM_SIZE:
			pDevViewInfo = GetDevViewInfo(hwndDevView);
			pEnt = pDevViewInfo->pEntries;
			if (pEnt)
			{
				DevView_GetColumns(hwndDevView, &nStaticPos, &nStaticWidth,
					&nEditPos, &nEditWidth, &nButtonPos, &nButtonWidth);
				while(pEnt->dev)
				{
					GetClientRect(pEnt->hwndStatic, &r);
					MapWindowPoints(pEnt->hwndStatic, hwndDevView, ((POINT *) &r), 2);
					//MoveWindow(pEnt->hwndStatic, nStaticPos, r.top, nStaticWidth, r.bottom - r.top, FALSE); // has its own line, no need to move
					// On next line, so need DEVVIEW_SPACING to put them down there.
					MoveWindow(pEnt->hwndEdit, nEditPos, r.top+DEVVIEW_SPACING, nEditWidth, r.bottom - r.top, FALSE);
					MoveWindow(pEnt->hwndBrowseButton, nButtonPos, r.top+DEVVIEW_SPACING, nButtonWidth, r.bottom - r.top, FALSE);
					pEnt++;
				}
			}
			break;
	}
	return rc;
}



void DevView_SetCallbacks(HWND hwndDevView, const struct DevViewCallbacks *pCallbacks)
{
	struct DevViewInfo *pDevViewInfo;
	pDevViewInfo = GetDevViewInfo(hwndDevView);
	pDevViewInfo->pCallbacks = pCallbacks;
}



void DevView_RegisterClass(void)
{
	WNDCLASS wc;
	memset(&wc, 0, sizeof(wc));
	wc.lpszClassName = TEXT("MessSoftwareDevView");
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpfnWndProc = DevView_WndProc;
	wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
	RegisterClass(&wc);
}



