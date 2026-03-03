// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * TODO:
 */
#include "emu.h"
#include "scsi.h"
#include "sc.h"

#include "machine/nscsi_bus.h"
#include "machine/z80scc.h"
#include "bus/rs232/rs232.h"

#include "bus/nscsi/hd.h"
#include "bus/nscsi/tape.h"

#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

namespace {

class sun_scsi_device
	: public device_t
	, public device_multibus_interface
{
public:
	sun_scsi_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
		: device_t(mconfig, SUN_SCSI, tag, owner, clock)
		, device_multibus_interface(mconfig, *this)
		, m_sc(*this, "scsi:7:sc")
		, m_scc(*this, "scc%u", 0U)
		, m_port(*this, "port%u", 0U)
	{
	}

	void map(address_map &map);

protected:
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	template <bool High> void addr_w(u16 data);
	u16 count_r();
	void count_w(u16 data);

private:
	required_device<sun_sc_device> m_sc;
	required_device_array<scc8530_device, 2> m_scc;
	required_device_array<rs232_port_device, 4> m_port;

	u32 m_addr;
	u16 m_count;
};

void sun_scsi_device::device_start()
{
	m_addr = 0;
	m_count = 0;
}

void sun_scsi_device::device_reset()
{
	m_bus->space(AS_PROGRAM).install_device(0x8'0000, 0x8'3fff, *this, &sun_scsi_device::map);
}

static void scsi_devices(device_slot_interface &device)
{
	device.option_add("disk", NSCSI_HARDDISK);
	device.option_add("tape", NSCSI_TAPE);
}
void sun_scsi_device::device_add_mconfig(machine_config &config)
{
	NSCSI_BUS(config, "scsi");
	NSCSI_CONNECTOR(config, "scsi:0", scsi_devices, "disk",  false);
	NSCSI_CONNECTOR(config, "scsi:1", scsi_devices, nullptr, false);
	NSCSI_CONNECTOR(config, "scsi:2", scsi_devices, nullptr, false);
	NSCSI_CONNECTOR(config, "scsi:3", scsi_devices, nullptr, false);
	NSCSI_CONNECTOR(config, "scsi:4", scsi_devices, nullptr, false);
	NSCSI_CONNECTOR(config, "scsi:5", scsi_devices, nullptr,  false);
	NSCSI_CONNECTOR(config, "scsi:6", scsi_devices, nullptr, false);
	NSCSI_CONNECTOR(config, "scsi:7").option_set("sc", SUN_SC);
#if false
	.machine_config(
		[this](device_t *device)
		{
			wd33c9x_base_device &wd33c93(downcast<wd33c9x_base_device &>(*device));

			wd33c93.set_clock(10'000'000);
			wd33c93.irq_cb().set(*this, FUNC(sgi_ip4_device::lio_irq<LIO_SCSI>)).invert();
			wd33c93.drq_cb().set(*this, FUNC(sgi_ip4_device::scsi_drq));
		});
#endif


	SCC8530(config, m_scc[0], 19.6608_MHz_XTAL / 4);
	m_scc[0]->out_txda_callback().set(m_port[0], FUNC(rs232_port_device::write_txd));
	m_scc[0]->out_txdb_callback().set(m_port[1], FUNC(rs232_port_device::write_txd));

	RS232_PORT(config, m_port[0], default_rs232_devices, nullptr);
	m_port[0]->rxd_handler().set(m_scc[0], FUNC(scc8530_device::rxa_w));
	m_port[0]->dcd_handler().set(m_scc[0], FUNC(scc8530_device::dcda_w));
	m_port[0]->cts_handler().set(m_scc[0], FUNC(scc8530_device::ctsa_w));

	RS232_PORT(config, m_port[1], default_rs232_devices, nullptr);
	m_port[1]->rxd_handler().set(m_scc[0], FUNC(scc8530_device::rxb_w));
	m_port[1]->dcd_handler().set(m_scc[0], FUNC(scc8530_device::dcdb_w));
	m_port[1]->cts_handler().set(m_scc[0], FUNC(scc8530_device::ctsb_w));

	SCC8530(config, m_scc[1], 19.6608_MHz_XTAL / 4);
	m_scc[1]->out_txda_callback().set(m_port[2], FUNC(rs232_port_device::write_txd));
	m_scc[1]->out_txdb_callback().set(m_port[3], FUNC(rs232_port_device::write_txd));

	RS232_PORT(config, m_port[2], default_rs232_devices, nullptr);
	m_port[2]->rxd_handler().set(m_scc[1], FUNC(scc8530_device::rxa_w));
	m_port[2]->dcd_handler().set(m_scc[1], FUNC(scc8530_device::dcda_w));
	m_port[2]->cts_handler().set(m_scc[1], FUNC(scc8530_device::ctsa_w));

	RS232_PORT(config, m_port[3], default_rs232_devices, nullptr);
	m_port[3]->rxd_handler().set(m_scc[1], FUNC(scc8530_device::rxb_w));
	m_port[3]->dcd_handler().set(m_scc[1], FUNC(scc8530_device::dcdb_w));
	m_port[3]->cts_handler().set(m_scc[1], FUNC(scc8530_device::ctsb_w));
}

void sun_scsi_device::map(address_map &map)
{
	map(0x0000, 0x0005).m(m_sc, FUNC(sun_sc_device::map));

	map(0x0008, 0x0009).w(FUNC(sun_scsi_device::addr_w<1>));
	map(0x000a, 0x000b).w(FUNC(sun_scsi_device::addr_w<0>));
	map(0x000c, 0x000d).rw(FUNC(sun_scsi_device::count_r), FUNC(sun_scsi_device::count_w));

	map(0x0800, 0x0807).rw(m_scc[0], FUNC(scc8530_device::ab_dc_r), FUNC(scc8530_device::ab_dc_w)).umask16(0xff00);
	map(0x1000, 0x1007).rw(m_scc[1], FUNC(scc8530_device::ab_dc_r), FUNC(scc8530_device::ab_dc_w)).umask16(0xff00);
}

template <bool High> void sun_scsi_device::addr_w(u16 data)
{
	LOG("%s: addr_w<%u> 0x%04x\n", machine().describe_context(), High, data);

	if (High)
		m_addr = u32(data & 0x00ffU) << 16 | u16(m_addr);
	else
		m_addr = (m_addr & 0x00ff'0000U) | data;
}
u16 sun_scsi_device::count_r()
{
	return m_count;
}
void sun_scsi_device::count_w(u16 data)
{
	LOG("%s: count_w 0x%04x\n", machine().describe_context(), data);

	m_count = data;
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(SUN_SCSI, device_multibus_interface, sun_scsi_device, "sun_scsi", "Sun Microsystems Sun-2/120 SCSI board")
