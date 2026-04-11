// license:BSD-3-Clause
// copyright-holders:hap
// thanks-to:Berger
/*******************************************************************************

Milton Bradley (Electronic) Grand Master (stylized as Grand·Master) (model 4243)
aka Phantom Chess Computer in the UK, and Milton in the rest of Europe

TODO:
- WIP
- move chessboard to a device (currently copied from emirage)

*******************************************************************************/

#include "emu.h"

#include "cpu/m6502/m6502.h"
#include "machine/7474.h"
#include "machine/clock.h"
#include "machine/input_merger.h"
#include "machine/sensorboard.h"
#include "sound/dac.h"
#include "video/pwm.h"

#include "speaker.h"

// internal artwork
//#include "grandmaster.lh"


namespace {

class grandmas_state : public driver_device
{
public:
	grandmas_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_irq_ff(*this, "irq_ff%u", 0),
		m_board(*this, "board"),
		m_display(*this, "display"),
		m_dac(*this, "dac"),
		m_inputs(*this, "IN.0"),
		m_piece_hand(*this, "cpu_hand"),
		m_out_motor(*this, "motor%u", 0U),
		m_out_pos(*this, "pos_%c", unsigned('x'))
	{ }

	void grandmas(machine_config &config);

protected:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;

private:
	// devices/pointers
	required_device<cpu_device> m_maincpu;
	required_device_array<ttl7474_device, 3> m_irq_ff;
	required_device<sensorboard_device> m_board;
	required_device<pwm_display_device> m_display;
	required_device<dac_1bit_device> m_dac;
	required_ioport m_inputs;
	output_finder<> m_piece_hand;
	output_finder<5> m_out_motor;
	output_finder<2> m_out_pos;

	u8 m_inp_mux = 0;

	u8 m_magnet = 0;
	u8 m_pieces_map[0x40][0x40] = { };

	u8 m_motor_control[2] = { };
	u32 m_motor_max[2] = { };
	u32 m_motor_pos[2] = { };
	s32 m_motor_drift[2] = { };
	u8 m_motor_quad[2] = { };

	attotime m_motor_period;
	attotime m_motor_remain[2];
	emu_timer *m_motor_timer[2];

	void init_board(u8 data);
	void clear_board(u8 data);
	void init_motors();

	void get_scaled_pos(double *x, double *y);
	void output_magnet_pos();
	void realign_magnet_pos();
	void update_piece(u8 magnet);

	TIMER_CALLBACK_MEMBER(motor_count);
	void motor_control(int m, u8 control);

	void main_map(address_map &map) ATTR_COLD;

	u8 irq_clear_r(offs_t offset);
	void irq_clear_w(offs_t offset, u8 data = 0);

