// For licensing and usage information, read docs/winui_license.txt
//************************************************************************************************
// MASTER
//
//  newui.c - This is the NEWUI Windows dropdown menu system
//
//  known bugs:
//  -  Unable to modify keyboard or joystick. Last known to be working in 0.158 .
//     Need to use the default UI.
//
//
//************************************************************************************************

// Set minimum windows version to XP
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x501


// standard windows headers
#include "newui.h"

#include <shellapi.h>


enum _win_file_dialog_type
{
	WIN_FILE_DIALOG_OPEN = 1,
	WIN_FILE_DIALOG_SAVE
};
typedef enum _win_file_dialog_type win_file_dialog_type;

typedef struct _win_open_file_name win_open_file_name;
struct _win_open_file_name
{
	win_file_dialog_type  type;                 // type of file dialog
	HWND                  owner;                // owner of the dialog
	HINSTANCE             instance;             // instance
	const char *          filter;               // pipe char ("|") delimited strings
	DWORD                 filter_index;         // index into filter
	char                  filename[MAX_PATH];   // filename buffer
	const char *          initial_directory;    // initial directory for dialog
	DWORD                 flags;                // standard flags
	LPARAM                custom_data;          // custom data for dialog hook
	LPOFNHOOKPROC         hook;                 // custom dialog hook
	LPCTSTR               template_name;        // custom dialog template
};


typedef struct _dialog_box dialog_box;

struct dialog_layout
{
	short label_width;
	short combo_width;
};

typedef void (*dialog_itemstoreval)(void *param, int val);
typedef void (*dialog_itemchangedproc)(dialog_box *dialog, HWND dlgitem, void *changed_param);
typedef void (*dialog_notification)(dialog_box *dialog, HWND dlgwnd, NMHDR *notification, void *param);

#ifdef UNICODE
#define win_dialog_tcsdup win_dialog_wcsdup
#else
#define win_dialog_tcsdup win_dialog_strdup
#endif

#define SEQWM_SETFOCUS  (WM_APP + 0)
#define SEQWM_KILLFOCUS (WM_APP + 1)

enum
{
	TRIGGER_INITDIALOG = 1,
	TRIGGER_APPLY = 2,
	TRIGGER_CHANGED = 4
};

typedef LRESULT (*trigger_function)(dialog_box *dialog, HWND dlgwnd, UINT message, WPARAM wparam, LPARAM lparam);

struct dialog_info_trigger
{
	struct dialog_info_trigger *next;
	WORD dialog_item;
	WORD trigger_flags;
	UINT message;
	WPARAM wparam;
	LPARAM lparam;
	void (*storeval)(void *param, int val);
	void *storeval_param;
	trigger_function trigger_proc;
};

struct dialog_object_pool
{
	HGDIOBJ objects[16];
};

struct _dialog_box
{
	HGLOBAL handle;
	size_t handle_size;
	struct dialog_info_trigger *trigger_first;
	struct dialog_info_trigger *trigger_last;
	WORD item_count;
	WORD size_x, size_y;
	WORD pos_x, pos_y;
	WORD cursize_x, cursize_y;
	WORD home_y;
	DWORD style;
	int combo_string_count;
	int combo_default_value;
	//object_pool *mempool;
	struct dialog_object_pool *objpool;
	const struct dialog_layout *layout;

	// singular notification callback; hack
	UINT notify_code;
	dialog_notification notify_callback;
	void *notify_param;
};

enum _seqselect_state
{
	SEQSELECT_STATE_NOT_POLLING,
	SEQSELECT_STATE_POLLING,
	SEQSELECT_STATE_POLLING_COMPLETE
};
typedef enum _seqselect_state seqselect_state;

// this is the structure that gets associated with each input_seq edit box
typedef struct _seqselect_info seqselect_info;
struct _seqselect_info
{
	WNDPROC oldwndproc;
	ioport_field *field; // pointer to the field
	ioport_field::user_settings settings; // the new settings
	input_seq *code; // the input_seq within settings
	WORD pos;
	BOOL is_analog;
	seqselect_state poll_state;
};



//============================================================
//  PARAMETERS
//============================================================

#define DIM_VERTICAL_SPACING    3
#define DIM_HORIZONTAL_SPACING  5
#define DIM_NORMAL_ROW_HEIGHT   10
#define DIM_COMBO_ROW_HEIGHT    12
#define DIM_SLIDER_ROW_HEIGHT   18
#define DIM_BUTTON_ROW_HEIGHT   12
#define DIM_EDIT_WIDTH          120
#define DIM_BUTTON_WIDTH        50
#define DIM_ADJUSTER_SCR_WIDTH  12
#define DIM_ADJUSTER_HEIGHT     12
#define DIM_SCROLLBAR_WIDTH     10
#define DIM_BOX_VERTSKEW        -3

#define DLGITEM_BUTTON          ((const WCHAR *) dlgitem_button)
#define DLGITEM_EDIT            ((const WCHAR *) dlgitem_edit)
#define DLGITEM_STATIC          ((const WCHAR *) dlgitem_static)
#define DLGITEM_LISTBOX         ((const WCHAR *) dlgitem_listbox)
#define DLGITEM_SCROLLBAR       ((const WCHAR *) dlgitem_scrollbar)
#define DLGITEM_COMBOBOX        ((const WCHAR *) dlgitem_combobox)

#define DLGTEXT_OK              "OK"
#define DLGTEXT_APPLY           "Apply"
#define DLGTEXT_CANCEL          "Cancel"

#define FONT_SIZE               8
#define FONT_FACE               L"Arial"

#define SCROLL_DELTA_LINE       10
#define SCROLL_DELTA_PAGE       100

#define LOG_WINMSGS             0
#define DIALOG_STYLE            WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION | DS_SETFONT
#define MAX_DIALOG_HEIGHT       200



//============================================================
//  LOCAL VARIABLES
//============================================================

static running_machine *Machine;         // HACK - please fix

static double pixels_to_xdlgunits;
static double pixels_to_ydlgunits;

static const struct dialog_layout default_layout = { 80, 140 };
static const WORD dlgitem_button[] = { 0xFFFF, 0x0080 };
static const WORD dlgitem_edit[] = { 0xFFFF, 0x0081 };
static const WORD dlgitem_static[] = { 0xFFFF, 0x0082 };
static const WORD dlgitem_listbox[] = { 0xFFFF, 0x0083 };
static const WORD dlgitem_scrollbar[] = { 0xFFFF, 0x0084 };
static const WORD dlgitem_combobox[] = { 0xFFFF, 0x0085 };
static int joystick_menu_setup = 0;
static char state_filename[MAX_PATH];
static void add_filter_entry(std::string &dest, const char *description, const char *extensions);
static std::map<std::string,std::string> slmap;
struct slot_data { std::string slotname; std::string optname; };
static std::map<int, slot_data> slot_map;


//============================================================
//  PROTOTYPES
//============================================================

static void dialog_prime(dialog_box *di);
static int dialog_write_item(dialog_box *di, DWORD style, short x, short y, short width, short height, const char *str, const WCHAR *class_name, WORD *id);



//============================================================
//  PARAMETERS
//============================================================

#define ID_FRAMESKIP_0   10000
#define ID_DEVICE_0      11000
#define ID_JOYSTICK_0    12000
#define ID_VIDEO_VIEW_0  14000
#define MAX_JOYSTICKS    (8)

enum
{
	DEVOPTION_OPEN,
	DEVOPTION_CREATE,
	DEVOPTION_CLOSE,
	DEVOPTION_ITEM,
	DEVOPTION_CASSETTE_PLAYRECORD,
	DEVOPTION_CASSETTE_STOPPAUSE,
	DEVOPTION_CASSETTE_PLAY,
	DEVOPTION_CASSETTE_RECORD,
	DEVOPTION_CASSETTE_REWIND,
	DEVOPTION_CASSETTE_FASTFORWARD,
	DEVOPTION_CASSETTE_MOTOR,
	DEVOPTION_CASSETTE_SOUND,
	DEVOPTION_MAX
};


//========================================================================
//  LOCAL STRING FUNCTIONS (these require free after being called)
//========================================================================

static WCHAR *ui_wstring_from_utf8(const char *utf8string)
{
	int char_count;
	WCHAR *result;

	// convert MAME string (UTF-8) to UTF-16
	char_count = MultiByteToWideChar(CP_UTF8, 0, utf8string, -1, nullptr, 0);
	result = (WCHAR *)malloc(char_count * sizeof(*result));
	if (result != nullptr)
		MultiByteToWideChar(CP_UTF8, 0, utf8string, -1, result, char_count);
 
	return result;
}

static char *ui_utf8_from_wstring(const WCHAR *wstring)
{
	int char_count;
	char *result;

	// convert UTF-16 to MAME string (UTF-8)
	char_count = WideCharToMultiByte(CP_UTF8, 0, wstring, -1, nullptr, 0, nullptr, nullptr);
	result = (char *)malloc(char_count * sizeof(*result));
	if (result != nullptr)
		WideCharToMultiByte(CP_UTF8, 0, wstring, -1, result, char_count, nullptr, nullptr);
	return result;
}



//============================================================
//  win_get_file_name_dialog - sanitize all of the ugliness
//  in invoking GetOpenFileName() and GetSaveFileName()
//     called from win_file_dialog, state_dialog
//============================================================

static BOOL win_get_file_name_dialog(win_open_file_name *ofn)
{
	BOOL result = false;
	BOOL dialog_result;
	OPENFILENAME os_ofn;
	LPTSTR t_filter = NULL;
	LPTSTR t_file = NULL;
	DWORD t_file_size = 0;
	LPTSTR t_initial_directory = NULL;
	LPTSTR buffer;
	char *utf8_file;
	int i;

	// do we have to translate the filter?
	if (ofn->filter)
	{
		buffer = ui_wstring_from_utf8(ofn->filter);
		if (!buffer)
			goto done;

		// convert a pipe-char delimited string into a NUL delimited string
		t_filter = (LPTSTR) alloca((_tcslen(buffer) + 2) * sizeof(*t_filter));
		for (i = 0; buffer[i] != '\0'; i++)
			t_filter[i] = (buffer[i] != '|') ? buffer[i] : '\0';
		t_filter[i++] = '\0';
		t_filter[i++] = '\0';
		free(buffer);
	}

	// do we need to translate the file parameter?
	if (ofn->filename)
	{
		buffer = ui_wstring_from_utf8(ofn->filename);
		if (!buffer)
			goto done;

		t_file_size = ((_tcslen(buffer) + 1) > MAX_PATH) ? (_tcslen(buffer) + 1) : MAX_PATH;
		t_file = (LPTSTR) alloca(t_file_size * sizeof(*t_file));
		_tcscpy(t_file, buffer);
		free(buffer);
	}

	// do we need to translate the initial directory?
	if (ofn->initial_directory)
	{
		t_initial_directory = ui_wstring_from_utf8(ofn->initial_directory);
		if (t_initial_directory == NULL)
			goto done;
	}

	// translate our custom structure to a native Win32 structure
	memset(&os_ofn, 0, sizeof(os_ofn));
	os_ofn.lStructSize = sizeof(OPENFILENAME);
	os_ofn.hwndOwner = ofn->owner;
	os_ofn.hInstance = ofn->instance;
	os_ofn.lpstrFilter = t_filter;
	os_ofn.nFilterIndex = ofn->filter_index;
	os_ofn.lpstrFile = t_file;
	os_ofn.nMaxFile = t_file_size;
	os_ofn.lpstrInitialDir = t_initial_directory;
	os_ofn.Flags = ofn->flags;
	os_ofn.lCustData = ofn->custom_data;
	os_ofn.lpfnHook = ofn->hook;
	os_ofn.lpTemplateName = ofn->template_name;

	// invoke the correct Win32 call
	switch(ofn->type)
	{
		case WIN_FILE_DIALOG_OPEN:
			dialog_result = GetOpenFileName(&os_ofn);
			break;

		case WIN_FILE_DIALOG_SAVE:
			dialog_result = GetSaveFileName(&os_ofn);
			break;

		default:
			// should not reach here
			dialog_result = false;
			break;
	}

	// copy data out
	ofn->filter_index = os_ofn.nFilterIndex;
	ofn->flags = os_ofn.Flags;

	// copy file back out into passed structure
	if (t_file)
	{
		utf8_file = ui_utf8_from_wstring(t_file);
		if (!utf8_file)
			goto done;

		snprintf(ofn->filename, ARRAY_LENGTH(ofn->filename), "%s", utf8_file);
		free(utf8_file);
	}

	// we've completed the process
	result = dialog_result;

done:
	if (t_initial_directory)
		free(t_initial_directory);
	return result;
}



//============================================================
//  win_scroll_window
//    called from dialog_proc
//============================================================

