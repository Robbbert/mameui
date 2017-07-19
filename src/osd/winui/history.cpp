// For licensing and usage information, read docs/winui_license.txt
//  MASTER
//****************************************************************************
/***************************************************************************

  history.cpp

 *   history functions.
 *      History database engine
 *      Collect all information on the selected driver, and return it as
 *         a string. Called by winui.cpp
 *
 *      Token parsing by Neil Bradley
 *      Modifications and higher-level functions by John Butler

 *      Further work by Mamesick and Robbbert

***************************************************************************/

// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include <windows.h>
#include <fstream>

// MAME/MAMEUI headers
#include "emu.h"
#include "screen.h"
#include "speaker.h"
#include "drivenum.h"
#include "mui_util.h"
#include "mui_opts.h"
#include "sound/samples.h"
#define WINUI_ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))

/****************************************************************************
 *      datafile constants
 ****************************************************************************/
//static const char *DATAFILE_TAG_KEY = "$info";
static const char *DATAFILE_TAG_BIO = "$bio";
static const char *DATAFILE_TAG_MAME = "$mame";
static const char *DATAFILE_TAG_DRIV = "$drv";
static const char *DATAFILE_TAG_SCORE = "$story";
static const char *DATAFILE_TAG_END = "$end";
static const char *DATAFILE_TAG_CMD = "$cmd";
static const char *DATAFILE_TAG_MARP = "$marp";

/* File Numbers:
   0 = Messinfo.dat
   1 = Mameinfo.dat
   2 = History.dat
   3 = Sysinfo.dat
   4 = Gameinit.dat
   5 = Command.dat
   6 = Story.dat
   7 = Marp.dat
*/

int file_sizes[8] = { 0 };
std::map<std::string, std::streampos> mymap[8];

static bool create_index(std::ifstream &fp, int filenum)
{
	size_t i;
	if (!fp.good())
		return false;
	// get file size
	fp.seekg(0, std::ios::end);
	size_t file_size = fp.tellg();
	// same file as before?
	if (file_size == file_sizes[filenum])
		return true;
	// new file, it needs to be indexed
	mymap[filenum].clear();
	file_sizes[filenum] = file_size;
	fp.seekg(0);
	std::string file_line, first, second;
	std::getline(fp, file_line);
	while (fp.good())
	{
		char t1 = file_line[0];
		if ((std::count(file_line.begin(),file_line.end(),'=') == 1) && (t1 == '$')) // line must start with $ and contain one =
		{
			// tellg is buggy, we need to rewind the pointer to the previous $
			int position = fp.tellg(); // get bugged info
			fp.seekg(position); // position file to wrong place
			int c = fp.get(); // now scan backwards until $ found
			while(c != 0x24)
			{
				position--;
				fp.seekg(position);
				c = fp.get();
			}
			// now start
			i = std::count(file_line.begin(),file_line.end(),',');
			if (i)
			{
				// line has commas, split up to separate keys
				size_t j = file_line.find('=');
				first = file_line.substr(0, j+1);
				file_line.erase(0, j+1);
				for (size_t k = 0; k < i; k++)
				{
					// remove leading space in command.dat
					t1 = file_line[0];
					if (t1 == ' ')
						file_line.erase(0,1);
					// find first comma
					j = file_line.find(',');
					if (j != std::string::npos)
					{
						second = file_line.substr(0, j);
						file_line.erase(0, j+1);
					}
					else
						second = file_line;
					// store into index
					mymap[filenum][first + second] = position;
				}
			}
			else
				mymap[filenum][file_line] = position;
		}
		std::getline(fp, file_line);
	}
	// check contents
//	if (filenum == 3)
//	for (auto const &it : mymap[filenum])
//		printf("%s = %X\n", it.first.c_str(), int(it.second));
	return true;
}

static std::string load_datafile_text(std::ifstream &fp, std::streampos offset, const char *tag)
{
	std::string readbuf;

	fp.seekg(offset);
	std::string file_line;

	/* read text until buffer is full or end of entry is encountered */
	while (std::getline(fp, file_line))
	{
		//printf("%s\n",file_line.c_str());
		if (file_line.find(DATAFILE_TAG_END)==0)
			break;

		if (file_line.find(tag)==0)
			continue;

		readbuf.append(file_line).append("\n");
	}

	return readbuf;
}

