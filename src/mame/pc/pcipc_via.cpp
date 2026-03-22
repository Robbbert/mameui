// license:BSD-3-Clause
// copyright-holders:Angelo Salese
/*
 * Sandbox for VIA based PCs
 *
 * Using Lucky Star 5AMVP3 given we already have a mostly compatible Super I/O (ITE 8661F)
 *
 * Notes:
 * - need to map_first the IDE for make it recognize both that and floppy disks;
 * - win98se: none of the via4in1 drivers seems to actually work with this BIOS. It will return
 *   "registry error, please reboot", with CD-ROM drive becoming non-functional afterwards.
 *   winme acts mostly the same, except that it manages to install with v4.17
 *
 * TODO:
 * - win98se/win98me: resource conflict between ACPI BIOS and AGP card(s), PCI cards works fine.
 *   Bridge memory/io bases not passed properly?
 * - win98se: PS/2 keyboard becomes unresponsive after a while, caused by keyboard irq stuck
 *   (workaround: open debugger window at I/O $60);
 * - win98se: ACPI has issues on power off and reboot (workaround: use restart in MSDOS mode);
 * - freedos13: APMDOS hangs system with JEMMEX preloaded, works when issued standalone;
 *
 * TODO (zidabx98):
 * - expects to reset after saving to CMOS from GPIO $5042 bit 3 low;
 * - BIOSes 108 and 106 seems unable to save to CMOS, worked around to use 105 by default
 *   (don't want long memory test);
 *
 */

#include "emu.h"

#include "bus/isa/isa_cards.h"
#include "bus/pci/pci_slot.h"
#include "bus/rs232/hlemouse.h"
#include "bus/rs232/null_modem.h"
#include "bus/rs232/rs232.h"
#include "bus/rs232/sun_kbd.h"
#include "bus/rs232/terminal.h"
#include "cpu/i386/i386.h"
#include "machine/it8671f.h"
#include "machine/pci.h"
#include "machine/vt82c586b_acpi.h"
#include "machine/vt82c586b_ide.h"
#include "machine/vt82c586b_isa.h"
#include "machine/vt82c586b_usb.h"
#include "machine/vt82c598mvp.h"

#include "softlist.h"
#include "softlist_dev.h"

namespace {

class mvp3_state : public driver_device
{
public:
	mvp3_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
	{ }

	void mvp3(machine_config &config) ATTR_COLD;
	void apollopro(machine_config &config) ATTR_COLD;

protected:
	void x86_softlists(machine_config &config);

private:
	void main_io(address_map &map) ATTR_COLD;
	void main_map(address_map &map) ATTR_COLD;

