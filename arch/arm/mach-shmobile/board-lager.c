/*
 * Lager board support
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
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

#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irqchip.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/pinctrl/machine.h>
#include <linux/platform_data/gpio-rcar.h>
#include <linux/platform_data/rcar-du.h>
#include <linux/platform_data/vsp1.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/mfd/tmio.h>
#include <linux/mmc/sh_mobile_sdhi.h>
#include <linux/mmc/sh_mmcif.h>
#include <mach/common.h>
#include <mach/r8a7790.h>
#include <mach/irqs.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

static struct i2c_board_info lager_i2c2_devices[] = {
	{ I2C_BOARD_INFO("ak4642", 0x12), },
};

static struct i2c_board_info lager_i2c3_devices[] = {
	{ I2C_BOARD_INFO("da9063", 0x58), },
};

/* DU */
static struct rcar_du_encoder_data lager_du_encoders[] = {
#if defined(CONFIG_DRM_ADV7511)
	{
		.type = RCAR_DU_ENCODER_HDMI,
		.output = RCAR_DU_OUTPUT_LVDS0,
	},
#endif
	{
		.type = RCAR_DU_ENCODER_NONE,
		.output = RCAR_DU_OUTPUT_LVDS1,
		.connector.lvds.panel = {
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
	}, {
		.type = RCAR_DU_ENCODER_VGA,
		.output = RCAR_DU_OUTPUT_DPAD0,
	},
};

static struct rcar_du_platform_data lager_du_pdata = {
	.encoders = lager_du_encoders,
	.num_encoders = ARRAY_SIZE(lager_du_encoders),
};

/* LEDS */
static struct gpio_led lager_leds[] = {
	{
		.name		= "led8",
		.gpio		= RCAR_GP_PIN(5, 17),
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	}, {
		.name		= "led7",
		.gpio		= RCAR_GP_PIN(4, 23),
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	}, {
		.name		= "led6",
		.gpio		= RCAR_GP_PIN(4, 22),
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
};

static __initdata struct gpio_led_platform_data lager_leds_pdata = {
	.leds		= lager_leds,
	.num_leds	= ARRAY_SIZE(lager_leds),
};

/* SPI Flash memory (Spansion S25FL512SAGMFIG11) */
static struct mtd_partition spiflash_part[] = {
	/* Reserved for user loader program, read-only */
	[0] = {
		.name = "loader_prg",
		.offset = 0,
		.size = SZ_256K,
		.mask_flags = MTD_WRITEABLE,    /* read only */
	},
	/* Reserved for user program, read-only */
	[1] = {
		.name = "user_prg",
		.offset = MTDPART_OFS_APPEND,
		.size = SZ_4M,
		.mask_flags = MTD_WRITEABLE,    /* read only */
	},
	/* All else is writable (e.g. JFFS2) */
	[2] = {
		.name = "flash_fs",
		.offset = MTDPART_OFS_APPEND,
		.size = MTDPART_SIZ_FULL,
		.mask_flags = 0,
	},
};

static struct flash_platform_data spiflash_data = {
	.name		= "m25p80",
	.parts		= spiflash_part,
	.nr_parts	= ARRAY_SIZE(spiflash_part),
	.type		= "s25fl512s",
};

static struct spi_board_info spi_info[] __initdata = {
	{
		.modalias	= "m25p80",
		.platform_data	= &spiflash_data,
		.mode		= SPI_MODE_0,
		.max_speed_hz	= 30000000,
		.bus_num	= 0,
		.chip_select	= 0,
	},
};

/* spidev for MSIOF */
static struct spi_board_info spi_bus[] __initdata = {
	{
		.modalias       = "spidev",
		.max_speed_hz   = 6000000,
		.mode           = SPI_MODE_3,
		.bus_num        = 2,
		.chip_select    = 0,
	},
};

/* GPIO KEY */
#define GPIO_KEY(c, g, d, ...) \
	{ .code = c, .gpio = g, .desc = d, .active_low = 1 }

static __initdata struct gpio_keys_button gpio_buttons[] = {
	GPIO_KEY(KEY_4,		RCAR_GP_PIN(1, 28),	"SW2-pin4"),
	GPIO_KEY(KEY_3,		RCAR_GP_PIN(1, 26),	"SW2-pin3"),
	GPIO_KEY(KEY_2,		RCAR_GP_PIN(1, 24),	"SW2-pin2"),
	GPIO_KEY(KEY_1,		RCAR_GP_PIN(1, 14),	"SW2-pin1"),
};

static __initdata struct gpio_keys_platform_data lager_keys_pdata = {
	.buttons	= gpio_buttons,
	.nbuttons	= ARRAY_SIZE(gpio_buttons),
};

/* VSP1 */
static struct vsp1_platform_data lager_vspr_pdata = {
	.features = 0,
	.rpf_count = 5,
	.uds_count = 1,
	.wpf_count = 4,
};

static struct vsp1_platform_data lager_vsps_pdata = {
	.features = 0,
	.rpf_count = 5,
	.uds_count = 3,
	.wpf_count = 4,
};

static struct vsp1_platform_data lager_vspd0_pdata = {
	.features = VSP1_HAS_LIF,
	.rpf_count = 4,
	.uds_count = 1,
	.wpf_count = 4,
};

static struct vsp1_platform_data lager_vspd1_pdata = {
	.features = VSP1_HAS_LIF,
	.rpf_count = 4,
	.uds_count = 1,
	.wpf_count = 4,
};

static const struct pinctrl_map lager_pinctrl_map[] = {
	/* DU (CN10: ARGB0, CN13: LVDS) */
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du-r8a7790", "pfc-r8a7790",
				  "du_rgb666", "du"),
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du-r8a7790", "pfc-r8a7790",
				  "du_sync_1", "du"),
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du-r8a7790", "pfc-r8a7790",
				  "du_clk_out_0", "du"),
	/* SCIF0 (CN19: DEBUG SERIAL0) */
	PIN_MAP_MUX_GROUP_DEFAULT("sh-sci.6", "pfc-r8a7790",
				  "scif0_data", "scif0"),
	/* SCIF1 (CN20: DEBUG SERIAL1) */
	PIN_MAP_MUX_GROUP_DEFAULT("sh-sci.7", "pfc-r8a7790",
				  "scif1_data", "scif1"),
	/* SDHI0 */
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.0", "pfc-r8a7790",
				  "sdhi0_data4", "sdhi0"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.0", "pfc-r8a7790",
				  "sdhi0_ctrl", "sdhi0"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.0", "pfc-r8a7790",
				  "sdhi0_cd", "sdhi0"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.0", "pfc-r8a7790",
				  "sdhi0_wp", "sdhi0"),
	/* SDHI2 */
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.2", "pfc-r8a7790",
				  "sdhi2_data4", "sdhi2"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.2", "pfc-r8a7790",
				  "sdhi2_ctrl", "sdhi2"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.2", "pfc-r8a7790",
				  "sdhi2_cd", "sdhi2"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.2", "pfc-r8a7790",
				  "sdhi2_wp", "sdhi2"),
	/* USB0 */
	PIN_MAP_MUX_GROUP_DEFAULT("ehci-platform.0", "pfc-r8a7790",
				  "usb0", "usb0"),
	/* USB1 */
	PIN_MAP_MUX_GROUP_DEFAULT("ehci-platform.1", "pfc-r8a7790",
				  "usb1", "usb1"),
	/* USB2 */
	PIN_MAP_MUX_GROUP_DEFAULT("ehci-platform.2", "pfc-r8a7790",
				  "usb2", "usb2"),
	/* MMC1 */
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mmcif.1", "pfc-r8a7790",
				  "mmc1_data8", "mmc1"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mmcif.1", "pfc-r8a7790",
				  "mmc1_ctrl", "mmc1"),
	/* MSIOF1 */
	PIN_MAP_MUX_GROUP_DEFAULT("spi_sh_msiof.1", "pfc-r8a7790",
				  "msiof1_clk", "msiof1"),
	PIN_MAP_MUX_GROUP_DEFAULT("spi_sh_msiof.1", "pfc-r8a7790",
				  "msiof1_ctrl", "msiof1"),
	PIN_MAP_MUX_GROUP_DEFAULT("spi_sh_msiof.1", "pfc-r8a7790",
				  "msiof1_data", "msiof1"),
	/* VIN0 */
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7790",
				  "vin0_data_g", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7790",
				  "vin0_data_r", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7790",
				  "vin0_data_b", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7790",
				  "vin0_hsync_signal", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7790",
				  "vin0_vsync_signal", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7790",
				  "vin0_field_signal", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7790",
				  "vin0_data_enable", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7790",
				  "vin0_clk", "vin0"),
	/* VIN1 */
	PIN_MAP_MUX_GROUP_DEFAULT("vin.1", "pfc-r8a7790",
				  "vin1_data", "vin1"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.1", "pfc-r8a7790",
				  "vin1_clk", "vin1"),
};

