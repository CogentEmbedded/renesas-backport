#ifndef MFD_TMIO_H
#define MFD_TMIO_H

#include <linux/device.h>
#include <linux/fb.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/dmaengine.h>

#define tmio_ioread8(addr) readb(addr)
#define tmio_ioread16(addr) readw(addr)
#define tmio_ioread16_rep(r, b, l) readsw(r, b, l)
#define tmio_ioread32(addr) \
	(((u32) readw((addr))) | (((u32) readw((addr) + 2)) << 16))

#define tmio_iowrite8(val, addr) writeb((val), (addr))
#define tmio_iowrite16(val, addr) writew((val), (addr))
#define tmio_iowrite16_rep(r, b, l) writesw(r, b, l)
#define tmio_iowrite32(val, addr) \
	do { \
	writew((val),       (addr)); \
	writew((val) >> 16, (addr) + 2); \
	} while (0)

#define CNF_CMD     0x04
#define CNF_CTL_BASE   0x10
#define CNF_INT_PIN  0x3d
#define CNF_STOP_CLK_CTL 0x40
#define CNF_GCLK_CTL 0x41
#define CNF_SD_CLK_MODE 0x42
#define CNF_PIN_STATUS 0x44
#define CNF_PWR_CTL_1 0x48
#define CNF_PWR_CTL_2 0x49
#define CNF_PWR_CTL_3 0x4a
#define CNF_CARD_DETECT_MODE 0x4c
#define CNF_SD_SLOT 0x50
#define CNF_EXT_GCLK_CTL_1 0xf0
#define CNF_EXT_GCLK_CTL_2 0xf1
#define CNF_EXT_GCLK_CTL_3 0xf9
#define CNF_SD_LED_EN_1 0xfa
#define CNF_SD_LED_EN_2 0xfe

#define   SDCREN 0x2   /* Enable access to MMC CTL regs. (flag in COMMAND_REG)*/

#define sd_config_write8(base, shift, reg, val) \
	tmio_iowrite8((val), (base) + ((reg) << (shift)))
#define sd_config_write16(base, shift, reg, val) \
	tmio_iowrite16((val), (base) + ((reg) << (shift)))
#define sd_config_write32(base, shift, reg, val) \
	do { \
		tmio_iowrite16((val), (base) + ((reg) << (shift)));   \
		tmio_iowrite16((val) >> 16, (base) + ((reg + 2) << (shift))); \
	} while (0)

/* tmio MMC platform flags */
#define TMIO_MMC_WRPROTECT_DISABLE	(1 << 0)
/*
 * Some controllers can support a 2-byte block size when the bus width
 * is configured in 4-bit mode.
 */
#define TMIO_MMC_BLKSZ_2BYTES		(1 << 1)
/*
 * Some controllers can support SDIO IRQ signalling.
 */
#define TMIO_MMC_SDIO_IRQ		(1 << 2)
/*
 * Some platforms can detect card insertion events with controller powered
 * down, using a GPIO IRQ, in which case they have to fill in cd_irq, cd_gpio,
 * and cd_flags fields of struct tmio_mmc_data.
 */
#define TMIO_MMC_HAS_COLD_CD		(1 << 3)
/*
 * Some controllers require waiting for the SD bus to become
 * idle before writing to some registers.
 */
#define TMIO_MMC_HAS_IDLE_WAIT		(1 << 4)
/*
 * A GPIO is used for card hotplug detection. We need an extra flag for this,
 * because 0 is a valid GPIO number too, and requiring users to specify
 * cd_gpio < 0 to disable GPIO hotplug would break backwards compatibility.
 */
#define TMIO_MMC_USE_GPIO_CD		(1 << 5)

/* Switch the register of controller to access the buffer of 16 bit. */
#define TMIO_MMC_BUFF_16BITACC_ACTIVE_HIGH	(1 << 6)

