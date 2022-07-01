// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include "winui.h"

/***************************************************************************
    function prototypes
 ***************************************************************************/

static int DIJoystick_init(void);
static void DIJoystick_exit(void);
static void DIJoystick_poll_joysticks(void);
static int DIJoystick_is_joy_pressed(int joycode);
static bool DIJoystick_Available(void);
static int CALLBACK DIJoystick_EnumDeviceProc(LPDIDEVICEINSTANCE pdidi, LPVOID pv);

/***************************************************************************
    External variables
 ***************************************************************************/

const struct OSDJoystick  DIJoystick =
{
	DIJoystick_init,                /* init              */
	DIJoystick_exit,                /* exit              */
	DIJoystick_is_joy_pressed,      /* joy_pressed       */
	DIJoystick_poll_joysticks,      /* poll_joysticks    */
	DIJoystick_Available,           /* Available         */
};

/***************************************************************************
    Internal structures
 ***************************************************************************/

typedef struct
{
	GUID guid;
	TCHAR *name;
	int offset; /* offset in dijoystate */
} axis_type;

typedef struct
{
	bool use_joystick;
	GUID guidDevice;
	TCHAR *name;
	bool is_light_gun;
	LPDIRECTINPUTDEVICE2 did;
	DWORD num_axes;
	axis_type axes[MAX_AXES];
	DWORD num_pov;
	DWORD num_buttons;
	DIJOYSTATE dijs;

} joystick_type;

struct tDIJoystick_private
{
	int   use_count; /* the gui and game can both init/exit us, so keep track */
	bool  m_bCoinSlot;
	DWORD num_joysticks;
	joystick_type joysticks[MAX_PHYSICAL_JOYSTICKS]; /* actual joystick data! */
};

/* internal functions needing our declarations */
static int CALLBACK DIJoystick_EnumAxisObjectsProc(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef);
static int CALLBACK DIJoystick_EnumPOVObjectsProc(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef);
static int CALLBACK DIJoystick_EnumButtonObjectsProc(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef);
static void ClearJoyState(DIJOYSTATE *pdijs);
static void InitJoystick(joystick_type *joystick);
static void ExitJoystick(joystick_type *joystick);
static int CALLBACK inputEnumDeviceProc(LPCDIDEVICEINSTANCE pdidi, LPVOID pv);
static HRESULT SetDIDwordProperty(LPDIRECTINPUTDEVICE2 pdev, REFGUID guidProperty, DWORD dwObject, DWORD dwHow, DWORD dwValue);

/***************************************************************************
    Internal variables
 ***************************************************************************/

static struct tDIJoystick_private   This;
static const GUID guidNULL = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
static LPDIRECTINPUT dinp = NULL;
static bool bBeenHere = false;
static bool bAvailable = false;

/***************************************************************************
    External OSD functions
 ***************************************************************************/
/*
    put here anything you need to do when the program is started. Return 0 if
    initialization was successful, nonzero otherwise.
*/
typedef HRESULT (WINAPI *dic_proc)(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUT *ppDI, LPUNKNOWN punkOuter);

bool DirectInputInitialize(void)
{
	HRESULT hr;
	dic_proc dic;

	/* Turn off error dialog for this call */
	UINT error_mode = SetErrorMode(0);
	HANDLE hDLL = LoadLibrary(TEXT("dinput.dll"));
	SetErrorMode(error_mode);

	if (hDLL == NULL)
		return false;

	dic = (dic_proc)GetProcAddress((HINSTANCE)hDLL, "DirectInputCreateW");

	if (dic == NULL)
		return false;

	hr = dic(GetModuleHandle(NULL), 0x0700, &dinp, NULL);

	if (FAILED(hr))
	{
		hr = dic(GetModuleHandle(NULL), 0x0500, &dinp, NULL);

		if (FAILED(hr))
		{
			hr = dic(GetModuleHandle(NULL), 0x0300, &dinp, NULL);
			
			if (FAILED(hr))
			{
				ErrorMessageBox("DirectInputCreate failed! error=%x\n", (unsigned int)hr);
				dinp = NULL;
				return false;
			}
		}
	}

	return true;
}

