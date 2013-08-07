/*
 * r8a7791 clock framework support
 *
 * Copyright (C) 2013  Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/sh_clk.h>
#include <linux/clkdev.h>
#include <mach/clock.h>
#include <mach/common.h>

/*
 *   MD		EXTAL		PLL0	PLL1	PLL3
 * 14 13 19	(MHz)		*1	*1
 *---------------------------------------------------
 * 0  0  0	15 x 1		x172/2	x208/2	x106
 * 0  0  1	15 x 1		x172/2	x208/2	x88
 * 0  1  0	20 x 1		x130/2	x156/2	x80
 * 0  1  1	20 x 1		x130/2	x156/2	x66
 * 1  0  0	26 / 2		x200/2	x240/2	x122
 * 1  0  1	26 / 2		x200/2	x240/2	x102
 * 1  1  0	30 / 2		x172/2	x208/2	x106
 * 1  1  1	30 / 2		x172/2	x208/2	x88
 *
 * *1 :	Table 7.6 indicates VCO ouput (PLLx = VCO/2)
 *	see "p1 / 2" on R8A7791_CLOCK_ROOT() below
 */

#define MD(nr)	(1 << nr)

#define CPG_BASE 0xe6150000
#define CPG_LEN 0x1000

#define SMSTPCR0	0xE6150130
#define SMSTPCR1	0xE6150134
#define SMSTPCR2	0xe6150138
#define SMSTPCR3	0xE615013C
#define SMSTPCR5	0xE6150144
#define SMSTPCR7	0xe615014c
#define SMSTPCR8	0xE6150990
#define SMSTPCR9	0xE6150994
#define SMSTPCR10	0xE6150998
#define SMSTPCR11	0xE615099C

#define MODEMR		0xE6160060
#define SDCKCR		0xE6150074
#define SD1CKCR		0xE6150078
#define SD2CKCR		0xE615007C
#define MMC0CKCR	0xE6150240
#define SSPCKCR		0xE6150248
#define SSPRSCKCR	0xE615024C

static struct clk_mapping cpg_mapping = {
	.phys   = CPG_BASE,
	.len    = CPG_LEN,
};

static struct clk extal_clk = {
	/* .rate will be updated on r8a7791_clock_init() */
	.mapping	= &cpg_mapping,
};

static struct sh_clk_ops followparent_clk_ops = {
	.recalc	= followparent_recalc,
};

static struct clk main_clk = {
	/* .parent will be set r8a73a4_clock_init */
	.ops	= &followparent_clk_ops,
};

/*
 * clock ratio of these clock will be updated
 * on r8a7791_clock_init()
 */
SH_FIXED_RATIO_CLK_SET(pll1_clk,		main_clk,	1, 1);
SH_FIXED_RATIO_CLK_SET(pll3_clk,		main_clk,	1, 1);
SH_FIXED_RATIO_CLK_SET(qspi_clk,		pll1_clk,	1, 1);

/* fixed ratio clock */
SH_FIXED_RATIO_CLK_SET(extal_div2_clk,		extal_clk,	1, 2);
SH_FIXED_RATIO_CLK_SET(cp_clk,			extal_clk,	1, 2);

SH_FIXED_RATIO_CLK_SET(pll1_div2_clk,		pll1_clk,	1, 2);
SH_FIXED_RATIO_CLK_SET(zg_clk,			pll1_clk,	1, 3);
SH_FIXED_RATIO_CLK_SET(hp_clk,			pll1_clk,	1, 12);
SH_FIXED_RATIO_CLK_SET(p_clk,			pll1_clk,	1, 24);

SH_FIXED_RATIO_CLK_SET(mp_clk,			pll1_div2_clk,	1, 15);
SH_FIXED_RATIO_CLK_SET(zx_clk,			pll1_clk,	1, 3);

static struct clk sata0_clk = {
	.rate		= 100000000,
	.mapping	= &cpg_mapping,
};

static struct clk sata1_clk = {
	.rate		= 100000000,
	.mapping	= &cpg_mapping,
};

