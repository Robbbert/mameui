// For licensing and usage information, read docs/release/winui_license.txt
//****************************************************************************
// NOTE: ifdef MESS doesn't work here
#ifndef WINUI_TREEVIEW_H
#define WINUI_TREEVIEW_H

#include "bitmask.h"
#include <stdint.h>
#include "emu_opts.h"

/***************************************************************************
    Folder And Filter Definitions
 ***************************************************************************/

typedef struct
{
	const char *m_lpTitle; // Folder Title
	const char *short_name;  // for saving in the .ini
	UINT        m_nFolderId; // ID
	UINT        m_nIconId; // icon for parent folder. if >= 0, IDI_xxx, otherwise index in image list
	UINT        m_nIconId2; // icon for subfolders
	DWORD       m_dwUnset; // Excluded filters
	DWORD       m_dwSet;   // Implied filters
	BOOL        m_process;      // 1 = process only if rebuilding the cache
	void        (*m_pfnCreateFolders)(int parent_index); // Constructor for special folders
	BOOL        (*m_pfnQuery)(int nDriver);              // Query function
	BOOL        m_bExpectedResult;                       // Expected query result
	OPTIONS_TYPE m_opttype = OPTIONS_MAX;                                // Has an ini file (vector.ini, etc)
} FOLDERDATA, *LPFOLDERDATA;

typedef const FOLDERDATA *LPCFOLDERDATA;

typedef struct
{
	DWORD m_dwFilterType;				/* Filter value */
	DWORD m_dwCtrlID;					/* Control ID that represents it */
	BOOL (*m_pfnQuery)(int nDriver);	/* Query function */
	BOOL m_bExpectedResult;				/* Expected query result */
} FILTER_ITEM, *LPFILTER_ITEM;

typedef const FILTER_ITEM *LPCFILTER_ITEM;

/***************************************************************************
    Functions to build builtin folder lists
 ***************************************************************************/

void CreateManufacturerFolders(int parent_index);
void CreateYearFolders(int parent_index);
void CreateSourceFolders(int parent_index);
void CreateScreenFolders(int parent_index);
void CreateResolutionFolders(int parent_index);
void CreateFPSFolders(int parent_index);
void CreateBIOSFolders(int parent_index);
void CreateCPUFolders(int parent_index);
void CreateSoundFolders(int parent_index);
void CreateOrientationFolders(int parent_index);
void CreateDeficiencyFolders(int parent_index);
void CreateDumpingFolders(int parent_index);

/***************************************************************************/

#define MAX_EXTRA_FOLDERS 512
#define MAX_EXTRA_SUBFOLDERS 512

/* TreeView structures */
enum
{
	FOLDER_NONE = 0,
	FOLDER_ALL,
	FOLDER_AVAIL,
	FOLDER_ARCADE,
	FOLDER_BIOS,
	FOLDER_CLONES,
	FOLDER_CPU,
	FOLDER_IMP,
	FOLDER_DUMP,
	FOLDER_FPS,
	FOLDER_HARDDISK,
	FOLDER_HORI,
	FOLDER_LIGHTGUN,
	FOLDER_MANU,
	FOLDER_MECH,
	FOLDER_MODIFIED,
	FOLDER_MOUSE,
	FOLDER_NONMECH,
	FOLDER_NW,
	FOLDER_PARENTS,
	FOLDER_RASTER,
	FOLDER_RESOL,
	FOLDER_SAMPLES,
	FOLDER_SAVESTATE,
	FOLDER_SCREENS,
	FOLDER_SOUND,
	FOLDER_SOURCE,
	FOLDER_STEREO,
	FOLDER_TRACKBALL,
	FOLDER_UNAVAIL,
	FOLDER_VECTOR,
	FOLDER_VERT,
	FOLDER_W,
	FOLDER_YEAR,
	MAX_FOLDERS,
};

typedef enum
{
	FI_CLONES        = 0x00000001,
	FI_NW            = 0x00000002,
	FI_UNAVAIL       = 0x00000004,
	FI_VECTOR        = 0x00000008,
	FI_RASTER        = 0x00000010,
	FI_PARENTS       = 0x00000020,
	FI_W             = 0x00000040,
	FI_AVAIL         = 0x00000080,
	FI_HORI          = 0x00000100,
	FI_VERT          = 0x00000200,
	FI_MECH          = 0x00000400,
	FI_ARCADE        = 0x00000800,
	FI_MESS          = 0x00001000,
	FI_MODIFIED      = 0x00008000,
	FI_MASK          = 0x0000FFFF,
	FI_INIEDIT       = 0x00010000, // There is an .ini that can be edited. MSH 20070811
	FI_CUSTOM        = 0x01000000  // for current .ini custom folders
} FOLDERFLAG;

typedef struct
{
	LPSTR       m_lpTitle;        // String contains the folder name
	LPTSTR      m_lptTitle;       // String contains the folder name as TCHAR*
	UINT        m_nFolderId;      // Index / Folder ID number
	int         m_nParent;        // Parent folder index in treeFolders[]
	int         m_nIconId;        // negative icon index into the ImageList, or IDI_xxx resource id
	DWORD       m_dwFlags;        // Misc flags
	LPBITS      m_lpGameBits;     // Game bits, represent game indices
} TREEFOLDER, *LPTREEFOLDER;

typedef struct
{
	char        m_szTitle[64];  // Folder Title
	UINT        m_nFolderId;    // ID
	int         m_nParent;      // Parent Folder index in treeFolders[]
	DWORD       m_dwFlags;      // Flags - Customisable and Filters
	int         m_nIconId;      // negative icon index into the ImageList, or IDI_xxx resource id
	int         m_nSubIconId;   // negative icon index into the ImageList, or IDI_xxx resource id
} EXFOLDERDATA, *LPEXFOLDERDATA;

void FreeFolders();
void ResetFilters();
void InitTree(LPCFOLDERDATA lpFolderData, LPCFILTER_ITEM lpFilterList);
void SetCurrentFolder(LPTREEFOLDER lpFolder);
UINT GetCurrentFolderID();

LPTREEFOLDER GetCurrentFolder();
int GetNumFolders();
LPTREEFOLDER GetFolder(UINT nFolder);
LPTREEFOLDER GetFolderByID(UINT nID);
LPTREEFOLDER GetFolderByName(int nParentId, const char *pszFolderName);

void AddGame(LPTREEFOLDER lpFolder, UINT nGame);
void RemoveGame(LPTREEFOLDER lpFolder, UINT nGame);
int  FindGame(LPTREEFOLDER lpFolder, int nGame);

void ResetWhichGamesInFolders();

LPCFOLDERDATA FindFilter(DWORD folderID);

BOOL GameFiltered(int nGame, DWORD dwFlags);
BOOL GetParentFound(int nGame);

LPCFILTER_ITEM GetFilterList();

void SetTreeIconSize(HWND hWnd, BOOL bLarge);
BOOL GetTreeIconSize();

void GetFolders(TREEFOLDER ***folders,int *num_folders);
BOOL TryRenameCustomFolder(LPTREEFOLDER lpFolder,const char *new_name);
void AddToCustomFolder(LPTREEFOLDER lpFolder,int driver_index);
void RemoveFromCustomFolder(LPTREEFOLDER lpFolder,int driver_index);

HIMAGELIST GetTreeViewIconList();
int GetTreeViewIconIndex(int icon_id);

void ResetTreeViewFolders();
void SelectTreeViewFolder(int folder_id);

#endif /* TREEVIEW_H */

