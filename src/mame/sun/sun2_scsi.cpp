// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * TODO:
 *
 * WIP
 * --
 *  - sun2 crashes at 0 after reading first block
 * 
 */

#include "emu.h"
#include "sun2_scsi.h"

#include "machine/nscsi_bus.h"
#include "machine/z80scc.h"
#include "bus/rs232/rs232.h"

#include "bus/nscsi/hd.h"
#include "bus/nscsi/tape.h"

#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

namespace {

enum ctrl_mask : u16
{
	CTRL_IRQEN = 0x0001, // interrupt enable
	CTRL_DMAEN = 0x0002, // DMA enable
	CTRL_WRDEN = 0x0004, // word mode enable
	CTRL_PAREN = 0x0008, // parity enable
	CTRL_RST   = 0x0010, // SCSI RST
	CTRL_SEL   = 0x0020, // SCSI SEL
	CTRL_BSY   = 0x0040, // SCSI BSY (ro)
	CTRL_PAR   = 0x0080, // SCSI PAR (ro)
	CTRL_IO    = 0x0100, // SCSI I/O (ro)
	CTRL_CD    = 0x0200, // SCSI C/D (ro)
	CTRL_MSG   = 0x0400, // SCSI MSG (ro)
	CTRL_REQ   = 0x0800, // SCSI REQ (ro)
	CTRL_IRQ   = 0x1000, // interrupt request (ro)
	CTRL_ODD   = 0x2000, // odd length (ro)
	CTRL_BERR  = 0x4000, // bus error (ro) 
	CTRL_PERR  = 0x8000, // parity error (ro)

