// For licensing and usage information, read docs/release/winui_license.txt
// MASTER
//****************************************************************************

 /***************************************************************************

  emu_opts.cpp

  Interface to MAME's options and ini files.

***************************************************************************/

// standard windows headers
#include <windows.h>
#include <windowsx.h>

// standard C headers
#include <tchar.h>

// MAME/MAMEUI headers
#include "emu.h"
#include "winmain.h"
#include "ui/moptions.h"
#include "drivenum.h"
#include "emu_opts.h"
#include "path.h"
#include "main.h"


static emu_options mameopts; // core options
static ui_options emu_ui; // ui.ini
static windows_options emu_global; // Global 'default' options from emuopts.cpp - not mame.ini

typedef std::basic_string<char> string;

// char names
void emu_set_value(windows_options *o, const char* name, float value)
{
	std::ostringstream ss;
	ss << value;
	string svalue(ss.str());
	string sname = string(name);
	o->set_value(sname, svalue, OPTION_PRIORITY_CMDLINE);
}

void emu_set_value(windows_options *o, const char* name, int value)
{
	string svalue = std::to_string(value);
	string sname = string(name);
	o->set_value(sname, svalue, OPTION_PRIORITY_CMDLINE);
}

void emu_set_value(windows_options *o, const char* name, string value)
{
	string sname = string(name);
	o->set_value(sname, value, OPTION_PRIORITY_CMDLINE);
}

// string names
void emu_set_value(windows_options *o, string name, float value)
{
	std::ostringstream ss;
	ss << value;
	string svalue(ss.str());
	o->set_value(name, svalue, OPTION_PRIORITY_CMDLINE);
}

void emu_set_value(windows_options *o, string name, int value)
{
	string svalue = std::to_string(value);
	o->set_value(name, svalue, OPTION_PRIORITY_CMDLINE);
}

void emu_set_value(windows_options *o, string name, string value)
{
	o->set_value(name, value, OPTION_PRIORITY_CMDLINE);
}

// char names
void emu_set_value(windows_options &o, const char* name, float value)
{
	std::ostringstream ss;
	ss << value;
	string svalue(ss.str());
	string sname = string(name);
	o.set_value(sname, svalue, OPTION_PRIORITY_CMDLINE);
}

void emu_set_value(windows_options &o, const char* name, int value)
{
	string svalue = std::to_string(value);
	string sname = string(name);
	o.set_value(sname, svalue, OPTION_PRIORITY_CMDLINE);
}

void emu_set_value(windows_options &o, const char* name, string value)
{
	string sname = string(name);
	o.set_value(sname, value, OPTION_PRIORITY_CMDLINE);
}

// string names
void emu_set_value(windows_options &o, string name, float value)
{
	std::ostringstream ss;
	ss << value;
	string svalue(ss.str());
	o.set_value(name, svalue, OPTION_PRIORITY_CMDLINE);
}

void emu_set_value(windows_options &o, string name, int value)
{
	string svalue = std::to_string(value);
	o.set_value(name, svalue, OPTION_PRIORITY_CMDLINE);
}

void emu_set_value(windows_options &o, string name, string value)
{
	o.set_value(name, value, OPTION_PRIORITY_CMDLINE);
}

void ui_set_value(ui_options &o, string name, string value)
{
	o.set_value(name, value, OPTION_PRIORITY_CMDLINE);
}

string emu_get_value(windows_options *o, string name)
{
	const char* t = o->value(name.c_str());
	if (t)
		return string(o->value(name.c_str()));
	else
		return "";
}

string emu_get_value(windows_options &o, string name)
{
	const char* t = o.value(name.c_str());
	if (t)
		return string(o.value(name.c_str()));
	else
		return "";
}

string ui_get_value(ui_options &o, string name)
{
	const char* t = o.value(name.c_str());
	if (t)
		return string(o.value(name.c_str()));
	else
		return "";
}


struct dir_data { string dir_path; int which; };
static std::map<int, dir_data> dir_map;
static string emu_path;

