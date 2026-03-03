// license:BSD-3-Clause
// copyright-holders:Miodrag Milanovic, R. Belmont

/*
 * Sun-2 machine type 1 processor.
 *
 * 501-1007  2/100U, 2/150U
 * 501-1051  2/120, 2/170
 *
 * MULTIBUS
 *      370-0502 ? 0167 Computer Products Corporation TAPEMASTER
 *      370-1012        Xylogics 450 SMD controller
 *      370-1021        Sky Floating Point Processor Multibus
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
#include "mt1.h"

#include "cpu/m68000/m68010.h"
#include "machine/am9513.h"
#include "machine/mm58167.h"
#include "machine/z80scc.h"
#include "machine/bankdev.h"
#include "machine/input_merger.h"
#include "bus/rs232/rs232.h"

namespace {

// page table entry constants
#define PM_VALID    (0x80000000)    // page is valid
#define PM_PROTMASK (0x7e000000)    // protection mask
#define PM_TYPEMASK (0x01c00000)    // type mask
#define PM_ACCESSED (0x00200000)    // accessed flag
#define PM_MODIFIED (0x00100000)    // modified flag

class sun_mt1_device
	: public device_t
	, public device_multibus_interface
{
public:
	sun_mt1_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
		: device_t(mconfig, SUN_MT1, tag, owner, clock)
		, device_multibus_interface(mconfig, *this)
		, m_cpu(*this, "cpu")
		, m_boot(*this, "boot")
		, m_id(*this, "id")
		, m_type1space(*this, "type1")
		, m_type2space(*this, "type2")
		, m_type3space(*this, "type3")
		, m_scc(*this, "scc")
		, m_port(*this, "port%u", 0U)
		, m_stc(*this, "stc")
		, m_rtc(*this, "rtc")
	{
	}

protected:
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	required_device<m68010_device> m_cpu;
	required_region_ptr<u16> m_boot;
	required_region_ptr<u8> m_id;
	required_device<address_map_bank_device> m_type1space, m_type2space, m_type3space;
	required_device<scc8530_device> m_scc;
	required_device_array<rs232_port_device, 2> m_port;
	required_device<am9513_device> m_stc;
	required_device<mm58167_device> m_rtc;

	memory_access<24, 1, 0, ENDIANNESS_LITTLE>::specific m_bus_mem;
	memory_access<16, 1, 0, ENDIANNESS_LITTLE>::specific m_bus_pio;

	uint16_t mmu_r(offs_t offset, uint16_t mem_mask = ~0);
	void mmu_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t tl_mmu_r(uint8_t fc, offs_t offset, uint16_t mem_mask);
	void tl_mmu_w(uint8_t fc, offs_t offset, uint16_t data, uint16_t mem_mask);

	void mbustype1space_map(address_map &map) ATTR_COLD;
	void mbustype2space_map(address_map &map) ATTR_COLD;
	void mbustype3space_map(address_map &map) ATTR_COLD;
	void sun2_mem(address_map &map) ATTR_COLD;

	uint16_t m_diagreg, m_sysenable, m_buserror;
	uint16_t m_context;
	uint8_t m_segmap[8][512];
	uint32_t m_pagemap[4097];
};

uint16_t sun_mt1_device::mmu_r(offs_t offset, uint16_t mem_mask)
{
	return tl_mmu_r(m_cpu->get_fc(), offset, mem_mask);
}

uint16_t sun_mt1_device::tl_mmu_r(uint8_t fc, offs_t offset, uint16_t mem_mask)
{
	if ((fc == 3) && !machine().side_effects_disabled())
	{
		if (offset & 0x4)   // set for CPU space
		{
			switch (offset & 7)
			{
				case 4:
					//printf("sun2: Read IDPROM @ %x (PC=%x)\n", offset<<1, m_cpu->pc());
					return m_id[(offset>>10) & 0x1f]<<8;

				case 5:
					//printf("sun2: Read diag reg\n");
					return m_diagreg;

				case 6:
					//printf("sun2: Read bus error @ PC %x\n", m_cpu->pc());
					return m_buserror;

				case 7:
					//printf("sun2: Read sysenable\n");
					return m_sysenable;
			}
		}
		else                // clear for MMU space
		{
			int page;

			switch (offset & 3)
			{
				case 0: // page map
				case 1:
					page = m_segmap[m_context & 7][offset >> 14] << 4;
					page += ((offset >> 10) & 0xf);

					//printf("sun2: Read page map at %x (entry %d)\n", offset<<1, page);
					if (offset & 1) // low-order 16 bits
					{
						return m_pagemap[page] & 0xffff;
					}
					return m_pagemap[page] >> 16;

				case 2: // segment map
					//printf("sun2: Read segment map at %x (entry %d, user ctx %d)\n", offset<<1, offset>>14, m_context & 7);
					return m_segmap[m_context & 7][offset >> 14];

				case 3: // context reg
					//printf("sun2: Read context reg\n");
					return m_context;
			}
		}
	}

	// boot mode?
	if ((fc == M68K_FC_SUPERVISOR_PROGRAM) && !(m_sysenable & 0x80))
	{
		return m_boot[offset & 0x3fff];
	}

	// debugger hack
	if (machine().side_effects_disabled() && (offset >= (0xef0000>>1)) && (offset <= (0xef8000>>1)))
	{
		return m_boot[offset & 0x3fff];
	}

	// it's translation time
	uint8_t context = (fc & 4) ? ((m_context >> 8) & 7) : (m_context & 7);
	uint8_t pmeg = m_segmap[context][offset >> 14];
	uint32_t entry = (pmeg << 4) + ((offset >> 10) & 0xf);

	//  printf("sun2: Context = %d, pmeg = %d, offset >> 14 = %x, entry = %d, page = %d\n", context, pmeg, offset >> 14, entry, (offset >> 10) & 0xf);

	if (m_pagemap[entry] & PM_VALID)
	{
		m_pagemap[entry] |= PM_ACCESSED;

		// Sun2 implementations only use 12 bits from the page entry
		uint32_t tmp = (m_pagemap[entry] & 0xfff) << 10;
		tmp |= (offset & 0x3ff);

	//  if (!machine().side_effects_disabled())
	//      printf("sun2: Translated addr: %08x, type %d (page %d page entry %08x, orig virt %08x, FC %d)\n", tmp << 1, (m_pagemap[entry] >> 22) & 7, entry, m_pagemap[entry], offset<<1, fc);

		switch ((m_pagemap[entry] >> 22) & 7)
		{
			case 0: // type 0 space
				return m_bus->space(AS_DATA).read_word(tmp << 1, mem_mask);

			case 1: // type 1 space
				// EPROM space is special: the MMU has a trap door
				// where the original bits of the virtual address are
				// restored so that the entire 32K EPROM can be
				// accessed via a 2K single page view.  This isn't
				// obvious in the sun2 manual, but the sun3 manual
				// (sun3 has the same mechanism) explains it well.
				// the 2/50 ROM tests this specifically at $EF0DF0.
				// Multibus has EPROM at 0x000000
				{
					if (tmp <= (0x7ff>>1))
					{
						return m_boot[offset & 0x7fff];
					}
				}

				//printf("read device space @ %x\n", tmp<<1);
				return m_type1space->read16(tmp, mem_mask);

			case 2: // type 2 space
				return m_type2space->read16(tmp, mem_mask);

			case 3: // type 3 space
				return m_type3space->read16(tmp, mem_mask);
		}
	}
	else
	{
		if (!machine().side_effects_disabled()) printf("sun2: pagemap entry not valid!\n");
	}

	if (!machine().side_effects_disabled()) printf("sun2: Unmapped read @ %08x (FC %d, mask %04x, PC=%x, seg %x)\n", offset<<1, fc, mem_mask, m_cpu->pc(), offset>>15);

	return 0xffff;
}

void sun_mt1_device::mmu_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	tl_mmu_w(m_cpu->get_fc(), offset, data, mem_mask);
}

void sun_mt1_device::tl_mmu_w(uint8_t fc, offs_t offset, uint16_t data, uint16_t mem_mask)
{
	//printf("sun2: Write %04x (FC %d, mask %04x, PC=%x) to %08x\n", data, fc, mem_mask, m_cpu->pc(), offset<<1);

	if (fc == 3)
	{
		if (offset & 0x4)   // set for CPU space
		{
			switch (offset & 7)
			{
				case 4:
					//printf("sun2: Write? IDPROM @ %x\n", offset<<1);
					return;

				case 5:
					// XOR to match Table 2-1 in the 2/50 Field Service Manual
					printf("sun2: CPU LEDs to %02x (PC=%x) => ", (data & 0xff) ^ 0xff, m_cpu->pc());
					m_diagreg = data & 0xff;
					for (int i = 0; i < 8; i++)
					{
						if (m_diagreg & (1<<(7-i)))
						{
							printf("*");
						}
						else
						{
							printf("O");
						}
					}
					printf("\n");
					return;

				case 6:
					//printf("sun2: Write %04x to bus error not allowed\n", data);
					return;

				case 7:
					//printf("sun2: Write %04x to system enable\n", data);
					COMBINE_DATA(&m_sysenable);
					return;
			}
		}
		else                // clear for MMU space
		{
			int page;

			switch (offset & 3)
			{
				case 0: // page map
				case 1:
					page = m_segmap[m_context & 7][offset >> 14] << 4;
					page += ((offset >> 10) & 0xf);

					//printf("sun2: Write %04x to page map at %x (entry %d), ", data, offset<<1, page);
					if (offset & 1) // low-order 16 bits
					{
						m_pagemap[page] &= 0xffff0000;
						m_pagemap[page] |= data;
					}
					else
					{
						m_pagemap[page] &= 0x0000ffff;
						m_pagemap[page] |= (data<<16);
					}
					//printf("entry now %08x (adr %08x  PC=%x)\n", m_pagemap[page], (m_pagemap[page] & 0xfffff) << 11, m_cpu->pc());
					return;

				case 2: // segment map
					//printf("sun2: Write %02x to segment map at %x (entry %d, user ctx %d PC=%x)\n", data & 0xff, offset<<1, offset>>14, m_context & 7, m_cpu->pc());
					m_segmap[m_context & 7][offset >> 14] = data & 0xff;
					return;

				case 3: // context reg
					//printf("sun2: Write %04x to context\n", data);
					COMBINE_DATA(&m_context);
					return;
			}
		}
	}

	// it's translation time
	uint8_t context = (fc & 4) ? ((m_context >> 8) & 7) : (m_context & 7);
	uint8_t pmeg = m_segmap[context][offset >> 14];
	uint32_t entry = (pmeg << 4) + ((offset >> 10) & 0xf);

	if (m_pagemap[entry] & PM_VALID)
	{
		m_pagemap[entry] |= (PM_ACCESSED | PM_MODIFIED);

		// only 12 of the 20 bits in the page table entry are used on either Sun2 implementation
		uint32_t tmp = (m_pagemap[entry] & 0xfff) << 10;
		tmp |= (offset & 0x3ff);

		//if (!machine().side_effects_disabled()) printf("sun2: Translated addr: %08x, type %d (page entry %08x, orig virt %08x)\n", tmp << 1, (m_pagemap[entry] >> 22) & 7, m_pagemap[entry], offset<<1);

		switch ((m_pagemap[entry] >> 22) & 7)
		{
			case 0: // type 0
				m_bus->space(AS_DATA).write_word(tmp << 1, data, mem_mask);
				return;

			case 1: // type 1
				//printf("write device space @ %x\n", tmp<<1);
				m_type1space->write16(tmp, data, mem_mask);
				return;

			case 2: // type 2
				m_type2space->write16(tmp, data, mem_mask);
				return;

			case 3: // type 3
				m_type3space->write16(tmp, data, mem_mask);
				return;
		}
	}
	else
	{
		if (!machine().side_effects_disabled()) printf("sun2: pagemap entry not valid!\n");
	}

	printf("sun2: Unmapped write %04x (FC %d, mask %04x, PC=%x) to %08x\n", data, fc, mem_mask, m_cpu->pc(), offset<<1);
}

void sun_mt1_device::sun2_mem(address_map &map)
{
	map(0x00'0000, 0xff'ffff).rw(FUNC(sun_mt1_device::mmu_r), FUNC(sun_mt1_device::mmu_w));
}

// type 1 device space
void sun_mt1_device::mbustype1space_map(address_map &map)
{
	map(0x00'0000, 0x00'07ff).rom().region("boot", 0);    // uses MMU loophole to read 32k from a 2k window
	// 001000-0017ff: AM9518 encryption processor
	// 001800-001fff: Parallel port
	map(0x00'2000, 0x00'27ff).rw(m_scc, FUNC(z80scc_device::ab_dc_r), FUNC(z80scc_device::ab_dc_w)).umask16(0xff00);
	map(0x00'2800, 0x00'2803).mirror(0x7fc).rw(m_stc, FUNC(am9513_device::read16), FUNC(am9513_device::write16));
	map(0x00'3800, 0x00'383f).mirror(0x7c0).rw(m_rtc, FUNC(mm58167_device::read), FUNC(mm58167_device::write)).umask16(0xff00); // 12 wait states generated by PAL16R6 (U415)
}

// type 2 device space (Multibus memory space)
void sun_mt1_device::mbustype2space_map(address_map &map)
{
	// 00'0000-03'f800 dvma
	// 04'0000-07'f800 ethernet memory #1
	// 08'0000-08'3800 scsi #1
	// 08'4000-08'7800 scsi #2
	// 08'8000-08'b800 ethernet control #1
	// 08'c000-08'f800 ethernet control #2
	// 0a'0000-0a'f800 ethernet memory #2
	// 0c'0000-0d'f800 frame buffer
	//
	// 0e'8000-0f'7800 sun color
	map(0x00'0000, 0x0f'ffff).lrw16(
		[this](offs_t offset, u16 mem_mask)
		{
			auto [data, flags] = m_bus_mem.read_word_flags(offset << 1, mem_mask);

			//if (flags)
			//	bus_error(0x4000'0000 + (offset << 1), true, false);

			return data;
		}, "mem_r",
		[this](offs_t offset, u16 data, u16 mem_mask)
		{
			if (m_bus_mem.write_word_flags(offset << 1, data, mem_mask))
				; // bus_error(0x4000'0000 + (offset << 1), false, false);
		}, "mem_w");
}

// type 3 device space (Multibus I/O space)
void sun_mt1_device::mbustype3space_map(address_map &map)
{
	map(0x00'0000, 0x00'ffff).lrw16(
		[this](offs_t offset, u16 mem_mask) -> u16
		{
			auto [data, flags] = m_bus_pio.read_word_flags(offset << 1, mem_mask);

			//if (flags)
			//	bus_error(0x5000'0000 + (offset << 1), true, false);

			return data;
		}, "pio_r",
		[this](offs_t offset, u16 data, u16 mem_mask)
		{
			if (m_bus_pio.write_word_flags(offset << 1, data, mem_mask))
				; // bus_error(0x5000'0000 + (offset << 1), false, false);
		}, "pio_w");
}

void sun_mt1_device::device_start()
{
	m_bus->space(AS_PROGRAM).specific(m_bus_mem);
	m_bus->space(AS_IO).specific(m_bus_pio);
}

void sun_mt1_device::device_reset()
{
	m_diagreg = 0;
	m_sysenable = 0;
	m_context = 0;
	m_buserror = 0;
	memset(m_segmap, 0, sizeof(m_segmap));
	memset(m_pagemap, 0, sizeof(m_pagemap));
}

void sun_mt1_device::device_add_mconfig(machine_config &config)
{
	/* basic machine hardware */
	M68010(config, m_cpu, 39.3216_MHz_XTAL / 4);
	m_cpu->set_addrmap(AS_PROGRAM, &sun_mt1_device::sun2_mem);

	// MMU Type 1 device space
	ADDRESS_MAP_BANK(config, "type1").set_map(&sun_mt1_device::mbustype1space_map).set_options(ENDIANNESS_BIG, 16, 32, 0x1000000);

	// MMU Type 2 device space
	ADDRESS_MAP_BANK(config, "type2").set_map(&sun_mt1_device::mbustype2space_map).set_options(ENDIANNESS_BIG, 16, 32, 0x1000000);

	// MMU Type 3 device space
	ADDRESS_MAP_BANK(config, "type3").set_map(&sun_mt1_device::mbustype3space_map).set_options(ENDIANNESS_BIG, 16, 32, 0x1000000);

	input_merger_any_high_device &irq5(INPUT_MERGER_ANY_HIGH(config, "irq5"));
	irq5.output_handler().set_inputline(m_cpu, M68K_IRQ_5); // 74LS05 open collectors

	AM9513A(config, m_stc, 39.3216_MHz_XTAL / 8);
	m_stc->fout_cb().set(m_stc, FUNC(am9513_device::gate1_w));
	m_stc->out1_cb().set_inputline(m_cpu, M68K_IRQ_7);
	m_stc->out2_cb().set(irq5, FUNC(input_merger_device::in_w<0>));
	m_stc->out3_cb().set(irq5, FUNC(input_merger_device::in_w<1>));
	m_stc->out4_cb().set(irq5, FUNC(input_merger_device::in_w<2>));
	m_stc->out5_cb().set(irq5, FUNC(input_merger_device::in_w<3>));

	SCC8530(config, m_scc, 39.3216_MHz_XTAL / 8);
	m_scc->out_txda_callback().set(m_port[0], FUNC(rs232_port_device::write_txd));
	m_scc->out_txdb_callback().set(m_port[1], FUNC(rs232_port_device::write_txd));
	m_scc->out_int_callback().set_inputline(m_cpu, M68K_IRQ_6);

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

