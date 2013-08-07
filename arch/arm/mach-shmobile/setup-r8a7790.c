/*
 * r8a7790 processor support
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
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/serial_sci.h>
#include <linux/sh_eth.h>
#include <linux/i2c/i2c-rcar.h>
#include <linux/sh_dma-desc.h>
#include <linux/dma-mapping.h>
#include <linux/sh_audma-pp.h>
#include <linux/platform_data/gpio-rcar.h>
#include <linux/platform_data/rcar-du.h>
#include <linux/platform_data/irq-renesas-irqc.h>
#include <linux/platform_data/vsp1.h>
#include <linux/serial_sci.h>
#include <linux/sh_timer.h>
#include <linux/platform_data/rcar-du.h>
#include <linux/clk.h>
#include <linux/usb/ehci_pdriver.h>
#include <linux/usb/ohci_pdriver.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/spi/spi.h>
#include <linux/dma-mapping.h>
#include <linux/spi/sh_msiof.h>
#include <media/vin.h>
#include <mach/common.h>
#include <mach/irqs.h>
#include <mach/r8a7790.h>
#include <mach/dma-register.h>
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
       HSCIF0, HSCIF1, SCIF2 };

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
	SCIF_DATA(SCIF2, 0xe6e56000, gic_spi(164)), /* SCIF2 */
};

static inline void r8a7790_register_scif(int idx)
{
	platform_device_register_data(&platform_bus, "sh-sci", idx, &scif[idx],
				      sizeof(struct plat_sci_port));
}

static struct resource eth_resources[] = {
	{
		.start  = 0xee700200,
		.end    = 0xee7003fc,
		.flags  = IORESOURCE_MEM,
	}, {
		.start  = gic_spi(162),		/* IRQ0 */
		.flags  = IORESOURCE_IRQ,
	},
};

static struct sh_eth_plat_data eth_platform_data = {
	.phy = 0x1,
	.edmac_endian = EDMAC_LITTLE_ENDIAN,
	.register_type = SH_ETH_REG_FAST_SH4,
	.phy_interface = PHY_INTERFACE_MODE_RMII,
	.ether_link_active_low = 1,
};

static struct platform_device eth_device = {
	.name = "sh-eth",
	.id	= 0,
	.dev = {
		.platform_data = &eth_platform_data,
	},
	.num_resources = ARRAY_SIZE(eth_resources),
	.resource = eth_resources,
};

