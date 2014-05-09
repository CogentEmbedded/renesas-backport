/*
 * Koelsch board support - Reference DT implementation
 *
 * Copyright (C) 2013  Renesas Electronics Corporation
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
#include <linux/kernel.h>
#include <linux/mfd/tmio.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sh_mobile_sdhi.h>
#include <linux/of_platform.h>
#include <linux/platform_data/rcar-du.h>
#include <mach/clock.h>
#include <mach/common.h>
#include <mach/irqs.h>
#include <mach/rcar-gen2.h>
#include <mach/r8a7791.h>
#include <asm/mach/arch.h>
#include <sound/rcar_snd.h>
#include <sound/simple_card.h>

/* SDHI0 */
static struct sh_mobile_sdhi_info sdhi0_info __initdata = {
	.dma_slave_tx	= SYS_DMAC_SLAVE_SDHI0_TX,
	.dma_slave_rx	= SYS_DMAC_SLAVE_SDHI0_RX,
	.tmio_caps	= MMC_CAP_SD_HIGHSPEED | MMC_CAP_SDIO_IRQ |
			  MMC_CAP_POWER_OFF_CARD,
	.tmio_caps2	= MMC_CAP2_NO_MULTI_READ,
	.tmio_flags	= TMIO_MMC_HAS_IDLE_WAIT,
	/* FIXME
	 * GPIO and GPIO regulator came from DT */
	.tmio_ocr_mask  = MMC_VDD_32_33 | MMC_VDD_33_34,
};

static struct resource sdhi0_resources[] __initdata = {
	DEFINE_RES_MEM(0xee100000, 0x200),
	DEFINE_RES_IRQ(gic_spi(165)),
};

/* SDHI2 */
static struct sh_mobile_sdhi_info sdhi1_info __initdata = {
	.dma_slave_tx	= SYS_DMAC_SLAVE_SDHI2_TX,
	.dma_slave_rx	= SYS_DMAC_SLAVE_SDHI2_RX,
	.tmio_caps	= MMC_CAP_SD_HIGHSPEED | MMC_CAP_SDIO_IRQ |
			  MMC_CAP_POWER_OFF_CARD,
	.tmio_caps2	= MMC_CAP2_NO_MULTI_READ,
	.tmio_flags	= TMIO_MMC_HAS_IDLE_WAIT,
	/* FIXME
	 * GPIO and GPIO regulator came from DT */
	.tmio_ocr_mask  = MMC_VDD_32_33 | MMC_VDD_33_34,
};

static struct resource sdhi1_resources[] __initdata = {
	DEFINE_RES_MEM(0xee140000, 0x100),
	DEFINE_RES_IRQ(gic_spi(167)),
};

/* SDHI3 */
static struct sh_mobile_sdhi_info sdhi2_info __initdata = {
	.dma_slave_tx	= SYS_DMAC_SLAVE_SDHI3_TX,
	.dma_slave_rx	= SYS_DMAC_SLAVE_SDHI3_RX,
	.tmio_caps	= MMC_CAP_SD_HIGHSPEED | MMC_CAP_SDIO_IRQ |
			  MMC_CAP_POWER_OFF_CARD,
	.tmio_caps2	= MMC_CAP2_NO_MULTI_READ,
	.tmio_flags	= TMIO_MMC_HAS_IDLE_WAIT |
			  TMIO_MMC_WRPROTECT_DISABLE,
	/* FIXME
	 * GPIO and GPIO regulator came from DT */
	.tmio_ocr_mask  = MMC_VDD_32_33 | MMC_VDD_33_34,
};

static struct resource sdhi2_resources[] __initdata = {
	DEFINE_RES_MEM(0xee160000, 0x100),
	DEFINE_RES_IRQ(gic_spi(168)),
};

