// license:BSD-3-Clause
// copyright-holders:Angelo Salese

#ifndef MAME_VIDEO_PC_VGA_SIS_H
#define MAME_VIDEO_PC_VGA_SIS_H

#pragma once

#include "video/pc_vga.h"

class sis6326_vga_device : public svga_device
{
public:
	// Chipset for AGP card, enough for BIOS checks and nothing else (cfr. SDD tests)
	static constexpr feature_type imperfect_features() { return feature::GRAPHICS; }

	sis6326_vga_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	virtual uint8_t mem_r(offs_t offset) override;
	virtual void mem_w(offs_t offset, uint8_t data) override;

	// Configuration pins
	// AGP bus enable
	auto md20_cb() { return m_md20_cb.bind(); }
	// AGP 2X transfer mode
	auto md21_cb() { return m_md21_cb.bind(); }
	// Enable 64K ROM
	auto md23_cb() { return m_md23_cb.bind(); }

protected:
	sis6326_vga_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void io_3cx_map(address_map &map) override ATTR_COLD;

	virtual void crtc_map(address_map &map) override ATTR_COLD;
	virtual void sequencer_map(address_map &map) override ATTR_COLD;
	virtual void tvout_map(address_map &map) ATTR_COLD;

	virtual uint16_t offset() override;
	virtual void recompute_params() override;
	virtual space_config_vector memory_space_config() const override;

	address_space_config m_tvout_space_config;

	devcb_read_line m_md20_cb;
	devcb_read_line m_md21_cb;

	devcb_read_line m_md23_cb;

	u8 m_ramdac_mode = 0;
	u8 m_ext_sr07;
	u8 m_ext_sr0b;
	u8 m_ext_sr0c;
	u8 m_ext_ddc;
	u8 m_ext_sr23;
	u8 m_ext_sr33;
	u8 m_ext_sr34;
	u8 m_ext_sr35;
	u8 m_ext_sr38;
	u8 m_ext_sr39;
	u8 m_ext_sr3c;
	u8 m_ext_ge26;
	u8 m_ext_ge27;
	//u16 m_ext_config_status = 0;
	u8 m_ext_scratch[5]{};
	u8 m_ext_vert_overflow = 0;
	u8 m_ext_horz_overflow[2]{};
	u8 m_bus_width = 0;
	u8 m_ext_dclk[3]{};
	u8 m_ext_eclk[3]{};
	u8 m_ext_clock_gen = 0;
	u8 m_ext_clock_source_select = 0;
	bool m_crtc_unlock_reg = false;
	bool m_seq_unlock_reg = false;
	u8 m_linear_address[2];

	u8 m_tvout_index;
	struct {
		u8 control;

		u16 pycin;
		bool enyf, encf, tvsense;
	} m_tv;

	virtual uint32_t latch_start_addr() override;
	virtual std::tuple<u8, u8> flush_true_color_mode();
	// TODO: 1024x768x16bpp wants it, mapped odd/even
	//virtual bool get_interlace_mode() override { return BIT(m_ramdac_mode, 5); }
};

class sis630_vga_device : public sis6326_vga_device
{
public:
	sis630_vga_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	//virtual void device_start() override ATTR_COLD;
	//virtual void device_reset() override ATTR_COLD;

	virtual void crtc_map(address_map &map) override ATTR_COLD;
	virtual void sequencer_map(address_map &map) override ATTR_COLD;

	virtual std::tuple<u8, u8> flush_true_color_mode() override;
};

DECLARE_DEVICE_TYPE(SIS6326_VGA, sis6326_vga_device)
DECLARE_DEVICE_TYPE(SIS630_VGA, sis630_vga_device)

#endif // MAME_VIDEO_PC_VGA_SIS_H
