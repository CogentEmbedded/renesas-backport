/*
 * r8a7790 processor support
 *
 * Copyright (C) 2013  Renesas Solutions Corp.
 * Copyright (C) 2013  Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/platform_data/gpio-rcar.h>
#include <linux/platform_data/rcar-du.h>
#include <linux/platform_data/irq-renesas-irqc.h>
#include <linux/platform_data/vsp1.h>
#include <linux/serial_sci.h>
#include <linux/sh_timer.h>
#include <mach/common.h>
#include <mach/irqs.h>
#include <mach/r8a7790.h>
#include <asm/mach/arch.h>

static const struct resource pfc_resources[] = {
	DEFINE_RES_MEM(0xe6060000, 0x250),
};

#define R8A7790_GPIO(idx)						\
static struct resource r8a7790_gpio##idx##_resources[] = {		\
	DEFINE_RES_MEM(0xe6050000 + 0x1000 * (idx), 0x50),		\
	DEFINE_RES_IRQ(gic_spi(4 + (idx))),				\
};									\
									\
static struct gpio_rcar_config r8a7790_gpio##idx##_platform_data = {	\
	.gpio_base	= 32 * (idx),					\
	.irq_base	= 0,						\
	.number_of_pins	= 32,						\
	.pctl_name	= "pfc-r8a7790",				\
	.has_both_edge_trigger = 1,					\
};									\

R8A7790_GPIO(0);
R8A7790_GPIO(1);
R8A7790_GPIO(2);
R8A7790_GPIO(3);
R8A7790_GPIO(4);
R8A7790_GPIO(5);

#define r8a7790_register_gpio(idx)					\
	platform_device_register_resndata(&platform_bus, "gpio_rcar", idx, \
		r8a7790_gpio##idx##_resources,				\
		ARRAY_SIZE(r8a7790_gpio##idx##_resources),		\
		&r8a7790_gpio##idx##_platform_data,			\
		sizeof(r8a7790_gpio##idx##_platform_data))

void __init r8a7790_pinmux_init(void)
{
	platform_device_register_simple("pfc-r8a7790", -1, pfc_resources,
					ARRAY_SIZE(pfc_resources));
	r8a7790_register_gpio(0);
	r8a7790_register_gpio(1);
	r8a7790_register_gpio(2);
	r8a7790_register_gpio(3);
	r8a7790_register_gpio(4);
	r8a7790_register_gpio(5);
}

/* DU */
static const struct resource du_resources[] = {
	DEFINE_RES_MEM(0xfeb00000, 0x70000),
	DEFINE_RES_MEM_NAMED(0xfeb90000, 0x1c, "lvds.0"),
	DEFINE_RES_MEM_NAMED(0xfeb94000, 0x1c, "lvds.1"),
	DEFINE_RES_IRQ(gic_spi(256)),
	DEFINE_RES_IRQ(gic_spi(268)),
	DEFINE_RES_IRQ(gic_spi(269)),
};

void __init r8a7790_add_du_device(struct rcar_du_platform_data *pdata)
{
	struct platform_device_info info = {
		.name = "rcar-du-r8a7790",
		.id = -1,
		.res = du_resources,
		.num_res = ARRAY_SIZE(du_resources),
		.data = pdata,
		.size_data = sizeof(*pdata),
		.dma_mask = DMA_BIT_MASK(32),
	};

	platform_device_register_full(&info);
}

/* VSP1 */
static const struct resource vsp1_0_resources[] = {
	DEFINE_RES_MEM(0xfe920000, 0x8000),
	DEFINE_RES_IRQ(gic_spi(266)),
};

static const struct resource vsp1_1_resources[] = {
	DEFINE_RES_MEM(0xfe928000, 0x8000),
	DEFINE_RES_IRQ(gic_spi(267)),
};

static const struct resource vsp1_2_resources[] = {
	DEFINE_RES_MEM(0xfe930000, 0x8000),
	DEFINE_RES_IRQ(gic_spi(246)),
};

static const struct resource vsp1_3_resources[] = {
	DEFINE_RES_MEM(0xfe938000, 0x8000),
	DEFINE_RES_IRQ(gic_spi(247)),
};

static const struct resource *vsp1_resources[4] = {
	vsp1_0_resources,
	vsp1_1_resources,
	vsp1_2_resources,
	vsp1_3_resources,
};

