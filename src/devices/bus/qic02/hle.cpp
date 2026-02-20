// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * QIC-02 Controller (high-level emulation)
 *
 * Sources:
 *   -
 *
 * TODO:
 *   - everything
 */
/*
 * QIC-11: 4 track, 450ft DC300XL - 20MB
 * QIC-24: 9 track, 450ft/600ft DC600A - 45MB/60MB
 * QIC-120: 15 track, DC6150, 125MB
 * QIC-150: 18 track, DC6150, 150MB
 *
 * QIC-02: 1/4-Inch Cartridge Tape Drive Intelligent Interface (rev F, 19-Apr-88)
 * QIC-36: 1/4-Inch Cartridge Tape Drive Basic Interface (rev C, 14-Sep-84)
 *
 * Wangtek Series 5000E: QIC-36
 *  DC600A, DC300XL, DC615A, DC600XTD
 * Wangtek 5099EQ/5125EQ/5150EQ: QIC-02
 *  
 * 
 */
 /*
 *
	 // IRIS 1400 tape drive specifications: 10,000 flux/inch, 100k/foot
	 // 90 inches/second
	 // 300 and 450 feet length
	 // 10000fci, 450ft, 45ips, 45MB QIC (Archive, Wangtek, Cypher)

 */

#include "emu.h"
#include "hle.h"

#include "imagedev/simh_tape_image.h"

#define LOG_STATE   (1U << 1)
#define LOG_CONTROL (1U << 2)
#define LOG_DATA    (1U << 3)

#define VERBOSE (LOG_GENERAL) //|LOG_STATE|LOG_CONTROL|LOG_DATA)
#include "logmacro.h"