string GetIniDir()
{
	//string global_ini = emu_global.value(OPTION_INIPATH);printf("GetIniDir = %s\n",global_ini.c_str());
	string global_ini = dir_get_value(7);printf("GetIniDir = %s\n",global_ini.c_str());
	if (global_ini.empty())
		return GetEmuPath() + PATH_SEPARATOR + "ini";

	char dir0[global_ini.length()+2] = { };
	strcpy(dir0, global_ini.c_str());
	char* t0 = strtok(dir0, ";");
	osd::directory::ptr b = osd::directory::open(t0);
	if (t0 && b)
	{ }
	else
		return GetEmuPath() + PATH_SEPARATOR + "ini";
	// see if relative or absolute path
	global_ini = t0;
	bool is_absolute = false;
	if ((global_ini.length() > 4) && (std::isalpha(global_ini[0])) && (global_ini[1] == ':') && (global_ini[2] == '\\'))
		is_absolute = true;
	if ((global_ini.length() > 4) && (global_ini[0] == '\\') && (global_ini[1] == '\\'))
		is_absolute = true;
	if (is_absolute)
		return global_ini;
	else
		return GetEmuPath() + PATH_SEPARATOR + global_ini;
}

string GetEmuPath()
{
	if (emu_path.empty())
	{
		char exe_path[MAX_PATH];
		GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
		emu_path = string(exe_path);
		std::size_t pos = emu_path.find_last_of("\\");
		emu_path.erase(pos);
		printf("GetEmuPath = %s\n",emu_path.c_str());
	}

	return emu_path;
}

string GetUiPath()
{
	return GetEmuPath() + PATH_SEPARATOR + "ui.ini";
}

// load mewui settings
static void LoadSettingsFile(ui_options &opts, string filename)
{
	util::core_file::ptr file;

	std::error_condition filerr = util::core_file::open(filename.c_str(), OPEN_FLAG_READ, file);
	if (!filerr)
	{
		opts.parse_ini_file(*file, OPTION_PRIORITY_CMDLINE, true, true);
		file.reset();
	}
}

// load a game ini
static void LoadSettingsFile(windows_options &opts, const char *filename)
{
	printf("LoadSettingsFile = %s\n",filename);fflush(stdout);
	util::core_file::ptr file;

	std::error_condition filerr = util::core_file::open(filename, OPEN_FLAG_READ, file);
	if (!filerr)
	{
		opts.parse_ini_file(*file, OPTION_PRIORITY_CMDLINE, true, true);
		file.reset();
	}
}

// This saves changes to <game>.INI or MAME.INI only
static void SaveSettingsFile(windows_options &opts, const char *filename, bool diff)
{
	util::core_file::ptr file;

	std::error_condition filerr = util::core_file::open(filename, OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS, file);

	if (!filerr)
	{
		string inistring;
		// MAME crashes when slot/media options encountered when using the DIFF option, so currently not used
//		if (diff)
//			inistring = opts.output_ini(&emu_global);
//		else
			inistring = opts.output_ini();
		// printf("=====%s=====\n%s\n",filename,inistring.c_str());  // for debugging
		file->puts(inistring.c_str());
		file.reset();
	}
}

/*  get options, based on passed in game number. */
void load_options(windows_options &opts, OPTIONS_TYPE opt_type, int drvindex, bool set_system_name)
{
	const game_driver *driver = NULL;
	driver = &driver_list::driver(drvindex);

	// Try base ini first
	string fname = GetEmuPath().append(PATH_SEPARATOR).append(emulator_info::get_configname()).append(".ini");
	LoadSettingsFile(opts, fname.c_str());

	fname.clear();
	if (opt_type == OPTIONS_SOURCE)
		fname = GetIniDir() + PATH_SEPARATOR + "source" + PATH_SEPARATOR + string(core_filename_extract_base(driver->type.source(), true)) + ".ini";
	else
	if (opt_type == OPTIONS_ARCADE)
		fname = GetIniDir() + PATH_SEPARATOR + "arcade.ini";
	else
	if (opt_type == OPTIONS_HORIZONTAL)
		fname = GetIniDir() + PATH_SEPARATOR + "horizontal.ini";
	else
	if (opt_type == OPTIONS_RASTER)
		fname = GetIniDir() + PATH_SEPARATOR + "raster.ini";
	else
	if (opt_type == OPTIONS_VECTOR)
		fname = GetIniDir() + PATH_SEPARATOR + "vector.ini";
	else
	if (opt_type == OPTIONS_VERTICAL)
		fname = GetIniDir() + PATH_SEPARATOR + "vertical.ini";

	if (!fname.empty())
	{
		LoadSettingsFile(opts, fname.c_str());
		return;
	}

	if ((opt_type == OPTIONS_GAME) && (drvindex > -1))
	{
		// Lastly, gamename.ini
		if (driver)
		{
			fname = GetIniDir() + PATH_SEPARATOR + string(driver->name).append(".ini");
			if (set_system_name)
				opts.set_value(OPTION_SYSTEMNAME, driver->name, OPTION_PRIORITY_CMDLINE);
			LoadSettingsFile(opts, fname.c_str());
		}
		SetDirectories(opts);
	}
}