void __init r8a7790_add_vsp1_device(struct vsp1_platform_data *pdata,
				    unsigned int index)
{
	struct platform_device_info info = {
		.name = "vsp1",
		.id = index,
		.data = pdata,
		.size_data = sizeof(*pdata),
		.num_res = 2,
		.dma_mask = DMA_BIT_MASK(32),
	};

	if (index >= ARRAY_SIZE(vsp1_resources))
		return;

	info.res = vsp1_resources[index];

	platform_device_register_full(&info);
}

#define SCIF_COMMON(scif_type, baseaddr, irq)			\
	.type		= scif_type,				\
	.mapbase	= baseaddr,				\
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,	\
	.irqs		= SCIx_IRQ_MUXED(irq)

#define SCIFA_DATA(index, baseaddr, irq)		\
[index] = {						\
	SCIF_COMMON(PORT_SCIFA, baseaddr, irq),		\
	.scbrr_algo_id	= SCBRR_ALGO_4,			\
	.scscr = SCSCR_RE | SCSCR_TE | SCSCR_CKE0,	\
}

#define SCIFB_DATA(index, baseaddr, irq)	\
[index] = {					\
	SCIF_COMMON(PORT_SCIFB, baseaddr, irq),	\
	.scbrr_algo_id	= SCBRR_ALGO_4,		\
	.scscr = SCSCR_RE | SCSCR_TE,		\
}

#define SCIF_DATA(index, baseaddr, irq)		\
[index] = {						\
	SCIF_COMMON(PORT_SCIF, baseaddr, irq),		\
	.scbrr_algo_id	= SCBRR_ALGO_2,			\
	.scscr = SCSCR_RE | SCSCR_TE,	\
}

#define HSCIF_DATA(index, baseaddr, irq)		\
[index] = {						\
	SCIF_COMMON(PORT_HSCIF, baseaddr, irq),		\
	.scbrr_algo_id	= SCBRR_ALGO_6,			\
	.scscr = SCSCR_RE | SCSCR_TE,	\
}

enum { SCIFA0, SCIFA1, SCIFB0, SCIFB1, SCIFB2, SCIFA2, SCIF0, SCIF1,
       HSCIF0, HSCIF1 };

static const struct plat_sci_port scif[] = {
	SCIFA_DATA(SCIFA0, 0xe6c40000, gic_spi(144)), /* SCIFA0 */
	SCIFA_DATA(SCIFA1, 0xe6c50000, gic_spi(145)), /* SCIFA1 */
	SCIFB_DATA(SCIFB0, 0xe6c20000, gic_spi(148)), /* SCIFB0 */
	SCIFB_DATA(SCIFB1, 0xe6c30000, gic_spi(149)), /* SCIFB1 */
	SCIFB_DATA(SCIFB2, 0xe6ce0000, gic_spi(150)), /* SCIFB2 */
	SCIFA_DATA(SCIFA2, 0xe6c60000, gic_spi(151)), /* SCIFA2 */
	SCIF_DATA(SCIF0, 0xe6e60000, gic_spi(152)), /* SCIF0 */
	SCIF_DATA(SCIF1, 0xe6e68000, gic_spi(153)), /* SCIF1 */
	HSCIF_DATA(HSCIF0, 0xe62c0000, gic_spi(154)), /* HSCIF0 */
	HSCIF_DATA(HSCIF1, 0xe62c8000, gic_spi(155)), /* HSCIF1 */
};

static inline void r8a7790_register_scif(int idx)
{
	platform_device_register_data(&platform_bus, "sh-sci", idx, &scif[idx],
				      sizeof(struct plat_sci_port));
}

static struct renesas_irqc_config irqc0_data = {
	.irq_base = irq_pin(0), /* IRQ0 -> IRQ3 */
};

static struct resource irqc0_resources[] = {
	DEFINE_RES_MEM(0xe61c0000, 0x200), /* IRQC Event Detector Block_0 */
	DEFINE_RES_IRQ(gic_spi(0)), /* IRQ0 */
	DEFINE_RES_IRQ(gic_spi(1)), /* IRQ1 */
	DEFINE_RES_IRQ(gic_spi(2)), /* IRQ2 */
	DEFINE_RES_IRQ(gic_spi(3)), /* IRQ3 */
};

