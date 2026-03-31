// license:BSD-3-Clause
// copyright-holders:Miodrag Milanovic, R. Belmont

/*
 * Sun-2 machine type 1 processor.
 *
 *  - Sun-2 Multibus       501-1007
 *  - Sun-2 Multibus Prime 501-1051
 *
 *  - backplanes
 *   - Sun-2/120 501-1090
 *   - Sun-2/170 501-1050
 *   - Sun-2/120 501-1049 Prime
 *   - Sun-2/170 501-1150 Prime
 *
 * MULTIBUS
 *      370-0502 ? 0167 Computer Products Corporation TAPEMASTER
 *      370-1012        Xylogics 450 SMD controller
 *      370-1021        Sky Floating Point Processor Multibus
 * 
 *      501-0288        3COM 3C400 Ethernet
 *      501-0289        color video
 *      501-1003        monochrome video/keyboard/mouse ECL only
 *      501-1004        Sun-2 Ethernet
 *      501-1006        Sun-2 SCSI/serial Multibus
 *      501-1007        100U,2/120,2/170 CPU
 *      501-1013        1M RAM
 *      501-1048        1M RAM
 *      501-1051        2/120,2/170 CPU
 *      501-1052        monochrome video/keyboard/mouse ECL/TTL
 *      xxx-xxxx        Systech MTI-800A/1600A Multiple Terminal Interface
 *      xxx-xxxx        Systech VPC-2200 Versatec Printer/Plotter controller
 */
/***************************************************************************

        Sun-2 Models
        ------------

    2/120
        Processor(s):   68010 @ 10MHz
        CPU:            501-1007/1051
        Chassis type:   deskside
        Bus:            Multibus (9 slots)
        Oscillator(s):  39.3216MHz
        Memory:         7M physical
        Notes:          First machines in deskside chassis. Serial
                        microswitch keyboard (type 2), Mouse Systems
                        optical mouse (Sun-2).

    2/100U
        Processor(s):   68010 @ 10MHz
        CPU:            501-1007
        Bus:            Multibus
        Notes:          Upgraded Sun 100. Replaced CPU and memory boards
                        with first-generation Sun-2 CPU and memory
                        boards so original customers could run SunOS
                        1.x. Still has parallel kb/mouse interface so
                        type 1 keyboards and Sun-1 mice could be
                        connected.

    2/150U
        Notes:          Apparently also an upgraded Sun-1.

    2/170
        Chassis type:   rackmount
        Bus:            Multibus (15 slots)
        Notes:          Rackmount version of 2/120, with more slots.

How the architecture works:
    - There are 3 address sub-spaces: CPU layer, MMU layer, and device layer
    - CPU layer uses MOVS instructions to output FC 3.
    - CPU layer: the low-order address bits A4-A1 specify the device
        0100x = ID Prom
        0101x = Diagnostic register (8 bits, 8 LEDs, bit = 0 for ON, 1 for OFF)
        0110x = Bus error register
        0111x = System enable register

        Bits A5+ address the actual individual parts of these things.  ID Prom bytes
        are at 0x0008, 0x0808, 0x1008, 0x1808, 0x2008, 0x2808, 0x3008, etc.

        System enable bits:
            b0 = enable parity generation
            b1 = cause level 1 IRQ
            b2 = cause level 2 IRQ
            b3 = cause level 3 IRQ
            b4 = enable parity error checking
            b5 = enable DVMA
            b6 = enable all interrupts
            b7 = boot state (0 = boot, 1 = normal)
                In boot state, all supervisor program reads go to the EPROM.

    - MMU layer: also accessed via FC 3
        PAGE MAP at 0 + V
        SEGMENT MAP at 4 + V
        CONTEXT REG at 6 + V

    There are 8 hardware contexts.  Supervisor and User FCs can have different contexts.

    Segment map is 4096 entries, from bits 23-15 of the virtual address + 3 context bits.
    Entries are 8 bits, which point to a page map entry group (PMEG), which is 16 consecutive
    page table entries (32 KB of space).

    Page map is 4096 entries each mapping a 2K page.  There are 256 groups of 16 entries;
    the PMEG points to these 256 groups.  The page map contains a 20-bit page number,
    which combines with the 11 low bits of the original address to get a 31-bit physical address.
    The entry from 0-15 is picked with bits 15-11 of the original address.

    Page map entries are written to the PMEG determined by their segment map entry; you must
    set the segment map validly in order to write to the page map.  This is how they get away
    with having 16 MB of segment entries and only 8 MB of PMEGs.

    See http://sunstuff.org/Sun-Hardware-Ref/s2hr/part2
****************************************************************************/

