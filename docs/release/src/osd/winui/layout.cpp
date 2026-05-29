// For licensing and usage information, read docs/release/winui_license.txt
// MASTER
//****************************************************************************

/***************************************************************************

  layout.cpp

  MAME specific TreeView definitions (and maybe more in the future)

***************************************************************************/
// standard windows headers
#include <windows.h>
#include <commctrl.h>

// MAME/MAMEUI headers
#include "bitmask.h"
#include "treeview.h"
#include "emu.h"
#include "mui_util.h"
#include "resource.h"
#include "mui_opts.h"
#include "splitters.h"
#include "help.h"
#include "mui_audit.h"
#include "properties.h"


static BOOL FilterAvailable(int drvindex)
{
	return !DriverUsesRoms(drvindex) || IsAuditResultYes(GetRomAuditResults(drvindex));
}

#ifdef MESS
extern const FOLDERDATA g_folderData[] =
{
	// Folder Label     Filename (if cached)  Folder Resource  Parent Icon       Child Icon      Filter-in   Filter-out  Cached?  Creation Function         Filter Function         ?      Name of ini file
	{"All Systems",     "allgames",          FOLDER_ALL,       IDI_FP_ALL,       0,              0,          0,          0, NULL,                       NULL,                    TRUE },
	{"Available",       "available",         FOLDER_AVAIL,     IDI_FP_AVAIL,     0,              FI_AVAIL,   0,          0, NULL,                       FilterAvailable,         TRUE },
	{"Arcade",          "arcade",            FOLDER_ARCADE,    IDI_FP_ARCADE,    0,              FI_ARCADE,  FI_MESS,    0, NULL,                       DriverIsArcade,          TRUE, OPTIONS_ARCADE },
	{"BIOS",            "bios",              FOLDER_BIOS,      IDI_FP_BIOS,      IDI_FC_BIOS,    0,          0,          0, CreateBIOSFolders,          DriverIsBios,            TRUE },
	{"CHD",             "harddisk",          FOLDER_HARDDISK,  IDI_FP_HARDDISK,  0,              0,          0,          0, NULL,                       DriverIsHarddisk,        TRUE },
	{"Clones",          "clones",            FOLDER_CLONES,    IDI_FP_CLONES,    0,              FI_CLONES,  FI_PARENTS, 0, NULL,                       DriverIsClone,           TRUE },
	{"CPU",             "cpu",               FOLDER_CPU,       IDI_FP_CPU,       IDI_FC_CHIP,    0,          0,          1, CreateCPUFolders },
	{"Dumping Status",  "dumping",           FOLDER_DUMP,      IDI_FP_DUMP,      IDI_FC_CHIP,    0,          0,          1, CreateDumpingFolders },
	{"FPS",             "fps",               FOLDER_FPS,       IDI_FP_FPS,       IDI_FP_DEF,     0,          0,          1, CreateFPSFolders },
	{"Horizontal",      "horizontal",        FOLDER_HORI,      IDI_FP_HORI,      0,              FI_HORI,    FI_VERT,    0, NULL,                       DriverIsVertical,        FALSE, OPTIONS_HORIZONTAL },
	{"Imperfect",       "imperfect",         FOLDER_IMP,       IDI_FP_IMP,       IDI_FP_DEF,     0,          0,          0, CreateDeficiencyFolders },
	{"Lightgun",        "lightgun",          FOLDER_LIGHTGUN,  IDI_FP_LIGHTGUN,  0,              0,          0,          0, NULL,                       DriverUsesLightGun,      TRUE },
	{"Manufacturer",    "manufacturer",      FOLDER_MANU,      IDI_FP_MANU,      IDI_FC_MANU,    0,          0,          1, CreateManufacturerFolders },
	{"Mechanical",      "mechanical",        FOLDER_MECH,      IDI_FP_MECH,      0,              0,          0,          0, NULL,                       DriverIsMechanical,      TRUE },
	{"Modified/Hacked", "modified",          FOLDER_MODIFIED,  IDI_FP_MODIFIED,  0,              0,          0,          0, NULL,                       DriverIsModified,        TRUE },
	{"Mouse",           "mouse",             FOLDER_MOUSE,     IDI_FP_MOUSE,     0,              0,          0,          0, NULL,                       DriverUsesMouse,         TRUE },
	{"Non Mechanical",  "nonmechanical",     FOLDER_NONMECH,   IDI_FP_NONMECH,   0,              0,          0,          0, NULL,                       DriverIsMechanical,      FALSE },
	{"Not Working",     "nonworking",        FOLDER_NW,        IDI_FP_NW,        0,              FI_NW,      FI_W,       0, NULL,                       DriverIsBroken,          TRUE },
	{"Parents",         "parents",           FOLDER_PARENTS,   IDI_FP_PARENTS,   0,              FI_PARENTS, FI_CLONES,  0, NULL,                       DriverIsClone,           FALSE },
//	{"Raster",          "raster",            FOLDER_RASTER,    IDI_FP_RASTER,    0,              FI_RASTER,  FI_VECTOR,  0, NULL,                       DriverIsVector,          FALSE, OPTIONS_RASTER },
	{"Resolution",      "resolution",        FOLDER_RESOL,     IDI_FP_RESOL,     IDI_FP_DEF,     0,          0,          1, CreateResolutionFolders },
	{"Samples",         "samples",           FOLDER_SAMPLES,   IDI_FP_SAMPLES,   0,              0,          0,          0, NULL,                       DriverUsesSamples,       TRUE },
	{"Save State",      "savestate",         FOLDER_SAVESTATE, IDI_FP_SAVESTATE, 0,              0,          0,          0, NULL,                       DriverSupportsSaveState, TRUE },
	{"Screens",         "screens",           FOLDER_SCREENS,   IDI_FP_MONITOR,   IDI_FC_MONITOR, 0,          0,          1, CreateScreenFolders },
	{"Sound",           "sound",             FOLDER_SOUND,     IDI_FP_SOUND,     IDI_FC_SOUND,   0,          0,          1, CreateSoundFolders },
	{"Source",          "source",            FOLDER_SOURCE,    IDI_FP_SOURCE,    IDI_FC_SOURCE,  0,          0,          1, CreateSourceFolders },
	{"Stereo",          "stereo",            FOLDER_STEREO,    IDI_FP_STEREO,    0,              0,          0,          0, NULL,                       DriverIsStereo,          TRUE },
	{"Trackball",       "trackball",         FOLDER_TRACKBALL, IDI_FP_TRACKBALL, 0,              0,          0,          0, NULL,                       DriverUsesTrackball,     TRUE },
	{"Unavailable",     "unavailable",       FOLDER_UNAVAIL,   IDI_FP_UNAVAIL,   0,              0,          FI_AVAIL,   0, NULL,                       FilterAvailable,         FALSE },
	{"Vector",          "vector",            FOLDER_VECTOR,    IDI_FP_VECTOR,    0,              FI_VECTOR,  FI_RASTER,  0, NULL,                       DriverIsVector,          TRUE, OPTIONS_VECTOR },
	{"Vertical",        "vertical",          FOLDER_VERT,      IDI_FP_VERT,      0,              FI_VERT,    FI_HORI,    0, NULL,                       DriverIsVertical,        TRUE, OPTIONS_VERTICAL },
	{"Working",         "working",           FOLDER_W,         IDI_FP_W,         0,              FI_W,       FI_NW,      0, NULL,                       DriverIsBroken,          FALSE },
	{"Year",            "year",              FOLDER_YEAR,      IDI_FP_YEAR,      IDI_FC_YEAR,    0,          0,          1, CreateYearFolders },
	{ NULL }
};
#else
extern const FOLDERDATA g_folderData[] =
{
	{"All Games",       "allgames",          FOLDER_ALLGAMES,     IDI_FOLDER_ALLGAMES,      0,             0,            0, NULL,                       NULL,                    TRUE },
	{"Available",       "available",         FOLDER_AVAILABLE,    IDI_FOLDER_AVAILABLE,     F_AVAILABLE,   0,            0, NULL,                       FilterAvailable,         TRUE },
	{"BIOS",            "bios",              FOLDER_BIOS,         IDI_FOLDER_BIOS,          0,             0,            1, CreateBIOSFolders,          DriverIsBios,            TRUE },
	{"CHD",             "harddisk",          FOLDER_HARDDISK,     IDI_FOLDER_HARDDISK,      0,             0,            0, NULL,                       DriverIsHarddisk,        TRUE },
	{"Clones",          "clones",            FOLDER_CLONES,       IDI_FOLDER_CLONES,        F_CLONES,      F_ORIGINALS,  0, NULL,                       DriverIsClone,           TRUE },
	{"CPU",             "cpu",               FOLDER_CPU,          IDI_FOLDER_CPU,           0,             0,            1, CreateCPUFolders },
	{"Dumping Status",  "dumping",           FOLDER_DUMPING,      IDI_FOLDER_DUMP,          0,             0,            1, CreateDumpingFolders },
	{"FPS",             "fps",               FOLDER_FPS,          IDI_FOLDER_FPS,           0,             0,            1, CreateFPSFolders },
	{"Horizontal",      "horizontal",        FOLDER_HORIZONTAL,   IDI_FOLDER_HORIZONTAL,    F_HORIZONTAL,  F_VERTICAL,   0, NULL,                       DriverIsVertical,        FALSE, OPTIONS_HORIZONTAL },
	{"Imperfect",       "imperfect",         FOLDER_DEFICIENCY,   IDI_FOLDER_IMPERFECT,     0,             0,            0, CreateDeficiencyFolders },
	{"Lightgun",        "Lightgun",          FOLDER_LIGHTGUN,     IDI_FOLDER_LIGHTGUN,      0,             0,            0, NULL,                       DriverUsesLightGun,      TRUE },
	{"Manufacturer",    "manufacturer",      FOLDER_MANUFACTURER, IDI_FOLDER_MANUFACTURER,  0,             0,            1, CreateManufacturerFolders },
	{"Mechanical",      "mechanical",        FOLDER_MECHANICAL,   IDI_FOLDER_MECHANICAL,    0,             0,            0, NULL,                       DriverIsMechanical,      TRUE },
	{"Non Mechanical",  "nonmechanical",     FOLDER_NONMECHANICAL,IDI_FOLDER,               0,             0,            0, NULL,                       DriverIsMechanical,      FALSE },
	{"Not Working",     "nonworking",        FOLDER_NONWORKING,   IDI_FOLDER_NONWORKING,    F_NONWORKING,  F_WORKING,    0, NULL,                       DriverIsBroken,          TRUE },
	{"Parents",         "parents",           FOLDER_ORIGINAL,     IDI_FOLDER_ORIGINALS,     F_ORIGINALS,   F_CLONES,     0, NULL,                       DriverIsClone,           FALSE },
	{"Raster",          "raster",            FOLDER_RASTER,       IDI_FOLDER_RASTER,        F_RASTER,      F_VECTOR,     0, NULL,                       DriverIsVector,          FALSE, OPTIONS_RASTER },
	{"Resolution",      "resolution",        FOLDER_RESOLUTION,   IDI_FOLDER_RESOL,         0,             0,            1, CreateResolutionFolders },
	{"Samples",         "samples",           FOLDER_SAMPLES,      IDI_FOLDER_SAMPLES,       0,             0,            0, NULL,                       DriverUsesSamples,       TRUE },
	{"Save State",      "savestate",         FOLDER_SAVESTATE,    IDI_FOLDER_SAVESTATE,     0,             0,            0, NULL,                       DriverSupportsSaveState, TRUE },
	{"Screens",         "screens",           FOLDER_SCREENS,      IDI_FOLDER,               0,             0,            1, CreateScreenFolders },
	{"Sound",           "sound",             FOLDER_SND,          IDI_FOLDER_SOUND,         0,             0,            1, CreateSoundFolders },
	{"Source",          "source",            FOLDER_SOURCE,       IDI_FOLDER_SOURCE,        0,             0,            1, CreateSourceFolders },
	{"Stereo",          "stereo",            FOLDER_STEREO,       IDI_FOLDER_SOUND,         0,             0,            0, NULL,                       DriverIsStereo,          TRUE },
	{"Trackball",       "trackball",         FOLDER_TRACKBALL,    IDI_FOLDER_TRACKBALL,     0,             0,            0, NULL,                       DriverUsesTrackball,     TRUE },
	{"Unavailable",     "unavailable",       FOLDER_UNAVAILABLE,  IDI_FOLDER_UNAVAILABLE,   0,             F_AVAILABLE,  0, NULL,                       FilterAvailable,         FALSE },
	{"Vector",          "vector",            FOLDER_VECTOR,       IDI_FOLDER_VECTOR,        F_VECTOR,      F_RASTER,     0, NULL,                       DriverIsVector,          TRUE, OPTIONS_VECTOR },
	{"Vertical",        "vertical",          FOLDER_VERTICAL,     IDI_FOLDER_VERTICAL,      F_VERTICAL,    F_HORIZONTAL, 0, NULL,                       DriverIsVertical,        TRUE, OPTIONS_VERTICAL },
	{"Working",         "working",           FOLDER_WORKING,      IDI_FOLDER_WORKING,       F_WORKING,     F_NONWORKING, 0, NULL,                       DriverIsBroken,          FALSE },
	{"Year",            "year",              FOLDER_YEAR,         IDI_FOLDER_YEAR,          0,             0,            1, CreateYearFolders },
	{ NULL }
};
#endif

