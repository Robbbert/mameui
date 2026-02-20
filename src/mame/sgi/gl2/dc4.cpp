// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * SGI IRIS DC4 Display Controller
 *
 * Sources:
 *  -
 *
 * TODO:
 *  - everything
 */

/*
 * WIP
 * --
 *  - color map is 4096x24 RAM
 *  - addressed with 12 bits, giving 4096 colors
 *  - accessed from bitplanes and Multibus
 *   - bitplanes (color lookup)
 *   - Multibus (update color map)
 */
#include "emu.h"

#include "dc4.h"

//#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

sgi_dc4_device::sgi_dc4_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: device_t(mconfig, SGI_DC4, tag, owner, clock)
	, device_multibus_interface(mconfig, *this)
	, m_screen(*this, "screen")
	, m_cmap(*this, "cmap")
	, m_bp(nullptr)
	, m_installed(false)
{
}

void sgi_dc4_device::device_start()
{
	m_bp = std::make_unique<u8[]>(0x10'0000); // 1024x1024x4 (not packed)

	save_pointer(NAME(m_bp), 0x10'0000);
}

void sgi_dc4_device::device_reset()
{
	if (!m_installed)
	{
		m_bus->space(AS_IO).install_write_handler(0x4000, 0x4001, emu::rw_delegate(*this, FUNC(sgi_dc4_device::flags_w)));
		m_bus->space(AS_IO).install_write_handler(0x4200, 0x43ff, emu::rw_delegate(*this, FUNC(sgi_dc4_device::colormap_w<0>)));
		m_bus->space(AS_IO).install_write_handler(0x4400, 0x45ff, emu::rw_delegate(*this, FUNC(sgi_dc4_device::colormap_w<1>)));
		m_bus->space(AS_IO).install_write_handler(0x4600, 0x47ff, emu::rw_delegate(*this, FUNC(sgi_dc4_device::colormap_w<2>)));

		m_installed = true;
	}
}

u32 sgi_dc4_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, rectangle const &cliprect)
{
	u8 const *src = m_bp.get();

	for (unsigned y = screen.visible_area().min_y; y <= screen.visible_area().max_y; y++)
	{
		for (unsigned x = screen.visible_area().min_x; x <= screen.visible_area().max_x; x++)
		{
			bitmap.pix(screen.visible_area().max_y - y, x) = m_cmap->pen_color(*src++);
		}
	}

	return 0;
}

void sgi_dc4_device::device_add_mconfig(machine_config &config)
{
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_raw(1024 * 768 * 60, 1024, 0, 1024, 768, 0, 768);
	m_screen->set_screen_update(FUNC(sgi_dc4_device::screen_update));

	// MCM6168P45 x8x3 (4096x4 SRAM) 16K x3
	PALETTE(config, m_cmap);
	m_cmap->set_entries(4096);
}

void sgi_dc4_device::flags_w(u16 data) { LOG("%s: flags_w 0x%04x\n", machine().describe_context(), data); m_flags = data; }

template <unsigned Channel> void sgi_dc4_device::colormap_w(offs_t offset, u16 data, u16 mem_mask)
{
	unsigned const index = BIT(m_flags, 0, 4) << 8 | offset;

	switch (Channel)
	{
	case 0: m_cmap->set_pen_red_level(index, data); break;
	case 1: m_cmap->set_pen_green_level(index, data); break;
	case 2: m_cmap->set_pen_blue_level(index, data); break;
	}
}

void sgi_dc4_device::bp_w(offs_t offset, u8 data, u8 mask)
{
	m_bp[offset] = (m_bp[offset] & ~mask) | (data & mask);
}

DEFINE_DEVICE_TYPE(SGI_DC4, sgi_dc4_device, "sgi_dc4", "Silicon Graphics DC4 Display Controller")