/* NO_CTL_RESET_SDIO register don't work. */
#define TMIO_MMC_NO_CTL_RESET_SDIO	(1 << 7)

/* CTL_CLK_AND_WAIT_CTL register don't work. */
#define TMIO_MMC_NO_CTL_CLK_AND_WAIT_CTL	(1 << 8)

/* Some controllers check the ILL_FUNC bit. */
#define TMIO_MMC_CHECK_ILL_FUNC		(1 << 9)

/* The start or stop of SD clock don't wait 10msec. */
#define TMIO_MMC_CLK_NO_SLEEP		(1 << 10)

/* Add SDIO status reserved bits for SH Mobile series. */
#define TMIO_MMC_SDIO_STATUS_QUIRK	(1 << 11)

/* Actual clock rate. */
#define TMIO_MMC_CLK_ACTUAL		(1 << 12)

int tmio_core_mmc_enable(void __iomem *cnf, int shift, unsigned long base);
int tmio_core_mmc_resume(void __iomem *cnf, int shift, unsigned long base);
void tmio_core_mmc_pwr(void __iomem *cnf, int shift, int state);
void tmio_core_mmc_clk_div(void __iomem *cnf, int shift, int state);

struct tmio_mmc_dma {
	void *chan_priv_tx;
	void *chan_priv_rx;
	int alignment_shift;
};

struct tmio_mmc_host;

/*
 * data for the MMC controller
 */
struct tmio_mmc_data {
	unsigned int			hclk;
	unsigned long			capabilities;
	unsigned long			capabilities2;
	unsigned long			flags;
	u32				ocr_mask;	/* available voltages */
	struct tmio_mmc_dma		*dma;
	struct device			*dev;
	unsigned int			cd_gpio;
	void (*set_pwr)(struct platform_device *host, int state);
	void (*set_clk_div)(struct platform_device *host, int state);
	int (*get_cd)(struct platform_device *host);
	int (*get_ro)(struct platform_device *host);
	int (*start_signal_voltage_switch)(struct tmio_mmc_host *host,
						unsigned char signal_voltage);
	void (*init_tuning)(struct tmio_mmc_host *host, unsigned long *num);
	int (*prepare_tuning)(struct tmio_mmc_host *host, unsigned long tap);
	int (*select_tuning)(struct tmio_mmc_host *host, unsigned long *tap);
	bool (*retuning)(struct tmio_mmc_host *host);
	int (*write16_hook)(struct tmio_mmc_host *host, int addr);
	bool (*dma_filter)(struct dma_chan *chan, void *arg);
	void (*disable_auto_cmd12)(int *val);
	void (*set_transfer_size)(struct tmio_mmc_host *host, int enable);
	/* clock management callbacks */
	int (*clk_enable)(struct platform_device *pdev, unsigned int *f);
	void (*clk_disable)(struct platform_device *pdev);
};

/*
 * This function is deprecated and will be removed soon. Please, convert your
 * platform to use drivers/mmc/core/cd-gpio.c
 */
#include <linux/mmc/host.h>
static inline void tmio_mmc_cd_wakeup(struct tmio_mmc_data *pdata)
{
	if (pdata)
		mmc_detect_change(dev_get_drvdata(pdata->dev),
				  msecs_to_jiffies(100));
}

/*
 * data for the NAND controller
 */
struct tmio_nand_data {
	struct nand_bbt_descr	*badblock_pattern;
	struct mtd_partition	*partition;
	unsigned int		num_partitions;
};

#define FBIO_TMIO_ACC_WRITE	0x7C639300
#define FBIO_TMIO_ACC_SYNC	0x7C639301

struct tmio_fb_data {
	int			(*lcd_set_power)(struct platform_device *fb_dev,
								bool on);
	int			(*lcd_mode)(struct platform_device *fb_dev,
					const struct fb_videomode *mode);
	int			num_modes;
	struct fb_videomode	*modes;

	/* in mm: size of screen */
	int			height;
	int			width;
};


#endif
