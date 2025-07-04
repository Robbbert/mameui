// license:BSD-3-Clause
// copyright-holders:Nigel Barnes
/**********************************************************************

    AY-3-4592 Keyboard Encoder emulation

    TODO:
    - verify keyboard rollover behaviour.

**********************************************************************/

#include "emu.h"
#include "ay34592.h"


DEFINE_DEVICE_TYPE(AY34592, ay34592_device, "ay34592", "AY-3-4592 Keyboard Encoder")

ay34592_device::ay34592_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, AY34592, tag, owner, clock)
	, m_read_x(*this, 0xff)
	, m_akd_cb(*this)
	, m_sli_cb(*this)
	, m_ali_cb(*this)
	, m_x15_cb(*this)
	, m_scan_timer(nullptr)
{
}


uint16_t ay34592_device::output_code(int mode, int x, int y)
{
	static const uint16_t OUTPUT_CODE[4][14][8] =
	{
		// normal
		{
			//  Y0     Y1     Y2     Y3     Y4     Y5     Y6     Y7
			{ 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x0ce }, // X0
			{ 0x1e4, 0x0cd, 0x0cd, 0x188, 0x18e, 0x18c, 0x19e, 0x185 }, // X1
			{ 0x17f, 0x0cb, 0x0cc, 0x18d, 0x19a, 0x19b, 0x187, 0x19c }, // X2
			{ 0x17e, 0x17d, 0x0ca, 0x18b, 0x199, 0x198, 0x189, 0x19d }, // X3
			{ 0x17c, 0x0c8, 0x0c9, 0x186, 0x197, 0x191, 0x0c9, 0x0df }, // X4
			{ 0x17b, 0x0c7, 0x0c8, 0x18a, 0x195, 0x194, 0x192, 0x0d3 }, // X5
			{ 0x17a, 0x0c6, 0x0c7, 0x196, 0x190, 0x194, 0x193, 0x192 }, // X6
			{ 0x179, 0x0cf, 0x0c6, 0x178, 0x18f, 0x0c4, 0x193, 0x0d1 }, // X7
			{ 0x0d2, 0x191, 0x18f, 0x1a4, 0x0d8, 0x0c4, 0x0d0, 0x177 }, // X8
			{ 0x0c2, 0x0c5, 0x176, 0x1a3, 0x175, 0x1a4, 0x1f2, 0x1a2 }, // X9
			{ 0x080, 0x174, 0x0d2, 0x173, 0x1f5, 0x1bf, 0x1a1, 0x1a0 }, // X10
			{ 0x172, 0x1f6, 0x0d2, 0x171, 0x190, 0x1a4, 0x1f7, 0x160 }, // X11
			{ 0x170, 0x0c8, 0x1f4, 0x16f, 0x0cb, 0x0d3, 0x0ce, 0x0cf }, // X12
			{ 0x16e, 0x0c6, 0x0c7, 0x0ca, 0x0c9, 0x0cd, 0x0cc, 0x0d1 }  // X13
		},

		// shift
		{
			//  Y0     Y1     Y2     Y3     Y4     Y5     Y6     Y7
			{ 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x0de }, // X0
			{ 0x1e4, 0x18f, 0x0dd, 0x1a8, 0x1ae, 0x1ac, 0x1be, 0x1a5 }, // X1
			{ 0x17f, 0x0db, 0x0dc, 0x1ad, 0x1ba, 0x1bb, 0x1a7, 0x1bc }, // X2
			{ 0x17e, 0x17d, 0x0da, 0x1ab, 0x1b9, 0x1b8, 0x1a9, 0x1bd }, // X3
			{ 0x17c, 0x0d9, 0x0d9, 0x1a6, 0x1b7, 0x1b1, 0x0c3, 0x0df }, // X4
			{ 0x17b, 0x0d5, 0x0d8, 0x1aa, 0x1b5, 0x1b4, 0x1b2, 0x0c3 }, // X5
			{ 0x17a, 0x0d7, 0x0d7, 0x1b6, 0x1b0, 0x1a4, 0x1b3, 0x1a2 }, // X6
			{ 0x179, 0x0d6, 0x0d6, 0x178, 0x1af, 0x0c5, 0x1a3, 0x0c1 }, // X7
			{ 0x1a0, 0x1a1, 0x1bf, 0x1a2, 0x0dd, 0x0d4, 0x0c0, 0x177 }, // X8
			{ 0x0d4, 0x0d5, 0x176, 0x083, 0x175, 0x084, 0x1f2, 0x082 }, // X9
			{ 0x080, 0x174, 0x1a0, 0x173, 0x1f5, 0x1a3, 0x081, 0x0c2 }, // X10
			{ 0x172, 0x1f6, 0x0c2, 0x171, 0x1a0, 0x1a2, 0x1f7, 0x160 }, // X11
			{ 0x170, 0x0c8, 0x1f4, 0x16f, 0x0cb, 0x0d3, 0x0ce, 0x0cf }, // X12
			{ 0x16e, 0x0c6, 0x0c7, 0x0ca, 0x0c9, 0x0cd, 0x0cc, 0x0d1 }  // X13
		},

		// control
		{
			//  Y0     Y1     Y2     Y3     Y4     Y5     Y6     Y7
			{ 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x0ce }, // X0
			{ 0x1e4, 0x0cd, 0x0cd, 0x1e8, 0x1ee, 0x1ec, 0x1fe, 0x1e5 }, // X1
			{ 0x17f, 0x0cb, 0x0cc, 0x1ed, 0x1fa, 0x1fb, 0x1e7, 0x1fc }, // X2
			{ 0x17e, 0x17d, 0x0ca, 0x1eb, 0x1f9, 0x1f8, 0x1e9, 0x1fd }, // X3
			{ 0x17c, 0x0c8, 0x0c9, 0x1e6, 0x1f7, 0x1f1, 0x0c9, 0x0df }, // X4
			{ 0x17b, 0x0c7, 0x0c8, 0x1ea, 0x1f5, 0x1f4, 0x1f2, 0x0d3 }, // X5
			{ 0x17a, 0x0c6, 0x0c7, 0x1f6, 0x1f0, 0x1f4, 0x1f3, 0x1f2 }, // X6
			{ 0x179, 0x0cf, 0x0c6, 0x178, 0x1ef, 0x0c4, 0x1f3, 0x0d1 }, // X7
			{ 0x0d2, 0x1f1, 0x1ef, 0x1e4, 0x0d8, 0x0c4, 0x0d0, 0x177 }, // X8
			{ 0x0c2, 0x0c5, 0x176, 0x1e3, 0x175, 0x1e4, 0x1f2, 0x1e2 }, // X9
			{ 0x080, 0x174, 0x1e0, 0x173, 0x1f5, 0x1ff, 0x1e1, 0x1a0 }, // X10
			{ 0x172, 0x1f6, 0x0d2, 0x171, 0x1f0, 0x1a4, 0x1f7, 0x160 }, // X11
			{ 0x170, 0x0c8, 0x1f4, 0x16f, 0x0cb, 0x0d3, 0x0ce, 0x0cf }, // X12
			{ 0x16e, 0x0c6, 0x0c7, 0x0ca, 0x0c9, 0x0cd, 0x0cc, 0x0d1 }  // X13
		},

		// shift/control
		{
			//  Y0     Y1     Y2     Y3     Y4     Y5     Y6     Y7
			{ 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x0de }, // X0
			{ 0x1e4, 0x1ff, 0x0dd, 0x1e8, 0x1ee, 0x1ec, 0x1fe, 0x1e5 }, // X1
			{ 0x17f, 0x0db, 0x0dc, 0x1ed, 0x1fa, 0x1fb, 0x1e7, 0x1fc }, // X2
			{ 0x17e, 0x17d, 0x0da, 0x1eb, 0x1f9, 0x1f8, 0x1e9, 0x1fd }, // X3
			{ 0x17c, 0x0d9, 0x0d9, 0x1e6, 0x1f7, 0x1f1, 0x0c3, 0x0df }, // X4
			{ 0x17b, 0x0d5, 0x0d8, 0x1ea, 0x1f5, 0x1f4, 0x1f2, 0x0c3 }, // X5
			{ 0x17a, 0x0d7, 0x0d7, 0x1f6, 0x1f0, 0x1e4, 0x1f3, 0x1e2 }, // X6
			{ 0x179, 0x0d6, 0x0d6, 0x178, 0x1ef, 0x0c5, 0x1e3, 0x0c1 }, // X7
			{ 0x1a0, 0x1e1, 0x1ff, 0x1e2, 0x0dd, 0x0d4, 0x0c0, 0x177 }, // X8
			{ 0x0d4, 0x0d5, 0x176, 0x1e3, 0x175, 0x1e4, 0x1f2, 0x1e2 }, // X9
			{ 0x080, 0x174, 0x1e0, 0x173, 0x1f5, 0x1ff, 0x1e1, 0x0c2 }, // X10
			{ 0x172, 0x1f6, 0x0c2, 0x171, 0x1e0, 0x1a2, 0x1f7, 0x160 }, // X11
			{ 0x170, 0x0c8, 0x1f4, 0x16f, 0x0cb, 0x0d3, 0x0ce, 0x0cf }, // X12
			{ 0x16e, 0x0c6, 0x0c7, 0x0ca, 0x0c9, 0x0cd, 0x0cc, 0x0d1 }  // X13
		}
	};
	return OUTPUT_CODE[mode][x][y];
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void ay34592_device::device_start()
{
	m_scan_timer = timer_alloc(FUNC(ay34592_device::scan_matrix), this);

	// register for state saving
	save_item(NAME(m_matrix_addr));
	save_item(NAME(m_keydown_addr));
	save_item(NAME(m_key_latch));
	save_item(NAME(m_opcode));
	save_item(NAME(m_akd));
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void ay34592_device::device_reset()
{
	m_scan_timer->adjust(attotime::zero, 0, attotime::from_hz(clock()));

	m_matrix_addr = 0x00;
	m_keydown_addr = 0x00;
	m_key_latch = 0x00;
	m_opcode = 0x00;
	m_akd = 0;
}


TIMER_CALLBACK_MEMBER(ay34592_device::scan_matrix)
{
	int akd = m_akd;

	// 7 bit counter
	m_matrix_addr--;
	m_matrix_addr &= 0x7f;

	const int x = m_matrix_addr >> 3;
	const int y = m_matrix_addr & 7;

	// scan rows X0 to X13 only
	if (x < 14)
	{
		if (x == 0 && y < 7) // FFB = 1
		{
			// Op-Code     Function
			// x x 0 0 0   Function key (with up/down codes)
			// x x 0 0 1   Right Shift Key
			// x x 0 1 0   Left Shift Key
			// x x 0 1 1   Shift Lock Key or Discrete Key (output SLI)
			// x x 1 0 0   Control Key
			// x x 1 0 1   Alpha Lock Key or Discrete Key (output ALI)
			// x 0 1 1 0   Error Reset Key or Discrete Key (output X15)
			// x x 1 1 1   Discrete Key (output D10)

			if (!BIT(m_read_x[x](), y))
				m_opcode |= 1 << output_code(0, x, y);
			else
				m_opcode &= ~(1 << output_code(0, x, y));

			m_sli_cb(BIT(m_opcode, 3)); // Shift Lock Indicator
			m_ali_cb(BIT(m_opcode, 5)); // Alpha Lock Indicator
			m_x15_cb(BIT(m_opcode, 6)); // X15 discrete output
		}
		else // FFB = 0
		{
			if (!BIT(m_read_x[x](), y))
			{
				if (m_matrix_addr == m_keydown_addr)
				{
					if ((m_opcode & 0x0e) && (m_opcode & 0x10))
						m_key_latch = output_code(3, x, y); // Shift/Control
					else if (m_opcode & 0x10)
						m_key_latch = output_code(2, x, y); // Control
					else if (m_opcode & 0x0e)
						m_key_latch = output_code(1, x, y); // Shift
					else
						m_key_latch = output_code(0, x, y); // Normal

					// set 'any key down'
					akd = 1;
				}
				else
				{
					m_keydown_addr = m_matrix_addr;
				}
			}
		}
	}

	// reset 'any key down' if all keys released
	bool keydown = false;
	for (int x = 0; x < 14; x++)
	{
		uint8_t keymask = (x == 0) ? 0x80 : 0xff;
		if ((m_read_x[x]() & keymask) != keymask)
		{
			keydown = true;
			break;
		}
	}
	if (!keydown)
	{
		// clear 'any key down'
		akd = 0;
	}

	// update AKD output (only if changed to avoid conflict with any external repeat circuit)
	if (akd != m_akd)
	{
		m_akd_cb(akd);
		m_akd = akd;
	}
}


uint16_t ay34592_device::data_r()
{
	return m_key_latch;
}
