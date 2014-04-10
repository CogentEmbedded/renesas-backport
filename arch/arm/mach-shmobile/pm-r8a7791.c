/*
 * r8a7791 Power management support
 *
 * Copyright (C) 2013-2014  Renesas Electronics Corporation
 * Copyright (C) 2011  Renesas Solutions Corp.
 * Copyright (C) 2011  Magnus Damm
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/kernel.h>
#include <linux/pm.h>
#include <linux/pm_clock.h>
#include <linux/smp.h>
#include <asm/io.h>
#include "common.h"
#include "pm-rcar.h"
#include "r8a7791.h"

#define RST		0xe6160000
#define CA15BAR		0x0020
#define CA15RESCNT	0x0040
#define RAM		0xe6300000

/* SYSC */
#define SYSCIER 0x0c
#define SYSCIMR 0x10

#if defined(CONFIG_PM) || defined(CONFIG_SMP)

static void __init r8a7791_sysc_init(void)
{
	void __iomem *base = rcar_sysc_init(0xe6180000);

	/* enable all interrupt sources, but do not use interrupt handler */
	iowrite32(0x0131000e, base + SYSCIER);
	iowrite32(0, base + SYSCIMR);
}

#else /* CONFIG_PM || CONFIG_SMP */

static inline void r8a7791_sysc_init(void) {}

#endif /* CONFIG_PM || CONFIG_SMP */

void __init r8a7791_pm_init(void)
{
	void __iomem *p;
	u32 bar;
	static int once;

	if (once++)
		return;

	/* RAM for jump stub, because BAR requires 256KB aligned address */
	p = ioremap_nocache(RAM, shmobile_boot_size);
	memcpy_toio(p, shmobile_boot_vector, shmobile_boot_size);
	iounmap(p);

	/* setup reset vectors */
	p = ioremap_nocache(RST, 0x63);
	bar = (RAM >> 8) & 0xfffffc00;
	writel_relaxed(bar, p + CA15BAR);
	writel_relaxed(bar | 0x10, p + CA15BAR);

	/* enable clocks to all CPUs */
	writel_relaxed((readl_relaxed(p + CA15RESCNT) & ~0x0f) | 0xa5a50000,
		       p + CA15RESCNT);
	iounmap(p);

	r8a7791_sysc_init();
	shmobile_smp_apmu_suspend_init();
}

#ifdef CONFIG_PM

static int pd_power_down(struct generic_pm_domain *genpd)
{
	struct rcar_sysc_ch *r8a7791_ch = to_r8a7791_ch(genpd);
	int ret;

	ret =  rcar_sysc_power_down(to_r8a7791_ch(genpd));

	if (r8a7791_ch->chan_offs == 0xc0) {
		/*
		 * Issue software reset to 3DG functional blocks right after
		 * the SGX power shut-off to avoid a hardware lock-up issue
		 * triggered when we bring the SGX power up next time.
		 */
		r8a7791_module_reset(1, BIT(12), 2); /* DVFS */
		r8a7791_module_reset(8, BIT(0), 2); /* CONST */
	}

	return ret;
}

static int pd_power_up(struct generic_pm_domain *genpd)
{
	return rcar_sysc_power_up(to_r8a7791_ch(genpd));
}

static bool pd_is_off(struct generic_pm_domain *genpd)
{
	return rcar_sysc_power_is_off(to_r8a7791_ch(genpd));
}

static bool pd_active_wakeup(struct device *dev)
{
	return true;
}

static void r8a7791_init_pm_domain(struct r8a7791_pm_domain *r8a7791_pd)
{
	struct generic_pm_domain *genpd = &r8a7791_pd->genpd;

	pm_genpd_init(genpd, NULL, true);
	genpd->dev_ops.stop = pm_clk_suspend;
	genpd->dev_ops.start = pm_clk_resume;
	genpd->dev_ops.active_wakeup = pd_active_wakeup;
	genpd->dev_irq_safe = true;
	genpd->power_off = pd_power_down;
	genpd->power_on = pd_power_up;

	if (pd_is_off(&r8a7791_pd->genpd))
		pd_power_up(&r8a7791_pd->genpd);
}

static struct r8a7791_pm_domain r8a7791_pm_domains[] = {
	{
		.genpd.name = "pvrsrvkm",
		.ch = {
			.chan_offs = 0xc0, /* PWRSR2 .. PWRER2 */
			.isr_bit = 20, /* SGX */
		},
	},
};

void __init r8a7791_init_pm_domains(void)
{
	int j;

	for (j = 0; j < ARRAY_SIZE(r8a7791_pm_domains); j++)
		r8a7791_init_pm_domain(&r8a7791_pm_domains[j]);
}

#endif /* CONFIG_PM */
