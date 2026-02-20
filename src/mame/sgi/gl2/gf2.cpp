// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * SGI IRIS GF2 Geometry Pipeline/Frame Buffer Controller
 *
 * Sources:
 *  -
 *
 * TODO:
 *  - everything
 */

#include "emu.h"

#include "gf2.h"

#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

namespace {

class sgi_gf2_device
	: public device_t
	, public device_multibus_interface
{
public:
	sgi_gf2_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
		: device_t(mconfig, SGI_GF2, tag, owner, clock)
		, device_multibus_interface(mconfig, *this)
		, m_installed(false)
	{
	}

	static constexpr feature_type unemulated_features() { return feature::GRAPHICS; }

protected:
	//virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	//virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	u16 fbc_readdpx_r() { return 0; }
	void fbc_clrint_w(u16 data) { LOG("%s: fbc_clrint_w 0x%04x\n", machine().describe_context(), data); }
	u16 fbc_flags_r() { return 0x0002; }
	void fbc_flags_w(u16 data) { LOG("%s: fbc_flags_w 0x%04x\n", machine().describe_context(), data); }
	u16 fbc_data_r() { return 0; }
	void fbc_data_w(u16 data) { LOG("%s: fbc_data_w 0x%04x\n", machine().describe_context(), data); }

	void ge_flags_w(u16 data) { LOG("%s: ge_flags_w 0x%04x\n", machine().describe_context(), data); }

private:
	bool m_installed;
};

void sgi_gf2_device::device_start()
{
}

void sgi_gf2_device::device_reset()
{
	if (!m_installed)
	{
		m_bus->space(AS_IO).install_readwrite_handler(0x2000, 0x2001,
			emu::rw_delegate(*this, FUNC(sgi_gf2_device::fbc_readdpx_r)),
			emu::rw_delegate(*this, FUNC(sgi_gf2_device::fbc_clrint_w)));

		m_bus->space(AS_IO).install_readwrite_handler(0x2400, 0x2401,
			emu::rw_delegate(*this, FUNC(sgi_gf2_device::fbc_flags_r)),
			emu::rw_delegate(*this, FUNC(sgi_gf2_device::fbc_flags_w)));

		m_bus->space(AS_IO).install_readwrite_handler(0x2800, 0x2801,
			emu::rw_delegate(*this, FUNC(sgi_gf2_device::fbc_data_r)),
			emu::rw_delegate(*this, FUNC(sgi_gf2_device::fbc_data_w)));

		m_bus->space(AS_IO).install_write_handler(0x2c00, 0x2c01,
			emu::rw_delegate(*this, FUNC(sgi_gf2_device::ge_flags_w)));

		m_installed = true;
	}
}

void sgi_gf2_device::device_add_mconfig(machine_config &config)
{
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(SGI_GF2, device_multibus_interface, sgi_gf2_device, "sgi_gf2", "Silicon Graphics GF2 Geometry Pipeline/Frame Buffer Controller")
