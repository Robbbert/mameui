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
		OB_MEM = 4,
		OB_PIO = 5,
		P1_MEM = 6,
		P1_PIO = 7
	};
	template <typename T> void set_ob_mem(T &&tag, int spacenum) { m_ob_mem.set_tag(std::forward<T>(tag), spacenum); }
	template <typename T> void set_p1_mem(T &&tag, int spacenum) { m_p1m.set_tag(std::forward<T>(tag), spacenum); }
	template <typename T> void set_p1_pio(T &&tag, int spacenum) { m_p1i.set_tag(std::forward<T>(tag), spacenum); }
	template <typename T> void set_boot_prom(T &&tag) { m_boot.set_tag(std::forward<T>(tag)); }

	auto berr() { return m_berr.bind(); }

	sun2_mmu_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	u16 mmu_r(u8 fc, offs_t offset, u16 mem_mask);
	void mmu_w(u8 fc, offs_t offset, u16 data, u16 mem_mask);

	u16 context_r();
	void context_w(offs_t offset, u16 data, u16 mem_mask);
	u8 segment_r(offs_t offset);
	void segment_w(offs_t offset, u8 data);
	u16 page_r(offs_t offset);
	void page_w(offs_t offset, u16 data);

	u16 buserror_r();

	u32 lookup(u8 fc, offs_t offset, bool write);

protected:
	void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual space_config_vector memory_space_config() const override ATTR_COLD;
	//virtual space_config_vector memory_logical_space_config() const;
	//virtual bool memory_translate(int spacenum, int intention, offs_t &address, address_space *&target_space);

private:
	//address_space_config m_ob_mem;
	required_address_space m_ob_mem;
	address_space_config m_ob_pio;

	required_address_space m_p1m;
	required_address_space m_p1i;

	required_region_ptr<u16> m_boot;

	devcb_write8 m_berr;

	memory_access<24, 1, 0, ENDIANNESS_LITTLE>::specific m_bus_mem;
	memory_access<16, 1, 0, ENDIANNESS_LITTLE>::specific m_bus_pio;

	u16 m_context;
	std::unique_ptr<u8[][512]> m_segment;
	std::unique_ptr<u32[][16]> m_page;
	u16 m_buserror;
};

DECLARE_DEVICE_TYPE(SUN2_MMU, sun2_mmu_device)

#endif // MAME_SUN_SUN2_MMU_H