static struct clk *main_clks[] = {
	&extal_clk,
	&extal_div2_clk,
	&main_clk,
	&pll1_clk,
	&pll1_div2_clk,
	&pll3_clk,
	&zg_clk,
	&qspi_clk,
	&hp_clk,
	&p_clk,
	&mp_clk,
	&cp_clk,
	&sata0_clk,
	&sata1_clk,
	&zx_clk,
};

/* SDHI (DIV4) clock */
static int divisors[] = { 2, 3, 4, 6, 8, 12, 16, 18, 24, 0, 36, 48, 10 };

static struct clk_div_mult_table div4_div_mult_table = {
	.divisors = divisors,
	.nr_divisors = ARRAY_SIZE(divisors),
};

static struct clk_div4_table div4_table = {
	.div_mult_table = &div4_div_mult_table,
};

enum {
	DIV4_SD0, DIV4_NR
};

static struct clk div4_clks[DIV4_NR] = {
	[DIV4_SD0] = SH_CLK_DIV4(&pll1_clk, SDCKCR, 4, 0x1de0,
						CLK_ENABLE_ON_INIT),
};

/* DIV6 clocks */
enum {
	DIV6_MMC,
	DIV6_SD1, DIV6_SD2,
	DIV6_NR
};

static struct clk div6_clks[DIV6_NR] = {
	[DIV6_MMC]	= SH_CLK_DIV6(&pll1_div2_clk, MMC0CKCR, 0),
	[DIV6_SD1]	= SH_CLK_DIV6(&pll1_div2_clk, SD1CKCR, 0),
	[DIV6_SD2]	= SH_CLK_DIV6(&pll1_div2_clk, SD2CKCR, 0),
};


/* MSTP */
enum {
	MSTP112,
	MSTP219, MSTP218,
	MSTP314, MSTP312, MSTP311,
	MSTP315,
	MSTP502, MSTP501,
	MSTP721, MSTP720,
	MSTP719, MSTP718, MSTP715, MSTP714,
	MSTP815, MSTP814,
	MSTP811, MSTP810, MSTP809,
	MSTP216, MSTP207, MSTP206,
	MSTP204, MSTP203, MSTP202, MSTP1105, MSTP1106, MSTP1107,
	MSTP704, MSTP703, MSTP328,
	MSTP000, MSTP208, MSTP205,
	MSTP917,
	MSTP931, MSTP930, MSTP929, MSTP928, MSTP927, MSTP925, MSTP922,
	MSTP1031, MSTP1030, MSTP1019, MSTP1018, MSTP1017, MSTP1015, MSTP1014,
	MSTP1005,
	MSTP726, MSTP724, MSTP723,
	MSTP_NR
};

