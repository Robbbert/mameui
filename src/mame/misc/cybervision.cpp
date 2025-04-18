// license:GPL-2.0+
// copyright-holders:Robbbert
/***************************************************************************

CYBERVISION 2001

Written for MAMEUI 0.277 on 2025-04-18

Next to no info available. No schematics, no manuals, all guesswork.
There's 4x CDP1852 used.

Situation:
- Machine boots and displays the initial screen, and is waiting for cassette
  input.
- Video is done.

ToDo:
- CASSETTE
-- Reading only; there's no facility to save
-- Reading is done entirely by hardware. EF2 goes high to indicate a byte
   is waiting; INP 2 to read it and reset EF2.
-- Cassette motor is controlled via the Q output.

- KEYBOARD
-- Each of the 2 players has their own 40-key miniature membrane controller.
-- It's assumed these are wired in parallel.
-- The scanning is done by hardware. When a key is pressed, EF3 goes high.
-- Read INP 1 to get the matrix code and reset EF3.
-- A lookup table exists to convert the matrix code to a character.
-- This table is not used by the bios; only by programs.

- ROM BANKING
-- The ROM resides at 8000, while 0000-0FFF is for RAM. Because the CPU
   starts at 0000, the ROM and RAM need to be switched after booting.
-- There's no I/O port for this, but the Q output could be pressed into
   service as a one-way switch.

- SPEAKER
-- Unknown if the unit has a speaker, but since the tape can have audio,
   there must be a way of making sound. There's nothing in the bios.

***************************************************************************/

#include "emu.h"

#include "cpu/cosmac/cosmac.h"
#include "sound/spkrdev.h"
#include "imagedev/cassette.h"
#include "emupal.h"
#include "speaker.h"
#include "screen.h"


namespace {

/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

class cyb2001_state : public driver_device
{
public:
	cyb2001_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_io_keyboard(*this, "X%d", 0U)
		, m_speaker(*this, "speaker")
		, m_screen(*this, "screen")
		, m_vram(*this, "vram")
		, m_palette(*this, "palette")
		, m_cassette(*this, "cassette")
	{
	}

	void cyb2001(machine_config &config);

private:
	required_device<cosmac_device> m_maincpu;
	required_ioport_array<5> m_io_keyboard;
	required_device<speaker_sound_device> m_speaker;
	required_device<screen_device> m_screen;
	required_shared_ptr<uint8_t> m_vram;
	required_device<palette_device> m_palette;
	required_device<cassette_image_device> m_cassette;

	virtual void machine_start() override ATTR_COLD;
	void cyb_palette(palette_device &palette) const;
	uint32_t screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
//	void keyboard_w(offs_t, uint8_t);
//	uint8_t keyboard_r();
	uint8_t ef2_r();
	uint8_t ef3_r();
	void q_w(uint8_t);
	uint8_t m_kbdrow = 0U;
	void mem_map(address_map &map) ATTR_COLD;
	void io_map(address_map &map) ATTR_COLD;
};


/***************************************************************************
    INPUTS
***************************************************************************/

//void cyb2001_state::keyboard_w(offs_t offset, uint8_t data)
//{
//	m_kbdrow = BIT(data, 4,4);
//}

//uint8_t cyb2001_state::keyboard_r()
//{
//	return m_io_keyboard[m_kbdrow]->read();
//}

/***************************************************************************
    CASSETTE
***************************************************************************/

uint8_t cyb2001_state::ef2_r()
{
	return 0;   // 1 = byte ready on INP 2
//	return (m_cassette->input() > 0.05) ? 0: 1;
}

uint8_t cyb2001_state::ef3_r()
{
	return 0;   // 1 = a key was pressed, read INP 1 to get matrix code
//	return ((m_cassette->get_state() & CASSETTE_MASK_UISTATE)!= CASSETTE_STOPPED) ? 1 : 0;
}

void cyb2001_state::q_w(uint8_t data)
{
	// 1 = turn on cassette motor
}

/***************************************************************************
    VIDEO EMULATION
***************************************************************************/

//uint8_t cyb2001_state::videoram_r(offs_t offset)
//{
//	return m_vram[offset];
//}


/***************************************************************************
    ADDRESS MAPS
***************************************************************************/

void cyb2001_state::mem_map(address_map &map)
{
	map.unmap_value_high();
	map(0x0000, 0x0002).rom();   // need to bank this properly
	map(0x0003, 0x07ff).ram();   // main ram
	map(0x0800, 0x0fff).ram().share("vram");
	map(0x8000, 0x83ff).rom().region("maincpu",0);
}

void cyb2001_state::io_map(address_map &map)
{
	map(0x0001, 0x0001).nopr();   // Reading the keyboard matrix (returns 0x00-0x27, or 0x28 if no key pressed)
	map(0x0002, 0x0002).nopr();   // Reading a byte from cassette
}


/***************************************************************************
    INPUT PORTS
***************************************************************************/

static INPUT_PORTS_START(cyb2001)
	// There's 2x 40-button handheld tiny keyboards, one per player. It seems as
	//  though both units are in parallel.
	PORT_START("X0")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_0) PORT_CHAR('0')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_1) PORT_CHAR('1')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_2) PORT_CHAR('2')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_3) PORT_CHAR('3')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_4) PORT_CHAR('4')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_5) PORT_CHAR('5')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_6) PORT_CHAR('6')
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_7) PORT_CHAR('7')

	PORT_START("X1")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_8) PORT_CHAR('8')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_9) PORT_CHAR('9')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_A) PORT_CHAR('A')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_B) PORT_CHAR('B')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_C) PORT_CHAR('C')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_D) PORT_CHAR('D')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_E) PORT_CHAR('E')
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_F) PORT_CHAR('F')

	PORT_START("X2")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_G) PORT_CHAR('G')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_H) PORT_CHAR('H')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_I) PORT_CHAR('I')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_J) PORT_CHAR('J')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_K) PORT_CHAR('K')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_L) PORT_CHAR('L')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_M) PORT_CHAR('M')
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_N) PORT_CHAR('N')

	PORT_START("X3")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_O) PORT_CHAR('O')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_P) PORT_CHAR('P')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Q) PORT_CHAR('Q')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_R) PORT_CHAR('R')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_S) PORT_CHAR('S')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_T) PORT_CHAR('T')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_U) PORT_CHAR('U')
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_V) PORT_CHAR('V')

	PORT_START("X4")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_W) PORT_CHAR('W')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_X) PORT_CHAR('X')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Y) PORT_CHAR('Y')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Z) PORT_CHAR('Z')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COMMA) // ON
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_ENTER) PORT_CHAR(13) // ENT
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_STOP) // OFF
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH) // CLR
INPUT_PORTS_END