/* list of filter/control Id pairs */
#ifdef MESS
extern const FILTER_ITEM g_filterList[] =
{
	{ FI_VECTOR,       IDC_FILTER_VECTOR,      DriverIsVector, TRUE },
	{ FI_RASTER,       IDC_FILTER_RASTER,      DriverIsVector, FALSE },
	{ FI_CLONES,       IDC_FILTER_CLONES,      DriverIsClone, TRUE },
	{ FI_PARENTS,      IDC_FILTER_ORIGINALS,   DriverIsClone, FALSE },
	{ FI_NW,           IDC_FILTER_NONWORKING,  DriverIsBroken, TRUE },
	{ FI_W,            IDC_FILTER_WORKING,     DriverIsBroken, FALSE },
	{ FI_HORI,         IDC_FILTER_HORIZONTAL,  DriverIsVertical, FALSE },
	{ FI_VERT,         IDC_FILTER_VERTICAL,    DriverIsVertical, TRUE },
	{ FI_UNAVAIL,      IDC_FILTER_UNAVAILABLE, FilterAvailable, FALSE },
	{ FI_MECH,         IDC_FILTER_MECHANICAL,  DriverIsMechanical, TRUE },
	{ FI_ARCADE,       IDC_FILTER_ARCADE,      DriverIsArcade, TRUE },
	{ FI_MESS,         IDC_FILTER_MESS,        DriverIsArcade, FALSE },
//	{ FI_MODIFIED,     IDC_FILTER_MODIFIED,    DriverIsModified, TRUE },
//	{ FI_AVAIL,        IDC_FILTER_AVAILABLE,   FilterAvailable, TRUE },
	{ 0 }
};
#else
extern const FILTER_ITEM g_filterList[] =
{
	{ FI_CLONES,       IDC_FILTER_CLONES,      DriverIsClone, TRUE },
	{ FI_NW,           IDC_FILTER_NONWORKING,  DriverIsBroken, TRUE },
	{ FI_UNAVAIL,      IDC_FILTER_UNAVAILABLE, FilterAvailable, FALSE },
	{ FI_RASTER,       IDC_FILTER_RASTER,      DriverIsVector, FALSE },
	{ FI_VECTOR,       IDC_FILTER_VECTOR,      DriverIsVector, TRUE },
	{ FI_PARENTS,      IDC_FILTER_ORIGINALS,   DriverIsClone, FALSE },
	{ FI_W,            IDC_FILTER_WORKING,     DriverIsBroken, FALSE },
	{ FI_AVAIL,        IDC_FILTER_AVAILABLE,   FilterAvailable, TRUE },
	{ FI_HORI,         IDC_FILTER_HORIZONTAL,  DriverIsVertical, FALSE },
	{ FI_VERT,         IDC_FILTER_VERTICAL,    DriverIsVertical, TRUE },
	{ FI_MECH,         IDC_FILTER_MECHANICAL,  DriverIsMechanical, TRUE },
	{ FI_ARCADE,       IDC_FILTER_ARCADE,      DriverIsArcade, TRUE },
	{ FI_MESS,         IDC_FILTER_MESS,        DriverIsArcade, FALSE },
	{ 0 }
};
#endif