	static void ite_superio_config(device_t *device);
};

// NOTE: something between these two will corrupt the Energy Star logo if mapped low
void mvp3_state::main_map(address_map &map)
{
	map.unmap_value_high();
}

void mvp3_state::main_io(address_map &map)
{
	map.unmap_value_high();
}

void mvp3_state::x86_softlists(machine_config &config)
{
	SOFTWARE_LIST(config, "pc_disk_list").set_original("ibm5150");
	SOFTWARE_LIST(config, "at_disk_list").set_original("ibm5170");
	SOFTWARE_LIST(config, "at_cdrom_list").set_original("ibm5170_cdrom");
	SOFTWARE_LIST(config, "win_cdrom_list").set_original("generic_cdrom").set_filter("ibmpc");
	SOFTWARE_LIST(config, "at_hdd_list").set_original("ibm5170_hdd");
	SOFTWARE_LIST(config, "midi_disk_list").set_compatible("midi_flop");
	SOFTWARE_LIST(config, "photocd_list").set_compatible("photo_cd");
}

static void isa_com(device_slot_interface &device)
{
	device.option_add("microsoft_mouse", MSFT_HLE_SERIAL_MOUSE);
	device.option_add("logitech_mouse", LOGITECH_HLE_SERIAL_MOUSE);
	device.option_add("wheel_mouse", WHEEL_HLE_SERIAL_MOUSE);
	device.option_add("msystems_mouse", MSYSTEMS_HLE_SERIAL_MOUSE);
	device.option_add("rotatable_mouse", ROTATABLE_HLE_SERIAL_MOUSE);
	device.option_add("terminal", SERIAL_TERMINAL);
	device.option_add("null_modem", NULL_MODEM);
	device.option_add("sun_kbd", SUN_KBD_ADAPTOR);
}

static void isa_internal_devices(device_slot_interface &device)
{
	device.option_add("it8661f", IT8661F);
}

void mvp3_state::ite_superio_config(device_t *device)
{
	it8661f_device &ite = *downcast<it8661f_device *>(device);
	ite.txd1().set(":serport0", FUNC(rs232_port_device::write_txd));
	ite.ndtr1().set(":serport0", FUNC(rs232_port_device::write_dtr));
	ite.nrts1().set(":serport0", FUNC(rs232_port_device::write_rts));
	ite.txd2().set(":serport1", FUNC(rs232_port_device::write_txd));
	ite.ndtr2().set(":serport1", FUNC(rs232_port_device::write_dtr));
	ite.nrts2().set(":serport1", FUNC(rs232_port_device::write_rts));
}

void mvp3_state::mvp3(machine_config &config)
{
	// Socket 7 / PGA321
	pentium_mmx_device &maincpu(PENTIUM_MMX(config, "maincpu", 66'000'000));
	maincpu.set_addrmap(AS_PROGRAM, &mvp3_state::main_map);
	maincpu.set_addrmap(AS_IO, &mvp3_state::main_io);
	maincpu.set_irq_acknowledge_callback("pci:07.0:pic0", FUNC(pic8259_device::inta_cb));
	maincpu.smiact().set("pci:00.0", FUNC(vt82c598mvp_host_device::smi_act_w));

	// TODO: config space not known
	PCI_ROOT(config, "pci", 0);
	// Max 768 MB
	VT82C598MVP_HOST(config, "pci:00.0", 0, "maincpu", 256*1024*1024);
	VT82C598MVP_BRIDGE(config, "pci:01.0", 0 );

	vt82c586b_isa_device &isa(VT82C586B_ISA(config, "pci:07.0", XTAL(33'000'000), "maincpu"));
	isa.boot_state_hook().set([](u8 data) { /* printf("%02x\n", data); */ });
	isa.a20m().set_inputline("maincpu", INPUT_LINE_A20);
	isa.cpureset().set_inputline("maincpu", INPUT_LINE_RESET);
	isa.pcirst().set([this] (int state) {
		if (state)
			machine().schedule_soft_reset();
	});
	//isa.smi().set_inputline("maincpu", INPUT_LINE_SMI);

	vt82c586b_ide_device &ide(VT82C586B_IDE(config, "pci:07.1", 0, "maincpu"));
	ide.irq_pri().set("pci:07.0", FUNC(vt82c586b_isa_device::pc_ide0_w));
	ide.irq_sec().set("pci:07.0", FUNC(vt82c586b_isa_device::pc_ide1_w));

	VT82C586B_USB (config, "pci:07.2", 0);

	vt82c586b_acpi_device &acpi_pci(VT82C586B_ACPI(config, "pci:07.3", 0));
	acpi_pci.sci_pin_cb().set("pci:07.0", FUNC(vt82c586b_isa_device::acpi_pin_config_w));
	acpi_pipc_device &acpi_dev(ACPI_PIPC(config, "pci:07.3:acpi"));
	acpi_dev.smi().set_inputline("maincpu", INPUT_LINE_SMI);
	acpi_dev.sci().set("pci:07.0", FUNC(vt82c586b_isa_device::pc_acpi_w));

	PCI_SLOT(config, "pci:01.0:0", agp_cards, 0, 0, 1, 2, 3, nullptr);

	// 8~11 is trusted, otherwise BIOS won't map intr_line(s) properly
	PCI_SLOT(config, "pci:1", pci_cards, 8,  0, 1, 2, 3, "sis6326_pci");
	PCI_SLOT(config, "pci:2", pci_cards, 9,  1, 2, 3, 0, "4dwavedx");
	PCI_SLOT(config, "pci:3", pci_cards, 10, 2, 3, 0, 1, nullptr);
	PCI_SLOT(config, "pci:4", pci_cards, 11, 3, 0, 1, 2, nullptr);

	ISA16_SLOT(config, "board4", 0, "pci:07.0:isabus", isa_internal_devices, "it8661f", true).set_option_machine_config("it8661f", ite_superio_config);
	ISA16_SLOT(config, "isa1", 0, "pci:07.0:isabus", pc_isa16_cards, nullptr, false);
	ISA16_SLOT(config, "isa2", 0, "pci:07.0:isabus", pc_isa16_cards, nullptr, false);
	ISA16_SLOT(config, "isa3", 0, "pci:07.0:isabus", pc_isa16_cards, nullptr, false);

	rs232_port_device &serport0(RS232_PORT(config, "serport0", isa_com, "logitech_mouse"));
	serport0.rxd_handler().set("board4:it8661f", FUNC(it8661f_device::rxd1_w));
	serport0.dcd_handler().set("board4:it8661f", FUNC(it8661f_device::ndcd1_w));
	serport0.dsr_handler().set("board4:it8661f", FUNC(it8661f_device::ndsr1_w));
	serport0.ri_handler().set("board4:it8661f", FUNC(it8661f_device::nri1_w));
	serport0.cts_handler().set("board4:it8661f", FUNC(it8661f_device::ncts1_w));

	rs232_port_device &serport1(RS232_PORT(config, "serport1", isa_com, nullptr));
	serport1.rxd_handler().set("board4:it8661f", FUNC(it8661f_device::rxd2_w));
	serport1.dcd_handler().set("board4:it8661f", FUNC(it8661f_device::ndcd2_w));
	serport1.dsr_handler().set("board4:it8661f", FUNC(it8661f_device::ndsr2_w));
	serport1.ri_handler().set("board4:it8661f", FUNC(it8661f_device::nri2_w));
	serport1.cts_handler().set("board4:it8661f", FUNC(it8661f_device::ncts2_w));

	x86_softlists(config);
}

void mvp3_state::apollopro(machine_config &config)
{
	// Slot 1 (Slot-242)
	pentium2_device &maincpu(PENTIUM2(config, "maincpu", 100'000'000));
	maincpu.set_addrmap(AS_PROGRAM, &mvp3_state::main_map);
	maincpu.set_addrmap(AS_IO, &mvp3_state::main_io);
	maincpu.set_irq_acknowledge_callback("pci:07.0:pic0", FUNC(pic8259_device::inta_cb));
	maincpu.smiact().set("pci:00.0", FUNC(vt82c598mvp_host_device::smi_act_w));

	// TODO: config space not known
	PCI_ROOT(config, "pci", 0);
	// Max 768 MB
	VT82C691_HOST(config, "pci:00.0", 0, "maincpu", 256*1024*1024);
	VT82C691_BRIDGE(config, "pci:01.0", 0 );

	vt82c586b_isa_device &isa(VT82C586B_ISA(config, "pci:07.0", XTAL(33'000'000), "maincpu"));
	isa.boot_state_hook().set([](u8 data) { /* printf("%02x\n", data); */ });
	isa.a20m().set_inputline("maincpu", INPUT_LINE_A20);
	isa.cpureset().set_inputline("maincpu", INPUT_LINE_RESET);
	isa.pcirst().set([this] (int state) {
		if (state)
			machine().schedule_soft_reset();
	});
	//isa.smi().set_inputline("maincpu", INPUT_LINE_SMI);

	vt82c586b_ide_device &ide(VT82C586B_IDE(config, "pci:07.1", 0, "maincpu"));
	ide.irq_pri().set("pci:07.0", FUNC(vt82c586b_isa_device::pc_ide0_w));
	ide.irq_sec().set("pci:07.0", FUNC(vt82c586b_isa_device::pc_ide1_w));

	VT82C586B_USB (config, "pci:07.2", 0);

	vt82c586b_acpi_device &acpi_pci(VT82C586B_ACPI(config, "pci:07.3", 0));
	acpi_pci.sci_pin_cb().set("pci:07.0", FUNC(vt82c586b_isa_device::acpi_pin_config_w));
	acpi_pipc_device &acpi_dev(ACPI_PIPC(config, "pci:07.3:acpi"));
	acpi_dev.smi().set_inputline("maincpu", INPUT_LINE_SMI);
	acpi_dev.sci().set("pci:07.0", FUNC(vt82c586b_isa_device::pc_acpi_w));

	PCI_SLOT(config, "pci:01.0:0", agp_cards, 0, 0, 1, 2, 3, nullptr);

	// 8~11 is trusted, otherwise BIOS won't map intr_line(s) properly
	PCI_SLOT(config, "pci:1", pci_cards, 8,  0, 1, 2, 3, "sis6326_pci");
	PCI_SLOT(config, "pci:2", pci_cards, 9,  1, 2, 3, 0, "4dwavedx");
	PCI_SLOT(config, "pci:3", pci_cards, 10, 2, 3, 0, 1, nullptr);
	PCI_SLOT(config, "pci:4", pci_cards, 11, 3, 0, 1, 2, nullptr);

	ISA16_SLOT(config, "board4", 0, "pci:07.0:isabus", isa_internal_devices, "it8661f", true).set_option_machine_config("it8661f", ite_superio_config);
	ISA16_SLOT(config, "isa1", 0, "pci:07.0:isabus", pc_isa16_cards, nullptr, false);
	ISA16_SLOT(config, "isa2", 0, "pci:07.0:isabus", pc_isa16_cards, nullptr, false);
	ISA16_SLOT(config, "isa3", 0, "pci:07.0:isabus", pc_isa16_cards, nullptr, false);

	rs232_port_device &serport0(RS232_PORT(config, "serport0", isa_com, "logitech_mouse"));
	serport0.rxd_handler().set("board4:it8661f", FUNC(it8661f_device::rxd1_w));
	serport0.dcd_handler().set("board4:it8661f", FUNC(it8661f_device::ndcd1_w));
	serport0.dsr_handler().set("board4:it8661f", FUNC(it8661f_device::ndsr1_w));
	serport0.ri_handler().set("board4:it8661f", FUNC(it8661f_device::nri1_w));
	serport0.cts_handler().set("board4:it8661f", FUNC(it8661f_device::ncts1_w));

	rs232_port_device &serport1(RS232_PORT(config, "serport1", isa_com, nullptr));
	serport1.rxd_handler().set("board4:it8661f", FUNC(it8661f_device::rxd2_w));
	serport1.dcd_handler().set("board4:it8661f", FUNC(it8661f_device::ndcd2_w));
	serport1.dsr_handler().set("board4:it8661f", FUNC(it8661f_device::ndsr2_w));
	serport1.ri_handler().set("board4:it8661f", FUNC(it8661f_device::nri2_w));
	serport1.cts_handler().set("board4:it8661f", FUNC(it8661f_device::ncts2_w));

	x86_softlists(config);
}


ROM_START( ls5amvp3 )
	ROM_REGION32_LE(0x20000, "pci:07.0", 0)
	ROM_SYSTEM_BIOS(0, "lec90", "LEC9-00")
	ROMX_LOAD( "lec9-0.bin", 0x00000, 0x20000, CRC(7e3a18e5) SHA1(e178eb1ff2a1257f7daf9a32ba4c05739a445568), ROM_BIOS(0))
ROM_END

ROM_START( zidabx98 )
	ROM_REGION32_LE(0x20000, "pci:07.0", 0)
	ROM_DEFAULT_BIOS("105")
	ROM_SYSTEM_BIOS(0, "108", "v1.08 (07/29/1999)")
	ROMX_LOAD( "bx98108e.bin", 0x00000, 0x20000, CRC(c2dfc7ba) SHA1(07cd8c69a396d37ae072770a6b642e4845e3cb6f), ROM_BIOS(0))
	ROM_SYSTEM_BIOS(1, "106", "v1.06 (03/12/1999)")
	ROMX_LOAD( "bx98106e.bin", 0x00000, 0x20000, CRC(dc1b2494) SHA1(4cc2f4e777056fb56caebf9cffd88666ba140ef8), ROM_BIOS(1))
	ROM_SYSTEM_BIOS(2, "105", "v1.05 (01/15/1999)")
	ROMX_LOAD( "bx98105e.bin", 0x00000, 0x20000, CRC(4bc323ad) SHA1(ea9ac16bc6fc7c8e516c470f49afeae51c99d3b8), ROM_BIOS(2))
	ROM_SYSTEM_BIOS(3, "103", "v1.03 (10/27/1998)")
	ROMX_LOAD( "bx98103e.bin", 0x00000, 0x20000, CRC(f9879af0) SHA1(e01390cbafbbf64521c6050ab87b8153521f1bf9), ROM_BIOS(3))
	ROM_SYSTEM_BIOS(4, "102", "v1.02 (09/09/1998)")
	ROMX_LOAD( "bx98102e.bin", 0x00000, 0x20000, CRC(7e8d2a1a) SHA1(93f9bf2315b236bc2e1381f2eeb8b09192ae31e4), ROM_BIOS(4))
ROM_END


} // anonymous namespace


// MVP3 chipset
COMP(1998, ls5amvp3,    0,     0, mvp3,        0, mvp3_state, empty_init, "Lucky Star", "5AMVP3 (VIA MVP3 chipset)", MACHINE_NOT_WORKING )

// Apollo Pro chipset + 586B
COMP(1998, zidabx98,    0,     0, apollopro,   0, mvp3_state, empty_init, "Zida",       "BX98 (VIA Apollo Pro chipset)", MACHINE_NOT_WORKING ) // a.k.a. Zida [Tomato] BX98-AT / Kobian/Mercury KOB BX98