enum command_type
{
	STANDARD,
	OPTIONAL,
	RESERVED,
	VENDOR,
};
struct command
{
	u8 command;
	u8 mask;
	command_type type;
	char const *const description;
}
commands[] =
{
	{ 0x00, 0xff, VENDOR,   nullptr },
	{ 0x01, 0xff, STANDARD, "select drive 1" },
	{ 0x02, 0xff, STANDARD, "select drive 2" },
	{ 0x03, 0xff, VENDOR,   nullptr },
	{ 0x04, 0xff, STANDARD, "select drive 3" },
	{ 0x05, 0xff, VENDOR,   nullptr },
	{ 0x06, 0xfe, VENDOR,   nullptr },
	{ 0x08, 0xff, STANDARD, "select drive 4" },
	{ 0x09, 0xff, VENDOR,   nullptr },
	{ 0x0a, 0xfe, VENDOR,   nullptr },
	{ 0x0c, 0xfc, VENDOR,   nullptr },

	{ 0x10, 0xff, VENDOR,   nullptr },
	{ 0x11, 0xff, OPTIONAL, "select drive 1, lock cartridge" },
	{ 0x12, 0xff, OPTIONAL, "select drive 2, lock cartridge" },
	{ 0x13, 0xff, VENDOR,   nullptr },
	{ 0x14, 0xff, OPTIONAL, "select drive 3, lock cartridge" },
	{ 0x15, 0xff, VENDOR,   nullptr },
	{ 0x16, 0xfe, VENDOR,   nullptr },
	{ 0x18, 0xff, OPTIONAL, "select drive 4, lock cartridge" },
	{ 0x19, 0xff, VENDOR,   nullptr },
	{ 0x1a, 0xfe, VENDOR,   nullptr },
	{ 0x1c, 0xfc, VENDOR,   nullptr },

	{ 0x20, 0xff, VENDOR,   nullptr },
	{ 0x21, 0xff, STANDARD, "position to beginning of tape" },
	{ 0x22, 0xff, STANDARD, "erase the entire tape" },
	{ 0x23, 0xff, VENDOR,   nullptr },
	{ 0x24, 0xff, STANDARD, "initialize cartridge" },
	{ 0x25, 0xff, OPTIONAL, "select auto cartridge initialization" },
	{ 0x26, 0xfe, VENDOR,   nullptr },
	{ 0x28, 0xf8, VENDOR,   nullptr },

	{ 0x30, 0xf0, VENDOR,   nullptr },

	{ 0x40, 0xff, STANDARD, "write" },
	{ 0x41, 0xff, OPTIONAL, "write without underruns" },
	{ 0x42, 0xfe, VENDOR,   nullptr },
	{ 0x44, 0xfc, VENDOR,   nullptr },
	{ 0x48, 0xff, OPTIONAL, "enter 6 byte parameter block" },
	{ 0x49, 0xff, VENDOR,   nullptr },
	{ 0x4a, 0xfe, VENDOR,   nullptr },
	{ 0x4c, 0xfc, VENDOR,   nullptr },

	{ 0x50, 0xf0, VENDOR,   nullptr },

	{ 0x60, 0xff, STANDARD, "write file mark" },
	{ 0x61, 0xff, RESERVED, nullptr },
	{ 0x62, 0xfe, VENDOR,   nullptr },
	{ 0x64, 0xfc, VENDOR,   nullptr },
	{ 0x68, 0xf8, VENDOR,   nullptr },

	{ 0x70, 0xf0, OPTIONAL, "write N file marks" },

	{ 0x80, 0xff, STANDARD, "read" },
	{ 0x81, 0xff, OPTIONAL, "space forward" },
	{ 0x82, 0xfe, VENDOR,   nullptr },
	{ 0x84, 0xff, OPTIONAL, "read reduced track density" },
	{ 0x85, 0xff, OPTIONAL, "space forward reduced track density" },
	{ 0x86, 0xfe, VENDOR,   nullptr },

	{ 0x88, 0xff, OPTIONAL, "read reverse" },
	{ 0x89, 0xff, OPTIONAL, "space reverse" },
	{ 0x8a, 0xfe, VENDOR,   nullptr },
	{ 0x8c, 0xff, OPTIONAL, "read reverse reduced track density" },
	{ 0x8d, 0xff, OPTIONAL, "space reverse reduced track density" },
	{ 0x8e, 0xfe, VENDOR,   nullptr },

	{ 0x90, 0xf0, VENDOR,   nullptr },

	{ 0xa0, 0xff, STANDARD, "read file mark" },
	{ 0xa1, 0xff, VENDOR,   nullptr },
	{ 0xa2, 0xff, VENDOR,   nullptr },
	{ 0xa3, 0xff, OPTIONAL, "seek end of data" },
	{ 0xa4, 0xff, OPTIONAL, "read file mark reduced track density" },
	{ 0xa5, 0xff, VENDOR,   nullptr },
	{ 0xa6, 0xff, VENDOR,   nullptr },
	{ 0xa7, 0xff, OPTIONAL, "seek end of data reduced track density" },
	{ 0xa8, 0xff, OPTIONAL, "read file mark reverse" },
	{ 0xa9, 0xff, VENDOR,   nullptr },
	{ 0xaa, 0xfe, VENDOR,   nullptr },
	{ 0xac, 0xff, OPTIONAL, "read file mark reverse reduced track density" },
	{ 0xad, 0xff, VENDOR,   nullptr },
	{ 0xae, 0xfe, VENDOR,   nullptr },

	{ 0xb0, 0xf0, OPTIONAL, "read N file marks" },

	{ 0xc0, 0xff, STANDARD, "read status" },
	{ 0xc1, 0xff, OPTIONAL, "read extended status 1" },
	{ 0xc2, 0xff, OPTIONAL, "run self test 1" },
	{ 0xc3, 0xff, VENDOR,   nullptr },
	{ 0xc4, 0xff, OPTIONAL, "read extended status 2" },
	{ 0xc5, 0xff, VENDOR,   nullptr },
	{ 0xc6, 0xfe, VENDOR,   nullptr },
	{ 0xc8, 0xfe, VENDOR,   nullptr },
	{ 0xca, 0xff, OPTIONAL, "run self test 2" },
	{ 0xcb, 0xff, VENDOR,   nullptr },
	{ 0xcc, 0xfc, VENDOR,   nullptr },

	{ 0xd0, 0xf0, VENDOR,   nullptr },

	{ 0xe0, 0xff, OPTIONAL, "read extended status 3" },
	{ 0xe1, 0xff, VENDOR,   nullptr },
	{ 0xe2, 0xfe, VENDOR,   nullptr },
	{ 0xe4, 0xfc, VENDOR,   nullptr },
	{ 0xe8, 0xf0, VENDOR,   nullptr },

	{ 0xf0, 0xf0, VENDOR,   nullptr },
};

