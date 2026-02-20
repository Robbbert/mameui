// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

#ifndef MAME_SGI_UC4_H
#define MAME_SGI_UC4_H

#pragma once

#include "bus/multibus/multibus.h"

#include "dc4.h"

class sgi_uc4_device
	: public device_t
	, public device_multibus_interface
{
public:
	template <typename T> void set_dc(T &&tag) { m_dc.set_tag(std::forward<T>(tag)); }

	sgi_uc4_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock);

	static constexpr feature_type imperfect_features() { return feature::GRAPHICS; }

protected:
	//virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	//virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	u16 buf_r(offs_t offset);
	void buf_w(offs_t offset, u16 data, u16 mem_mask);
	u16 ucr_r();
	void ucr_w(u16 data);
	u16 cmd_r(offs_t offset);
	void cmd_w(offs_t offset, u16 data, u16 mem_mask);

private:
	memory_share_creator<u16> m_font;
	optional_device<sgi_dc4_device> m_dc;

	u16 m_ed; // error delta
	u16 m_ec; // error correct
	u16 m_xs; // x start
	u16 m_xe; // x end
	u16 m_ys; // y start
	u16 m_ye; // y end
	u16 m_fma; // font memory address
	u16 m_da[4]; // dda address
	u16 m_dd[4]; // dda delta
	u8 m_md; // mode
	u8 m_rp; // repeat
	u16 m_cf; // config

	u16 m_ucr;

	u16 m_color;
	u16 m_we;

	bool m_installed;
};

DECLARE_DEVICE_TYPE(SGI_UC4, sgi_uc4_device)

#endif // MAME_SGI_UC4_H