std::string load_software_history(const game_driver *drv, const char* datsdir, std::string software, int filenum)
{
	std::string buffer;
	char filename[MAX_PATH];  /* datafile name */
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\history.dat", datsdir);

	/* try to open datafile */
	std::ifstream fp (filename,std::ios::binary);

	/* try to open datafile */
	if (create_index(fp, filenum))
	{
		size_t i = software.find(":");
		std::string ssys = software.substr(0, i);
		std::string ssoft = software.substr(i+1);
		std::string first = std::string("$") + ssys + std::string("=") + ssoft;
		// find pointer
		auto search = mymap[filenum].find(first);
		if (search != mymap[filenum].end())
		{
			std::streampos position = mymap[filenum].find(first)->second;
			/* load informational text (append) */
			std::string temp = load_datafile_text(fp, position, DATAFILE_TAG_BIO);
			if (!temp.empty())
				buffer.append("\n**** :HISTORY item: ****\n\n").append(temp).append("\n\n\n");
		}

		fp.close();
	}

	return buffer;
}

std::string load_driver_history(const game_driver *drv, const char* datsdir, int filenum)
{
	std::string buffer;
	char filename[MAX_PATH];  /* datafile name */
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\history.dat", datsdir);

	/* try to open datafile */
	std::ifstream fp (filename);

	/* try to open datafile */
	if (create_index(fp, filenum))
	{
		std::string first = std::string("$info=")+drv->name;
		// find pointer
		auto search = mymap[filenum].find(first);
		if (search != mymap[filenum].end())
		{
			std::streampos position = mymap[filenum].find(first)->second;
			/* load informational text (append) */
			std::string temp = load_datafile_text(fp, position, DATAFILE_TAG_BIO);
			if (!temp.empty())
				buffer.append("\n**** :HISTORY: ****\n\n").append(temp).append("\n\n\n");
		}

		fp.close();
	}

	return buffer;
}

std::string load_driver_sysinfo(const game_driver *drv, const char* datsdir, int filenum)
{
	std::string buffer;
	char filename[MAX_PATH];  /* datafile name */
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\sysinfo.dat", datsdir);

	/* try to open datafile */
	std::ifstream fp (filename);

	/* try to open datafile */
	if (create_index(fp, filenum))
	{
		std::string first = std::string("$info=")+drv->name;
		// find pointer
		auto search = mymap[filenum].find(first);
		if (search != mymap[filenum].end())
		{
			std::streampos position = mymap[filenum].find(first)->second;
			/* load informational text (append) */
			std::string temp = load_datafile_text(fp, position, DATAFILE_TAG_BIO);
			if (!temp.empty())
				buffer.append("\n**** :SYSINFO: ****\n\n").append(temp).append("\n\n\n");
		}

		fp.close();
	}

	return buffer;
}

std::string load_driver_mameinfo(const game_driver *drv, const char* datsdir, int filenum)
{
	std::string buffer;
	char filename[MAX_PATH];  /* datafile name */
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\mameinfo.dat", datsdir);
	std::ifstream fp (filename);

	/* try to open datafile */
	if (create_index(fp, filenum))
	{
		std::string first = std::string("$info=")+drv->name;
		// find pointer
		auto search = mymap[filenum].find(first);
		if (search != mymap[filenum].end())
		{
			std::streampos position = mymap[filenum].find(first)->second;
			/* load informational text (append) */
			std::string temp = load_datafile_text(fp, position, DATAFILE_TAG_MAME);
			if (!temp.empty())
				buffer.append("\n**** :MAMEINFO: ****\n\n").append(temp).append("\n\n\n");
		}

		fp.close();
	}

	return buffer;
}

