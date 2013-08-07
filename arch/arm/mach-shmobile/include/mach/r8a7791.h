#ifndef __ASM_R8A7791_H__
#define __ASM_R8A7791_H__

#include <asm/mach/time.h>
#include <linux/pm_domain.h>

struct platform_device;

struct rcar_du_platform_data;

void r8a7791_add_standard_devices(void);
void r8a7791_add_du_device(struct rcar_du_platform_data *pdata);
void r8a7791_clock_init(void);
void r8a7791_pinmux_init(void);

extern struct sys_timer r8a7791_timer;
extern struct smp_operations r8a7791_smp_ops;

struct r8a7791_pm_ch {
	unsigned long chan_offs;
	unsigned int chan_bit;
	unsigned int isr_bit;
};

struct r8a7791_pm_domain {
	struct generic_pm_domain genpd;
	struct r8a7791_pm_ch ch;
};

static inline struct r8a7791_pm_ch *to_r8a7791_ch(struct generic_pm_domain *d)
{
	return &container_of(d, struct r8a7791_pm_domain, genpd)->ch;
}

extern int r8a7791_sysc_power_down(struct r8a7791_pm_ch *r8a7791_ch);
extern int r8a7791_sysc_power_up(struct r8a7791_pm_ch *r8a7791_ch);

#ifdef CONFIG_PM
extern struct r8a7791_pm_domain r8a7791_sgx;

extern void r8a7791_init_pm_domain(struct r8a7791_pm_domain *r8a7791_pd);
extern void r8a7791_add_device_to_domain(struct r8a7791_pm_domain *r8a7791_pd,
					 struct platform_device *pdev);
#else
#define r8a7791_init_pm_domain(pd) do { } while (0)
#define r8a7791_add_device_to_domain(pd, pdev) do { } while (0)
#endif /* CONFIG_PM */

/* USB Host */
#define SHUSBH_MAX_CH		3

#define SHUSBH_BASE		UL(0xee080000)
#define SHUSBH_OHCI_BASE	(SHUSBH_BASE)
#define SHUSBH_OHCI_SIZE	0x1000
#define SHUSBH_EHCI_BASE	(SHUSBH_BASE + 0x1000)
#define SHUSBH_EHCI_SIZE	0x1000

#define SHUSBH_XHCI_BASE	UL(0xEE000000)
#define SHUSBH_XHCI_SIZE	0x0C00

/* PCI Configuration Registers for AHB-PCI Bridge Registers */
#define PCI_CONF_AHBPCI_BAS	(SHUSBH_BASE + 0x10000)
#define VID_DID			0x0000
#define CMND_STS		0x0004
#define REVID_CC		0x0008
#define CLS_LT_HT_BIST		0x000C
#define BASEAD			0x0010
#define WIN1_BASEAD		0x0014
#define WIN2_BASEAD		0x0018
#define SSVID_SSID		0x002C

/* PCI Configuration Registers for OHCI/EHCI */
#define PCI_CONF_OHCI_BASE	(SHUSBH_BASE + 0x10000)
#define OHCI_VID_DID		0x0000
#define OHCI_CMND_STS		0x0004
#define OHCI_BASEAD		0x0010

#define PCI_CONF_EHCI_BASE	(SHUSBH_BASE + 0x10100)
#define EHCI_VID_DID		0x0000
#define EHCI_CMND_STS		0x0004
#define EHCI_BASEAD		0x0010

/* AHB-PCI Bridge Register */
#define AHBPCI_BASE		(SHUSBH_BASE + 0x10800)
#define PCIAHB_WIN1_CTR		0x0000
#define PCIAHB_WIN2_CTR		0x0004
#define PCIAHB_DCT_CTR		0x0008
#define AHBPCI_WIN1_CTR		0x0010
#define AHBPCI_WIN2_CTR		0x0014
#define AHBPCI_DCT_CTR		0x001C
#define PCI_INT_ENABLE		0x0020
#define PCI_INT_STATUS		0x0024
#define AHB_BUS_CTR		0x0030
#define USBCTR			0x0034
#define PCI_ARBITER_CTR		0x0040
#define PCI_UNIT_REV		0x004C

/* BIT */
#define BIT00			0x00000001
#define BIT01			0x00000002
#define BIT02			0x00000004
#define BIT03			0x00000008
#define BIT04			0x00000010
#define BIT05			0x00000020
#define BIT06			0x00000040
#define BIT07			0x00000080
#define BIT08			0x00000100
#define BIT09			0x00000200
#define BIT10			0x00000400
#define BIT11			0x00000800
#define BIT12			0x00001000
#define BIT13			0x00002000
#define BIT14			0x00004000
#define BIT15			0x00008000
#define BIT16			0x00010000
#define BIT17			0x00020000
#define BIT18			0x00040000
#define BIT19			0x00080000
#define BIT20			0x00100000
#define BIT21			0x00200000
#define BIT22			0x00400000
#define BIT23			0x00800000
#define BIT24			0x01000000
#define BIT25			0x02000000
#define BIT26			0x04000000
#define BIT27			0x08000000
#define BIT28			0x10000000
#define BIT29			0x20000000
#define BIT30			0x40000000
#define BIT31			0x80000000

