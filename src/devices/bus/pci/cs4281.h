// license:BSD-3-Clause
// copyright-holders:

#ifndef MAME_BUS_PCI_CS4281_H
#define MAME_BUS_PCI_CS4281_H

#pragma once

#include "pci_slot.h"

class cs4281_device : public pci_card_device
{
public:
	cs4281_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	static constexpr feature_type unemulated_features() { return feature::SOUND; }

protected:
	cs4281_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_add_mconfig(machine_config &config) override;

	virtual void config_map(address_map &map) override;
	virtual u8 capptr_r() override;
private:
	void io_map(address_map &map);
	void mmio_map(address_map &map);
};

DECLARE_DEVICE_TYPE(CS4281, cs4281_device)

#endif // MAME_BUS_PCI_CS4281_H