std::string load_driver_mamedrv(const game_driver *drv, const char* datsdir, int filenum)
{
	std::string buffer;
	char tmp[100];
	char filename[MAX_PATH];  /* datafile name */

	std::string temp = drv->type.source();
	size_t i = temp.find_last_of("/");
	temp.erase(0,i+1);

	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\mameinfo.dat", datsdir);
	std::ifstream fp (filename);
	snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "\n***:MAMEINFO DRIVER: %s\n", temp.c_str());

	if (create_index(fp, filenum))
	{
		std::string first = std::string("$info=")+temp;
		// find pointer
		auto search = mymap[filenum].find(first);
		if (search != mymap[filenum].end())
		{
			std::streampos position = mymap[filenum].find(first)->second;
			/* load informational text (append) */
			std::string temp = load_datafile_text(fp, position, DATAFILE_TAG_DRIV);
			if (!temp.empty())
				buffer.append(tmp).append(temp).append("\n\n\n");
		}

		fp.close();
	}

	return buffer;
}

std::string load_driver_messinfo(const game_driver *drv, const char* datsdir, int filenum)
{
	std::string buffer;
	char filename[MAX_PATH];  /* datafile name */
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\messinfo.dat", datsdir);
	std::ifstream fp (filename);

	/* try to open datafile */
	if (create_index(fp, filenum))
	{
		std::string first = std::string("$info=")+drv->name;
		// find pointer
		auto search = mymap[filenum].find(first);
		if (search != mymap[filenum].end())
		{
			std::streampos position = mymap[filenum].find(first)->second;
			/* load informational text (append) */
			std::string temp = load_datafile_text(fp, position, DATAFILE_TAG_MAME);
			if (!temp.empty())
				buffer.append("\n**** :MESSINFO: ****\n\n").append(temp).append("\n\n\n");
		}

		fp.close();
	}

	return buffer;
}

std::string load_driver_messdrv(const game_driver *drv, const char* datsdir, int filenum)
{
	std::string buffer;
	char tmp[100];
	char filename[MAX_PATH];  /* datafile name */

	std::string temp = drv->type.source();
	size_t i = temp.find_last_of("/");
	temp.erase(0,i+1);

	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\messinfo.dat", datsdir);
	std::ifstream fp (filename);
	snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "\n***:MESSINFO DRIVER: %s\n", temp.c_str());

	if (create_index(fp, filenum))
	{
		std::string first = std::string("$info=")+temp;
		// find pointer
		auto search = mymap[filenum].find(first);
		if (search != mymap[filenum].end())
		{
			std::streampos position = mymap[filenum].find(first)->second;
			/* load informational text (append) */
			std::string temp = load_datafile_text(fp, position, DATAFILE_TAG_DRIV);
			if (!temp.empty())
				buffer.append(tmp).append(temp).append("\n\n\n");
		}

		fp.close();
	}

	return buffer;
}

std::string load_driver_initinfo(const game_driver *drv, const char* datsdir, int filenum)
{
	std::string buffer;
	char filename[MAX_PATH];  /* datafile name */
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\gameinit.dat", datsdir);
	std::ifstream fp (filename);
	if (create_index(fp, filenum))
	{
		std::string first = std::string("$info=")+drv->name;
		// find pointer
		auto search = mymap[filenum].find(first);
		if (search != mymap[filenum].end())
		{
			std::streampos position = mymap[filenum].find(first)->second;
			/* load informational text (append) */
			std::string temp = load_datafile_text(fp, position, DATAFILE_TAG_MAME);
			if (!temp.empty())
				buffer.append("\n**** :GAMEINIT: ****\n\n").append(temp).append("\n\n\n");
		}

		fp.close();
	}

	return buffer;
}

std::string load_driver_command(const game_driver *drv, const char* datsdir, int filenum)
{
	std::string buffer;
	char filename[MAX_PATH];  /* datafile name */
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\command.dat", datsdir);
	std::ifstream fp (filename);

	/* try to open datafile */
	if (create_index(fp, filenum))
	{
		std::string first = std::string("$info=")+drv->name;
		// find pointer
		auto search = mymap[filenum].find(first);
		if (search != mymap[filenum].end())
		{
			std::streampos position = mymap[filenum].find(first)->second;
			/* load informational text (append) */
			std::string temp = load_datafile_text(fp, position, DATAFILE_TAG_CMD);
			if (!temp.empty())
				buffer.append("\n**** :COMMANDS: ****\n\n").append(temp).append("\n\n\n");
		}

		fp.close();
	}

	return buffer;
}

