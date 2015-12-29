/*
 *  Qualcomm Atheros QCA955X EXT interrupt controller
 *
 *  Copyright (C) 2015 Alban Bedel <albeu@free.fr>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/irqchip.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#define QCA955X_EXT_INT_WMAC_MISC		BIT(0)
#define QCA955X_EXT_INT_WMAC_TX			BIT(1)
#define QCA955X_EXT_INT_WMAC_RXLP		BIT(2)
#define QCA955X_EXT_INT_WMAC_RXHP		BIT(3)
#define QCA955X_EXT_INT_PCIE_RC1		BIT(4)
#define QCA955X_EXT_INT_PCIE_RC1_INT0		BIT(5)
#define QCA955X_EXT_INT_PCIE_RC1_INT1		BIT(6)
#define QCA955X_EXT_INT_PCIE_RC1_INT2		BIT(7)
#define QCA955X_EXT_INT_PCIE_RC1_INT3		BIT(8)
#define QCA955X_EXT_INT_PCIE_RC2		BIT(12)
#define QCA955X_EXT_INT_PCIE_RC2_INT0		BIT(13)
#define QCA955X_EXT_INT_PCIE_RC2_INT1		BIT(14)
#define QCA955X_EXT_INT_PCIE_RC2_INT2		BIT(15)
#define QCA955X_EXT_INT_PCIE_RC2_INT3		BIT(16)
#define QCA955X_EXT_INT_USB1			BIT(24)
#define QCA955X_EXT_INT_USB2			BIT(28)

#define QCA955X_EXT_INT_WMAC_ALL \
	(QCA955X_EXT_INT_WMAC_MISC | QCA955X_EXT_INT_WMAC_TX | \
	 QCA955X_EXT_INT_WMAC_RXLP | QCA955X_EXT_INT_WMAC_RXHP)

#define QCA955X_EXT_INT_PCIE_RC1_ALL \
	(QCA955X_EXT_INT_PCIE_RC1 | QCA955X_EXT_INT_PCIE_RC1_INT0 | \
	 QCA955X_EXT_INT_PCIE_RC1_INT1 | QCA955X_EXT_INT_PCIE_RC1_INT2 | \
	 QCA955X_EXT_INT_PCIE_RC1_INT3)

#define QCA955X_EXT_INT_PCIE_RC2_ALL \
	(QCA955X_EXT_INT_PCIE_RC2 | QCA955X_EXT_INT_PCIE_RC2_INT0 | \
	 QCA955X_EXT_INT_PCIE_RC2_INT1 | QCA955X_EXT_INT_PCIE_RC2_INT2 | \
	 QCA955X_EXT_INT_PCIE_RC2_INT3)

static void qca955x_ext_irq_handler(struct irq_desc *desc, u32 mask)
{
	struct irq_domain *domain = irq_desc_get_handler_data(desc);
	void __iomem *base = domain->host_data;
	u32 pending;

	pending = __raw_readl(base) & mask;

	if (!pending) {
		spurious_interrupt();
		return;
	}

	while (pending) {
		int bit = __ffs(pending);

		generic_handle_irq(irq_linear_revmap(domain, bit));
		pending &= ~BIT(bit);
	}
}

static void qca955x_ext_irq_handler0(struct irq_desc *desc)
{
	qca955x_ext_irq_handler(desc,
				QCA955X_EXT_INT_PCIE_RC1_ALL |
				QCA955X_EXT_INT_WMAC_ALL);
}

static void qca955x_ext_irq_handler1(struct irq_desc *desc)
{
	qca955x_ext_irq_handler(desc,
				QCA955X_EXT_INT_PCIE_RC2_ALL |
				QCA955X_EXT_INT_USB1 |
				QCA955X_EXT_INT_USB2);
}

static void qca955x_ext_irq_noop(struct irq_data *d) { }

static struct irq_chip qca955x_ext_irq_chip = {
	.name		= "EXT",
	.irq_unmask	= qca955x_ext_irq_noop,
	.irq_mask	= qca955x_ext_irq_noop,
};

static int qca955x_ext_map(struct irq_domain *d, unsigned int irq,
			irq_hw_number_t hw)
{
	irq_set_chip_and_handler(irq, &qca955x_ext_irq_chip, handle_level_irq);
	return 0;
}

static const struct irq_domain_ops qca955x_ext_irq_domain_ops = {
	.xlate = irq_domain_xlate_onecell,
	.map = qca955x_ext_map,
};

static int __init qca955x_ext_intc_of_init(
	struct device_node *node, struct device_node *parent)
{
	struct irq_domain *domain;
	void __iomem *base;
	int irq[2];
	int i;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("Failed to get EXT IRQ registers\n");
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(irq); i++) {
		irq[i] = irq_of_parse_and_map(node, i);
		if (!irq[i]) {
			pr_err("Failed to get EXT IRQ0\n");
			return -EINVAL;
		}
	}

	domain = irq_domain_add_linear(node, 32,
				&qca955x_ext_irq_domain_ops, base);
	if (!domain) {
		pr_err("Failed to add MISC irqdomain\n");
		return -EINVAL;
	}

	irq_set_chained_handler_and_data(
		irq[0], qca955x_ext_irq_handler0, domain);
	irq_set_chained_handler_and_data(
		irq[1], qca955x_ext_irq_handler1, domain);

	return 0;
}

IRQCHIP_DECLARE(qca955x_ext_intc, "qca,qca9550-ext-intc",
		qca955x_ext_intc_of_init);