/* Save ini file based on game_number. */
void save_options(windows_options &opts, OPTIONS_TYPE opt_type, int drvindex)
{
	string fname, filepath;
	fname.clear();
	filepath.clear();

	if (opt_type == OPTIONS_ARCADE)
		fname = GetIniDir() + PATH_SEPARATOR + "arcade.ini";
	else
	if (opt_type == OPTIONS_HORIZONTAL)
		fname = GetIniDir() + PATH_SEPARATOR + "horizontal.ini";
	else
	if (opt_type == OPTIONS_RASTER)
		fname = GetIniDir() + PATH_SEPARATOR + "raster.ini";
	else
	if (opt_type == OPTIONS_VECTOR)
		fname = GetIniDir() + PATH_SEPARATOR + "vector.ini";
	else
	if (opt_type == OPTIONS_VERTICAL)
		fname = GetIniDir() + PATH_SEPARATOR + "vertical.ini";

	if (!fname.empty())
	{
		SaveSettingsFile(opts, fname.c_str(), 1);
		return;
	}

	if (drvindex >= 0)
	{
		const game_driver *driver = NULL;
		driver = &driver_list::driver(drvindex);
		if (driver)
		{
			fname.assign(driver->name);
			if (opt_type == OPTIONS_SOURCE)
				filepath = GetIniDir() + PATH_SEPARATOR + "source" + PATH_SEPARATOR + string(core_filename_extract_base(driver->type.source(), true)) + ".ini";
		}
	}

	if (!fname.empty() && filepath.empty())
		filepath = GetIniDir().append(PATH_SEPARATOR).append(fname.c_str()).append(".ini");

	if (opt_type == OPTIONS_GLOBAL)
		filepath = GetEmuPath().append(PATH_SEPARATOR).append(emulator_info::get_configname()).append(".ini");

	if (!filepath.empty())
	{
		if (drvindex >= 0)
			SetDirectories(opts);
		SaveSettingsFile(opts, filepath.c_str(), 1);
//		printf("Settings saved to %s\n",filepath.c_str());
	}
//	else
//		printf("Unable to save settings\n");
}

