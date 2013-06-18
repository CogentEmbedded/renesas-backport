/*
 * marzen board support
 *
 * Copyright (C) 2011  Renesas Solutions Corp.
 * Copyright (C) 2011  Magnus Damm
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/dma-mapping.h>
#include <linux/pinctrl/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/smsc911x.h>
#include <linux/platform_data/rcar-du.h>
#include <mach/hardware.h>
#include <mach/r8a7779.h>
#include <mach/common.h>
#include <mach/irqs.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/traps.h>

/* Dummy supplies, where voltage doesn't matter */
static struct regulator_consumer_supply dummy_supplies[] = {
	REGULATOR_SUPPLY("vddvario", "smsc911x"),
	REGULATOR_SUPPLY("vdd33a", "smsc911x"),
};

/* SMSC LAN89218 */
static struct resource smsc911x_resources[] = {
	[0] = {
		.start		= 0x18000000, /* ExCS0 */
		.end		= 0x180000ff, /* A1->A7 */
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= irq_pin(1), /* IRQ1 */
		.flags		= IORESOURCE_IRQ,
	},
};

static struct smsc911x_platform_config smsc911x_platdata = {
	.flags		= SMSC911X_USE_32BIT, /* 32-bit SW on 16-bit HW bus */
	.phy_interface	= PHY_INTERFACE_MODE_MII,
	.irq_polarity	= SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
	.irq_type	= SMSC911X_IRQ_TYPE_PUSH_PULL,
};

static struct platform_device eth_device = {
	.name		= "smsc911x",
	.id		= -1,
	.dev  = {
		.platform_data = &smsc911x_platdata,
	},
	.resource	= smsc911x_resources,
	.num_resources	= ARRAY_SIZE(smsc911x_resources),
};

/* DU
 *
 * The panel only specifies the [hv]display and [hv]total values. The position
 * and width of the sync pulses don't matter, they're copied from VESA timings.
 */
static struct rcar_du_encoder_data du_encoders[] = {
	{
		.encoder = RCAR_DU_ENCODER_VGA,
		.output = 0,
	}, {
		.encoder = RCAR_DU_ENCODER_LVDS,
		.output = 1,
		.u.lvds.panel = {
			.width_mm = 210,
			.height_mm = 158,
			.mode = {
				.clock = 65000,
				.hdisplay = 1024,
				.hsync_start = 1048,
				.hsync_end = 1184,
				.htotal = 1344,
				.vdisplay = 768,
				.vsync_start = 771,
				.vsync_end = 777,
				.vtotal = 806,
				.flags = 0,
			},
		},
	},
};

static struct rcar_du_platform_data du_pdata = {
	.encoders = du_encoders,
	.num_encoders = ARRAY_SIZE(du_encoders),
};

/* LEDS */
static struct gpio_led marzen_leds[] = {
	{
		.name		= "led2",
		.gpio		= 157,
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	}, {
		.name		= "led3",
		.gpio		= 158,
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	}, {
		.name		= "led4",
		.gpio		= 159,
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
};

static struct gpio_led_platform_data marzen_leds_pdata = {
	.leds		= marzen_leds,
	.num_leds	= ARRAY_SIZE(marzen_leds),
};

static struct platform_device leds_device = {
	.name	= "leds-gpio",
	.id	= 0,
	.dev	= {
		.platform_data  = &marzen_leds_pdata,
	},
};

static struct platform_device *marzen_devices[] __initdata = {
	&eth_device,
	&leds_device,
};

static const struct pinctrl_map marzen_pinctrl_map[] = {
	/* DU (CN10: ARGB0, CN13: LVDS) */
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du", "pfc-r8a7779",
				  "du0_rgb888", "du0"),
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du", "pfc-r8a7779",
				  "du0_sync_1", "du0"),
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du", "pfc-r8a7779",
				  "du0_clk_out_0", "du0"),
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du", "pfc-r8a7779",
				  "du1_rgb666", "du1"),
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du", "pfc-r8a7779",
				  "du1_sync_1", "du1"),
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du", "pfc-r8a7779",
				  "du1_clk_out", "du1"),
	/* SCIF2 (CN18: DEBUG0) */
	PIN_MAP_MUX_GROUP_DEFAULT("sh-sci.2", "pfc-r8a7779",
				  "scif2_data_c", "scif2"),
	/* SCIF4 (CN19: DEBUG1) */
	PIN_MAP_MUX_GROUP_DEFAULT("sh-sci.4", "pfc-r8a7779",
				  "scif4_data", "scif4"),
	/* SDHI0 */
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.0", "pfc-r8a7779",
				  "sdhi0_data4", "sdhi0"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.0", "pfc-r8a7779",
				  "sdhi0_ctrl", "sdhi0"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.0", "pfc-r8a7779",
				  "sdhi0_cd", "sdhi0"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.0", "pfc-r8a7779",
				  "sdhi0_wp", "sdhi0"),
	/* SMSC */
	PIN_MAP_MUX_GROUP_DEFAULT("smsc911x", "pfc-r8a7779",
				  "intc_irq1_b", "intc"),
	PIN_MAP_MUX_GROUP_DEFAULT("smsc911x", "pfc-r8a7779",
				  "lbsc_ex_cs0", "lbsc"),
};

static void __init marzen_init(void)
{
	regulator_register_fixed(0, dummy_supplies, ARRAY_SIZE(dummy_supplies));

	pinctrl_register_mappings(marzen_pinctrl_map,
				  ARRAY_SIZE(marzen_pinctrl_map));
	r8a7779_pinmux_init();
	r8a7779_init_irq_extpin(1); /* IRQ1 as individual interrupt */

	r8a7779_add_standard_devices();
	platform_add_devices(marzen_devices, ARRAY_SIZE(marzen_devices));
	r8a7779_add_du_device(&du_pdata);
}

MACHINE_START(MARZEN, "marzen")
	.smp		= smp_ops(r8a7779_smp_ops),
	.map_io		= r8a7779_map_io,
	.init_early	= r8a7779_add_early_devices,
	.nr_irqs	= NR_IRQS_LEGACY,
	.init_irq	= r8a7779_init_irq,
	.init_machine	= marzen_init,
	.init_late	= shmobile_init_late,
	.timer		= &shmobile_timer,
MACHINE_END