static int DIJoystick_init(void)
{
	DWORD i = 0;
	HRESULT hr;
	LPDIRECTINPUT di = dinp;

	This.use_count++;
	This.num_joysticks = 0;

	if (di == NULL)
	{
		ErrorMessageBox("DirectInput not initialized");
		return 0;
	}

	/* enumerate for joystick devices */
	hr = IDirectInput_EnumDevices(di, DIDEVTYPE_JOYSTICK, (LPDIENUMDEVICESCALLBACK)DIJoystick_EnumDeviceProc,
		NULL, DIEDFL_ATTACHEDONLY);

	if (FAILED(hr))
	{
		ErrorMessageBox("DirectInput EnumDevices() failed: %s", DirectXDecodeError(hr));
		return 0;
	}

	/* create each joystick device, enumerate each joystick for axes, etc */
	for (i = 0; i < This.num_joysticks; i++)
	{
		InitJoystick(&This.joysticks[i]);
	}

	/* Are there any joysticks attached? */
	if (This.num_joysticks < 1)
		return 0;

	return 0;
}

/*
    put here cleanup routines to be executed when the program is terminated.
*/
void DirectInputClose(void)
{
	/* Release any lingering IDirectInput object. */
	if (dinp)
	{
		IDirectInput_Release(dinp);
		dinp = NULL;
	}
}

static void DIJoystick_exit(void)
{
	DWORD i = 0;

	This.use_count--;

	if (This.use_count > 0)
		return;

	for (i = 0; i < This.num_joysticks; i++)
		ExitJoystick(&This.joysticks[i]);

	This.num_joysticks = 0;
}

static void DIJoystick_poll_joysticks(void)
{
	HRESULT hr;
	DWORD i = 0;

	This.m_bCoinSlot = 0;

	for (i = 0; i < This.num_joysticks; i++)
	{
		/* start by clearing the structure, then fill it in if possible */

		ClearJoyState(&This.joysticks[i].dijs);

		if (This.joysticks[i].did == NULL)
			continue;

		if (This.joysticks[i].use_joystick == false)
			continue;

		IDirectInputDevice2_Poll(This.joysticks[i].did);
		hr = IDirectInputDevice2_GetDeviceState(This.joysticks[i].did, sizeof(DIJOYSTATE), &This.joysticks[i].dijs);

		if (FAILED(hr))
		{
			if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
				hr = IDirectInputDevice2_Acquire(This.joysticks[i].did);

			continue;
		}
	}
}

/*
    check if the DIJoystick is moved in the specified direction, defined in
    osdepend.h. Return 0 if it is not pressed, nonzero otherwise.
*/
static int DIJoystick_is_joy_pressed(int joycode)
{
	DIJOYSTATE dijs;
	int num_pov = GET_JOYCODE_BUTTON(joycode) / 4;
	int code = GET_JOYCODE_BUTTON(joycode) % 4;
	int joy_num = GET_JOYCODE_JOY(joycode);
	int stick = GET_JOYCODE_STICK(joycode);
	int axis = GET_JOYCODE_AXIS(joycode);
	int dir  = GET_JOYCODE_DIR(joycode);

	/* do we have as many sticks? */
	if (joy_num == 0 || This.num_joysticks < joy_num)
		return 0;

	joy_num--;

	if (This.joysticks[joy_num].use_joystick == false)
		return 0;

	dijs = This.joysticks[joy_num].dijs;

	if (stick == JOYCODE_STICK_BTN)
	{
		int button = GET_JOYCODE_BUTTON(joycode);
		button--;

		if (button >= This.joysticks[joy_num].num_buttons || GET_JOYCODE_DIR(joycode) != JOYCODE_DIR_BTN)
			return 0;

		return dijs.rgbButtons[button] != 0;
	}

	if (stick == JOYCODE_STICK_POV)
	{
		axis = code / 2;
		dir  = code % 2;

		if (num_pov >= This.joysticks[joy_num].num_pov)
			return 0;

		int pov_value = dijs.rgdwPOV[num_pov];

		if (LOWORD(pov_value) == 0xffff)
			return 0;

		int angle = (pov_value + 27000) % 36000;
		angle = (36000 - angle) % 36000;
		angle /= 100;
		int axis_value = 0;

		/* angle is now in degrees counterclockwise from x axis*/
		if (axis == 1)
			axis_value = 128 + (int)(127 * cos(2 * M_PI * angle / 360.0)); /* x */
		else
			axis_value = 128 + (int)(127 * sin(2 * M_PI * angle / 360.0)); /* y */

		if (dir == 1)
			return axis_value <= (128 - 128 * 60 / 100);
		else
			return axis_value >= (128 + 128 * 60 / 100);
	}

	/* sticks */
	if (axis == 0 || This.joysticks[joy_num].num_axes < axis)
		return 0;

	axis--;
	int value = *(int *)(((byte *)&dijs) + This.joysticks[joy_num].axes[axis].offset);

	if (dir == JOYCODE_DIR_NEG)
		return value <= (128 - 128 * 60 / 100);
	else
		return value >= (128 + 128 * 60 / 100);
}

