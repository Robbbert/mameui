// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * Sun-2 Ethernet (part 501-1004).
 *
 * Sources:
 *  - Sun-2 Ethernet Interface Spec, W. M. Bradley
 *
 * TODO:
 *  - P2 memory expansion
 *  - ID prom (unused)
 *  - parity
 */

#include "emu.h"
#include "sun2_enet.h"

#include "machine/i82586.h"

#define LOG_BUS  (1U << 1)
#define LOG_NET  (1U << 2)

//#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

namespace {

enum status_mask : u16
{
	ST_BASE     = 0x000f, // memory base address
	ST_BIG      = 0x0010, // memory size (0=256K, 1=1M)
	ST_EXP      = 0x0020, // expansion enabled
	ST_EINT     = 0x0100, // EDLC interrupt
	ST_PARERR   = 0x0200, // parity error
	ST_PARINTEN = 0x0800, // parity error interrupt enable
	ST_INTEN    = 0x1000, // EDLC interrupt enable
	ST_CA       = 0x2000, // channel attention
	ST_LOOPB    = 0x4000, // loopback (0=enable, 1=disable)
	ST_RST      = 0x8000, // reset

	ST_WMASK    = 0xfc00,
};
enum page_mask : u16
{
	PM_PN  = 0x0fff, // page number
	PM_P2  = 0x2000, // memory select (0=on-board, 1=P2)
	PM_SWP = 0x8000, // swap control (0=little endian, 1=big endian)
};

class sun2_enet_device
	: public device_t
	, public device_multibus_interface
{
public:
	sun2_enet_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
		: device_t(mconfig, SUN2_ENET, tag, owner, clock)
		, device_multibus_interface(mconfig, *this)
		, m_net(*this, "net")
		, m_ram(*this, "ram", 0x4'0000, ENDIANNESS_LITTLE)
		, m_j500(*this, "J500")
		, m_u503(*this, "U503")
		, m_u505(*this, "U505")
		, m_u506(*this, "U506")
		, m_installed(false)
	{
	}

protected:
	//virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	void reg_map(address_map &map);
	void net_map(address_map &map);

	// host handlers
	u16 status_r();
	void status_w(offs_t offset, u16 data, u16 mem_mask);
	u16 page_r(offs_t offset);
	void page_w(offs_t offset, u16 data, u16 mem_mask);
	u16 bus_r(offs_t offset, u16 mem_mask);
	void bus_w(offs_t offset, u16 data, u16 mem_mask);

	// card handlers
	u16 net_r(offs_t offset, u16 mem_mask);
	void net_w(offs_t offset, u16 data, u16 mem_mask);

private:
	required_device<i82586_device> m_net;
	memory_share_creator<u16> m_ram;

	required_ioport m_j500;
	required_ioport m_u503;
	required_ioport m_u505;
	required_ioport m_u506;

	u16 m_status;
	std::unique_ptr<u16[]> m_page;
	std::optional<unsigned> m_int;

	bool m_installed;
};

void sun2_enet_device::device_add_mconfig(machine_config &config)
{
	I82586(config, m_net, 16_MHz_XTAL / 2);
	m_net->set_addrmap(AS_PROGRAM, &sun2_enet_device::net_map);
	m_net->out_irq_cb().set(
		[this](int state)
		{
			if (state)
				m_status |= ST_EINT;
			else
				m_status &= ~ST_EINT;

			if ((m_status & ST_INTEN) && m_int)
				int_w(*m_int, !state);
		});
}

void sun2_enet_device::device_start()
{
	m_page = std::make_unique<u16[]>(1024);

	save_item(NAME(m_status));
	save_pointer(NAME(m_page), 1024);
}

void sun2_enet_device::device_reset()
{
	if (!m_installed)
	{
		switch (m_j500->read())
		{
		case 0x01: m_int = 0; break;
		case 0x02: m_int = 1; break;
		case 0x04: m_int = 2; break;
		case 0x08: m_int = 3; break;
		case 0x10: m_int = 4; break;
		case 0x20: m_int = 5; break;
		case 0x40: m_int = 6; break;
		case 0x80: m_int = 7; break;
		default: m_int = std::nullopt; break;
		}

		offs_t const mem_base = m_u505->read() << 16;
		offs_t const reg_base = m_u503->read() << 12;
		offs_t window = 0;
		switch (m_u506->read())
		{
		case 0x66: window = 0x4'0000; break;
		case 0x96: window = 0x2'0000; break;
		case 0x99: window = 0x1'0000; break;
		}

		m_bus->space(AS_PROGRAM).install_readwrite_handler(mem_base, mem_base + window - 1,
			emu::rw_delegate(*this, FUNC(sun2_enet_device::bus_r)),
			emu::rw_delegate(*this, FUNC(sun2_enet_device::bus_w)));

		m_bus->space(AS_PROGRAM).install_device(reg_base, reg_base | 0x0fffU, *this, &sun2_enet_device::reg_map);

		LOG("control 0x%06x memory 0x%06x\n", reg_base, mem_base);
		m_installed = true;
	}

	m_status = m_u505->read();
	m_net->set_loopback(false);
}

void sun2_enet_device::reg_map(address_map &map)
{
	map(0x000, 0x7ff).rw(FUNC(sun2_enet_device::page_r), FUNC(sun2_enet_device::page_w));
	map(0x800, 0x83f).noprw(); // id prom
	map(0x840, 0x841).rw(FUNC(sun2_enet_device::status_r), FUNC(sun2_enet_device::status_w));
	map(0x842, 0x843).noprw();
	map(0x844, 0x845).noprw(); // parity error address high/acknowledge
	map(0x846, 0x847).noprw(); // parity error address low
}

void sun2_enet_device::net_map(address_map &map)
{
	map.global_mask(0xf'ffff);

	map(0x0'0000, 0xf'ffff).rw(FUNC(sun2_enet_device::net_r), FUNC(sun2_enet_device::net_w));
}

u16 sun2_enet_device::status_r()
{
	return m_status;
}

void sun2_enet_device::status_w(offs_t offset, u16 data, u16 mem_mask)
{
	LOG("%s: status_w 0x%04x mask 0x%04x\n", machine().describe_context(), data & mem_mask, mem_mask);

	if (!(data & ST_RST))
	{
		if ((data ^ m_status) & ST_INTEN)
		{
			if ((m_status & ST_EINT) && m_int)
				int_w(*m_int, !(data & ST_INTEN));
		}

		if ((data ^ m_status) & ST_LOOPB)
			m_net->set_loopback(!(data & ST_LOOPB));

		if ((data ^ m_status) & ST_CA)
			m_net->ca(data & ST_CA);

		m_status = (m_status & ~(mem_mask & ST_WMASK)) | (data & mem_mask & ST_WMASK);
	}
	else
		reset();
}

u16 sun2_enet_device::page_r(offs_t offset)
{
	return m_page[offset];
}

void sun2_enet_device::page_w(offs_t offset, u16 data, u16 mem_mask)
{
	LOG("%s: page_w[0x%03x] 0x%04x mask 0x%04x\n", machine().describe_context(), offset, data & mem_mask, mem_mask);

	COMBINE_DATA(&m_page[offset]);
}

u16 sun2_enet_device::net_r(offs_t offset, u16 mem_mask)
{
	u16 const page = m_page[BIT(offset, 9, 10)];
	offs_t const physical = (page & PM_PN) << 9 | (offset & 0x1ff);

	u16 const data = m_ram[physical];

	LOGMASKED(LOG_NET, "%s: net_r 0x%06x phys 0x%06x data 0x%04x mask 0x%04x\n",
		machine().describe_context(), offset << 1, physical << 1, data, mem_mask);

	return data;
}

void sun2_enet_device::net_w(offs_t offset, u16 data, u16 mem_mask)
{
	u16 const page = m_page[BIT(offset, 9, 10)];
	offs_t const physical = (page & PM_PN) << 9 | (offset & 0x1ff);

	LOGMASKED(LOG_NET, "%s: net_w 0x%06x phys 0x%06x data 0x%04x mask 0x%04x\n",
		machine().describe_context(), offset << 1, physical << 1, data, mem_mask);

	COMBINE_DATA(&m_ram[physical]);
}

u16 sun2_enet_device::bus_r(offs_t offset, u16 mem_mask)
{
	u16 const page = m_page[BIT(offset, 9, 8)];
	offs_t const physical = (page & PM_PN) << 9 | (offset & 0x1ff);

	u16 data = m_ram[physical];

	LOGMASKED(LOG_BUS, "%s: bus_r 0x%06x phys 0x%06x data 0x%04x mask 0x%04x\n",
		machine().describe_context(), offset << 1, physical << 1, data, mem_mask);

	if (page & PM_SWP)
		data = swapendian_int16(data);

	return data;
}

void sun2_enet_device::bus_w(offs_t offset, u16 data, u16 mem_mask)
{
	u16 const page = m_page[BIT(offset, 9, 8)];
	offs_t const physical = (page & PM_PN) << 9 | (offset & 0x1ff);

	LOGMASKED(LOG_BUS, "%s: bus_w 0x%06x phys 0x%06x data 0x%04x mask 0x%04x\n",
		machine().describe_context(), offset << 1, physical << 1, data, mem_mask);

	if (page & PM_SWP)
	{
		data = swapendian_int16(data);
		mem_mask = swapendian_int16(mem_mask);
	}

	COMBINE_DATA(&m_ram[physical]);
}

/*
 * Only the settings for the first and second controllers in a system as
 * documented in the field engineer handbook are available here.
 */
static INPUT_PORTS_START(sun2_enet)
	// this jumper block does not have pins fitted, and is hard-wired to level 3
	PORT_START("J500")
	PORT_DIPNAME(0xff, 0x08, "Interrupt")
	PORT_DIPSETTING(0x00, "None")
	PORT_DIPSETTING(0x01, "0")
	PORT_DIPSETTING(0x02, "1")
	PORT_DIPSETTING(0x04, "2")
	PORT_DIPSETTING(0x08, "3")
	PORT_DIPSETTING(0x10, "4")
	PORT_DIPSETTING(0x20, "5")
	PORT_DIPSETTING(0x40, "6")
	PORT_DIPSETTING(0x80, "7")

	PORT_START("U503")
	PORT_DIPNAME(0xff, 0x88, "Control") PORT_DIPLOCATION("U503:1,2,3,4,5,6,7,8")
	PORT_DIPSETTING(0x88, "0x88000") // controller #1
	PORT_DIPSETTING(0x8c, "0x8c000") // controller #2

	PORT_START("U505")
	PORT_DIPNAME(0x0f, 0x04, "Memory") PORT_DIPLOCATION("U505:1,2,3,4")
	PORT_DIPSETTING(0x04, "0x40000") // controller #1
	PORT_DIPSETTING(0x0a, "0xa0000") // controller #2

	PORT_START("U506")
	PORT_DIPNAME(0xff, 0x66, "Window") PORT_DIPLOCATION("U506:1,2,3,4,5,6,7,8")
	PORT_DIPSETTING(0x66, "256K") // controller #1
	//PORT_DIPSETTING(0x96, "128K")
	PORT_DIPSETTING(0x99, "64K")  // controller #2
INPUT_PORTS_END

ioport_constructor sun2_enet_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(sun2_enet);
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(SUN2_ENET, device_multibus_interface, sun2_enet_device, "sun2_enet", "Sun-2 Ethernet")
