// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * TODO:
 */
#include "emu.h"
#include "sc.h"

#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

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
};

DEFINE_DEVICE_TYPE(SUN_SC, sun_sc_device, "sun_sc", "Sun Microsystems Sun-2/120 SCSI device")

sun_sc_device::sun_sc_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: nscsi_device(mconfig, SUN_SC, tag, owner, clock)
	, nscsi_slot_card_interface(mconfig, *this, DEVICE_SELF)
{
}

void sun_sc_device::device_start()
{
	m_status = 0;
	m_command = 0;
	m_ctrl = 0;
}

void sun_sc_device::device_reset()
{
}

void sun_sc_device::map(address_map &map)
{
	map(0x0000, 0x0001).rw(FUNC(sun_sc_device::data_r), FUNC(sun_sc_device::data_w));
	map(0x0002, 0x0002).rw(FUNC(sun_sc_device::status_r), FUNC(sun_sc_device::command_w));
	map(0x0004, 0x0005).rw(FUNC(sun_sc_device::ctrl_r), FUNC(sun_sc_device::ctrl_w));
}

u16 sun_sc_device::data_r()
{
	return scsi_bus->data_r() << 8;
}
void sun_sc_device::data_w(u16 data)
{
	LOG("%s: data_w 0x%04x\n", machine().describe_context(), data);

	scsi_bus->data_w(scsi_refid, data >> 8);
}
u8 sun_sc_device::status_r()
{
	return m_status;
}
void sun_sc_device::command_w(u8 data)
{
	LOG("%s: command_w 0x%02x\n", machine().describe_context(), data);

	m_command = data;
}
u16 sun_sc_device::ctrl_r()
{
	u16 data = m_ctrl & 0xf00fU;
	u32 const ctrl = scsi_bus->ctrl_r();

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
	if (ctrl & S_REQ)
		data |= CTRL_REQ;

	return data;
}
void sun_sc_device::ctrl_w(u16 data)
{
	LOG("%s: ctrl_w 0x%04x\n", machine().describe_context(), data);

	if ((data ^ m_ctrl) & CTRL_SEL)
		scsi_bus->ctrl_w(scsi_refid, (data & CTRL_SEL) ? S_SEL : 0, S_SEL);

	if ((data ^ m_ctrl) & CTRL_RST)
		scsi_bus->ctrl_w(scsi_refid, (data & CTRL_RST) ? S_RST : 0, S_RST);

	m_ctrl = data;
}
