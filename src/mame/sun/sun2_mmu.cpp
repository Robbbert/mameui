// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * Sun-2 Memory Management Unit
 *
 * Sources:
 *  - Sun-2 Architecture Manual, Draft Version 0.5, 15 December 1983, Sun Microsystems Inc.
 *
 * TODO:
 *  - DVMA valid/protection failures
 */

#include "emu.h"
#include "sun2_mmu.h"

//#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

// 0xf08001ff

enum page_mask : u32
{
	PAGE_PPN  = 0x000f'ffff, // physical page number
	PAGE_M    = 0x0010'0000, // modified
	PAGE_A    = 0x0020'0000, // accessed
	PAGE_TYPE = 0x01c0'0000, // type
	PAGE_PROT = 0x7e00'0000, // protection
	PAGE_V    = 0x8000'0000, // valid
};
enum prot_bits : unsigned
{
	S_R = 30, // supervisor read
	S_W = 29, // supervisor write
	S_X = 28, // supervisor execute
	U_R = 27, // user read
	U_W = 26, // user write
	U_X = 25, // user execute
};
enum berr_mask : u16
{
	BERR_PARERRL = 0x0001, // parity error low byte
	BERR_PARERRH = 0x0002, // parity error high byte
	BERR_TIMEOUT = 0x0004, // timeout error
	BERR_PROTERR = 0x0008, // protection error
	BERR_PGVALID = 0x0080, // valid page
};

DEFINE_DEVICE_TYPE(SUN2_MMU, sun2_mmu_device, "sun2_mmu", "Sun-2 Memory Management Unit")

sun2_mmu_device::sun2_mmu_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: device_t(mconfig, SUN2_MMU, tag, owner, clock)
	, device_memory_interface(mconfig, *this)
	, m_obm_cfg("obm", ENDIANNESS_BIG, 16, 23)
	, m_obi_cfg("obi", ENDIANNESS_BIG, 16, 14)
	, m_p1m_as(*this, finder_base::DUMMY_TAG, -1)
	, m_p1i_as(*this, finder_base::DUMMY_TAG, -1)
	, m_boot(*this, finder_base::DUMMY_TAG)
	, m_berr_cb(*this)
	, m_segment(nullptr)
	, m_page(nullptr)
{
}

void sun2_mmu_device::device_start()
{
	m_segment = std::make_unique<u8[][512]>(8);
	m_page = std::make_unique<u32[][16]>(256);

	save_item(NAME(m_context));
	save_pointer(NAME(m_segment), 8);
	save_pointer(NAME(m_page), 256);
	save_item(NAME(m_berr));

	space(OBM).specific(m_obm);
	space(OBI).specific(m_obi);
	m_p1m_as->specific(m_p1m);
	m_p1i_as->specific(m_p1i);
}

void sun2_mmu_device::device_reset()
{
	m_context = 0;
	m_berr = 0;
}

device_memory_interface::space_config_vector sun2_mmu_device::memory_space_config() const
{
	return space_config_vector{ { OBM, &m_obm_cfg }, { OBI, &m_obi_cfg } };
}

u16 sun2_mmu_device::context_r()
{
	return m_context;
}

void sun2_mmu_device::context_w(offs_t offset, u16 data, u16 mem_mask)
{
	LOG("%s: context_w 0x%04 mask 0x%04xx\n", machine().describe_context(), data, mem_mask);

	COMBINE_DATA(&m_context);
}

u8 sun2_mmu_device::segment_r(offs_t offset)
{
	return m_segment[m_context & 7][BIT(offset, 15, 9)];
}

void sun2_mmu_device::segment_w(offs_t offset, u8 data)
{
	LOG("%s: segment_w[%u][0x%03x] 0x%02x\n", machine().describe_context(), m_context & 7, BIT(offset, 15, 9), data);

	m_segment[m_context & 7][BIT(offset, 15, 9)] = data;
}

u16 sun2_mmu_device::page_r(offs_t offset)
{
	u8 const pmeg = m_segment[m_context & 7][BIT(offset, 15, 9)];
	u32 const page = m_page[pmeg][BIT(offset, 11, 4)];

	if (offset & 2)
		return page;
	else
		return page >> 16;
}

void sun2_mmu_device::page_w(offs_t offset, u16 data, u16 mem_mask)
{
	u8 const pmeg = m_segment[m_context & 7][BIT(offset, 15, 9)];
	u32 &page = m_page[pmeg][BIT(offset, 11, 4)];

	if (offset & 2)
		page = (page & ~u32(mem_mask)) | (data & mem_mask);
	else
		page = u32(data & mem_mask) << 16 | (page & ~(u32(mem_mask) << 16));

	LOG("%s: page_w[%u][0x%03x] 0x%08x\n", machine().describe_context(), pmeg, BIT(offset, 11, 4), page);
}

