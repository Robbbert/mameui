// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * SGI IRIS GL2 high-level emulation
 *
 * This device is an interim high-level emulation of a combination of GF2, UC4,
 * DC4 and BP3 boards. Currently, it supports the limited subset of functions
 * used by the IRIS 3130 firmware to display the monitor. When the hardware is
 * better understood and more software becomes usable, this device should be
 * replaced by individual Multibus devices emulating each of the four cards
 * mentioned earlier.
 *
 * Sources:
 *  - IRIS 3.7 source code
 *
 * TODO:
 *  - everything
 */

#include "emu.h"

#include "gl2.h"

#include "emupal.h"
#include "screen.h"

#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

namespace {

enum cfr_mask : u16
{
	CFR_UPENABLEB  = 0x0001,
	CFR_UPENABLEA  = 0x0002,
	CFR_DSPENABLEB = 0x0004,
	CFR_DSPENABLEA = 0x0008,
	CFR_LDLINESTIP = 0x0010,
	CFR_FINISHLINE = 0x0020,
	CFR_INVERT     = 0x0040,
	CFR_SCREENMASK = 0x0080,
	CFR_PFICD      = 0x0100,
	CFR_PFICOLUMN  = 0x0200,
	CFR_PFIYDOWN   = 0x0400,
	CFR_PATTERN32  = 0x0800,
	CFR_PATTERN64  = 0x1000,
	CFR_ALLPATTERN = 0x2000,
	CFR_PFIXDOWN   = 0x4000,
	CFR_PFIREAD    = 0x8000,
};
enum ucr_mask : u16
{
	UCR_BOARDDIS = 0x0100,
	UCR_MBENAB   = 0x0200,
	UCR_INTRENAB = 0x0400,
	UCR_DMAENAB  = 0x0800,
	UCR_ZERO     = 0x1000, // read only
	UCR_VERTICAL = 0x2000, // read only
	UCR_VERTINTR = 0x4000, // read only
	UCR_BUSY     = 0x8000, // read only

	UCR_WM       = 0x0f00,
};

class sgi_gl2_device
	: public device_t
	, public device_multibus_interface
{
public:
	sgi_gl2_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
		: device_t(mconfig, SGI_GL2, tag, owner, clock)
		, device_multibus_interface(mconfig, *this)
		, m_screen(*this, "screen")
		, m_cmap(*this, "cmap")
		, m_font(*this, "font", 0x8000, ENDIANNESS_BIG)
		, m_bp(nullptr)
		, m_installed(false)
	{
	}

	static constexpr feature_type imperfect_features() { return feature::GRAPHICS; }

protected:
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	// GF2
	u16 fbc_readdpx_r();
	void fbc_clrint_w(u16 data);
	u16 fbc_flags_r();
	void fbc_flags_w(u16 data);
	u16 fbc_data_r();
	void fbc_data_w(u16 data);
	void ge_flags_w(u16 data);

	// DC4
	void flags_w(u16 data);
	template <unsigned Channel> void colormap_w(offs_t offset, u16 data, u16 mem_mask);
	u32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, rectangle const &cliprect);

	// UC4
	u16 buf_r(offs_t offset);
	void buf_w(offs_t offset, u16 data, u16 mem_mask);
	u16 ucr_r();
	void ucr_w(u16 data);
	u16 cmd_r(offs_t offset);
	void cmd_w(offs_t offset, u16 data, u16 mem_mask);

	// BP4
	void bp_w(offs_t offset, u8 data, u8 mask);

private:
	required_device<screen_device> m_screen;
	required_device<palette_device> m_cmap;
	memory_share_creator<u16> m_font;

	std::unique_ptr<u8[]> m_bp;

	u16 m_ed; // error delta
	u16 m_ec; // error correct
	u16 m_xs; // x start
	u16 m_xe; // x end
	u16 m_ys; // y start
	u16 m_ye; // y end
	u16 m_fma; // font memory address
	u16 m_da[4]; // dda address
	u16 m_dd[4]; // dda delta
	u8 m_md; // mode
	u8 m_rp; // repeat
	u16 m_cf; // config

	u16 m_ucr;

	u16 m_color;
	u16 m_we;

	u16 m_flags;

	util::fifo<u16, 16> m_params;
	u8 m_expect;

	bool m_installed;
};

