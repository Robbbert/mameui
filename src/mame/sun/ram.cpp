// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

#include "emu.h"
#include "ram.h"

namespace {

class sun_ram_device
	: public device_t
	, public device_multibus_interface
{
public:
	sun_ram_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
		: device_t(mconfig, SUN_RAM, tag, owner, clock)
		, device_multibus_interface(mconfig, *this)
		, m_ram(*this, "ram", 0x40'0000, ENDIANNESS_BIG)
	{
	}

protected:
	virtual void device_start() override ATTR_COLD;

private:
	memory_share_creator<u16> m_ram;
};

void sun_ram_device::device_start()
{
	m_bus->space(AS_DATA).install_ram(0x00'0000, 0x3f'ffff, m_ram);
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(SUN_RAM, device_multibus_interface, sun_ram_device, "sun_ram", "Sun Microsystems Sun-2/120 memory board")
