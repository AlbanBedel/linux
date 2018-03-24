/*
 *  Atheros AR71xx built-in ethernet mac driver
 *
 *  Copyright (C) 2008-2010 Gabor Juhos <juhosg@openwrt.org>
 *  Copyright (C) 2008 Imre Kaloz <kaloz@openwrt.org>
 *
 *  Based on Atheros' AG7100 driver
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/phy.h>
#include <linux/clk.h>
#include <linux/io.h>

#define AG71XX_MDIO_RATE	2500000
#define AG71XX_MDIO_RETRY	1000
#define AG71XX_MDIO_DELAY	5

#define AG71XX_REG_MII_CFG	0x0020
#define AG71XX_REG_MII_CMD	0x0024
#define AG71XX_REG_MII_ADDR	0x0028
#define AG71XX_REG_MII_CTRL	0x002c
#define AG71XX_REG_MII_STATUS	0x0030
#define AG71XX_REG_MII_IND	0x0034

#define MII_CMD_WRITE		0x0
#define MII_CMD_READ		0x1
#define MII_ADDR_SHIFT		8
#define MII_IND_BUSY		BIT(0)
#define MII_IND_INVALID		BIT(2)

#define MII_CFG_RESET		BIT(31)

struct ag71xx_mdio_hw {
	const u32		*div_table;
	unsigned int		div_table_size;
};

struct ag71xx_mdio {
	struct mii_bus		*mii_bus;
	void __iomem		*mdio_base;
	struct clk		*ref_clk;
	u32			mdio_rate;
	const struct ag71xx_mdio_hw *hw;
};

// TODO: Use a regmap that does the flush
static inline void ag71xx_mdio_wr(struct ag71xx *ag, unsigned reg,
				  u32 value)
{
	__raw_writel(value, am->mdio_base + reg);

	/* flush write */
	(void) __raw_readl(am->mdio_base + reg);
}

static inline u32 ag71xx_mdio_rr(struct ag71xx *ag, unsigned reg)
{
	return __raw_readl(am->mdio_base + reg);
}

static void ag71xx_mdio_dump_regs(struct ag71xx *ag)
{
	dev_dbg(&am->mii_bus->dev,
		"mii_cfg=%08x, mii_cmd=%08x, mii_addr=%08x\n",
		ag71xx_mdio_rr(am, AG71XX_REG_MII_CFG),
		ag71xx_mdio_rr(am, AG71XX_REG_MII_CMD),
		ag71xx_mdio_rr(am, AG71XX_REG_MII_ADDR));
	dev_dbg(&am->mii_bus->dev,
		"mii_ctrl=%08x, mii_status=%08x, mii_ind=%08x\n",
		ag71xx_mdio_rr(am, AG71XX_REG_MII_CTRL),
		ag71xx_mdio_rr(am, AG71XX_REG_MII_STATUS),
		ag71xx_mdio_rr(am, AG71XX_REG_MII_IND));
}

static u32 ag71xx_mdio_get_divider(struct ag71xx *ag)
{
	unsigned long ref_clock;
	int i;

	ref_clock = clk_get_rate(am->ref_clk);
	if (!ref_clock)
		return -EINVAL;

	for (i = 0; i <= am->hw->div_table_size; i++) {
		unsigned long r;

		r = ref_clock / am->hw->div_table[i];
		if (r <= am->mdio_rate)
			return am->hw->div_table[i];
	}

	/* Fallback on the slowest possible clock */
	return am->hw->div_table[i];
}

static int ag71xx_mdio_reset(struct mii_bus *bus)
{
	struct ag71xx *ag = bus->priv;
	u32 div;

	div = ag71xx_mdio_get_divider(am);

	ag71xx_mdio_wr(am, AG71XX_REG_MII_CFG, div | MII_CFG_RESET);
	udelay(100);

	ag71xx_mdio_wr(am, AG71XX_REG_MII_CFG, div);
	udelay(100);

	return 0;
}

static int ag71xx_mdio_wait_busy(struct ag71xx *ag)
{
	int i;

	for (i = 0; i < AG71XX_MDIO_RETRY; i++) {
		u32 busy;

		busy = ag71xx_mdio_rr(am, AG71XX_REG_MII_IND);
		if (!busy)
			return 0;

		udelay(AG71XX_MDIO_DELAY);
	}

	dev_err(&am->mii_bus->dev, "MDIO operation timed out\n");

	return -ETIMEDOUT;
}

int ag71xx_mdio_read(struct mii_bus *bus, int addr, int reg)
{
	struct ag71xx *ag = bus->priv;
	int val, err;

	err = ag71xx_mdio_wait_busy(am);
	if (err)
		return err;

	ag71xx_mdio_wr(am, AG71XX_REG_MII_CMD, MII_CMD_WRITE);
	ag71xx_mdio_wr(am, AG71XX_REG_MII_ADDR,
			((addr & 0xff) << MII_ADDR_SHIFT) | (reg & 0xff));
	ag71xx_mdio_wr(am, AG71XX_REG_MII_CMD, MII_CMD_READ);

	err = ag71xx_mdio_wait_busy(am);
	if (err)
		return err;

	val = ag71xx_mdio_rr(am, AG71XX_REG_MII_STATUS) & 0xffff;
	ag71xx_mdio_wr(am, AG71XX_REG_MII_CMD, MII_CMD_WRITE);

	dev_dbg(&bus->dev, "mii_read: addr=%04x, reg=%04x, value=%04x\n",
	    addr, reg, val);

	return val;
}

int ag71xx_mdio_write(struct mii_bus *bus, int addr, int reg, u16 val)
{
	struct ag71xx *ag = bus->priv;

	dev_dbg(&bus->dev, "mii_write: addr=%04x, reg=%04x, value=%04x\n",
		addr, reg, val);

	ag71xx_mdio_wr(am, AG71XX_REG_MII_ADDR,
			((addr & 0xff) << MII_ADDR_SHIFT) | (reg & 0xff));
	ag71xx_mdio_wr(am, AG71XX_REG_MII_CTRL, val);

	return ag71xx_mdio_wait_busy(am);
}

static int ag71xx_mdio_probe(struct ag71xx *ag)
{
	of_property_read_u32(np, "mdio-frequency", &ag->mdio_rate);

	ag->mii_bus = devm_mdiobus_alloc(&pdev->dev);
	if (ag->mii_bus == NULL)
		return -ENOMEM;

	ag->mii_bus->name = "ag71xx_mdio";
	ag->mii_bus->read = ag71xx_mdio_read;
	ag->mii_bus->write = ag71xx_mdio_write;
	ag->mii_bus->reset = ag71xx_mdio_reset;
	ag->mii_bus->priv = am;
	ag->mii_bus->parent = &pdev->dev;
	snprintf(ag->mii_bus->id, MII_BUS_ID_SIZE, "%s",
		 dev_name(&pdev->dev));
	ag->mii_bus->phy_mask = ~0;

	for (i = 0; i < PHY_MAX_ADDR; i++)
		ag->mii_bus->irq[i] = PHY_POLL;

	// Why? That will reset the MAC!
	//ag71xx_mdio_wr(am, AG71XX_REG_MAC_CFG1, 0);
	platform_set_drvdata(pdev, am);

	err = mdiobus_register(ag->mii_bus);
	if (err)
		return err;

	ag71xx_mdio_dump_regs(am);

	return 0;
}

//	mdiobus_unregister(ag->mii_bus);