u16 sun2_mmu_device::mmu_r(u8 fc, offs_t offset, u16 mem_mask)
{
	auto [page, pa] = translate(fc & 4, offset);

	// check valid
	if (!(page & PAGE_V))
	{
		berr_w(offset, BERR_PROTERR, READ | FAULT);
		return 0;
	}

	// check access
	static constexpr unsigned rx[] = { U_R, U_X, S_R, S_X };
	if (!BIT(page, rx[fc >> 1]))
	{
		berr_w(offset, BERR_PGVALID | BERR_PROTERR, READ | FAULT);
		return 0;
	}

	if (!machine().side_effects_disabled())
		page |= PAGE_A;

	switch (BIT(page, 22, 2))
	{
	case 0:
		// on-board memory
		return m_obm.read_word(pa, mem_mask);

	case 1:
		// on-board I/O
		if (pa < 0x800)
			// boot PROM accessed using virtual address via page map
			return m_boot[(offset & (m_boot.bytes() - 1)) >> 1];
		else
			return m_obi.read_word(pa, mem_mask);

	case 2:
		// Multibus memory
		// TODO: check for dvma self-reference
		{
			auto [data, flags] = m_p1m.read_word_flags(pa, mem_mask);

			if (flags)
				berr_w(offset, BERR_PGVALID | BERR_TIMEOUT, READ);

			return data;
		}

	case 3:
		// Multibus I/O
		{
			auto [data, flags] = m_p1i.read_word_flags(pa, mem_mask);

			if (flags)
				berr_w(offset, BERR_PGVALID | BERR_TIMEOUT, READ);

			return data;
		}
	}

	// can't happen
	abort();
}

void sun2_mmu_device::mmu_w(u8 fc, offs_t offset, u16 data, u16 mem_mask)
{
	auto [page, pa] = translate(fc & 4, offset);

	// check valid
	if (!(page & PAGE_V))
	{
		berr_w(offset, BERR_PROTERR, FAULT);
		return;
	}

	// check access
	static constexpr unsigned w[] = { U_W, S_W };
	if (!BIT(page, w[fc >> 2]))
	{
		berr_w(offset, BERR_PGVALID | BERR_PROTERR, FAULT);
		return;
	}

	if (!machine().side_effects_disabled())
		page |= PAGE_A | PAGE_M;

	switch (BIT(page, 22, 2))
	{
	case 0:
		// on-board memory
		m_obm.write_word(pa, data, mem_mask);
		break;

	case 1:
		// on-board I/O
		m_obi.write_word(pa, data, mem_mask);
		break;

	case 2:
		// Multibus memory
		// TODO: check for dvma self-reference
		if (m_p1m.write_word_flags(pa, data, mem_mask))
			berr_w(offset, BERR_PGVALID | BERR_TIMEOUT, 0);
		break;

	case 3:
		// Multibus I/O
		if (m_p1i.write_word_flags(pa, data, mem_mask))
			berr_w(offset, BERR_PGVALID | BERR_TIMEOUT, 0);
		break;
	}
}

u16 sun2_mmu_device::dvma_r(offs_t offset, u16 mem_mask)
{
	auto [page, pa] = translate(true, 0xf0'0000U | offset << 1);

	// check valid
	if (!(page & PAGE_V))
		return 0;

	// check access
	if (!BIT(page, S_R))
		return 0;

	if (!machine().side_effects_disabled())
		page |= PAGE_A;

	if (BIT(page, 22, 2) == 0)
		// on-board memory
		return m_obm.read_word(pa, mem_mask);
	else
		return 0;
}

void sun2_mmu_device::dvma_w(offs_t offset, u16 data, u16 mem_mask)
{
	auto [page, pa] = translate(true, 0xf0'0000U | offset << 1);

	// check valid
	if (!(page & PAGE_V))
		return;

	// check access
	if (!BIT(page, S_W))
		return;

	if (!machine().side_effects_disabled())
		page |= PAGE_A | PAGE_M;

	if (BIT(page, 22, 2) == 0)
		// on-board memory
		m_obm.write_word(pa, data, mem_mask);
}

u16 sun2_mmu_device::buserror_r()
{
	return m_berr;
}

std::tuple<u32 &, offs_t> sun2_mmu_device::translate(bool super, offs_t va)
{
	unsigned const context = BIT(m_context, super ? 8 : 0, 3);
	u8 const pmeg = m_segment[context][BIT(va, 15, 9)];

	u32 &page = m_page[pmeg][BIT(va, 11, 4)];

	return { page, (page & PAGE_PPN) << 11 | (va & 0x7ff) };
}

void sun2_mmu_device::berr_w(offs_t offset, u16 data, u8 flags)
{
	if (!machine().side_effects_disabled())
	{
		m_berr = data;

		m_berr_cb(offset, flags);
	}
}
