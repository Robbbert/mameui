// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * Sun-2 Multibus Memory Board.
 *
 *  501-1013  Sun-2 Multibus (1MB)        Sun-2/100U/120/150U/170  
 *  501-1048  Sun-2 Multibus Prime (1MB)  Sun-2/120/170            
 *  501-1232  Sun-2 Multibus Prime (4MB)  Sun-2/100U/120/150U/170  
 *
 * Sources:
 *  - Engineering Manual for the Sun-2/120 Memory Board, Revision 50 of 28 September 1984, Sun Microsystems, Inc.
 *
 * TODO:
 *  - parity
 */

#include "emu.h"
#include "sun2_ram.h"

namespace {

class sun2_ram_device_base
	: public device_t
	, public device_multibus_interface
{
protected:
	sun2_ram_device_base(machine_config const &mconfig, device_type type, char const *tag, device_t *owner, u32 clock, size_t ram_size)
		: device_t(mconfig, type, tag, owner, clock)
		, device_multibus_interface(mconfig, *this)
		, m_ram(*this, "ram", ram_size, ENDIANNESS_BIG)
		, m_installed(false)
	{
	}

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual u8 config() = 0;

private:
	memory_share_creator<u16> m_ram;

	bool m_installed;
};

void sun2_ram_device_base::device_start()
{
}

void sun2_ram_device_base::device_reset()
{
	if (!m_installed)
	{
		if (m_bus->p2())
		{
			u8 const bank = config();

			// install up to 8 banks of 1M each
			for (unsigned i = 0; i < 8; i++)
			{
				if (BIT(bank, i))
				{
					offs_t const base = i * 0x10'0000;

					m_bus->p2()->install_ram(base, base | 0xf'ffffU, &m_ram[(base % m_ram.bytes()) >> 1]);
				}
			}
		}
		else
			logerror("Multibus P2 space not available\n");

		m_installed = true;
	}
}

class sun2_1mb_device
	: public sun2_ram_device_base
{
public:
	sun2_1mb_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
		: sun2_ram_device_base(mconfig, SUN2_1MB, tag, owner, clock, 0x10'0000)
		, m_u506(*this, "U506")
	{
	}

protected:
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;

	virtual u8 config() override;

private:
	required_ioport m_u506;
};

u8 sun2_1mb_device::config()
{
	return m_u506->read();
}

INPUT_PORTS_START(sun2_1mb)
	PORT_START("U506")
	PORT_DIPNAME(0xff, 0x01, "Memory Size")
	PORT_DIPSETTING(0x01, "1st MB")
	PORT_DIPSETTING(0x02, "2nd MB")
	PORT_DIPSETTING(0x04, "3rd MB")
	PORT_DIPSETTING(0x08, "4th MB")
	PORT_DIPSETTING(0x10, "5th MB")
	PORT_DIPSETTING(0x20, "6th MB")
	PORT_DIPSETTING(0x40, "7th MB") // non-standard
	PORT_DIPSETTING(0x80, "8th MB") // non-standard
INPUT_PORTS_END

ioport_constructor sun2_1mb_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(sun2_1mb);
}

class sun2_4mb_device
	: public sun2_ram_device_base
{
public:
	sun2_4mb_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
		: sun2_ram_device_base(mconfig, SUN2_4MB, tag, owner, clock, 0x40'0000)
		, m_u1115(*this, "U1115")
	{
	}

protected:
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;

	virtual u8 config() override;

private:
	required_ioport m_u1115;
};

u8 sun2_4mb_device::config()
{
	return m_u1115->read();
}

INPUT_PORTS_START(sun2_4mb)
	PORT_START("U1115")
	PORT_DIPNAME(0x7f, 0x0f, "Memory Size")
	PORT_DIPSETTING(0x0f, "1-4MB")
	PORT_DIPSETTING(0x70, "5-7MB")
INPUT_PORTS_END

ioport_constructor sun2_4mb_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(sun2_4mb);
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(SUN2_1MB, device_multibus_interface, sun2_1mb_device, "sun2_1mb", "Sun-2 Multibus Memory Board 1MB")
DEFINE_DEVICE_TYPE_PRIVATE(SUN2_4MB, device_multibus_interface, sun2_4mb_device, "sun2_4mb", "Sun-2 Multibus Memory Board 4MB")