namespace {

enum hbc_mask : u8
{
	HBC_ACK = 0x01, // acknowledge
	HBC_RDY = 0x02, // ready
	HBC_EXC = 0x04, // exception
	HBC_DIR = 0x08, // direction
	HBC_ONL = 0x10, // online
	HBC_REQ = 0x20, // request
	HBC_RST = 0x40, // reset
	HBC_XFR = 0x80, // transfer
};

enum state : u8
{
	EXCEPTION,
	EXCEPTION_EXC,
	EXCEPTION_RDY,
	EXCEPTION_CMD,
	EXCEPTION_NRDY,
	EXCEPTION_DAT,
	EXCEPTION_REQ,
	EXCEPTION_NREQ,

	READY,
	COMMAND,
	COMMAND_RDY,
	COMMAND_NREQ,
	COMMAND_NRDY,

	EXECUTE,

	READ,
	READ_BLOCK,
	READ_BLOCK_1,
	READ_BLOCK_2,
	READ_BLOCK_3,
};

class qic02_hle_device
	: public device_t
	, public device_qic02_interface
{
public:
	qic02_hle_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0)
		: device_t(mconfig, QIC02_HLE, tag, owner, clock)
		, device_qic02_interface(mconfig, *this)
		, m_tape(*this, "tape")
		, m_st{}
	{
	}

	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	// device_qic02_interface implementation
	virtual void onl_w(int state) override;
	virtual void req_w(int state) override;
	virtual void rst_w(int state) override;
	virtual void xfr_w(int state) override;

	virtual u8 data_r() override;
	virtual void data_w(u8 data) override;

protected:
	virtual void ack_w(int state) override;
	virtual void rdy_w(int state) override;
	virtual void exc_w(int state) override;
	virtual void dir_w(int state) override;

	void state_timer(int param = 0);
	int state_step();
	int command();

	void exception_po(); // power on/reset
	void exception_nc(); // no cartridge
	void exception_nd(); // no drive
	void exception_ic(); // illegal command
	void exception_fm(); // file mark

	void logstate();

private:
	required_device<simh_tape_image_device> m_tape;

	emu_timer *m_timer;

	u8 m_ctrl; // host bus control
	u8 m_data; // host bus data

	u8 m_select;

	util::fifo<u8, 512> m_fifo;
	u8 m_cmd;

	u8 m_st[6];

	u8 m_state;

	u8 m_buf[512];

	unsigned m_blk = 0;
};

void qic02_hle_device::device_add_mconfig(machine_config &config)
{
	SIMH_TAPE_IMAGE(config, m_tape).set_interface("tape");
}

void qic02_hle_device::device_start()
{
	save_item(NAME(m_ctrl));
	save_item(NAME(m_data));
	save_item(NAME(m_st));

	m_ctrl = 0x00;
	m_data = 0x00;

	m_timer = timer_alloc(timer_expired_delegate(FUNC(qic02_hle_device::state_timer), this));
}

void qic02_hle_device::device_reset()
{
	ack_w(1);
	rdy_w(1);
	exc_w(1);
	dir_w(1);

	exception_po();

	m_select = 1;

	if (m_tape->get_file())
		m_tape->get_file()->rewind(false);
}

void qic02_hle_device::ack_w(int state)
{
	if (state == BIT(m_ctrl, 0))
	{
		LOGMASKED(LOG_CONTROL, "ack_w %d\n", state);

		if (!state)
			m_ctrl |= HBC_ACK;
		else
			m_ctrl &= ~HBC_ACK;

		logstate();
		qic().ack_w(state);
	}
}
void qic02_hle_device::rdy_w(int state)
{
	if (state == BIT(m_ctrl, 1))
	{
		LOGMASKED(LOG_CONTROL, "rdy_w %d\n", state);

		if (!state)
			m_ctrl |= HBC_RDY;
		else
			m_ctrl &= ~HBC_RDY;

		logstate();
		qic().rdy_w(state);
	}
}
void qic02_hle_device::exc_w(int state)
{
	if (state == BIT(m_ctrl, 2))
	{
		LOGMASKED(LOG_CONTROL, "exc_w %d\n", state);

		if (!state)
			m_ctrl |= HBC_EXC;
		else
			m_ctrl &= ~HBC_EXC;

		logstate();
		qic().exc_w(state);
	}
}
void qic02_hle_device::dir_w(int state)
{
	if (state == BIT(m_ctrl, 3))
	{
		LOGMASKED(LOG_CONTROL, "dir_w %d\n", state);

		if (!state)
			m_ctrl |= HBC_DIR;
		else
			m_ctrl &= ~HBC_DIR;

		logstate();
		qic().dir_w(state);
	}
}

