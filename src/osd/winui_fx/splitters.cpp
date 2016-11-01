// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#include "winui.h"

/* Local Variables */
static int firstTime = true;
static bool bTracking = 0;
static int numSplitters = 0;
static int currentSplitter = 0;
static HZSPLITTER *splitter;
static LPHZSPLITTER lpCurSpltr = 0;
static HCURSOR hSplitterCursor = 0;
int *nSplitterOffset;
static void AdjustSplitter2Rect(HWND hWnd, LPRECT lpRect);
static void AdjustSplitter1Rect(HWND hWnd, LPRECT lpRect);

extern const SPLITTERINFO g_splitterInfo[] =
{
	{ 0.5,	IDC_SPLITTER,	IDC_TREE,	IDC_LIST,		AdjustSplitter1Rect },
	{ 0.5,	IDC_SPLITTER2,	IDC_LIST,	IDC_SSFRAME,	AdjustSplitter2Rect },
	{ -1 }
};


bool InitSplitters(void)
{
	/* load the cursor for the splitter */
	hSplitterCursor = LoadCursor(GetModuleHandle(0), MAKEINTRESOURCE(IDC_CURSOR_HSPLIT));
	int nSplitterCount = GetSplitterCount();
	splitter = (HZSPLITTER*)malloc(sizeof(HZSPLITTER) * nSplitterCount);

	if (!splitter)
		goto error;

	memset(splitter, 0, sizeof(HZSPLITTER) * nSplitterCount);
	nSplitterOffset = (int*)malloc(sizeof(int) * nSplitterCount);

	if (!nSplitterOffset)
		goto error;

	memset(nSplitterOffset, 0, sizeof(int) * nSplitterCount);
	return true;

error:
	SplittersExit();
	return false;
}

void SplittersExit(void)
{
	if (splitter)
	{
		free(splitter);
		splitter = NULL;
	}

	if (nSplitterOffset)
	{
		free(nSplitterOffset);
		nSplitterOffset = NULL;
	}
}

/* Called with hWnd = Parent Window */
static void CalcSplitter(HWND hWnd, LPHZSPLITTER lpSplitter)
{
	POINT p = {0,0};
	RECT leftRect, rightRect;

	ClientToScreen(hWnd, &p);
	GetWindowRect(lpSplitter->m_hWnd, &lpSplitter->m_dragRect);
	GetWindowRect(lpSplitter->m_hWndLeft, &leftRect);
	GetWindowRect(lpSplitter->m_hWndRight, &rightRect);
	OffsetRect(&lpSplitter->m_dragRect, -p.x, -p.y);
	OffsetRect(&leftRect, -p.x, -p.y);
	OffsetRect(&rightRect, -p.x, -p.y);
	int dragWidth = lpSplitter->m_dragRect.right - lpSplitter->m_dragRect.left;
	lpSplitter->m_limitRect.left = leftRect.left + 20;
	lpSplitter->m_limitRect.right = rightRect.right - 20;
	lpSplitter->m_limitRect.top = lpSplitter->m_dragRect.top;
	lpSplitter->m_limitRect.bottom = lpSplitter->m_dragRect.bottom;

	if (lpSplitter->m_func)
		lpSplitter->m_func(hWnd, &lpSplitter->m_limitRect);

	if (lpSplitter->m_dragRect.left < lpSplitter->m_limitRect.left)
		lpSplitter->m_dragRect.left = lpSplitter->m_limitRect.left;

	if (lpSplitter->m_dragRect.right > lpSplitter->m_limitRect.right)
		lpSplitter->m_dragRect.left = lpSplitter->m_limitRect.right - dragWidth;

	lpSplitter->m_dragRect.right = lpSplitter->m_dragRect.left + dragWidth;
}

static void AdjustSplitter2Rect(HWND hWnd, LPRECT lpRect)
{
	RECT pRect;

	GetClientRect(hWnd, &pRect);

	if (lpRect->right > pRect.right)
		lpRect->right = pRect.right;

	lpRect->right = MIN(lpRect->right - GetMinimumScreenShotWindowWidth(), lpRect->right);
	lpRect->left = MAX((pRect.right - (pRect.right - pRect.left)) / 2, lpRect->left);
}

