/*
 * arch/arm/mach-shmobile/smp-r8a7791.c
 *     SMP support for R-Mobile / SH-Mobile - r8a7791 portion
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
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/smp.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/smp_plat.h>
#include <mach/common.h>
#include <mach/hardware.h>

#define IO_BASE		0xe6150000
#define CA15BAR		0x016020
#define CA15RESCNT	0x010040
#define RESCNT		0x010050
#define CA15WUPCR	0x004010
#define SYSCSR		0x030000
#define SECRAM		0xe6300000
#define CCI_BASE	0xf0190000
#define CCI_SLAVE3	0x4000
#define CCI_SLAVE4	0x5000
#define CCI_SNOOP	0x0000
#define CCI_STATUS	0x000c

static unsigned int r8a7791_get_core_count(void)
{
	return CONFIG_NR_CPUS;
}

static void __init r8a7791_smp_init_cpus(void)
{
	unsigned int ncores = r8a7791_get_core_count();

	shmobile_smp_init_cpus(ncores);
}

static void __init r8a7791_smp_prepare_cpus(unsigned int max_cpus)
{
	u32 bar;
	void __iomem *p;

	/* SECRAM for jump stub, because BAR requires 256KB aligned address */
	p = ioremap_nocache(SECRAM, 16);
	memcpy(p, shmobile_secondary_vector, 16);
	iounmap(p);

	p = ioremap_nocache(IO_BASE, 0x40000);
	bar = (SECRAM >> 8) & 0xfffffc00;
	__raw_writel(bar, (int)p + CA15BAR);
	__raw_writel(bar | 0x10, (int)p + CA15BAR);
	iounmap(p);
}

static void __cpuinit r8a7791_secondary_init(unsigned int cpu)
{
	gic_secondary_init(0);
}

static int __cpuinit r8a7791_boot_secondary(unsigned int cpu,
					    struct task_struct *idle)
{
	u32 val;
	void __iomem *p;

	cpu = cpu_logical_map(cpu);

	p = ioremap_nocache(IO_BASE, 0x40000);

	__raw_writel(1 << (cpu & 3), (int)p + CA15WUPCR);

	while ((__raw_readl((int)p + SYSCSR) & 0x3) != 0x3)
		;
	while (__raw_readl((int)p + CA15WUPCR) != 0x0)
		;

	val = __raw_readl((int)p + CA15RESCNT);
	val |= 0xa5a50000;
	switch (cpu & 3) {
	case 1:
		__raw_writel(val & ~0x4, (int)p + CA15RESCNT);
		break;
	case 2:
		__raw_writel(val & ~0x2, (int)p + CA15RESCNT);
		break;
	case 3:
		__raw_writel(val & ~0x1, (int)p + CA15RESCNT);
		break;
	}

	iounmap(p);
	return 0;
}

static int __maybe_unused r8a7791_cpu_kill(unsigned int cpu)
{
	int k;

	/* this function is running on another CPU than the offline target,
	 * here we need wait for shutdown code in platform_cpu_die() to
	 * finish before asking SoC-specific code to power off the CPU core.
	 */
	for (k = 0; k < 1000; k++) {
		if (shmobile_cpu_is_dead(cpu))
			return 1;

		mdelay(1);
	}

	return 0;
}

struct smp_operations r8a7791_smp_ops  __initdata = {
	.smp_init_cpus		= r8a7791_smp_init_cpus,
	.smp_prepare_cpus	= r8a7791_smp_prepare_cpus,
	.smp_secondary_init	= r8a7791_secondary_init,
	.smp_boot_secondary	= r8a7791_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_kill		= r8a7791_cpu_kill,
	.cpu_die		= shmobile_cpu_die,
	.cpu_disable		= shmobile_cpu_disable,
#endif
};