static void lager_restart(char mode, const char *cmd)
{
	struct i2c_adapter *adap;
	struct i2c_client *client;
	u8 val;
	int busnum = 3;

	adap = i2c_get_adapter(busnum);
	if (!adap) {
		pr_err("failed to get adapter i2c%d\n", busnum);
		return;
	}

	client = i2c_new_device(adap, &lager_i2c3_devices[0]);
	if (!client)
		pr_err("failed to register %s to i2c%d\n",
		       lager_i2c3_devices[0].type, busnum);

	i2c_put_adapter(adap);

	val = i2c_smbus_read_byte_data(client, 0x13);

	if (val < 0)
		pr_err("couldn't access da9063\n");

	val |= 0x02;

	i2c_smbus_write_byte_data(client, 0x13, val);
}

static struct resource sdhi0_resources[] = {
	[0] = {
		.name	= "sdhi0",
		.start	= 0xee100000,
		.end	= 0xee1003ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(165),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct sh_mobile_sdhi_info sdhi0_platform_data = {
	.dma_slave_tx	= SHDMA_SLAVE_SDHI0_TX,
	.dma_slave_rx	= SHDMA_SLAVE_SDHI0_RX,
	.tmio_caps	= MMC_CAP_SD_HIGHSPEED | MMC_CAP_SDIO_IRQ,
	.tmio_caps2	= MMC_CAP2_NO_2BLKS_READ,
	.tmio_flags	= TMIO_MMC_BUFF_16BITACC_ACTIVE_HIGH
				| TMIO_MMC_CLK_NO_SLEEP
				| TMIO_MMC_HAS_IDLE_WAIT
				| TMIO_MMC_NO_CTL_CLK_AND_WAIT_CTL
				| TMIO_MMC_NO_CTL_RESET_SDIO
				| TMIO_MMC_SDIO_STATUS_QUIRK
				| TMIO_MMC_WRPROTECT_DISABLE,
};

static struct platform_device sdhi0_device = {
	.name = "sh_mobile_sdhi",
	.num_resources = ARRAY_SIZE(sdhi0_resources),
	.resource = sdhi0_resources,
	.id = 0,
	.dev = {
		.platform_data = &sdhi0_platform_data,
	}
};

static struct resource sdhi1_resources[] = {
	[0] = {
		.name	= "sdhi1",
		.start	= 0xee120000,
		.end	= 0xee1203ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(166),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct sh_mobile_sdhi_info sdhi1_platform_data = {
	.dma_slave_tx	= SHDMA_SLAVE_SDHI1_TX,
	.dma_slave_rx	= SHDMA_SLAVE_SDHI1_RX,
	.tmio_caps	= MMC_CAP_SD_HIGHSPEED | MMC_CAP_SDIO_IRQ,
	.tmio_caps2	= MMC_CAP2_NO_2BLKS_READ,
	.tmio_flags	= TMIO_MMC_BUFF_16BITACC_ACTIVE_HIGH
				| TMIO_MMC_CLK_NO_SLEEP
				| TMIO_MMC_HAS_IDLE_WAIT
				| TMIO_MMC_NO_CTL_CLK_AND_WAIT_CTL
				| TMIO_MMC_NO_CTL_RESET_SDIO
				| TMIO_MMC_SDIO_STATUS_QUIRK
				| TMIO_MMC_WRPROTECT_DISABLE,
};

static struct platform_device sdhi1_device = {
	.name = "sh_mobile_sdhi",
	.num_resources = ARRAY_SIZE(sdhi1_resources),
	.resource = sdhi1_resources,
	.id = 1,
	.dev = {
		.platform_data = &sdhi1_platform_data,
	}
};

static struct resource sdhi2_resources[] = {
	[0] = {
		.name	= "sdhi2",
		.start	= 0xee140000,
		.end	= 0xee1400ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(167),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct sh_mobile_sdhi_info sdhi2_platform_data = {
	.dma_slave_tx	= SHDMA_SLAVE_SDHI2_TX,
	.dma_slave_rx	= SHDMA_SLAVE_SDHI2_RX,
	.tmio_caps	= MMC_CAP_SD_HIGHSPEED | MMC_CAP_SDIO_IRQ,
	.tmio_caps2	= MMC_CAP2_NO_2BLKS_READ,
	.tmio_flags	= TMIO_MMC_CHECK_ILL_FUNC
				| TMIO_MMC_CLK_NO_SLEEP
				| TMIO_MMC_HAS_IDLE_WAIT
				| TMIO_MMC_NO_CTL_CLK_AND_WAIT_CTL
				| TMIO_MMC_NO_CTL_RESET_SDIO
				| TMIO_MMC_SDIO_STATUS_QUIRK
				| TMIO_MMC_WRPROTECT_DISABLE,
};

static struct platform_device sdhi2_device = {
	.name = "sh_mobile_sdhi",
	.num_resources = ARRAY_SIZE(sdhi2_resources),
	.resource = sdhi2_resources,
	.id = 2,
	.dev = {
		.platform_data = &sdhi2_platform_data,
	}
};

static struct resource sdhi3_resources[] = {
	[0] = {
		.name	= "sdhi3",
		.start	= 0xee160000,
		.end	= 0xee1600ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(168),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct sh_mobile_sdhi_info sdhi3_platform_data = {
	.dma_slave_tx	= SHDMA_SLAVE_SDHI3_TX,
	.dma_slave_rx	= SHDMA_SLAVE_SDHI3_RX,
	.tmio_caps	= MMC_CAP_SD_HIGHSPEED | MMC_CAP_SDIO_IRQ,
	.tmio_caps2	= MMC_CAP2_NO_2BLKS_READ,
	.tmio_flags	= TMIO_MMC_CHECK_ILL_FUNC
				| TMIO_MMC_CLK_NO_SLEEP
				| TMIO_MMC_HAS_IDLE_WAIT
				| TMIO_MMC_NO_CTL_CLK_AND_WAIT_CTL
				| TMIO_MMC_NO_CTL_RESET_SDIO
				| TMIO_MMC_SDIO_STATUS_QUIRK
				| TMIO_MMC_WRPROTECT_DISABLE,
};

static struct platform_device sdhi3_device = {
	.name = "sh_mobile_sdhi",
	.num_resources = ARRAY_SIZE(sdhi3_resources),
	.resource = sdhi3_resources,
	.id = 3,
	.dev = {
		.platform_data = &sdhi3_platform_data,
	}
};

static void shmmcif_set_pwr(struct platform_device *pdev, int state)
{
}

static void shmmcif_down_pwr(struct platform_device *pdev)
{
}

static int shmmcif_get_cd(struct platform_device *pdev)
{
	return 1;
}

static struct resource sh_mmcif0_resources[] = {
	[0] = {
		.name	= "mmc0",
		.start	= 0xEE200000,
		.end	= 0xEE200080-1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(169),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct sh_mmcif_plat_data sh_mmcif0_plat = {
	.set_pwr	= shmmcif_set_pwr,
	.down_pwr	= shmmcif_down_pwr,
	.get_cd		= shmmcif_get_cd,
	.slave_id_tx	= SHDMA_SLAVE_MMC0_TX,
	.slave_id_rx	= SHDMA_SLAVE_MMC0_RX,
	.use_cd_gpio	= 0,
	.cd_gpio	= 0,
	.sup_pclk	= 0 ,
	.caps		= MMC_CAP_MMC_HIGHSPEED |
			  MMC_CAP_8_BIT_DATA | MMC_CAP_NONREMOVABLE ,
	.ocr		= MMC_VDD_32_33 | MMC_VDD_33_34 ,
};

static struct platform_device mmc0_device = {
	.name		= "sh_mmcif",
	.num_resources	= ARRAY_SIZE(sh_mmcif0_resources),
	.resource	= sh_mmcif0_resources,
	.id		= 0,
	.dev = {
		.platform_data	= &sh_mmcif0_plat,
	}
};

static struct resource sh_mmcif1_resources[] = {
	[0] = {
		.name	= "mmc1",
		.start	= 0xEE220000,
		.end	= 0xEE220080-1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(170),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct sh_mmcif_plat_data sh_mmcif1_plat = {
	.set_pwr	= shmmcif_set_pwr,
	.down_pwr	= shmmcif_down_pwr,
	.get_cd		= shmmcif_get_cd,
	.slave_id_tx	= SHDMA_SLAVE_MMC1_TX,
	.slave_id_rx	= SHDMA_SLAVE_MMC1_RX,
	.use_cd_gpio	= 0,
	.cd_gpio	= 0,
	.sup_pclk	= 0 ,
	.caps		= MMC_CAP_MMC_HIGHSPEED |
			  MMC_CAP_8_BIT_DATA | MMC_CAP_NONREMOVABLE ,
	.ocr		= MMC_VDD_32_33 | MMC_VDD_33_34 ,
};

static struct platform_device mmc1_device = {
	.name		= "sh_mmcif",
	.num_resources	= ARRAY_SIZE(sh_mmcif1_resources),
	.resource	= sh_mmcif1_resources,
	.id		= 1,
	.dev = {
		.platform_data	= &sh_mmcif1_plat,
	}
};

static struct platform_device *lager_devices[] __initdata = {
	&sdhi0_device,
	&sdhi1_device,
	&sdhi2_device,
	&sdhi3_device,
	&mmc0_device,
	&mmc1_device,
};

static void __init lager_add_standard_devices(void)
{
	r8a7790_clock_init();

	pinctrl_register_mappings(lager_pinctrl_map,
				  ARRAY_SIZE(lager_pinctrl_map));
	r8a7790_pinmux_init();

	r8a7790_add_standard_devices();
	platform_add_devices(lager_devices, ARRAY_SIZE(lager_devices));

	i2c_register_board_info(2, lager_i2c2_devices,
				ARRAY_SIZE(lager_i2c2_devices));

	r8a7790_add_du_device(&lager_du_pdata);

	platform_device_register_data(&platform_bus, "leds-gpio", -1,
				      &lager_leds_pdata,
				      sizeof(lager_leds_pdata));
	platform_device_register_data(&platform_bus, "gpio-keys", -1,
				      &lager_keys_pdata,
				      sizeof(lager_keys_pdata));

	r8a7790_add_vsp1_device(&lager_vspd0_pdata, 2);

	/* QSPI flash memory */
	spi_register_board_info(spi_info, ARRAY_SIZE(spi_info));

	/* spidev for MSIOF */
	spi_register_board_info(spi_bus, ARRAY_SIZE(spi_bus));
}

static const char *lager_boards_compat_dt[] __initdata = {
	"renesas,lager",
	NULL,
};

DT_MACHINE_START(LAGER_DT, "lager")
	.smp		= smp_ops(r8a7790_smp_ops),
	.init_irq	= r8a7790_init_irq,
	.timer		= &r8a7790_timer,
	.init_machine	= lager_add_standard_devices,
	.restart	= lager_restart,
	.dt_compat	= lager_boards_compat_dt,
MACHINE_END