#ifdef MESS
extern const MAMEHELPINFO g_helpInfo[] =
{
	//{ ID_HELP_CONTENTS,    TRUE,  TEXT(MAMEUIHELP"::/windows/main.htm") },
	{ ID_HELP_CONTENTS,    TRUE,  TEXT(MAMEUIHELP) }, // 0 - call up CHM file
	//{ ID_HELP_RELEASE,     TRUE,  TEXT(MAMEUIHELP) },
	//{ ID_HELP_WHATS_NEW,   TRUE,  TEXT(MAMEUIHELP"::/messnew.txt") },
	{ ID_HELP_WHATS_NEW,   TRUE,  TEXT("") }, // 1 - call up whatsnew at mamedev.org
	{ -1 }
};
#else
extern const MAMEHELPINFO g_helpInfo[] =
{
	{ ID_HELP_CONTENTS,    TRUE,  TEXT(MAMEUIHELP) },
	//{ ID_HELP_WHATS_NEWUI, TRUE,  TEXT(MAMEUIHELP"::/html/mameui_changes.txt") },
	//{ ID_HELP_TROUBLE,     TRUE,  TEXT(MAMEUIHELP"::/html/mameui_support.htm") },
	//{ ID_HELP_RELEASE,     FALSE, TEXT("windows.txt") },
	{ ID_HELP_WHATS_NEW,   TRUE,  TEXT(MAMEUIHELP"::/docs/whatsnew.txt") },
	{ -1 }
};
#endif