static struct clk mstp_clks[MSTP_NR] = {
	[MSTP219] = SH_CLK_MSTP32(&hp_clk, SMSTPCR2, 19, 0),
	[MSTP218] = SH_CLK_MSTP32(&hp_clk, SMSTPCR2, 18, 0),
	[MSTP314] = SH_CLK_MSTP32(&div4_clks[DIV4_SD0], SMSTPCR3, 14, 0),
	[MSTP312] = SH_CLK_MSTP32(&div6_clks[DIV6_SD1], SMSTPCR3, 12, 0),
	[MSTP311] = SH_CLK_MSTP32(&div6_clks[DIV6_SD2], SMSTPCR3, 11, 0),
	[MSTP315] = SH_CLK_MSTP32(&div6_clks[DIV6_MMC], SMSTPCR3, 15, 0),
	[MSTP502] = SH_CLK_MSTP32(&hp_clk, SMSTPCR5, 2, 0),
	[MSTP501] = SH_CLK_MSTP32(&hp_clk, SMSTPCR5, 1, 0),
	[MSTP721] = SH_CLK_MSTP32(&p_clk, SMSTPCR7, 21, 0), /* SCIF0 */
	[MSTP720] = SH_CLK_MSTP32(&p_clk, SMSTPCR7, 20, 0), /* SCIF1 */
	[MSTP719] = SH_CLK_MSTP32(&p_clk, SMSTPCR7, 19, 0), /* SCIF2 */
	[MSTP718] = SH_CLK_MSTP32(&p_clk, SMSTPCR7, 18, 0), /* SCIF3 */
	[MSTP715] = SH_CLK_MSTP32(&p_clk, SMSTPCR7, 15, 0), /* SCIF4 */
	[MSTP714] = SH_CLK_MSTP32(&p_clk, SMSTPCR7, 14, 0), /* SCIF5 */
	[MSTP112] = SH_CLK_MSTP32(&zg_clk, SMSTPCR1, 12, 0),
	[MSTP815] = SH_CLK_MSTP32(&sata0_clk, SMSTPCR8, 15, 0),
	[MSTP814] = SH_CLK_MSTP32(&sata1_clk, SMSTPCR8, 14, 0),
	[MSTP811] = SH_CLK_MSTP32(&zg_clk, SMSTPCR8, 11, 0),
	[MSTP810] = SH_CLK_MSTP32(&zg_clk, SMSTPCR8, 10, 0),
	[MSTP809] = SH_CLK_MSTP32(&zg_clk, SMSTPCR8,  9, 0),
	[MSTP216] = SH_CLK_MSTP32(&mp_clk, SMSTPCR2, 16, 0), /* SCIFB2 */
	[MSTP207] = SH_CLK_MSTP32(&mp_clk, SMSTPCR2, 7, 0), /* SCIFB1 */
	[MSTP206] = SH_CLK_MSTP32(&mp_clk, SMSTPCR2, 6, 0), /* SCIFB0 */
	[MSTP204] = SH_CLK_MSTP32(&mp_clk, SMSTPCR2, 4, 0), /* SCIFA0 */
	[MSTP203] = SH_CLK_MSTP32(&mp_clk, SMSTPCR2, 3, 0), /* SCIFA1 */
	[MSTP202] = SH_CLK_MSTP32(&mp_clk, SMSTPCR2, 2, 0), /* SCIFA2 */
	[MSTP1105] = SH_CLK_MSTP32(&mp_clk, SMSTPCR11, 5, 0), /* SCIFA3 */
	[MSTP1106] = SH_CLK_MSTP32(&mp_clk, SMSTPCR11, 6, 0), /* SCIFA4 */
	[MSTP1107] = SH_CLK_MSTP32(&mp_clk, SMSTPCR11, 7, 0), /* SCIFA5 */
	[MSTP704] = SH_CLK_MSTP32(&mp_clk, SMSTPCR7, 4, 0), /* HSUSB */
	[MSTP703] = SH_CLK_MSTP32(&mp_clk, SMSTPCR7, 3, 0), /* EHCI */
	[MSTP328] = SH_CLK_MSTP32(&mp_clk, SMSTPCR3, 28, 0), /* SSUSB */
	[MSTP917] = SH_CLK_MSTP32(&qspi_clk, SMSTPCR9, 17, 0), /* QSPI */
	[MSTP000] = SH_CLK_MSTP32(&mp_clk, SMSTPCR0, 0, 0), /* MSIOF0 */
	[MSTP208] = SH_CLK_MSTP32(&mp_clk, SMSTPCR2, 8, 0), /* MSIOF1 */
	[MSTP205] = SH_CLK_MSTP32(&mp_clk, SMSTPCR2, 5, 0), /* MSIOF2 */
	[MSTP931] = SH_CLK_MSTP32(&hp_clk, SMSTPCR9, 31, 0),
	[MSTP930] = SH_CLK_MSTP32(&hp_clk, SMSTPCR9, 30, 0),
	[MSTP929] = SH_CLK_MSTP32(&hp_clk, SMSTPCR9, 29, 0),
	[MSTP928] = SH_CLK_MSTP32(&hp_clk, SMSTPCR9, 28, 0),
	[MSTP927] = SH_CLK_MSTP32(&hp_clk, SMSTPCR9, 27, 0),
	[MSTP925] = SH_CLK_MSTP32(&hp_clk, SMSTPCR9, 25, 0),
	[MSTP922] = SH_CLK_MSTP32(&hp_clk, SMSTPCR9, 22, 0),
	[MSTP1031] = SH_CLK_MSTP32(&hp_clk, SMSTPCR10, 31, 0),
	[MSTP1030] = SH_CLK_MSTP32(&hp_clk, SMSTPCR10, 30, 0),
	[MSTP1019] = SH_CLK_MSTP32(&hp_clk, SMSTPCR10, 19, 0),
	[MSTP1018] = SH_CLK_MSTP32(&hp_clk, SMSTPCR10, 18, 0),
	[MSTP1017] = SH_CLK_MSTP32(&hp_clk, SMSTPCR10, 17, 0),
	[MSTP1015] = SH_CLK_MSTP32(&hp_clk, SMSTPCR10, 15, 0),
	[MSTP1014] = SH_CLK_MSTP32(&hp_clk, SMSTPCR10, 14, 0),
	[MSTP1005] = SH_CLK_MSTP32(&hp_clk, SMSTPCR10, 05, 0),
	[MSTP726] = SH_CLK_MSTP32(&zx_clk, SMSTPCR7, 26, 0), /* LVDS0 */
	[MSTP724] = SH_CLK_MSTP32(&zx_clk, SMSTPCR7, 24, 0), /* DU0 */
	[MSTP723] = SH_CLK_MSTP32(&zx_clk, SMSTPCR7, 23, 0), /* DU1 */
};

