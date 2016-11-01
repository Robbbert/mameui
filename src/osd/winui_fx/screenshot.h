// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#pragma once
 
#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#define WIDTHBYTES(width) ((width) / 8)

bool LoadScreenShot(int nGame, int nType);
HANDLE GetScreenShotHandle(void);
int GetScreenShotWidth(void);
int GetScreenShotHeight(void);
void FreeScreenShot(void);
bool ScreenShotLoaded(void);

#endif