void sgi_gl2_device::device_start()
{
	m_bp = std::make_unique<u8[]>(0x10'0000); // 1024x1024x4 (not packed)

	save_item(NAME(m_ed));
	save_item(NAME(m_ec));
	save_item(NAME(m_xs));
	save_item(NAME(m_xe));
	save_item(NAME(m_ys));
	save_item(NAME(m_ye));
	save_item(NAME(m_fma));
	save_item(NAME(m_da));
	save_item(NAME(m_dd));
	save_item(NAME(m_md));
	save_item(NAME(m_rp));
	save_item(NAME(m_cf));

	save_item(NAME(m_ucr));
	save_item(NAME(m_color));
	save_item(NAME(m_we));

	save_pointer(NAME(m_bp), 0x10'0000);
}

void sgi_gl2_device::device_reset()
{
	if (!m_installed)
	{
		// GF2 handlers
		m_bus->space(AS_IO).install_readwrite_handler(0x2000, 0x2001,
			emu::rw_delegate(*this, FUNC(sgi_gl2_device::fbc_readdpx_r)),
			emu::rw_delegate(*this, FUNC(sgi_gl2_device::fbc_clrint_w)));

		m_bus->space(AS_IO).install_readwrite_handler(0x2400, 0x2401,
			emu::rw_delegate(*this, FUNC(sgi_gl2_device::fbc_flags_r)),
			emu::rw_delegate(*this, FUNC(sgi_gl2_device::fbc_flags_w)));

		m_bus->space(AS_IO).install_readwrite_handler(0x2800, 0x2801,
			emu::rw_delegate(*this, FUNC(sgi_gl2_device::fbc_data_r)),
			emu::rw_delegate(*this, FUNC(sgi_gl2_device::fbc_data_w)));

		m_bus->space(AS_IO).install_write_handler(0x2c00, 0x2c01,
			emu::rw_delegate(*this, FUNC(sgi_gl2_device::ge_flags_w)));

		// DC4 handlers
		m_bus->space(AS_IO).install_write_handler(0x4000, 0x4001,
			 emu::rw_delegate(*this, FUNC(sgi_gl2_device::flags_w)));
		m_bus->space(AS_IO).install_write_handler(0x4200, 0x43ff,
			 emu::rw_delegate(*this, FUNC(sgi_gl2_device::colormap_w<0>)));
		m_bus->space(AS_IO).install_write_handler(0x4400, 0x45ff,
			 emu::rw_delegate(*this, FUNC(sgi_gl2_device::colormap_w<1>)));
		m_bus->space(AS_IO).install_write_handler(0x4600, 0x47ff,
			 emu::rw_delegate(*this, FUNC(sgi_gl2_device::colormap_w<2>)));

		// UC4 handlers
		m_bus->space(AS_IO).install_readwrite_handler(0x3080, 0x30a5,
			emu::rw_delegate(*this, FUNC(sgi_gl2_device::buf_r)),
			emu::rw_delegate(*this, FUNC(sgi_gl2_device::buf_w)));

		m_bus->space(AS_IO).install_readwrite_handler(0x3180, 0x3181,
			emu::rw_delegate(*this, FUNC(sgi_gl2_device::ucr_r)),
			emu::rw_delegate(*this, FUNC(sgi_gl2_device::ucr_w)));

		m_bus->space(AS_IO).install_readwrite_handler(0x3200, 0x323f,
			emu::rw_delegate(*this, FUNC(sgi_gl2_device::cmd_r)),
			emu::rw_delegate(*this, FUNC(sgi_gl2_device::cmd_w)));

		m_installed = true;
	}

	m_params.clear();
	m_expect = 0;
}

static const gfx_layout gl2_layout =
{
	8, 16, 1024, 1,
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 15 * 16, 14 * 16, 13 * 16, 12 * 16, 11 * 16, 10 * 16, 9 * 16, 8 * 16, 7 * 16, 6 * 16, 5 * 16, 4 * 16, 3 * 16, 2 * 16, 1 * 16, 0 * 16 },
	16 * 16
};

static GFXDECODE_START(gl2_gfx)
	GFXDECODE_RAM("font", 0x0, gl2_layout, 0, 1)
GFXDECODE_END

void sgi_gl2_device::device_add_mconfig(machine_config &config)
{
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_raw(1024 * 768 * 60, 1024, 0, 1024, 768, 0, 768);
	m_screen->set_screen_update(FUNC(sgi_gl2_device::screen_update));

	// MCM6168P45 x8x3 (4096x4 SRAM) 16K x3
	PALETTE(config, m_cmap);
	m_cmap->set_entries(4096);

	// gfxdecode is only to show the font data in the tile viewer
	GFXDECODE(config, "gfx", m_cmap, gl2_gfx);
}

u32 sgi_gl2_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, rectangle const &cliprect)
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

u16 sgi_gl2_device::fbc_readdpx_r()
{
	return 0;
}

void sgi_gl2_device::fbc_clrint_w(u16 data)
{
	LOG("%s: fbc_clrint_w 0x%04x\n", machine().describe_context(), data);
}

u16 sgi_gl2_device::fbc_flags_r()
{
	return 0x0002;
}

void sgi_gl2_device::fbc_flags_w(u16 data)
{
	//LOG("%s: fbc_flags_w 0x%04x\n", machine().describe_context(), data);
}

u16 sgi_gl2_device::fbc_data_r()
{
	return 0x40; // HACK: always ready
}

void sgi_gl2_device::fbc_data_w(u16 data)
{
	LOG("%s: fbc_data_w 0x%04x expect %u\n", machine().describe_context(), data, m_expect);
}

void sgi_gl2_device::ge_flags_w(u16 data)
{
	LOG("%s: ge_flags_w 0x%04x\n", machine().describe_context(), data);
}