std::string load_driver_scoreinfo(const game_driver *drv, const char* datsdir, int filenum)
{
	std::string buffer;
	char filename[MAX_PATH];  /* datafile name */
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\story.dat", datsdir);
	std::ifstream fp (filename);

	/* try to open datafile */
	if (create_index(fp, filenum))
	{
		std::string first = std::string("$info=")+drv->name;
		// find pointer
		auto search = mymap[filenum].find(first);
		if (search != mymap[filenum].end())
		{
			std::streampos position = mymap[filenum].find(first)->second;
			/* load informational text (append) */
			std::string temp = load_datafile_text(fp, position, DATAFILE_TAG_SCORE);
			if (!temp.empty())
				buffer.append("\n**** :HIGH SCORES: ****\n\n").append(temp).append("\n\n\n");
		}

		fp.close();
	}

	return buffer;
}

std::string load_driver_marpinfo(const game_driver *drv, const char* datsdir, int filenum)
{
	std::string buffer;
	char filename[MAX_PATH];  /* datafile name */
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\marp.dat", datsdir);
	std::ifstream fp (filename);

	/* try to open datafile */
	if (create_index(fp, filenum))
	{
		std::string first = std::string("$info=")+drv->name;
		// find pointer
		auto search = mymap[filenum].find(first);
		if (search != mymap[filenum].end())
		{
			std::streampos position = mymap[filenum].find(first)->second;
			/* load informational text (append) */
			std::string temp = load_datafile_text(fp, position, DATAFILE_TAG_MARP);
			if (!temp.empty())
				buffer.append("\n**** :MARP HIGH SCORES: ****\n\n").append(temp).append("\n\n\n");
		}

		fp.close();
	}

	return buffer;
}

