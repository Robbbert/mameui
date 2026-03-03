// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

#ifndef MAME_SUN_SC_H
#define MAME_SUN_SC_H

#pragma once

#include "machine/nscsi_bus.h"

class sun_sc_device
	: public nscsi_device
	, public nscsi_slot_card_interface
{
public:
	sun_sc_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void map(address_map &map);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	u16 data_r();
	void data_w(u16 data);
	u8 status_r();
	void command_w(u8 data);
	u16 ctrl_r();
	void ctrl_w(u16 data);

private:
	u8 m_status;
	u8 m_command;
	u16 m_ctrl;
};

DECLARE_DEVICE_TYPE(SUN_SC, sun_sc_device)

#endif // MAME_SUN_SC_H