void qic02_hle_device::onl_w(int state)
{
	if (state == BIT(m_ctrl, 4))
	{
		LOGMASKED(LOG_CONTROL, "%s: onl_w %d\n", machine().describe_context(), state);
		logerror("%s: onl_w %d\n", machine().describe_context(), state);

		if (!state)
			m_ctrl |= HBC_ONL;
		else
			m_ctrl &= ~HBC_ONL;

		m_timer->adjust(attotime::zero);
		logstate();
	}
}
void qic02_hle_device::req_w(int state)
{
	if (state == BIT(m_ctrl, 5))
	{
		LOGMASKED(LOG_CONTROL, "%s: req_w %d\n", machine().describe_context(), state);

		if (!state)
			m_ctrl |= HBC_REQ;
		else
			m_ctrl &= ~HBC_REQ;

		m_timer->adjust(attotime::zero);
		logstate();
	}
}
void qic02_hle_device::rst_w(int state)
{
	if (state == BIT(m_ctrl, 6))
	{
		LOGMASKED(LOG_CONTROL, "%s: rst_w %d\n", machine().describe_context(), state);

		if (!state)
		{
			m_ctrl |= HBC_RST;

			reset();
		}
		else
			m_ctrl &= ~HBC_RST;

		logstate();
	}
}
void qic02_hle_device::xfr_w(int state)
{
	if (state == BIT(m_ctrl, 7))
	{
		LOGMASKED(LOG_CONTROL, "%s: xfr_w %d\n", machine().describe_context(), state);

		if (!state)
			m_ctrl |= HBC_XFR;
		else
			m_ctrl &= ~HBC_XFR;

		m_timer->adjust(attotime::zero);
		logstate();
	}
}

void qic02_hle_device::state_timer(int param)
{
	int delay = state_step();

	if (delay < 0)
		return;

	m_timer->adjust(attotime::from_usec(delay));
}


