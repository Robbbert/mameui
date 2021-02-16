// For licensing and usage information, read docs/winui_license.txt
// MASTER
//****************************************************************************

//============================================================
//
//  datamap.c - Win32 dialog and options bridge code
//
//============================================================

// standard windows headers
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tchar.h>

// standard C headers

// MAME/MAMEUI headers
#include "mui_opts.h"
#include "datamap.h"
#include "winutf8.h"
#include "emu_opts.h"


#ifdef _MSC_VER
#define snprintf _snprintf
#endif


//============================================================
//  TYPE DEFINITIONS
//============================================================

enum _control_type
{
	CT_UNKNOWN,
	CT_BUTTON,
	CT_STATIC,
	CT_EDIT,
	CT_COMBOBOX,
	CT_TRACKBAR,
	CT_LISTVIEW
};

typedef enum _control_type control_type;


typedef struct _datamap_entry datamap_entry;
struct _datamap_entry
{
	// the basics about the entry
	int dlgitem;
	datamap_entry_type type;
	const char *option_name;

	// callbacks
	datamap_callback callbacks[DCT_COUNT];
	get_option_name_callback get_option_name;

	// formats
	const char *int_format;
	const char *float_format;

	// trackbar options
	BOOL use_trackbar_options;
	float trackbar_min;
	float trackbar_max;
	float trackbar_increments;
};


struct _datamap
{
	int entry_count;
	datamap_entry entries[256];	// 256 options entries seems enough for now...
};

typedef void (*datamap_default_callback)(datamap *map, HWND hwnd, windows_options *o, datamap_entry *entry, const char *option_name);



//============================================================
//  PROTOTYPES
//============================================================

static datamap_entry *find_entry(datamap *map, int dlgitem);
static control_type get_control_type(HWND hwnd);
static int control_operation(datamap *map, HWND dialog, windows_options *o, datamap_entry *entry, datamap_callback_type callback_type);
static void read_control(datamap *map, HWND hwnd, windows_options *o, datamap_entry *entry, const char *option_name);
static void populate_control(datamap *map, HWND hwnd, windows_options *o, datamap_entry *entry, const char *option_name);
//static char *tztrim(float float_value);


//============================================================
//  datamap_create
//============================================================

datamap *datamap_create(void)
{
	datamap *map = (datamap *)malloc(sizeof(*map));
	if (!map)
		return NULL;

	map->entry_count = 0;
	return map;
}



//============================================================
//  datamap_free
//============================================================

void datamap_free(datamap *map)
{
	free(map);
}



//============================================================
//  datamap_add
//============================================================

void datamap_add(datamap *map, int dlgitem, datamap_entry_type type, const char *option_name)
{
	// sanity check for too many entries
	if (!(map->entry_count < std::size(map->entries)))
	{
		printf("Datamap.cpp Line __LINE__ too many entries\n");
		return;
	}

	// add entry to the datamap
	memset(&map->entries[map->entry_count], 0, sizeof(map->entries[map->entry_count]));
	map->entries[map->entry_count].dlgitem = dlgitem;
	map->entries[map->entry_count].type = type;
	map->entries[map->entry_count].option_name = option_name;
	map->entry_count++;
}



//============================================================
//  datamap_set_callback
//============================================================

void datamap_set_callback(datamap *map, int dlgitem, datamap_callback_type callback_type, datamap_callback callback)
{
	datamap_entry *entry;

	assert(callback_type >= 0);
	assert(callback_type < DCT_COUNT);

	entry = find_entry(map, dlgitem);
	entry->callbacks[callback_type] = callback;
}



//============================================================
//  datamap_set_option_name_callback
//============================================================

void datamap_set_option_name_callback(datamap *map, int dlgitem, get_option_name_callback get_option_name)
{
	datamap_entry *entry;
	entry = find_entry(map, dlgitem);
	entry->get_option_name = get_option_name;
}



//============================================================
//  datamap_set_trackbar_range
//============================================================

void datamap_set_trackbar_range(datamap *map, int dlgitem, float min, float max, float increments)
{
	datamap_entry *entry = find_entry(map, dlgitem);
	entry->use_trackbar_options = TRUE;
	entry->trackbar_min = min;
	entry->trackbar_max = max;
	entry->trackbar_increments = increments;
}



//============================================================
//  datamap_set_int_format
//============================================================

void datamap_set_int_format(datamap *map, int dlgitem, const char *format)
{
	datamap_entry *entry = find_entry(map, dlgitem);
	entry->int_format = format;
}



//============================================================
//  datamap_set_float_format
//============================================================

void datamap_set_float_format(datamap *map, int dlgitem, const char *format)
{
	datamap_entry *entry = find_entry(map, dlgitem);
	entry->float_format = format;
}