extern const PROPERTYSHEETINFO g_propSheets[] =
{
	{ FALSE, NULL,                   IDD_PROP_GAME,          GamePropertiesDialogProc },
	{ FALSE, NULL,                   IDD_PROP_AUDIT,         GameAuditDialogProc },
	{ TRUE,  NULL,                   IDD_PROP_DISPLAY,       GameOptionsProc },
	{ TRUE,  NULL,                   IDD_PROP_ADVANCED,      GameOptionsProc },
	{ TRUE,  NULL,                   IDD_PROP_SCREEN,        GameOptionsProc },
	{ TRUE,  NULL,                   IDD_PROP_SOUND,         GameOptionsProc },
	{ TRUE,  NULL,                   IDD_PROP_INPUT,         GameOptionsProc },
	{ TRUE,  NULL,                   IDD_PROP_CONTROLLER,    GameOptionsProc },
	{ TRUE,  NULL,                   IDD_PROP_MISC,          GameOptionsProc },
	{ TRUE,  NULL,                   IDD_PROP_LUA,           GameOptionsProc },
	{ TRUE,  NULL,                   IDD_PROP_OPENGL,        GameOptionsProc },
	{ TRUE,  NULL,                   IDD_PROP_SHADER,        GameOptionsProc },
	{ TRUE,  NULL,                   IDD_PROP_SNAP,          GameOptionsProc },
#ifdef MESS
	{ FALSE, DriverHasSoftware,      IDD_PROP_SOFTWARE,      GameMessOptionsProc },
	{ FALSE, DriverHasRam,           IDD_PROP_CONFIGURATION, GameMessOptionsProc }, // PropSheetFilter_Config not needed
#endif
	{ TRUE,  DriverIsVector,         IDD_PROP_VECTOR,        GameOptionsProc },     // PropSheetFilter_Vector not needed
	{ FALSE }
};

