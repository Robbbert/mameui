// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

#ifndef MAME_SUN_SUN2_MMU_H
#define MAME_SUN_SUN2_MMU_H

#pragma once

class sun2_mmu_device
	: public device_t
	, public device_memory_interface
{
public:
	enum spaces : int
	{
		OBM = 4, // on-board memory (P2)
		OBI = 5, // on-board I/O
	};
	enum berr_type : u8
	{
		FAULT = 1,
		READ  = 2,
	};
	template <typename T> void set_boot_prom(T &&tag) { m_boot.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_p1m(T &&tag, int spacenum) { m_p1m_as.set_tag(std::forward<T>(tag), spacenum); }
	template <typename T> void set_p1i(T &&tag, int spacenum) { m_p1i_as.set_tag(std::forward<T>(tag), spacenum); }

	auto berr() { return m_berr_cb.bind(); }

	sun2_mmu_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	// translated accessors
	u16 mmu_r(u8 fc, offs_t offset, u16 mem_mask);
	void mmu_w(u8 fc, offs_t offset, u16 data, u16 mem_mask);
	u16 dvma_r(offs_t offset, u16 mem_mask);
	void dvma_w(offs_t offset, u16 data, u16 mem_mask); 

	// registers
	u16 context_r();
	void context_w(offs_t offset, u16 data, u16 mem_mask);
	u8 segment_r(offs_t offset);
	void segment_w(offs_t offset, u8 data);
	u16 page_r(offs_t offset);
	void page_w(offs_t offset, u16 data, u16 mem_mask);
	u16 buserror_r();

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual space_config_vector memory_space_config() const override ATTR_COLD;

	std::tuple<u32 &, offs_t> translate(bool super, offs_t va);
	void berr_w(offs_t offset, u16 data, u8 flags);

private:
	address_space_config m_obm_cfg;
	address_space_config m_obi_cfg;

	required_address_space m_p1m_as;
	required_address_space m_p1i_as;
	required_region_ptr<u16> m_boot;

	devcb_write8 m_berr_cb;

	memory_access<23, 1, 0, ENDIANNESS_BIG>::specific m_obm;    // on-board memory (Multibus P2)
	memory_access<14, 1, 0, ENDIANNESS_BIG>::specific m_obi;    // on-board I/O
	memory_access<24, 1, 0, ENDIANNESS_LITTLE>::specific m_p1m; // Multibus memory
	memory_access<16, 1, 0, ENDIANNESS_LITTLE>::specific m_p1i; // Multibus I/O

	u16 m_context;
	std::unique_ptr<u8[][512]> m_segment;
	std::unique_ptr<u32[][16]> m_page;
	u16 m_berr;
};

DECLARE_DEVICE_TYPE(SUN2_MMU, sun2_mmu_device)

#endif // MAME_SUN_SUN2_MMU_H
