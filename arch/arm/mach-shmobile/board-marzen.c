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
#include <linux/gpio.h>
#include <linux/dma-mapping.h>
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
#include <asm/hardware/gic.h>
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

/* DU */
static struct resource rcar_du_resources[] = {
	[0] = {
		.name	= "Display Unit",
		.start	= 0xfff80000,
		.end	= 0xfffb1007,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(31),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct rcar_du_platform_data rcar_du_pdata = {
	.encoders = {
		[0] = {
			.encoder = RCAR_DU_ENCODER_VGA,
		},
	},
};

static struct platform_device rcar_du_device = {
	.name		= "rcar-du",
	.num_resources	= ARRAY_SIZE(rcar_du_resources),
	.resource	= rcar_du_resources,
	.dev	= {
		.platform_data = &rcar_du_pdata,
		.coherent_dma_mask = ~0,
	},
};

static struct platform_device *marzen_devices[] __initdata = {
	&eth_device,
	&rcar_du_device,
};

static void __init marzen_init(void)
{
	regulator_register_fixed(0, dummy_supplies, ARRAY_SIZE(dummy_supplies));

	r8a7779_pinmux_init();
	r8a7779_init_irq_extpin(1); /* IRQ1 as individual interrupt */

	/* SCIF2 (CN18: DEBUG0) */
	gpio_request(GPIO_FN_TX2_C, NULL);
	gpio_request(GPIO_FN_RX2_C, NULL);

	/* SCIF4 (CN19: DEBUG1) */
	gpio_request(GPIO_FN_TX4, NULL);
	gpio_request(GPIO_FN_RX4, NULL);

	/* LAN89218 */
	gpio_request(GPIO_FN_EX_CS0, NULL); /* nCS */
	gpio_request(GPIO_FN_IRQ1_B, NULL); /* IRQ + PME */

	/* Display Unit 0 (CN10: ARGB0) */
	gpio_request(GPIO_FN_DU0_DR7, NULL);
	gpio_request(GPIO_FN_DU0_DR6, NULL);
	gpio_request(GPIO_FN_DU0_DR5, NULL);
	gpio_request(GPIO_FN_DU0_DR4, NULL);
	gpio_request(GPIO_FN_DU0_DR3, NULL);
	gpio_request(GPIO_FN_DU0_DR2, NULL);
	gpio_request(GPIO_FN_DU0_DR1, NULL);
	gpio_request(GPIO_FN_DU0_DR0, NULL);
	gpio_request(GPIO_FN_DU0_DG7, NULL);
	gpio_request(GPIO_FN_DU0_DG6, NULL);
	gpio_request(GPIO_FN_DU0_DG5, NULL);
	gpio_request(GPIO_FN_DU0_DG4, NULL);
	gpio_request(GPIO_FN_DU0_DG3, NULL);
	gpio_request(GPIO_FN_DU0_DG2, NULL);
	gpio_request(GPIO_FN_DU0_DG1, NULL);
	gpio_request(GPIO_FN_DU0_DG0, NULL);
	gpio_request(GPIO_FN_DU0_DB7, NULL);
	gpio_request(GPIO_FN_DU0_DB6, NULL);
	gpio_request(GPIO_FN_DU0_DB5, NULL);
	gpio_request(GPIO_FN_DU0_DB4, NULL);
	gpio_request(GPIO_FN_DU0_DB3, NULL);
	gpio_request(GPIO_FN_DU0_DB2, NULL);
	gpio_request(GPIO_FN_DU0_DB1, NULL);
	gpio_request(GPIO_FN_DU0_DB0, NULL);
	gpio_request(GPIO_FN_DU0_EXVSYNC_DU0_VSYNC, NULL);
	gpio_request(GPIO_FN_DU0_EXHSYNC_DU0_HSYNC, NULL);
	gpio_request(GPIO_FN_DU0_DOTCLKOUT0, NULL);
	gpio_request(GPIO_FN_DU0_DOTCLKOUT1, NULL);
	gpio_request(GPIO_FN_DU0_DISP, NULL);

	r8a7779_add_standard_devices();
	platform_add_devices(marzen_devices, ARRAY_SIZE(marzen_devices));
}

MACHINE_START(MARZEN, "marzen")
	.map_io		= r8a7779_map_io,
	.init_early	= r8a7779_add_early_devices,
	.nr_irqs	= NR_IRQS_LEGACY,
	.init_irq	= r8a7779_init_irq,
	.handle_irq	= gic_handle_irq,
	.init_machine	= marzen_init,
	.init_late	= shmobile_init_late,
	.timer		= &shmobile_timer,
MACHINE_END