static bool DIJoystick_Available(void)
{
	HRESULT hr;
	GUID guidDevice = guidNULL;
	LPDIRECTINPUTDEVICE didTemp;
	LPDIRECTINPUTDEVICE didJoystick;
	LPDIRECTINPUT di = dinp;

	if (di == NULL)
		return false;

	if (bBeenHere == false)
		bBeenHere = true;
	else
		return bAvailable;

	/* enumerate for joystick devices */
	hr = IDirectInput_EnumDevices(di, DIDEVTYPE_JOYSTICK, inputEnumDeviceProc, &guidDevice, DIEDFL_ATTACHEDONLY);

	if (FAILED(hr))
		return false;

	/* Are there any joysticks attached? */
	if (IsEqualGUID(guidDevice, guidNULL))
		return false;

	hr = IDirectInput_CreateDevice(di, guidDevice, &didTemp, NULL);

	if (FAILED(hr))
		return false;

	/* Determine if DX5 is available by a QI for a DX5 interface. */
	hr = IDirectInputDevice_QueryInterface(didTemp, IID_IDirectInputDevice2, (void**)&didJoystick);

	if (FAILED(hr))
		bAvailable = false;
	else
	{
		bAvailable = true;
		IDirectInputDevice_Release(didJoystick);
	}

	/* dispose of the temp interface */
	IDirectInputDevice_Release(didTemp);
	return bAvailable;
}

/***************************************************************************
    Internal functions
 ***************************************************************************/

static int CALLBACK DIJoystick_EnumDeviceProc(LPDIDEVICEINSTANCE pdidi, LPVOID pv)
{
	TCHAR buffer[5000];

	This.joysticks[This.num_joysticks].guidDevice = pdidi->guidInstance;
	_sntprintf(buffer, WINUI_ARRAY_LENGTH(buffer), TEXT("%s (%s)"), pdidi->tszProductName, pdidi->tszInstanceName);
	This.joysticks[This.num_joysticks].name = (TCHAR *)malloc((_tcslen(buffer) + 1) * sizeof(TCHAR));
	_tcscpy(This.joysticks[This.num_joysticks].name, buffer);
	This.num_joysticks++;
	return DIENUM_CONTINUE;
}