int qic02_hle_device::state_step()
{
	int delay = -1;

	// wait REQ
	// clr EXC
	// set RDY

	// wait !REQ
	// clr RDY
	// set DIR

	// loop:
	// set data
	// set RDY

	// wait REQ
	// clr RDY
	// clr data

	// wait !REQ


	switch (m_state)
	{
	case EXCEPTION:
		if (m_ctrl & HBC_REQ)
		{
			m_state = EXCEPTION_EXC;
			delay = 10; // REQ -> RDY >20μs (500μs nominal)
		}
		break;
	case EXCEPTION_EXC:
		if (m_data == 0xc0)
		{
			logerror("status\n");

			exc_w(1);
			m_state = EXCEPTION_RDY;
			delay = 10; // !EXC -> RDY >10μs
		}
		else
			m_state = EXCEPTION;
		break;
	case EXCEPTION_RDY:
		rdy_w(0);
		m_state = EXCEPTION_CMD;
		// wait !REQ
		break;
	case EXCEPTION_CMD:
		if (!(m_ctrl & HBC_REQ))
		{
			m_state = EXCEPTION_NRDY;
			delay = 50; // !REQ -> !RDY 20..100μs
		}
		break;
	case EXCEPTION_NRDY:
		// enqueue current status
		m_fifo.clear();
		logerror("st0 0x%02x st1 0x%02x dec 0x%04x urc 0x%04x\n", m_st[0], m_st[1], u16(m_st[3]) << 8 | m_st[2], u16(m_st[5]) << 8 | m_st[4]);
		for (u8 st : m_st)
			m_fifo.enqueue(st);

		// clear some st0 bits
		m_st[0] &= ~(ST0_ST0 | ST0_UDA | ST0_BNL | ST0_FIL);
		if (m_st[0])
			m_st[0] |= ST0_ST0;

		// clear some st1 bits
		m_st[1] &= ~(ST1_ST1 | ST1_ILL | ST1_NDT | ST1_MBD | ST1_POR);
		if (m_st[1])
			m_st[1] |= ST1_ST1;

		// clear data error counter
		m_st[2] = 0;
		m_st[3] = 0;

		// clear underrun counter
		m_st[4] = 0;
		m_st[5] = 0;

		rdy_w(1);
		dir_w(0);
		m_state = EXCEPTION_DAT;
		delay = 20; // !RDY -> RDY >20μs
		break;

	case EXCEPTION_DAT:
		m_data = m_fifo.dequeue();
		rdy_w(0);
		m_state = EXCEPTION_REQ;
		// wait REQ
		break;
	case EXCEPTION_REQ:
		if (m_ctrl & HBC_REQ)
		{
			rdy_w(1); // REQ -> !RDY <1μs
			m_data = 0;
			m_state = EXCEPTION_NREQ;
			// wait !REQ
		}
		break;
	case EXCEPTION_NREQ:
		if (!(m_ctrl & HBC_REQ))
		{
			m_state = m_fifo.empty() ? READY : EXCEPTION_DAT;
			delay = 50; // !REQ -> RDY 20..100μs
		}
		break;

	case READY:
		dir_w(1);
		rdy_w(0);
		m_state = COMMAND;
		// wait REQ
		break;
	case COMMAND:
		if (m_ctrl & HBC_REQ)
		{
			rdy_w(1); // REQ -> !RDY <1μs
			m_state = COMMAND_RDY;
			delay = 20; // !RDY -> RDY >20μs (500μs nominal)
		}
		break;
	case COMMAND_RDY:
		m_cmd = m_data;
		rdy_w(0);
		m_state = COMMAND_NREQ;
		// wait !REQ
		break;
	case COMMAND_NREQ:
		if (!(m_ctrl & HBC_REQ))
		{
			m_state = COMMAND_NRDY;
			delay = 50; // !REQ -> !RDY 20..100μs
		}
		break;
	case COMMAND_NRDY:
		rdy_w(1);
		m_state = EXECUTE;

		delay = command();
		break;

	case READ_BLOCK:
		{
			auto [status, length] = m_tape->get_file()->read_block(m_buf, std::size(m_buf));

			if (status == tape_status::OK)
			{
				m_fifo.clear();
				for (u8 const byte : m_buf)
					m_fifo.enqueue(byte);

				dir_w(0);
				m_data = m_fifo.dequeue();
				rdy_w(0);

				m_state = READ_BLOCK_1;
				delay = 0;
			}
			else
			{
				logerror("read %u blocks status %u\n", m_blk, u8(status));
				m_blk = 0;

				exception_fm();
#if false
				if (status == tape_status::FILEMARK || status == tape_status::FILEMARK_EW)
					exception_fm();
				else if (status == tape_status::EOD || status == tape_status::EOD_EW)
					exception_eod();
				else if (status == tape_status::EOM)
					exception_eom();
				{
					//if (status == tape_status::EOD)
					//else if (status == tape_status::EOM)
					m_fifo.enqueue(0x00);
					m_fifo.enqueue(0x00);
					m_fifo.enqueue(0x00);
					m_fifo.enqueue(0x00);
					m_fifo.enqueue(0x00);
					m_fifo.enqueue(0x00);

					dir_w(1);
					exc_w(0);

					m_state = EXCEPTION;
				}
#endif
			}
		}
		break;
	case READ_BLOCK_1:
		ack_w(0);
		m_state = READ_BLOCK_2;
		break;
	case READ_BLOCK_2:
		if (m_ctrl & HBC_XFR)
		{
			rdy_w(1);
			ack_w(1);
			m_state = READ_BLOCK_3;
		}
		break;
	case READ_BLOCK_3:
		if (!(m_ctrl & HBC_XFR))
		{
			if (!m_fifo.empty())
			{
				m_data = m_fifo.dequeue();
				ack_w(0);

				m_state = READ_BLOCK_2;
			}
			else
			{
				logerror("read block %u\n", m_blk++);
				m_state = READ_BLOCK;
				delay = 0;
			}
		}
		break;
	}

	return delay;
}

