// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

#include "emu.h"
#include "sun2_mmu.h"

//#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

enum page_mask : u32
{
	PAGE_PPN  = 0x000f'ffff, // physical page number
	PAGE_M    = 0x0010'0000, // modified
	PAGE_A    = 0x0020'0000, // accessed
	PAGE_TYPE = 0x01c0'0000, // type
	PAGE_PROT = 0x7e00'0000, // protection
	PAGE_V    = 0x8000'0000, // valid
};
enum berr_mask : u16
{
	BERR_PARERRL = 0x0001,
	BERR_PARERRH = 0x0002,
	BERR_TIMEOUT = 0x0004,
	BERR_PROTERR = 0x0008,
	BERR_PGVALID = 0x0080,
};

DEFINE_DEVICE_TYPE(SUN2_MMU, sun2_mmu_device, "sun2_mmu", "Sun Microsystems Sun-2 MMU")

sun2_mmu_device::sun2_mmu_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: device_t(mconfig, SUN2_MMU, tag, owner, clock)
	, device_memory_interface(mconfig, *this)
	//, m_ob_mem("ob_mem", ENDIANNESS_BIG, 16, 23)
	, m_ob_mem(*this, finder_base::DUMMY_TAG, 0)
	, m_ob_pio("ob_pio", ENDIANNESS_BIG, 16, 14)
	, m_p1m(*this, finder_base::DUMMY_TAG, 0)
	, m_p1i(*this, finder_base::DUMMY_TAG, 0)
	, m_boot(*this, finder_base::DUMMY_TAG)
	, m_berr(*this)
	, m_context(0)
	, m_segment(nullptr)
	, m_page(nullptr)
	, m_buserror(0)
{
}

void sun2_mmu_device::device_add_mconfig(machine_config &config)
{
}

void sun2_mmu_device::device_start()
{
	m_segment = std::make_unique<u8[][512]>(8);
	m_page = std::make_unique<u32[][16]>(256);

	save_item(NAME(m_context));
	save_pointer(NAME(m_segment), 8);
	save_pointer(NAME(m_page), 256);

	m_p1m->specific(m_bus_mem);
	m_p1i->specific(m_bus_pio);
}

void sun2_mmu_device::device_reset()
{
}

device_memory_interface::space_config_vector sun2_mmu_device::memory_space_config() const
{
	return space_config_vector{ { OB_PIO, &m_ob_pio } };
}

u16 sun2_mmu_device::context_r()
{
	return m_context;
}
void sun2_mmu_device::context_w(offs_t offset, u16 data, u16 mem_mask)
{
	LOG("%s: context_w 0x%04x\n", machine().describe_context(), data);

	m_context = (m_context & ~mem_mask) | (data & mem_mask & 0x0707);
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
// ssss'ssss'sppp'p...'....'....
// 0000'0000'0000'1000'0000'0000

u16 sun2_mmu_device::page_r(offs_t offset)
{
	u8 const pmeg = m_segment[m_context & 7][BIT(offset, 15, 9)];
	u32 const page = m_page[pmeg][BIT(offset, 11, 4)];

	if (offset & 2)
		return page;
	else
		return page >> 16;
}
void sun2_mmu_device::page_w(offs_t offset, u16 data)
{
	u8 const pmeg = m_segment[m_context & 7][BIT(offset, 15, 9)];
	u32 &page = m_page[pmeg][BIT(offset, 11, 4)];

	if (offset & 2)
		page = (page & 0xffff'0000U) | data;
	else
		page = u32(data) << 16 | (page & 0x0000'ffffU);

	LOG("%s: page_w[%u][0x%03x] 0x%08x\n", machine().describe_context(), pmeg, BIT(offset, 11, 4), page);
}

u32 sun2_mmu_device::lookup(u8 fc, offs_t offset, bool write)
{
	u8 const context = ((fc & 4) ? m_context >> 8 : m_context) & 7;
	u8 const pmeg = m_segment[context][BIT(offset, 15, 9)];

	u32 &page = m_page[pmeg][BIT(offset, 11, 4)];

	//LOG("%s: lookup[%u][0x%03x] 0x%08x\n", machine().describe_context(), pmeg, BIT(offset, 11, 4), page);

	if (!machine().side_effects_disabled())
	{
		if ((page & PAGE_V))
		{
			if (write)
				page |= PAGE_A | PAGE_M;
			else
				page |= PAGE_A;

			m_buserror |= BERR_PGVALID;
		}
		else
			m_buserror &= ~BERR_PGVALID;
	}

	return page;
}

u16 sun2_mmu_device::mmu_r(u8 fc, offs_t offset, u16 mem_mask)
{
	u32 const page = lookup(fc, offset, false);

	if (page & PAGE_V)
	{
		offs_t const pa = BIT(page, 0, 12) << 11 | (offset & 0x7ff);

		switch (BIT(page, 22, 3))
		{
		case 0: return m_ob_mem->read_word(pa, mem_mask);
		case 1:
			if (pa < m_boot.bytes())
				return m_boot[(offset & (m_boot.bytes() - 1)) >> 1];
			else
				return space(OB_PIO).read_word(pa, mem_mask);
		case 2:
		{
			auto [data, flags] = m_bus_mem.read_word_flags(pa, mem_mask);

			if (flags)
			{
				m_buserror |= BERR_TIMEOUT;
				m_berr(offset << 1, 2);	// 3=read fault, 2=read error, 1=write fault, 0=write error

			}

			return data;
		}
		case 3:
		{
			auto [data, flags] = m_bus_pio.read_word_flags(pa, mem_mask);

			if (flags)
			{
				m_buserror |= BERR_TIMEOUT;
				m_berr(offset << 1, 2);
			}

			return data;
		}
		}
	}
	else
		m_berr(offset << 1, 3);

	return 0;
}
void sun2_mmu_device::mmu_w(u8 fc, offs_t offset, u16 data, u16 mem_mask)
{
	u32 const page = lookup(fc, offset, true);

	if (page & PAGE_V)
	{
		offs_t const pa = BIT(page, 0, 12) << 11 | (offset & 0x7ff);

		switch (BIT(page, 22, 3))
		{
		case 0: m_ob_mem->write_word(pa, data, mem_mask); break;
		case 1: space(OB_PIO).write_word(pa, data, mem_mask); break;
		case 2:
			if (m_bus_mem.write_word_flags(pa, data, mem_mask))
			{
				m_buserror |= BERR_TIMEOUT;
				m_berr(offset << 1, 0);
			}
			break;
		case 3:
			if (m_bus_pio.write_word_flags(offset << 1, data, mem_mask))
			{
				m_buserror |= BERR_TIMEOUT;
				m_berr(offset << 1, 0);
			}
			break;
		}
	}
	else
		m_berr(offset << 1, 1);
}

u16 sun2_mmu_device::buserror_r()
{
	return m_buserror;
}