static int CALLBACK DIJoystick_EnumAxisObjectsProc(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef)
{
	joystick_type* joystick = (joystick_type*)pvRef;
	DIPROPRANGE diprg;
	HRESULT hr;

	joystick->axes[joystick->num_axes].guid = lpddoi->guidType;
	joystick->axes[joystick->num_axes].name = (TCHAR *)malloc((_tcslen(lpddoi->tszName) + 1) * sizeof(TCHAR));
	_tcscpy(joystick->axes[joystick->num_axes].name, lpddoi->tszName);
	joystick->axes[joystick->num_axes].offset = lpddoi->dwOfs;

	/*ErrorMessageBox("got axis %s, offset %i",lpddoi->tszName, lpddoi->dwOfs);*/

	memset(&diprg, 0, sizeof(DIPROPRANGE));
	diprg.diph.dwSize = sizeof(DIPROPRANGE);
	diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	diprg.diph.dwObj = lpddoi->dwOfs;
	diprg.diph.dwHow = DIPH_BYOFFSET;
	diprg.lMin = 0;
	diprg.lMax = 255;

	hr = IDirectInputDevice2_SetProperty(joystick->did, DIPROP_RANGE, &diprg.diph);

	if (FAILED(hr)) /* if this fails, don't use this axis */
	{
		free(joystick->axes[joystick->num_axes].name);
		joystick->axes[joystick->num_axes].name = NULL;
		return DIENUM_CONTINUE;
	}

#ifdef JOY_DEBUG
	if (FAILED(hr))
	{
		ErrorMessageBox("DirectInput SetProperty() joystick axis %s failed - %s\n",
				 joystick->axes[joystick->num_axes].name,
				 DirectXDecodeError(hr));
	}
#endif

	/* Set axis dead zone to 0; we need accurate #'s for analog joystick reading. */
	SetDIDwordProperty(joystick->did, DIPROP_DEADZONE, lpddoi->dwOfs, DIPH_BYOFFSET, 0);

#ifdef JOY_DEBUG
	if (FAILED(hr))
	{
		ErrorMessageBox("DirectInput SetProperty() joystick axis %s dead zone failed - %s\n",
				 joystick->axes[joystick->num_axes].name,
				 DirectXDecodeError(hr));
	}
#endif

	joystick->num_axes++;
	return DIENUM_CONTINUE;
}

static int CALLBACK DIJoystick_EnumPOVObjectsProc(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef)
{
	joystick_type* joystick = (joystick_type*)pvRef;
	joystick->num_pov++;
	return DIENUM_CONTINUE;
}

static int CALLBACK DIJoystick_EnumButtonObjectsProc(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef)
{
	joystick_type* joystick = (joystick_type*)pvRef;
	joystick->num_buttons++;
	return DIENUM_CONTINUE;
}

static void ClearJoyState(DIJOYSTATE *pdijs)
{
	memset(pdijs, 0, sizeof(DIJOYSTATE));
	pdijs->lX = 128;
	pdijs->lY = 128;
	pdijs->lZ = 128;
	pdijs->lRx = 128;
	pdijs->lRy = 128;
	pdijs->lRz = 128;
	pdijs->rglSlider[0] = 128;
	pdijs->rglSlider[1] = 128;
	pdijs->rgdwPOV[0] = -1;
	pdijs->rgdwPOV[1] = -1;
	pdijs->rgdwPOV[2] = -1;
	pdijs->rgdwPOV[3] = -1;
}

