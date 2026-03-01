// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
#ifndef MAME_CPU_E132XS_E132XSFE_H
#define MAME_CPU_E132XS_E132XSFE_H

#pragma once

#include "e132xs.h"

#include "cpu/drcfe.h"

#include <algorithm>
#include <bitset>


class hyperstone_device::opcode_desc : public opcode_desc_base<opcode_desc, 32>
{
public:
	uint16_t        opptr[3];               // copy of up to 3 halfwords of opcode

	void set_can_change_modes() { m_extra_flags.set(CAN_CHANGE_MODES); }
	void set_reads_memory() { m_extra_flags.set(READS_MEMORY); }
	void set_writes_memory() { m_extra_flags.set(WRITES_MEMORY); }

	bool can_change_modes() const { return m_extra_flags[CAN_CHANGE_MODES]; }
	bool reads_memory() const { return m_extra_flags[READS_MEMORY]; }
	bool writes_memory() const { return m_extra_flags[WRITES_MEMORY]; }

	// epc - compute the exception PC
	uint32_t epc() const
	{
		return in_delay_slot() ? branch->pc : pc;
	}

	void reset(offs_t curpc, bool in_delay_slot)
	{
		opcode_desc_base::reset(curpc, in_delay_slot);

		std::fill(std::begin(opptr), std::end(opptr), 0);
		m_extra_flags.reset();
	}

protected:
	enum
	{
		CAN_CHANGE_MODES = 0,
		READS_MEMORY,
		WRITES_MEMORY,

		EXTRA_FLAG_COUNT
	};

	std::bitset<EXTRA_FLAG_COUNT> m_extra_flags;
};


class hyperstone_device::frontend : public drc_frontend_base<opcode_desc>
{
public:
	frontend(hyperstone_device &cpu, uint32_t window_start, uint32_t window_end, uint32_t max_sequence);
	~frontend();

	opcode_desc const *describe_code(offs_t startpc);

private:
	bool describe(opcode_desc &desc, opcode_desc const *prev);

	uint16_t read_word(opcode_desc &desc);
	uint16_t read_imm1(opcode_desc &desc);
	uint16_t read_imm2(opcode_desc &desc);
	uint32_t read_ldstxx_imm(opcode_desc &desc);
	uint32_t read_limm(opcode_desc &desc, uint16_t op);
	int32_t decode_pcrel(opcode_desc &desc, uint16_t op);
	int32_t decode_call(opcode_desc &desc);

	hyperstone_device &m_cpu;
};

#endif // MAME_CPU_E132XS_E132XSFE_H