static INPUT_PORTS_START(sun_mt1)
INPUT_PORTS_END

ioport_constructor sun_mt1_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(sun_mt1);
}

ROM_START(sun_mt1) // ROMs are located on the '501-1007' CPU PCB at locations B11 and B10; J400 is set to 1-2 for 27128 EPROMs and 3-4 for 27256 EPROMs
	ROM_REGION16_BE(0x10000, "boot", ROMREGION_ERASEFF)
	// There is an undumped revision 1.1.2, which uses 27256 EPROMs
	ROM_SYSTEM_BIOS(0, "rev10f", "Bootrom Rev 1.0F")
	ROMX_LOAD("1.0f.b11", 0x0000, 0x8000, CRC(8fb0050a) SHA1(399cdb894b2a66d847d76d8a5d266906fb1d3430), ROM_SKIP(1) | ROM_BIOS(0)) // actual rom stickers had fallen off
	ROMX_LOAD("1.0f.b10", 0x0001, 0x8000, CRC(70de816d) SHA1(67e980497f463dbc529f64ec5f3e0046b3901b7e), ROM_SKIP(1) | ROM_BIOS(0)) // "
	ROM_SYSTEM_BIOS(1, "revr", "Bootrom Rev R")
	ROMX_LOAD("520-1102-03.b11", 0x0000, 0x4000, CRC(020bb0a8) SHA1(a7b60e89a40757975a5d345d57ea02781dea4f89), ROM_SKIP(1) | ROM_BIOS(1))
	ROMX_LOAD("520-1101-03.b10", 0x0001, 0x4000, CRC(b97c61f7) SHA1(9f08fe232cfc3da48539fa66673fc1f89a362b1e), ROM_SKIP(1) | ROM_BIOS(1))
	// There is an undumped revision Q, with roms:
	//ROM_SYSTEM_BIOS( 8, "revq", "Bootrom Rev Q")
	// ROMX_LOAD( "520-1104-02.b11", 0x0000, 0x4000, NO_DUMP, ROM_SKIP(1) | ROM_BIOS(8))
	// ROMX_LOAD( "520-1103-02.b10", 0x0001, 0x4000, NO_DUMP, ROM_SKIP(1) | ROM_BIOS(8))
	ROM_SYSTEM_BIOS( 2, "revn", "Bootrom Rev N") // SunOS 2.0 requires this bootrom version at a minimum; this version supports the sun-2 keyboard
	ROMX_LOAD("revn.b11", 0x0000, 0x4000, CRC(b1e70965) SHA1(726b3ed9323750a1ae238cf6dccaed6ff5981ad1), ROM_SKIP(1) | ROM_BIOS(2)) // actual rom stickers had fallen off
	ROMX_LOAD("revn.b10", 0x0001, 0x4000, CRC(95fd9242) SHA1(1eee2d291f4b18f6aafdde1a9521d88e454843b9), ROM_SKIP(1) | ROM_BIOS(2)) // "
	ROM_SYSTEM_BIOS( 3, "revm", "Bootrom Rev M") // SunOS 1.0 apparently requires this bootrom revision; this version might only support the sun-1 keyboard?
	ROMX_LOAD("sun2-revm-8.b11", 0x0000, 0x4000, CRC(98b8ae55) SHA1(55485f4d8fd1ebc218aa8527c8bb62752c34abf7), ROM_SKIP(1) | ROM_BIOS(3)) // handwritten label: "SUN2-RevM-8"
	ROMX_LOAD("sun2-revm-0.b10", 0x0001, 0x4000, CRC(5117f431) SHA1(fce85c11ada1614152dde35bb329350f6fb2ecd9), ROM_SKIP(1) | ROM_BIOS(3)) // handwritten label: "SUN2-RevM-0"

	ROM_REGION(0x20, "id", ROMREGION_ERASEFF)
	ROM_LOAD("sun2120-idprom.bin", 0x000000, 0x000020, CRC(eec8cd1d) SHA1(6a78dc0ea6f9cc7687cffea754d65864fb751ebf))
ROM_END

const tiny_rom_entry *sun_mt1_device::device_rom_region() const
{
	return ROM_NAME(sun_mt1);
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(SUN_MT1, device_multibus_interface, sun_mt1_device, "sun_mt1", "Sun Microsystems Sun-2 Machine Type 1 CPU")