static struct resource powervr_resources[] = {
	{
		.start  = 0xfd000000,
		.end    = 0xfd00ffff,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = gic_spi(119),
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device powervr_device = {
	.name           = "pvrsrvkm",
	.id             = -1,
	.resource       = powervr_resources,
	.num_resources  = ARRAY_SIZE(powervr_resources),
};

static u64 usb_dmamask = ~(u32)0;

struct usb_ehci_pdata ehci_pdata = {
	.caps_offset	= 0,
	.has_tt		= 0,
};

struct usb_ohci_pdata ohci_pdata = {
};

static struct resource ehci0_resources[] = {
	[0] = {
		.start	= 0xee080000 + 0x1000,
		.end	= 0xee080000 + 0x1000 + 0x0fff - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(108),
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ehci0_device = {
	.name	= "ehci-platform",
	.id	= 0,
	.dev	= {
		.platform_data		= &ehci_pdata,
		.dma_mask		= &usb_dmamask,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(ehci0_resources),
	.resource	= ehci0_resources,
};

static struct resource ohci0_resources[] = {
	[0] = {
		.start	= 0xee080000,
		.end	= 0xee080000 + 0x0fff - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(108),
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ohci0_device = {
	.name	= "ohci-platform",
	.id	= 0,
	.dev	= {
		.platform_data		= &ohci_pdata,
		.dma_mask		= &usb_dmamask,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(ohci0_resources),
	.resource	= ohci0_resources,
};

static struct resource ehci1_resources[] = {
	[0] = {
		.start	= 0xee0a0000 + 0x1000,
		.end	= 0xee0a0000 + 0x1000 + 0x0fff - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(112),
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ehci1_device = {
	.name	= "ehci-platform",
	.id	= 1,
	.dev	= {
		.platform_data		= &ehci_pdata,
		.dma_mask		= &usb_dmamask,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(ehci1_resources),
	.resource	= ehci1_resources,
};

static struct resource ohci1_resources[] = {
	[0] = {
		.start	= 0xee0a0000,
		.end	= 0xee0a0000 + 0x0fff - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(112),
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ohci1_device = {
	.name	= "ohci-platform",
	.id	= 1,
	.dev	= {
		.platform_data		= &ohci_pdata,
		.dma_mask		= &usb_dmamask,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(ohci1_resources),
	.resource	= ohci1_resources,
};

static struct resource ehci2_resources[] = {
	[0] = {
		.start	= 0xee0c0000 + 0x1000,
		.end	= 0xee0c0000 + 0x1000 + 0x0fff - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(113),
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ehci2_device = {
	.name	= "ehci-platform",
	.id	= 2,
	.dev	= {
		.platform_data		= &ehci_pdata,
		.dma_mask		= &usb_dmamask,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(ehci2_resources),
	.resource	= ehci2_resources,
};

static struct resource ohci2_resources[] = {
	[0] = {
		.start	= 0xee0c0000,
		.end	= 0xee0c0000 + 0x0fff - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(113),
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ohci2_device = {
	.name	= "ohci-platform",
	.id	= 2,
	.dev	= {
		.platform_data		= &ohci_pdata,
		.dma_mask		= &usb_dmamask,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(ohci2_resources),
	.resource	= ohci2_resources,
};

static struct resource xhci0_resources[] = {
	[0] = {
		.start	= SHUSBH_XHCI_BASE,
		.end	= SHUSBH_XHCI_BASE + SHUSBH_XHCI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(101),
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device xhci0_device = {
	.name	= "xhci-hcd",
	.id	= 0,
	.dev	= {
		.dma_mask		= &usb_dmamask,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(xhci0_resources),
	.resource	= xhci0_resources,
};

static void __init usbh_internal_pci_bridge_init(int ch)
{
	u32 data;
	void __iomem *ahbpci_base;
	void __iomem *pci_conf_ahbpci_bas;

	ahbpci_base =
		ioremap_nocache((AHBPCI_BASE + (ch * 0x20000)), 0x400);
	if (!ahbpci_base)
		return;

	pci_conf_ahbpci_bas =
		ioremap_nocache((PCI_CONF_AHBPCI_BAS + (ch * 0x20000)),
								0x100);
	if (!pci_conf_ahbpci_bas)
		goto err_iounmap_ahbpci;

	/* Clock & Reset & Direct Power Down */
	data = ioread32(ahbpci_base + USBCTR);
	data &= ~(DIRPD);
	iowrite32(data, (ahbpci_base + USBCTR));

	data &= ~(PLL_RST | PCICLK_MASK | USBH_RST);
	iowrite32(data | PCI_AHB_WIN1_SIZE_1G, (ahbpci_base + USBCTR));

	data = ioread32((ahbpci_base + AHB_BUS_CTR));
	if (data == AHB_BUS_CTR_SET)
		goto err_iounmap_pci_conf;

	/****** AHB-PCI Bridge Communication Registers ******/
	/* AHB_BUS_CTR */
	iowrite32(AHB_BUS_CTR_SET, (ahbpci_base + AHB_BUS_CTR));

	/* PCIAHB_WIN1_CTR */
	iowrite32((0x40000000 | PREFETCH),
			(ahbpci_base + PCIAHB_WIN1_CTR));

	/* AHBPCI_WIN2_CTR */
	iowrite32((SHUSBH_OHCI_BASE | PCIWIN2_PCICMD),
			(ahbpci_base + AHBPCI_WIN2_CTR));

	/* PCI_ARBITER_CTR */
	data = ioread32((ahbpci_base + PCI_ARBITER_CTR));
	data |= (PCIBP_MODE | PCIREQ1 | PCIREQ0);
	iowrite32(data, (ahbpci_base + PCI_ARBITER_CTR));

	/* AHBPCI_WIN1_CTR : set PCI Configuratin Register for AHBPCI */
	iowrite32(PCIWIN1_PCICMD | AHB_CFG_AHBPCI,
			(ahbpci_base + AHBPCI_WIN1_CTR));

	/****** PCI Configuration Registers for AHBPCI ******/
	/* BASEAD */
	iowrite32(AHBPCI_BASE, (pci_conf_ahbpci_bas + BASEAD));

	/* WIN1_BASEAD */
	iowrite32(0x40000000, (pci_conf_ahbpci_bas + WIN1_BASEAD));

	/* System error enable, Parity error enable, PCI Master enable, */
	/* Memory cycle enable */
	iowrite32(((ioread32(pci_conf_ahbpci_bas + CMND_STS) & ~0x00100000)
			| (SERREN | PERREN | MASTEREN | MEMEN)),
			(pci_conf_ahbpci_bas + CMND_STS));

	/****** PCI Configuration Registers for OHCI/EHCI ******/
	iowrite32(PCIWIN1_PCICMD | AHB_CFG_HOST,
			(ahbpci_base + AHBPCI_WIN1_CTR));

err_iounmap_pci_conf:
	iounmap(pci_conf_ahbpci_bas);
err_iounmap_ahbpci:
	iounmap(ahbpci_base);

}

static int __init usbh_ohci_init(int ch)
{
	u32 val;
	int retval;

	void __iomem *pci_conf_ohci_base
		= ioremap_nocache((PCI_CONF_OHCI_BASE + (ch * 0x20000)),
								0x100);

	if (!pci_conf_ohci_base)
		return -ENOMEM;

	val = ioread32((pci_conf_ohci_base + OHCI_VID_DID));

	if (val == OHCI_ID) {
		/* OHCI_BASEAD */
		iowrite32(SHUSBH_OHCI_BASE,
				(pci_conf_ohci_base + OHCI_BASEAD));
		retval = 0;

		/* System error enable, Parity error enable, */
		/* PCI Master enable, Memory cycle enable */
		iowrite32(ioread32(pci_conf_ohci_base + OHCI_CMND_STS)
				| (SERREN | PERREN | MASTEREN | MEMEN),
				(pci_conf_ohci_base + OHCI_CMND_STS));
	} else {
		printk(KERN_ERR "Don't found OHCI controller. %x\n", val);
		retval = -1;
	}
	iounmap(pci_conf_ohci_base);

	return retval;
}

static int __init usbh_ehci_init(int ch)
{
	u32 val;
	int retval;

	void __iomem *pci_conf_ehci_base
		= ioremap_nocache((PCI_CONF_EHCI_BASE + (ch * 0x20000)),
								 0x100);

	if (!pci_conf_ehci_base)
		return -ENOMEM;

	val = ioread32((pci_conf_ehci_base + EHCI_VID_DID));
	if (val == EHCI_ID) {
		/* EHCI_BASEAD */
		iowrite32(SHUSBH_EHCI_BASE,
				(pci_conf_ehci_base + EHCI_BASEAD));

		/* System error enable, Parity error enable, */
		/* PCI Master enable, Memory cycle enable */
		iowrite32(ioread32(pci_conf_ehci_base + EHCI_CMND_STS) |
				(SERREN | PERREN | MASTEREN | MEMEN),
				(pci_conf_ehci_base + EHCI_CMND_STS));
		retval = 0;
	} else {
		printk(KERN_ERR "Don't found EHCI controller. %x\n", val);
		retval = -1;
	}
	iounmap(pci_conf_ehci_base);

	return retval;
}

static void __init usbh_pci_int_enable(int ch)
{
	void __iomem *ahbpci_base =
		ioremap_nocache((AHBPCI_BASE + (ch * 0x20000)), 0x400);
	u32 data;

	if (!ahbpci_base)
		return;

	/* PCI_INT_ENABLE */
	data = ioread32((ahbpci_base + PCI_INT_ENABLE));
	data |= USBH_PMEEN | USBH_INTBEN | USBH_INTAEN;
	iowrite32(data, (ahbpci_base + PCI_INT_ENABLE));

	iounmap(ahbpci_base);
}

static int __init usbh_init(void)
{
	struct clk *clk_hs, *clk_ehci;
#if defined(CONFIG_USB_XHCI_HCD)
	struct clk *clk_xhci;
#endif /* CONFIG_USB_XHCI_HCD */
	void __iomem *hs_usb = ioremap_nocache(0xE6590000, 0x1ff);
	unsigned int ch;
	int ret = 0;

	if (!hs_usb)
		return -ENOMEM;

	clk_hs = clk_get(NULL, "hs_usb");
	if (IS_ERR(clk_hs)) {
		ret = PTR_ERR(clk_hs);
		goto err_iounmap;
	}

	clk_ehci = clk_get(NULL, "usb_fck");
	if (IS_ERR(clk_ehci)) {
		ret = PTR_ERR(clk_ehci);
		goto err_iounmap;
	}

	clk_enable(clk_hs);
	clk_enable(clk_ehci);

#if defined(CONFIG_USB_XHCI_HCD)
	clk_xhci = clk_get(NULL, "ss_usb");
	if (IS_ERR(clk_xhci)) {
		ret = PTR_ERR(clk_xhci);
		goto err_iounmap;
	}

	clk_enable(clk_xhci);

	/* Select XHCI for ch2 and EHCI for ch0 */
	iowrite32(0x80000011, (hs_usb + 0x184));
#else
	/* Set EHCI for UGCTRL2 */
	iowrite32(0x00000011, (hs_usb + 0x184));
#endif /* CONFIG_USB_XHCI_HCD */

	for (ch = 0; ch < SHUSBH_MAX_CH; ch++) {
		/* internal pci-bus bridge initialize */
		usbh_internal_pci_bridge_init(ch);

		/* ohci initialize */
		usbh_ohci_init(ch);

		/* ehci initialize */
		usbh_ehci_init(ch);

		/* pci int enable */
		usbh_pci_int_enable(ch);
	}
err_iounmap:
	iounmap(hs_usb);

	return ret;
}

/* Fixed 3.3V regulator to be used by SDHI0/1/2/3 */
static struct regulator_consumer_supply fixed3v3_power_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sh_mobile_sdhi.0"),
	REGULATOR_SUPPLY("vqmmc", "sh_mobile_sdhi.0"),
	REGULATOR_SUPPLY("vmmc", "sh_mobile_sdhi.1"),
	REGULATOR_SUPPLY("vqmmc", "sh_mobile_sdhi.1"),
	REGULATOR_SUPPLY("vmmc", "sh_mobile_sdhi.2"),
	REGULATOR_SUPPLY("vqmmc", "sh_mobile_sdhi.2"),
	REGULATOR_SUPPLY("vmmc", "sh_mobile_sdhi.3"),
	REGULATOR_SUPPLY("vqmmc", "sh_mobile_sdhi.3"),
};

/* QSPI resource */
static struct resource qspi_resources[] = {
	[0] = {
		.name = "QSPI",
		.start = 0xe6b10000,
		.end = 0xe6b10fff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = gic_spi(184),
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device qspi_device = {
	.name = "qspi",
	.id = 0,
	.num_resources = ARRAY_SIZE(qspi_resources),
	.resource	= qspi_resources,
};

/* MSIOF */
static struct sh_msiof_spi_info sh_msiof_info = {
	.rx_fifo_override	= 256,
	.num_chipselect		= 1,
};

static struct resource sh_msiof0_resources[] = {
	[0] = {
		.start	= 0xe6e20000,
		.end	= 0xe6e20064,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(156),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource sh_msiof1_resources[] = {
	[0] = {
		.start	= 0xe6e10000,
		.end	= 0xe6e10064,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(157),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource sh_msiof2_resources[] = {
	[0] = {
		.start	= 0xe6e00000,
		.end	= 0xe6e00064,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(158),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource sh_msiof3_resources[] = {
	[0] = {
		.start	= 0xe6c90000,
		.end	= 0xe6c90064,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(159),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device sh_msiof0_device = {
	.name		= "spi_sh_msiof",
	.id		= 1,
	.dev		= {
		.platform_data	= &sh_msiof_info,
	},
	.num_resources	= ARRAY_SIZE(sh_msiof0_resources),
	.resource	= sh_msiof0_resources,
};

static struct platform_device sh_msiof1_device = {
	.name		= "spi_sh_msiof",
	.id		= 2,
	.dev		= {
		.platform_data	= &sh_msiof_info,
	},
	.num_resources	= ARRAY_SIZE(sh_msiof1_resources),
	.resource	= sh_msiof1_resources,
};

static struct platform_device sh_msiof2_device = {
	.name		= "spi_sh_msiof",
	.id		= 3,
	.dev		= {
		.platform_data	= &sh_msiof_info,
	},
	.num_resources	= ARRAY_SIZE(sh_msiof2_resources),
	.resource	= sh_msiof2_resources,
};

static struct platform_device sh_msiof3_device = {
	.name		= "spi_sh_msiof",
	.id		= 4,
	.dev		= {
		.platform_data	= &sh_msiof_info,
	},
	.num_resources	= ARRAY_SIZE(sh_msiof3_resources),
	.resource	= sh_msiof3_resources,
};

/* I2C */
static struct i2c_rcar_platform_data i2c_pd[] = {
	{
		.bus_speed	= 400000,
		.icccr_cdf_width = I2C_RCAR_ICCCR_IS_3BIT,
	}, {
		.bus_speed	= 400000,
		.icccr_cdf_width = I2C_RCAR_ICCCR_IS_3BIT,
	}, {
		/* Recommended values of bus speed 100kHz by H2 H/W spec. */
		.icccr	= 6,
		.icccr2	= 7,
		.icmpr	= 10,
		.ichpr	= 632,
		.iclpr	= 640,
	}, {
		.bus_speed	= 400000,
		.icccr_cdf_width = I2C_RCAR_ICCCR_IS_3BIT,
	},
};

static struct resource rcar_i2c0_res[] = {
	{
		.start  = 0xe6508000,
		.end    = (0xe6510000 - 1),
		.flags  = IORESOURCE_MEM,
	}, {
		.start  = gic_spi(287),
		.flags  = IORESOURCE_IRQ,
	},
};

static struct resource rcar_i2c1_res[] = {
	{
		.start  = 0xe6518000,
		.end    = (0xe6520000 - 1),
		.flags  = IORESOURCE_MEM,
	}, {
		.start  = gic_spi(288),
		.flags  = IORESOURCE_IRQ,
	},
};

static struct resource rcar_i2c2_res[] = {
	{
		.start  = 0xe6530000,
		.end    = (0xe6538000 - 1),
		.flags  = IORESOURCE_MEM,
	}, {
		.start  = gic_spi(286),
		.flags  = IORESOURCE_IRQ,
	},
};

static struct resource rcar_i2c3_res[] = {
	{
		.start  = 0xe6540000,
		.end    = (0xe6548000 - 1),
		.flags  = IORESOURCE_MEM,
	}, {
		.start  = gic_spi(290),
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device i2c0_device = {
	.name		= "i2c-rcar",
	.id		= 0,
	.dev = {
		.platform_data = &i2c_pd[0],
	},
	.num_resources	= ARRAY_SIZE(rcar_i2c0_res),
	.resource	= rcar_i2c0_res,
};

static struct platform_device i2c1_device = {
	.name		= "i2c-rcar",
	.id		= 1,
	.dev = {
		.platform_data = &i2c_pd[1],
	},
	.num_resources	= ARRAY_SIZE(rcar_i2c1_res),
	.resource	= rcar_i2c1_res,
};

static struct platform_device i2c2_device = {
	.name		= "i2c-rcar",
	.id		= 2,
	.dev = {
		.platform_data = &i2c_pd[2],
	},
	.num_resources	= ARRAY_SIZE(rcar_i2c2_res),
	.resource	= rcar_i2c2_res,
};

static struct platform_device i2c3_device = {
	.name		= "i2c-rcar",
	.id		= 3,
	.dev = {
		.platform_data = &i2c_pd[3],
	},
	.num_resources	= ARRAY_SIZE(rcar_i2c3_res),
	.resource	= rcar_i2c3_res,
};

/* DMA */
#define DMA_CHANNEL(a, b, c)	\
{				\
	.offset		= a,	\
	.dmars		= b,	\
	.dmars_bit	= 0,	\
	.chclr_offset	= c	\
}

#define AUDMAPP_CHANNEL(a)	\
{				\
	.offset		= a,	\
}

/* Audio-DMA */
/*  audmal  : Audio-DMAC lower (ch0-12)  */
/*  audmau  : Audio-DMAC upper (ch13-25) */
/*  audmapp : Audio-DMAC-pp (ch0-28)     */
static struct clk *audma_clk_get(struct platform_device *pdev)
{
	if (pdev->id == SHDMA_DEVID_AUDIO_LO)
		return clk_get(NULL, "audmac_lo");
	else if (pdev->id == SHDMA_DEVID_AUDIO_UP)
		return clk_get(NULL, "audmac_up");
	else
		return NULL;
}

static const struct sh_dmadesc_slave_config r8a7790_audma_slaves[] = {
	{
		.slave_id	= SHDMA_SLAVE_PCM_MEM_SSI0,
		.addr		= 0xec241008,
		.chcr		= CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid	= 0x01,
		.desc_mode	= 2,
		.desc_offset	= 0x0,
		.desc_stepnum	= 4,
	}, {
		.slave_id	= SHDMA_SLAVE_PCM_MEM_SRC0,
		.addr		= 0xec000000,
		.chcr		= CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid	= 0x85,
		.desc_mode	= 2,
		.desc_offset	= 0x0,
		.desc_stepnum	= 4,
	}, {
		.slave_id	= SHDMA_SLAVE_PCM_SSI1_MEM,
		.addr		= 0xec24104c,
		.chcr		= CHCR_RX(XMIT_SZ_32BIT),
		.mid_rid	= 0x04,
		.desc_mode	= 2,
		.desc_offset	= 0x100,
		.desc_stepnum	= 4,
	}, {
		.slave_id	= SHDMA_SLAVE_PCM_SRC1_MEM,
		.addr		= 0xec004400,
		.chcr		= CHCR_RX(XMIT_SZ_32BIT),
		.mid_rid	= 0x9c,
		.desc_mode	= 2,
		.desc_offset	= 0x100,
		.desc_stepnum	= 4,
	}, {
		.slave_id	= SHDMA_SLAVE_PCM_CMD1_MEM,
		.addr		= 0xec008400,
		.chcr		= CHCR_RX(XMIT_SZ_32BIT),
		.mid_rid	= 0xbe,
		.desc_mode	= 2,
		.desc_offset	= 0x100,
		.desc_stepnum	= 4,
	},
};

static const struct sh_audmapp_slave_config r8a7790_audmapp_slaves[] = {
	{
		.slave_id	= SHDMA_SLAVE_PCM_SRC0_SSI0,
		.sar		= 0xec304000,
		.dar		= 0xec400000,
		.chcr		= 0x2d000000,
	}, {
		.slave_id	= SHDMA_SLAVE_PCM_CMD0_SSI0,
		.sar		= 0xec308000,
		.dar		= 0xec400000,
		.chcr		= 0x37000000,
	}, {
		.slave_id	= SHDMA_SLAVE_PCM_SSI1_SRC1,
		.sar		= 0xec401000,
		.dar		= 0xec300400,
		.chcr		= 0x042e0000,
	},
};

static const struct sh_dmadesc_channel r8a7790_audma_channels[] = {
	DMA_CHANNEL(0x00008000, 0x40, 0),
	DMA_CHANNEL(0x00008080, 0x40, 0),
	DMA_CHANNEL(0x00008100, 0x40, 0),
	DMA_CHANNEL(0x00008180, 0x40, 0),
	DMA_CHANNEL(0x00008200, 0x40, 0),
	DMA_CHANNEL(0x00008280, 0x40, 0),
	DMA_CHANNEL(0x00008300, 0x40, 0),
	DMA_CHANNEL(0x00008380, 0x40, 0),
	DMA_CHANNEL(0x00008400, 0x40, 0),
	DMA_CHANNEL(0x00008480, 0x40, 0),
	DMA_CHANNEL(0x00008500, 0x40, 0),
	DMA_CHANNEL(0x00008580, 0x40, 0),
	DMA_CHANNEL(0x00008600, 0x40, 0),
};

static const struct sh_audmapp_channel r8a7790_audmapp_channels[] = {
	AUDMAPP_CHANNEL(0x0000),
	AUDMAPP_CHANNEL(0x0010),
	AUDMAPP_CHANNEL(0x0020),
	AUDMAPP_CHANNEL(0x0030),
	AUDMAPP_CHANNEL(0x0040),
	AUDMAPP_CHANNEL(0x0050),
	AUDMAPP_CHANNEL(0x0060),
	AUDMAPP_CHANNEL(0x0070),
	AUDMAPP_CHANNEL(0x0080),
	AUDMAPP_CHANNEL(0x0090),
	AUDMAPP_CHANNEL(0x00a0),
	AUDMAPP_CHANNEL(0x00b0),
	AUDMAPP_CHANNEL(0x00c0),
	AUDMAPP_CHANNEL(0x00d0),
	AUDMAPP_CHANNEL(0x00e0),
	AUDMAPP_CHANNEL(0x00f0),
	AUDMAPP_CHANNEL(0x0100),
	AUDMAPP_CHANNEL(0x0110),
	AUDMAPP_CHANNEL(0x0120),
	AUDMAPP_CHANNEL(0x0130),
	AUDMAPP_CHANNEL(0x0140),
	AUDMAPP_CHANNEL(0x0150),
	AUDMAPP_CHANNEL(0x0160),
	AUDMAPP_CHANNEL(0x0170),
	AUDMAPP_CHANNEL(0x0180),
	AUDMAPP_CHANNEL(0x0190),
	AUDMAPP_CHANNEL(0x01a0),
	AUDMAPP_CHANNEL(0x01b0),
	AUDMAPP_CHANNEL(0x01c0),
};

static struct sh_dmadesc_pdata audma_platform_data = {
	.slave		= r8a7790_audma_slaves,
	.slave_num	= ARRAY_SIZE(r8a7790_audma_slaves),
	.channel	= r8a7790_audma_channels,
	.channel_num	= ARRAY_SIZE(r8a7790_audma_channels),
	.ts_low_shift	= TS_LOW_SHIFT,
	.ts_low_mask	= TS_LOW_BIT << TS_LOW_SHIFT,
	.ts_high_shift	= TS_HI_SHIFT,
	.ts_high_mask	= TS_HI_BIT << TS_HI_SHIFT,
	.ts_shift	= dma_ts_shift,
	.ts_shift_num	= ARRAY_SIZE(dma_ts_shift),
	.dmaor_init	= DMAOR_DME,
	.chclr_present	= 1,
	.clk_get	= audma_clk_get,
};

static struct sh_audmapp_pdata audmapp_platform_data = {
	.slave		= r8a7790_audmapp_slaves,
	.slave_num	= ARRAY_SIZE(r8a7790_audmapp_slaves),
	.channel	= r8a7790_audmapp_channels,
	.channel_num	= ARRAY_SIZE(r8a7790_audmapp_channels),
};

static struct resource r8a7790_audmal_resources[] = {
	{
		.start	= 0xec700000,
		.end	= 0xec70a7ff,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "error_irq",
		.start	= gic_spi(346),
		.end	= gic_spi(346),
		.flags	= IORESOURCE_IRQ,
	},
	{
		/* IRQ for channels */
		.start	= gic_spi(320),
		.end	= gic_spi(332),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource r8a7790_audmau_resources[] = {
	{
		.start	= 0xec720000,
		.end	= 0xec72a7ff,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "error_irq",
		.start	= gic_spi(347),
		.end	= gic_spi(347),
		.flags	= IORESOURCE_IRQ,
	},
	{
		/* IRQ for channels */
		.start	= gic_spi(333),
		.end	= gic_spi(345),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource r8a7790_audmapp_resources[] = {
	{
		/* channel registers (0-28) */
		.start	= 0xec740020,
		.end	= 0xec7401ef,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device audmal_device = {
	.name		= "sh-dmadesc-engine",
	.id		= SHDMA_DEVID_AUDIO_LO,
	.resource	= r8a7790_audmal_resources,
	.num_resources	= ARRAY_SIZE(r8a7790_audmal_resources),
	.dev		= {
		.platform_data	= &audma_platform_data,
	},
};

static struct platform_device audmau_device = {
	.name		= "sh-dmadesc-engine",
	.id		= SHDMA_DEVID_AUDIO_UP,
	.resource	= r8a7790_audmau_resources,
	.num_resources	= ARRAY_SIZE(r8a7790_audmau_resources),
	.dev		= {
		.platform_data	= &audma_platform_data,
	},
};

static struct platform_device audmapp_device = {
	.name		= "sh-audmapp-engine",
	.id		= SHDMA_DEVID_AUDIOPP,
	.resource	= r8a7790_audmapp_resources,
	.num_resources	= ARRAY_SIZE(r8a7790_audmapp_resources),
	.dev		= {
		.platform_data	= &audmapp_platform_data,
	},
};

/* SYS-DMA */
static bool sysdma_filter(struct platform_device *pdev)
{
	if ((pdev->id != SHDMA_DEVID_SYS_LO) &&
	    (pdev->id != SHDMA_DEVID_SYS_UP))
		return false;
	return true;
}

static struct clk *sysdma_clk_get(struct platform_device *pdev)
{
	if (pdev->id == SHDMA_DEVID_SYS_LO)
		return clk_get(NULL, "sysdmac_lo");
	else if (pdev->id == SHDMA_DEVID_SYS_UP)
		return clk_get(NULL, "sysdmac_up");
	else
		return NULL;
}

static const struct sh_dmadesc_slave_config r8a7790_sysdma_slaves[] = {
	{
		.slave_id	= SHDMA_SLAVE_SDHI0_TX,
		.addr		= 0xee100060,
		.chcr		= CHCR_TX(XMIT_SZ_16BIT),
		.mid_rid	= 0xcd,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI0_RX,
		.addr		= 0xee100060 + 0x2000,
		.chcr		= CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid	= 0xce,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI1_TX,
		.addr		= 0xee120030,
		.chcr		= CHCR_TX(XMIT_SZ_16BIT),
		.mid_rid	= 0xc9,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI1_RX,
		.addr		= 0xee120030 + 0x2000,
		.chcr		= CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid	= 0xca,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI2_TX,
		.addr		= 0xee140030,
		.chcr		= CHCR_TX(XMIT_SZ_16BIT),
		.mid_rid	= 0xc1,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI2_RX,
		.addr		= 0xee140030 + 0x2000,
		.chcr		= CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid	= 0xc2,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI3_TX,
		.addr		= 0xee160030,
		.chcr		= CHCR_TX(XMIT_SZ_16BIT),
		.mid_rid	= 0xd3,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI3_RX,
		.addr		= 0xee160030 + 0x2000,
		.chcr		= CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid	= 0xd4,
	}, {
		.slave_id	= SHDMA_SLAVE_MMC0_TX,
		.addr		= 0xee200034,
		.chcr		= CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid	= 0xd1,
	}, {
		.slave_id	= SHDMA_SLAVE_MMC0_RX,
		.addr		= 0xee200034,
		.chcr		= CHCR_RX(XMIT_SZ_32BIT),
		.mid_rid	= 0xd2,
	}, {
		.slave_id	= SHDMA_SLAVE_MMC1_TX,
		.addr		= 0xee220034,
		.chcr		= CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid	= 0xe1,
	}, {
		.slave_id	= SHDMA_SLAVE_MMC1_RX,
		.addr		= 0xee220034,
		.chcr		= CHCR_RX(XMIT_SZ_32BIT),
		.mid_rid	= 0xe2,
	},
};

static const struct sh_dmadesc_channel r8a7790_sysdma_channels[] = {
	DMA_CHANNEL(0x00008000, 0x40, 0),
	DMA_CHANNEL(0x00008080, 0x40, 0),
	DMA_CHANNEL(0x00008100, 0x40, 0),
	DMA_CHANNEL(0x00008180, 0x40, 0),
	DMA_CHANNEL(0x00008200, 0x40, 0),
	DMA_CHANNEL(0x00008280, 0x40, 0),
	DMA_CHANNEL(0x00008300, 0x40, 0),
	DMA_CHANNEL(0x00008380, 0x40, 0),
	DMA_CHANNEL(0x00008400, 0x40, 0),
	DMA_CHANNEL(0x00008480, 0x40, 0),
	DMA_CHANNEL(0x00008500, 0x40, 0),
	DMA_CHANNEL(0x00008580, 0x40, 0),
	DMA_CHANNEL(0x00008600, 0x40, 0),
	DMA_CHANNEL(0x00008680, 0x40, 0),
	DMA_CHANNEL(0x00008700, 0x40, 0),
};

static struct sh_dmadesc_pdata sysdma_platform_data = {
	.slave		= r8a7790_sysdma_slaves,
	.slave_num	= ARRAY_SIZE(r8a7790_sysdma_slaves),
	.channel	= r8a7790_sysdma_channels,
	.channel_num	= ARRAY_SIZE(r8a7790_sysdma_channels),
	.ts_low_shift	= TS_LOW_SHIFT,
	.ts_low_mask	= TS_LOW_BIT << TS_LOW_SHIFT,
	.ts_high_shift	= TS_HI_SHIFT,
	.ts_high_mask	= TS_HI_BIT << TS_HI_SHIFT,
	.ts_shift	= dma_ts_shift,
	.ts_shift_num	= ARRAY_SIZE(dma_ts_shift),
	.dmaor_init	= DMAOR_DME,
	.chclr_present	= 1,
	.dma_filter	= sysdma_filter,
	.clk_get	= sysdma_clk_get,
};

static struct resource r8a7790_sysdmal_resources[] = {
	{
		.start	= 0xe6700000,
		.end	= 0xe670a7ff,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "error_irq",
		.start	= gic_spi(197),
		.end	= gic_spi(197),
		.flags	= IORESOURCE_IRQ,
	},
	{
		/* IRQ for channels */
		.start	= gic_spi(200),
		.end	= gic_spi(214),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource r8a7790_sysdmau_resources[] = {
	{
		.start	= 0xe6720000,
		.end	= 0xe672a7ff,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "error_irq",
		.start	= gic_spi(220),
		.end	= gic_spi(220),
		.flags	= IORESOURCE_IRQ,
	},
	{
		/* IRQ for channels */
		.start	= gic_spi(216),
		.end	= gic_spi(219),
		.flags	= IORESOURCE_IRQ,
	},
	{
		/* IRQ for channels */
		.start	= gic_spi(308),
		.end	= gic_spi(318),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device sysdmal_device = {
	.name		= "sh-dmadesc-engine",
	.id		= SHDMA_DEVID_SYS_LO,
	.resource	= r8a7790_sysdmal_resources,
	.num_resources	= ARRAY_SIZE(r8a7790_sysdmal_resources),
	.dev		= {
		.platform_data	= &sysdma_platform_data,
	},
};

static struct platform_device sysdmau_device = {
	.name		= "sh-dmadesc-engine",
	.id		= SHDMA_DEVID_SYS_UP,
	.resource	= r8a7790_sysdmau_resources,
	.num_resources	= ARRAY_SIZE(r8a7790_sysdmau_resources),
	.dev		= {
		.platform_data	= &sysdma_platform_data,
	},
};

/* Audio */
static struct platform_device alsa_soc_platform_device = {
	.name		= "lager_alsa_soc_platform",
	.id		= 0,
};

static struct resource scu_resources[] = {
	[0] = {
		.name   = "scu",
		.start  = 0xec000000,
		.end    = 0xec500fff,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.name   = "ssiu",
		.start  = 0xec540000,
		.end    = 0xec54085f,
		.flags  = IORESOURCE_MEM,
	},
	[2] = {
		.name   = "ssi",
		.start  = 0xec541000,
		.end    = 0xec54127f,
		.flags  = IORESOURCE_MEM,
	},
	[3] = {
		.name   = "adg",
		.start  = 0xec5a0000,
		.end    = 0xec5a0067,
		.flags  = IORESOURCE_MEM,
	},
};

static struct platform_device scu_device = {
	.name		= "scu-pcm-audio",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(scu_resources),
	.resource	= scu_resources,
};

/* SATA0 */
static struct resource sata0_resources[] = {
	[0] = {
		.name	= "sata0",
		.start  = 0xee300000,
		.end    = (0xee500000 - 1),
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = gic_spi(105),
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device sata0_device = {
	.name		= "sata_rcar",
	.id		= 0,
	.resource	= sata0_resources,
	.num_resources	= ARRAY_SIZE(sata0_resources),
	.dev = {
		.dma_mask	= &sata0_device.dev.coherent_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};


/* SATA1 */
static struct resource sata1_resources[] = {
	[0] = {
		.name	= "sata1",
		.start  = 0xee500000,
		.end    = (0xee700000 - 1),
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = gic_spi(106),
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device sata1_device = {
	.name		= "sata_rcar",
	.id		= 1,
	.resource	= sata1_resources,
	.num_resources	= ARRAY_SIZE(sata1_resources),
	.dev = {
		.dma_mask	= &sata1_device.dev.coherent_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};


/* VIN */
static struct resource vin0_resources[] = {
	[0] = {
		.name = "VIN0",
		.start = 0xe6ef0000,
		.end = 0xe6ef0fff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = gic_spi(188),
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource vin1_resources[] = {
	[0] = {
		.name = "VIN1",
		.start = 0xe6ef1000,
		.end = 0xe6ef1fff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = gic_spi(189),
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource vin2_resources[] = {
	[0] = {
		.name = "VIN2",
		.start = 0xe6ef2000,
		.end = 0xe6ef2fff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = gic_spi(190),
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource vin3_resources[] = {
	[0] = {
		.name = "VIN3",
		.start = 0xe6ef3000,
		.end = 0xe6ef3fff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = gic_spi(191),
		.flags = IORESOURCE_IRQ,
	},
};

static struct vin_info vin_info[] = {
	[0] = {
		.input = VIN_INPUT_ITUR_BT709_24BIT,
		.flags = 0,
	},
	[1] = {
		.input = VIN_INPUT_ITUR_BT656_8BIT,
		.flags = 0,
	},
	[2] = {
		.input = VIN_INPUT_UNDEFINED,
		.flags = 0,
	},
	[3] = {
		.input = VIN_INPUT_UNDEFINED,
		.flags = 0,
	},
};

static u64 vin_dmamask = DMA_BIT_MASK(32);

static struct platform_device vin0_device = {
	.name  = "vin",
	.id = 0,
	.num_resources = ARRAY_SIZE(vin0_resources),
	.resource  = vin0_resources,
	.dev  = {
		.dma_mask = &vin_dmamask,
		.platform_data = &vin_info[0],
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct platform_device vin1_device = {
	.name  = "vin",
	.id = 1,
	.num_resources = ARRAY_SIZE(vin1_resources),
	.resource  = vin1_resources,
	.dev  = {
		.dma_mask = &vin_dmamask,
		.platform_data = &vin_info[1],
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct platform_device vin2_device = {
	.name  = "vin",
	.id = 2,
	.num_resources = ARRAY_SIZE(vin2_resources),
	.resource  = vin2_resources,
	.dev  = {
		.dma_mask = &vin_dmamask,
		.platform_data = &vin_info[2],
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct platform_device vin3_device = {
	.name  = "vin",
	.id = 3,
	.num_resources = ARRAY_SIZE(vin3_resources),
	.resource  = vin3_resources,
	.dev  = {
		.dma_mask = &vin_dmamask,
		.platform_data = &vin_info[3],
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct i2c_board_info lager_i2c_camera[] = {
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
	.board_info = &lager_i2c_camera[0],
	.i2c_adapter_id = 2,
	.module_name = "adv7612",
};

static struct soc_camera_link adv7180_ch1_link = {
	.bus_id = 1,
	.power  = adv7180_power,
	.board_info = &lager_i2c_camera[1],
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

static struct platform_device *r8a7790_early_devices[] __initdata = {
	&eth_device,
	&powervr_device,
	&ehci0_device,
	&ohci0_device,
	&ehci1_device,
	&ohci1_device,
#if defined(CONFIG_USB_XHCI_HCD)
	&xhci0_device,
#else
	&ehci2_device,
	&ohci2_device,
#endif /* CONFIG_USB_XHCI_HCD */
	&qspi_device,
	&sh_msiof0_device,
	&sh_msiof1_device,
	&sh_msiof2_device,
	&sh_msiof3_device,
	&i2c0_device,
	&i2c1_device,
	&i2c2_device,
	&i2c3_device,
	&audmal_device,
	&audmau_device,
	&audmapp_device,
	&sysdmal_device,
	&sysdmau_device,
	&alsa_soc_platform_device,
	&scu_device,
	&sata0_device,
	&sata1_device,
	&vin0_device,
	&vin1_device,
	&vin2_device,
	&vin3_device,
	&soc_camera_device[0],
	&soc_camera_device[1],
};

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
	r8a7790_pm_init();

	r8a7790_init_pm_domain(&r8a7790_rgx);

	regulator_register_fixed(0, fixed3v3_power_consumers,
				ARRAY_SIZE(fixed3v3_power_consumers));

	/* SD control registers IOCTRLn: SD pins driving ability */
	__raw_writel(~0xAAAAAAAA, 0xE6060000);	/* PMMR */
	__raw_writel(0xAAAAAAAA, 0xE6060060);	/* IOCTRL0 */
	__raw_writel(~0xAAAAAAAA, 0xE6060000);	/* PMMR */
	__raw_writel(0xAAAAAAAA, 0xE6060064);	/* IOCTRL1 */
	__raw_writel(~0x00110000, 0xE6060000);	/* PMMR */
	__raw_writel(0x00110000, 0xE6060088);	/* IOCTRL5 */
	__raw_writel(~0xFFFFFFFF, 0xE6060000);	/* PMMR */
	__raw_writel(0xFFFFFFFF, 0xE606008C);	/* IOCTRL6 */

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
	usbh_init();
	platform_add_devices(r8a7790_early_devices,
			     ARRAY_SIZE(r8a7790_early_devices));

	r8a7790_add_device_to_domain(&r8a7790_rgx, &powervr_device);

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
	.init		= r8a7790_timer_init,
};

#ifdef CONFIG_USE_OF
void __init r8a7790_add_standard_devices_dt(void)
{
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

static const char *r8a7790_boards_compat_dt[] __initdata = {
	"renesas,r8a7790",
	NULL,
};

DT_MACHINE_START(R8A7790_DT, "Generic R8A7790 (Flattened Device Tree)")
	.init_irq	= irqchip_init,
	.init_machine	= r8a7790_add_standard_devices_dt,
	.timer		= &r8a7790_timer,
	.dt_compat	= r8a7790_boards_compat_dt,
MACHINE_END
#endif /* CONFIG_USE_OF */