// General hardware information
std::string load_driver_geninfo(const game_driver *drv, const char* datsdir)
{
	machine_config config(*drv, MameUIGlobal());
	const game_driver *parent = NULL;
	char name[512];
	bool is_bios = false;
	std::string buffer = "\n**** :GENERAL MACHINE INFO: ****\n\n";

	/* List the game info 'flags' */
	if (drv->flags & MACHINE_NOT_WORKING)
		buffer.append("This game doesn't work properly\n");

	if (drv->flags & MACHINE_UNEMULATED_PROTECTION)
		buffer.append("This game has protection which isn't fully emulated.\n");

	if (drv->flags & MACHINE_IMPERFECT_GRAPHICS)
		buffer.append("The video emulation isn't 100% accurate.\n");

	if (drv->flags & MACHINE_WRONG_COLORS)
		buffer.append("The colors are completely wrong.\n");

	if (drv->flags & MACHINE_IMPERFECT_COLORS)
		buffer.append("The colors aren't 100% accurate.\n");

	if (drv->flags & MACHINE_NO_SOUND)
		buffer.append("This game lacks sound.\n");

	if (drv->flags & MACHINE_IMPERFECT_SOUND)
		buffer.append("The sound emulation isn't 100% accurate.\n");

	if (drv->flags & MACHINE_SUPPORTS_SAVE)
		buffer.append("Save state support.\n");

	if (drv->flags & MACHINE_MECHANICAL)
		buffer.append("This game contains mechanical parts.\n");

	if (drv->flags & MACHINE_IS_INCOMPLETE)
		buffer.append("This game was never completed.\n");

	if (drv->flags & MACHINE_NO_SOUND_HW)
		buffer.append("This game has no sound hardware.\n");

	buffer.append("\n");

	if (drv->flags & MACHINE_IS_BIOS_ROOT)
		is_bios = true;

	/* GAME INFORMATIONS */
	snprintf(name, WINUI_ARRAY_LENGTH(name), "\nGAME: %s\n", drv->name);
	buffer.append(name);
	snprintf(name, WINUI_ARRAY_LENGTH(name), "%s", drv->type.fullname());
	buffer.append(name);
	snprintf(name, WINUI_ARRAY_LENGTH(name), " (%s %s)\n\nCPU:\n", drv->manufacturer, drv->year);
	buffer.append(name);
	/* iterate over CPUs */
	execute_interface_iterator cpuiter(config.root_device());
	std::unordered_set<std::string> exectags;

	for (device_execute_interface &exec : cpuiter)
	{
		if (!exectags.insert(exec.device().tag()).second)
			continue;

		int count = 1;
		int clock = exec.device().clock();
		const char *cpu_name = exec.device().name();

		for (device_execute_interface &scan : cpuiter)
			if (exec.device().type() == scan.device().type() && strcmp(cpu_name, scan.device().name()) == 0 && clock == scan.device().clock())
				if (exectags.insert(scan.device().tag()).second)
					count++;

		if (count > 1)
		{
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%d x ", count);
			buffer.append(name);
		}

		if (clock >= 1000000)
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%s %d.%06d MHz\n", cpu_name, clock / 1000000, clock % 1000000);
		else
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%s %d.%03d kHz\n", cpu_name, clock / 1000, clock % 1000);

		buffer.append(name);
	}

	buffer.append("\nSOUND:\n");
	int has_sound = 0;
	/* iterate over sound chips */
	sound_interface_iterator sounditer(config.root_device());
	std::unordered_set<std::string> soundtags;

	for (device_sound_interface &sound : sounditer)
	{
		if (!soundtags.insert(sound.device().tag()).second)
			continue;

		has_sound = 1;
		int count = 1;
		int clock = sound.device().clock();
		const char *sound_name = sound.device().name();

		for (device_sound_interface &scan : sounditer)
			if (sound.device().type() == scan.device().type() && strcmp(sound_name, scan.device().name()) == 0 && clock == scan.device().clock())
				if (soundtags.insert(scan.device().tag()).second)
					count++;

		if (count > 1)
		{
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%d x ", count);
			buffer.append(name);
		}

		buffer.append(sound_name);

		if (clock)
		{
			if (clock >= 1000000)
				snprintf(name, WINUI_ARRAY_LENGTH(name), " %d.%06d MHz", clock / 1000000, clock % 1000000);
			else
				snprintf(name, WINUI_ARRAY_LENGTH(name), " %d.%03d kHz", clock / 1000, clock % 1000);

			buffer.append(name);
		}

		buffer.append("\n");
	}

	if (has_sound)
	{
		speaker_device_iterator audioiter(config.root_device());
		int channels = audioiter.count();

		if(channels == 1)
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%d Channel\n", channels);
		else
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%d Channels\n", channels);

		buffer.append(name);
	}

	buffer.append("\nVIDEO:\n");
	screen_device_iterator screeniter(config.root_device());
	int scrcount = screeniter.count();

	if (scrcount == 0)
		buffer.append("Screenless");
	else
	{
		for (screen_device &screen : screeniter)
		{
			if (screen.screen_type() == SCREEN_TYPE_VECTOR)
				buffer.append("Vector");
			else
			{
				const rectangle &visarea = screen.visible_area();

				if (drv->flags & ORIENTATION_SWAP_XY)
					snprintf(name, WINUI_ARRAY_LENGTH(name), "%d x %d (V) %f Hz", visarea.width(), visarea.height(), ATTOSECONDS_TO_HZ(screen.refresh_attoseconds()));
				else
					snprintf(name, WINUI_ARRAY_LENGTH(name), "%d x %d (H) %f Hz", visarea.width(), visarea.height(), ATTOSECONDS_TO_HZ(screen.refresh_attoseconds()));

				buffer.append(name);
			}

			buffer.append("\n");
		}
	}

	buffer.append("\nROM REGION:\n");
	int g = driver_list::clone(*drv);

	if (g != -1)
		parent = &driver_list::driver(g);

	for (device_t &device : device_iterator(config.root_device()))
	{
		for (const rom_entry *region = rom_first_region(device); region; region = rom_next_region(region))
		{
			for (const rom_entry *rom = rom_first_file(region); rom; rom = rom_next_file(rom))
			{
				util::hash_collection hashes(ROM_GETHASHDATA(rom));

				if (g != -1)
				{
					machine_config pconfig(*parent, MameUIGlobal());

					for (device_t &device : device_iterator(pconfig.root_device()))
					{
						for (const rom_entry *pregion = rom_first_region(device); pregion; pregion = rom_next_region(pregion))
						{
							for (const rom_entry *prom = rom_first_file(pregion); prom; prom = rom_next_file(prom))
							{
								util::hash_collection phashes(ROM_GETHASHDATA(prom));

								if (hashes == phashes)
									break;
							}
						}
					}
				}

				snprintf(name, WINUI_ARRAY_LENGTH(name), "%-16s \t", ROM_GETNAME(rom));
				buffer.append(name);
				snprintf(name, WINUI_ARRAY_LENGTH(name), "%09d \t", rom_file_size(rom));
				buffer.append(name);
				snprintf(name, WINUI_ARRAY_LENGTH(name), "%-10s", ROMREGION_GETTAG(region));
				buffer.append(name).append("\n");
			}
		}
	}

	for (samples_device &device : samples_device_iterator(config.root_device()))
	{
		samples_iterator sampiter(device);

		if (sampiter.altbasename())
		{
			snprintf(name, WINUI_ARRAY_LENGTH(name), "\nSAMPLES (%s):\n", sampiter.altbasename());
			buffer.append(name);
		}

		std::unordered_set<std::string> already_printed;

		for (const char *samplename = sampiter.first(); samplename; samplename = sampiter.next())
		{
			// filter out duplicates
			if (!already_printed.insert(samplename).second)
				continue;

			// output the sample name
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%s.wav\n", samplename);
			buffer.append(name);
		}
	}

	if (!is_bios)
	{
		int g = driver_list::clone(*drv);

		if (g != -1)
			drv = &driver_list::driver(g);

		buffer.append("\nORIGINAL:\n");
		buffer.append(drv->type.fullname());
		buffer.append("\n\nCLONES:\n");

		for (int i = 0; i < driver_list::total(); i++)
		{
			if (!strcmp (drv->name, driver_list::driver(i).parent))
			{
				buffer.append(driver_list::driver(i).type.fullname());
				buffer.append("\n");
			}
		}
	}

	char source_file[40], tmp[100];
	std::string temp = core_filename_extract_base(drv->type.source(), false);
	strcpy(source_file, temp.c_str());
	snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "\nGENERAL SOURCE INFO: %s\n", temp.c_str());
	buffer.append(tmp);
	buffer.append("\nGAMES SUPPORTED:\n");

	for (int i = 0; i < driver_list::total(); i++)
	{
		std::string t1 = driver_list::driver(i).type.source();
		size_t j = t1.find_last_of("/");
		t1.erase(0, j+1);
		if ((strcmp(source_file, t1.c_str())==0) && !(DriverIsBios(i)))
			buffer.append(driver_list::driver(i).type.fullname()).append("\n");
	}

	return buffer;
}