	CTRL_RMASK = 0xf80f,
	CTRL_WMASK = 0x003f,
};

class sun2_scsi_device
	: public device_t
	, public device_multibus_interface
	, public nscsi_device_interface
{
public:
	sun2_scsi_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
		: device_t(mconfig, SUN2_SCSI, tag, owner, clock)
		, device_multibus_interface(mconfig, *this)
		, nscsi_device_interface(mconfig, *this)
		, m_scsi(*this, "scsi")
		, m_scc(*this, "scc%u", 0U)
		, m_port(*this, "port%u", 0U)
		, m_dma(nullptr)
	{
	}

	void map(address_map &map);

protected:
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void scsi_ctrl_changed() override;

	u16 data_r(offs_t offset, u16 mem_mask);
	void data_w(offs_t offset, u16 data, u16 mem_mask);
	u8 status_r();
	void command_w(u8 data);
	u16 ctrl_r();
	void ctrl_w(u16 data);
	template <bool High> void addr_w(u16 data);
	u16 count_r();
	void count_w(u16 data);

	void dma(s32 param);

	void interrupt(bool assert);

private:
	required_device<nscsi_bus_device> m_scsi;
	required_device_array<scc8530_device, 2> m_scc;
	required_device_array<rs232_port_device, 4> m_port;

	emu_timer *m_dma;

	u16 m_data;
	u16 m_ctrl;
	u32 m_addr;
	u16 m_count;
};

static void scsi_devices(device_slot_interface &device)
{
	device.option_add("disk", NSCSI_HARDDISK);
	device.option_add("tape", NSCSI_TAPE);
}
void sun2_scsi_device::device_add_mconfig(machine_config &config)
{
	NSCSI_BUS(config, m_scsi);
	NSCSI_CONNECTOR(config, "scsi:0", scsi_devices, "disk",  false);
	NSCSI_CONNECTOR(config, "scsi:1", scsi_devices, nullptr, false);
	NSCSI_CONNECTOR(config, "scsi:2", scsi_devices, nullptr, false);
	NSCSI_CONNECTOR(config, "scsi:3", scsi_devices, nullptr, false);
	NSCSI_CONNECTOR(config, "scsi:4", scsi_devices, nullptr, false);
	NSCSI_CONNECTOR(config, "scsi:5", scsi_devices, nullptr,  false);
	NSCSI_CONNECTOR(config, "scsi:6", scsi_devices, nullptr, false);
	m_scsi->set_external_device(7, *this);

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

void sun2_scsi_device::device_start()
{
	m_dma = timer_alloc(FUNC(sun2_scsi_device::dma), this);

	m_ctrl = 0;
	m_addr = 0;
	m_count = 0;
}

void sun2_scsi_device::device_reset()
{
	m_bus->space(AS_PROGRAM).install_device(0x8'0000, 0x8'3fff, *this, &sun2_scsi_device::map);

	// monitor all control lines
	m_scsi_bus->ctrl_wait(m_scsi_refid, S_ALL, S_ALL);
}

void sun2_scsi_device::map(address_map &map)
{
	map(0x0000, 0x0001).rw(FUNC(sun2_scsi_device::data_r), FUNC(sun2_scsi_device::data_w));
	map(0x0003, 0x0003).rw(FUNC(sun2_scsi_device::status_r), FUNC(sun2_scsi_device::command_w));
	map(0x0004, 0x0005).rw(FUNC(sun2_scsi_device::ctrl_r), FUNC(sun2_scsi_device::ctrl_w));
	map(0x0008, 0x0009).w(FUNC(sun2_scsi_device::addr_w<1>));
	map(0x000a, 0x000b).w(FUNC(sun2_scsi_device::addr_w<0>));
	map(0x000c, 0x000d).rw(FUNC(sun2_scsi_device::count_r), FUNC(sun2_scsi_device::count_w));

	//map(0x0800, 0x0807).rw(m_scc[0], FUNC(scc8530_device::ab_dc_r), FUNC(scc8530_device::ab_dc_w)).umask16(0xff00);
	//map(0x1000, 0x1007).rw(m_scc[1], FUNC(scc8530_device::ab_dc_r), FUNC(scc8530_device::ab_dc_w)).umask16(0xff00);
}

void sun2_scsi_device::scsi_ctrl_changed()
{
	u32 const ctrl = m_scsi_bus->ctrl_r();

	if (!(m_ctrl & CTRL_REQ) && (ctrl & S_REQ))
	{
		switch (ctrl & S_PHASE_MASK)
		{
		case S_PHASE_DATA_OUT:
		case S_PHASE_DATA_IN:
			if (m_ctrl & CTRL_DMAEN)
				m_dma->adjust(attotime::zero, ctrl & S_INP);
			else
				interrupt(true);
			break;

		case S_PHASE_COMMAND:
			m_ctrl |= CTRL_REQ;
			break;

		case S_PHASE_STATUS:
		case S_PHASE_MSG_OUT:
		case S_PHASE_MSG_IN:
			m_ctrl |= CTRL_REQ;
			interrupt(true);
			break;
		}
	}
	else
		m_scsi_bus->ctrl_w(m_scsi_refid, 0, S_ACK);
}

u16 sun2_scsi_device::data_r(offs_t offset, u16 mem_mask)
{
	return m_scsi_bus->data_r() << 8;
}
void sun2_scsi_device::data_w(offs_t offset, u16 data, u16 mem_mask)
{
	LOG("%s: data_w 0x%04x mask 0x%04x\n", machine().describe_context(), data, mem_mask);

	m_scsi_bus->data_w(m_scsi_refid, data >> 8);
}
u8 sun2_scsi_device::status_r()
{
	if ((m_scsi_bus->ctrl_r() & (S_REQ | S_CTL | S_INP)) == (S_REQ | S_CTL | S_INP))
	{
		m_ctrl &= ~CTRL_REQ;

		u8 const data = m_scsi_bus->data_r();
		LOG("%s: status_r 0x%02x\n", machine().describe_context(), data);

		m_scsi_bus->ctrl_w(m_scsi_refid, S_ACK, S_ACK);

		interrupt(false);
	}
	else
		LOG("%s: status_r\n", machine().describe_context());

	return 0;
}
void sun2_scsi_device::command_w(u8 data)
{
	LOG("%s: command_w 0x%02x\n", machine().describe_context(), data);

	if ((m_scsi_bus->ctrl_r() & (S_REQ | S_CTL | S_INP)) == (S_REQ | S_CTL))
	{
		m_ctrl &= ~CTRL_REQ;

		m_scsi_bus->data_w(m_scsi_refid, data);
		m_scsi_bus->ctrl_w(m_scsi_refid, S_ACK, S_ACK);
	}
}
u16 sun2_scsi_device::ctrl_r()
{
	u16 data = m_ctrl & CTRL_RMASK;

	if (u32 const ctrl = m_scsi_bus->ctrl_r())
	{
		if (ctrl & S_RST)
			data |= CTRL_RST;
		if (ctrl & S_SEL)
			data |= CTRL_SEL;
		if (ctrl & S_BSY)
			data |= CTRL_BSY;
		//if (ctrl & S_PAR)
		//	data |= CTRL_PAR;
		if (ctrl & S_INP)
			data |= CTRL_IO;
		if (ctrl & S_CTL)
			data |= CTRL_CD;
		if (ctrl & S_MSG)
			data |= CTRL_MSG;
	}

	return data;
}
void sun2_scsi_device::ctrl_w(u16 data)
{
	LOG("%s: ctrl_w 0x%04x\n", machine().describe_context(), data);

	if ((data ^ m_ctrl) & CTRL_SEL)
		m_scsi_bus->ctrl_w(m_scsi_refid, (data & CTRL_SEL) ? S_SEL : 0, S_SEL);

	if ((data ^ m_ctrl) & CTRL_RST)
		m_scsi_bus->ctrl_w(m_scsi_refid, (data & CTRL_RST) ? S_RST : 0, S_RST);

	m_ctrl = (m_ctrl & ~CTRL_WMASK) | (data & CTRL_WMASK);
}
template <bool High> void sun2_scsi_device::addr_w(u16 data)
{
	LOG("%s: addr_w<%u> 0x%04x\n", machine().describe_context(), High, data);

	if (High)
		m_addr = u32(data & 0x00ffU) << 16 | u16(m_addr);
	else
		m_addr = (m_addr & 0x00ff'0000U) | data;
}
u16 sun2_scsi_device::count_r()
{
	return m_count;
}
void sun2_scsi_device::count_w(u16 data)
{
	LOG("%s: count_w 0x%04x\n", machine().describe_context(), data);

	m_count = data;
}

void sun2_scsi_device::dma(s32 param)
{
	// TODO: byte mode

	if (param)
	{
		// read byte from scsi
		if (m_ctrl & CTRL_ODD)
			m_data |= m_scsi_bus->data_r();
		else
			m_data = m_scsi_bus->data_r() << 8;

		// flip odd bit
		m_ctrl ^= CTRL_ODD;

		if (!(m_ctrl & CTRL_ODD))
		{
			// transfer word to memory
			m_bus->space(AS_PROGRAM).write_word(m_addr, m_data);

			m_addr += 2;
			m_count += 2;
		}
	}
	else
	{
		if (!(m_ctrl & CTRL_ODD))
		{
			// transfer word from memory
			m_data = m_bus->space(AS_PROGRAM).read_word(m_addr);

			m_addr += 2;
			m_count += 2;

			// write first byte to scsi
			m_scsi_bus->data_w(m_scsi_refid, u8(m_data >> 8));
		}
		else
			// write second byte to scsi
			m_scsi_bus->data_w(m_scsi_refid, u8(m_data));

		// flip odd bit
		m_ctrl ^= CTRL_ODD;
	}

	// assert ACK
	m_scsi_bus->ctrl_w(m_scsi_refid, S_ACK, S_ACK);
}

void sun2_scsi_device::interrupt(bool assert)
{
	if (assert == bool(m_ctrl & CTRL_IRQ))
		return;

	if (assert)
		m_ctrl |= CTRL_IRQ;
	else
		m_ctrl &= ~CTRL_IRQ;

	if (m_ctrl & CTRL_IRQEN)
		int_w<2>(!assert);
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(SUN2_SCSI, device_multibus_interface, sun2_scsi_device, "sun2_scsi", "Sun Microsystems Sun-2/120 SCSI board")