static void InitJoystick(joystick_type *joystick)
{
	LPDIRECTINPUTDEVICE didTemp;
	HRESULT hr;
	LPDIRECTINPUT di = dinp;

	joystick->use_joystick = false;
	joystick->did = NULL;
	joystick->num_axes = 0;
	joystick->is_light_gun = (_tcscmp(joystick->name, TEXT("ACT LABS GS (ACT LABS GS)")) == 0);

	/* get a did1 interface first... */
	hr = IDirectInput_CreateDevice(di, joystick->guidDevice, &didTemp, NULL);

	if (FAILED(hr))
	{
		ErrorMessageBox("DirectInput CreateDevice() joystick failed: %s\n", DirectXDecodeError(hr));
		return;
	}

	/* get a did2 interface to work with polling (most) joysticks */
	hr = IDirectInputDevice_QueryInterface(didTemp, IID_IDirectInputDevice2, (void**)&joystick->did);
	/* dispose of the temp interface */
	IDirectInputDevice_Release(didTemp);

	/* check result of getting the did2 */
	if (FAILED(hr))
	{
		/* no error message because this happens in dx3 */
		/* ErrorMessageBox("DirectInput QueryInterface joystick failed\n"); */
		joystick->did = NULL;
		return;
	}

	hr = IDirectInputDevice2_SetCooperativeLevel(joystick->did, GetMainWindow(), DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

	if (FAILED(hr))
	{
		ErrorMessageBox("DirectInput SetCooperativeLevel() joystick failed: %s\n", DirectXDecodeError(hr));
		return;
	}

	hr = IDirectInputDevice2_SetDataFormat(joystick->did, &c_dfDIJoystick);

	if (FAILED(hr))
	{
		ErrorMessageBox("DirectInput SetDataFormat() joystick failed: %s\n", DirectXDecodeError(hr));
		return;
	}

	if (joystick->is_light_gun)
	{
		/* setup light gun to report raw screen pixel data */
		DIPROPDWORD diprop;

		memset(&diprop, 0, sizeof(DIPROPDWORD));
		diprop.diph.dwSize	= sizeof(DIPROPDWORD);
		diprop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		diprop.diph.dwObj = 0;
		diprop.diph.dwHow = DIPH_DEVICE;
		diprop.dwData = DIPROPCALIBRATIONMODE_RAW;

		IDirectInputDevice2_SetProperty(joystick->did, DIPROP_CALIBRATIONMODE, &diprop.diph);
	}
	else
	{
		/* enumerate our axes */
		hr = IDirectInputDevice_EnumObjects(joystick->did, DIJoystick_EnumAxisObjectsProc, joystick, DIDFT_AXIS);

		if (FAILED(hr))
		{
			ErrorMessageBox("DirectInput EnumObjects() Axes failed: %s\n", DirectXDecodeError(hr));
			return;
		}

		/* enumerate our POV hats */
		joystick->num_pov = 0;
		hr = IDirectInputDevice_EnumObjects(joystick->did, DIJoystick_EnumPOVObjectsProc, joystick, DIDFT_POV);

		if (FAILED(hr))
		{
			ErrorMessageBox("DirectInput EnumObjects() POVs failed: %s\n", DirectXDecodeError(hr));
			return;
		}
	}

	/* enumerate our buttons */
	joystick->num_buttons = 0;
	hr = IDirectInputDevice_EnumObjects(joystick->did, DIJoystick_EnumButtonObjectsProc, joystick, DIDFT_BUTTON);

	if (FAILED(hr))
	{
		ErrorMessageBox("DirectInput EnumObjects() Buttons failed: %s\n", DirectXDecodeError(hr));
		return;
	}

	hr = IDirectInputDevice2_Acquire(joystick->did);

	if (FAILED(hr))
	{
		ErrorMessageBox("DirectInputDevice Acquire joystick failed!\n");
		return;
	}

	/* start by clearing the structures */
	ClearJoyState(&joystick->dijs);
	joystick->use_joystick = true;
}

static void ExitJoystick(joystick_type *joystick)
{
	DWORD i = 0;

	if (joystick->did != NULL)
	{
		IDirectInputDevice_Unacquire(joystick->did);
		IDirectInputDevice_Release(joystick->did);
		joystick->did = NULL;
	}

	for (i = 0; i < joystick->num_axes; i++)
	{
		if (joystick->axes[i].name)
			free(joystick->axes[i].name);
		joystick->axes[i].name = NULL;
	}

	if (joystick->name != NULL)
	{
		free(joystick->name);
		joystick->name = NULL;
	}
}

static int CALLBACK inputEnumDeviceProc(LPCDIDEVICEINSTANCE pdidi, LPVOID pv)
{
	/* report back the instance guid of the device we enumerated */
	if (pv)
	{
		GUID *pguidDevice  = (GUID *)pv;
		*pguidDevice = pdidi->guidInstance;
	}

	/* BUGBUG for now, stop after the first device has been found */
	return DIENUM_STOP;
}

static HRESULT SetDIDwordProperty(LPDIRECTINPUTDEVICE2 pdev, REFGUID guidProperty, DWORD dwObject, DWORD dwHow, DWORD dwValue)
{
	DIPROPDWORD dipdw;

	memset(&dipdw, 0, sizeof(DIPROPDWORD));
	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = dwObject;
	dipdw.diph.dwHow = dwHow;
	dipdw.dwData = dwValue;

	return IDirectInputDevice2_SetProperty(pdev, guidProperty, &dipdw.diph);
}