char * GetGameHistory(int driver_index, std::string software)
{
	std::string fullbuf;
	char buf[400];
	strcpy(buf, GetDatsDir());
	// only want first path
	const char* datsdir = strtok(buf, ";");

	if (!software.empty())
		fullbuf.append(load_software_history(&driver_list::driver(driver_index), datsdir, software, 2));

	fullbuf.append(load_driver_history(&driver_list::driver(driver_index), datsdir, 2));
	fullbuf.append(load_driver_sysinfo(&driver_list::driver(driver_index), datsdir, 3));
	fullbuf.append(load_driver_initinfo(&driver_list::driver(driver_index), datsdir, 4));
	fullbuf.append(load_driver_mameinfo(&driver_list::driver(driver_index), datsdir, 1));
	fullbuf.append(load_driver_mamedrv(&driver_list::driver(driver_index), datsdir, 1));
	fullbuf.append(load_driver_messinfo(&driver_list::driver(driver_index), datsdir, 0));
	fullbuf.append(load_driver_messdrv(&driver_list::driver(driver_index), datsdir, 0));
	fullbuf.append(load_driver_command(&driver_list::driver(driver_index), datsdir, 5));
	fullbuf.append(load_driver_scoreinfo(&driver_list::driver(driver_index), datsdir, 6));
	fullbuf.append(load_driver_marpinfo(&driver_list::driver(driver_index), datsdir, 7));
	fullbuf.append(load_driver_geninfo(&driver_list::driver(driver_index), datsdir));

	return ConvertToWindowsNewlines(fullbuf.c_str());
}

