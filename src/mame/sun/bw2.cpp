// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * TODO:
 *  - copy mode
 *  - interrupts
 */
#include "emu.h"
#include "bw2.h"

#include "machine/z80scc.h"
#include "bus/rs232/rs232.h"
#include "bus/sunkbd/sunkbd.h"
#include "bus/sunmouse/sunmouse.h"

#include "screen.h"

#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

namespace {

enum ctrl_mask : u16
{
	CMBASE  = 0x007e,
	CONFIG  = 0x0f00,
	VVINT   = 0x1000,
	VINTEN  = 0x2000,
	VCOPYEN = 0x4000,
	VIDEN   = 0x8000,
};

class sun_bw2_device
	: public device_t
	, public device_multibus_interface
{
public:
	sun_bw2_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
		: device_t(mconfig, SUN_BW2, tag, owner, clock)
		, device_multibus_interface(mconfig, *this)
		, m_screen(*this, "screen")
		, m_scc(*this, "scc")
		, m_kport(*this, "keyboard")
		, m_mport(*this, "mouse")
		, m_ram(*this, "ram", 0x2'0000, ENDIANNESS_BIG)
	{
	}

	void map(address_map &map);

protected:
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	u32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, rectangle const &cliprect);

	u16 ctrl_r();
	void ctrl_w(u16 data);

private:
	required_device<screen_device> m_screen;
	required_device<scc8530_device> m_scc;
	required_device<sun_keyboard_port_device> m_kport;
	required_device<sun_mouse_port_device> m_mport;
	memory_share_creator<u16> m_ram;

	u16 m_ctrl;
};

void sun_bw2_device::device_start()
{
	m_bus->space(AS_DATA).install_device(0x70'0000, 0x7f'ffff, *this, &sun_bw2_device::map);
}

void sun_bw2_device::device_reset()
{
	m_ctrl = 0;
}

void sun_bw2_device::device_add_mconfig(machine_config &config)
{
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_raw(100_MHz_XTAL, 1600, 0, 1152, 937, 0, 900);
	m_screen->set_screen_update(FUNC(sun_bw2_device::screen_update));

	SCC8530(config, m_scc, 19.6608_MHz_XTAL / 4);
	m_scc->out_txda_callback().set(m_kport, FUNC(sun_keyboard_port_device::write_txd));
	m_scc->out_txdb_callback().set(m_mport, FUNC(sun_mouse_port_device::write_txd));
	//m_scc->out_int_callback().set_inputline(m_cpu, M68K_IRQ_6);

	SUNKBD_PORT(config, m_kport, default_sun_keyboard_devices, "type3hle");
	m_kport->rxd_handler().set(m_scc, FUNC(z80scc_device::rxa_w));

	SUNMOUSE_PORT(config, m_mport, default_sun_mouse_devices, "hle1200");
	m_mport->rxd_handler().set(m_scc, FUNC(z80scc_device::rxb_w));
}

void sun_bw2_device::map(address_map &map)
{
	map(0x0'0000, 0x1'ffff).mirror(0x6'0000).ram().share(m_ram);
	map(0x8'0000, 0x8'0007).mirror(0x7'e7f8).rw(m_scc, FUNC(scc8530_device::ab_dc_r), FUNC(scc8530_device::ab_dc_w)).umask16(0xff00);
	map(0x8'1800, 0x8'1801).mirror(0x7'e7fe).rw(FUNC(sun_bw2_device::ctrl_r), FUNC(sun_bw2_device::ctrl_w));
}

u32 sun_bw2_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, rectangle const &cliprect)
{
	if (m_ctrl & VIDEN)
	{
		rgb_t const palette[] = { rgb_t::black(), rgb_t::white() };

		u16 *ram = m_ram.target();

		for (unsigned y = screen.visible_area().min_y; y <= screen.visible_area().max_y; y++)
		{
			for (unsigned x = screen.visible_area().min_x; x <= screen.visible_area().max_x; x += 16)
			{
				u16 const data = *ram++;

				bitmap.pix(y, x + 0) = palette[BIT(data, 15)];
				bitmap.pix(y, x + 1) = palette[BIT(data, 14)];
				bitmap.pix(y, x + 2) = palette[BIT(data, 13)];
				bitmap.pix(y, x + 3) = palette[BIT(data, 12)];
				bitmap.pix(y, x + 4) = palette[BIT(data, 11)];
				bitmap.pix(y, x + 5) = palette[BIT(data, 10)];
				bitmap.pix(y, x + 6) = palette[BIT(data, 9)];
				bitmap.pix(y, x + 7) = palette[BIT(data, 8)];
				bitmap.pix(y, x + 8) = palette[BIT(data, 7)];
				bitmap.pix(y, x + 9) = palette[BIT(data, 6)];
				bitmap.pix(y, x + 10) = palette[BIT(data, 5)];
				bitmap.pix(y, x + 11) = palette[BIT(data, 4)];
				bitmap.pix(y, x + 12) = palette[BIT(data, 3)];
				bitmap.pix(y, x + 13) = palette[BIT(data, 2)];
				bitmap.pix(y, x + 14) = palette[BIT(data, 1)];
				bitmap.pix(y, x + 15) = palette[BIT(data, 0)];
			}
		}
	}
	else
		bitmap.fill(rgb_t::black());

	return 0;
}

u16 sun_bw2_device::ctrl_r()
{
	return m_ctrl;
}

void sun_bw2_device::ctrl_w(u16 data)
{
	LOG("%s: ctrl_w 0x%04x\n", machine().describe_context(), data);

	m_ctrl = data;
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(SUN_BW2, device_multibus_interface, sun_bw2_device, "sun_bw2", "Sun Microsystems Sun-2/120 video board")