void emu_opts_init(bool b)
{
	printf("emuOptsInit: About to load Global Options\n");fflush(stdout);
	load_options(emu_global, OPTIONS_GLOBAL, -1, 0);   // parse MAME.INI
	string t = GetUiPath();
	printf("emuOptsInit: About to load %s\n",t.c_str());fflush(stdout);
	LoadSettingsFile(emu_ui, GetUiPath());                // parse UI.INI
	printf("emuOptsInit: Finished\n");fflush(stdout);

	if (b)
		return;

	dir_map[1] = dir_data { OPTION_PLUGINDATAPATH, 0 };
	dir_map[2] = dir_data { OPTION_MEDIAPATH, 0 };
	dir_map[3] = dir_data { OPTION_HASHPATH, 0 };
	dir_map[4] = dir_data { OPTION_SAMPLEPATH, 0 };
	dir_map[5] = dir_data { OPTION_ARTPATH, 0 };
	dir_map[6] = dir_data { OPTION_CTRLRPATH, 0 };
	dir_map[7] = dir_data { OPTION_INIPATH, 0 };
	dir_map[8] = dir_data { OPTION_FONTPATH, 0 };
	dir_map[9] = dir_data { OPTION_CHEATPATH, 0 };
	dir_map[10] = dir_data { OPTION_CROSSHAIRPATH, 0 };
	dir_map[11] = dir_data { OPTION_PLUGINSPATH, 0 };
	dir_map[12] = dir_data { OPTION_LANGUAGEPATH, 0 };
	dir_map[13] = dir_data { OPTION_SWPATH, 0 };
	dir_map[14] = dir_data { OPTION_CFG_DIRECTORY, 0 };
	dir_map[15] = dir_data { OPTION_NVRAM_DIRECTORY, 0 };
	dir_map[16] = dir_data { OPTION_INPUT_DIRECTORY, 0 };
	dir_map[17] = dir_data { OPTION_STATE_DIRECTORY, 0 };
	dir_map[18] = dir_data { OPTION_SNAPSHOT_DIRECTORY, 0 };
	dir_map[19] = dir_data { OPTION_DIFF_DIRECTORY, 0 };
	dir_map[20] = dir_data { OPTION_COMMENT_DIRECTORY, 0 };
	dir_map[21] = dir_data { OSDOPTION_BGFX_PATH, 0 };
	dir_map[22] = dir_data { WINOPTION_HLSLPATH, 0 };
	dir_map[23] = dir_data { OPTION_HISTORY_PATH, 1 };
	dir_map[24] = dir_data { OPTION_CATEGORYINI_PATH, 1 };
	dir_map[25] = dir_data { OPTION_CABINETS_PATH, 1 };
	dir_map[26] = dir_data { OPTION_CPANELS_PATH, 1 };
	dir_map[27] = dir_data { OPTION_PCBS_PATH, 1 };
	dir_map[28] = dir_data { OPTION_FLYERS_PATH, 1 };
	dir_map[29] = dir_data { OPTION_TITLES_PATH, 1 };
	dir_map[30] = dir_data { OPTION_ENDS_PATH, 1 };
	dir_map[31] = dir_data { OPTION_MARQUEES_PATH, 1 };
	dir_map[32] = dir_data { OPTION_ARTPREV_PATH, 1 };
	dir_map[33] = dir_data { OPTION_BOSSES_PATH, 1 };
	dir_map[34] = dir_data { OPTION_LOGOS_PATH, 1 };
	dir_map[35] = dir_data { OPTION_SCORES_PATH, 1 };
	dir_map[36] = dir_data { OPTION_VERSUS_PATH, 1 };
	dir_map[37] = dir_data { OPTION_GAMEOVER_PATH, 1 };
	dir_map[38] = dir_data { OPTION_HOWTO_PATH, 1 };
	dir_map[39] = dir_data { OPTION_SELECT_PATH, 1 };
	dir_map[40] = dir_data { OPTION_ICONS_PATH, 1 };
	dir_map[41] = dir_data { OPTION_COVER_PATH, 1 };
	dir_map[42] = dir_data { OPTION_UI_PATH, 1 };
}

void dir_set_value(int dir_index, string value)
{
	if (dir_index)
	{
		if (dir_map.count(dir_index) > 0)
		{
			string sname = dir_map[dir_index].dir_path;
			int which = dir_map[dir_index].which;
			if (which)
				ui_set_value(emu_ui, sname, value);
			else
				emu_set_value(emu_global, sname, value);
		}
	}
}

string dir_get_value(int dir_index)
{
	if (dir_index)
	{
		if (dir_map.count(dir_index) > 0)
		{
			string sname = dir_map[dir_index].dir_path;
			int which = dir_map[dir_index].which;
			if (which)
				return ui_get_value(emu_ui, sname);
			else
				return emu_get_value(emu_global, sname);
		}
	}
	return "";
}