#include "emu.h"
#include "sun2_mt1.h"
#include "sun2_mmu.h"

#include "bus/rs232/rs232.h"
#include "cpu/m68000/m68010.h"
#include "machine/am9513.h"
#include "machine/input_merger.h"
#include "machine/mm58167.h"
#include "machine/z80scc.h"

#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

namespace {

enum enable_mask : u8
{
	EN_PARGEN = 0x01, // enable parity generation
	EN_INT1   = 0x02, // interrupt level 1
	EN_INT2   = 0x04, // interrupt level 2
	EN_INT3   = 0x08, // interrupt level 3
	EN_PARERR = 0x10, // enable parity checking
	EN_DVMA   = 0x20, // enable direct virtual memory access
	EN_INT    = 0x40, // enable interrupts
	EN_BOOT_  = 0x80, // boot state (0=boot, 1=normal)
};

class sun2_mt1_device
	: public device_t
	, public device_multibus_interface
{
public:
	sun2_mt1_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
		: device_t(mconfig, SUN2_MT1, tag, owner, clock)
		, device_multibus_interface(mconfig, *this)
		, m_cpu(*this, "cpu")
		, m_mmu(*this, "mmu")
		, m_stc(*this, "stc")
		, m_scc(*this, "scc")
		, m_port(*this, "port%u", 0U)
		, m_rtc(*this, "rtc")
		, m_irq(*this, "irq%u", 0U)
		, m_boot(*this, "boot")
		, m_id(*this, "id")
		, m_dvma(*this, "dvma")
		, m_led(*this, "led%u", 0U)
	{
	}

protected:
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_config_complete() override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_reset_after_children() override ATTR_COLD;

	void cpu_map(address_map &map) ATTR_COLD;
	void obi_map(address_map &map) ATTR_COLD;

	u16 mmu_r(offs_t offset, u16 mem_mask);
	void mmu_w(offs_t offset, u16 data, u16 mem_mask);

	void enable_w(u8 data);
	template <unsigned I> void interrupt_w(int state);

private:
	required_device<m68010_device> m_cpu;
	required_device<sun2_mmu_device> m_mmu;
	required_device<am9513_device> m_stc;
	required_device<scc8530_device> m_scc;
	required_device_array<rs232_port_device, 2> m_port;
	required_device<mm58167_device> m_rtc;

	required_device_array<input_merger_any_high_device, 8> m_irq;

	required_region_ptr<u16> m_boot;
	required_region_ptr<u8> m_id;
	memory_view m_dvma;

	output_finder<8> m_led;

	u8 m_enable;
	u8 m_int;
};

void sun2_mt1_device::device_add_mconfig(machine_config &config)
{
	M68010(config, m_cpu, 39.3216_MHz_XTAL / 4);
	m_cpu->set_addrmap(AS_PROGRAM, &sun2_mt1_device::cpu_map);

	INPUT_MERGER_ANY_HIGH(config, m_irq[0]).output_handler().set(FUNC(sun2_mt1_device::interrupt_w<0>));
	INPUT_MERGER_ANY_HIGH(config, m_irq[1]).output_handler().set(FUNC(sun2_mt1_device::interrupt_w<1>)); // soft 1
	INPUT_MERGER_ANY_HIGH(config, m_irq[2]).output_handler().set(FUNC(sun2_mt1_device::interrupt_w<2>)); // soft 2, scsi
	INPUT_MERGER_ANY_HIGH(config, m_irq[3]).output_handler().set(FUNC(sun2_mt1_device::interrupt_w<3>)); // soft 3, net
	INPUT_MERGER_ANY_HIGH(config, m_irq[4]).output_handler().set(FUNC(sun2_mt1_device::interrupt_w<4>)); // video
	INPUT_MERGER_ANY_HIGH(config, m_irq[5]).output_handler().set(FUNC(sun2_mt1_device::interrupt_w<5>)); // stc 2,3,4,5
	INPUT_MERGER_ANY_HIGH(config, m_irq[6]).output_handler().set(FUNC(sun2_mt1_device::interrupt_w<6>)); // scc
	INPUT_MERGER_ANY_HIGH(config, m_irq[7]).output_handler().set(FUNC(sun2_mt1_device::interrupt_w<7>)); // stc 1

	// Multibus interrupts
	int_callback<1>().set(m_irq[1], FUNC(input_merger_any_high_device::in_w<0>)).invert();
	int_callback<2>().set(m_irq[2], FUNC(input_merger_any_high_device::in_w<0>)).invert();
	int_callback<3>().set(m_irq[3], FUNC(input_merger_any_high_device::in_w<0>)).invert();
	int_callback<4>().set(m_irq[4], FUNC(input_merger_any_high_device::in_w<0>)).invert();
	int_callback<5>().set(m_irq[5], FUNC(input_merger_any_high_device::in_w<0>)).invert();
	int_callback<6>().set(m_irq[6], FUNC(input_merger_any_high_device::in_w<0>)).invert();
	int_callback<7>().set(m_irq[7], FUNC(input_merger_any_high_device::in_w<0>)).invert();

	SUN2_MMU(config, m_mmu);
	m_mmu->set_boot_prom(m_boot);
	m_mmu->set_addrmap(sun2_mmu_device::OBI, &sun2_mt1_device::obi_map);
	m_mmu->berr().set(
		[this](offs_t offset, u8 data)
		{
			u8 const fc = m_cpu->get_fc();

			m_cpu->set_buserror_details(offset, data & sun2_mmu_device::READ, fc, data & sun2_mmu_device::FAULT);

			if (!(data & sun2_mmu_device::FAULT))
				m_cpu->set_input_line(M68K_LINE_BUSERROR, ASSERT_LINE);
		});

	AM9513A(config, m_stc, 39.3216_MHz_XTAL / 8);
	m_stc->fout_cb().set(m_stc, FUNC(am9513_device::gate1_w));
	m_stc->out1_cb().set(m_irq[7], FUNC(input_merger_any_high_device::in_w<1>));
	m_stc->out2_cb().set(m_irq[5], FUNC(input_merger_any_high_device::in_w<1>));
	m_stc->out3_cb().set(m_irq[5], FUNC(input_merger_any_high_device::in_w<2>));
	m_stc->out4_cb().set(m_irq[5], FUNC(input_merger_any_high_device::in_w<3>));
	m_stc->out5_cb().set(m_irq[5], FUNC(input_merger_any_high_device::in_w<4>));

	SCC8530(config, m_scc, 39.3216_MHz_XTAL / 8);
	m_scc->out_txda_callback().set(m_port[0], FUNC(rs232_port_device::write_txd));
	m_scc->out_txdb_callback().set(m_port[1], FUNC(rs232_port_device::write_txd));
	m_scc->out_int_callback().set(m_irq[6], FUNC(input_merger_any_high_device::in_w<1>));

	RS232_PORT(config, m_port[0], default_rs232_devices, nullptr);
	m_port[0]->rxd_handler().set(m_scc, FUNC(z80scc_device::rxa_w));
	m_port[0]->dcd_handler().set(m_scc, FUNC(z80scc_device::dcda_w));
	m_port[0]->cts_handler().set(m_scc, FUNC(z80scc_device::ctsa_w));

	RS232_PORT(config, m_port[1], default_rs232_devices, nullptr);
	m_port[1]->rxd_handler().set(m_scc, FUNC(z80scc_device::rxb_w));
	m_port[1]->dcd_handler().set(m_scc, FUNC(z80scc_device::dcdb_w));
	m_port[1]->cts_handler().set(m_scc, FUNC(z80scc_device::ctsb_w));

	MM58167(config, m_rtc, 32.768_kHz_XTAL);
}

void sun2_mt1_device::device_config_complete()
{
	// 00'0000-03'f800 dvma
	// 04'0000-07'f800 ethernet memory #1
	// 08'0000-08'3800 scsi #1
	// 08'4000-08'7800 scsi #2
	// 08'8000-08'b800 ethernet control #1
	// 08'c000-08'f800 ethernet control #2
	// 0a'0000-0a'f800 ethernet memory #2
	// 0c'0000-0d'f800 frame buffer

	// 0e'8000-0f'7800 sun color

	// FIXME: avoid crash in validate
	m_bus.lookup()->set_p2(m_mmu, sun2_mmu_device::OBM);

	m_mmu.lookup()->set_p1m(m_bus, AS_PROGRAM);
	m_mmu.lookup()->set_p1i(m_bus, AS_IO);
}

void sun2_mt1_device::device_start()
{
	m_led.resolve();

	save_item(NAME(m_enable));
	save_item(NAME(m_int));

	m_bus->space(AS_PROGRAM).install_view(0x0'0000, 0x3'ffff, m_dvma);

	m_dvma[0].install_readwrite_handler(0x0'0000, 0x3'ffff,
		emu::rw_delegate(m_mmu, FUNC(sun2_mmu_device::dvma_r)),
		emu::rw_delegate(m_mmu, FUNC(sun2_mmu_device::dvma_w)));

	m_int = 0;
}

void sun2_mt1_device::device_reset()
{
	m_enable = 0;

	m_dvma.disable();

	for (auto &led : m_led)
		led = 0;
}

void sun2_mt1_device::device_reset_after_children()
{
	m_cpu->set_emmu_enable(true);
}

void sun2_mt1_device::cpu_map(address_map &map)
{
	map(0x00'0000, 0xff'ffff).rw(FUNC(sun2_mt1_device::mmu_r), FUNC(sun2_mt1_device::mmu_w));
}

// on-board I/O (type 1)
void sun2_mt1_device::obi_map(address_map &map)
{
	map(0x00'0000, 0x00'07ff).rom().region("boot", 0);    // uses MMU loophole to read 32k from a 2k window
	// 000800-000fff: reserved
	// 001000-0017ff: AM9518 encryption processor (not fitted)
	// 001800-001fff: Parallel port
	map(0x00'2000, 0x00'27ff).rw(m_scc, FUNC(z80scc_device::ab_dc_r), FUNC(z80scc_device::ab_dc_w)).umask16(0xff00);
	map(0x00'2800, 0x00'2803).mirror(0x7fc).rw(m_stc, FUNC(am9513_device::read16), FUNC(am9513_device::write16));
	// 003000-0037ff: RasterOp processor (not fitted)
	map(0x00'3800, 0x00'383f).mirror(0x7c0).rw(m_rtc, FUNC(mm58167_device::read), FUNC(mm58167_device::write)).umask16(0xff00); // 12 wait states generated by PAL16R6 (U415)
}

u16 sun2_mt1_device::mmu_r(offs_t offset, u16 mem_mask)
{
	u8 const fc = m_cpu->get_fc();
	offs_t const va = offset << 1;

	switch (fc)
	{
	case 3:
		switch (offset & 7)
		{
		case 0: return m_mmu->page_r(va);
		case 1: return m_mmu->page_r(va);
		case 2: return m_mmu->segment_r(va);
		case 3: return m_mmu->context_r();
		case 4: return m_id[(offset >> 10) & 0x1f] << 8;
		case 6: return m_mmu->buserror_r();
		case 7: return m_enable;
		}
		break;

	case 6:
		// boot mode
		if (!(m_enable & EN_BOOT_))
			return m_boot[offset & 0x7fff];
		break;
	}

	return m_mmu->mmu_r(fc, va, mem_mask);
}

void sun2_mt1_device::mmu_w(offs_t offset, u16 data, u16 mem_mask)
{
	u8 const fc = m_cpu->get_fc();
	offs_t const va = offset << 1;

	switch (fc)
	{
	case 3:
		switch (offset & 7)
		{
		case 0: m_mmu->page_w(va, data, mem_mask); break;
		case 1: m_mmu->page_w(va, data, mem_mask); break;
		case 2: m_mmu->segment_w(va, data); break;
		case 3: m_mmu->context_w(0, data, mem_mask); break;
		case 5:
			for (unsigned i = 0; i < 8; i++)
				m_led[i] = BIT(data, i);
			break;
		case 7:
			if (mem_mask & 0x00ffU)
				enable_w(data);
			break;
		}
		break;

	default:
		m_mmu->mmu_w(fc, va, data, mem_mask);
		break;
	}
}

void sun2_mt1_device::enable_w(u8 data)
{
	LOG("%s: enable_w 0x%02x\n", machine().describe_context(), data);

	if ((m_enable ^ data) & EN_DVMA)
	{
		if (data & EN_DVMA)
			m_dvma.select(0);
		else
			m_dvma.disable();
	}

	// handle interrupt disable
	if ((m_enable & EN_INT) && !(data & EN_INT))
	{
		// clear active interrupts
		for (unsigned i = 1; i < 8; i++)
			if (BIT(m_int, i))
				m_cpu->set_input_line(INPUT_LINE_IRQ0 + i, CLEAR_LINE);

		m_enable &= ~EN_INT;
	}

	// handle soft interrupts 1-3
	if ((m_enable ^ data) & EN_INT1)
		m_irq[1]->in_w<1>(data & EN_INT1);
	if ((m_enable ^ data) & EN_INT2)
		m_irq[2]->in_w<1>(data & EN_INT2);
	if ((m_enable ^ data) & EN_INT3)
		m_irq[3]->in_w<1>(data & EN_INT3);

	// handle interrupt enable
	if (!(m_enable & EN_INT) && (data & EN_INT))
	{
		// assert active interrupts
		for (unsigned i = 1; i < 8; i++)
			if (BIT(m_int, i))
				m_cpu->set_input_line(INPUT_LINE_IRQ0 + i, ASSERT_LINE);
	}

	m_enable = data;
}

template <unsigned I> void sun2_mt1_device::interrupt_w(int state)
{
	if (BIT(m_int, I) != state)
	{
		if (state)
			m_int |= 1U << I;
		else
			m_int &= ~(1U << I);

		if (m_enable & EN_INT)
			m_cpu->set_input_line(INPUT_LINE_IRQ0 + I, state ? ASSERT_LINE : CLEAR_LINE);
	}
}

static INPUT_PORTS_START(sun2_mt1)
INPUT_PORTS_END

ioport_constructor sun2_mt1_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(sun2_mt1);
}

