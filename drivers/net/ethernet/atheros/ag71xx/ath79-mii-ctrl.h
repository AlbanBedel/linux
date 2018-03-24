#ifndef ATH79_MII_CTRL_H
#define ATH79_MII_CTRL_H

#include <linux/phy.h>

struct mii_ctrl_handle;

struct mii_ctrl_handle *devm_mii_ctrl_get(struct device *dev);
void devm_mii_ctrl_put(struct device *dev, struct mii_ctrl_handle *hdl);

int mii_ctrl_set_interface(
	const struct mii_ctrl_handle *hdl, phy_interface_t iface);
int mii_ctrl_set_speed(
	const struct mii_ctrl_handle *hdl, int link_speed);

#endif /* ATH79_MII_CTRL_H */
