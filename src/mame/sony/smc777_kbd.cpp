// license:BSD-3-Clause
// copyright-holders:Angelo Salese
/**************************************************************************************************

SMC-777 keyboard interface

TODO:
- Really comes from a 8041A with undumped ROM, with a custom protocol (host doesn't see any
  serial-like i/f);
- Remaining keys;
- CTRL modifier;
- Key repeat;
- Irq handling;
- Function key commands (can override F1~F5~H keys);
- Understand how multikey presses really works;

**************************************************************************************************/

#include "emu.h"
#include "smc777_kbd.h"
#include "machine/keyboard.ipp"

#define VERBOSE (LOG_GENERAL)
//#define LOG_OUTPUT_FUNC osd_printf_info

#include "logmacro.h"

DEFINE_DEVICE_TYPE(SMC777_KBD, smc777_kbd_device, "smc777_kbd", "Sony SMC-777 Keyboard HLE")

smc777_kbd_device::smc777_kbd_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SMC777_KBD, tag, owner, clock)
	, device_matrix_keyboard_interface(mconfig, *this, "KEY0", "KEY1", "KEY2", "KEY3", "KEY4", "KEY5", "KEY6", "KEY7", "KEY8", "KEY9", "KEYA")
	, m_key_mod(*this, "KEY_MOD")
{
}

void smc777_kbd_device::device_start()
{
	save_item(NAME(m_command));
	save_item(NAME(m_status));
	save_item(NAME(m_scan_code));
	save_item(NAME(m_repeat_interval));
	save_item(NAME(m_repeat_start));
}


