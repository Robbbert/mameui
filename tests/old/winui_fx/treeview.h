// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#pragma once
 
#ifndef TREEVIEW_H
#define TREEVIEW_H

#define MAX_EXTRA_FOLDERS 			256
#define MAX_EXTRA_SUBFOLDERS 		256
#define ICON_MAX 		WINUI_ARRAY_LENGTH(treeIconNames)

/***************************************************************************
    Folder And Filter Definitions
 ***************************************************************************/

typedef struct
{
	const char *m_lpTitle; 									// Folder Title
	const char *short_name;  								// for saving in the .ini
	UINT        m_nFolderId; 								// ID
	UINT        m_nIconId; 									// if >= 0, resource id of icon (IDI_xxx), otherwise index in image list
	DWORD       m_dwUnset; 									// Excluded filters
	DWORD       m_dwSet;   									// Implied filters
	void        (*m_pfnCreateFolders)(int parent_index); 	// Constructor for special folders
	bool        (*m_pfnQuery)(int nDriver);			  	// Query function
	bool        m_bExpectedResult;					   	// Expected query result
} FOLDERDATA, *LPFOLDERDATA;

typedef const FOLDERDATA *LPCFOLDERDATA;

typedef struct
{
	DWORD m_dwFilterType;				/* Filter value */
	DWORD m_dwCtrlID;					/* Control ID that represents it */
	bool (*m_pfnQuery)(int nDriver);	/* Query function */
	bool m_bExpectedResult;				/* Expected query result */
} FILTER_ITEM, *LPFILTER_ITEM;

typedef const FILTER_ITEM *LPCFILTER_ITEM;

/* TreeView structures */
enum
{
	FOLDER_NONE = 0,
	FOLDER_ALLGAMES,
	FOLDER_AVAILABLE,
	FOLDER_UNAVAILABLE,
	FOLDER_MANUFACTURER,
	FOLDER_YEAR,
	FOLDER_CPU,
	FOLDER_SOUND,
	FOLDER_BIOS,
	FOLDER_HARDDISK,
	FOLDER_SAMPLES,
	FOLDER_MECHANICAL,
	FOLDER_LIGHTGUN,
	FOLDER_TRACKBALL,
	FOLDER_WORKING,
	FOLDER_DEFICIENCY,
	FOLDER_NONWORKING,
	FOLDER_ORIGINAL,
	FOLDER_CLONES,
	FOLDER_HORIZONTAL,
	FOLDER_VERTICAL,
	FOLDER_RASTER,
	FOLDER_VECTOR,
	FOLDER_SCREENS,
	FOLDER_RESOLUTION,
	FOLDER_FPS,
	FOLDER_SOURCE,
	FOLDER_SAVESTATE,
	FOLDER_DUMPING,
	MAX_FOLDERS
};

typedef enum
{
	F_CLONES        = 0x00000001,
	F_NONWORKING    = 0x00000002,
	F_UNAVAILABLE   = 0x00000004,
	F_VECTOR        = 0x00000008,
	F_RASTER        = 0x00000010,
	F_ORIGINALS     = 0x00000020,
	F_WORKING       = 0x00000040,
	F_AVAILABLE     = 0x00000080,
	F_HORIZONTAL    = 0x00001000,
	F_VERTICAL      = 0x00002000,
	F_MECHANICAL    = 0x00004000,
	F_MASK          = 0x0000FFFF,
	F_INIEDIT       = 0x00010000, 	// There is an .ini that can be edited. MSH 20070811
	F_CUSTOM        = 0x01000000  	// for current .ini custom folders
} FOLDERFLAG;

typedef struct
{
	char 		*m_lpTitle;          // String contains the folder name
	TCHAR 		*m_lptTitle;         // String contains the folder name as TCHAR*
	UINT        m_nFolderId;      	// Index / Folder ID number
	int         m_nParent;        	// Parent folder index in treeFolders[]
	int         m_nIconId;        	// negative icon index into the ImageList, or IDI_xxx resource id
	DWORD       m_dwFlags;        	// Misc flags
	LPBITS      m_lpGameBits;     	// Game bits, represent game indices
} TREEFOLDER, *LPTREEFOLDER;

typedef struct
{
	char        m_szTitle[64];  	// Folder Title
	UINT        m_nFolderId;    	// ID
	int         m_nParent;      	// Parent Folder index in treeFolders[]
	DWORD       m_dwFlags;      	// Flags - Customizable and Filters
	int         m_nIconId;      	// negative icon index into the ImageList, or IDI_xxx resource id
	int         m_nSubIconId;   	// negative icon index into the ImageList, or IDI_xxx resource id
} EXFOLDERDATA, *LPEXFOLDERDATA;

typedef struct
{
	int nResourceID;
	const char* lpName;
} TREEICON;

void FreeFolders(void);
void ResetFilters(void);
void InitTree(LPCFOLDERDATA lpFolderData, LPCFILTER_ITEM lpFilterList);
void SetCurrentFolder(LPTREEFOLDER lpFolder);
UINT GetCurrentFolderID(void);
LPTREEFOLDER GetCurrentFolder(void);
int GetNumFolders(void);
LPTREEFOLDER GetFolder(UINT nFolder);
LPTREEFOLDER GetFolderByID(UINT nID);
LPTREEFOLDER GetFolderByName(int nParentId, const char *pszFolderName);
void AddGame(LPTREEFOLDER lpFolder, UINT nGame);
void RemoveGame(LPTREEFOLDER lpFolder, UINT nGame);
int  FindGame(LPTREEFOLDER lpFolder, int nGame);
void ResetWhichGamesInFolders(void);
LPCFOLDERDATA FindFilter(DWORD folderID);
bool GameFiltered(int nGame, DWORD dwFlags);
bool GetParentFound(int nGame);
LPCFILTER_ITEM GetFilterList(void);
void SetTreeIconSize(HWND hWnd, bool bLarge);
bool GetTreeIconSize(void);
void GetFolders(TREEFOLDER ***folders,int *num_folders);
bool TryRenameCustomFolder(LPTREEFOLDER lpFolder,const char *new_name);
void AddToCustomFolder(LPTREEFOLDER lpFolder,int driver_index);
void RemoveFromCustomFolder(LPTREEFOLDER lpFolder,int driver_index);
HIMAGELIST GetTreeViewIconList(void);
int GetTreeViewIconIndex(int icon_id);
void ResetTreeViewFolders(void);
void SelectTreeViewFolder(int folder_id);

#endif
