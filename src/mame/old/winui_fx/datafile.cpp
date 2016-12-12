// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include "winui.h"

/****************************************************************************
 *      datafile constants
 ****************************************************************************/
static const char *DATAFILE_TAG_KEY	= "$info";
static const char *DATAFILE_TAG_BIO = "$bio";
static const char *DATAFILE_TAG_MAME = "$mame";
static const char *DATAFILE_TAG_DRIV = "$drv";
static const char *DATAFILE_TAG_SCORE = "$story";
static const char *DATAFILE_TAG_END = "$end";

/****************************************************************************
 *      private data for parsing functions
 ****************************************************************************/
static FILE *fp = NULL;								/* Our file pointer */
static UINT64 dwFilePos = 0;                     	/* file position */
static char filename[MAX_PATH];						/* datafile name */

struct tDatafileIndex
{
	long offset;
	const game_driver *driver;
};

static struct tDatafileIndex *hist_idx = NULL;
static struct tDatafileIndex *init_idx = NULL;
static struct tDatafileIndex *mame_idx = NULL;
static struct tDatafileIndex *driv_idx = NULL;
static struct tDatafileIndex *score_idx = NULL;

/****************************************************************************
 *      ParseClose - Closes the existing opened file (if any)
 ****************************************************************************/
static void ParseClose(void)
{
	/* If the file is open, time for fclose. */
	if (fp)
		fclose(fp);
}

/****************************************************************************
 *      ParseOpen - Open up file for reading
 ****************************************************************************/
static bool ParseOpen(const char *pszFilename)
{
	/* MAME core file parsing functions fail in recognizing UNICODE chars in UTF-8 without BOM, 
	so it's better and faster use standard C fileio functions */
	fp = fopen(pszFilename, "r");

	if (fp == NULL)
		return false;

	/* Otherwise, prepare! */
	dwFilePos = 0;
	/* identify text file type first */
	fgetc(fp);
	fseek(fp, dwFilePos, SEEK_SET);
	return true;
}

/****************************************************************************
 *      ParseSeek - Move the file position indicator
 ****************************************************************************/
static UINT8 ParseSeek(UINT64 offset, int whence)
{
	int result = fseek(fp, offset, whence);

	if (result == 0)
		dwFilePos = ftell(fp);

	return (UINT8)result;
}

/**************************************************************************
 **************************************************************************
 *
 *              Datafile functions
 *
 **************************************************************************
 **************************************************************************/

 /**************************************************************************
 *      index_datafile
 *      Create an index for the records in the currently open datafile.
 *
 *      Returns 0 on error, or the number of index entries created.
 **************************************************************************/
static int index_datafile(struct tDatafileIndex **_index, bool source)
{
	struct tDatafileIndex *idx;
	int count = 0;
	char readbuf[512];
	char name[40];
	int num_games = driver_list::total();

	/* rewind file */
	if (ParseSeek (0L, SEEK_SET)) 
		return 0;

	/* allocate index */
	idx = *_index = global_alloc_array(tDatafileIndex, (num_games + 1) * sizeof (struct tDatafileIndex));
 
	if (!idx) 
		return 0;

	while (fgets(readbuf, 512, fp))
	{
		/* DATAFILE_TAG_KEY identifies the driver */
		if (!core_strnicmp(DATAFILE_TAG_KEY, readbuf, strlen(DATAFILE_TAG_KEY)))
		{
			int game_index = 0;
			char *curpoint = &readbuf[strlen(DATAFILE_TAG_KEY) + 1];
			char *pch = NULL;
			char *ends = &readbuf[strlen(readbuf) - 1];

			while (curpoint < ends)
			{
				// search for comma
				pch = strpbrk(curpoint, ",");
				
				// found it 
				if (pch)
				{
					// copy data and validate driver
					int len = pch - curpoint;
					strncpy(name, curpoint, len);
					name[len] = '\0';

					if (!source)
						game_index = GetGameNameIndex(name);
					else
						game_index = GetSrcDriverIndex(name);

					if (game_index >= 0)
					{
						idx->driver = &driver_list::driver(game_index);
						idx->offset = ftell(fp);
						idx++;
						count++;
					}

					// update current point
					curpoint = pch + 1;
				}
				// if comma not found, copy data while until reach the end of string
				else if (!pch && curpoint < ends)
				{
					int len = ends - curpoint;
					strncpy(name, curpoint, len);
					name[len] = '\0';

					if (!source)
						game_index = GetGameNameIndex(name);
					else
						game_index = GetSrcDriverIndex(name);

					if (game_index >= 0)
					{
						idx->driver = &driver_list::driver(game_index);
						idx->offset = ftell(fp);
						idx++;
						count++;
					}

					// update current point
					curpoint = ends;
				}
			}
		}
	}
	
	/* mark end of index */
	idx->offset = 0L;
	idx->driver = 0;
	return count;
}

