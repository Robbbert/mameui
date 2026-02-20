// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * SGI IRIS UC4 Update Controller
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
 *  - font ram MB81416-12 x4 (16384x4 RAM)
 *  - counter low 8 bits -> rectadr
 *  - counter hi 8 bits -> patadr
 */
#include "emu.h"

#include "uc4.h"

#include "emupal.h"
#include "screen.h"

//#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

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
sgi_uc4_device::sgi_uc4_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: device_t(mconfig, SGI_UC4, tag, owner, clock)
	, device_multibus_interface(mconfig, *this)
	, m_font(*this, "font", 0x8000, ENDIANNESS_BIG)
	, m_dc(*this, finder_base::DUMMY_TAG)
	, m_installed(false)
{
}

void sgi_uc4_device::device_start()
{
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
}

void sgi_uc4_device::device_reset()
{
	if (!m_installed)
	{
		m_bus->space(AS_IO).install_readwrite_handler(0x3080, 0x30a5,
			emu::rw_delegate(*this, FUNC(sgi_uc4_device::buf_r)),
			emu::rw_delegate(*this, FUNC(sgi_uc4_device::buf_w)));

		m_bus->space(AS_IO).install_readwrite_handler(0x3180, 0x3181,
			emu::rw_delegate(*this, FUNC(sgi_uc4_device::ucr_r)),
			emu::rw_delegate(*this, FUNC(sgi_uc4_device::ucr_w)));

		m_bus->space(AS_IO).install_readwrite_handler(0x3200, 0x323f,
			emu::rw_delegate(*this, FUNC(sgi_uc4_device::cmd_r)),
			emu::rw_delegate(*this, FUNC(sgi_uc4_device::cmd_w)));

		m_installed = true;
	}

	m_fma = 0;
	m_md = 0;
	m_rp = 0;
	m_cf = 0;

	m_ucr = 0;
}

static const gfx_layout uc4_layout =
{
	8, 16, 1024, 1,
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 15 * 16, 14 * 16, 13 * 16, 12 * 16, 11 * 16, 10 * 16, 9 * 16, 8 * 16, 7 * 16, 6 * 16, 5 * 16, 4 * 16, 3 * 16, 2 * 16, 1 * 16, 0 * 16 },
	16 * 16
};

static GFXDECODE_START(uc4_gfx)
	GFXDECODE_RAM("font", 0x0, uc4_layout, 0, 1)
GFXDECODE_END

void sgi_uc4_device::device_add_mconfig(machine_config &config)
{
	// gfxdecode is only to show the font data in the tile viewer
	PALETTE(config, "palette", palette_device::MONOCHROME);
	GFXDECODE(config, "gfx", "palette", uc4_gfx);
}

u16 sgi_uc4_device::buf_r(offs_t offset)
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

void sgi_uc4_device::buf_w(offs_t offset, u16 data, u16 mem_mask)
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

u16 sgi_uc4_device::ucr_r()
{
	return m_ucr;
}

void sgi_uc4_device::ucr_w(u16 data)
{
	LOG("%s: ucr_w 0x%04x\n", machine().describe_context(), data);

	m_ucr = (m_ucr & ~UCR_WM) | (data & UCR_WM);
}


u16 sgi_uc4_device::cmd_r(offs_t offset) { return 0; }

void sgi_uc4_device::cmd_w(offs_t offset, u16 data, u16 mem_mask)
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
				m_dc->bp_w((m_ys + y) * 0x400 + m_xs + x, BIT(data, (x & 7) ^ 7) ? m_color : 0, m_we);
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

DEFINE_DEVICE_TYPE(SGI_UC4, sgi_uc4_device, "sgi_uc4", "Silicon Graphics UC4 Update Controller")
