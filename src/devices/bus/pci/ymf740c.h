// license:BSD-3-Clause
// copyright-holders:

#ifndef MAME_BUS_PCI_YMF740C_H
#define MAME_BUS_PCI_YMF740C_H

#pragma once

#include "pci_slot.h"

class ymf740c_device : public pci_card_device
{
public:
	ymf740c_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	static constexpr feature_type unemulated_features() { return feature::SOUND; }

protected:
	ymf740c_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_add_mconfig(machine_config &config) override;

	virtual void config_map(address_map &map) override;
	virtual u8 capptr_r() override;
private:
	void map(address_map &map);
};

DECLARE_DEVICE_TYPE(YMF740C, ymf740c_device)

#endif // MAME_BUS_PCI_YMF740C_H
