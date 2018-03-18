/*
 *  Atheros AR71XX/AR724X/AR913X MII Control support
 *
 *  Copyright (C) 2018 Alban Bedel <albeu@free.fr>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/kref.h>

#include "ath79-mii-ctrl.h"

#define MII_CTRL_MAX_INTERFACE_COUNT	2

#define MII_CTRL_SELECT_SHIFT		0
#define MII_CTRL_SELECT_MASK		0x3

#define MII_CTRL_SELECT_PORT1_MASK	0x1

#define MII_CTRL_SELECT_GMII		0
#define MII_CTRL_SELECT_MII		1
#define MII_CTRL_SELECT_RGMII		2
#define MII_CTRL_SELECT_RMII		3

#define MII_CTRL_SPEED_SHIFT		4
#define MII_CTRL_SPEED_MASK		0x3

#define MII_CTRL_SPEED_10		0
#define MII_CTRL_SPEED_100		1
#define MII_CTRL_SPEED_1000		2

struct mii_ctrl {
	void __iomem *base;
	unsigned num_port;
	bool has_gbit;
};

struct mii_ctrl_handle {
	struct device *dev;
	struct mii_ctrl *ctrl;
	unsigned port;
};

static const struct mii_ctrl ar7100_mii_ctrl = {
	.num_port = 2,
	.has_gbit = 1,
};

static const struct mii_ctrl ar7130_mii_ctrl = {
	.num_port = 2,
	.has_gbit = 0,
};

static void mii_ctrl_release(struct device *dev, void *res)
{
	struct mii_ctrl_handle *hdl = res;

	put_device(hdl->dev);
	kfree(hdl);
}

struct mii_ctrl_handle *devm_mii_ctrl_get(struct device *dev)
{
	struct of_phandle_args phandle;
	struct platform_device *pdev;
	struct mii_ctrl_handle *hdl;
	struct mii_ctrl *ctrl;
	int err;

	err = of_parse_phandle_with_fixed_args(
		dev->of_node, "qca,mii-ctrl", 1, 0, &phandle);
	if (err) {
		dev_err(dev, "Failed to parse MII ctrl phandle\n");
		return ERR_PTR(err);
	}

	pdev = of_find_device_by_node(phandle.np);
	of_node_put(phandle.np);
	if (!pdev)
		return ERR_PTR(-ENODEV);

	/* Defer the probe if the device is not bound yet */
	if (!pdev->dev.driver) {
		err = -EPROBE_DEFER;
		goto put_dev;
	}

	ctrl = platform_get_drvdata(pdev);
	if (!ctrl) {
		err = -EINVAL;
		goto put_dev;
	}

	if (phandle.args[0] >= ctrl->num_port) {
		dev_err(dev, "Bad MII control port number: %u\n",
			phandle.args[0]);
		err = -EINVAL;
		goto put_dev;
	}

	hdl = devres_alloc(mii_ctrl_release, sizeof(*hdl), GFP_KERNEL);
	if (!hdl) {
		err = -ENOMEM;
		goto put_dev;
	}

	hdl->dev = &pdev->dev;
	hdl->ctrl = ctrl;
	hdl->port = phandle.args[0];
	devres_add(dev, hdl);

	return hdl;

put_dev:
	put_device(&pdev->dev);
	return ERR_PTR(err);
}

void devm_mii_ctrl_put(struct device *dev, struct mii_ctrl_handle *hdl)
{
	WARN_ON(devres_release(dev, mii_ctrl_release, NULL, NULL));
}