	u8 status_r();
	void control_w(u8 data);
	void leds_w(u8 data);
	u8 input_r(offs_t offset);
};



/*******************************************************************************
    Initialization
*******************************************************************************/

void grandmas_state::machine_start()
{
	init_motors();

	// resolve outputs
	m_piece_hand.resolve();
	m_out_motor.resolve();
	m_out_pos.resolve();

	// register for savestates
	save_item(NAME(m_inp_mux));

	save_item(NAME(m_magnet));
	save_item(NAME(m_pieces_map));
	save_item(NAME(m_motor_control));
	save_item(NAME(m_motor_pos));
	save_item(NAME(m_motor_drift));
	save_item(NAME(m_motor_quad));
	save_item(NAME(m_motor_remain));
}

void grandmas_state::machine_reset()
{
	// make sure 7474 is not in an indeterminate state
	for (int i = 0; i < 3; i++)
		irq_clear_w(i);

	memset(m_motor_drift, 0, sizeof(m_motor_drift));
	output_magnet_pos();
}

void grandmas_state::init_board(u8 data)
{
	m_board->preset_chess(data);

	// reposition pieces if board will be rotated
	if (data & 2)
	{
		for (int y = 0; y < 8; y++)
			for (int x = 7; x >= 0; x--)
			{
				m_board->write_piece(x + 4, y, m_board->read_piece(x, y));
				m_board->write_piece(x, y, 0);
			}
	}
}

void grandmas_state::clear_board(u8 data)
{
	memset(m_pieces_map, 0, sizeof(m_pieces_map));
	m_piece_hand = 0;
	m_board->clear_board(data);
}



/*******************************************************************************
    Motor Sim
*******************************************************************************/

void grandmas_state::init_motors()
{
	m_motor_period = attotime::from_usec(2500);

	for (int i = 0; i < 2; i++)
	{
		m_motor_timer[i] = timer_alloc(FUNC(grandmas_state::motor_count), this);
		m_motor_remain[i] = m_motor_period / 2;
	}

	m_motor_max[1] = 575 - 1;
	m_motor_pos[1] = 522;

	m_motor_max[0] = 750 - 1;
	m_motor_pos[0] = 139;
}

void grandmas_state::get_scaled_pos(double *x, double *y)
{
	// 64 counts per square
	*x = std::clamp(double(int(m_motor_pos[0]) - 10) / (64.0 / 4.0) + 2.0, 0.0, 48.0);
	*y = std::clamp(double(int(m_motor_pos[1]) - 75) / (64.0 / 4.0) + 2.0, 0.0, 32.0);
}

void grandmas_state::output_magnet_pos()
{
	double x, y;
	get_scaled_pos(&x, &y);

	// put active state on x bit 11
	const int active = m_magnet ? 0x800 : 0;
	m_out_pos[0] = int(x * 25.0 + 0.5) | active;
	m_out_pos[1] = int(y * 25.0 + 0.5);

	popmessage("%d %d",m_motor_pos[0], m_motor_pos[1]);
}

void grandmas_state::realign_magnet_pos()
{
	return;

	// compensate for gradual drift, see BTANB
	for (int i = 0; i < 2; )
	{
		double pos[2];
		get_scaled_pos(&pos[0], &pos[1]);

		const double limit = 1.0 / 8.0; // 4 counts
		int inc = 0;

		if ((round(pos[i]) - pos[i]) > limit && m_motor_pos[i] < m_motor_max[i] - 4)
			inc = 1;
		else if ((round(pos[i]) - pos[i]) < -limit && m_motor_pos[i] > 4)
			inc = -1;
		else
			i++;

		if (inc != 0)
		{
			m_motor_pos[i] += inc * 4;
			m_motor_drift[i] -= inc;

			logerror("motor %c drift error (%d total)\n", 'X' + i, m_motor_drift[i]);
		}
	}
}

void grandmas_state::update_piece(u8 magnet)
{
	if (magnet == m_magnet)
		return;

	realign_magnet_pos();

	m_magnet = magnet;
	output_magnet_pos();

	double dx, dy;
	get_scaled_pos(&dx, &dy);

	int mx = dx + 0.5;
	int my = dy + 0.5;

	// convert motors position into board coordinates
	int x = mx / 4 - 2;
	int y = 7 - (my / 4);

	if (x < 0)
		x += 12;

	const bool valid_pos = (mx & 3) == 2 && (my & 3) == 2;

	// sensorboard handling is almost the same as fidelity/phantom.cpp
	if (magnet)
	{
		if (valid_pos)
		{
			// pick up piece, unless it was picked up by the user
			const int pos = (y << 4 & 0xf0) | (x & 0x0f);
			if (pos != m_board->get_handpos())
			{
				m_piece_hand = m_board->read_piece(x, y);

				if (m_piece_hand != 0)
				{
					m_board->write_piece(x, y, 0);
					m_board->refresh();
				}
			}
		}
		else
		{
			// pick up piece from internal pieces map
			m_piece_hand = m_pieces_map[my][mx];
			m_pieces_map[my][mx] = 0;
		}
	}
	else if (m_piece_hand != 0)
	{
		if (valid_pos)
		{
			// collision with piece on board (user interference)
			if (m_board->read_piece(x, y) != 0)
				popmessage("Collision at %c%d!", x + 'A', y + 1);
			else
			{
				m_board->write_piece(x, y, m_piece_hand);
				m_board->refresh();
			}
		}
		else
		{
			// collision with internal pieces map (shouldn't happen)
			if (m_pieces_map[my][mx] != 0)
				popmessage("Internal collision!");
			else
				m_pieces_map[my][mx] = m_piece_hand;
		}

		m_piece_hand = 0;
	}
}

TIMER_CALLBACK_MEMBER(grandmas_state::motor_count)
{
	const int m = param ? 1 : 0;
	assert(m_motor_control[m] & 3);

	// 1 quarter rotation per period
	int inc = 0;
	if (m_motor_control[m] & 2)
	{
		if (m_motor_pos[m] < m_motor_max[m])
			inc = 1;
	}
	else if (m_motor_pos[m] > 0)
		inc = -1;

	m_motor_remain[m] = m_motor_period;

	if (inc == 0)
		return;

	m_motor_pos[m] += inc;
	m_motor_timer[m]->adjust(m_motor_period, m);

	output_magnet_pos();

	// update quadrature encoder
	static const u8 lut_quad[4] = { 0,1,3,2 };
	m_motor_quad[m] = lut_quad[m_motor_pos[m] & 3];

	m_irq_ff[m + 1]->clock_w(m_motor_quad[m] >> 1 & 1);
}

void grandmas_state::motor_control(int m, u8 control)
{
	// it's not moving when both directions are set
	if (control == 3)
		control = 0;

	if (control == m_motor_control[m])
		return;

	// remember remaining time
	if (m_motor_control[m] & 3 && !m_motor_timer[m]->remaining().is_never())
	{
		m_motor_remain[m] = m_motor_timer[m]->remaining();
		m_motor_timer[m]->adjust(attotime::never);
	}

	// invert remaining time if direction flipped
	if ((m_motor_control[m] ^ control) & 2)
		m_motor_remain[m] = m_motor_period - m_motor_remain[m];

	m_motor_control[m] = control;

	// (re)start the timer
	if (control & 3)
		m_motor_timer[m]->adjust(m_motor_remain[m], m);
}



/*******************************************************************************
    I/O
*******************************************************************************/

void grandmas_state::irq_clear_w(offs_t offset, u8 data)
{
	// any read or write here clears IRQ F/F
	m_irq_ff[offset]->clear_w(0);
	m_irq_ff[offset]->clear_w(1);
}

u8 grandmas_state::irq_clear_r(offs_t offset)
{
	if (!machine().side_effects_disabled())
		irq_clear_w(offset);

	return 0xff;
}

u8 grandmas_state::status_r()
{
	// d0: magnet sensor
	u8 data = m_piece_hand ? 1 : 0;

	// d1,d3,d5: IRQ F/F Q
	for (int i = 0; i < 3; i++)
		data |= m_irq_ff[2 - i]->output_r() << (i * 2 + 1);

	// d2,d4: motor y/x quadrature state
	for (int i = 0; i < 2; i++)
		data |= ((m_motor_quad[i] ^ m_motor_quad[i] >> 1) & 1) << (i ? 2 : 4);

	return data | 0xc0;
}

void grandmas_state::control_w(u8 data)
{
	// d0: vertical motor down
	// d1: vertical motor up
	motor_control(1, bitswap<2>(data, 0, 1));

	// d2: horizontal motor left
	// d3: horizontal motor right
	motor_control(0, data >> 2 & 3);

	// d4: electromagnet
	update_piece(BIT(data, 4));

	// d5: speaker out
	m_dac->write(BIT(data, 5));
}

void grandmas_state::leds_w(u8 data)
{
	// d0-d3: 74145 A-D
	// 74145 1-8: led data
	// 74145 0-8: input mux
	m_inp_mux = data & 0xf;

	// d4,d5: led select
	m_display->matrix(data >> 4 & 3, (1 << m_inp_mux) >> 1);
}

u8 grandmas_state::input_r(offs_t offset)
{
	u16 data = 0;

	if (m_inp_mux == 8)
		data = m_inputs->read();

	return ~data >> (offset * 6) | 0xc0;
}



/*******************************************************************************
    Address Maps
*******************************************************************************/

void grandmas_state::main_map(address_map &map)
{
	map.unmap_value_high();
	map.global_mask(0x7fff);

	map(0x0000, 0x07ff).mirror(0x1800).ram();

	map(0x2000, 0x2002).mirror(0x1ff8).rw(FUNC(grandmas_state::irq_clear_r), FUNC(grandmas_state::irq_clear_w));
	map(0x2003, 0x2003).mirror(0x1ff8).r(FUNC(grandmas_state::status_r));
	map(0x2004, 0x2004).mirror(0x1ff8).w(FUNC(grandmas_state::control_w));
	map(0x2005, 0x2006).mirror(0x1ff8).r(FUNC(grandmas_state::input_r));
	map(0x2007, 0x2007).mirror(0x1ff8).w(FUNC(grandmas_state::leds_w));

	map(0x4000, 0x7fff).rom().region("maincpu", 0);
}



/*******************************************************************************
    Input Ports
*******************************************************************************/

static INPUT_PORTS_START( grandmas )
	PORT_START("IN.0")
	PORT_BIT(0x001, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_1) // auto/problem
	PORT_BIT(0x002, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_2)
	PORT_BIT(0x004, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_3)
	PORT_BIT(0x008, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_4)
	PORT_BIT(0x010, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_5)
	PORT_BIT(0x020, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_6)
	PORT_BIT(0x040, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_7)
	PORT_BIT(0x080, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_8)
	PORT_BIT(0x100, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_Q)
	PORT_BIT(0x200, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_W)
	PORT_BIT(0x400, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_E)
	PORT_BIT(0x800, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_R)
INPUT_PORTS_END



/*******************************************************************************
    Machine Configs
*******************************************************************************/

void grandmas_state::grandmas(machine_config &config)
{
	// basic machine hardware
	M6502(config, m_maincpu, 3.58_MHz_XTAL / 2);
	m_maincpu->set_addrmap(AS_PROGRAM, &grandmas_state::main_map);

	INPUT_MERGER_ANY_HIGH(config, "mainirq").output_handler().set_inputline(m_maincpu, m6502_device::IRQ_LINE);

	TTL7474(config, m_irq_ff[0]).output_cb().set("mainirq", FUNC(input_merger_device::in_w<0>));
	TTL7474(config, m_irq_ff[1]).output_cb().set("mainirq", FUNC(input_merger_device::in_w<1>));
	TTL7474(config, m_irq_ff[2]).output_cb().set("mainirq", FUNC(input_merger_device::in_w<2>));

	auto &irq_clock(CLOCK(config, "irq_clock"));
	irq_clock.set_period(attotime::from_ticks(0x1000, 3.58_MHz_XTAL / 2)); // CD4020 Q12
	irq_clock.signal_handler().set(m_irq_ff[0], FUNC(ttl7474_device::clock_w));

	SENSORBOARD(config, m_board).set_type(sensorboard_device::BUTTONS);
	m_board->set_size(8+4, 8);
	m_board->clear_cb().set(FUNC(grandmas_state::clear_board));
	m_board->init_cb().set(FUNC(grandmas_state::init_board));
	m_board->set_delay(attotime::from_msec(150));

	// video hardware
	PWM_DISPLAY(config, m_display).set_size(2, 8);

	//config.set_default_layout(layout_grandmaster);

	// sound hardware
	SPEAKER(config, "speaker").front_center();
	DAC_1BIT(config, m_dac).add_route(ALL_OUTPUTS, "speaker", 0.25);
}



/*******************************************************************************
    ROM Definitions
*******************************************************************************/

ROM_START( grandmas )
	ROM_REGION( 0x4000, "maincpu", 0 )
	ROM_LOAD("c19679_7830043002.u3", 0x0000, 0x2000, CRC(c7a91b34) SHA1(43f9caf8a13ae1a3274851fbcc411bc50c21fe1d) )
	ROM_LOAD("c19680_7830043001.u2", 0x2000, 0x2000, CRC(b47dda81) SHA1(02c946fa47db1c7edfb59e11ad3c802b92f4d3a1) )
ROM_END

} // anonymous namespace



/*******************************************************************************
    Drivers
*******************************************************************************/

//    YEAR  NAME      PARENT  COMPAT  MACHINE   INPUT     CLASS           INIT        COMPANY, FULLNAME, FLAGS
SYST( 1983, grandmas, 0,      0,      grandmas, grandmas, grandmas_state, empty_init, "Milton Bradley", "Grand Master", MACHINE_SUPPORTS_SAVE | MACHINE_NOT_WORKING )
