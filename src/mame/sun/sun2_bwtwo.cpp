// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * Sun-2 Monochrome Frame Buffer.
 *
 *  501-1003  TTL Monochrome Frame Buffer      Sun-2/120/170
 *  501-1052  TTL/ECL Monochrome Frame Buffer  Sun-2/120/170
 *
 * Sources:
 *  - Engineering Manual for the Sun-2/120 Video Board, Revision 50 of 28 September 1984, Sun Microystems, Inc.
 *
 * TODO:
 *  - test copy mode
 */

#include "emu.h"
#include "sun2_bwtwo.h"

#include "bus/sunkbd/sunkbd.h"
#include "bus/sunmouse/sunmouse.h"
#include "machine/z80scc.h"

#include "screen.h"

#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

namespace {

enum ctrl_mask : u16
{
	CTRL_CMBASE  = 0x007e, // copy mode base address
	CTRL_CONFIG  = 0x0f00, // configuration jumpers
	CTRL_VVINT   = 0x1000, // vertical interrupt status
	CTRL_VINTEN  = 0x2000, // vertical interrupt enable
	CTRL_VCOPYEN = 0x4000, // copy enable
	CTRL_VIDEN   = 0x8000, // video enable
};

class sun2_bwtwo_device
	: public device_t
	, public device_multibus_interface
{
public:
	sun2_bwtwo_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
		: device_t(mconfig, SUN2_BWTWO, tag, owner, clock)
		, device_multibus_interface(mconfig, *this)
		, m_screen(*this, "screen")
		, m_scc(*this, "scc")
		, m_kport(*this, "keyboard")
		, m_mport(*this, "mouse")
		, m_j1600(*this, "J1600")
		, m_j1903(*this, "J1903")
		, m_j1904(*this, "J1904")
		, m_ram(*this, "ram", 0x2'0000, ENDIANNESS_BIG)
		, m_ctrl(0)
	{
	}

	void map(address_map &map);

protected:
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
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
	required_ioport m_j1600;
	required_ioport m_j1903;
	required_ioport m_j1904;
	memory_share_creator<u16> m_ram;

	u16 m_ctrl;

	std::optional<unsigned> m_vint;
	std::optional<unsigned> m_sint;
	memory_passthrough_handler m_mph;
};

void sun2_bwtwo_device::device_add_mconfig(machine_config &config)
{
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_raw(100_MHz_XTAL, 1600, 0, 1152, 937, 0, 900);

	m_screen->set_screen_update(FUNC(sun2_bwtwo_device::screen_update));
	m_screen->screen_vblank().set(
		[this](int state)
		{
			if (state)
			{
				bool const active = m_ctrl & CTRL_VVINT;

				m_ctrl |= CTRL_VVINT;

				if (!active && (m_ctrl & CTRL_VINTEN) && m_vint)
					int_w(*m_vint, 0);
			}
		});

	SCC8530(config, m_scc, 19.6608_MHz_XTAL / 4);
	m_scc->out_txda_callback().set(m_kport, FUNC(sun_keyboard_port_device::write_txd));
	m_scc->out_txdb_callback().set(m_mport, FUNC(sun_mouse_port_device::write_txd));
	m_scc->out_int_callback().set(
		[this](int state)
		{
			if (m_sint)
				int_w(*m_sint, state ? 0 : 1);
		});

	SUNKBD_PORT(config, m_kport, default_sun_keyboard_devices, "type3hle");
	m_kport->rxd_handler().set(m_scc, FUNC(z80scc_device::rxa_w));

	SUNMOUSE_PORT(config, m_mport, default_sun_mouse_devices, "hle1200");
	m_mport->rxd_handler().set(m_scc, FUNC(z80scc_device::rxb_w));
}

void sun2_bwtwo_device::device_start()
{
	save_item(NAME(m_ctrl));

	if (m_bus->p2())
		m_bus->p2()->install_device(0x70'0000, 0x7f'ffff, *this, &sun2_bwtwo_device::map);
	else
		logerror("Multibus P2 space not available\n");
}

void sun2_bwtwo_device::device_reset()
{
	if (m_j1600->read() & 0x0100)
		m_screen->configure(1600, 1061, rectangle(0, 1023, 0, 1023), attotime::from_ticks(1600 * 1061, 100_MHz_XTAL).as_attoseconds());

	switch (m_j1903->read())
	{
	case 0x01: m_sint = 0; break;
	case 0x02: m_sint = 1; break;
	case 0x04: m_sint = 2; break;
	case 0x08: m_sint = 3; break;
	case 0x10: m_sint = 4; break;
	case 0x20: m_sint = 5; break;
	case 0x40: m_sint = 6; break;
	case 0x80: m_sint = 7; break;
	default: m_sint = std::nullopt; break;
	}

	switch (m_j1904->read())
	{
	case 0x01: m_vint = 0; break;
	case 0x02: m_vint = 1; break;
	case 0x04: m_vint = 2; break;
	case 0x08: m_vint = 3; break;
	case 0x10: m_vint = 4; break;
	case 0x20: m_vint = 5; break;
	case 0x40: m_vint = 6; break;
	case 0x80: m_vint = 7; break;
	default: m_vint = std::nullopt; break;
	}

	m_ctrl = 0;
	m_mph.remove();
}

void sun2_bwtwo_device::map(address_map &map)
{
	map(0x0'0000, 0x1'ffff).mirror(0x6'0000).ram().share(m_ram);
	map(0x8'0000, 0x8'0007).mirror(0x7'e7f8).rw(m_scc, FUNC(scc8530_device::ab_dc_r), FUNC(scc8530_device::ab_dc_w)).umask16(0xff00);
	map(0x8'1800, 0x8'1801).mirror(0x7'e7fe).rw(FUNC(sun2_bwtwo_device::ctrl_r), FUNC(sun2_bwtwo_device::ctrl_w));
}

u32 sun2_bwtwo_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, rectangle const &cliprect)
{
	static constexpr rgb_t palette[] = { rgb_t::black(), rgb_t::white() };

	if (m_ctrl & CTRL_VIDEN)
	{
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
		bitmap.fill(palette[0]);

	return 0;
}

u16 sun2_bwtwo_device::ctrl_r()
{
	return m_ctrl | m_j1600->read();
}

void sun2_bwtwo_device::ctrl_w(u16 data)
{
	LOG("%s: ctrl_w 0x%04x\n", machine().describe_context(), data);

	if ((m_ctrl ^ data) & (CTRL_VCOPYEN | CTRL_CMBASE))
	{
		m_mph.remove();

		if ((data & CTRL_VCOPYEN) && m_bus->p2())
		{
			offs_t const cmbase = BIT(data, 1, 6) << 17;
			logerror("copy mode 0x%08x\n", cmbase);

			m_bus->p2()->install_write_tap(cmbase, cmbase + m_ram.bytes() - 1, "copy_w",
				[this](offs_t offset, u16 &data, u16 mem_mask)
				{
					COMBINE_DATA(&m_ram[BIT(offset, 1, 16)]);
				}, &m_mph);
		}
	}

	if (m_vint && (m_ctrl & CTRL_VINTEN) && (m_ctrl & CTRL_VVINT) && (!(data & CTRL_VVINT) || !(data & CTRL_VINTEN)))
		int_w(*m_vint, 1);

	m_ctrl = data;
}

INPUT_PORTS_START(sun2_bwtwo)
	PORT_START("J1600")
	PORT_DIPNAME(0x0100, 0x0000, "Video Resolution") PORT_DIPLOCATION("J1600:!1")
	PORT_DIPSETTING(0x0100, "1024x1024")
	PORT_DIPSETTING(0x0000, "1152x900")
	PORT_DIPNAME(0x0200, 0x0000, "S1") PORT_DIPLOCATION("J1600:!2")
	PORT_DIPSETTING(0x0200, DEF_STR(Off))
	PORT_DIPSETTING(0x0000, DEF_STR(On))
	PORT_DIPNAME(0x0400, 0x0000, "S2") PORT_DIPLOCATION("J1600:!3")
	PORT_DIPSETTING(0x0400, DEF_STR(Off))
	PORT_DIPSETTING(0x0000, DEF_STR(On))
	PORT_DIPNAME(0x0800, 0x0000, "S3") PORT_DIPLOCATION("J1600:!4")
	PORT_DIPSETTING(0x0800, DEF_STR(Off))
	PORT_DIPSETTING(0x0000, DEF_STR(On))

	PORT_START("J1903")
	PORT_DIPNAME(0xff, 0x40, "Serial Interrupt")
	PORT_DIPSETTING(0x00, "None")
	PORT_DIPSETTING(0x01, "0")
	PORT_DIPSETTING(0x02, "1")
	PORT_DIPSETTING(0x04, "2")
	PORT_DIPSETTING(0x08, "3")
	PORT_DIPSETTING(0x10, "4")
	PORT_DIPSETTING(0x20, "5")
	PORT_DIPSETTING(0x40, "6")
	PORT_DIPSETTING(0x80, "7")

	PORT_START("J1904")
	PORT_DIPNAME(0xff, 0x10, "Video Interrupt")
	PORT_DIPSETTING(0x00, "None")
	PORT_DIPSETTING(0x01, "0")
	PORT_DIPSETTING(0x02, "1")
	PORT_DIPSETTING(0x04, "2")
	PORT_DIPSETTING(0x08, "3")
	PORT_DIPSETTING(0x10, "4")
	PORT_DIPSETTING(0x20, "5")
	PORT_DIPSETTING(0x40, "6")
	PORT_DIPSETTING(0x80, "7")
INPUT_PORTS_END

ioport_constructor sun2_bwtwo_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(sun2_bwtwo);
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(SUN2_BWTWO, device_multibus_interface, sun2_bwtwo_device, "sun2_bwtwo", "Sun-2 Monochrome Frame Buffer")