void cyb2001_state::cyb_palette(palette_device &palette) const
{
	// RGB
	palette.set_pen_color(0, rgb_t(0xff, 0xff, 0xff));   // White
	palette.set_pen_color(1, rgb_t(0xff, 0xff, 0x00));   // Yellow
	palette.set_pen_color(2, rgb_t(0x00, 0xff, 0x00));   // Bright Green
	palette.set_pen_color(3, rgb_t(0xff, 0x00, 0x00));   // Red
	palette.set_pen_color(4, rgb_t(0x00, 0x00, 0x00));   // Black
}

uint32_t cyb2001_state::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	uint8_t fg, bg = 4;
	uint16_t sy = 0;
	uint16_t ma = 0;
	for (uint8_t y = 0; y < 32; y++)
	{
		for (uint8_t ra = 0; ra < 3; ra++)
		{
			uint16_t *p = &bitmap.pix(sy++);

			for (uint16_t x = 0; x < 64; x++)
			{
				// get pattern of pixels for that character scanline
				uint8_t gfx = m_vram[ma + x];
				fg = BIT(gfx,6,2);
				uint8_t pos = 4 - (ra << 1);

				// Display 2 dots
				*p++ = BIT(gfx, pos++) ? fg : bg;
				*p++ = BIT(gfx, pos) ? fg : bg;
			}
		}
		ma+=64;
	}
	return 0;
}

/***************************************************************************
    MACHINE DRIVERS
***************************************************************************/

void cyb2001_state::machine_start()
{
	save_item(NAME(m_kbdrow));
}

void cyb2001_state::cyb2001(machine_config &config)
{
	// basic machine hardware
	CDP1802(config, m_maincpu, 2'517'483);
	m_maincpu->set_addrmap(AS_PROGRAM, &cyb2001_state::mem_map);
	m_maincpu->set_addrmap(AS_IO, &cyb2001_state::io_map);
	m_maincpu->wait_cb().set_constant(1);
	m_maincpu->clear_cb().set_constant(1);
	m_maincpu->ef2_cb().set(FUNC(cyb2001_state::ef2_r));
	m_maincpu->ef3_cb().set(FUNC(cyb2001_state::ef3_r));
	m_maincpu->q_cb().set(FUNC(cyb2001_state::q_w));

	// video hardware
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_refresh_hz(60);
	m_screen->set_vblank_time(ATTOSECONDS_IN_USEC(2500)); /* not accurate */
	m_screen->set_size(128, 96);
	m_screen->set_visarea_full();
	m_screen->set_screen_update(FUNC(cyb2001_state::screen_update));
	m_screen->set_palette(m_palette);

	PALETTE(config, m_palette, FUNC(cyb2001_state::cyb_palette), 16);

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

ROM_START( cybervision )
	ROM_REGION(0x400, "maincpu", 0)
	ROM_LOAD("cyb2001.bin", 0x0000, 0x400, CRC(bafbdf9e) SHA1(ea81c028e1d88571b9d0dcc47d52ab9d0ac390b6) )
ROM_END

} // anonymous namespace


/***************************************************************************
    GAME DRIVERS
***************************************************************************/

//    YEAR  NAME           PARENT    COMPAT  MACHINE    INPUT    CLASS            INIT        COMPANY                   FULLNAME                          FLAGS
COMP( 1978, cybervision,   0,        0,      cyb2001,   cyb2001, cyb2001_state,   empty_init, "Montgomery Ward",       "CyberVision 2001", MACHINE_NOT_WORKING | MACHINE_SUPPORTS_SAVE )
