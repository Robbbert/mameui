// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include "winui.h"

/***************************************************************/

typedef struct tagERRORCODE
{
	HRESULT hr;
	const char *szError;
} ERRORCODE, * LPERRORCODE;

/***************************************************************/
static const ERRORCODE g_ErrorCode[] =
{
	{   (HRESULT)DIERR_OLDDIRECTINPUTVERSION,        "DIERR_OLDDIRECTINPUTVERSION" },
	{   (HRESULT)DIERR_BETADIRECTINPUTVERSION,       "DIERR_BETADIRECTINPUTVERSION" },
	{   (HRESULT)DIERR_BADDRIVERVER,                 "DIERR_BADDRIVERVER" },
	{   (HRESULT)DIERR_DEVICENOTREG,                 "DIERR_DEVICENOTREG" },
	{   (HRESULT)DIERR_NOTFOUND,                     "DIERR_NOTFOUND" },
	{   (HRESULT)DIERR_OBJECTNOTFOUND,               "DIERR_OBJECTNOTFOUND" },
	{   (HRESULT)DIERR_INVALIDPARAM,                 "DIERR_INVALIDPARAM" },
	{   (HRESULT)DIERR_NOINTERFACE,                  "DIERR_NOINTERFACE" },
	{   (HRESULT)DIERR_GENERIC,                      "DIERR_GENERIC" },
	{   (HRESULT)DIERR_OUTOFMEMORY,                  "DIERR_OUTOFMEMORY" },
	{   (HRESULT)DIERR_UNSUPPORTED,                  "DIERR_UNSUPPORTED" },
	{   (HRESULT)DIERR_NOTINITIALIZED,               "DIERR_NOTINITIALIZED" },
	{   (HRESULT)DIERR_ALREADYINITIALIZED,           "DIERR_ALREADYINITIALIZED" },
	{   (HRESULT)DIERR_NOAGGREGATION,                "DIERR_NOAGGREGATION" },
	{   (HRESULT)DIERR_OTHERAPPHASPRIO,              "DIERR_OTHERAPPHASPRIO" },
	{   (HRESULT)DIERR_INPUTLOST,                    "DIERR_INPUTLOST" },
	{   (HRESULT)DIERR_ACQUIRED,                     "DIERR_ACQUIRED" },
	{   (HRESULT)DIERR_NOTACQUIRED,                  "DIERR_NOTACQUIRED" },
	{   (HRESULT)DIERR_READONLY,                     "DIERR_READONLY" },
	{   (HRESULT)DIERR_HANDLEEXISTS,                 "DIERR_HANDLEEXISTS" },
	{   (HRESULT)E_PENDING,                          "E_PENDING" },
	{   (HRESULT)DIERR_INSUFFICIENTPRIVS,            "DIERR_INSUFFICIENTPRIVS" },
	{   (HRESULT)DIERR_DEVICEFULL,                   "DIERR_DEVICEFULL" },
	{   (HRESULT)DIERR_MOREDATA,                     "DIERR_MOREDATA" },
	{   (HRESULT)DIERR_NOTDOWNLOADED,                "DIERR_NOTDOWNLOADED" },
	{   (HRESULT)DIERR_HASEFFECTS,                   "DIERR_HASEFFECTS" },
	{   (HRESULT)DIERR_NOTEXCLUSIVEACQUIRED,         "DIERR_NOTEXCLUSIVEACQUIRED" },
	{   (HRESULT)DIERR_INCOMPLETEEFFECT,             "DIERR_INCOMPLETEEFFECT" },
	{   (HRESULT)DIERR_NOTBUFFERED,                  "DIERR_NOTBUFFERED" },
	{   (HRESULT)DIERR_EFFECTPLAYING,                "DIERR_EFFECTPLAYING" },
	{   (HRESULT)E_NOINTERFACE,                      "E_NOINTERFACE" }
};

const char * DirectXDecodeError(HRESULT errorval)
{
	static char tmp[64];

	for (int i = 0; i < WINUI_ARRAY_LENGTH(g_ErrorCode); i++)
	{
		if (g_ErrorCode[i].hr == errorval)
		{
			return g_ErrorCode[i].szError;
		}
	}

	snprintf(tmp, WINUI_ARRAY_LENGTH(tmp), "UNKNOWN: 0x%x", (unsigned int)errorval);
	return tmp;
}
