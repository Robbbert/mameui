// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#pragma once

#ifndef DATAFILE_H
#define DATAFILE_H

#define MAX_TOKEN_LENGTH        256
#define DATAFILE_TAG            '$'

int load_driver_history(const game_driver *drv, char *buffer, int bufsize);
int load_driver_initinfo(const game_driver *drv, char *buffer, int bufsize);
int load_driver_mameinfo(const game_driver *drv, char *buffer, int bufsize);
int load_driver_driverinfo(const game_driver *drv, char *buffer, int bufsize);
int load_driver_scoreinfo(const game_driver *drv, char *buffer, int bufsize);

#endif
