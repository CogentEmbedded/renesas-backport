#ifndef __ASM_R8A7779_H__
#define __ASM_R8A7779_H__

#include <linux/sh_clk.h>
#include <linux/pm_domain.h>

struct platform_device;
struct rcar_du_platform_data;

struct r8a7779_pm_ch {
	unsigned long chan_offs;
	unsigned int chan_bit;
	unsigned int isr_bit;
};

struct r8a7779_pm_domain {
	struct generic_pm_domain genpd;
	struct r8a7779_pm_ch ch;
};

static inline struct r8a7779_pm_ch *to_r8a7779_ch(struct generic_pm_domain *d)
{
	return &container_of(d, struct r8a7779_pm_domain, genpd)->ch;
}

extern void r8a7779_add_du_device(struct rcar_du_platform_data *pdata);
extern int r8a7779_sysc_power_down(struct r8a7779_pm_ch *r8a7779_ch);
extern int r8a7779_sysc_power_up(struct r8a7779_pm_ch *r8a7779_ch);

#ifdef CONFIG_PM
extern struct r8a7779_pm_domain r8a7779_sh4a;
extern struct r8a7779_pm_domain r8a7779_sgx;
extern struct r8a7779_pm_domain r8a7779_vdp1;
extern struct r8a7779_pm_domain r8a7779_impx3;

extern void r8a7779_init_pm_domain(struct r8a7779_pm_domain *r8a7779_pd);
extern void r8a7779_add_device_to_domain(struct r8a7779_pm_domain *r8a7779_pd,
					struct platform_device *pdev);
#else
#define r8a7779_init_pm_domain(pd) do { } while (0)
#define r8a7779_add_device_to_domain(pd, pdev) do { } while (0)
#endif /* CONFIG_PM */

extern struct smp_operations r8a7779_smp_ops;

#endif /* __ASM_R8A7779_H__ */