/*** PCI Configration Register for OHCI ***/
/* VendorID, DeviceID 00h */
#define OHCI_ID			0x00351033

/*** PCI Configration Register for EHCI ***/
/* VendorID, DeviceID 00h */
#define EHCI_ID			0x00e01033

/*** PCI Configration Register for AHB-PCI Bridge ***/
/* CMND_STS 04h */
#define DETPERR			BIT31
#define SIGSERR			BIT30
#define REMABORT		BIT29
#define RETABORT		BIT28
#define SIGTABORT		BIT27
#define DEVTIM			(BIT26|BIT25)
#define MDPERR			BIT24
#define FBTBCAP			BIT23

#define _66MCAP			BIT21
#define CAPLIST			BIT20

#define FBTBEN			BIT09
#define SERREN			BIT08
#define STEPCTR			BIT07
#define PERREN			BIT06
#define VGAPSNP			BIT05
#define MWINVEN			BIT04
#define SPECIALC		BIT03
#define MASTEREN		BIT02
#define MEMEN			BIT01
#define IOEN			BIT00

/*** AHB-PCI Bridge Setting ***/
/* PCIAHB_WIN1_CTR 0x0800 */
#define PREFETCH		(BIT01|BIT00)
/* AHBPCI_WIN*_CTR 0810h,0814h */
#define PCIWIN1_PCICMD		(BIT03|BIT01)

#define AHB_CFG_HOST		0x80000000

#define AHB_CFG_AHBPCI		0x40000000
#define PCIWIN2_PCICMD		(BIT02|BIT01)

/* PCI_INT_ENABLE 0820h */
#define USBH_PMEEN		BIT19
#define USBH_INTBEN		BIT17
#define USBH_INTAEN		BIT16
#define PCIAHB_WIN2_INTEN	BIT13
#define PCIAHB_WIN1_INTEN	BIT12
#define RESERR_INTEN		BIT05
#define SIGSERR_INTEN		BIT04
#define PERR_INTEN		BIT03
#define REMARBORT_INTEN		BIT02
#define RETARBORT_INTEN		BIT01
#define SIGTARBORT_INTEN	BIT00

/* AHB_BUS_CTR 0830h */
#define AHB_BUS_CTR_SET \
	(BIT17 | BIT07 | BIT02 | BIT01 | BIT00)
/* USBCTR 0834h */
#define PCI_AHB_WIN1_SIZE_256M	0
#define PCI_AHB_WIN1_SIZE_512M	BIT10
#define PCI_AHB_WIN1_SIZE_1G	BIT11
#define PCI_AHB_WIN1_SIZE_2G	(BIT10 | BIT11)
#define DIRPD			BIT08
#define PLL_RST			BIT02
#define PCICLK_MASK		BIT01
#define USBH_RST		BIT00

/* PCI_ARBITER_CTR 0840h */
#define PCIBP_MODE		BIT12
#define PCIREQ1			BIT01
#define PCIREQ0			BIT00

/* DMA device IDs */
enum {
	SHDMA_DEVID_AUDIO_LO,
	SHDMA_DEVID_AUDIO_UP,
	SHDMA_DEVID_AUDIOPP,
	SHDMA_DEVID_SYS_LO,
	SHDMA_DEVID_SYS_UP,
};
#define SHDMA_DEVID_AUDIO	(SHDMA_DEVID_AUDIO_LO | SHDMA_DEVID_AUDIO_UP)

/* DMA slave IDs for Audio-DMAC and Audio-DMAC-pp */
enum {
	SHDMA_SLAVE_PCM_MEM_SSI0,
	SHDMA_SLAVE_PCM_MEM_SRC0,
	SHDMA_SLAVE_PCM_SSI1_MEM,
	SHDMA_SLAVE_PCM_SRC1_MEM,
	SHDMA_SLAVE_PCM_CMD1_MEM,
	SHDMA_SLAVE_PCM_SRC0_SSI0,
	SHDMA_SLAVE_PCM_CMD0_SSI0,
	SHDMA_SLAVE_PCM_SSI1_SRC1,
	SHDMA_SLAVE_PCM_MAX,
};

/* DMA slave IDs for SYS-DMAC */
enum {
	SHDMA_SLAVE_INVALID,
	SHDMA_SLAVE_SDHI0_TX,
	SHDMA_SLAVE_SDHI0_RX,
	SHDMA_SLAVE_SDHI1_TX,
	SHDMA_SLAVE_SDHI1_RX,
	SHDMA_SLAVE_SDHI2_TX,
	SHDMA_SLAVE_SDHI2_RX,
	SHDMA_SLAVE_MMC_TX,
	SHDMA_SLAVE_MMC_RX,
};

#endif /* __ASM_R8A7791_H__ */