void smc777_kbd_device::device_reset()
{
	reset_key_state();
	start_processing(attotime::from_hz(1'200));
	typematic_stop();

	m_command = 0;
	m_status = 0;
	m_scan_code = 0;
	m_repeat_interval = 100;
	m_repeat_start = 1000;
}

static INPUT_PORTS_START( smc777_kbd )
	PORT_START("KEY0") //0x00-0x07
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_0_PAD) PORT_CHAR(UCHAR_MAMEKEY(0_PAD))
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_1_PAD) PORT_CHAR(UCHAR_MAMEKEY(1_PAD))
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_2_PAD) PORT_CHAR(UCHAR_MAMEKEY(2_PAD))
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_3_PAD) PORT_CHAR(UCHAR_MAMEKEY(3_PAD))
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_4_PAD) PORT_CHAR(UCHAR_MAMEKEY(4_PAD))
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_5_PAD) PORT_CHAR(UCHAR_MAMEKEY(5_PAD))
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_6_PAD) PORT_CHAR(UCHAR_MAMEKEY(6_PAD))
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_7_PAD) PORT_CHAR(UCHAR_MAMEKEY(7_PAD))

	PORT_START("KEY1") //0x08-0x0f
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_8_PAD) PORT_CHAR(UCHAR_MAMEKEY(8_PAD))
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_9_PAD) PORT_CHAR(UCHAR_MAMEKEY(9_PAD))
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("/ PAD") PORT_CODE(KEYCODE_SLASH_PAD)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("* PAD") PORT_CODE(KEYCODE_ASTERISK)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("- PAD") PORT_CODE(KEYCODE_MINUS_PAD)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("+ PAD") PORT_CODE(KEYCODE_PLUS_PAD)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("ENTER PAD") PORT_CODE(KEYCODE_ENTER_PAD) PORT_CHAR(13)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME(". PAD") PORT_CODE(KEYCODE_DEL_PAD)

	PORT_START("KEY2") //0x10-0x17
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED ) // PORT_NAME("@") PORT_CODE(KEYCODE_OPENBRACE) PORT_CHAR('@')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("A") PORT_CODE(KEYCODE_A) PORT_CHAR('A')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("B") PORT_CODE(KEYCODE_B) PORT_CHAR('B')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("C") PORT_CODE(KEYCODE_C) PORT_CHAR('C')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("D") PORT_CODE(KEYCODE_D) PORT_CHAR('D')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("E") PORT_CODE(KEYCODE_E) PORT_CHAR('E')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("F") PORT_CODE(KEYCODE_F) PORT_CHAR('F')
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("G") PORT_CODE(KEYCODE_G) PORT_CHAR('G')

	PORT_START("KEY3") //0x18-0x1f
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("H") PORT_CODE(KEYCODE_H) PORT_CHAR('H')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("I") PORT_CODE(KEYCODE_I) PORT_CHAR('I')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("J") PORT_CODE(KEYCODE_J) PORT_CHAR('J')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("K") PORT_CODE(KEYCODE_K) PORT_CHAR('K')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("L") PORT_CODE(KEYCODE_L) PORT_CHAR('L')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("M") PORT_CODE(KEYCODE_M) PORT_CHAR('M')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("N") PORT_CODE(KEYCODE_N) PORT_CHAR('N')
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("O") PORT_CODE(KEYCODE_O) PORT_CHAR('O')

	PORT_START("KEY4") //0x20-0x27
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("P") PORT_CODE(KEYCODE_P) PORT_CHAR('P')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Q") PORT_CODE(KEYCODE_Q) PORT_CHAR('Q')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("R") PORT_CODE(KEYCODE_R) PORT_CHAR('R')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("S") PORT_CODE(KEYCODE_S) PORT_CHAR('S')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("T") PORT_CODE(KEYCODE_T) PORT_CHAR('T')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("U") PORT_CODE(KEYCODE_U) PORT_CHAR('U')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("V") PORT_CODE(KEYCODE_V) PORT_CHAR('V')
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("W") PORT_CODE(KEYCODE_W) PORT_CHAR('W')

	PORT_START("KEY5") //0x28-0x2f
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("X") PORT_CODE(KEYCODE_X) PORT_CHAR('X')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Y") PORT_CODE(KEYCODE_Y) PORT_CHAR('Y')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Z") PORT_CODE(KEYCODE_Z) PORT_CHAR('Z')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("0-1") /* TODO: labels */
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("0-2")
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("1-1")
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("1-2")

	PORT_START("KEY6") //0x30-0x37
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("0") PORT_CODE(KEYCODE_0) PORT_CHAR('0')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("1") PORT_CODE(KEYCODE_1) PORT_CHAR('1')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("2") PORT_CODE(KEYCODE_2) PORT_CHAR('2')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("3") PORT_CODE(KEYCODE_3) PORT_CHAR('3')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("4") PORT_CODE(KEYCODE_4) PORT_CHAR('4')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("5") PORT_CODE(KEYCODE_5) PORT_CHAR('5')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("6") PORT_CODE(KEYCODE_6) PORT_CHAR('6')
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("7") PORT_CODE(KEYCODE_7) PORT_CHAR('7')

	PORT_START("KEY7") //0x38-0x3f
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("8") PORT_CODE(KEYCODE_8) PORT_CHAR('8')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("9") PORT_CODE(KEYCODE_9) PORT_CHAR('9')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("2-1") /* TODO: labels */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("2-3")
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("3-1")
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("3-2")
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("3-3")

	PORT_START("KEY8") //0x40-0x47
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("RETURN") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(27)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("SPACE") PORT_CODE(KEYCODE_SPACE) PORT_CHAR(' ')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("BACKSPACE") PORT_CODE(KEYCODE_BACKSPACE) PORT_CHAR(8)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("TAB") PORT_CODE(KEYCODE_TAB) PORT_CHAR(9)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("ESC") PORT_CHAR(27)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("INS") PORT_CODE(KEYCODE_INSERT)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("DEL") PORT_CODE(KEYCODE_DEL)
	/* TODO: control inputs */

	PORT_START("KEY9") //0x40-0x47
	/* TODO: cursor inputs */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("CURSOR UP") PORT_CODE(KEYCODE_UP)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("CURSOR DOWN") PORT_CODE(KEYCODE_DOWN)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("CURSOR LEFT") PORT_CODE(KEYCODE_LEFT)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("CURSOR RIGHT") PORT_CODE(KEYCODE_RIGHT)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("HOME") PORT_CODE(KEYCODE_HOME)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("END") PORT_CODE(KEYCODE_END)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("PGUP") PORT_CODE(KEYCODE_PGUP)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("PGDOWN") PORT_CODE(KEYCODE_PGDN)

	PORT_START("KEYA") //0x48-0x4f
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("F1") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("F2") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("F3") PORT_CODE(KEYCODE_F3)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("F4") PORT_CODE(KEYCODE_F4)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("F5") PORT_CODE(KEYCODE_F5)

	PORT_START("KEY_MOD")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_LSHIFT) PORT_CODE(KEYCODE_RSHIFT) PORT_CHAR(UCHAR_SHIFT_1)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("CTRL") PORT_CODE(KEYCODE_LCONTROL) PORT_CHAR(UCHAR_SHIFT_2)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("KANA SHIFT") PORT_CODE(KEYCODE_LALT)


	#if 0
	PORT_BIT(0x00002000,IP_ACTIVE_HIGH,IPT_KEYBOARD) PORT_NAME("-") PORT_CODE(KEYCODE_MINUS) PORT_CHAR('-')

	PORT_BIT(0x04000000,IP_ACTIVE_HIGH,IPT_KEYBOARD) PORT_NAME(":") PORT_CODE(KEYCODE_QUOTE) PORT_CHAR(':')
	PORT_BIT(0x08000000,IP_ACTIVE_HIGH,IPT_KEYBOARD) PORT_NAME(";") PORT_CODE(KEYCODE_COLON) PORT_CHAR(';')
	PORT_BIT(0x08000000,IP_ACTIVE_HIGH,IPT_KEYBOARD) PORT_NAME("[") PORT_CODE(KEYCODE_CLOSEBRACE) PORT_CHAR('[')
	PORT_BIT(0x20000000,IP_ACTIVE_HIGH,IPT_KEYBOARD) PORT_NAME("]") PORT_CODE(KEYCODE_BACKSLASH) PORT_CHAR(']')
	PORT_BIT(0x40000000,IP_ACTIVE_HIGH,IPT_KEYBOARD) PORT_NAME("^") PORT_CODE(KEYCODE_EQUALS) PORT_CHAR('^')
	PORT_BIT(0x80000000,IP_ACTIVE_HIGH,IPT_KEYBOARD) PORT_NAME("_")
	#endif
