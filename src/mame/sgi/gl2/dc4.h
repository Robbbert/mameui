// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

#ifndef MAME_SGI_DC4_H
#define MAME_SGI_DC4_H

#pragma once

#include "bus/multibus/multibus.h"

#include "emupal.h"
#include "screen.h"

class sgi_dc4_device
	: public device_t
	, public device_multibus_interface
{
public:
	sgi_dc4_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock);

	void bp_w(offs_t offset, u8 data, u8 mask);

	static constexpr feature_type imperfect_features() { return feature::GRAPHICS; }

protected:
	//virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	//virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	u32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, rectangle const &cliprect);

	void flags_w(u16 data);

	template <unsigned Channel> void colormap_w(offs_t offset, u16 data, u16 mem_mask);

private:
	required_device<screen_device> m_screen;
	required_device<palette_device> m_cmap; 

	std::unique_ptr<u8[]> m_bp;

	u16 m_flags;

	bool m_installed;
};

DECLARE_DEVICE_TYPE(SGI_DC4, sgi_dc4_device)

#endif // MAME_SGI_DC4_H