#define r8a7790_register_irqc(idx)					\
	platform_device_register_resndata(&platform_bus, "renesas_irqc", \
					  idx, irqc##idx##_resources,	\
					  ARRAY_SIZE(irqc##idx##_resources), \
					  &irqc##idx##_data,		\
					  sizeof(struct renesas_irqc_config))

static struct resource thermal_resources[] __initdata = {
	DEFINE_RES_MEM(0xe61f0000, 0x14),
	DEFINE_RES_MEM(0xe61f0100, 0x38),
	DEFINE_RES_IRQ(gic_spi(69)),
};

#define r8a7790_register_thermal()					\
	platform_device_register_simple("rcar_thermal", -1,		\
					thermal_resources,		\
					ARRAY_SIZE(thermal_resources))

static struct sh_timer_config cmt00_platform_data = {
	.name = "CMT00",
	.timer_bit = 0,
	.clockevent_rating = 80,
};

static struct resource cmt00_resources[] = {
	DEFINE_RES_MEM(0xffca0510, 0x0c),
	DEFINE_RES_MEM(0xffca0500, 0x04),
	DEFINE_RES_IRQ(gic_spi(142)), /* CMT0_0 */
};

#define r8a7790_register_cmt(idx)					\
	platform_device_register_resndata(&platform_bus, "sh_cmt",	\
					  idx, cmt##idx##_resources,	\
					  ARRAY_SIZE(cmt##idx##_resources), \
					  &cmt##idx##_platform_data,	\
					  sizeof(struct sh_timer_config))

void __init r8a7790_add_standard_devices(void)
{
	r8a7790_register_scif(SCIFA0);
	r8a7790_register_scif(SCIFA1);
	r8a7790_register_scif(SCIFB0);
	r8a7790_register_scif(SCIFB1);
	r8a7790_register_scif(SCIFB2);
	r8a7790_register_scif(SCIFA2);
	r8a7790_register_scif(SCIF0);
	r8a7790_register_scif(SCIF1);
	r8a7790_register_scif(HSCIF0);
	r8a7790_register_scif(HSCIF1);
	r8a7790_register_irqc(0);
	r8a7790_register_thermal();
	r8a7790_register_cmt(00);
}

#define MODEMR 0xe6160060

u32 __init r8a7790_read_mode_pins(void)
{
	void __iomem *modemr = ioremap_nocache(MODEMR, 4);
	u32 mode;

	BUG_ON(!modemr);
	mode = ioread32(modemr);
	iounmap(modemr);

	return mode;
}

#define CNTCR 0
#define CNTFID0 0x20

void __init r8a7790_timer_init(void)
{
#ifdef CONFIG_ARM_ARCH_TIMER
	u32 mode = r8a7790_read_mode_pins();
	void __iomem *base;
	int extal_mhz = 0;
	u32 freq;

	/* At Linux boot time the r8a7790 arch timer comes up
	 * with the counter disabled. Moreover, it may also report
	 * a potentially incorrect fixed 13 MHz frequency. To be
	 * correct these registers need to be updated to use the
	 * frequency EXTAL / 2 which can be determined by the MD pins.
	 */

	switch (mode & (MD(14) | MD(13))) {
	case 0:
		extal_mhz = 15;
		break;
	case MD(13):
		extal_mhz = 20;
		break;
	case MD(14):
		extal_mhz = 26;
		break;
	case MD(13) | MD(14):
		extal_mhz = 30;
		break;
	}

	/* The arch timer frequency equals EXTAL / 2 */
	freq = extal_mhz * (1000000 / 2);

	/* Remap "armgcnt address map" space */
	base = ioremap(0xe6080000, PAGE_SIZE);

	/* Update registers with correct frequency */
	iowrite32(freq, base + CNTFID0);
	asm volatile("mcr p15, 0, %0, c14, c0, 0" : : "r" (freq));

	/* make sure arch timer is started by setting bit 0 of CNTCR */
	iowrite32(1, base + CNTCR);
	iounmap(base);
#endif /* CONFIG_ARM_ARCH_TIMER */

	shmobile_timer_init();
}

struct sys_timer r8a7790_timer = {
        .init           = r8a7790_timer_init,
};

void __init r8a7790_init_delay(void)
{
#ifndef CONFIG_ARM_ARCH_TIMER
	shmobile_setup_delay(1300, 2, 4); /* Cortex-A15 @ 1300MHz */
#endif
}

#ifdef CONFIG_USE_OF

static const char *r8a7790_boards_compat_dt[] __initdata = {
	"renesas,r8a7790",
	NULL,
};

DT_MACHINE_START(R8A7790_DT, "Generic R8A7790 (Flattened Device Tree)")
	.smp		= smp_ops(r8a7790_smp_ops),
	.init_early	= r8a7790_init_delay,
	.timer		= &r8a7790_timer,
	.dt_compat	= r8a7790_boards_compat_dt,
MACHINE_END
#endif /* CONFIG_USE_OF */