ROM_START(sun2_mt1) // ROMs are located on the '501-1007' CPU PCB at locations B11 and B10; J400 is set to 1-2 for 27128 EPROMs and 3-4 for 27256 EPROMs
	ROM_REGION16_BE(0x10000, "boot", ROMREGION_ERASEFF)

	// There is an undumped revision 1.1.2, which uses 27256 EPROMs
	ROM_SYSTEM_BIOS(0, "rev10f", "Bootrom Rev 1.0F") // 87/12/29 Copyr 1986 Sun Micro
	ROMX_LOAD("1.0f.b11", 0x0000, 0x8000, CRC(8fb0050a) SHA1(399cdb894b2a66d847d76d8a5d266906fb1d3430), ROM_SKIP(1) | ROM_BIOS(0)) // actual rom stickers had fallen off
	ROMX_LOAD("1.0f.b10", 0x0001, 0x8000, CRC(70de816d) SHA1(67e980497f463dbc529f64ec5f3e0046b3901b7e), ROM_SKIP(1) | ROM_BIOS(0)) // "

	// Rev R, @(#)version.c 2.8 85/02/19 Copyright (c) 1985 by Sun Microsystems, Inc.
	ROM_SYSTEM_BIOS(1, "revr", "Bootrom Rev R")
	ROMX_LOAD("520-1102-03.b11", 0x0000, 0x4000, CRC(020bb0a8) SHA1(a7b60e89a40757975a5d345d57ea02781dea4f89), ROM_SKIP(1) | ROM_BIOS(1))
	ROMX_LOAD("520-1101-03.b10", 0x0001, 0x4000, CRC(b97c61f7) SHA1(9f08fe232cfc3da48539fa66673fc1f89a362b1e), ROM_SKIP(1) | ROM_BIOS(1))

	// There is an undumped revision Q, with roms:
	//ROM_SYSTEM_BIOS( 8, "revq", "Bootrom Rev Q")
	// ROMX_LOAD( "520-1104-02.b11", 0x0000, 0x4000, NO_DUMP, ROM_SKIP(1) | ROM_BIOS(8))
	// ROMX_LOAD( "520-1103-02.b10", 0x0001, 0x4000, NO_DUMP, ROM_SKIP(1) | ROM_BIOS(8))

	// Rev N, @(#)version.c 2.7 84/02/06 Copyright (c) 1984 by Sun Microsystems, Inc.
	ROM_SYSTEM_BIOS( 2, "revn", "Bootrom Rev N") // SunOS 2.0 requires this bootrom version at a minimum; this version supports the sun-2 keyboard
	ROMX_LOAD("revn.b11", 0x0000, 0x4000, CRC(b1e70965) SHA1(726b3ed9323750a1ae238cf6dccaed6ff5981ad1), ROM_SKIP(1) | ROM_BIOS(2)) // actual rom stickers had fallen off
	ROMX_LOAD("revn.b10", 0x0001, 0x4000, CRC(95fd9242) SHA1(1eee2d291f4b18f6aafdde1a9521d88e454843b9), ROM_SKIP(1) | ROM_BIOS(2)) // "

	// Rev M, @(#)version.c 2.6 83/09/19 Copyright (c) 1983 by Sun Microsystems, Inc.
	ROM_SYSTEM_BIOS( 3, "revm", "Bootrom Rev M") // SunOS 1.0 apparently requires this bootrom revision; this version might only support the sun-1 keyboard?
	ROMX_LOAD("sun2-revm-8.b11", 0x0000, 0x4000, CRC(98b8ae55) SHA1(55485f4d8fd1ebc218aa8527c8bb62752c34abf7), ROM_SKIP(1) | ROM_BIOS(3)) // handwritten label: "SUN2-RevM-8"
	ROMX_LOAD("sun2-revm-0.b10", 0x0001, 0x4000, CRC(5117f431) SHA1(fce85c11ada1614152dde35bb329350f6fb2ecd9), ROM_SKIP(1) | ROM_BIOS(3)) // handwritten label: "SUN2-RevM-0"

	ROM_REGION(0x20, "id", ROMREGION_ERASEFF)
	ROM_LOAD("sun2120-idprom.bin", 0x000000, 0x000020, CRC(eec8cd1d) SHA1(6a78dc0ea6f9cc7687cffea754d65864fb751ebf))
	//ROM_LOAD("idprom.u411", 0x000000, 0x000020, CRC(73f4c19e) SHA1(c672b6bf0028dae1c31ca87fbd75dad53864c060))
ROM_END

const tiny_rom_entry *sun2_mt1_device::device_rom_region() const
{
	return ROM_NAME(sun2_mt1);
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(SUN2_MT1, device_multibus_interface, sun2_mt1_device, "sun2_mt1", "Sun Microsystems Sun-2 Machine Type 1 CPU")