INPUT_PORTS_END

ioport_constructor smc777_kbd_device::device_input_ports() const
{
	return INPUT_PORTS_NAME( smc777_kbd );
}

//**************************************************************************
//  device_matrix_keyboard
//**************************************************************************

uint8_t smc777_kbd_device::translate(uint8_t row, uint8_t column)
{
	const u8 keytable[2][0xa0] =
	{
		/* normal*/
		{
			0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* numpad */
			0x38, 0x39, 0x2f, 0x2a, 0x2d, 0x2b, 0x0d, 0x2e,
			0xff, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, /* A - G */
			0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, /* H - O */
			0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, /* P - W */
			0x78, 0x79, 0x7a, 0x2d, 0x5d, 0x60, 0xff, 0xff, /* X - Z */
			0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 0 - 7*/
			0x38, 0x39, 0x3b, 0x5c, 0x2c, 0x2e, 0x2f, 0xff, /* 8 - 9 */
			0x0d, 0x20, 0x08, 0x09, 0x1b, 0x0f, 0x11, 0xff,
			0x17, 0x1c, 0x16, 0x19, 0x14, 0x0e, 0x12, 0x03,
			0x01, 0x02, 0x04, 0x06, 0x0b, 0xff, 0xff, 0xff
		},
		/* shift */
		{
			0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* numpad */
			0x38, 0x39, 0x2f, 0x2a, 0x2d, 0x2b, 0x0d, 0x2e,
			0xff, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
			0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, /* H - O */
			0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, /* P - W */
			0x58, 0x59, 0x5a, 0x5f, 0x7d, 0x7e, 0xff, 0xff, /* X - Z */
			0x29, 0x21, 0x40, 0x23, 0x24, 0x25, 0x5e, 0x26,
			0x2a, 0x28, 0x3a, 0x7c, 0x3c, 0x3e, 0x3f, 0xff,
			0x0d, 0x20, 0x08, 0x09, 0x1b, 0x0f, 0x11, 0xff,
			0x17, 0x1c, 0x16, 0x19, 0x14, 0x0e, 0x12, 0x03,
			0x15, 0x18, 0x12, 0x05, 0x03, 0xff, 0xff, 0xff /* F1 - F5 */
		}
	};

	// TODO: CTRL modifier, CAPS LOCK modifier, not all keys supports kana modifier
	const u8 shift = BIT(m_key_mod->read(), 0);

	return keytable[shift][row * 8 + column] | (BIT(m_key_mod->read(), 4) << 7);
}