static void AdjustSplitter1Rect(HWND hWnd, LPRECT lpRect)
{
	// nothing here to do...
}

void RecalcSplitters(void)
{
	for (int i = 0; i < numSplitters; i++)
	{
		CalcSplitter(GetParent(splitter[i].m_hWnd), &splitter[i]);
		nSplitterOffset[i] = splitter[i].m_dragRect.left;
	}

	for (int i = numSplitters - 1; i >= 0; i--)
	{
		CalcSplitter(GetParent(splitter[i].m_hWnd), &splitter[i]);
		nSplitterOffset[i] = splitter[i].m_dragRect.left;
	}

}

void OnSizeSplitter(HWND hWnd)
{
	int changed = false;
	RECT rWindowRect;
	POINT p;

	int nSplitterCount = GetSplitterCount();

	if (firstTime)
	{
		for (int i = 0; i < nSplitterCount; i++)
			nSplitterOffset[i] = GetSplitterPos(i);

		changed = true;
		firstTime = false;
	}

	GetWindowRect(hWnd, &rWindowRect);

	for (int i = 0; i < nSplitterCount; i++)
	{
		p.x = 0;
		p.y = 0;
		ClientToScreen(splitter[i].m_hWnd, &p);
		/* We must change if our window is not in the window rect */
		bool bMustChange = !PtInRect(&rWindowRect, p);

		/* We should also change if we are ahead the next splitter */
		if ((i < nSplitterCount - 1) && (nSplitterOffset[i] >= nSplitterOffset[i + 1]))
			bMustChange = true;

		/* ...or if we are behind the previous splitter */
		if ((i > 0) && (nSplitterOffset[i] <= nSplitterOffset[i - 1]))
			bMustChange = true;

		if (bMustChange)
		{
			nSplitterOffset[i] = (rWindowRect.right - rWindowRect.left) * g_splitterInfo[i].dPosition;
			changed = true;
		}
	}

	if (changed)
	{
		ResizePickerControls(hWnd);
		RecalcSplitters();
		UpdateScreenShot();
	}
}

void AddSplitter(HWND hWnd, HWND hWndLeft, HWND hWndRight, void (*func)(HWND hWnd,LPRECT lpRect))
{
	LPHZSPLITTER lpSpltr = &splitter[numSplitters];

	if (numSplitters >= GetSplitterCount())
		return;

	lpSpltr->m_hWnd = hWnd;
	lpSpltr->m_hWndLeft = hWndLeft;
	lpSpltr->m_hWndRight = hWndRight;
	lpSpltr->m_func = func;
	CalcSplitter(GetParent(hWnd), lpSpltr);
	numSplitters++;
}

static void OnInvertTracker(HWND hWnd, const RECT *rect)
{
	HDC hDC = GetDC(hWnd);
	HBRUSH hBrush = GetSysColorBrush(COLOR_WINDOWTEXT);
	HBRUSH hOldBrush = NULL;

	if (hBrush != NULL)
		hOldBrush = (HBRUSH)SelectObject(hDC, hBrush);

	PatBlt(hDC, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, DSTINVERT);

	if (hOldBrush != NULL)
		SelectObject(hDC, hOldBrush);

	ReleaseDC(hWnd, hDC);
}

static void StartTracking(HWND hWnd, UINT hitArea)
{
	if (!bTracking && lpCurSpltr != 0 && hitArea == SPLITTER_HITITEM)
	{
		// disable main window style
		LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
		style &= ~WS_CLIPCHILDREN;
		SetWindowLongPtr(hWnd, GWL_STYLE, style);
		// Ensure we have and updated cursor structure
		CalcSplitter(hWnd, lpCurSpltr);
		// Draw the first splitter shadow
		OnInvertTracker(hWnd, &lpCurSpltr->m_dragRect);
		// Capture the mouse
		SetCapture(hWnd);
		// Set tracking to true
		bTracking = true;
		SetCursor(hSplitterCursor);
	}
}