//============================================================
//  datamap_read_control
//============================================================

BOOL datamap_read_control(datamap *map, HWND dialog, windows_options &o, int dlgitem)
{
	datamap_entry *entry = find_entry(map, dlgitem);
	return control_operation(map, dialog, &o, entry, DCT_READ_CONTROL);
}



//============================================================
//  datamap_read_all_controls
//============================================================

void datamap_read_all_controls(datamap *map, HWND dialog, windows_options &o)
{
	for (int i = 0; i < map->entry_count; i++)
		control_operation(map, dialog, &o, &map->entries[i], DCT_READ_CONTROL);
}



//============================================================
//  datamap_populate_control
//============================================================

void datamap_populate_control(datamap *map, HWND dialog, windows_options &o, int dlgitem)
{
	datamap_entry *entry = find_entry(map, dlgitem);
	control_operation(map, dialog, &o, entry, DCT_POPULATE_CONTROL);
}



//============================================================
//  datamap_populate_all_controls
//============================================================

void datamap_populate_all_controls(datamap *map, HWND dialog, windows_options &o)
{
	for (int i = 0; i < map->entry_count; i++)
		control_operation(map, dialog, &o, &map->entries[i], DCT_POPULATE_CONTROL);
}



//============================================================
//  datamap_update_control
//============================================================

void datamap_update_control(datamap *map, HWND dialog, windows_options &o, int dlgitem)
{
	datamap_entry *entry = find_entry(map, dlgitem);
	control_operation(map, dialog, &o, entry, DCT_UPDATE_STATUS);
}



//============================================================
//  datamap_update_all_controls
//============================================================

void datamap_update_all_controls(datamap *map, HWND dialog, windows_options *o)
{
	for (int i = 0; i < map->entry_count; i++)
		control_operation(map, dialog, o, &map->entries[i], DCT_UPDATE_STATUS);
}



//============================================================
//  find_entry
//============================================================

static datamap_entry *find_entry(datamap *map, int dlgitem)
{
	for (int i = 0; i < map->entry_count; i++)
		if (map->entries[i].dlgitem == dlgitem)
			return &map->entries[i];

	// should not reach here
	printf("Datamap.cpp line __LINE__ couldn't find an entry\n");
	return NULL;
}



//============================================================
//  get_control_type
//============================================================

static control_type get_control_type(HWND hwnd)
{
	control_type type;
	TCHAR class_name[256];

	GetClassName(hwnd, class_name, std::size(class_name));
	if (!_tcscmp(class_name, WC_BUTTON))
		type = CT_BUTTON;
	else if (!_tcscmp(class_name, WC_STATIC))
		type = CT_STATIC;
	else if (!_tcscmp(class_name, WC_EDIT))
		type = CT_EDIT;
	else if (!_tcscmp(class_name, WC_COMBOBOX))
		type = CT_COMBOBOX;
	else if (!_tcscmp(class_name, TRACKBAR_CLASS))
		type = CT_TRACKBAR;
	else if (!_tcscmp(class_name, WC_LISTVIEW))
		type = CT_LISTVIEW;
	else
		type = CT_UNKNOWN;

	return type;
}



//============================================================
//  is_control_displayonly
//============================================================

static BOOL is_control_displayonly(HWND hwnd)
{
	BOOL displayonly = false;
	switch(get_control_type(hwnd))
	{
		case CT_STATIC:
			displayonly = true;
			break;

		case CT_EDIT:
			displayonly = (GetWindowLong(hwnd, GWL_STYLE) & ES_READONLY) ? true : false;
			break;

		default:
			displayonly = false;
			break;
	}

	if (!IsWindowEnabled(hwnd))
		displayonly = true;
	return displayonly;
}



//============================================================
//  broadcast_changes
//============================================================

static void broadcast_changes(datamap *map, HWND dialog, windows_options *o, datamap_entry *entry, const char *option_name)
{
	HWND other_control;
	const char *that_option_name;

	for (int i = 0; i < map->entry_count; i++)
	{
		// search for an entry with the same option_name, but is not the exact
		// same entry
		that_option_name = map->entries[i].option_name;
		if (map->entries[i].option_name && (&map->entries[i] != entry) && !strcmp(that_option_name, option_name))
		{
			// we've found a control sharing the same option; populate it
			other_control = GetDlgItem(dialog, map->entries[i].dlgitem);
			if (other_control)
				populate_control(map, other_control, o, &map->entries[i], that_option_name);
		}
	}
}



//============================================================
//  control_operation
//============================================================

