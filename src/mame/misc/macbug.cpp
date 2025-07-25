// license:GPL-2.0+
// copyright-holders:Robbbert
/***************************************************************************

MACBUG 1802

Written for MAMEUI 0.276 on 2025-03-12

Next to no info available. No schematics, no manuals, all guesswork.
There's a CDP1851 used, but that device isn't emulated. Fortunately it's
just a simple I/O port, its advanced features are not used.

With a crystal of 2MHz, the cassette is perfect Kansas City format at 300
baud, readable by the Super-80 D-command.

Cassette notes:
- the dump omits the last byte when saving and loading, need to specify one
  more byte
- there's no checksum checking.
- file names are 5 characters.

Commands:
 D nnnn - hex dump (use space for next line, or other key to exit)
 F - fill memory
 L - load
 M nnnn - modify memory
 Q - reboot
 R nnnn - Go to
 S - save

***************************************************************************/

#include "emu.h"

#include "cpu/cosmac/cosmac.h"
#include "sound/spkrdev.h"
#include "video/mc6847.h"
#include "imagedev/cassette.h"
#include "speaker.h"


namespace {

/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

class macbug_state : public driver_device
{
public:
	macbug_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_crtc(*this, "crtc")
		, m_io_keyboard(*this, "X%d", 0U)
		, m_speaker(*this, "speaker")
		, m_vram(*this, "vram")
		, m_cassette(*this, "cassette")
	{
	}

	void macbug(machine_config &config);

private:
	required_device<cosmac_device> m_maincpu;
	required_device<mc6847_base_device> m_crtc;
	required_ioport_array<16> m_io_keyboard;
	required_device<speaker_sound_device> m_speaker;
	required_shared_ptr<uint8_t> m_vram;
	required_device<cassette_image_device> m_cassette;

	virtual void machine_start() override ATTR_COLD;
	void keyboard_w(offs_t, uint8_t);
	uint8_t keyboard_r();
	uint8_t ef1_r();
	uint8_t ef2_r();
	void q_w(uint8_t);
	uint8_t videoram_r(offs_t offset);
	uint8_t m_kbdrow = 0U;
	void mem_map(address_map &map) ATTR_COLD;
	void io_map(address_map &map) ATTR_COLD;
};


/***************************************************************************
    INPUTS
***************************************************************************/

void macbug_state::keyboard_w(offs_t offset, uint8_t data)
{
	m_kbdrow = BIT(data, 4,4);
}

uint8_t macbug_state::keyboard_r()
{
	return m_io_keyboard[m_kbdrow]->read();
}

/***************************************************************************
    CASSETTE
***************************************************************************/

uint8_t macbug_state::ef1_r()
{
	return (m_cassette->input() > 0.05) ? 0: 1;
}

uint8_t macbug_state::ef2_r()
{
	return ((m_cassette->get_state() & CASSETTE_MASK_UISTATE)!= CASSETTE_STOPPED) ? 1 : 0;
}

void macbug_state::q_w(uint8_t data)
{
	m_cassette->output(data ? -1.0 : +1.0);
}

/***************************************************************************
    VIDEO EMULATION
***************************************************************************/

uint8_t macbug_state::videoram_r(offs_t offset)
{
	if (offset == ~0) return 0xff;

	m_crtc->inv_w(BIT(m_vram[offset], 6));

	return m_vram[offset] & 0x3f;
}


/***************************************************************************
    ADDRESS MAPS
***************************************************************************/

void macbug_state::mem_map(address_map &map)
{
	map.unmap_value_high();
	map(0x0000, 0x07ff).rom();
	map(0x0800, 0xbfff).ram();
	map(0xc000, 0xc1ff).ram().share("vram");
	map(0xd8fc, 0xd8fc).nopw(); // writes 1 at boot
	map(0xe001, 0xe001).nopw(); // programming codes for CDP1851
	map(0xe002, 0xe002).w(FUNC(macbug_state::keyboard_w));
	map(0xe003, 0xe003).r(FUNC(macbug_state::keyboard_r));
	map(0xf000, 0xffff).ram();
}

void macbug_state::io_map(address_map &map)
{
	map(0x0001, 0x0001).nopw(); // ?cassette info
	map(0x0004, 0x0004).nopw(); // ?cassette info
}


/***************************************************************************
    INPUT PORTS
***************************************************************************/

static INPUT_PORTS_START(macbug)
	// Doesn't seem to be an Enter key or a cursor-right key.
	// The keyboard is a surplus word-processing unit, but since we only need alpha and
	// hex keys, there's no need to define the remainder.
	// There's code in the ROM to process C1 to C6 and CD, but they aren't really needed.
	// Code CD is newline (not enter), but it isn't in the keyboard matrix (0x780-0x7FF).
	PORT_START("X0")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED) // '.'
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_M) PORT_CHAR('M')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_B) PORT_CHAR('B')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_C) PORT_CHAR('C')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Z) PORT_CHAR('Z')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_UNUSED) // '>'
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED) // ':'

	PORT_START("X1")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED) // C3 repeating space
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED) // 99
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_UNUSED) // 9E
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_UNUSED) // 9B
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_UNUSED) // 9A
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED) // 61

	PORT_START("X2")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_P) PORT_CHAR('F')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_I) PORT_CHAR('I')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Y) PORT_CHAR('Y')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_R) PORT_CHAR('R')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_W) PORT_CHAR('W')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_UNUSED) // CF
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED) // '='

	PORT_START("X3")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED) // C1 CURSOR UP
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_UNUSED) // C2 CURSOR DOWN
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_UNUSED) // C4 CURSOR HOME
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("X4")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_SPACE) PORT_CHAR(' ')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED) // BACKSLASH
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_L) PORT_CHAR('L')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_J) PORT_CHAR('J')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_G) PORT_CHAR('G')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_D) PORT_CHAR('D')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_A) PORT_CHAR('A')
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("X5")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED) // 93
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_UNUSED) // 95
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_UNUSED) // 94
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("X6")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS) PORT_CHAR('-')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_9) PORT_CHAR('9')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_7) PORT_CHAR('7')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_5) PORT_CHAR('5')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_3) PORT_CHAR('3')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_1) PORT_CHAR('1')
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("X7")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH) PORT_CHAR('/')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_UNUSED) // ','
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_N) PORT_CHAR('N')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_V) PORT_CHAR('V')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_X) PORT_CHAR('X')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_UNUSED) // AA
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED) // AA

	PORT_START("X8")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED) // AA
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED) // ';'
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_K) PORT_CHAR('K')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_H) PORT_CHAR('H')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_F) PORT_CHAR('F')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_S) PORT_CHAR('S')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_UNUSED) // 00
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED) // '<'

	PORT_START("X9")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED) // '#'

	PORT_START("X10")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED) // '+'
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_0) PORT_CHAR('0')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_8) PORT_CHAR('8')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_6) PORT_CHAR('6')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_4) PORT_CHAR('4')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_2) PORT_CHAR('2')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_UNUSED) // C5 CLEAR SCREEN AND CURSOR HOME
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED) // CF

	PORT_START("X11")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED) // C6 CURSOR LEFT
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("X12")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_O) PORT_CHAR('O')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_U) PORT_CHAR('U')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_T) PORT_CHAR('T')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_E) PORT_CHAR('E')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Q) PORT_CHAR('Q')
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("X13")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("X14")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("X15")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED) // 9F
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED) // EF
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_UNUSED) // 62
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED)
INPUT_PORTS_END