static void __init koelsch_add_sdhi_devices(void)
{

	platform_device_register_resndata(&platform_bus, "sh_mobile_sdhi", 0,
					  sdhi0_resources, ARRAY_SIZE(sdhi0_resources),
					  &sdhi0_info, sizeof(struct sh_mobile_sdhi_info));

	platform_device_register_resndata(&platform_bus, "sh_mobile_sdhi", 1,
					  sdhi1_resources, ARRAY_SIZE(sdhi1_resources),
					  &sdhi1_info, sizeof(struct sh_mobile_sdhi_info));

	platform_device_register_resndata(&platform_bus, "sh_mobile_sdhi", 2,
					  sdhi2_resources, ARRAY_SIZE(sdhi2_resources),
					  &sdhi2_info, sizeof(struct sh_mobile_sdhi_info));
}

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
};

static struct rcar_du_platform_data koelsch_du_pdata = {
	.encoders = koelsch_du_encoders,
	.num_encoders = ARRAY_SIZE(koelsch_du_encoders),
};

static const struct resource du_resources[] __initconst = {
	DEFINE_RES_MEM(0xfeb00000, 0x40000),
	DEFINE_RES_MEM_NAMED(0xfeb90000, 0x1c, "lvds.0"),
	DEFINE_RES_IRQ(gic_spi(256)),
	DEFINE_RES_IRQ(gic_spi(268)),
};

static void __init koelsch_add_du_device(void)
{
	struct platform_device_info info = {
		.name = "rcar-du-r8a7791",
		.id = -1,
		.res = du_resources,
		.num_res = ARRAY_SIZE(du_resources),
		.data = &koelsch_du_pdata,
		.size_data = sizeof(koelsch_du_pdata),
		.dma_mask = DMA_BIT_MASK(32),
	};

	platform_device_register_full(&info);
}

/* Sound */
static struct rsnd_ssi_platform_info rsnd_ssi[] = {
	RSND_SSI(AUDIOPP_DMAC_SLAVE_CMD0_TO_SSI0, gic_spi(370), 0),
	RSND_SSI(AUDIOPP_DMAC_SLAVE_SSI1_TO_SCU1, gic_spi(371), RSND_SSI_CLK_PIN_SHARE),
};

static struct rsnd_src_platform_info rsnd_src[2] = {
	RSND_SRC(0, AUDIO_DMAC_SLAVE_SCU0_TX),
	RSND_SRC(0, AUDIO_DMAC_SLAVE_SCU1_RX),
};

static struct rsnd_dvc_platform_info rsnd_dvc = {
};

static struct rsnd_dai_platform_info rsnd_dai = {
	.playback = { .ssi = &rsnd_ssi[0], .src = &rsnd_src[0], .dvc = &rsnd_dvc, },
	.capture  = { .ssi = &rsnd_ssi[1], .src = &rsnd_src[1], },
};

static struct rcar_snd_info rsnd_info = {
	.flags		= RSND_GEN2,
	.ssi_info	= rsnd_ssi,
	.ssi_info_nr	= ARRAY_SIZE(rsnd_ssi),
	.src_info	= rsnd_src,
	.src_info_nr	= ARRAY_SIZE(rsnd_src),
	.dvc_info	= &rsnd_dvc,
	.dvc_info_nr	= 1,
	.dai_info	= &rsnd_dai,
	.dai_info_nr	= 1,
};

static struct asoc_simple_card_info rsnd_card_info = {
	.name		= "SSI01-AK4643",
	.codec		= "ak4642-codec.2-0012",
	.platform	= "rcar_sound",
	.daifmt		= SND_SOC_DAIFMT_LEFT_J | SND_SOC_DAIFMT_CBM_CFM,
	.cpu_dai = {
		.name	= "rcar_sound",
	},
	.codec_dai = {
		.name	= "ak4642-hifi",
		.sysclk	= 11289600,
	},
};

static void __init koelsch_add_rsnd_device(void)
{
	struct resource rsnd_resources[] = {
		[RSND_GEN2_SCU]  = DEFINE_RES_MEM(0xec500000, 0x1000),
		[RSND_GEN2_ADG]  = DEFINE_RES_MEM(0xec5a0000, 0x100),
		[RSND_GEN2_SSIU] = DEFINE_RES_MEM(0xec540000, 0x1000),
		[RSND_GEN2_SSI]  = DEFINE_RES_MEM(0xec541000, 0x1280),
	};

	struct platform_device_info cardinfo = {
		.parent         = &platform_bus,
		.name           = "asoc-simple-card",
		.id             = -1,
		.data           = &rsnd_card_info,
		.size_data      = sizeof(struct asoc_simple_card_info),
		.dma_mask       = DMA_BIT_MASK(32),
	};

	platform_device_register_resndata(
		&platform_bus, "rcar_sound", -1,
		rsnd_resources, ARRAY_SIZE(rsnd_resources),
		&rsnd_info, sizeof(rsnd_info));

	platform_device_register_full(&cardinfo);
}