static int control_operation(datamap *map, HWND dialog, windows_options *o, datamap_entry *entry, datamap_callback_type callback_type)
{
	static const datamap_default_callback default_callbacks[DCT_COUNT] =
	{
		read_control,
		populate_control,
		NULL
	};
	int result = 0;
	const char *option_name;
	char option_name_buffer[64];
	char option_value[1024] = {0, };

	HWND hwnd = GetDlgItem(dialog, entry->dlgitem);
	if (hwnd)
	{
		// don't do anything if we're reading from a display-only control
		if ((callback_type != DCT_READ_CONTROL) || !is_control_displayonly(hwnd))
		{
			// figure out the option_name
			if (entry->get_option_name)
			{
				option_name_buffer[0] = '\0';
				entry->get_option_name(map, dialog, hwnd, option_name_buffer, std::size(option_name_buffer));
				option_name = option_name_buffer;
			}
			else
				option_name = entry->option_name;

			// if reading, get the option value, solely for the purposes of comparison
			if ((callback_type == DCT_READ_CONTROL) && option_name)
				snprintf(option_value, std::size(option_value), "%s", o->value(option_name));

			if (entry->callbacks[callback_type])
			{
				// use custom callback
				result = entry->callbacks[callback_type](map, dialog, hwnd, o, option_name);
			}
			else
			if (default_callbacks[callback_type] && option_name)
			{
				// use default callback
				default_callbacks[callback_type](map, hwnd, o, entry, option_name);
			}

			// the result is dependent on the type of control
			switch(callback_type)
			{
				case DCT_READ_CONTROL:
					// For callbacks that returned TRUE, do not broadcast_changes.
					if (!result)
					{
						// do a check to see if the control changed
						result = (option_name) && (strcmp(option_value, o->value(option_name)) != 0);
						if (result)
						{
							// the value has changed; we may need to broadcast the change
							broadcast_changes(map, dialog, o, entry, option_name);
						}
					}
					break;

				default:
					// do nothing
					break;
			}
		}
	}
	return result;
}



//============================================================
//  trackbar_value_from_position
//============================================================

static float trackbar_value_from_position(datamap_entry *entry, int position)
{
	float position_f = position;

	if (entry->use_trackbar_options)
		position_f = (position_f * entry->trackbar_increments) + entry->trackbar_min;

	return position_f;
}



//============================================================
//  trackbar_position_from_value
//============================================================

static int trackbar_position_from_value(datamap_entry *entry, float value)
{
	if (entry->use_trackbar_options)
		value = floor((value - entry->trackbar_min) / entry->trackbar_increments + 0.5);

	return (int) value;
}



//============================================================
//  read_control
//============================================================

static void read_control(datamap *map, HWND hwnd, windows_options *o, datamap_entry *entry, const char *option_name)
{
	BOOL bool_value = 0;
	int int_value = 0;
	float float_value = 0;
	const char *string_value;
	int selected_index = 0;
	int trackbar_pos = 0;
	// use default read value behavior
	switch(get_control_type(hwnd))
	{
		case CT_BUTTON:
			//assert(entry->type == DM_BOOL);
			bool_value = Button_GetCheck(hwnd);
			emu_set_value(o, option_name, bool_value);
			break;

		case CT_COMBOBOX:
			selected_index = ComboBox_GetCurSel(hwnd);
			if (selected_index >= 0)
			{
				switch(entry->type)
				{
					case DM_INT:
						int_value = (int) ComboBox_GetItemData(hwnd, selected_index);
						emu_set_value(o, option_name, int_value);
						break;

					case DM_STRING:
					{
						string_value = (const char *) ComboBox_GetItemData(hwnd, selected_index);
						string svalue = string_value ? string(string_value) : "";
						emu_set_value(o, option_name, svalue);
						break;
					}

					default:
						break;
				}
			}
			break;

		case CT_TRACKBAR:
			trackbar_pos = SendMessage(hwnd, TBM_GETPOS, 0, 0);
			float_value = trackbar_value_from_position(entry, trackbar_pos);
			switch(entry->type)
			{
				case DM_INT:
					int_value = (int) float_value;
					emu_set_value(o, option_name, int_value);
					break;

				case DM_FLOAT:
					emu_set_value(o, option_name, float_value);
					break;

				default:
					break;
			}
			break;

		case CT_EDIT:
			// NYI
			break;

		case CT_STATIC:
		case CT_LISTVIEW:
		case CT_UNKNOWN:
			// non applicable
			break;
	}
}



//============================================================
//  populate_control
//============================================================

