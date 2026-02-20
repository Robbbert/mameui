// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * SGI IRIS BP3 Bitplane
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
 *  - each bit plane is 1024x1024x1
 *  - BP3 consists of 4 bit planes
 *  - 1024x768 displayed
 *  - bitplanes can store different data depending on mode
 *   - RGB mode: digital color codes, minimum 6 BP3 boards, 3x8-bit color
 *   - color map mode: index into color map
 *   - z-buffer mode: depth data (12 bitplanes also hold color map data)
 *  - bitplanes are divided into halves A&B
 *  - each half is used in double buffer mode
 *   - single buffer mode: B1=8 A1=4 B0=2 A0=1
 *   - double buffer mode: A1/B1=2 A0/B0=1
 *  - lower numbered slots produce higher-order bitplane data
 *  - 
 */
#include "emu.h"

#include "bp3.h"

#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

namespace {

class sgi_bp3_device
	: public device_t
	, public device_multibus_interface
{
public:
	sgi_bp3_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
		: device_t(mconfig, SGI_BP3, tag, owner, clock)
		, device_multibus_interface(mconfig, *this)
		, m_installed(false)
	{
	}

protected:
	//virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	//virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	bool m_installed;
};

void sgi_bp3_device::device_start()
{
}

void sgi_bp3_device::device_reset()
{
	if (!m_installed)
	{
		//m_bus->space(AS_IO).install_read_handler(0x1400, 0x1401, emu::rw_delegate(*this, FUNC(sgi_bp3_device::)));
		//m_bus->space(AS_IO).install_write_handler(0x1400, 0x1401, emu::rw_delegate(*this, FUNC(sgi_bp3_device::)));

		m_installed = true;
	}
}

void sgi_bp3_device::device_add_mconfig(machine_config &config)
{
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(SGI_BP3, device_multibus_interface, sgi_bp3_device, "sgi_bp3", "Silicon Graphics BP3 Bitplane")