int mii_ctrl_set_interface(
	const struct mii_ctrl_handle *hdl, phy_interface_t iface)
{
	u32 val, select, speed, port;

	val = readl(hdl->ctrl->base + hdl->port * 4);
	speed = (val >> MII_CTRL_SPEED_SHIFT) & MII_CTRL_SPEED_MASK;

	switch(iface) {
	case PHY_INTERFACE_MODE_GMII:
		/* GMII is only supported on port 0 */
		if (port > 0)
			return -EINVAL;
		select = MII_CTRL_SELECT_GMII;
		break;

	case PHY_INTERFACE_MODE_MII:
		/* MII is only supported on port 0 */
		if (port > 0)
			return -EINVAL;
		select = MII_CTRL_SELECT_MII;
		break;

	case PHY_INTERFACE_MODE_RGMII:
		select = MII_CTRL_SELECT_RGMII;
		break;

	case PHY_INTERFACE_MODE_RMII:
		select = MII_CTRL_SELECT_RMII;
		break;

	default:
		return -EINVAL;
	}

	/* The select field is smaller on port 1 */
	if (port == 1)
		select &= MII_CTRL_SELECT_PORT1_MASK;

	switch(iface) {
	case PHY_INTERFACE_MODE_GMII:
	case PHY_INTERFACE_MODE_RGMII:
		/* Make sure gigabit is supported */
		if (!hdl->ctrl->has_gbit)
			return -EINVAL;
		break;
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_RMII:
		/* Make sure the speed is compatible */
		if (speed > MII_CTRL_SPEED_100)
			speed = MII_CTRL_SPEED_100;
		break;
	default:
		return -EINVAL;
	}

	/* Write the new mode */
	val = (select << MII_CTRL_SELECT_SHIFT) |
		(speed << MII_CTRL_SPEED_SHIFT);
	writel(val, hdl->ctrl->base + hdl->port * 4);

	return 0;
}
EXPORT_SYMBOL_GPL(mii_ctrl_set_interface);


int mii_ctrl_set_speed(
	const struct mii_ctrl_handle *hdl, int link_speed)
{
	u32 val, select, speed;

	val = readl(hdl->ctrl->base + hdl->port * 4);
	select = (val >> MII_CTRL_SELECT_SHIFT) & MII_CTRL_SELECT_MASK;

	switch (link_speed) {
	case 10:
		speed = MII_CTRL_SPEED_10;
	case 100:
		speed = MII_CTRL_SPEED_100;
		break;
	case 1000:
		/* gigabit is not supported with (r)mii */
		if (select == MII_CTRL_SELECT_MII ||
		    select == MII_CTRL_SELECT_RMII)
			return -EINVAL;
		speed = MII_CTRL_SPEED_1000;
		break;
	default:
		return -EINVAL;
	}

	/* Write the new mode */
	val = (select << MII_CTRL_SELECT_SHIFT) |
		(speed << MII_CTRL_SPEED_SHIFT);
	writel(val, hdl->ctrl->base + hdl->port * 4);

	return 0;
}
EXPORT_SYMBOL_GPL(mii_ctrl_set_speed);

static int mii_ctrl_probe(struct platform_device *pdev)
{
	const struct mii_ctrl *cfg;
	struct resource *mem;
	struct mii_ctrl *ctrl;

	cfg = of_device_get_match_data(&pdev->dev);
	if (!cfg)
		return -EINVAL;

	ctrl = devm_kmemdup(&pdev->dev, cfg, sizeof(*cfg), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem)
		return -EINVAL;

	ctrl->base = devm_ioremap_nocache(
		&pdev->dev, mem->start, resource_size(mem));
	if (!ctrl->base)
		return -ENOMEM;

	platform_set_drvdata(pdev, ctrl);

	return 0;
}

static int mii_ctrl_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id mii_ctrl_of_match[] = {
	{ .compatible = "qca,ar7100-mii-ctrl", .data = &ar7100_mii_ctrl },
	{ .compatible = "qca,ar7130-mii-ctrl", .data = &ar7130_mii_ctrl },
	{}
};

static struct platform_driver mii_ctrl_driver = {
	.probe		= mii_ctrl_probe,
	.remove		= mii_ctrl_remove,
	.driver = {
		.name	= "ath79-mii-ctrl",
		.of_match_table = mii_ctrl_of_match,
	},
};
module_platform_driver(mii_ctrl_driver);
