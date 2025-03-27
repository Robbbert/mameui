// For licensing and usage information, read docs/release/winui_license.txt
//****************************************************************************

#ifndef WINUI_DIRWATCHER_H
#define WINUI_DIRWATCHER_H

#include <string>
typedef struct DirWatcher *PDIRWATCHER;

PDIRWATCHER DirWatcher_Init(HWND hwndTarget, UINT nMessage);
void DirWatcher_Watch(PDIRWATCHER, WORD nIndex, std::string, BOOL);
void DirWatcher_Free(PDIRWATCHER);

#endif