extern const ICONDATA g_iconData[] =
{
	// These bad ones must be first; the order matters, don't change it.
	{ IDI_LV_RN,             "rn" },   // roms missing
	{ IDI_LV_RU,             "ru" },   // not audited
	{ IDI_LV_BIOS,           "bios" },
	{ IDI_LV_CX,             "cx" },   // red x instead of cn
	{ IDI_LV_PX,             "px" },   // red x instead of pn
	{ IDI_LV_CI,             "ci" },   // imperfect clone
	{ IDI_LV_PI,             "pi" },   // imperfect parent
	{ IDI_LV_CN,             "cn" },   // not working clone
	{ IDI_LV_PN,             "pn" },   // not working parent
	{ IDI_LV_CW,             "cw" },   // working clone
	{ IDI_LV_PW,             "pw" },   // working parent
#ifdef MESS
	// These go in alphabetical order
	{ IDI_SO_CART,           "cart" },
	{ IDI_SO_CASS,           "cass" },
	{ IDI_SO_CDRM,           "cdrm" },
	{ IDI_SO_CYLN,           "cyln" },
	{ IDI_SO_FLOP,           "flop" },
	{ IDI_SO_HARD,           "hard" },
	{ IDI_SO_MEMC,           "memc" },
	{ IDI_SO_MIDI,           "midi" },
	{ IDI_SO_MTAP,           "mtap" },
	{ IDI_SO_PCRD,           "pcrd" },
	{ IDI_SO_PRIN,           "prin" },
	{ IDI_SO_PTAP,           "ptap" },
	{ IDI_SO_SERL,           "serl" },
	{ IDI_SO_SNAP,           "snap" },
#endif
	{ 0 }
};

#ifdef MESS
extern const TCHAR g_szPlayGameString[] = TEXT("&Run %s");
extern const char g_szGameCountString[] = "%d machines";
#else
extern const TCHAR g_szPlayGameString[] = TEXT("&Play %s");
extern const char g_szGameCountString[] = "%d games";
#endif