static struct clk_lookup lookups[] = {

	/* main clocks */
	CLKDEV_CON_ID("extal",		&extal_clk),
	CLKDEV_CON_ID("extal_div2",	&extal_div2_clk),
	CLKDEV_CON_ID("main",		&main_clk),
	CLKDEV_CON_ID("pll1",		&pll1_clk),
	CLKDEV_CON_ID("pll1_div2",	&pll1_div2_clk),
	CLKDEV_CON_ID("pll3",		&pll3_clk),
	CLKDEV_CON_ID("qspi",           &qspi_clk),
	CLKDEV_CON_ID("hp",		&hp_clk),
	CLKDEV_CON_ID("p",		&p_clk),
	CLKDEV_CON_ID("mp",		&mp_clk),
	CLKDEV_CON_ID("cp",		&cp_clk),
	CLKDEV_CON_ID("peripheral_clk", &hp_clk),
	CLKDEV_CON_ID("zx",		&zx_clk),

	/* DIV6 */
	CLKDEV_CON_ID("mmc.0", &div6_clks[DIV6_MMC]),
	CLKDEV_CON_ID("sdhi1", &div6_clks[DIV6_SD1]),
	CLKDEV_CON_ID("sdhi2", &div6_clks[DIV6_SD2]),

	/* MSTP */
	CLKDEV_DEV_ID("pvrsrvkm", &mstp_clks[MSTP112]),
	CLKDEV_CON_ID("sysdmac_lo", &mstp_clks[MSTP219]),
	CLKDEV_CON_ID("sysdmac_up", &mstp_clks[MSTP218]),
	CLKDEV_CON_ID("audmac_lo", &mstp_clks[MSTP502]),
	CLKDEV_CON_ID("audmac_up", &mstp_clks[MSTP501]),
	CLKDEV_DEV_ID("sh-sci.0", &mstp_clks[MSTP204]), /* SCIFA0 */
	CLKDEV_DEV_ID("sh-sci.1", &mstp_clks[MSTP203]), /* SCIFA1 */
	CLKDEV_DEV_ID("sh-sci.2", &mstp_clks[MSTP206]), /* SCIFB0 */
	CLKDEV_DEV_ID("sh-sci.3", &mstp_clks[MSTP207]), /* SCIFB1 */
	CLKDEV_DEV_ID("sh-sci.4", &mstp_clks[MSTP216]), /* SCIFB2 */
	CLKDEV_DEV_ID("sh-sci.5", &mstp_clks[MSTP202]), /* SCIFA2 */
	CLKDEV_DEV_ID("sh-sci.6", &mstp_clks[MSTP721]), /* SCIF0 */
	CLKDEV_DEV_ID("sh-sci.7", &mstp_clks[MSTP720]), /* SCIF1 */
	CLKDEV_DEV_ID("sh-sci.10", &mstp_clks[MSTP719]), /* SCIF2 */
	CLKDEV_DEV_ID("sh-sci.11", &mstp_clks[MSTP718]), /* SCIF3 */
	CLKDEV_DEV_ID("sh-sci.12", &mstp_clks[MSTP715]), /* SCIF4 */
	CLKDEV_DEV_ID("sh-sci.13", &mstp_clks[MSTP714]), /* SCIF5 */
	CLKDEV_DEV_ID("sh-sci.14", &mstp_clks[MSTP1105]), /* SCIFA3 */
	CLKDEV_DEV_ID("sh-sci.15", &mstp_clks[MSTP1106]), /* SCIFA4 */
	CLKDEV_DEV_ID("sh-sci.16", &mstp_clks[MSTP1107]), /* SCIFA5 */
	CLKDEV_CON_ID("hs_usb", &mstp_clks[MSTP704]), /* HSUSB */
	CLKDEV_CON_ID("usb_fck", &mstp_clks[MSTP703]), /* ECHI */
	CLKDEV_CON_ID("ss_usb", &mstp_clks[MSTP328]), /* SSUSB */
	CLKDEV_DEV_ID("qspi.0", &mstp_clks[MSTP917]),
	CLKDEV_DEV_ID("spi_sh_msiof.1", &mstp_clks[MSTP000]),
	CLKDEV_DEV_ID("spi_sh_msiof.2", &mstp_clks[MSTP208]),
	CLKDEV_DEV_ID("spi_sh_msiof.3", &mstp_clks[MSTP205]),
	CLKDEV_DEV_ID("i2c-rcar.0", &mstp_clks[MSTP931]),
	CLKDEV_DEV_ID("i2c-rcar.1", &mstp_clks[MSTP930]),
	CLKDEV_DEV_ID("i2c-rcar.2", &mstp_clks[MSTP929]),
	CLKDEV_DEV_ID("i2c-rcar.3", &mstp_clks[MSTP928]),
	CLKDEV_DEV_ID("i2c-rcar.4", &mstp_clks[MSTP927]),
	CLKDEV_DEV_ID("i2c-rcar.5", &mstp_clks[MSTP925]),
	CLKDEV_DEV_ID("sh_mobile_sdhi.0", &mstp_clks[MSTP314]),
	CLKDEV_DEV_ID("sh_mobile_sdhi.1", &mstp_clks[MSTP312]),
	CLKDEV_DEV_ID("sh_mobile_sdhi.2", &mstp_clks[MSTP311]),
	CLKDEV_DEV_ID("ee200000.mmcif", &mstp_clks[MSTP315]),
	CLKDEV_DEV_ID("sh_mmcif.0", &mstp_clks[MSTP315]),
	CLKDEV_DEV_ID("sata_rcar.0", &mstp_clks[MSTP815]),
	CLKDEV_DEV_ID("sata_rcar.1", &mstp_clks[MSTP814]),
	CLKDEV_DEV_ID("vin.0", &mstp_clks[MSTP811]),
	CLKDEV_DEV_ID("vin.1", &mstp_clks[MSTP810]),
	CLKDEV_DEV_ID("vin.2", &mstp_clks[MSTP809]),
	CLKDEV_CON_ID("adg", &mstp_clks[MSTP922]),
	CLKDEV_CON_ID("src0", &mstp_clks[MSTP1031]),
	CLKDEV_CON_ID("src1", &mstp_clks[MSTP1030]),
	CLKDEV_CON_ID("dvc0", &mstp_clks[MSTP1019]),
	CLKDEV_CON_ID("dvc1", &mstp_clks[MSTP1018]),
	CLKDEV_CON_ID("scu", &mstp_clks[MSTP1017]),
	CLKDEV_CON_ID("ssi0", &mstp_clks[MSTP1015]),
	CLKDEV_CON_ID("ssi1", &mstp_clks[MSTP1014]),
	CLKDEV_CON_ID("ssi", &mstp_clks[MSTP1005]),
	CLKDEV_ICK_ID("lvds.0", "rcar-du-r8a7791", &mstp_clks[MSTP726]),
	CLKDEV_ICK_ID("du.0", "rcar-du-r8a7791", &mstp_clks[MSTP724]),
	CLKDEV_ICK_ID("du.1", "rcar-du-r8a7791", &mstp_clks[MSTP723]),
};