/**************************************************************************
 *      load_datafile_text
 *
 *      Loads text field for a driver into the buffer specified. Specify the
 *      driver, a pointer to the buffer, the buffer size, the index created by
 *      index_datafile(), and the desired text field (e.g., DATAFILE_TAG_BIO).
 *
 *      Returns 0 if successful.
 **************************************************************************/
static int load_datafile_text(const game_driver *drv, char *buffer, int bufsize, struct tDatafileIndex *idx, const char *tag, bool source_file, bool mameinfo)
{
	char readbuf[16384];

	*buffer = '\0';

	if (!source_file)
	{
		/* find driver in datafile index */
		while (idx->driver)
		{
			if (idx->driver == drv) 
				break;

			idx++;
		}
	}
	else
	{
		/* find source file in datafile index */
		while (idx->driver)
		{
			if (idx->driver->source_file == drv->source_file) 
				break;

			idx++;
		}
	}

	if (idx->driver == 0) 
		return 1; /* driver not found in index */

	/* seek to correct point in datafile */
	if (ParseSeek (idx->offset, SEEK_SET)) 
		return 1;

	/* read text until buffer is full or end of entry is encountered */
	while (fgets(readbuf, 16384, fp))
	{
		if (!core_strnicmp(DATAFILE_TAG_END, readbuf, strlen(DATAFILE_TAG_END))) 
			break;

		if (!core_strnicmp(tag, readbuf, strlen(tag))) 
			continue;

		if (strlen(buffer) + strlen(readbuf) > bufsize) 
			break;

		if (mameinfo)
		{
			char *temp = strtok(readbuf, "\r\n\r\n");

			if (temp != NULL)
				strcat(buffer, temp);
			else
				strcat(buffer, readbuf);
		}
		else
			strcat(buffer, readbuf);
	}

	return 0;
}

/**************************************************************************
 *      load_driver_history
 *      Load history text for the specified driver into the specified buffer.
 *      Combines $bio field of HISTORY.DAT with $mame field of MAMEINFO.DAT.
 *
 *      Returns 0 if successful.
 *
 *      NOTE: For efficiency the indices are never freed (intentional leak).
 **************************************************************************/
int load_driver_history(const game_driver *drv, char *buffer, int bufsize)
{
	int history = 0;

	*buffer = 0;
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\history.dat", GetDatsDir());

	/* try to open history datafile */
	if (ParseOpen(filename))
	{
		/* create index if necessary */
		if (hist_idx)
			history = 1;
		else
			history = (index_datafile (&hist_idx, false) != 0);

		/* load history text (append)*/
		if (hist_idx)
		{
			int len = strlen(buffer);
			int err = 0;
			const game_driver *gdrv;
			gdrv = drv;

			do
			{
				err = load_datafile_text(gdrv, buffer + len, bufsize - len, hist_idx, DATAFILE_TAG_BIO, false, false);
				int g = driver_list::clone(*gdrv);

				if (g != -1) 
					gdrv = &driver_list::driver(g); 
				else 
					gdrv = NULL;
			} while (err && gdrv);

			if (err) 
				history = 0;
		}

		ParseClose();
		strcat(buffer, "\n\n");
	}

	return (history == 0);
}