static void win_scroll_window(HWND window, WPARAM wparam, int scroll_bar, int scroll_delta_line)
{
	SCROLLINFO si;
	int scroll_pos;

	// retrieve vital info about the scroll bar
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
	GetScrollInfo(window, scroll_bar, &si);

	scroll_pos = si.nPos;

	// change scroll_pos in accordance with this message
	switch(LOWORD(wparam))
	{
		case SB_BOTTOM:
			scroll_pos = si.nMax;
			break;
		case SB_LINEDOWN:
			scroll_pos += scroll_delta_line;
			break;
		case SB_LINEUP:
			scroll_pos -= scroll_delta_line;
			break;
		case SB_PAGEDOWN:
			scroll_pos += scroll_delta_line;
			break;
		case SB_PAGEUP:
			scroll_pos -= scroll_delta_line;
			break;
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			scroll_pos = HIWORD(wparam);
			break;
		case SB_TOP:
			scroll_pos = si.nMin;
			break;
	}

	// max out the scroll bar value
	if (scroll_pos < si.nMin)
		scroll_pos = si.nMin;
	else if (scroll_pos > (si.nMax - si.nPage))
		scroll_pos = (si.nMax - si.nPage);

	// if the value changed, set the scroll position
	if (scroll_pos != si.nPos)
	{
		SetScrollPos(window, scroll_bar, scroll_pos, true);
		ScrollWindowEx(window, 0, si.nPos - scroll_pos, NULL, NULL, NULL, NULL, SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
	}
}



//============================================================
//  win_append_menu_utf8
//    create a menu item
//============================================================

static BOOL win_append_menu_utf8(HMENU menu, UINT flags, UINT_PTR id, const char *item)
{
	BOOL result;
	const TCHAR *t_item = (const TCHAR*)item;
	TCHAR *t_str = NULL;

	// only convert string when it's not a bitmap
	if (!(flags & MF_BITMAP) && item)
	{
		t_str = ui_wstring_from_utf8(item);
		t_item = t_str;
	}

	result = AppendMenu(menu, flags, id, t_item);

	if (t_str)
		free(t_str);

	return result;
}



//============================================================
//  call_windowproc
//    called from adjuster_sb_wndproc, seqselect_wndproc
//============================================================

static LRESULT call_windowproc(WNDPROC wndproc, HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	LRESULT rc;
	if (IsWindowUnicode(hwnd))
		rc = CallWindowProcW(wndproc, hwnd, msg, wparam, lparam);
	else
		rc = CallWindowProcA(wndproc, hwnd, msg, wparam, lparam);
	return rc;
}



//==========================================================================
//  dialog_write
//    called from dialog_write_string, win_dialog_init, dialog_write_item
//==========================================================================

static int dialog_write(struct _dialog_box *di, const void *ptr, size_t sz, int align)
{
	HGLOBAL newhandle;
	size_t base;
	UINT8 *mem;
	UINT8 *mem2;

	if (!di->handle)
	{
		newhandle = GlobalAlloc(GMEM_ZEROINIT, sz);
		base = 0;
	}
	else
	{
		base = di->handle_size;
		base += align - 1;
		base -= base % align;
		newhandle = GlobalReAlloc(di->handle, base + sz, GMEM_ZEROINIT);
		if (!newhandle)
		{
			newhandle = GlobalAlloc(GMEM_ZEROINIT, base + sz);
			if (newhandle)
			{
				mem = (UINT8*)GlobalLock(di->handle);
				mem2 = (UINT8*)GlobalLock(newhandle);
				memcpy(mem2, mem, base);
				GlobalUnlock(di->handle);
				GlobalUnlock(newhandle);
				GlobalFree(di->handle);
			}
		}
	}
	if (!newhandle)
		return 1;

	mem = (UINT8*)GlobalLock(newhandle);
	memcpy(mem + base, ptr, sz);
	GlobalUnlock(newhandle);
	di->handle = newhandle;
	di->handle_size = base + sz;
	return 0;
}



//============================================================
//  dialog_write_string
//    called from win_dialog_init, dialog_write_item
//============================================================

static int dialog_write_string(dialog_box *di, const WCHAR *str)
{
	if (!str)
		str = L"";
	return dialog_write(di, str, (wcslen(str) + 1) * sizeof(WCHAR), 2);
}




//============================================================
//  win_dialog_exit
//    called from win_dialog_init, calc_dlgunits_multiple, change_device, and all customise_input functions
//============================================================

static void win_dialog_exit(dialog_box *dialog)
{
	int i;
	struct dialog_object_pool *objpool;

	if (!dialog)
		printf("No dialog passed to win_dialog_exit\n");
	assert(dialog);

	objpool = dialog->objpool;
	if (objpool)
	{
		for (i = 0; i < ARRAY_LENGTH(objpool->objects); i++)
			DeleteObject(objpool->objects[i]);
	}

	if (dialog->handle)
		GlobalFree(dialog->handle);

	free(dialog);
}



//===========================================================================
//  win_dialog_init
//    called from calc_dlgunits_multiple, and all customise_input functions
//===========================================================================

dialog_box *win_dialog_init(const char *title, const struct dialog_layout *layout)
{
	struct _dialog_box *di;
	DLGTEMPLATE dlg_template;
	WORD w[2];
	WCHAR *w_title;
	int rc;

	// use default layout if not specified
	if (!layout)
		layout = &default_layout;

	// create the dialog structure
	di = (_dialog_box *)malloc(sizeof(struct _dialog_box));
	if (!di)
		goto error;
	memset(di, 0, sizeof(*di));

	di->layout = layout;

	memset(&dlg_template, 0, sizeof(dlg_template));
	dlg_template.style = di->style = DIALOG_STYLE;
	dlg_template.x = 10;
	dlg_template.y = 10;
	if (dialog_write(di, &dlg_template, sizeof(dlg_template), 4))
		goto error;

	w[0] = 0;
	w[1] = 0;
	if (dialog_write(di, w, sizeof(w), 2))
		goto error;

	w_title = ui_wstring_from_utf8(title);
	rc = dialog_write_string(di, w_title);
	free(w_title);
	if (rc)
		goto error;

	// set the font, if necessary
	if (di->style & DS_SETFONT)
	{
		w[0] = FONT_SIZE;
		if (dialog_write(di, w, sizeof(w[0]), 2))
			goto error;
		if (dialog_write_string(di, FONT_FACE))
			goto error;
	}

	return di;

error:
	if (di)
		win_dialog_exit(di);
	return NULL;
}


//============================================================
//  compute_dlgunits_multiple
//    called from dialog_scrollbar_init
//============================================================

static void calc_dlgunits_multiple(void)
{
	dialog_box *dialog = NULL;
	short offset_x = 2048;
	short offset_y = 2048;
	const char *wnd_title = "Foo";
	WORD id;
	HWND dlg_window = NULL;
	HWND child_window;
	RECT r;

	if ((pixels_to_xdlgunits == 0) || (pixels_to_ydlgunits == 0))
	{
		// create a bogus dialog
		dialog = win_dialog_init(NULL, NULL);
		if (!dialog)
			goto done;

		if (dialog_write_item(dialog, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, offset_x, offset_y, wnd_title, DLGITEM_STATIC, &id))
			goto done;

		dialog_prime(dialog);
		dlg_window = CreateDialogIndirectParam(NULL, (const DLGTEMPLATE*)dialog->handle, NULL, NULL, 0);
		child_window = GetDlgItem(dlg_window, id);

		GetWindowRect(child_window, &r);
		pixels_to_xdlgunits = (double)(r.right - r.left) / offset_x;
		pixels_to_ydlgunits = (double)(r.bottom - r.top) / offset_y;

done:
		if (dialog)
			win_dialog_exit(dialog);
		if (dlg_window)
			DestroyWindow(dlg_window);
	}
}



//============================================================
//  dialog_trigger
//    called from dialog_proc, file_dialog_hook
//============================================================

static void dialog_trigger(HWND dlgwnd, WORD trigger_flags)
{
	LRESULT result;
	HWND dialog_item;
	struct _dialog_box *di;
	struct dialog_info_trigger *trigger;
	LONG_PTR l;

	l = GetWindowLongPtr(dlgwnd, GWLP_USERDATA);
	di = (struct _dialog_box *) l;
	if (!di)
		printf("Unexpected result of di in dialog_trigger\n");
	assert(di);
	for (trigger = di->trigger_first; trigger; trigger = trigger->next)
	{
		if (trigger->trigger_flags & trigger_flags)
		{
			if (trigger->dialog_item)
				dialog_item = GetDlgItem(dlgwnd, trigger->dialog_item);
			else
				dialog_item = dlgwnd;
			if (!dialog_item)
				printf("Unexpected result of dialog_item in dialog_trigger\n");
			assert(dialog_item);
			result = 0;

			if (trigger->message)
				result = SendMessage(dialog_item, trigger->message, trigger->wparam, trigger->lparam);
			if (trigger->trigger_proc)
				result = trigger->trigger_proc(di, dialog_item, trigger->message, trigger->wparam, trigger->lparam);

			if (trigger->storeval)
				trigger->storeval(trigger->storeval_param, result);
		}
	}
}

//============================================================
//  dialog_proc
//    called from win_dialog_runmodal
//============================================================

static INT_PTR CALLBACK dialog_proc(HWND dlgwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	INT_PTR handled = true;
	std::string buf;
	WORD command;

	switch(msg)
	{
		case WM_INITDIALOG:
			SetWindowLongPtr(dlgwnd, GWLP_USERDATA, (LONG_PTR) lparam);
			dialog_trigger(dlgwnd, TRIGGER_INITDIALOG);
			break;

		case WM_COMMAND:
			command = LOWORD(wparam);

			buf = win_get_window_text_utf8((HWND) lparam);
			if (!strcmp(buf.c_str(), DLGTEXT_OK))
				command = IDOK;
			else if (!strcmp(buf.c_str(), DLGTEXT_CANCEL))
				command = IDCANCEL;
			else
				command = 0;

			switch(command)
			{
				case IDOK:
					dialog_trigger(dlgwnd, TRIGGER_APPLY);
					// fall through

				case IDCANCEL:
					EndDialog(dlgwnd, 0);
					break;

				default:
					handled = false;
					break;
			}
			break;

		case WM_SYSCOMMAND:
			if (wparam == SC_CLOSE)
				EndDialog(dlgwnd, 0);
			else
				handled = false;

			break;

		case WM_VSCROLL:
			if (lparam)
				// this scroll message came from an actual scroll bar window;
				// pass it on
				SendMessage((HWND) lparam, msg, wparam, lparam);
			else
				// scroll the dialog
				win_scroll_window(dlgwnd, wparam, SB_VERT, SCROLL_DELTA_LINE);

			break;

		default:
			handled = false;
			break;
	}
	return handled;
}



//=========================================================================================================================================================================================
//  dialog_write_item
//    called from calc_dlgunits_multiple, win_dialog_add_active_combobox, win_dialog_add_adjuster, dialog_add_single_seqselect, win_dialog_add_portselect, win_dialog_add_standard_buttons
//=========================================================================================================================================================================================

static int dialog_write_item(dialog_box *di, DWORD style, short x, short y, short width, short height, const char *str, const WCHAR *class_name, WORD *id)
{
	DLGITEMTEMPLATE item_template;
	UINT class_name_length;
	WORD w;
	WCHAR *w_str;
	int rc;

	memset(&item_template, 0, sizeof(item_template));
	item_template.style = style;
	item_template.x = x;
	item_template.y = y;
	item_template.cx = width;
	item_template.cy = height;
	item_template.id = di->item_count + 1;

	if (dialog_write(di, &item_template, sizeof(item_template), 4))
		return 1;

	if (*class_name == 0xffff)
		class_name_length = 4;
	else
		class_name_length = (wcslen(class_name) + 1) * sizeof(WCHAR);
	if (dialog_write(di, class_name, class_name_length, 2))
		return 1;

	w_str = str ? ui_wstring_from_utf8(str) : NULL;
	rc = dialog_write_string(di, w_str);
	if (w_str)
		free(w_str);
	if (rc)
		return 1;

	w = 0;
	if (dialog_write(di, &w, sizeof(w), 2))
		return 1;

	di->item_count++;

	if (id)
		*id = di->item_count;
	return 0;
}



//==========================================================================================================================================================
//  dialog_add_trigger
//    called from dialog_add_scrollbar, win_dialog_add_active_combobox, win_dialog_add_combobox_item, win_dialog_add_adjuster, dialog_add_single_seqselect
//==========================================================================================================================================================

static int dialog_add_trigger(struct _dialog_box *di, WORD dialog_item, WORD trigger_flags, UINT message, trigger_function trigger_proc,
	WPARAM wparam, LPARAM lparam, void (*storeval)(void *param, int val), void *storeval_param)
{
	if (!di)
		printf("Unexpected result of di in dialog_add_trigger\n");
	assert(di);
	if (!trigger_flags)
		printf("Unexpected result of trigger_flags in dialog_add_trigger\n");
	assert(trigger_flags);

	dialog_info_trigger *trigger = new(dialog_info_trigger);

	trigger->next = NULL;
	trigger->trigger_flags = trigger_flags;
	trigger->dialog_item = dialog_item;
	trigger->message = message;
	trigger->trigger_proc = trigger_proc;
	trigger->wparam = wparam;
	trigger->lparam = lparam;
	trigger->storeval = storeval;
	trigger->storeval_param = storeval_param;

	if (di->trigger_last)
		di->trigger_last->next = trigger;
	else
		di->trigger_first = trigger;
	di->trigger_last = trigger;
	return 0;
}



//============================================================
//  dialog_scrollbar_init
//    called from dialog_add_scrollbar
//============================================================

static LRESULT dialog_scrollbar_init(dialog_box *dialog, HWND dlgwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	SCROLLINFO si;

	calc_dlgunits_multiple();

	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.nMin  = pixels_to_ydlgunits * 0;
	si.nMax  = pixels_to_ydlgunits * dialog->size_y;
	si.nPage = pixels_to_ydlgunits * MAX_DIALOG_HEIGHT;
	si.fMask = SIF_PAGE | SIF_RANGE;

	SetScrollInfo(dlgwnd, SB_VERT, &si, true);
	return 0;
}



//============================================================
//  dialog_add_scrollbar
//    called from dialog_prime
//============================================================

static int dialog_add_scrollbar(dialog_box *dialog)
{
	if (dialog_add_trigger(dialog, 0, TRIGGER_INITDIALOG, 0, dialog_scrollbar_init, 0, 0, NULL, NULL))
		return 1;

	dialog->style |= WS_VSCROLL;
	return 0;
}



//==============================================================================
//  dialog_prime
//    called from calc_dlgunits_multiple, win_dialog_runmodal, win_file_dialog
//==============================================================================

static void dialog_prime(dialog_box *di)
{
	DLGTEMPLATE *dlg_template;

	if (di->size_y > MAX_DIALOG_HEIGHT)
	{
		di->size_x += DIM_SCROLLBAR_WIDTH;
		dialog_add_scrollbar(di);
	}

	dlg_template = (DLGTEMPLATE *) GlobalLock(di->handle);
	dlg_template->cdit = di->item_count;
	dlg_template->cx = di->size_x;
	dlg_template->cy = (di->size_y > MAX_DIALOG_HEIGHT) ? MAX_DIALOG_HEIGHT : di->size_y;
	dlg_template->style = di->style;
	GlobalUnlock(di->handle);
}



//============================================================
//  dialog_get_combo_value
//    called from win_dialog_add_active_combobox
//============================================================

static LRESULT dialog_get_combo_value(dialog_box *dialog, HWND dialog_item, UINT message, WPARAM wparam, LPARAM lparam)
{
	int idx;
	idx = SendMessage(dialog_item, CB_GETCURSEL, 0, 0);
	if (idx == CB_ERR)
		return 0;
	return SendMessage(dialog_item, CB_GETITEMDATA, idx, 0);
}



//============================================================
//  dialog_get_adjuster_value
//    called from win_dialog_add_adjuster
//============================================================

static LRESULT dialog_get_adjuster_value(dialog_box *dialog, HWND dialog_item, UINT message, WPARAM wparam, LPARAM lparam)
{
	TCHAR buf[32];
	GetWindowText(dialog_item, buf, ARRAY_LENGTH(buf));
	return _ttoi(buf);
}



//====================================================================================================
//  dialog_new_control
//    called from win_dialog_add_active_combobox, win_dialog_add_adjuster, win_dialog_add_portselect
//====================================================================================================

static void dialog_new_control(struct _dialog_box *di, short *x, short *y)
{
	*x = di->pos_x + DIM_HORIZONTAL_SPACING;
	*y = di->pos_y + DIM_VERTICAL_SPACING;
}



//====================================================================================================
//  dialog_finish_control
//    called from win_dialog_add_active_combobox, win_dialog_add_adjuster, win_dialog_add_portselect
//====================================================================================================

static void dialog_finish_control(struct _dialog_box *di, short x, short y)
{
	di->pos_y = y;

	// update dialog size
	if (x > di->size_x)
		di->size_x = x;
	if (y > di->size_y)
		di->size_y = y;
	if (x > di->cursize_x)
		di->cursize_x = x;
	if (y > di->cursize_y)
		di->cursize_y = y;
}



//============================================================
//  dialog_combo_changed
//    called from win_dialog_add_active_combobox
//============================================================

static LRESULT dialog_combo_changed(dialog_box *dialog, HWND dlgitem, UINT message, WPARAM wparam, LPARAM lparam)
{
	dialog_itemchangedproc changed = (dialog_itemchangedproc) wparam;
	changed(dialog, dlgitem, (void *) lparam);
	return 0;
}



//============================================================
//  win_dialog_wcsdup
//    called from win_dialog_add_adjuster (via define)
//============================================================

static WCHAR *win_dialog_wcsdup(dialog_box *dialog, const WCHAR *s)
{
	WCHAR *result = global_alloc_array(WCHAR, wcslen(s) + 1);
	if (result)
		wcscpy(result, s);
	return result;
}



//============================================================
//  win_dialog_add_active_combobox
//    called from win_dialog_add_combobox
//       dialog = handle of dialog box?
//       item_label = name of key
//       default_value = current value of key
//       storeval = function to handle changed key
//       storeval_param = ?
//       changed = ?
//       changed_param = ?
//       rc = return code (1 = failure)
//============================================================

static int win_dialog_add_active_combobox(dialog_box *dialog, const char *item_label, int default_value,
	dialog_itemstoreval storeval, void *storeval_param, dialog_itemchangedproc changed, void *changed_param)
{
	int rc = 1;
	short x;
	short y;

	dialog_new_control(dialog, &x, &y);

	// put name of key on the left
	if (dialog_write_item(dialog, WS_CHILD | WS_VISIBLE | SS_LEFT, x, y, dialog->layout->label_width, DIM_COMBO_ROW_HEIGHT, item_label, DLGITEM_STATIC, NULL))
		goto done;

	y += DIM_BOX_VERTSKEW;

	x += dialog->layout->label_width + DIM_HORIZONTAL_SPACING;
	if (dialog_write_item(dialog, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
			x, y, dialog->layout->combo_width, DIM_COMBO_ROW_HEIGHT * 8, NULL, DLGITEM_COMBOBOX, NULL))
		goto done;
	dialog->combo_string_count = 0;
	dialog->combo_default_value = default_value; // show current value

	// add the trigger invoked when the apply button is pressed
	if (dialog_add_trigger(dialog, dialog->item_count, TRIGGER_APPLY, 0, dialog_get_combo_value, 0, 0, storeval, storeval_param))
		goto done;

	// if appropriate, add the optional changed trigger
	if (changed)
		if (dialog_add_trigger(dialog, dialog->item_count, TRIGGER_INITDIALOG | TRIGGER_CHANGED, 0, dialog_combo_changed, (WPARAM) changed, (LPARAM) changed_param, NULL, NULL))
			goto done;

	x += dialog->layout->combo_width + DIM_HORIZONTAL_SPACING;
	y += DIM_COMBO_ROW_HEIGHT + DIM_VERTICAL_SPACING * 2;

	dialog_finish_control(dialog, x, y);
	rc = 0;

done:
	return rc;
}



//============================================================
//  win_dialog_add_combobox
//    called from customise_switches, customise_analogcontrols
//============================================================

static int win_dialog_add_combobox(dialog_box *dialog, const char *item_label, int default_value, void (*storeval)(void *param, int val), void *storeval_param)
{
	return win_dialog_add_active_combobox(dialog, item_label, default_value, storeval, storeval_param, NULL, NULL);
}



//============================================================
//  win_dialog_add_combobox_item
//    called from customise_switches, customise_analogcontrols
//============================================================

static int win_dialog_add_combobox_item(dialog_box *dialog, const char *item_label, int item_data)
{
	// create our own copy of the string
	size_t newsize = strlen(item_label) + 1;
	wchar_t * t_item_label = new wchar_t[newsize];
	//size_t convertedChars = 0;
	//mbstowcs_s(&convertedChars, t_item_label, newsize, item_label, _TRUNCATE);
	mbstowcs(t_item_label, item_label, newsize);

	if (dialog_add_trigger(dialog, dialog->item_count, TRIGGER_INITDIALOG, CB_ADDSTRING, NULL, 0, (LPARAM) t_item_label, NULL, NULL))
		return 1;
	dialog->combo_string_count++;
	if (dialog_add_trigger(dialog, dialog->item_count, TRIGGER_INITDIALOG, CB_SETITEMDATA, NULL, dialog->combo_string_count-1, (LPARAM) item_data, NULL, NULL))
		return 1;
	if (item_data == dialog->combo_default_value)
	{
		if (dialog_add_trigger(dialog, dialog->item_count, TRIGGER_INITDIALOG, CB_SETCURSEL, NULL, dialog->combo_string_count-1, 0, NULL, NULL))
			return 1;
	}
	return 0;
}



//============================================================
//  adjuster_sb_wndproc
//    called from adjuster_sb_setup
//============================================================

struct adjuster_sb_stuff
{
	WNDPROC oldwndproc;
	int min_value;
	int max_value;
};

static INT_PTR CALLBACK adjuster_sb_wndproc(HWND sbwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	INT_PTR result;
	struct adjuster_sb_stuff *stuff;
	std::string buf;
	CHAR BUF[64];
	HWND dlgwnd, editwnd;
	int value, id;
	LONG_PTR l;

	l = GetWindowLongPtr(sbwnd, GWLP_USERDATA);
	stuff = (struct adjuster_sb_stuff *) l;

	if (msg == WM_VSCROLL)
	{
		id = GetWindowLong(sbwnd, GWL_ID);
		dlgwnd = GetParent(sbwnd);
		editwnd = GetDlgItem(dlgwnd, id - 1);
		buf = win_get_window_text_utf8(editwnd);
		value = atoi(buf.c_str());

		switch(wparam)
		{
			case SB_LINEDOWN:
			case SB_PAGEDOWN:
				value--;
				break;

			case SB_LINEUP:
			case SB_PAGEUP:
				value++;
				break;
		}

		if (value < stuff->min_value)
			value = stuff->min_value;
		else if (value > stuff->max_value)
			value = stuff->max_value;
		_snprintf(BUF, 64, "%d", value);
		win_set_window_text_utf8(editwnd, BUF);
		result = 0;
	}
	else
		result = call_windowproc(stuff->oldwndproc, sbwnd, msg, wparam, lparam);

	return result;
}



//============================================================
//  adjuster_sb_setup
//    called from win_dialog_add_adjuster
//============================================================

static LRESULT adjuster_sb_setup(dialog_box *dialog, HWND sbwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	struct adjuster_sb_stuff *stuff;
	LONG_PTR l;

	stuff = global_alloc(adjuster_sb_stuff);
	if (!stuff)
		return 1;
	stuff->min_value = (WORD) (lparam >> 0);
	stuff->max_value = (WORD) (lparam >> 16);

	l = (LONG_PTR) stuff;
	SetWindowLongPtr(sbwnd, GWLP_USERDATA, l);
	l = (LONG_PTR) adjuster_sb_wndproc;
	l = SetWindowLongPtr(sbwnd, GWLP_WNDPROC, l);
	stuff->oldwndproc = (WNDPROC) l;
	return 0;
}



//============================================================
//  win_dialog_add_adjuster
//    called from customise_analogcontrols
//============================================================

static int win_dialog_add_adjuster(dialog_box *dialog, const char *item_label, int default_value,
	int min_value, int max_value, BOOL is_percentage, dialog_itemstoreval storeval, void *storeval_param)
{
	short x;
	short y;
	TCHAR buf[32];
	TCHAR *s;

	dialog_new_control(dialog, &x, &y);

	if (dialog_write_item(dialog, WS_CHILD | WS_VISIBLE | SS_LEFT, x, y, dialog->layout->label_width, DIM_ADJUSTER_HEIGHT, item_label, DLGITEM_STATIC, NULL))
		goto error;
	x += dialog->layout->label_width + DIM_HORIZONTAL_SPACING;

	y += DIM_BOX_VERTSKEW;

	if (dialog_write_item(dialog, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER,
			x, y, dialog->layout->combo_width - DIM_ADJUSTER_SCR_WIDTH, DIM_ADJUSTER_HEIGHT, NULL, DLGITEM_EDIT, NULL))
		goto error;
	x += dialog->layout->combo_width - DIM_ADJUSTER_SCR_WIDTH;

	_sntprintf(buf, ARRAY_LENGTH(buf), is_percentage ? TEXT("%d%%") : TEXT("%d"), default_value);
	s = win_dialog_tcsdup(dialog, buf);

	if (!s)
		return 1;
	if (dialog_add_trigger(dialog, dialog->item_count, TRIGGER_INITDIALOG, WM_SETTEXT, NULL, 0, (LPARAM) s, NULL, NULL))
		goto error;

	// add the trigger invoked when the apply button is pressed
	if (dialog_add_trigger(dialog, dialog->item_count, TRIGGER_APPLY, 0, dialog_get_adjuster_value, 0, 0, storeval, storeval_param))
		goto error;

	if (dialog_write_item(dialog, WS_CHILD | WS_VISIBLE | WS_TABSTOP | SBS_VERT, x, y, DIM_ADJUSTER_SCR_WIDTH, DIM_ADJUSTER_HEIGHT, NULL, DLGITEM_SCROLLBAR, NULL))
		goto error;
	x += DIM_ADJUSTER_SCR_WIDTH + DIM_HORIZONTAL_SPACING;

	if (dialog_add_trigger(dialog, dialog->item_count, TRIGGER_INITDIALOG, 0, adjuster_sb_setup, 0, MAKELONG(min_value, max_value), NULL, NULL))
		return 1;

	y += DIM_COMBO_ROW_HEIGHT + DIM_VERTICAL_SPACING * 2;

	dialog_finish_control(dialog, x, y);
	return 0;

error:
	return 1;
}




//============================================================
//  get_seqselect_info
//============================================================

static seqselect_info *get_seqselect_info(HWND editwnd)
{
	LONG_PTR lp;
	lp = GetWindowLongPtr(editwnd, GWLP_USERDATA);
	return (seqselect_info *) lp;
}



//=============================================================================
//  seqselect_settext
//    called from seqselect_start_read_from_main_thread, seqselect_setup
//=============================================================================
//#pragma GCC diagnostic push
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunused-value"
#endif

static void seqselect_settext(HWND editwnd)
{
	seqselect_info *stuff;
	std::string seqstring, buf;

	// the basics
	stuff = get_seqselect_info(editwnd);
	if (stuff == NULL)
		return; // this should not happen - need to fix this

	// retrieve the seq name
	seqstring = Machine->input().seq_name(*stuff->code);

	// change the text - avoid calls to SetWindowText() if we can
	buf = win_get_window_text_utf8(editwnd);
	if (buf != seqstring)
		win_set_window_text_utf8(editwnd, seqstring.c_str());

	// reset the selection
	if (GetFocus() == editwnd)
	{
		DWORD start = 0, end = 0;
		SendMessage(editwnd, EM_GETSEL, (WPARAM) (LPDWORD) &start, (LPARAM) (LPDWORD) &end);
		if ((start != 0) || (end != buf.size()))
			SendMessage(editwnd, EM_SETSEL, 0, -1);
	}
}

#ifdef __GNUC__
#pragma GCC diagnostic error "-Wunused-value"
#endif


//============================================================
//  seqselect_start_read_from_main_thread
//    called from seqselect_wndproc
//============================================================

static void seqselect_start_read_from_main_thread(void *param)
{
	seqselect_info *stuff;

	// get the basics
	HWND editwnd = (HWND) param;
	stuff = get_seqselect_info(editwnd);

	// are we currently polling?  if so bail out
	if (stuff->poll_state != SEQSELECT_STATE_NOT_POLLING)
		return;

	// change the state
	stuff->poll_state = SEQSELECT_STATE_POLLING;

	// the Win32 OSD code thinks that we are paused, we need to temporarily
	// unpause ourselves or else we will block
	int pause_count = 0;
	while(Machine->paused() && !winwindow_ui_is_paused(*Machine))
	{
		winwindow_ui_pause(*Machine, false);
		pause_count++;
	}

	// start the polling
	(*Machine).input().seq_poll_start(stuff->is_analog ? ITEM_CLASS_ABSOLUTE : ITEM_CLASS_SWITCH, NULL);

	while(stuff->poll_state == SEQSELECT_STATE_POLLING)
	{
		// poll
		if (Machine->input().seq_poll())       // This never returns anything, so updating keys doesn't work.
		{
			*stuff->code = Machine->input().seq_poll_final();
			seqselect_settext(editwnd);
		}
	}

	// we are no longer polling
	stuff->poll_state = SEQSELECT_STATE_NOT_POLLING;

	// repause the OSD code
	while(pause_count--)
		winwindow_ui_pause(*Machine, true);
}



//============================================================
//  seqselect_stop_read_from_main_thread
//    called from seqselect_wndproc
//============================================================

static void seqselect_stop_read_from_main_thread(void *param)
{
	HWND editwnd;
	seqselect_info *stuff;

	// get the basics
	editwnd = (HWND) param;
	stuff = get_seqselect_info(editwnd);

	// stop the read
	if (stuff->poll_state == SEQSELECT_STATE_POLLING)
		stuff->poll_state = SEQSELECT_STATE_POLLING_COMPLETE;
}



//============================================================
//  seqselect_wndproc
//    called from seqselect_setup
//============================================================

static INT_PTR CALLBACK seqselect_wndproc(HWND editwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	seqselect_info *stuff;
	INT_PTR result = 0;
	BOOL call_baseclass = true;

	stuff = get_seqselect_info(editwnd);

	switch(msg)
	{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_CHAR:
		case WM_KEYUP:
		case WM_SYSKEYUP:
			result = 1;
			call_baseclass = false;
			break;

		case WM_SETFOCUS:
			PostMessage(editwnd, SEQWM_SETFOCUS, 0, 0);
			break;

		case WM_KILLFOCUS:
			PostMessage(editwnd, SEQWM_KILLFOCUS, 0, 0);
			break;

		case SEQWM_SETFOCUS:
			// if we receive the focus, we should start a polling loop
			seqselect_start_read_from_main_thread( (void *) editwnd);
			break;

		case SEQWM_KILLFOCUS:
			// when we abort the focus, end any current polling loop
			seqselect_stop_read_from_main_thread( (void *) editwnd);
			break;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			SetFocus(editwnd);
			SendMessage(editwnd, EM_SETSEL, 0, -1);
			call_baseclass = false;
			result = 0;
			break;
	}

	if (call_baseclass)
		result = call_windowproc(stuff->oldwndproc, editwnd, msg, wparam, lparam);
	return result;
}



//============================================================
//  seqselect_setup
//    called from dialog_add_single_seqselect
//============================================================

static LRESULT seqselect_setup(dialog_box *dialog, HWND editwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	seqselect_info *stuff = (seqselect_info *) lparam;
	LONG_PTR lp;

	lp = SetWindowLongPtr(editwnd, GWLP_WNDPROC, (LONG_PTR) seqselect_wndproc);
	stuff->oldwndproc = (WNDPROC) lp;
	SetWindowLongPtr(editwnd, GWLP_USERDATA, lparam);
	seqselect_settext(editwnd);
	return 0;
}



//============================================================
//  seqselect_apply
//    called from dialog_add_single_seqselect
//============================================================

static LRESULT seqselect_apply(dialog_box *dialog, HWND editwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	seqselect_info *stuff;
	stuff = get_seqselect_info(editwnd);

	// store the settings
	stuff->field->set_user_settings(stuff->settings);

	return 0;
}

//============================================================
//  dialog_add_single_seqselect
//    called from win_dialog_add_portselect
//============================================================

static int dialog_add_single_seqselect(struct _dialog_box *di, short x, short y, short cx, short cy, ioport_field *field, int is_analog, int seqtype)
{
	// write the dialog item
	if (dialog_write_item(di, WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS | ES_CENTER | SS_SUNKEN, x, y, cx, cy, NULL, DLGITEM_EDIT, NULL))
		return 1;

	// allocate a seqselect_info
	seqselect_info *stuff = new(seqselect_info);
	//seqselect_info *stuff;
	//stuff = global_alloc(seqselect_info);
	if (!stuff)
		return 1;

	// initialize the structure
	memset(stuff, 0, sizeof(*stuff));
	field->get_user_settings(stuff->settings);
	stuff->field = field;
	stuff->pos = di->item_count;
	stuff->is_analog = is_analog;

	// This next line is completely unsafe, but I do not know what to use *****************
	stuff->code = const_cast <input_seq*> (&field->seq( SEQ_TYPE_STANDARD ));

	if (dialog_add_trigger(di, di->item_count, TRIGGER_INITDIALOG, 0, seqselect_setup, di->item_count, (LPARAM) stuff, NULL, NULL))
		return 1;
	if (dialog_add_trigger(di, di->item_count, TRIGGER_APPLY, 0, seqselect_apply, 0, 0, NULL, NULL))
		return 1;
	return 0;
}



//============================================================
//  win_dialog_add_seqselect
//    called from customise_input, customise_miscinput
//============================================================

static int win_dialog_add_portselect(dialog_box *dialog, ioport_field *field)
{
	dialog_box *di = dialog;
	short x;
	short y;
	const char *port_name;
	const char *this_port_name;
	char *s;
	int seq;
	int seq_count = 0;
	const char *port_suffix[3];
	int seq_types[3];
	int is_analog[3];
	int len;

	port_name = field->name();
	if (!port_name)
		printf("Blank port_name encountered in win_dialog_add_portselect\n");
	assert(port_name);

	if (field->type() > IPT_ANALOG_FIRST && field->type() < IPT_ANALOG_LAST)
	{
		seq_types[seq_count] = SEQ_TYPE_STANDARD;
		port_suffix[seq_count] = " Analog";
		is_analog[seq_count] = true;
		seq_count++;

		seq_types[seq_count] = SEQ_TYPE_DECREMENT;
		port_suffix[seq_count] = " Dec";
		is_analog[seq_count] = false;
		seq_count++;

		seq_types[seq_count] = SEQ_TYPE_INCREMENT;
		port_suffix[seq_count] = " Inc";
		is_analog[seq_count] = false;
		seq_count++;
	}
	else
	{
		seq_types[seq_count] = SEQ_TYPE_STANDARD;
		port_suffix[seq_count] = NULL;
		is_analog[seq_count] = false;
		seq_count++;
	}

	for (seq = 0; seq < seq_count; seq++)
	{
		// create our local name for this entry; also convert from
		// MAME strings to wide strings
		len = strlen(port_name);
		s = (char *) alloca((len + (port_suffix[seq] ? strlen(port_suffix[seq]) : 0) + 1) * sizeof(*s));
		strcpy(s, port_name);

		if (port_suffix[seq])
			strcpy(s + len, port_suffix[seq]);
		this_port_name = s;

		// no positions specified
		dialog_new_control(di, &x, &y);

		if (dialog_write_item(di, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, x, y,
				dialog->layout->label_width, DIM_NORMAL_ROW_HEIGHT, this_port_name, DLGITEM_STATIC, NULL))
			return 1;
		x += dialog->layout->label_width + DIM_HORIZONTAL_SPACING;

		if (dialog_add_single_seqselect(di, x, y, DIM_EDIT_WIDTH, DIM_NORMAL_ROW_HEIGHT, field, is_analog[seq], seq_types[seq]))
			return 1;
		y += DIM_VERTICAL_SPACING + DIM_NORMAL_ROW_HEIGHT;
		x += DIM_EDIT_WIDTH + DIM_HORIZONTAL_SPACING;

		dialog_finish_control(di, x, y);
	}
	return 0;
}



//============================================================
//  win_dialog_add_standard_buttons
//============================================================

static int win_dialog_add_standard_buttons(dialog_box *dialog)
{
	dialog_box *di = dialog;
	short x;
	short y;

	// Handle the case of an empty dialog box (size_x & size_y will be 0)
	if (!di->size_x)
		di->size_x = 3 * DIM_HORIZONTAL_SPACING + 2 * DIM_BUTTON_WIDTH;
	if (!di->size_y)
		di->size_y = DIM_VERTICAL_SPACING;

	// work out where cancel button goes
	x = di->size_x - DIM_HORIZONTAL_SPACING - DIM_BUTTON_WIDTH;
	y = di->size_y + DIM_VERTICAL_SPACING;

	// display cancel button
	if (dialog_write_item(di, WS_CHILD | WS_VISIBLE | SS_LEFT, x, y, DIM_BUTTON_WIDTH, DIM_BUTTON_ROW_HEIGHT, DLGTEXT_CANCEL, DLGITEM_BUTTON, NULL))
		return 1;

	// work out where OK button goes
	x -= DIM_HORIZONTAL_SPACING + DIM_BUTTON_WIDTH;

	// display OK button
	if (dialog_write_item(di, WS_CHILD | WS_VISIBLE | SS_LEFT, x, y, DIM_BUTTON_WIDTH, DIM_BUTTON_ROW_HEIGHT, DLGTEXT_OK, DLGITEM_BUTTON, NULL))
		return 1;

	di->size_y += DIM_BUTTON_ROW_HEIGHT + DIM_VERTICAL_SPACING * 2;

	return 0;
}



//============================================================
//  before_display_dialog
//============================================================

static void before_display_dialog(running_machine &machine)
{
	Machine = &machine;
	winwindow_ui_pause(machine, true);
}



//============================================================
//  after_display_dialog
//============================================================

static void after_display_dialog(running_machine &machine)
{
	winwindow_ui_pause(machine, false);
}



//============================================================
//  win_dialog_runmodal
//    called from the various customise_inputs functions
//============================================================

static void win_dialog_runmodal(running_machine &machine, HWND wnd, dialog_box *dialog)
{
	if (!dialog)
		printf("Unexpected result in win_dialog_runmodal\n");
	assert(dialog);

	// finishing touches on the dialog
	dialog_prime(dialog);

	// show the dialog
	before_display_dialog(machine);
	DialogBoxIndirectParamW(NULL, (const DLGTEMPLATE*)dialog->handle, wnd, (DLGPROC) dialog_proc, (LPARAM) dialog);
	after_display_dialog(machine);
}



//============================================================
//  win_file_dialog
//    called from change_device
//============================================================

static BOOL win_file_dialog(running_machine &machine, HWND parent, win_file_dialog_type dlgtype, const char *filter, const char *initial_dir, char *filename)
{
	win_open_file_name ofn;
	BOOL result = false;

	// set up the OPENFILENAME data structure
	memset(&ofn, 0, sizeof(ofn));
	ofn.type = dlgtype;
	ofn.owner = parent;
	ofn.flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.filter = filter;
	ofn.initial_directory = initial_dir;

	if (dlgtype == WIN_FILE_DIALOG_OPEN)
		ofn.flags |= OFN_FILEMUSTEXIST;

	snprintf(ofn.filename, ARRAY_LENGTH(ofn.filename), "%s", filename);

	before_display_dialog(machine);
	result = win_get_file_name_dialog(&ofn);
	after_display_dialog(machine);

	snprintf(filename, ARRAY_LENGTH(ofn.filename), "%s", ofn.filename);
	return result;
}



//============================================================
//  customise_input
//============================================================

static void customise_input(running_machine &machine, HWND wnd, const char *title, int player, int inputclass)
{
	dialog_box *dlg;
	int this_inputclass, this_player;

	/* create the dialog */
	dlg = win_dialog_init(title, NULL);
	if (!dlg)
		return;

	for (auto &port : machine.ioport().ports())
	{
		for (ioport_field &field : port.second->fields())
		{
			/* add if we match the group and we have a valid name */
			const char *name = field.name();
			this_inputclass = field.type_class();
			this_player = field.player();

			if ((name)
			&& (field.enabled())
//check me		&& ((field->type() == IPT_OTHER && field->name()) || (machine.ioport().type_group(field->type(), field->player()) != IPG_INVALID))
			&& (this_player == player)
			&& (this_inputclass == inputclass))
			{
				if (win_dialog_add_portselect(dlg, &field))
					goto done;
			}
		}
	}

	/* ...and now add OK/Cancel */
	if (win_dialog_add_standard_buttons(dlg))
		goto done;

	/* ...and finally display the dialog */
	win_dialog_runmodal(machine, wnd, dlg);

done:
	if (dlg)
		win_dialog_exit(dlg);
}



//============================================================
//  customise_joystick
//============================================================

static void customise_joystick(running_machine &machine, HWND wnd, int joystick_num)
{
	customise_input(machine, wnd, "Joysticks/Controllers", joystick_num, INPUT_CLASS_CONTROLLER);
}



//============================================================
//  customise_keyboard
//============================================================

static void customise_keyboard(running_machine &machine, HWND wnd)
{
	customise_input(machine, wnd, "Emulated Keyboard", 0, INPUT_CLASS_KEYBOARD);
}



//===============================================================================================
//  check_for_miscinput
//  (to decide if "Miscellaneous Inputs" menu item should show or not).
//  We do this here because the core check has been broken for years (always returns false).
//===============================================================================================

static bool check_for_miscinput(running_machine &machine)
{
	int this_inputclass = 0;

	for (auto &port : machine.ioport().ports())
	{
		for (ioport_field &field : port.second->fields())
		{
			const char *name = field.name();
			this_inputclass = field.type_class();

			/* add if we match the group and we have a valid name */
			if ((name)
			&& (field.enabled())
			&& ((field.type() == IPT_OTHER && field.name()) || (machine.ioport().type_group(field.type(), field.player()) != IPG_INVALID))
			&& (this_inputclass != INPUT_CLASS_CONTROLLER)
			&& (this_inputclass != INPUT_CLASS_KEYBOARD))
			{
				return true;
			}
		}
	}
	return false;
}



//============================================================
//  customise_miscinput
//============================================================

static void customise_miscinput(running_machine &machine, HWND wnd)
{
	dialog_box *dlg;
	int this_inputclass;
	const char *title = "Miscellaneous Inputs";

	/* create the dialog */
	dlg = win_dialog_init(title, NULL);
	if (!dlg)
		return;

	for (auto &port : machine.ioport().ports())
	{
		for (ioport_field &field : port.second->fields())
		{
			const char *name = field.name();
			this_inputclass = field.type_class();

			/* add if we match the group and we have a valid name */
			if ((name)
			&& (field.enabled())
			&& ((field.type() == IPT_OTHER && field.name()) || (machine.ioport().type_group(field.type(), field.player()) != IPG_INVALID))
			&& (this_inputclass != INPUT_CLASS_CONTROLLER)
			&& (this_inputclass != INPUT_CLASS_KEYBOARD))
			{
				if (win_dialog_add_portselect(dlg, &field))
					goto done;
			}
		}
	}

	/* ...and now add OK/Cancel */
	if (win_dialog_add_standard_buttons(dlg))
		goto done;

	/* ...and finally display the dialog */
	win_dialog_runmodal(machine, wnd, dlg);

done:
	if (dlg)
		win_dialog_exit(dlg);
}



//============================================================
//  update_keyval
//    called from customise_switches
//============================================================

static void update_keyval(void *param, int val)
{
	ioport_field *field = (ioport_field *) param;
	ioport_field::user_settings settings;

	field->get_user_settings(settings);
	settings.value = (ioport_value) val;
	field->set_user_settings(settings);
}



//============================================================
//  customise_switches
//============================================================

static void customise_switches(running_machine &machine, HWND wnd, const char* title_string, UINT32 ipt_name)
{
	dialog_box *dlg;
	ioport_field *afield;
	ioport_setting *setting;
	const char *switch_name = NULL;
	ioport_field::user_settings settings;

	UINT32 type = 0;

	dlg = win_dialog_init(title_string, NULL);
	if (!dlg)
		return;

	for (auto &port : machine.ioport().ports())
	{
		for (ioport_field &field : port.second->fields())
		{
			type = field.type();

			if (type == ipt_name)
			{
				switch_name = field.name();

				field.get_user_settings(settings);
				afield = &field;
				if (win_dialog_add_combobox(dlg, switch_name, settings.value, update_keyval, (void *) afield))
					goto done;

				for (setting = field.settings().first(); setting; setting = setting->next())
				{
					if (win_dialog_add_combobox_item(dlg, setting->name(), setting->value()))
						goto done;
				}
			}
		}
	}

	if (win_dialog_add_standard_buttons(dlg))
		goto done;

	win_dialog_runmodal(machine, wnd, dlg);

done:
	if (dlg)
		win_dialog_exit(dlg);
}



//============================================================
//  customise_dipswitches
//============================================================

static void customise_dipswitches(running_machine &machine, HWND wnd)
{
	customise_switches(machine, wnd, "Dip Switches", IPT_DIPSWITCH);
}



//============================================================
//  customise_configuration
//============================================================

static void customise_configuration(running_machine &machine, HWND wnd)
{
	customise_switches(machine, wnd, "Driver Configuration", IPT_CONFIG);
}



//============================================================
//  customise_analogcontrols
//============================================================

enum
{
	ANALOG_ITEM_KEYSPEED,
	ANALOG_ITEM_CENTERSPEED,
	ANALOG_ITEM_REVERSE,
	ANALOG_ITEM_SENSITIVITY
};



static void store_analogitem(void *param, int val, int selected_item)
{
	ioport_field *field = (ioport_field *) param;
	ioport_field::user_settings settings;

	field->get_user_settings(settings);

	switch(selected_item)
	{
		case ANALOG_ITEM_KEYSPEED:
			settings.delta = val;
			break;
		case ANALOG_ITEM_CENTERSPEED:
			settings.centerdelta = val;
			break;
		case ANALOG_ITEM_REVERSE:
			settings.reverse = val;
			break;
		case ANALOG_ITEM_SENSITIVITY:
			settings.sensitivity = val;
			break;
	}
	field->set_user_settings(settings);
}



static void store_delta(void *param, int val)
{
	store_analogitem(param, val, ANALOG_ITEM_KEYSPEED);
}



static void store_centerdelta(void *param, int val)
{
	store_analogitem(param, val, ANALOG_ITEM_CENTERSPEED);
}



static void store_reverse(void *param, int val)
{
	store_analogitem(param, val, ANALOG_ITEM_REVERSE);
}



static void store_sensitivity(void *param, int val)
{
	store_analogitem(param, val, ANALOG_ITEM_SENSITIVITY);
}



static int port_type_is_analog(int type)
{
	return (type > IPT_ANALOG_FIRST && type < IPT_ANALOG_LAST);
}



static void customise_analogcontrols(running_machine &machine, HWND wnd)
{
	dialog_box *dlg;
	ioport_field::user_settings settings;
	ioport_field *afield;
	const char *name;
	char buf[255];
	static const struct dialog_layout layout = { 120, 52 };

	dlg = win_dialog_init("Analog Controls", &layout);
	if (!dlg)
		return;

	for (auto &port : machine.ioport().ports())
	{
		for (ioport_field &field : port.second->fields())
		{
			if (port_type_is_analog(field.type()))
			{
				field.get_user_settings(settings);
				name = field.name();
				afield = &field;

				_snprintf(buf, ARRAY_LENGTH(buf), "%s %s", name, "Digital Speed");
				if (win_dialog_add_adjuster(dlg, buf, settings.delta, 1, 255, false, store_delta, (void *) afield))
					goto done;

				_snprintf(buf, ARRAY_LENGTH(buf), "%s %s", name, "Autocenter Speed");
				if (win_dialog_add_adjuster(dlg, buf, settings.centerdelta, 0, 255, false, store_centerdelta, (void *) afield))
					goto done;

				_snprintf(buf, ARRAY_LENGTH(buf), "%s %s", name, "Reverse");
				if (win_dialog_add_combobox(dlg, buf, settings.reverse ? 1 : 0, store_reverse, (void *) afield))
					goto done;
				if (win_dialog_add_combobox_item(dlg, "Off", 0))
					goto done;
				if (win_dialog_add_combobox_item(dlg, "On", 1))
					goto done;

				_snprintf(buf, ARRAY_LENGTH(buf), "%s %s", name, "Sensitivity");
				if (win_dialog_add_adjuster(dlg, buf, settings.sensitivity, 1, 255, true, store_sensitivity, (void *) afield))
					goto done;
			}
		}
	}

	if (win_dialog_add_standard_buttons(dlg))
		goto done;

	win_dialog_runmodal(machine, wnd, dlg);

done:
	if (dlg)
		win_dialog_exit(dlg);
}


//============================================================
//  win_dirname
//    called from state_dialog
//============================================================

static char *win_dirname(const char *filename)
{
	// NULL begets NULL
	if (!filename)
		return NULL;

	char *dirname;
	char *c;

	// allocate space for it
	dirname = (char*)malloc(strlen(filename) + 1);
	if (!dirname)
		return NULL;

	// copy in the name
	strcpy(dirname, filename);

	// search backward for a slash or a colon
	for (c = dirname + strlen(dirname) - 1; c >= dirname; c--)
		if (*c == '\\' || *c == '/' || *c == ':')
		{
			// found it: NULL terminate and return
			*(c + 1) = 0;
			return dirname;
		}

	// otherwise, return an empty string
	dirname[0] = 0;
	return dirname;
}


//============================================================
//  state_dialog
//    called when loading or saving a state
//============================================================

static void state_dialog(HWND wnd, win_file_dialog_type dlgtype, DWORD fileproc_flags, bool is_load, running_machine &machine)
{
	char *dir = NULL;

	if (state_filename[0])
		dir = win_dirname(state_filename);

	win_open_file_name ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.type = dlgtype;
	ofn.owner = wnd;
	ofn.flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | fileproc_flags;
	ofn.filter = "State Files (*.sta)|*.sta|All Files (*.*)|*.*";
	ofn.initial_directory = dir;

	if (!core_filename_ends_with(ofn.filename, "sta"))
		snprintf(ofn.filename, ARRAY_LENGTH(ofn.filename), "%s.sta", state_filename);
	else
		snprintf(ofn.filename, ARRAY_LENGTH(ofn.filename), "%s", state_filename);

	BOOL result = win_get_file_name_dialog(&ofn);

	if (result)
	{
		// the core doesn't add the extension if it's an absolute path
		if (osd_is_absolute_path(ofn.filename) && !core_filename_ends_with(ofn.filename, "sta"))
			snprintf(state_filename, ARRAY_LENGTH(state_filename), "%s.sta", ofn.filename);
		else
			snprintf(state_filename, ARRAY_LENGTH(state_filename), "%s", ofn.filename);

		if (is_load)
			machine.schedule_load(state_filename);
		else
			machine.schedule_save(state_filename);
	}

	if (dir)
		free(dir);
}



static void state_load(HWND wnd, running_machine &machine)
{
	state_dialog(wnd, WIN_FILE_DIALOG_OPEN, OFN_FILEMUSTEXIST, true, machine);
}

static void state_save_as(HWND wnd, running_machine &machine)
{
	state_dialog(wnd, WIN_FILE_DIALOG_SAVE, OFN_OVERWRITEPROMPT, false, machine);
}

static void state_save(running_machine &machine)
{
	machine.schedule_save(state_filename);
}



//============================================================
//  copy_extension_list
//============================================================

static void copy_extension_list(std::string &dest, const char *extensions)
{
	// our extension lists are comma delimited; Win32 expects to see lists
	// delimited by semicolons
	char const *s = extensions;
	while (*s)
	{
		// append a semicolon if not at the beginning
		if (s != extensions)
			dest.push_back(';');

		// append ".*"
		dest.append("*.");

		// append the file extension
		while (*s && (*s != ','))
			dest.push_back(*s++);

		// if we found a comma, advance
		while(*s == ',')
			s++;
	}
}



//============================================================
//  add_filter_entry
//============================================================

static void add_filter_entry(std::string &dest, const char *description, const char *extensions)
{
	// add the description
	dest.append(description);
	dest.append(" (");

	// add the extensions to the description
	copy_extension_list(dest, extensions);

	// add the trailing rparen and '|' character
	dest.append(")|");

	// now add the extension list itself
	copy_extension_list(dest, extensions);

	// append a '|'
	dest.append("|");
}



//============================================================
//  build_generic_filter
//============================================================

static void build_generic_filter(device_image_interface *img, bool is_save, std::string &filter)
{
	std::string file_extension;

	if (img)
		file_extension = img->file_extensions();

	if (!is_save)
		file_extension.append(",zip,7z");

	add_filter_entry(filter, "Common image types", file_extension.c_str());

	filter.append("All files (*.*)|*.*|");

	if (!is_save)
		filter.append("Compressed Images (*.zip;*.7z)|*.zip;*.7z|");
}



//============================================================
//  get_softlist_info
//============================================================
static bool get_softlist_info(HWND wnd, device_image_interface *img)
{
	bool has_software = false;
	bool passes_tests = false;
	std::string sl_dir, opt_name = img->instance_name();
	LONG_PTR ptr = GetWindowLongPtr(wnd, GWLP_USERDATA);
	win_window_info *window = (win_window_info *)ptr;

	/* Get the media_path */
	char rompath[2048];
	strcpy(rompath, window->machine().options().emu_options::media_path());

	// Get the path to suitable software
	for (software_list_device &swlist : software_list_device_iterator(window->machine().root_device()))
	{
		for (const software_info &swinfo : swlist.get_info())
		{
			const software_part &part = swinfo.parts().front();
			if (swlist.is_compatible(part) == SOFTWARE_IS_COMPATIBLE)
			{
				for (device_image_interface &image : image_interface_iterator(window->machine().root_device()))
				{
					if (!image.user_loadable())
						continue;
					if (!has_software && (opt_name == image.instance_name()))
					{
						const char *interface = image.image_interface();
						if (interface && part.matches_interface(interface))
						{
							sl_dir = "\\" + swlist.list_name();
							has_software = true;
						}
					}
				}
			}
		}
	}

	if (has_software)
	{
		// Now, scan through the media_path looking for the required folder
		char* sl_root = strtok(rompath, ";");
		while (sl_root && !passes_tests)
		{
			std::string test_path = sl_root + sl_dir;
			TCHAR *szPath = ui_wstring_from_utf8(test_path.c_str());
			DWORD dwAttrib = GetFileAttributes(szPath);
			if ((dwAttrib != INVALID_FILE_ATTRIBUTES) && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
			{
				passes_tests = true;
				slmap[opt_name] = test_path;
			}
			sl_root = strtok(NULL, ";");
		}
	}

	return passes_tests;
}



//============================================================
//  change_device
//    open a dialog box to open or create a software file
//============================================================

static void change_device(HWND wnd, device_image_interface *image, bool is_save)
{
	// Get the path for loose software from <gamename>.ini
	// if this is invalid, then windows chooses whatever directory it used last.
	char buf[2048];
	strcpy(buf, image->device().machine().options().emu_options::sw_path());
	// This pulls out the first path from a multipath field
	const char* t1 = strtok(buf, ";");
	std::string initial_dir = t1 ? std::string(t1) : "";
	// must be specified, must exist
	if (initial_dir.empty() || (!osd::directory::open(initial_dir.c_str())))
	{
		// NOTE: the working directory can come from the .cfg file. If it's wrong delete the cfg.
		initial_dir = image->working_directory().c_str();
		// make sure this exists too
		if (!osd::directory::open(initial_dir.c_str()))
			// last fallback is to mame root
			osd_get_full_path(initial_dir, ".");
	}

	// remove any trailing backslash
	if (initial_dir.length() == initial_dir.find_last_of("\\"))
		initial_dir.erase(initial_dir.length()-1);

	// file name
	char filename[MAX_PATH];
	if (image->exists())
		strcpy(filename, image->basename());
	else
		filename[0] = '\0';

	// build a normal filter
	std::string filter;
	build_generic_filter(image, is_save, filter);

	// display the dialog
	util::option_resolution *create_args = NULL;
	bool result = win_file_dialog(image->device().machine(), wnd, is_save ? WIN_FILE_DIALOG_SAVE : WIN_FILE_DIALOG_OPEN, filter.c_str(), initial_dir.c_str(), filename);
	if (result)
	{
		// mount the image
		if (is_save)
			(image_error_t)image->create(filename, image->device_get_indexed_creatable_format(0), create_args);
		else
			(image_error_t)image->load( filename);
	}
}


//============================================================
//  load_item
//    open a dialog box to choose a software-list-item to load
//============================================================
static void load_item(HWND wnd, device_image_interface *img, bool is_save)
{
	std::string opt_name = img->instance_name();
	std::string as = slmap.find(opt_name)->second;

	/* Make sure a folder was specified in the tab, and that it exists */
	if ((!osd::directory::open(as.c_str())) || (as.find(':') == std::string::npos))
	{
		/* Default to emu directory */
		osd_get_full_path(as, ".");
	}

	// build a normal filter
	std::string filter;
	build_generic_filter(NULL, is_save, filter);

	// display the dialog
	char filename[MAX_PATH] = "";
	bool result = win_file_dialog(img->device().machine(), wnd, WIN_FILE_DIALOG_OPEN, filter.c_str(), as.c_str(), filename);

	if (result)
	{
		// Get the Item name out of the full path
		std::string buf = filename; // convert to a c++ string so we can manipulate it
		size_t t1 = buf.find(".zip"); // get rid of zip name and anything after
		if (t1 != std::string::npos)
			buf.erase(t1);
		else
		{
			t1 = buf.find(".7z"); // get rid of 7zip name and anything after
			if (t1 != std::string::npos)
				buf.erase(t1);
		}
		t1 = buf.find_last_of("\\");   // put the swlist name in
		buf[t1] = ':';
		t1 = buf.find_last_of("\\"); // get rid of path; we only want the item name
		buf.erase(0, t1+1);

		// load software
		img->load_software( buf.c_str());
	}
}



//============================================================
//  pause
//============================================================

static void pause(running_machine &machine)
{
	if (!winwindow_ui_is_paused(machine))
		machine.pause();
	else
		machine.resume();
}



//============================================================
//  get_menu_item_string
//============================================================

static BOOL get_menu_item_string(HMENU menu, UINT item, BOOL by_position, HMENU *sub_menu, LPTSTR buffer, size_t buffer_len)
{
	MENUITEMINFO mii;

	// clear out results
	memset(buffer, '\0', sizeof(*buffer) * buffer_len);
	if (sub_menu)
		*sub_menu = NULL;

	// prepare MENUITEMINFO structure
	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_TYPE | (sub_menu ? MIIM_SUBMENU : 0);
	mii.dwTypeData = buffer;
	mii.cch = buffer_len;

	// call GetMenuItemInfo()
	if (!GetMenuItemInfo(menu, item, by_position, &mii))
		return false;

	// return results
	if (sub_menu)
		*sub_menu = mii.hSubMenu;
	if (mii.fType == MFT_SEPARATOR)
		_sntprintf(buffer, buffer_len, TEXT("-"));
	return true;
}



//============================================================
//  find_sub_menu
//============================================================

static HMENU find_sub_menu(HMENU menu, const char *menutext, bool create_sub_menu)
{
	TCHAR buf[128];
	HMENU sub_menu;

	while(*menutext)
	{
		TCHAR *t_menutext = ui_wstring_from_utf8(menutext);

		int i = -1;
		do
		{
			if (!get_menu_item_string(menu, ++i, true, &sub_menu, buf, ARRAY_LENGTH(buf)))
			{
				free(t_menutext);
				return NULL;
			}
		}
		while(_tcscmp(t_menutext, buf));

		free(t_menutext);

		if (!sub_menu && create_sub_menu)
		{
			MENUITEMINFO mii;
			memset(&mii, 0, sizeof(mii));
			mii.cbSize = sizeof(mii);
			mii.fMask = MIIM_SUBMENU;
			mii.hSubMenu = CreateMenu();
			if (!SetMenuItemInfo(menu, i, true, &mii))
			{
				i = GetLastError();
				return NULL;
			}
			sub_menu = mii.hSubMenu;
		}
		menu = sub_menu;
		if (!menu)
			return NULL;
		menutext += strlen(menutext) + 1;
	}
	return menu;
}



//============================================================
//  set_command_state
//============================================================

static void set_command_state(HMENU menu_bar, UINT command, UINT state)
{
	MENUITEMINFO mii;
	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_STATE;
	mii.fState = state;
	BOOL result = SetMenuItemInfo(menu_bar, command, false, &mii);
	result++;
}




//============================================================
//  remove_menu_items
//============================================================

static void remove_menu_items(HMENU menu)
{
	while(RemoveMenu(menu, 0, MF_BYPOSITION))
		;
}



//============================================================
//  setup_joystick_menu
//============================================================

static void setup_joystick_menu(running_machine &machine, HMENU menu_bar)
{
	HMENU joystick_menu = find_sub_menu(menu_bar, "&Options\0&Joysticks\0", true);
	if (!joystick_menu)
		return;

	// set up joystick menu
	char buf[256];
	int child_count = 0;
	int joystick_count = machine.ioport().count_players();
	if (joystick_count > 0)
	{
		for (int i = 0; i < joystick_count; i++)
		{
			snprintf(buf, ARRAY_LENGTH(buf), "Joystick %i", i + 1);
			win_append_menu_utf8(joystick_menu, MF_STRING, ID_JOYSTICK_0 + i, buf);
			child_count++;
		}
	}

	// last but not least, enable the joystick menu (or not)
	set_command_state(menu_bar, ID_OPTIONS_JOYSTICKS, child_count ? MFS_ENABLED : MFS_GRAYED);
}



//============================================================
//  is_windowed
//============================================================

static int is_windowed(void)
{
	return video_config.windowed;
}



//============================================================
//  frameskip_level_count
//============================================================

static int frameskip_level_count(running_machine &machine)
{
	static int count = -1;

	if (count < 0)
	{
		int frameskip_max = 10;
		count = frameskip_max + 1;
	}
	return count;
}



//============================================================
//  prepare_menus
//============================================================

static void prepare_menus(HWND wnd)
{
	LONG_PTR ptr = GetWindowLongPtr(wnd, GWLP_USERDATA);
	win_window_info *window = (win_window_info *)ptr;
	HMENU menu_bar = GetMenu(wnd);
	if (!menu_bar)
		return;

	if (!joystick_menu_setup)
	{
		setup_joystick_menu(window->machine(), menu_bar);
		joystick_menu_setup = 1;
	}

	int frameskip = window->machine().video().frameskip();

	int orientation = window->m_target->orientation();

	int speed = window->machine().video().throttled() ? window->machine().video().speed_factor() : 0;

	bool has_config = window->machine().ioport().type_class_present(INPUT_CLASS_CONFIG);
	bool has_dipswitch = window->machine().ioport().type_class_present(INPUT_CLASS_DIPSWITCH);
	bool has_keyboard = window->machine().ioport().type_class_present(INPUT_CLASS_KEYBOARD);
	bool has_misc = check_for_miscinput(window->machine());

	bool has_analog = false;
	for (auto &port : window->machine().ioport().ports())
	{
		for (ioport_field &field : port.second->fields())
		{
			if (port_type_is_analog(field.type()))
			{
				has_analog = true;
				break;
			}
		}
	}

	if (window->machine().system().flags & MACHINE_SUPPORTS_SAVE)
	{
		set_command_state(menu_bar, ID_FILE_LOADSTATE_NEWUI, MFS_ENABLED);
		set_command_state(menu_bar, ID_FILE_SAVESTATE_AS, MFS_ENABLED);
		set_command_state(menu_bar, ID_FILE_SAVESTATE, state_filename[0] != '\0' ? MFS_ENABLED : MFS_GRAYED);
	}
	else
	{
		set_command_state(menu_bar, ID_FILE_LOADSTATE_NEWUI, MFS_GRAYED);
		set_command_state(menu_bar, ID_FILE_SAVESTATE_AS, MFS_GRAYED);
		set_command_state(menu_bar, ID_FILE_SAVESTATE, MFS_GRAYED);
	}

	set_command_state(menu_bar, ID_EDIT_PASTE, window->machine().ioport().natkeyboard().can_post() ? MFS_ENABLED : MFS_GRAYED);

	set_command_state(menu_bar, ID_OPTIONS_PAUSE, winwindow_ui_is_paused(window->machine()) ? MFS_CHECKED : MFS_ENABLED);
	set_command_state(menu_bar, ID_OPTIONS_CONFIGURATION, has_config ? MFS_ENABLED : MFS_GRAYED);
	set_command_state(menu_bar, ID_OPTIONS_DIPSWITCHES, has_dipswitch ? MFS_ENABLED : MFS_GRAYED);
	set_command_state(menu_bar, ID_OPTIONS_MISCINPUT, has_misc ? MFS_ENABLED : MFS_GRAYED);
	set_command_state(menu_bar, ID_OPTIONS_ANALOGCONTROLS, has_analog ? MFS_ENABLED : MFS_GRAYED);
	set_command_state(menu_bar, ID_FILE_FULLSCREEN, !is_windowed() ? MFS_CHECKED : MFS_ENABLED);
	set_command_state(menu_bar, ID_OPTIONS_TOGGLEFPS, mame_machine_manager::instance()->ui().show_fps() ? MFS_CHECKED : MFS_ENABLED);
	set_command_state(menu_bar, ID_FILE_UIACTIVE, has_keyboard ? (window->machine().ui_active() ? MFS_CHECKED : MFS_ENABLED): MFS_CHECKED | MFS_GRAYED);

	set_command_state(menu_bar, ID_KEYBOARD_EMULATED, has_keyboard ? (!window->machine().ioport().natkeyboard().in_use() ? MFS_CHECKED : MFS_ENABLED): MFS_GRAYED);
	set_command_state(menu_bar, ID_KEYBOARD_NATURAL, (has_keyboard && window->machine().ioport().natkeyboard().can_post()) ? (window->machine().ioport().natkeyboard().in_use() ? MFS_CHECKED : MFS_ENABLED): MFS_GRAYED);
	set_command_state(menu_bar, ID_KEYBOARD_CUSTOMIZE, has_keyboard ? MFS_ENABLED : MFS_GRAYED);

	set_command_state(menu_bar, ID_VIDEO_ROTATE_0, (orientation == ROT0) ? MFS_CHECKED : MFS_ENABLED);
	set_command_state(menu_bar, ID_VIDEO_ROTATE_90, (orientation == ROT90) ? MFS_CHECKED : MFS_ENABLED);
	set_command_state(menu_bar, ID_VIDEO_ROTATE_180, (orientation == ROT180) ? MFS_CHECKED : MFS_ENABLED);
	set_command_state(menu_bar, ID_VIDEO_ROTATE_270, (orientation == ROT270) ? MFS_CHECKED : MFS_ENABLED);

	set_command_state(menu_bar, ID_THROTTLE_50, (speed == 500) ? MFS_CHECKED : MFS_ENABLED);
	set_command_state(menu_bar, ID_THROTTLE_100, (speed == 1000) ? MFS_CHECKED : MFS_ENABLED);
	set_command_state(menu_bar, ID_THROTTLE_200, (speed == 2000) ? MFS_CHECKED : MFS_ENABLED);
	set_command_state(menu_bar, ID_THROTTLE_500, (speed == 5000) ? MFS_CHECKED : MFS_ENABLED);
	set_command_state(menu_bar, ID_THROTTLE_1000, (speed == 10000) ? MFS_CHECKED : MFS_ENABLED);
	set_command_state(menu_bar, ID_THROTTLE_UNTHROTTLED, (speed == 0) ? MFS_CHECKED : MFS_ENABLED);

	set_command_state(menu_bar, ID_FRAMESKIP_AUTO, (frameskip < 0) ? MFS_CHECKED : MFS_ENABLED);
	int i;
	for (i = 0; i < frameskip_level_count(window->machine()); i++)
		set_command_state(menu_bar, ID_FRAMESKIP_0 + i, (frameskip == i) ? MFS_CHECKED : MFS_ENABLED);

	// set up screens in video menu
	TCHAR t_buf[MAX_PATH];
	HMENU video_menu = find_sub_menu(menu_bar, "&Options\0&Video\0", false);
	do
	{
		get_menu_item_string(video_menu, 0, true, NULL, t_buf, ARRAY_LENGTH(t_buf));
		if (_tcscmp(t_buf, TEXT("-")))
			RemoveMenu(video_menu, 0, MF_BYPOSITION);
	}
	while(_tcscmp(t_buf, TEXT("-")));

	i = 0;
	const char *view_name;
	int view_index = window->m_target->view();
	while((view_name = window->m_target->view_name(i)))
	{
		TCHAR *t_view_name = ui_wstring_from_utf8(view_name);
		InsertMenu(video_menu, i, MF_BYPOSITION | (i == view_index ? MF_CHECKED : 0), ID_VIDEO_VIEW_0 + i, t_view_name);
		free(t_view_name);
		i++;
	}

	// set up device menu; first remove all existing menu items
	HMENU sub_menu, device_menu = find_sub_menu(menu_bar, "&Media\0", false);
	remove_menu_items(device_menu);

	UINT_PTR new_item;
	UINT flags_for_exists = 0;
	UINT flags_for_writing = 0;
	int cnt = 0;
	char buf[MAX_PATH];
	// then set up the actual devices
	for (device_image_interface &img : image_interface_iterator(window->machine().root_device()))
	{
		if (!img.user_loadable())
			continue;

		new_item = ID_DEVICE_0 + (cnt * DEVOPTION_MAX);
		flags_for_exists = MF_STRING;

		if (!img.exists())
			flags_for_exists |= MF_GRAYED;

		flags_for_writing = flags_for_exists;
		if (img.is_readonly())
			flags_for_writing |= MF_GRAYED;

		sub_menu = CreateMenu();

		// see if a software list exists and if so add the Mount Item menu
		if (get_softlist_info(wnd, &img))
			win_append_menu_utf8(sub_menu, MF_STRING, new_item + DEVOPTION_ITEM, "Mount Item...");

		win_append_menu_utf8(sub_menu, MF_STRING, new_item + DEVOPTION_OPEN, "Mount File...");

		if (img.is_creatable())
			win_append_menu_utf8(sub_menu, MF_STRING, new_item + DEVOPTION_CREATE, "Create...");

		win_append_menu_utf8(sub_menu, flags_for_exists, new_item + DEVOPTION_CLOSE, "Unmount");

		if (img.device().type() == CASSETTE)
		{
			cassette_state state;
			state = (cassette_state)(img.exists() ? (dynamic_cast<cassette_image_device*>(&img.device())->get_state() & CASSETTE_MASK_UISTATE) : CASSETTE_STOPPED);
			win_append_menu_utf8(sub_menu, MF_SEPARATOR, 0, NULL);
			win_append_menu_utf8(sub_menu, flags_for_exists | ((state == CASSETTE_STOPPED) ? MF_CHECKED : 0), new_item + DEVOPTION_CASSETTE_STOPPAUSE, "Pause/Stop");
			win_append_menu_utf8(sub_menu, flags_for_exists | ((state == CASSETTE_PLAY) ? MF_CHECKED : 0), new_item + DEVOPTION_CASSETTE_PLAY, "Play");
			win_append_menu_utf8(sub_menu, flags_for_writing | ((state == CASSETTE_RECORD) ? MF_CHECKED : 0), new_item + DEVOPTION_CASSETTE_RECORD, "Record");
			win_append_menu_utf8(sub_menu, flags_for_exists, new_item + DEVOPTION_CASSETTE_REWIND, "Rewind");
			win_append_menu_utf8(sub_menu, flags_for_exists, new_item + DEVOPTION_CASSETTE_FASTFORWARD, "Fast Forward");
			win_append_menu_utf8(sub_menu, MF_SEPARATOR, 0, NULL);
			// Motor state can be overriden by the driver
			state = (cassette_state)(img.exists() ? (dynamic_cast<cassette_image_device*>(&img.device())->get_state() & CASSETTE_MASK_MOTOR) : 0);
			win_append_menu_utf8(sub_menu, flags_for_exists | ((state == CASSETTE_MOTOR_ENABLED) ? MF_CHECKED : 0), new_item + DEVOPTION_CASSETTE_MOTOR, "Motor");
			// Speaker requires that cassette-wave device be included in the machine config
			state = (cassette_state)(img.exists() ? (dynamic_cast<cassette_image_device*>(&img.device())->get_state() & CASSETTE_MASK_SPEAKER) : 0);
			win_append_menu_utf8(sub_menu, flags_for_exists | ((state == CASSETTE_SPEAKER_ENABLED) ? MF_CHECKED : 0), new_item + DEVOPTION_CASSETTE_SOUND, "Audio while Loading");
		}

		std::string filename;
		if (img.basename())
		{
			filename.assign(img.basename());

			// if the image has been loaded through softlist, also show the loaded part
			if (img.loaded_through_softlist())
			{
				const software_part *tmp = img.part_entry();
				if (!tmp->name().empty())
				{
					filename.append(" (").append(tmp->name());
					// also check if this part has a specific part_id (e.g. "Map Disc", "Bonus Disc", etc.), and in case display it
					if (img.get_feature("part_id"))
						filename.append(": ").append(img.get_feature("part_id"));
					filename.append(")");
				}
			}
		}
		else
			filename.assign("---");

		// Get instance names instead, like Media View, and mame's File Manager
		std::string instance = img.instance_name() + std::string(" (") + img.brief_instance_name() + std::string("): ") + filename;
		std::transform(instance.begin(), instance.begin()+1, instance.begin(), ::toupper); // turn first char to uppercase

		snprintf(buf, ARRAY_LENGTH(buf), "%s", instance.c_str());
		win_append_menu_utf8(device_menu, MF_POPUP, (UINT_PTR)sub_menu, buf);

		cnt++;
	}

	// set up slot menu; first remove all existing menu items
	HMENU slot_menu = find_sub_menu(menu_bar, "&Slots\0", false);
	remove_menu_items(slot_menu);
	cnt = 3400;
	// cycle through all slots for this system
	for (device_slot_interface &slot : slot_interface_iterator(window->machine().root_device()))
	{
		if (slot.fixed())
			continue;
		// does this slot have any selectable options?
		if (!slot.has_selectable_options())
			continue;

		std::string opt_name="0", current="0";

		// name this option
		const char *slot_option_name = slot.slot_name();
		current = window->machine().options().slot_option(slot_option_name).value();

		const device_slot_interface::slot_option *option = slot.option(current.c_str());
		if (option)
			opt_name = option->name();

		sub_menu = CreateMenu();
		// add the slot
		win_append_menu_utf8(slot_menu, MF_POPUP, (UINT_PTR)sub_menu, slot.slot_name());
		// build a list of user-selectable options
		std::vector<device_slot_interface::slot_option *> option_list;
		for (auto &option : slot.option_list())
			if (option.second->selectable())
				option_list.push_back(option.second.get());

		// add the empty option
		slot_map[cnt] = slot_data { slot.slot_name(), "" };
		win_append_menu_utf8(sub_menu, MF_STRING | (opt_name == "0") ? MF_CHECKED : 0, cnt++, "[Empty]");

		// sort them by name
		std::sort(option_list.begin(), option_list.end(), [](device_slot_interface::slot_option *opt1, device_slot_interface::slot_option *opt2) {return strcmp(opt1->name(), opt2->name()) < 0;});

		// add each option in sorted order
		for (device_slot_interface::slot_option *opt : option_list)
		{
			std::string temp = opt->name() + std::string(" (") + opt->devtype().fullname() + std::string(")");
			slot_map[cnt] = slot_data { slot.slot_name(), opt->name() };
			win_append_menu_utf8(sub_menu, MF_STRING | (opt->name()==opt_name) ? MF_CHECKED : 0, cnt++, temp.c_str());
		}
	}
}



//============================================================
//  set_speed
//============================================================

static void set_speed(running_machine &machine, int speed)
{
	if (speed != 0)
	{
		machine.video().set_speed_factor(speed);
		machine.options().emu_options::set_value(OPTION_SPEED, speed / 1000, OPTION_PRIORITY_CMDLINE);
	}

	machine.video().set_throttled(speed != 0);
	machine.options().emu_options::set_value(OPTION_THROTTLE, (speed != 0), OPTION_PRIORITY_CMDLINE);
}



//============================================================
//  win_toggle_menubar
//============================================================

static void win_toggle_menubar(void)
{
	LONG width_diff = 0;
	LONG height_diff = 0;
	DWORD style = 0, exstyle = 0;
	HWND hwnd = 0;
	HMENU menu = 0;

	for (auto window : osd_common_t::s_window_list)
	{
		RECT before_rect = { 100, 100, 200, 200 };
		RECT after_rect = { 100, 100, 200, 200 };

		hwnd = std::static_pointer_cast<win_window_info>(window)->platform_window();

		// get current menu
		menu = GetMenu(hwnd);

		// get before rect
		style = GetWindowLong(hwnd, GWL_STYLE);
		exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
		AdjustWindowRectEx(&before_rect, style, menu ? true : false, exstyle);

		// toggle the menu
		if (menu)
		{
			SetProp(hwnd, TEXT("menu"), (HANDLE) menu);
			menu = NULL;
		}
		else
			menu = (HMENU) GetProp(hwnd, TEXT("menu"));

		SetMenu(hwnd, menu);

		// get after rect, and width/height diff
		AdjustWindowRectEx(&after_rect, style, menu ? true : false, exstyle);
		width_diff = (after_rect.right - after_rect.left) - (before_rect.right - before_rect.left);
		height_diff = (after_rect.bottom - after_rect.top) - (before_rect.bottom - before_rect.top);

		if (is_windowed())
		{
			RECT window_rect;
			GetWindowRect(hwnd, &window_rect);
			SetWindowPos(hwnd, HWND_TOP, 0, 0, window_rect.right - window_rect.left + width_diff, window_rect.bottom - window_rect.top + height_diff, SWP_NOMOVE | SWP_NOZORDER);
		}

		RedrawWindow(hwnd, NULL, NULL, 0);
	}
}



//============================================================
//  device_command
//    This handles all options under the "Media" dropdown
//============================================================

static void device_command(HWND wnd, device_image_interface *img, int devoption)
{
	switch(devoption)
	{
		case DEVOPTION_OPEN:
			change_device(wnd, img, false);
			break;

		case DEVOPTION_ITEM:
			load_item(wnd, img, false);
			break;

		case DEVOPTION_CREATE:
			change_device(wnd, img, true);
			break;

		case DEVOPTION_CLOSE:
			img->unload();
			img->device().machine().options().image_option(img->instance_name()).specify("");
			//img->device().machine().options().emu_options::set_value(img->instance_name().c_str(), "", OPTION_PRIORITY_CMDLINE);
			break;

		default:
			if (img->device().type() == CASSETTE)
			{
				cassette_image_device* cassette = dynamic_cast<cassette_image_device*>(&img->device());
				bool s;
				switch(devoption)
				{
					case DEVOPTION_CASSETTE_STOPPAUSE:
						cassette->change_state(CASSETTE_STOPPED, CASSETTE_MASK_UISTATE);
						break;

					case DEVOPTION_CASSETTE_PLAY:
						cassette->change_state(CASSETTE_PLAY, CASSETTE_MASK_UISTATE);
						break;

					case DEVOPTION_CASSETTE_RECORD:
						cassette->change_state(CASSETTE_RECORD, CASSETTE_MASK_UISTATE);
						break;

					case DEVOPTION_CASSETTE_REWIND:
						cassette->seek(0, SEEK_SET); // start
						break;

					case DEVOPTION_CASSETTE_FASTFORWARD:
						cassette->seek(+300.0, SEEK_CUR); // 5 minutes forward or end, whichever comes first
						break;

					case DEVOPTION_CASSETTE_MOTOR:
						s =((cassette->get_state() & CASSETTE_MASK_MOTOR) == CASSETTE_MOTOR_DISABLED);
						cassette->change_state(s ? CASSETTE_MOTOR_ENABLED : CASSETTE_MOTOR_DISABLED, CASSETTE_MASK_MOTOR);
						break;

					case DEVOPTION_CASSETTE_SOUND:
						s =((cassette->get_state() & CASSETTE_MASK_SPEAKER) == CASSETTE_SPEAKER_MUTED);
						cassette->change_state(s ? CASSETTE_SPEAKER_ENABLED : CASSETTE_SPEAKER_MUTED, CASSETTE_MASK_SPEAKER);
						break;
				}
			}
			break;
	}
}



//============================================================
//  help_display
//============================================================

static void help_display(HWND wnd, const char *chapter)
{
	typedef HWND (WINAPI *htmlhelpproc)(HWND hwndCaller, LPCTSTR pszFile, UINT uCommand, DWORD_PTR dwData);
	static htmlhelpproc htmlhelp;
	static DWORD htmlhelp_cookie;
	LPCSTR htmlhelp_funcname;

	if (htmlhelp == NULL)
	{
#ifdef UNICODE
		htmlhelp_funcname = "HtmlHelpW";
#else
		htmlhelp_funcname = "HtmlHelpA";
#endif
		htmlhelp = (htmlhelpproc) GetProcAddress(LoadLibrary(TEXT("hhctrl.ocx")), htmlhelp_funcname);
		if (!htmlhelp)
			return;
		htmlhelp(NULL, NULL, 28 /*HH_INITIALIZE*/, (DWORD_PTR) &htmlhelp_cookie);
	}

	// if full screen, turn it off
	if (!is_windowed())
		winwindow_toggle_full_screen();

	TCHAR *t_chapter = ui_wstring_from_utf8(chapter);
//	htmlhelp(wnd, t_chapter, 0 /*HH_DISPLAY_TOPIC*/, 0);
//	TCHAR *szSite = new TCHAR[100];
//	_tcscpy(szSite, TEXT("http://messui.polygonal-moogle.com/onlinehelp/"));
//	_tcscat(szSite, t_chapter);
//	_tcscat(szSite, TEXT(".html"));
//	ShellExecute(wnd, TEXT("open"), TEXT("http://www.microsoft.com/directx"), TEXT(""), NULL, SW_SHOWNORMAL);
	ShellExecute(wnd, TEXT("open"), t_chapter, TEXT(""), NULL, SW_SHOWNORMAL);
	free(t_chapter);
//	free(szSite);
}



//============================================================
//  help_about_mess
//============================================================

static void help_about_mess(HWND wnd)
{
	//help_display(wnd, "mess.chm::/windows/main.htm"); //doesnt do anything
	//help_display(wnd, "mess.chm");
	help_display(wnd, "https://mamedev.org/");
}



//============================================================
//  help_about_thissystem
//============================================================

static void help_about_thissystem(running_machine &machine, HWND wnd)
{
	char buf[100];
//	snprintf(buf, ARRAY_LENGTH(buf), "mess.chm::/sysinfo/%s.htm", machine.system().name);
//	snprintf(buf, ARRAY_LENGTH(buf), "http://messui.polygonal-moogle.com/onlinehelp/%s.html", machine.system().name);
//	snprintf(buf, ARRAY_LENGTH(buf), "http://www.progettoemma.net/mess/system.php?machine=%s", machine.system().name);
	snprintf(buf, ARRAY_LENGTH(buf), "http://adb.arcadeitalia.net/dettaglio_mame.php?game_name=%s", machine.system().name);
	help_display(wnd, buf);
}



//============================================================
//  decode_deviceoption
//============================================================

static device_image_interface *decode_deviceoption(running_machine &machine, int command, int *devoption)
{
	command -= ID_DEVICE_0;
	int absolute_index = command / DEVOPTION_MAX;

	if (devoption)
		*devoption = command % DEVOPTION_MAX;

	image_interface_iterator iter(machine.root_device());
	return iter.byindex(absolute_index);
}



//============================================================
//  set_window_orientation
//============================================================

static void set_window_orientation(win_window_info *window, int orientation)
{
	window->m_target->set_orientation(orientation);
	if (window->m_target->is_ui_target())
	{
		render_container::user_settings settings;
		window->machine().render().ui_container().get_user_settings(settings);
		settings.m_orientation = orientation;
		window->machine().render().ui_container().set_user_settings(settings);
	}
	window->update();
}



//============================================================
//  pause_for_command
//============================================================

static int pause_for_command(UINT command)
{
	// we really should be more conservative and only pause for commands
	// that do dialog stuff
	return (command != ID_OPTIONS_PAUSE);
}



//============================================================
//  invoke_command
//============================================================

static bool invoke_command(HWND wnd, UINT command)
{
	bool handled = true;
	int dev_command = 0;
	device_image_interface *img;
	LONG_PTR ptr = GetWindowLongPtr(wnd, GWLP_USERDATA);
	win_window_info *window = (win_window_info *)ptr;
	ioport_field::user_settings settings;

	// pause while invoking certain commands
	if (pause_for_command(command))
		winwindow_ui_pause(window->machine(), true);

	switch(command)
	{
		case ID_FILE_LOADSTATE_NEWUI:
			state_load(wnd, window->machine());
			break;

		case ID_FILE_SAVESTATE:
			state_save(window->machine());
			break;

		case ID_FILE_SAVESTATE_AS:
			state_save_as(wnd, window->machine());
			break;

		case ID_FILE_SAVESCREENSHOT:
			window->machine().video().save_active_screen_snapshots();
			break;

		case ID_FILE_UIACTIVE:
			window->machine().set_ui_active(!window->machine().ui_active());
			break;

		case ID_FILE_EXIT_NEWUI:
			window->machine().schedule_exit();
			break;

		case ID_EDIT_PASTE:
			PostMessage(wnd, WM_PASTE, 0, 0);
			break;

		case ID_KEYBOARD_NATURAL:
			window->machine().ioport().natkeyboard().set_in_use(true);
			break;

		case ID_KEYBOARD_EMULATED:
			window->machine().ioport().natkeyboard().set_in_use(false);
			break;

		case ID_KEYBOARD_CUSTOMIZE:
			customise_keyboard(window->machine(), wnd);
			break;

		case ID_VIDEO_ROTATE_0:
			set_window_orientation(window, ROT0);
			break;

		case ID_VIDEO_ROTATE_90:
			set_window_orientation(window, ROT90);
			break;

		case ID_VIDEO_ROTATE_180:
			set_window_orientation(window, ROT180);
			break;

		case ID_VIDEO_ROTATE_270:
			set_window_orientation(window, ROT270);
			break;

		case ID_OPTIONS_PAUSE:
			pause(window->machine());
			break;

		case ID_OPTIONS_HARDRESET:
			window->machine().schedule_hard_reset();
			break;

		case ID_OPTIONS_SOFTRESET:
			window->machine().schedule_soft_reset();
			break;

		case ID_OPTIONS_CONFIGURATION:
			customise_configuration(window->machine(), wnd);
			break;

		case ID_OPTIONS_DIPSWITCHES:
			customise_dipswitches(window->machine(), wnd);
			break;

		case ID_OPTIONS_MISCINPUT:
			customise_miscinput(window->machine(), wnd);
			break;

		case ID_OPTIONS_ANALOGCONTROLS:
			customise_analogcontrols(window->machine(), wnd);
			break;

		case ID_FILE_OLDUI:
			mame_machine_manager::instance()->ui().show_menu();
			break;

		case ID_FILE_FULLSCREEN:
			winwindow_toggle_full_screen();
			break;

		case ID_OPTIONS_TOGGLEFPS:
			mame_machine_manager::instance()->ui().set_show_fps(!mame_machine_manager::instance()->ui().show_fps());
			break;

		case ID_OPTIONS_USEMOUSE:
			{
				// FIXME
//              extern int win_use_mouse;
//              win_use_mouse = !win_use_mouse;
			}
			break;

		case ID_FILE_TOGGLEMENUBAR:
			win_toggle_menubar();
			break;

		case ID_FRAMESKIP_AUTO:
			window->machine().video().set_frameskip(-1);
			window->machine().options().emu_options::set_value(OPTION_AUTOFRAMESKIP, 1, OPTION_PRIORITY_CMDLINE);
			break;

		case ID_HELP_ABOUT_NEWUI:
			help_about_mess(wnd);
			break;

		case ID_HELP_ABOUTSYSTEM:
			help_about_thissystem(window->machine(), wnd);
			break;

		case ID_THROTTLE_50:
			set_speed(window->machine(), 500);
			break;

		case ID_THROTTLE_100:
			set_speed(window->machine(), 1000);
			break;

		case ID_THROTTLE_200:
			set_speed(window->machine(), 2000);
			break;

		case ID_THROTTLE_500:
			set_speed(window->machine(), 5000);
			break;

		case ID_THROTTLE_1000:
			set_speed(window->machine(), 10000);
			break;

		case ID_THROTTLE_UNTHROTTLED:
			set_speed(window->machine(), 0);
			break;

		default:
			if ((command >= ID_FRAMESKIP_0) && (command < ID_FRAMESKIP_0 + frameskip_level_count(window->machine())))
			{
				// change frameskip
				window->machine().video().set_frameskip(command - ID_FRAMESKIP_0);
				window->machine().options().emu_options::set_value(OPTION_AUTOFRAMESKIP, 0, OPTION_PRIORITY_CMDLINE);
				window->machine().options().emu_options::set_value(OPTION_FRAMESKIP, (int)command - ID_FRAMESKIP_0, OPTION_PRIORITY_CMDLINE);
			}
			else
			if ((command >= ID_DEVICE_0) && (command < ID_DEVICE_0 + (IO_COUNT*DEVOPTION_MAX)))
			{
				// change device
				img = decode_deviceoption(window->machine(), command, &dev_command);
				device_command(wnd, img, dev_command);
			}
			else
			if ((command >= ID_JOYSTICK_0) && (command < ID_JOYSTICK_0 + MAX_JOYSTICKS))
				// customise joystick
				customise_joystick(window->machine(), wnd, command - ID_JOYSTICK_0);
			else
			if ((command >= ID_VIDEO_VIEW_0) && (command < ID_VIDEO_VIEW_0 + 1000))
			{
				// render views
				window->m_target->set_view(command - ID_VIDEO_VIEW_0);
				window->update(); // actually change window size
			}
			else
			if ((command >= 3400) && (command < 4000))
			{
				slot_option &opt(window->machine().options().slot_option(slot_map[command].slotname.c_str()));
				opt.specify(slot_map[command].optname.c_str());
				window->machine().schedule_hard_reset();
			}
			else
				// bogus command
				handled = false;

			break;
	}

	// resume emulation
	if (pause_for_command(command))
		winwindow_ui_pause(window->machine(), false);

	return handled;
}



//============================================================
//  set_menu_text
//============================================================

static void set_menu_text(HMENU menu_bar, int command, const char *text)
{
	TCHAR *t_text;
	MENUITEMINFO mii;

	// convert to TCHAR
	t_text = ui_wstring_from_utf8(text);

	// invoke SetMenuItemInfo()
	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_TYPE;
	mii.dwTypeData = t_text;
	SetMenuItemInfo(menu_bar, command, false, &mii);

	// cleanup
	free(t_text);
}



//============================================================
//  win_setup_menus
//============================================================

static int win_setup_menus(running_machine &machine, HMODULE module, HMENU menu_bar)
{
	HMENU frameskip_menu;
	char buf[256];
	int i = 0;

	// verify that our magic numbers work
	assert((ID_DEVICE_0 + IO_COUNT * DEVOPTION_MAX) < ID_JOYSTICK_0);

	// initialize critical values
	joystick_menu_setup = 0;

	// set up frameskip menu
	frameskip_menu = find_sub_menu(menu_bar, "&Options\0&Frameskip\0", false);

	if (!frameskip_menu)
		return 1;

	for(i = 0; i < frameskip_level_count(machine); i++)
	{
		snprintf(buf, ARRAY_LENGTH(buf), "%i", i);
		win_append_menu_utf8(frameskip_menu, MF_STRING, ID_FRAMESKIP_0 + i, buf);
	}

	// set the help menu to refer to this machine
	snprintf(buf, ARRAY_LENGTH(buf), "About %s (%s)...", machine.system().type.fullname(), machine.system().name);
	set_menu_text(menu_bar, ID_HELP_ABOUTSYSTEM, buf);

	// initialize state_filename for each driver, so we don't carry names in-between them
	{
		char *src;
		char *dst;

		snprintf(state_filename, ARRAY_LENGTH(state_filename), "%s State", machine.system().type.fullname());

		src = state_filename;
		dst = state_filename;
		do
		{
			if ((*src == '\0') || isalnum(*src) || isspace(*src) || strchr("(),.", *src))
				*(dst++) = *src;
		}
		while(*(src++));
	}

	return 0;
}



//============================================================
//  win_resource_module
//============================================================

static HMODULE win_resource_module(void)
{
	static HMODULE module;
	if (module == NULL)
	{
		MEMORY_BASIC_INFORMATION info;
		if ((VirtualQuery((const void*)win_resource_module, &info, sizeof(info))) == sizeof(info))
			module = (HMODULE)info.AllocationBase;
	}
	return module;
}

///// Interface to the core /////

//============================================================
//  win_create_menu
//============================================================

int win_create_menu(running_machine &machine, HMENU *menus)
{
	// do not show in the mewui ui.
	if (strcmp(machine.system().name, "___empty") == 0)
		return 0;

	HMODULE module = win_resource_module();
	HMENU menu_bar = LoadMenu(module, MAKEINTRESOURCE(IDR_RUNTIME_MENU));

	if (!menu_bar)
	{
		printf("No memory for the menu, running without it.\n");
		return 0;
	}

	if (win_setup_menus(machine, module, menu_bar))
	{
		printf("Unable to setup the menu, running without it.\n");
		if (menu_bar)
			DestroyMenu(menu_bar);
		return 0; // return 1 causes a crash
	}

	*menus = menu_bar;
	return 0;
}



//============================================================
//  winwindow_video_window_proc_ui
//============================================================

LRESULT CALLBACK winwindow_video_window_proc_ui(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch(message)
	{
		case WM_INITMENU:
			prepare_menus(wnd);
			break;

		case WM_PASTE:
			{
				mame_machine_manager::instance()->ui().paste();
			}
			break;

		case WM_COMMAND:
			if (invoke_command(wnd, wparam))
				break;
			/* fall through */

		default:
			return win_window_info::video_window_proc(wnd, message, wparam, lparam);
	}
	return 0;
}