/*
 * This is a really crude hack to provide clkdev support to platform
 * devices until they get moved to DT.
 */
static const struct clk_name clk_names[] __initconst = {
	{ "cmt0", NULL, "sh_cmt.0" },
	{ "scifa0", NULL, "sh-sci.0" },
	{ "scifa1", NULL, "sh-sci.1" },
	{ "scifb0", NULL, "sh-sci.2" },
	{ "scifb1", NULL, "sh-sci.3" },
	{ "scifb2", NULL, "sh-sci.4" },
	{ "scifa2", NULL, "sh-sci.5" },
	{ "scif0", NULL, "sh-sci.6" },
	{ "scif1", NULL, "sh-sci.7" },
	{ "scif2", NULL, "sh-sci.8" },
	{ "scif3", NULL, "sh-sci.9" },
	{ "scif4", NULL, "sh-sci.10" },
	{ "scif5", NULL, "sh-sci.11" },
	{ "scifa3", NULL, "sh-sci.12" },
	{ "scifa4", NULL, "sh-sci.13" },
	{ "scifa5", NULL, "sh-sci.14" },
	{ "hscif0", NULL, "sh-sci.15" },
	{ "hscif1", NULL, "sh-sci.16" },
	{ "hscif2", NULL, "sh-sci.17" },
	{ "du0", "du.0", "rcar-du-r8a7791" },
	{ "du1", "du.1", "rcar-du-r8a7791" },
	{ "lvds0", "lvds.0", "rcar-du-r8a7791" },
	{ "ssi0", "ssi.0", "rcar_sound" },
	{ "ssi1", "ssi.1", "rcar_sound" },
	{ "src0", "src.0", "rcar_sound" },
	{ "src1", "src.1", "rcar_sound" },
	{ "dvc0", "dvc.0", "rcar_sound" },
};

/*
 * This is a really crude hack to work around core platform clock issues
 */
static const struct clk_name clk_enables[] __initconst = {
	{ "ether", NULL, "ee700000.ethernet" },
	{ "i2c2", NULL, "e6530000.i2c" },
	{ "msiof0", NULL, "e6e20000.spi" },
	{ "qspi_mod", NULL, "e6b10000.spi" },
	{ "sdhi0", NULL, "sh_mobile_sdhi.0" },
	{ "sdhi1", NULL, "sh_mobile_sdhi.1" },
	{ "sdhi2", NULL, "sh_mobile_sdhi.2" },
	{ "thermal", NULL, "e61f0000.thermal" },
	{ "ssi", NULL, "rcar_sound" },
	{ "scu", NULL, "rcar_sound" },
	{ "dmal", NULL, "sh-dma-engine.0" },
	{ "dmah", NULL, "sh-dma-engine.1" },
};

static void __init koelsch_add_standard_devices(void)
{
	shmobile_clk_workaround(clk_names, ARRAY_SIZE(clk_names), false);
	shmobile_clk_workaround(clk_enables, ARRAY_SIZE(clk_enables), true);
	r8a7791_add_dt_devices();
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);

	koelsch_add_du_device();
	koelsch_add_rsnd_device();
	koelsch_add_sdhi_devices();
}

static const char * const koelsch_boards_compat_dt[] __initconst = {
	"renesas,koelsch",
	"renesas,koelsch-reference",
	NULL,
};

DT_MACHINE_START(KOELSCH_DT, "koelsch")
	.smp		= smp_ops(r8a7791_smp_ops),
	.init_early	= r8a7791_init_early,
	.init_time	= rcar_gen2_timer_init,
	.init_machine	= koelsch_add_standard_devices,
	.init_late	= shmobile_init_late,
	.dt_compat	= koelsch_boards_compat_dt,
MACHINE_END