// This saves changes to UI.INI only
static void SaveSettingsFile(ui_options &opts)
{
	util::core_file::ptr file;

	// Save .\ui.ini
	string filename = GetUiPath();
	std::error_condition filerr = util::core_file::open(filename.c_str(), OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS, file);

	if (!filerr)
	{
		string inistring = opts.output_ini();
		file->puts(inistring.c_str());
		file.reset();
	}

	// Save backup to ini\ui.ini
	filename = GetIniDir() + PATH_SEPARATOR + "ui.ini";
	filerr = util::core_file::open(filename.c_str(), OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS, file);

	if (!filerr)
	{
		string inistring = opts.output_ini();
		file->puts(inistring.c_str());
		file.reset();
	}
}

// called from winui.cpp
void ui_save_ini()
{
	SaveSettingsFile(emu_ui);
}

void SetDirectories(windows_options &o)
{
	emu_set_value(o, OPTION_PLUGINDATAPATH, GetEmuPath());
	emu_set_value(o, OPTION_MEDIAPATH, dir_get_value(2));
	emu_set_value(o, OPTION_SAMPLEPATH, dir_get_value(4));
	emu_set_value(o, OPTION_INIPATH, dir_get_value(7));
	emu_set_value(o, OPTION_CFG_DIRECTORY, dir_get_value(14));
	emu_set_value(o, OPTION_SNAPSHOT_DIRECTORY, dir_get_value(18));
	emu_set_value(o, OPTION_INPUT_DIRECTORY, dir_get_value(16));
	emu_set_value(o, OPTION_STATE_DIRECTORY, dir_get_value(17));
	emu_set_value(o, OPTION_ARTPATH, dir_get_value(5));
	emu_set_value(o, OPTION_NVRAM_DIRECTORY, dir_get_value(15));
	emu_set_value(o, OPTION_CTRLRPATH, dir_get_value(6));
	emu_set_value(o, OPTION_CHEATPATH, dir_get_value(9));
	emu_set_value(o, OPTION_CROSSHAIRPATH, dir_get_value(10));
	emu_set_value(o, OPTION_FONTPATH, dir_get_value(8));
	emu_set_value(o, OPTION_DIFF_DIRECTORY, dir_get_value(19));
	emu_set_value(o, OPTION_SNAPNAME, emu_get_value(emu_global, OPTION_SNAPNAME));
	emu_set_value(o, OPTION_DEBUG, "0");
	emu_set_value(o, OPTION_SPEAKER_REPORT, "0");
	emu_set_value(o, OPTION_VERBOSE, "0");
	emu_set_value(o, OPTION_LOG, "0");
	emu_set_value(o, OPTION_OSLOG, "0");
}

// For dialogs.cpp
const char* GetSnapName(void)
{
	return emu_global.value(OPTION_SNAPNAME);
}

void SetSnapName(const char* value)
{
	string nvalue = value ? string(value) : "";
	emu_set_value(emu_global, OPTION_SNAPNAME, nvalue);
	global_save_ini();
}

// For winui.cpp
const string GetLanguageUI(void)
{
	return emu_global.value(OPTION_LANGUAGE);
}

bool GetEnablePlugins(void)
{
	return emu_global.bool_value(OPTION_PLUGINS);
}

const string GetPlugins(void)
{
	return emu_global.value(OPTION_PLUGIN);
}

bool GetSkipWarnings(void)
{
	return emu_ui.bool_value(OPTION_SKIP_WARNINGS);
}

void SetSkipWarnings(BOOL val)
{
	string c = val ? "1" : "0";
	ui_set_value(emu_ui, OPTION_SKIP_WARNINGS, c);
}