static void populate_control(datamap *map, HWND hwnd, windows_options *o, datamap_entry *entry, const char *option_name)
{
	int i = 0;
	BOOL bool_value = 0;
	int int_value = 0;
	float float_value = 0;
	int selected_index = 0;
	char buffer[128];
	int trackbar_range = 0;
	int trackbar_pos = 0;
	double trackbar_range_d = 0;
	string name = string(option_name);
	string c = emu_get_value(o, name);

	// use default populate control value
	switch(get_control_type(hwnd))
	{
		case CT_BUTTON:
			bool_value = (c == "0") ? 0 : 1;
			Button_SetCheck(hwnd, bool_value);
			break;

		case CT_EDIT:
		case CT_STATIC:
			switch(entry->type)
			{
				case DM_STRING:
					break;

				case DM_INT:
					if (entry->int_format)
					{
						sscanf(c.c_str(), "%d", &int_value);
						snprintf(buffer, std::size(buffer), entry->int_format, int_value);
						c = string(buffer);
					}
					break;

				case DM_FLOAT:
					if (entry->float_format)
					{
						sscanf(c.c_str(), "%f", &float_value);
						snprintf(buffer, std::size(buffer), entry->float_format, float_value);
						c = string(buffer);
					}
					break;

				default:
					c.clear();
					break;
			}
			if (c.empty())
				c = "";
			win_set_window_text_utf8(hwnd, c.c_str());
			break;

		case CT_COMBOBOX:
			selected_index = 0;
			switch(entry->type)
			{
				case DM_INT:
					sscanf(c.c_str(), "%d", &int_value);
					for (i = 0; i < ComboBox_GetCount(hwnd); i++)
					{
						if (int_value == (int) ComboBox_GetItemData(hwnd, i))
						{
							selected_index = i;
							break;
						}
					}
					break;

				case DM_STRING:
					for (i = 0; i < ComboBox_GetCount(hwnd); i++)
					{
						name = (const char*) ComboBox_GetItemData(hwnd, i);
						if (c == name)
						{
							selected_index = i;
							break;
						}
					}
					break;

				default:
					break;
			}
			ComboBox_SetCurSel(hwnd, selected_index);
			break;

		case CT_TRACKBAR:
			// do we need to set the trackbar options?
/*          if (!entry->use_trackbar_options)
            {
                switch(options_get_range_type(o, option_name))
                {
                    case OPTION_RANGE_NONE:
                        // do nothing
                        break;

                    case OPTION_RANGE_INT:
                        options_get_range_int(o, option_name, &minval_int, &maxval_int);
                        entry->use_trackbar_options = TRUE;
                        entry->trackbar_min = minval_int;
                        entry->trackbar_max = maxval_int;
                        entry->trackbar_increments = 1;
                        break;

                    case OPTION_RANGE_FLOAT:
                        options_get_range_float(o, option_name, &minval_float, &maxval_float);
                        entry->use_trackbar_options = TRUE;
                        entry->trackbar_min = minval_float;
                        entry->trackbar_max = maxval_float;
                        entry->trackbar_increments = (float)0.05;
                        break;
                }
            }
        */

			// do we specify default options for this control?  if so, we need to specify
			// the range
			if (entry->use_trackbar_options)
			{
				trackbar_range_d = floor(((entry->trackbar_max - entry->trackbar_min) / entry->trackbar_increments) + 0.5);
				trackbar_range = (int) trackbar_range_d;
				SendMessage(hwnd, TBM_SETRANGEMIN, (WPARAM) FALSE, (LPARAM) 0);
				SendMessage(hwnd, TBM_SETRANGEMAX, (WPARAM) FALSE, (LPARAM) trackbar_range);
			}

			switch(entry->type)
			{
				case DM_INT:
					sscanf(c.c_str(), "%d", &int_value);
					trackbar_pos = trackbar_position_from_value(entry, int_value);
					break;

				case DM_FLOAT:
					sscanf(c.c_str(), "%f", &float_value);
					trackbar_pos = trackbar_position_from_value(entry, float_value);
					break;

				default:
					trackbar_pos = 0;
					break;
			}
			SendMessage(hwnd, TBM_SETPOS, (WPARAM) TRUE, (LPARAM) trackbar_pos);
			break;

		case CT_LISTVIEW:
		case CT_UNKNOWN:
			// non applicable
			break;
	}
}

#if 0
// Return a string from a float value with trailing zeros removed.
static char *tztrim(float float_value)
{
	static char tz_string[20];
	char float_string[20];
	int i = 0;

	sprintf(float_string, "%f", float_value);

	char* ptr = float_string;

	// Copy before the '.'
	while (*ptr && *ptr != '.')
		tz_string[i++] = *ptr++;

	// add the '.' and the next digit
	if (*ptr == '.')
	{
		tz_string[i++] = *ptr++;
		tz_string[i++] = *ptr++;
	}
	// Keep copying until we hit a '0'
	while (*ptr && *ptr != '0')
		tz_string[i++] = *ptr++;

	// Null terminate
	tz_string[i] = '\0';
	return tz_string;
}
#endif

