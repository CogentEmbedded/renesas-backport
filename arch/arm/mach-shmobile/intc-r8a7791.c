/*
 * arch/arm/mach-shmobile/intc-r8a7791.c
 *     r8a7791 processor support - INTC hardware block
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <asm/io.h>
#include <asm/mach/irq.h>
#include <mach/irqs.h>
#include <mach/r8a7791.h>

extern struct irq_chip gic_arch_extn;
int gic_of_init(struct device_node *node, struct device_node *parent);

static int r8a7791_set_wake(struct irq_data *data, unsigned int on)
{
	return 0;
}

static  struct of_device_id r8a7791_irq_match[] __initdata = {
	{ .compatible = "arm,cortex-a15-gic", .data = gic_of_init, },
	{}
};

void __init r8a7791_init_irq(void)
{
	of_irq_init(r8a7791_irq_match);
	gic_arch_extn.irq_set_wake = r8a7791_set_wake;
}