u16 sgi_gl2_device::buf_r(offs_t offset)
{
	switch (offset)
	{
	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
		return m_da[offset & 0x3];
	}

	return 0;
}

void sgi_gl2_device::buf_w(offs_t offset, u16 data, u16 mem_mask)
{
	static char const *const buf[] = {
		"", "LDED", "LDEC", "LDXS",
		"LDXE", "LDYS", "LDYE", "LDFMADDR",
		"LDDDASAF", "LDDDASAI", "LDDDAEAF", "LDDDAEAI",
		"LDDDASDF", "LDDDASDI", "LDDDAEDF", "LDDDAEDI",
		"LDMODE", "LDREPEAT", "LDCONFIG",
	};

	switch (offset)
	{
	case 0x01: m_ed = data & 0xfffU; break;
	case 0x02: m_ec = data & 0xfffU; break;
	case 0x03: m_xs = data & 0xfffU; break;
	case 0x04: m_xe = data & 0xfffU; break;
	case 0x05: m_ys = data & 0xfffU; break;
	case 0x06: m_ye = data & 0xfffU; break;
	case 0x07: m_fma = data; break;
	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
		m_da[offset & 3] = data & 0xfffU;
		break;
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
		m_dd[offset & 3] = data & 0xfffU;
		break;
	case 0x10: m_md = data & 0xfU; break;
	case 0x11: m_rp = data; break;
	case 0x12: m_cf = data; break;

	default:
		LOG("%s: buf_w 0x%02x %s(0x%04x)\n", machine().describe_context(), offset, buf[offset], data);
		break;
	}
}

u16 sgi_gl2_device::ucr_r()
{
	return m_ucr;
}

void sgi_gl2_device::ucr_w(u16 data)
{
	LOG("%s: ucr_w 0x%04x\n", machine().describe_context(), data);

	m_ucr = (m_ucr & ~UCR_WM) | (data & UCR_WM);
}


u16 sgi_gl2_device::cmd_r(offs_t offset) { return 0; }

void sgi_gl2_device::cmd_w(offs_t offset, u16 data, u16 mem_mask)
{
	static char const *const cmd[] = {
		"UC_READFONT", "UC_WRITEFONT", "UC_READREPEAT", "UC_SETADDRS",
		"UC_SAVEWORD", "UC_DRAWWORD", "UC_READLSTIP", "UC_NOOP",
		"", "UC_DRAWCHAR", "UC_FILLRECT", "UC_FILLTRAP",
		"UC_DRAWLINE1", "UC_DRAWLINE2", "UC_DRAWLINE4", "UC_DRAWLINE5",
		"UC_SETSCRMASKX", "UC_SETSCRMASKY", "", "",
		"UC_SETCOLORCD", "UC_SETCOLORAB", "UC_SETWECD", "UC_SETWEAB",
		"UC_READPIXELCD", "UC_READPIXELAB", "UC_DRAWPIXELCD", "UC_DRAWPIXELAB",
		"UC_DRAWLINE11", "UC_DRAWLINE10", "UC_DRAWLINE8", "UC_DRAWLINE7",
	};

	//LOG("%s: cmd_w 0x%02x %s(0x%04x)\n", machine().describe_context(), offset, cmd[offset], data);

	switch (offset)
	{
	case 0x01: // UC_WRITEFONT
		m_font[m_fma++ & 0x3fff] = data;
		break;
	case 0x09: // UC_DRAWCHAR
	case 0x0a: // UC_FILLRECT
		for (int y = 0; y < m_ye - m_ys; y++)
		{
			u8 const data = m_font[(m_fma + (y & 0xfU)) & 0x3fff] >> 8;
			for (int x = 0; x < m_xe - m_xs; x++)
			{
				bp_w((m_ys + y) * 0x400 + m_xs + x, BIT(data, (x & 7) ^ 7) ? m_color : 0, m_we);
			}
		}
		break;
	case 0x15: m_color = data; break;
	case 0x17: m_we = data; break;
	default:
		LOG("%s: cmd_w 0x%02x %s(0x%04x)\n", machine().describe_context(), offset, cmd[offset], data);
		break;
	}
}

void sgi_gl2_device::flags_w(u16 data)
{
	LOG("%s: flags_w 0x%04x\n", machine().describe_context(), data);

	m_flags = data;
}

template <unsigned Channel> void sgi_gl2_device::colormap_w(offs_t offset, u16 data, u16 mem_mask)
{
	unsigned const index = BIT(m_flags, 0, 4) << 8 | offset;

	switch (Channel)
	{
	case 0: m_cmap->set_pen_red_level(index, data); break;
	case 1: m_cmap->set_pen_green_level(index, data); break;
	case 2: m_cmap->set_pen_blue_level(index, data); break;
	}
}

void sgi_gl2_device::bp_w(offs_t offset, u8 data, u8 mask)
{
	m_bp[offset] = (m_bp[offset] & ~mask) | (data & mask);
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(SGI_GL2, device_multibus_interface, sgi_gl2_device, "sgi_gl2", "Silicon Graphics GL2")