#define R8A7791_CLOCK_ROOT(e, m, p0, p1, p30, p31)		\
	extal_clk.rate	= e * 1000 * 1000;			\
	main_clk.parent	= m;					\
	SH_CLK_SET_RATIO(&pll1_clk_ratio, p1 / 2, 1);		\
	if (mode & MD(19))					\
		SH_CLK_SET_RATIO(&pll3_clk_ratio, p31, 1);	\
	else							\
		SH_CLK_SET_RATIO(&pll3_clk_ratio, p30, 1)


static void __init r8a7791_sdhi_clock_init(void)
{
	int ret = 0;
	struct clk *sdhi_clk;

	/* set SDHI1 clock to 97.5 MHz */
	sdhi_clk = clk_get(NULL, "sdhi1");
	if (IS_ERR(sdhi_clk)) {
		pr_err("Cannot get sdhi1 clock\n");
		goto sdhi1_out;
	}
	ret = clk_set_rate(sdhi_clk, 97500000);
	if (ret < 0)
		pr_err("Cannot set sdhi1 clock rate :%d\n", ret);

	clk_put(sdhi_clk);
sdhi1_out:

	/* set SDHI2 clock to 97.5 MHz */
	sdhi_clk = clk_get(NULL, "sdhi2");
	if (IS_ERR(sdhi_clk)) {
		pr_err("Cannot get sdhi2 clock\n");
		goto sdhi2_out;
	}
	ret = clk_set_rate(sdhi_clk, 97500000);
	if (ret < 0)
		pr_err("Cannot set sdhi2 clock rate :%d\n", ret);

	clk_put(sdhi_clk);
sdhi2_out:

	return;
}

