// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * Wangtek Series 5099EQ/5125EQ/5150EQ QIC-02 Interface Streaming 1/4 Inch Tape Cartridge Drive
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
#include "emu.h"
#include "wangtek_5150eq.h"

#include "cpu/i8085/i8085.h"
#include "machine/i8257.h"

#include "imagedev/simh_tape_image.h"

#define LOG_STATE   (1U << 1)
#define LOG_CONTROL (1U << 2)
#define LOG_DATA    (1U << 3)

#define VERBOSE (LOG_GENERAL) //|LOG_STATE|LOG_CONTROL|LOG_DATA)
#include "logmacro.h"

namespace {

class wangtek_5150eq_device
	: public device_t
	, public device_qic02_interface
{
public:
	wangtek_5150eq_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0)
		: device_t(mconfig, WANGTEK_5150EQ, tag, owner, clock)
		, device_qic02_interface(mconfig, *this)
		, m_cpu(*this, "cpu")
		, m_dma(*this, "dma")
		, m_tape(*this, "tape")
	{
	}

	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	// device_qic02_interface implementation
	virtual void onl_w(int state) override {}
	virtual void req_w(int state) override {}
	virtual void rst_w(int state) override {}
	virtual void xfr_w(int state) override {}

	virtual u8 data_r() override { return 0; }
	virtual void data_w(u8 data) override {}

protected:
	virtual void ack_w(int state) override {}
	virtual void rdy_w(int state) override {}
	virtual void exc_w(int state) override {}
	virtual void dir_w(int state) override {}

	void cpu_mem(address_map &map);
	void cpu_pio(address_map &map);

private:
	required_device<i8085a_cpu_device> m_cpu;
	required_device<i8257_device> m_dma;

	required_device<simh_tape_image_device> m_tape;

};

void wangtek_5150eq_device::device_add_mconfig(machine_config &config)
{
	I8085A(config, m_cpu, 7'200'200); // 18MHz / 2.5 (by PLL chip)?
	m_cpu->set_addrmap(AS_PROGRAM, &wangtek_5150eq_device::cpu_mem);
	m_cpu->set_addrmap(AS_IO, &wangtek_5150eq_device::cpu_pio);

	I8257(config, m_dma, 7'200'200 / 2);

	// U31 Wangtek custom write IC
	// U25 erase driver
	// TL041 read signal processor
	// BOT, EOT, LP, EW sensors

	SIMH_TAPE_IMAGE(config, m_tape).set_interface("tape");
}

void wangtek_5150eq_device::device_start()
{
}

void wangtek_5150eq_device::device_reset()
{
	if (m_tape->get_file())
		m_tape->get_file()->rewind(false);
}

void wangtek_5150eq_device::cpu_mem(address_map &map)
{
	map(0x0000, 0x3fff).rom().region("cpu", 0);
	map(0x4000, 0x6000).ram(); // CXK5864BM-10L
}
void wangtek_5150eq_device::cpu_pio(address_map &map)
{
}

ROM_START(wangtek_5150eq)
	ROM_REGION(0x4000, "cpu", 0)
	ROM_LOAD("32988_005__d60b.u17", 0x0000, 0x4000, CRC(9ad1a072) SHA1(ee97ca0971e707558a5ae02d173ca0d61cb263c3))
ROM_END

const tiny_rom_entry *wangtek_5150eq_device::device_rom_region() const
{
	return ROM_NAME(wangtek_5150eq);
}

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(WANGTEK_5150EQ, device_qic02_interface, wangtek_5150eq_device, "wangtek_5150eq", "Wangtek 5150EQ")
