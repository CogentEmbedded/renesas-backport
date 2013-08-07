/*
 * Koelsch board support
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
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
#include <media/vin.h>
#include <mach/r8a7791.h>
#include <mach/irqs.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

static struct i2c_board_info koelsch_i2c_devices[] = {
	{ I2C_BOARD_INFO("ak4642", 0x12), },
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
                .bus_num        = 1,
                .chip_select    = 0,
        },
};

/* DU */
static struct rcar_du_encoder_data koelsch_du_encoders[] = {
	{
		.type = RCAR_DU_ENCODER_NONE,
		.output = RCAR_DU_OUTPUT_LVDS0,
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
	},
#if defined(CONFIG_DRM_ADV7511)
	{
		.type = RCAR_DU_ENCODER_HDMI,
		.output = RCAR_DU_OUTPUT_DPAD0,
	},
#endif
};

static struct rcar_du_platform_data koelsch_du_pdata = {
	.encoders = koelsch_du_encoders,
	.num_encoders = ARRAY_SIZE(koelsch_du_encoders),
};

static const struct pinctrl_map koelsch_pinctrl_map[] = {
	/* SCIF0 (CN19: DEBUG SERIAL0) */
	PIN_MAP_MUX_GROUP_DEFAULT("sh-sci.6", "pfc-r8a7791",
				  "scif0_data_d", "scif0"),
	/* SCIF1 (CN20: DEBUG SERIAL1) */
	PIN_MAP_MUX_GROUP_DEFAULT("sh-sci.7", "pfc-r8a7791",
				  "scif1_data_d", "scif1"),

	/* USB0 */
	PIN_MAP_MUX_GROUP_DEFAULT("ehci-platform.0", "pfc-r8a7791",
				  "usb0", "usb0"),
	/* USB1 */
	PIN_MAP_MUX_GROUP_DEFAULT("ehci-platform.1", "pfc-r8a7791",
				  "usb1", "usb1"),

	/* MSIOF0 */
	PIN_MAP_MUX_GROUP_DEFAULT("spi_sh_msiof.0", "pfc-r8a7791",
				  "msiof0_clk", "msiof0"),
	PIN_MAP_MUX_GROUP_DEFAULT("spi_sh_msiof.0", "pfc-r8a7791",
				  "msiof0_ctrl", "msiof0"),
	PIN_MAP_MUX_GROUP_DEFAULT("spi_sh_msiof.0", "pfc-r8a7791",
				  "msiof0_data", "msiof0"),
	/* VIN0 */
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7791",
				  "vin0_data_g", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7791",
				  "vin0_data_r", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7791",
				  "vin0_data_b", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7791",
				  "vin0_hsync_signal", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7791",
				  "vin0_vsync_signal", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7791",
				  "vin0_field_signal", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7791",
				  "vin0_data_enable", "vin0"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.0", "pfc-r8a7791",
				  "vin0_clk", "vin0"),
	/* VIN1 */
	PIN_MAP_MUX_GROUP_DEFAULT("vin.1", "pfc-r8a7791",
				  "vin1_data", "vin1"),
	PIN_MAP_MUX_GROUP_DEFAULT("vin.1", "pfc-r8a7791",
				  "vin1_clk", "vin1"),

	/* SDHI0 */
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.0", "pfc-r8a7791",
				  "sdhi0_data4", "sdhi0"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.0", "pfc-r8a7791",
				  "sdhi0_ctrl", "sdhi0"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.0", "pfc-r8a7791",
				  "sdhi0_cd", "sdhi0"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.0", "pfc-r8a7791",
				  "sdhi0_wp", "sdhi0"),
	/* SDHI1 */
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.1", "pfc-r8a7791",
				  "sdhi1_data4", "sdhi1"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.1", "pfc-r8a7791",
				  "sdhi1_ctrl", "sdhi1"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.1", "pfc-r8a7791",
				  "sdhi1_cd", "sdhi1"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.1", "pfc-r8a7791",
				  "sdhi1_wp", "sdhi1"),
	/* SDHI2 */
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.2", "pfc-r8a7791",
				  "sdhi2_data4", "sdhi2"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.2", "pfc-r8a7791",
				  "sdhi2_ctrl", "sdhi2"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.2", "pfc-r8a7791",
				  "sdhi2_cd", "sdhi2"),
	PIN_MAP_MUX_GROUP_DEFAULT("sh_mobile_sdhi.2", "pfc-r8a7791",
				  "sdhi2_wp", "sdhi2"),
	/* DU (CN11: HDMI, CN13: LVDS) */
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du-r8a7791", "pfc-r8a7791",
				  "du_rgb888", "du"),
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du-r8a7791", "pfc-r8a7791",
				  "du_sync_1", "du"),
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du-r8a7791", "pfc-r8a7791",
				  "du_sync_0", "du"),
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du-r8a7791", "pfc-r8a7791",
				  "du_clk_out_0", "du"),
	PIN_MAP_MUX_GROUP_DEFAULT("rcar-du-r8a7791", "pfc-r8a7791",
				  "du_clk_out_1", "du"),
};

static struct i2c_board_info koelsch_i2c_camera[] = {
	{ I2C_BOARD_INFO("adv7612", 0x4C), },
	{ I2C_BOARD_INFO("adv7180", 0x20), },
};