static void StopTracking(HWND hWnd)
{
	if (bTracking)
	{
		// erase the tracking image
		OnInvertTracker(hWnd, &lpCurSpltr->m_dragRect);
		// Release the mouse
		ReleaseCapture();
		// set tracking to false
		bTracking = false;
		SetCursor(LoadCursor(0, IDC_ARROW));
		// set the new splitter position
		nSplitterOffset[currentSplitter] = lpCurSpltr->m_dragRect.left;
		// Redraw the screen area
		ResizePickerControls(hWnd);
		UpdateScreenShot();
		InvalidateRect(GetMainWindow(), NULL, true);
		// enable main window style
		LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
		SetWindowLongPtr(hWnd, GWL_STYLE, style | WS_CLIPCHILDREN);
	}
}

static UINT SplitterHitTest(HWND hWnd, POINTS p)
{
	RECT rect;
	POINT pt;

	pt.x = p.x;
	pt.y = p.y;
	// Check which area we hit
	ClientToScreen(hWnd, &pt);

	for (int i = 0; i < numSplitters; i++)
	{
		GetWindowRect(splitter[i].m_hWnd, &rect);

		if (PtInRect(&rect, pt))
		{
			lpCurSpltr = &splitter[i];
			currentSplitter = i;
			// We hit the splitter
			return SPLITTER_HITITEM;
		}
	}

	lpCurSpltr = 0;
	// We missed the splitter
	return SPLITTER_HITNOTHING;
}

void OnMouseMove(HWND hWnd, UINT nFlags, POINTS p)
{
	RECT rect;
	POINT pt;

	if (bTracking)	// move the tracking image
	{
		pt.x = (int)p.x;
		pt.y = (int)p.y;
		ClientToScreen(hWnd, &pt);
		GetWindowRect(hWnd, &rect);

		if (!PtInRect(&rect, pt))
		{
			if ((short)pt.x < (short)rect.left)
				pt.x = rect.left;

			if ((short)pt.x > (short)rect.right)
				pt.x = rect.right;

			pt.y = rect.top + 1;
		}

		ScreenToClient(hWnd, &pt);
		// Erase the old tracking image
		OnInvertTracker(hWnd, &lpCurSpltr->m_dragRect);
		// calc the new one based on p.x draw it
		int nWidth = lpCurSpltr->m_dragRect.right - lpCurSpltr->m_dragRect.left;
		lpCurSpltr->m_dragRect.right = pt.x + nWidth / 2;
		lpCurSpltr->m_dragRect.left  = pt.x - nWidth / 2;

		if (pt.x - nWidth / 2 > lpCurSpltr->m_limitRect.right)
		{
			lpCurSpltr->m_dragRect.right = lpCurSpltr->m_limitRect.right;
			lpCurSpltr->m_dragRect.left  = lpCurSpltr->m_dragRect.right - nWidth;
		}
		
		if (pt.x + nWidth / 2 < lpCurSpltr->m_limitRect.left)
		{
			lpCurSpltr->m_dragRect.left  = lpCurSpltr->m_limitRect.left;
			lpCurSpltr->m_dragRect.right = lpCurSpltr->m_dragRect.left + nWidth;
		}
		
		OnInvertTracker(hWnd, &lpCurSpltr->m_dragRect);
	}
	else
	{
		switch(SplitterHitTest(hWnd, p))
		{
		case SPLITTER_HITNOTHING:
			SetCursor(LoadCursor(0, IDC_ARROW));
			break;

		case SPLITTER_HITITEM:
			SetCursor(hSplitterCursor);
			break;
		}
	}
}

void OnLButtonDown(HWND hWnd, UINT nFlags, POINTS p)
{
	if (!bTracking) // See if we need to start a splitter drag
		StartTracking(hWnd, SplitterHitTest(hWnd, p));
}

void OnLButtonUp(HWND hWnd, UINT nFlags, POINTS p)
{
	if (bTracking)
		StopTracking(hWnd);
}

int GetSplitterCount(void)
{
	int nSplitterCount = 0;

	while(g_splitterInfo[nSplitterCount].dPosition > 0)
		nSplitterCount++;

	return nSplitterCount;
}