void smc777_kbd_device::key_make(uint8_t row, uint8_t column)
{
	m_scan_code = translate(row, column);
	m_status |= 5;
}

void smc777_kbd_device::key_break(uint8_t row, uint8_t column)
{
//	m_scan_code = 0;
	m_status &= ~4;
}

void smc777_kbd_device::key_repeat(uint8_t row, uint8_t column)
{
	// ...
}

//**************************************************************************
//  Interface
//**************************************************************************

void smc777_kbd_device::scan_mode(u8 data)
{
	// ICF: clear irq
	if (BIT(data, 7))
	{
		m_status &= ~1;
		LOG("Irq ack\n");
	}
	// FEF: enter setting mode
	if (BIT(data, 4))
	{
		LOG("Enter setting mode\n");
		m_command = 0xff;
	}
	// IEF: enable interrupt
	if (BIT(data, 0))
		LOG("Enable ief_key (tbd)\n");
}

u8 smc777_kbd_device::data_r(offs_t offset)
{
	// TODO: really cleared after 80 usec
	if (!machine().side_effects_disabled())
		m_status &= ~1;
	return m_scan_code;
}

void smc777_kbd_device::data_w(offs_t offset, u8 data)
{
	if (m_command == 0)
		scan_mode(data);
	else
	{
		LOG("data_w function key code TBD %02x\n", data);
	}
}

/*
 * In scan mode:
 * x--- ---- CF  CTRL pressed
 * -x-- ---- SF  SHIFT pressed
 * ---- -x-- ASF key pressed now
 * ---- ---x BSF key was pressed (released on data read)
 * In setting mode:
 * ---- -x-- /BUSY command done if 1
 * ---- --x- /CS command accepted if 0
 * ---- ---x DR data ready if 1
 */
u8 smc777_kbd_device::status_r(offs_t offset)
{
	u8 res = 0;
	if (m_command == 0)
	{
		res = m_status;
		res|= (m_key_mod->read() & 3) << 6;
	}
	else
		res = 4 | (m_command == 0x80);

	return res;
}

void smc777_kbd_device::control_w(offs_t offset, u8 data)
{
	if (m_command == 0)
	{
		scan_mode(data);
	}
	else
	{
		m_command = data & 0xc0;
		LOG("Command %02x - %02x\n", m_command, data & 0x3f);
		switch(m_command)
		{
			case 0x00:
				LOG("Exit setting mode\n");
				break;

			case 0x40:
				if (BIT(data, 5))
				{
					m_repeat_start = 500 + (100 * (data & 0xf));
					LOG("Key repeat start %d\n", m_repeat_start);
				}
				else
				{
					m_repeat_interval = 20 + (20 * (data & 0xf));
					LOG("Key repeat interval %d\n", m_repeat_interval);
				}
				break;

			case 0x80:
				LOG("Function key %s (tbd)\n", BIT(data, 5) ? "write" : "read");
				break;

			case 0xc0:
				this->reset();
				LOG("Initialize key\n");
				break;
		}
	}
}

