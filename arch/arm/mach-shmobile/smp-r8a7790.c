/*
 * SMP support for r8a7790
 *
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

#define RST	0xe6160000
#define CA15BAR	0x0020
#define CA7BAR	0x0030
#define CA15RESCNT 0x0040
#define CA7RESCNT 0x0044

#define APMU	0xe6150000
#define CA15WUPCR 0x2010
#define CA7WUPCR 0x1010

#define MERAM	0xe8080000

enum { R8A7790_CLST_CA15, R8A7790_CLST_CA7, R8A7790_CLST_NR };

static struct {
	unsigned int bar;
	unsigned int rescnt;
	unsigned int rescnt_magic;
} r8a7790_clst[R8A7790_CLST_NR] = {
	[R8A7790_CLST_CA15] = {
		.bar = CA15BAR,
		.rescnt = CA15RESCNT,
		.rescnt_magic = 0xa5a50000,
	},
	[R8A7790_CLST_CA7] = {
		.bar = CA7BAR,
		.rescnt = CA7RESCNT,
		.rescnt_magic = 0x5a5a0000,
	},
};

#define r8a7790_clst_id(cpu) (cpu_logical_map((cpu)) >> 8)
#define r8a7790_cpu_id(cpu) (cpu_logical_map((cpu)) & 0xff)

static void r8a7790_deassert_reset(unsigned int cpu)
{
	void __iomem *p, *rescnt;
	u32 bar, mask, magic;
	unsigned int clst_id = r8a7790_clst_id(cpu);

	/* setup reset vectors */
	p = ioremap_nocache(RST, 0x63);
	bar = (MERAM >> 8) & 0xfffffc00;
	writel_relaxed(bar, p + r8a7790_clst[clst_id].bar);
	writel_relaxed(bar | 0x10, p + r8a7790_clst[clst_id].bar);

	/* enable per-core clocks */
	mask = BIT(3 - r8a7790_cpu_id(cpu));
	magic = r8a7790_clst[clst_id].rescnt_magic;
	rescnt = p + r8a7790_clst[clst_id].rescnt;
	writel_relaxed((readl_relaxed(rescnt) & ~mask) | magic, rescnt);

	iounmap(p);
}

static void r8a7790_assert_reset(unsigned int cpu)
{
	void __iomem *p, *rescnt;
	u32 mask, magic;
	unsigned int clst_id = r8a7790_clst_id(cpu);

	p = ioremap_nocache(RST, 0x63);

	/* disable per-core clocks */
	mask = BIT(3 - r8a7790_cpu_id(cpu));
	magic = r8a7790_clst[clst_id].rescnt_magic;
	rescnt = p + r8a7790_clst[clst_id].rescnt;
	writel_relaxed((readl_relaxed(rescnt) | mask) | magic, rescnt);

	iounmap(p);
}

static void r8a7790_power_on(unsigned int cpu)
{
	void __iomem *p, *cawupcr;

	/* wake up CPU core via APMU */
	p = ioremap_nocache(APMU, 0x3000);
	cawupcr = p + (r8a7790_clst_id(cpu) ? CA7WUPCR : CA15WUPCR);
	writel_relaxed(BIT(r8a7790_cpu_id(cpu)), cawupcr);

	/* wait for APMU to finish */
	while (readl_relaxed(cawupcr) != 0)
		;

	iounmap(p);
}

static void __init r8a7790_smp_prepare_cpus(unsigned int max_cpus)
{
	void __iomem *p;
	unsigned int k;

	shmobile_boot_fn = virt_to_phys(shmobile_invalidate_start);

	/* MERAM for jump stub, because BAR requires 256KB aligned address */
	p = ioremap_nocache(MERAM, 16);
	memcpy_toio(p, shmobile_boot_vector, 16);
	iounmap(p);

	/* keep secondary CPU cores in reset */
	for (k = 1; k < 8; k++)
		r8a7790_assert_reset(k);
}

static int __cpuinit r8a7790_boot_secondary(unsigned int cpu,
					    struct task_struct *idle)
{
	/* only allow a single cluster for now */
	if (r8a7790_clst_id(cpu) != r8a7790_clst_id(0))
		return -ENOTSUPP;

	r8a7790_power_on(cpu);
	r8a7790_deassert_reset(cpu);
	return 0;
}

struct smp_operations r8a7790_smp_ops __initdata = {
	.smp_prepare_cpus	= r8a7790_smp_prepare_cpus,
	.smp_boot_secondary	= r8a7790_boot_secondary,
};
