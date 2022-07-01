// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#pragma once 
 
#ifndef DIRECTORIES_H
#define DIRECTORIES_H

#define DIRDLG_ROM          1
#define MAX_DIRS            256
#define DIRLIST_NEWENTRYTEXT          "<insert new path here>"
#define DirInfo_NumDir(pInfo, path)     ((pInfo)[(path)].m_Path->m_NumDirectories)

INT_PTR CALLBACK DirectoriesDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);

#endif