/***************************************************************************
    MACHINE DRIVERS
***************************************************************************/

void macbug_state::machine_start()
{
	save_item(NAME(m_kbdrow));
}

void macbug_state::macbug(machine_config &config)
{
	// basic machine hardware
	CDP1802(config, m_maincpu, 2_MHz_XTAL);
	m_maincpu->set_addrmap(AS_PROGRAM, &macbug_state::mem_map);
	m_maincpu->set_addrmap(AS_IO, &macbug_state::io_map);
	m_maincpu->wait_cb().set_constant(1);
	m_maincpu->clear_cb().set_constant(1);
	m_maincpu->ef1_cb().set(FUNC(macbug_state::ef1_r));
	m_maincpu->ef2_cb().set(FUNC(macbug_state::ef2_r));
	m_maincpu->q_cb().set(FUNC(macbug_state::q_w));

	// video hardware
	MC6847(config, m_crtc, XTAL(3'579'545)); // unknown if PAL or NTSC
	m_crtc->set_screen("screen");
	m_crtc->input_callback().set(FUNC(macbug_state::videoram_r));
	// not using any graphic modes

	SCREEN(config, "screen", SCREEN_TYPE_RASTER);

	// sound hardware
	SPEAKER(config, "mono").front_center();
	SPEAKER_SOUND(config, m_speaker).add_route(ALL_OUTPUTS, "mono", 0.50);

	CASSETTE(config, m_cassette);
	m_cassette->set_default_state(CASSETTE_STOPPED | CASSETTE_MOTOR_ENABLED | CASSETTE_SPEAKER_ENABLED);
	m_cassette->add_route(ALL_OUTPUTS, "mono", 0.05);
}

/***************************************************************************
    ROM DEFINITIONS
***************************************************************************/

ROM_START( macbug )
	ROM_REGION(0x800, "maincpu", 0)
	ROM_LOAD("macbug.bin", 0x0000, 0x800, CRC(ce1efffe) SHA1(89e81f58aabe645dfe6613092c973784487327bb) )
ROM_END

} // anonymous namespace


/***************************************************************************
    GAME DRIVERS
***************************************************************************/

//    YEAR  NAME       PARENT    COMPAT  MACHINE    INPUT   CLASS            INIT        COMPANY                   FULLNAME                          FLAGS
COMP( 1982, macbug,    0,        0,      macbug,    macbug, macbug_state,    empty_init, "unknown",       "Macbug-1802", MACHINE_SUPPORTS_SAVE ) // year best guess