int qic02_hle_device::command()
{
	int delay = -1;

	// wait command complete
	switch (m_cmd)
	{
	case CMD_SELECT1: logerror("select1\n"); m_select = 1; m_state = READY; delay = 20; break;
	case CMD_SELECT2: m_select = 2; m_state = READY; delay = 20; break;
	case CMD_SELECT3: m_select = 3; m_state = READY; delay = 20; break;
	case CMD_SELECT4: m_select = 4; m_state = READY; delay = 20; break;
	case CMD_BOT:
		logerror("bot\n");
		if (m_select == 1)
		{
			if (m_tape->get_file())
			{
				m_tape->get_file()->rewind(false);
				m_state = READY;
				delay = 5'000'000;
			}
			else
				exception_nc();
		}
		else
			exception_nd();
		break;
	//case CMD_ERASE: m_state = READY; delay = 20; break;
	//case CMD_INIT: m_state = READY; delay = 20; break;
	//case CMD_WRITE:
	//case CMD_WFM:
	case CMD_READ:
		logerror("read\n");
		if (m_select == 1)
		{
			if (m_ctrl & HBC_ONL)
			{
				m_fifo.clear();
				m_state = READ_BLOCK;
				delay = 2'500'000;
			}
			else
				exception_ic();
		}
		else
			exception_nd();
		break;
	case CMD_RFM:
		logerror("rfm\n");
		if (m_select == 1)
		{
			if (m_ctrl & HBC_ONL)
			{
				m_tape->get_file()->space_filemarks(1);
				m_state = READY;
				delay = 2'500'000;
			}
			else
				exception_ic();
		}
		else
			exception_nd();
		break;
	case CMD_STATUS:
		logerror("status\n");
		m_state = EXCEPTION_NRDY;
		delay = 50;
		break;
	default:
		exception_ic();
		break;
	}

	return delay;
}

void qic02_hle_device::logstate()
{
	LOGMASKED(LOG_STATE, "host:%s,%s,%s,%s device:%s,%s,%s,%s\n",
		(m_ctrl & HBC_XFR) ? "XFR" : "", (m_ctrl & HBC_RST) ? "RST" : "", (m_ctrl & HBC_REQ) ? "REQ" : "", (m_ctrl & HBC_ONL) ? "ONL" : "",
		(m_ctrl & HBC_DIR) ? "DIR" : "", (m_ctrl & HBC_EXC) ? "EXC" : "", (m_ctrl & HBC_RDY) ? "RDY" : "", (m_ctrl & HBC_ACK) ? "ACK" : "");
}

u8 qic02_hle_device::data_r()
{
	LOGMASKED(LOG_DATA, "%s: data_r 0x%02x\n", machine().describe_context(), m_data);

	return m_data;
}
void qic02_hle_device::data_w(u8 data)
{
	LOGMASKED(LOG_DATA, "%s: data_w 0x%02x\n", machine().describe_context(), data);

	m_data = data;
}

void qic02_hle_device::exception_po()
{
	logerror("exception_po\n");

	m_st[1] |= ST1_ST1 | ST1_POR;

	dir_w(1);
	exc_w(0);

	m_state = EXCEPTION;
}
void qic02_hle_device::exception_nc()
{
	logerror("exception_nc\n");

	// cartridge absent after BOT, RET, ERASE, WRITE, WFM, READ or RFM
	// cartridge removed while drive selected
	m_st[0] |= ST0_ST0 | ST0_CNI;

	dir_w(1);
	exc_w(0);

	m_state = EXCEPTION;
}
void qic02_hle_device::exception_nd()
{
	logerror("exception_nd\n");

	// drive not present after BOT, RET, ERASE, WRITE, WFM, READ or WFM
	m_st[0] |= ST0_ST0 | ST0_CNI | ST0_USL | ST0_WRP;

	dir_w(1);
	exc_w(0);

	m_state = EXCEPTION;
}
void qic02_hle_device::exception_ic()
{
	logerror("exception_ic\n");

	m_st[1] |= ST1_ST1 | ST1_ILL;

	dir_w(1);
	exc_w(0);

	m_state = EXCEPTION;
}
void qic02_hle_device::exception_fm()
{
	logerror("exception_fm\n");

	m_st[0] |= ST0_ST0 | ST0_FIL;

	dir_w(1);
	exc_w(0);

	m_state = EXCEPTION;
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(QIC02_HLE, device_qic02_interface, qic02_hle_device, "qic02_hle", "QIC-02 Controller (HLE)")