static void camera_power_on(void)
{
	return;
}

static void camera_power_off(void)
{
	return;
}

static int adv7612_power(struct device *dev, int mode)
{
	if (mode)
		camera_power_on();
	else
		camera_power_off();

	return 0;
}

static int adv7180_power(struct device *dev, int mode)
{
	if (mode)
		camera_power_on();
	else
		camera_power_off();

	return 0;
}

static struct soc_camera_link adv7612_ch0_link = {
	.bus_id = 0,
	.power  = adv7612_power,
	.board_info = &koelsch_i2c_camera[0],
	.i2c_adapter_id = 2,
	.module_name = "adv7612",
};

static struct soc_camera_link adv7180_ch1_link = {
	.bus_id = 1,
	.power  = adv7180_power,
	.board_info = &koelsch_i2c_camera[1],
	.i2c_adapter_id = 2,
	.module_name = "adv7180",
};

static struct platform_device soc_camera_device[] = {
	{
		.name = "soc-camera-pdrv",
		.id = 0,
		.dev = {
			.platform_data = &adv7612_ch0_link,
		},
	},
	{
		.name = "soc-camera-pdrv",
		.id = 1,
		.dev = {
			 .platform_data = &adv7180_ch1_link,
		},
	},
};

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
				| TMIO_MMC_SDIO_STATUS_QUIRK,
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
		.start	= 0xee140000,
		.end	= 0xee1400ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(167),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct sh_mobile_sdhi_info sdhi1_platform_data = {
	.dma_slave_tx	= SHDMA_SLAVE_SDHI1_TX,
	.dma_slave_rx	= SHDMA_SLAVE_SDHI1_RX,
	.tmio_caps	= MMC_CAP_SD_HIGHSPEED | MMC_CAP_SDIO_IRQ,
	.tmio_caps2	= MMC_CAP2_NO_2BLKS_READ,
	.tmio_flags	= TMIO_MMC_CHECK_ILL_FUNC
				| TMIO_MMC_CLK_NO_SLEEP
				| TMIO_MMC_HAS_IDLE_WAIT
				| TMIO_MMC_NO_CTL_CLK_AND_WAIT_CTL
				| TMIO_MMC_NO_CTL_RESET_SDIO
				| TMIO_MMC_SDIO_STATUS_QUIRK,
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
		.start	= 0xee160000,
		.end	= 0xee1600ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(168),
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

/* MMC */
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

static struct resource sh_mmcif_resources[] = {
	[0] = {
		.name	= "mmc",
		.start	= 0xEE200000,
		.end	= 0xEE200080-1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(169),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct sh_mmcif_plat_data sh_mmcif_plat = {
	.set_pwr	= shmmcif_set_pwr,
	.down_pwr	= shmmcif_down_pwr,
	.get_cd		= shmmcif_get_cd,
	.slave_id_tx	= SHDMA_SLAVE_MMC_TX,
	.slave_id_rx	= SHDMA_SLAVE_MMC_RX,
	.use_cd_gpio	= 0,
	.cd_gpio	= 0,
	.sup_pclk	= 0 ,
	.caps		= MMC_CAP_MMC_HIGHSPEED |
			  MMC_CAP_8_BIT_DATA | MMC_CAP_NONREMOVABLE ,
	.ocr		= MMC_VDD_32_33 | MMC_VDD_33_34 ,
};

static struct platform_device mmc_device = {
	.name		= "sh_mmcif",
	.num_resources	= ARRAY_SIZE(sh_mmcif_resources),
	.resource	= sh_mmcif_resources,
	.id		= 0,
	.dev = {
		.platform_data	= &sh_mmcif_plat,
	}
};

static struct platform_device *koelsch_devices[] __initdata = {
	&soc_camera_device[0],
	&soc_camera_device[1],
	&sdhi0_device,
	&sdhi1_device,
	&sdhi2_device,
	&mmc_device,
};


static void __init koelsch_add_standard_devices(void)
{
	r8a7791_clock_init();

	pinctrl_register_mappings(koelsch_pinctrl_map,
				  ARRAY_SIZE(koelsch_pinctrl_map));
	r8a7791_pinmux_init();

	r8a7791_add_du_device(&koelsch_du_pdata);

	r8a7791_add_standard_devices();

	platform_add_devices(koelsch_devices,
			     ARRAY_SIZE(koelsch_devices));

	i2c_register_board_info(2, koelsch_i2c_devices,
				ARRAY_SIZE(koelsch_i2c_devices));

	/* QSPI flash memory */
	spi_register_board_info(spi_info, ARRAY_SIZE(spi_info));

	/* spidev for MSIOF */
	spi_register_board_info(spi_bus, ARRAY_SIZE(spi_bus));
}

static const char *koelsch_boards_compat_dt[] __initdata = {
	"renesas,koelsch",
	NULL,
};

DT_MACHINE_START(KOELSCH_DT, "koelsch")
	.smp		= smp_ops(r8a7791_smp_ops),
	.init_irq	= r8a7791_init_irq,
	.timer		= &r8a7791_timer,
	.init_machine	= koelsch_add_standard_devices,
	.dt_compat	= koelsch_boards_compat_dt,
MACHINE_END