static int __init r8a7791_mmc_clock_init(void)
{
	int ret = 0;
	struct clk *mmc_clk;

	/* set MMC0 clock to 97.5 MHz */
	mmc_clk = clk_get(NULL, "mmc.0");
	if (IS_ERR(mmc_clk)) {
		pr_err("Cannot get mmc.0 clock\n");
		goto mmc0_out;
	}
	ret = clk_set_rate(mmc_clk, 97500000);
	if (ret < 0)
		pr_err("Cannot set mmc.0 clock rate :%d\n", ret);

	clk_put(mmc_clk);
mmc0_out:

	return;
}

void __init r8a7791_clock_init(void)
{
	void __iomem *modemr = ioremap_nocache(MODEMR, PAGE_SIZE);
	u32 mode;
	int k, ret = 0;

	BUG_ON(!modemr);
	mode = ioread32(modemr);
	iounmap(modemr);

	switch (mode & (MD(14) | MD(13))) {
	case 0:
		R8A7791_CLOCK_ROOT(15, &extal_clk, 172, 208, 106, 88);
		break;
	case MD(13):
		R8A7791_CLOCK_ROOT(20, &extal_clk, 130, 156, 80, 66);
		break;
	case MD(14):
		R8A7791_CLOCK_ROOT(26, &extal_div2_clk, 200, 240, 122, 102);
		break;
	case MD(13) | MD(14):
		R8A7791_CLOCK_ROOT(30, &extal_div2_clk, 172, 208, 106, 88);
		break;
	}

	if ((mode & (MD(3) | MD(2) | MD(1))) == MD(2))
		SH_CLK_SET_RATIO(&qspi_clk_ratio, 1, 16);
	else
		SH_CLK_SET_RATIO(&qspi_clk_ratio, 1, 20);

	for (k = 0; !ret && (k < ARRAY_SIZE(main_clks)); k++)
		ret = clk_register(main_clks[k]);

	if (!ret)
		ret = sh_clk_div4_register(div4_clks, DIV4_NR, &div4_table);

	if (!ret)
		ret = sh_clk_div6_register(div6_clks, DIV6_NR);

	if (!ret)
		ret = sh_clk_mstp_register(mstp_clks, MSTP_NR);

	clkdev_add_table(lookups, ARRAY_SIZE(lookups));

	if (!ret)
		shmobile_clk_init();
	else
		goto epanic;

	r8a7791_sdhi_clock_init();
	r8a7791_mmc_clock_init();

	return;

epanic:
	panic("failed to setup r8a7791 clocks\n");
}