int load_driver_initinfo(const game_driver *drv, char *buffer, int bufsize)
{
	int gameinit = 0;

	*buffer = 0;
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\gameinit.dat", GetDatsDir());

	/* try to open gameinit datafile */
	if (ParseOpen(filename))
	{
		/* create index if necessary */
		if (init_idx)
			gameinit = 1;
		else
			gameinit = (index_datafile (&init_idx, false) != 0);

		/* load history text (append)*/
		if (init_idx)
		{
			int len = strlen(buffer);
			int err = 0;
			const game_driver *gdrv;
			gdrv = drv;

			do
			{
				err = load_datafile_text(gdrv, buffer + len, bufsize - len, init_idx, DATAFILE_TAG_MAME, false, true);
				int g = driver_list::clone(*gdrv);

				if (g != -1) 
					gdrv = &driver_list::driver(g); 
				else 
					gdrv = NULL;
			} while (err && gdrv);

			if (err) 
				gameinit = 0;
		}

		ParseClose();
		strcat(buffer, "\n\n\n");
	}

	return (gameinit == 0);
}

int load_driver_mameinfo(const game_driver *drv, char *buffer, int bufsize)
{
	machine_config config(*drv, MameUIGlobal());
	const game_driver *parent = NULL;
	char name[512];
	int mameinfo = 0;
	int is_bios = 0;

	*buffer = 0;
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\mameinfo.dat", GetDatsDir());
	strcat(buffer, "MAMEINFO:\n");

	/* List the game info 'flags' */
	if (drv->flags & MACHINE_NOT_WORKING)
		strcat(buffer, "This game doesn't work properly\n");

	if (drv->flags & MACHINE_UNEMULATED_PROTECTION)
		strcat(buffer, "This game has protection which isn't fully emulated.\n");

	if (drv->flags & MACHINE_IMPERFECT_GRAPHICS)
		strcat(buffer, "The video emulation isn't 100% accurate.\n");

	if (drv->flags & MACHINE_WRONG_COLORS)
		strcat(buffer, "The colors are completely wrong.\n");

	if (drv->flags & MACHINE_IMPERFECT_COLORS)
		strcat(buffer, "The colors aren't 100% accurate.\n");

	if (drv->flags & MACHINE_NO_SOUND)
		strcat(buffer, "This game lacks sound.\n");

	if (drv->flags & MACHINE_IMPERFECT_SOUND)
		strcat(buffer, "The sound emulation isn't 100% accurate.\n");

	if (drv->flags & MACHINE_SUPPORTS_SAVE)
		strcat(buffer, "Save state support.\n");

	if (drv->flags & MACHINE_MECHANICAL)
		strcat(buffer, "This game contains mechanical parts.\n");

	if (drv->flags & MACHINE_IS_INCOMPLETE)
		strcat(buffer, "This game was never completed.\n");

	if (drv->flags & MACHINE_NO_SOUND_HW)
		strcat(buffer, "This game has no sound hardware.\n");

	strcat(buffer, "\n");

	if (drv->flags & MACHINE_IS_BIOS_ROOT)
		is_bios = 1;

	/* try to open mameinfo datafile */
	if (ParseOpen(filename))
	{
		/* create index if necessary */
		if (mame_idx)
			mameinfo = 1;
		else
			mameinfo = (index_datafile (&mame_idx, false) != 0);

		/* load informational text (append) */
		if (mame_idx)
		{
			int len = strlen(buffer);
			int err = 0;
			const game_driver *gdrv;
			gdrv = drv;

			do
			{
				err = load_datafile_text(gdrv, buffer + len, bufsize - len, mame_idx, DATAFILE_TAG_MAME, false, true);
				int g = driver_list::clone(*gdrv);

				if (g != -1) 
					gdrv = &driver_list::driver(g); 
				else 
					gdrv = NULL;
			} while (err && gdrv);

			if (err) 
				mameinfo = 0;
		}

		ParseClose();
	}

	/* GAME INFORMATIONS */
	snprintf(name, WINUI_ARRAY_LENGTH(name), "\nGAME: %s\n", drv->name);
	strcat(buffer, name);
	snprintf(name, WINUI_ARRAY_LENGTH(name), "%s", drv->description);
	strcat(buffer, name);
	snprintf(name, WINUI_ARRAY_LENGTH(name), " (%s %s)\n\nCPU:\n", drv->manufacturer, drv->year);
	strcat(buffer, name);
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
		{
			if (exec.device().type() == scan.device().type() && strcmp(cpu_name, scan.device().name()) == 0 && clock == scan.device().clock())
				if (exectags.insert(scan.device().tag()).second)
					count++;
		}

		if (count > 1)
		{
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%d x ", count);
			strcat(buffer, name);
		}

		if (clock >= 1000000)
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%s %d.%06d MHz\n", cpu_name, clock / 1000000, clock % 1000000);
		else
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%s %d.%03d kHz\n", cpu_name, clock / 1000, clock % 1000);

		strcat(buffer, name);
	}

	strcat(buffer, "\nSOUND:\n");
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
		{
			if (sound.device().type() == scan.device().type() && strcmp(sound_name, scan.device().name()) == 0 && clock == scan.device().clock())
				if (soundtags.insert(scan.device().tag()).second)
					count++;
		}

		if (count > 1)
		{
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%d x ", count);
			strcat(buffer, name);
		}

		strcat(buffer, sound_name);

		if (clock)
		{
			if (clock >= 1000000)
				snprintf(name, WINUI_ARRAY_LENGTH(name), " %d.%06d MHz", clock / 1000000, clock % 1000000);
			else
				snprintf(name, WINUI_ARRAY_LENGTH(name), " %d.%03d kHz", clock / 1000, clock % 1000);

			strcat(buffer, name);
		}

		strcat(buffer, "\n");
	}

	if (has_sound)
	{
		speaker_device_iterator audioiter(config.root_device());
		int channels = audioiter.count();

		if(channels == 1)
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%d Channel\n", channels);
		else
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%d Channels\n", channels);

		strcat(buffer, name);
	}

	strcat(buffer, "\nVIDEO:\n");
	screen_device_iterator screeniter(config.root_device());
	int scrcount = screeniter.count();

	if (scrcount == 0)
		strcpy(buffer, "Screenless");
	else
	{
		for (screen_device &screen : screeniter)
		{
			if (screen.screen_type() == SCREEN_TYPE_VECTOR)
				strcat(buffer, "Vector");
			else 
			{
				const rectangle &visarea = screen.visible_area();

				if (drv->flags & ORIENTATION_SWAP_XY)
					snprintf(name, WINUI_ARRAY_LENGTH(name), "%d x %d (V) %f Hz", visarea.width(), visarea.height(), ATTOSECONDS_TO_HZ(screen.refresh_attoseconds()));
				else
					snprintf(name, WINUI_ARRAY_LENGTH(name), "%d x %d (H) %f Hz", visarea.width(), visarea.height(), ATTOSECONDS_TO_HZ(screen.refresh_attoseconds()));

				strcat(buffer, name);
			}

			strcat(buffer, "\n");
		}
	}

	strcat(buffer, "\nROM REGION:\n");
	int g = driver_list::clone(*drv);

	if (g != -1) 
		parent = &driver_list::driver(g);

	for (device_t &device : device_iterator(config.root_device()))
	{
		for (const rom_entry *region = rom_first_region(device); region != nullptr; region = rom_next_region(region))
		{
			for (const rom_entry *rom = rom_first_file(region); rom != nullptr; rom = rom_next_file(rom))
			{
				util::hash_collection hashes(ROM_GETHASHDATA(rom));

				if (g != -1)
				{
					machine_config pconfig(*parent, MameUIGlobal());

					for (device_t &device : device_iterator(pconfig.root_device()))
					{
						for (const rom_entry *pregion = rom_first_region(device); pregion != nullptr; pregion = rom_next_region(pregion))
						{
							for (const rom_entry *prom = rom_first_file(pregion); prom != nullptr; prom = rom_next_file(prom))
							{
								util::hash_collection phashes(ROM_GETHASHDATA(prom));

								if (hashes == phashes)
									break;
							}
						}
					}
				}

				snprintf(name, WINUI_ARRAY_LENGTH(name), "%-16s \t", ROM_GETNAME(rom));
				strcat(buffer, name);
				snprintf(name, WINUI_ARRAY_LENGTH(name), "%09d \t", rom_file_size(rom));
				strcat(buffer, name);
				snprintf(name, WINUI_ARRAY_LENGTH(name), "%-10s", ROMREGION_GETTAG(region));
				strcat(buffer, name);
				strcat(buffer, "\n");
			}
		}
	}

	for (samples_device &device : samples_device_iterator(config.root_device()))
	{
		samples_iterator sampiter(device);

		if (sampiter.altbasename() != NULL)
		{
			snprintf(name, WINUI_ARRAY_LENGTH(name), "\nSAMPLES (%s):\n", sampiter.altbasename());
			strcat(buffer, name);
		}

		std::unordered_set<std::string> already_printed;

		for (const char *samplename = sampiter.first(); samplename; samplename = sampiter.next())
		{
			// filter out duplicates
			if (!already_printed.insert(samplename).second)
				continue;

			// output the sample name
			snprintf(name, WINUI_ARRAY_LENGTH(name), "%s.wav\n", samplename);
			strcat(buffer, name);
		}
	}

	if (!is_bios)
	{
		int g = driver_list::clone(*drv);

		if (g != -1) 
			drv = &driver_list::driver(g);

		strcat(buffer, "\nORIGINAL:\n");
		strcat(buffer, drv->description);
		strcat(buffer, "\n\nCLONES:\n");

		for (int i = 0; i < driver_list::total(); i++)
		{
			if (!strcmp (drv->name, driver_list::driver(i).parent)) 
			{
				strcat(buffer, GetDriverGameTitle(i));
				strcat(buffer, "\n");
			}
		}
	}

	strcat(buffer, "\n\n\n");
	return (mameinfo == 0);
}

