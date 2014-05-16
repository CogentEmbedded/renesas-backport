/*
 * SMP support for r8a7790
 *
 * Copyright (C) 2014 Renesas Electronics Corporation
 * Copyright (C) 2012-2013 Renesas Solutions Corp.
 * Copyright (C) 2012 Takashi Yoshii <takashi.yoshii.ze@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <asm/smp_plat.h>
#include <mach/common.h>
#include <mach/r8a7790.h>

#define RST		0xe6160000
#define CA15BAR		0x0020
#define CA7BAR		0x0030
#define CA15RESCNT	0x0040
#define CA7RESCNT	0x0044
#define MERAM		0xe8080000
#define APMU		0xe6151000
#define CA7DBGRCR	0x0180
#define CA15DBGRCR	0x1180

static void __init r8a7790_smp_prepare_cpus(unsigned int max_cpus)
{
	void __iomem *p;
	unsigned int k;
	u32 bar, val;

	/* MERAM for jump stub, because BAR requires 256KB aligned address */
	shmobile_boot_p = ioremap_nocache(MERAM, SZ_256K);

	/* let APMU code install data related to shmobile_boot_vector */
	shmobile_smp_apmu_prepare_cpus(max_cpus);

	/* setup reset vectors */
	p = r8a779x_rst_base = ioremap_nocache(RST, 0x64);
	bar = (MERAM >> 8) & 0xfffffc00;
	writel_relaxed(bar, p + CA15BAR);
	writel_relaxed(bar, p + CA7BAR);
	writel_relaxed(bar | 0x10, p + CA15BAR);
	writel_relaxed(bar | 0x10, p + CA7BAR);

	/* setup for debug mode */
	if (rcar_gen2_read_mode_pins() & MD(21)) {
		p = ioremap_nocache(APMU, 0x2000);
		val = readl_relaxed(p + CA15DBGRCR);
		writel_relaxed((val | 0x01f80000), p + CA15DBGRCR);
		val = readl_relaxed(p + CA7DBGRCR);
		writel_relaxed((val | 0x01f83330), p + CA7DBGRCR);
		iounmap(p);
	}

	/* keep secondary CPU cores in reset */
	for (k = 1; k < max_cpus; k++)
		r8a779x_assert_reset(k);
}

struct smp_operations r8a7790_smp_ops __initdata = {
	.smp_prepare_cpus	= r8a7790_smp_prepare_cpus,
	.smp_boot_secondary	= shmobile_smp_apmu_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable		= shmobile_smp_cpu_disable,
	.cpu_die		= shmobile_smp_apmu_cpu_die,
	.cpu_kill		= shmobile_smp_apmu_cpu_kill,
#endif
};