void SetSelectedSoftware(int drvindex, string opt_name, const char *software)
{
	if (opt_name.empty())
	{
		// Software List Item, we write to SOFTWARENAME to ensure all parts of a multipart set are loaded
		windows_options o;
		printf("About to write %s to OPTION_SOFTWARENAME\n",software);fflush(stdout);
		load_options(o, OPTIONS_GAME, drvindex, 1);
		o.set_value(OPTION_SOFTWARENAME, software, OPTION_PRIORITY_CMDLINE);
		save_options(o, OPTIONS_GAME, drvindex);
	}
	else
	{
		// Loose software, we write the filename to the requested image device
		char endch = opt_name.back();
		if (endch > '9')
			opt_name += "1";
		const char *s = opt_name.c_str();

		printf("SetSelectedSoftware: slot=%s driver=%s software='%s'\n", s, driver_list::driver(drvindex).name, software);

		printf("SetSelectedSoftware: About to load %s into slot %s\n",software,s);fflush(stdout);
		windows_options o;
		load_options(o, OPTIONS_GAME, drvindex, 1);
		//string swpath0 = o.value(OPTION_SWPATH);
		o.set_value(s, software, OPTION_PRIORITY_CMDLINE);
		//o.image_option(opt_name).specify(software);
		save_options(o, OPTIONS_GAME, drvindex);
		//string swpath1 = o.value(OPTION_SWPATH);
		//printf("SetSelectedSoftware: Done = %s = %s\n",swpath0.c_str(),swpath1.c_str());fflush(stdout);
	}
}

// See if this driver has software support
bool DriverHasSoftware(int drvindex)
{
	if (drvindex < 0)
		return 0;
	if (drvindex < driver_list::total())
	{
		windows_options o;
		load_options(o, OPTIONS_GAME, drvindex, 1);
		machine_config config(driver_list::driver(drvindex), o);

		for (device_image_interface &img : image_interface_enumerator(config.root_device()))
			if (img.user_loadable())
				return 1;
	}

	return 0;
}

// called from winui.cpp
void global_save_ini()
{
	// Save .\mame.ini
	string fname = GetEmuPath() + PATH_SEPARATOR + string(emulator_info::get_configname()).append(".ini");
	SaveSettingsFile(emu_global, fname.c_str(), 0);
	// Backup to ini\mame.ini
	fname = GetIniDir() + PATH_SEPARATOR + string(emulator_info::get_configname()).append(".ini");
	SaveSettingsFile(emu_global, fname.c_str(), 0);
}
#if 0
bool AreOptionsEqual(windows_options &opts1, windows_options &opts2)
{
	for (auto &curentry : opts1.entries())
	{
		if (curentry->type() != core_options::option_type::HEADER)
		{
			const char *value = curentry->value();
			const char *comp = opts2.value(curentry->name().c_str());
			if (!value && !comp) // both empty, they are the same
			{}
			else
			if (!value || !comp) // only one empty, they are different
				return false;
			else
			if (strcmp(value, comp) != 0) // both not empty, do proper compare
				return false;
		}
	}
	return true;
}
#endif
void OptionsCopy(windows_options &source, windows_options &dest)
{
	for (auto &dest_entry : source.entries())
	{
		if (dest_entry->names().size() > 0)
		{
			// identify the source entry
			const core_options::entry::shared_ptr source_entry = source.get_entry(dest_entry->name());
			if (source_entry)
			{
				const char *value = source_entry->value();
				if (value)
					dest_entry->set_value(value, source_entry->priority(), true);
			}
		}
	}
}

// Reset the given windows_options to their default settings.
static void ResetToDefaults(windows_options &opts, int priority)
{
	// iterate through the options setting each one back to the default value.
	windows_options dummy;
	OptionsCopy(dummy, opts);
}

void ResetGameOptions(int drvindex)
{
	//save_options(NULL, OPTIONS_GAME, drvindex);
}

void ResetGameDefaults(void)
{
	// Walk the global settings and reset everything to defaults;
	ResetToDefaults(emu_global, OPTION_PRIORITY_CMDLINE);
	save_options(emu_global, OPTIONS_GLOBAL, GLOBAL_OPTIONS);
}

/*
 * Reset all game, vector and source options to defaults.
 * No reason to reboot if this is done.
 */
void ResetAllGameOptions(void)
{
	for (int i = 0; i < driver_list::total(); i++)
		ResetGameOptions(i);
}

windows_options & MameUIGlobal(void)
{
	return emu_global;
}

void SetSystemName(windows_options &opts, OPTIONS_TYPE opt_type, int driver_index)
{
	if (driver_index >= 0)
		mameopts.set_system_name(driver_list::driver(driver_index).name);
}