int load_driver_driverinfo(const game_driver *drv, char *buffer, int bufsize)
{
	int drivinfo = 0;
	char source_file[40];
	char tmp[100];

	std::string temp = core_filename_extract_base(drv->source_file, false);
	strcpy(source_file, temp.c_str());

	*buffer = 0;
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\mameinfo.dat", GetDatsDir());
	/* Print source code file */
	snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "DRIVER: %s\n", source_file);
	strcat(buffer, tmp);

	/* Try to open mameinfo datafile - driver section*/
	if (ParseOpen(filename))
	{
		/* create index if necessary */
		if (driv_idx)
			drivinfo = 1;
		else
			drivinfo = (index_datafile (&driv_idx, true) != 0);

		/* load informational text (append) */
		if (driv_idx)
		{
			int len = strlen(buffer);
			int err = load_datafile_text(drv, buffer + len, bufsize - len, driv_idx, DATAFILE_TAG_DRIV, true, true);

			if (err) 
				drivinfo = 0;
		}

		ParseClose();
	}

	strcat(buffer, "\nGAMES SUPPORTED:\n");

	for (int i = 0; i < driver_list::total(); i++)
	{
		if (!strcmp(source_file, GetDriverFileName(i)) && !(DriverIsBios(i)))
		{
			strcat(buffer, GetDriverGameTitle(i));
			strcat(buffer,"\n");
		}
	}

	strcat(buffer, "\n\n");
	return (drivinfo == 0);
}

int load_driver_scoreinfo(const game_driver *drv, char *buffer, int bufsize)
{
	int scoreinfo = 0;

	*buffer = 0;
	snprintf(filename, WINUI_ARRAY_LENGTH(filename), "%s\\story.dat", GetDatsDir());

	/* try to open story datafile */
	if (ParseOpen(filename))
	{
		/* create index if necessary */
		if (score_idx)
			scoreinfo = 1;
		else
			scoreinfo = (index_datafile (&score_idx, false) != 0);

		/* load informational text (append) */
		if (score_idx)
		{
			int len = strlen(buffer);
			int err = 0;
			const game_driver *gdrv;
			gdrv = drv;

			do
			{
				err = load_datafile_text(gdrv, buffer + len, bufsize - len, score_idx, DATAFILE_TAG_SCORE, false, false);
				int g = driver_list::clone(*gdrv);

				if (g != -1) 
					gdrv = &driver_list::driver(g); 
				else 
					gdrv = NULL;
			} while (err && gdrv);

			if (err) 
				scoreinfo = 0;
		}

		ParseClose();
	}

	return (scoreinfo == 0);
}
