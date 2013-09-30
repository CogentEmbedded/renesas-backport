/*
 * drivers/spi/spi-qspi.c
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
 *
 * spi-qspi.c QSPI bus driver
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/spi/spi.h>

/* SPI transfer mode definition */
#define	QSPI_NON_SEQUENTIAL	0	/* non sequential transfer mode */
#define	QSPI_SINGLE_RW_MODE	1	/* single transfer R/W mode */
#define	QSPI_DUAL_RW_MODE	2	/* dual transfer R/W mode */
#define	QSPI_QUAD_RW_MODE	4	/* quad transfer R/W mode */

/* timeout value for sending/receiving data */
#define QSPI_TIMEOUT		(3 * HZ)

/****************************************/
/* QSPI register address                */
/****************************************/

#define	QSPI_BASE	0xe6b10000	/* Base register */

#define QSPI_SPCR	0x00	/* Control register */
#define	QSPI_SSLP	0x01	/* Slave select polarity register */
#define	QSPI_SPPCR	0x02	/* Pin control register */
#define	QSPI_SPSR	0x03	/* Status register */
#define	QSPI_SPDR	0x04	/* Data register */
#define	QSPI_SPSCR	0x08	/* Sequence control register */
#define	QSPI_SPSSR	0x09	/* Sequence status register */
#define	QSPI_SPBR	0x0a	/* Bit rate register */
#define	QSPI_SPDCR	0x0b	/* Data control register */
#define	QSPI_SPCKD	0x0c	/* Clock delay register */
#define	QSPI_SSLND	0x0d	/* Slave select negation delay register */
#define	QSPI_SPND	0x0e	/* Next-access delay register */
#define	QSPI_SPCMD0	0x10	/* Command register 0 */
#define	QSPI_SPCMD1	0x12	/* Command register 1 */
#define	QSPI_SPCMD2	0x14	/* Command register 2 */
#define	QSPI_SPCMD3	0x16	/* Command register 3 */
#define	QSPI_SPBFCR	0x18	/* Buffer control register */
#define	QSPI_SPBDCR	0x1a	/* Buffer data count register */
#define	QSPI_SPBMUL0	0x1c	/* Multiplier setting register 0 */
#define	QSPI_SPBMUL1	0x20	/* Multiplier setting register 1 */
#define	QSPI_SPBMUL2	0x24	/* Multiplier setting register 2 */
#define	QSPI_SPBMUL3	0x28	/* Multiplier setting register 3 */


/* SPCR */
#define QSPI_SPCR_SPRIE		0x80	/* receive interrupt enable */
#define QSPI_SPCR_SPE		0x40	/* SPI function enable */
#define QSPI_SPCR_SPTIE		0x20	/* transmit interrupt enable */
#define QSPI_SPCR_SPEIE		0x10	/* error interrupt enable */
#define QSPI_SPCR_MSTR		0x08	/* master/slave mode select */

/* SPSR */
#define QSPI_SPSR_SPRFF_MASK	0x80	/* receive buffer full flag mask */
#define QSPI_SPSR_SPRFF_FULL	0x80	/* receive buffer is full */
#define QSPI_SPSR_SPRFF_LESS	0x00	/* receive buffer is not full */

#define QSPI_SPSR_TEND_MASK	0x40	/* transmit end flag mask */
#define QSPI_SPSR_TEND_COMP	0x40	/* transmission is completed */
#define QSPI_SPSR_TEND_NCOMP	0x00	/* transmission is not completed */

#define QSPI_SPSR_SPTEF_MASK	0x20	/* transmit buffer empty flag */
#define QSPI_SPSR_SPTEF_EMPTY	0x20	/* transmit buffer is empty */
#define QSPI_SPSR_SPTEF_	0x00	/* transmit buffer is not empty */

/* SPCMD */
#define QSPI_SPCMD_SXXDEN	0xe000	/* delay setting enable */
#define QSPI_SPCMD_SCKDEN	0x8000	/* clock delay setting enable */
#define QSPI_SPCMD_SLNDEN	0x4000	/* SSL Negation Delay setting enable */
#define QSPI_SPCMD_SPNDEN	0x2000	/* next access delay setting enable */
#define QSPI_SPCMD_LSBF		0x1000	/* LSB first */
#define QSPI_SPCMD_SSLKP	0x0080	/* SSL signal level keeping */
#define QSPI_SPCMD_SPB_MASK	0x0f00	/* transfer data length setting mask */
#define QSPI_SPCMD_SPB_8	0x0000	/* transfer data length is 8 */
#define QSPI_SPCMD_SPB_16	0x0100	/* transfer data length is 16 */
#define QSPI_SPCMD_SPB_32	0x0200	/* transfer data length is 32 */

#define QSPI_SPCMD_MOD_SINGLE	0x0000	/* single mode */
#define QSPI_SPCMD_MOD_DUAL	0x0020	/* dual mode */
#define QSPI_SPCMD_MOD_QUAD	0x0040	/* quad mode */

#define QSPI_SPCMD_SPRW_MASK	0x0010	/* spi read/write access */
#define QSPI_SPCMD_SPRW_WRITE	0x0000	/* write operation */
#define QSPI_SPCMD_SPRW_READ	0x0010	/* read operation */

#define QSPI_SPCMD_CPOL		0x0002	/* negative polarity */
#define QSPI_SPCMD_CPHA		0x0001	/* data shift on odd edge */

/* SPBFCR */
#define QSPI_SPBFCR_TXRST	0x80	/* transmit buffer data reset */
#define QSPI_SPBFCR_RXRST	0x40	/* receive buffer data reset */

#define QSPI_SPBFCR_TXTRG_MASK	0x30	/* transmit buffer triggering number */
#define QSPI_SPBFCR_TXTRG_31B	0x00	/*   31 bytes (1 byte available) */
#define QSPI_SPBFCR_TXTRG_0B	0x30	/*   0 byte (32 bytes available) */
#define QSPI_SPBFCR_RXTRG_MASK	0x07	/* receive buffer triggering number */
#define QSPI_SPBFCR_RXTRG_1B	0x00	/*   1 byte (31 bytes available) */
#define QSPI_SPBFCR_RXTRG_2B	0x01	/*   2 bytes (30 bytes available) */
#define QSPI_SPBFCR_RXTRG_4B	0x02	/*   4 bytes (28 bytes available) */
#define QSPI_SPBFCR_RXTRG_8B	0x04	/*   8 bytes (24 bytes available) */
#define QSPI_SPBFCR_RXTRG_32B	0x07	/*   32 bytes (0 byte avaliable) */

#define QSPI_BUFFER_SIZE	32u	/* transmit and receive buffer size */


/****************************************/
/* SPI Flash memory command definitions */
/****************************************/

#define SPI_FLASH_READ_COM		0x03	/* Read Data */
#define SPI_FLASH_4READ_COM		0x13	/* Read Data */
#define SPI_FLASH_FAST_READ_COM		0x0b	/* Read Data at Fast Speed */
#define SPI_FLASH_4FAST_READ_COM	0x0c	/* Read Data at Fast Speed */
#define SPI_FLASH_DDRFAST_READ_COM	0x0d	/* DDR Fast Read */
#define SPI_FLASH_4DDRFAST_READ_COM	0x0e	/* DDR Fast Read */
#define	SPI_FLASH_DUAL_READ_COM		0x3b	/* Dual Output Read */
#define	SPI_FLASH_4DUAL_READ_COM	0x3c	/* Dual Output Read */
#define	SPI_FLASH_QUAD_READ_COM		0x6b	/* Quad Output Read */
#define	SPI_FLASH_4QUAD_READ_COM	0x6c	/* Quad Output Read */
#define	SPI_FLASH_DIO_READ_COM		0xbb	/* Dual I/O High Perf. Read */
#define	SPI_FLASH_4DIO_READ_COM		0xbc	/* Dual I/O High Perf. Read */
#define	SPI_FLASH_DDRDIO_READ_COM	0xbd	/* DDR Dual I/O Read */
#define	SPI_FLASH_4DDRDIO_READ_COM	0xbe	/* DDR Dual I/O Read */
#define	SPI_FLASH_QIO_READ_COM		0xeb	/* Quad I/O High Perf. Read */
#define	SPI_FLASH_4QIO_READ_COM		0xec	/* Quad I/O High Perf. Read */
#define	SPI_FLASH_DDRQIO_READ_COM	0xed	/* DDR Quad I/O Read */
#define	SPI_FLASH_4DDRQIO_READ_COM	0xee	/* DDR Quad I/O Read */

#define	SPI_FLASH_PAGE_COM		0x02	/* Page Program */
#define	SPI_FLASH_4PAGE_COM		0x12	/* Page Program */
#define	SPI_FLASH_QUAD_PAGE_COM		0x32	/* Quad Page Program */
#define	SPI_FLASH_QUAD_PAGE2_COM	0x38	/* Quad Page Program */
#define	SPI_FLASH_4QUAD_PAGE_COM	0x34	/* Quad Page Program */


/* device private data */
struct qspi_priv {
	void __iomem *addr;
	u32 max_speed_hz;
	struct spi_master *master;
	struct spi_device *spi;
	struct device *dev;
	wait_queue_head_t wait;
	struct clk *clk;
	unsigned char spsr;
};


/*
 *		basic function
 */
static void qspi_write8(struct qspi_priv *qspi, int reg, u8 val)
{
	iowrite8(val, qspi->addr + reg);
}

static void qspi_write16(struct qspi_priv *qspi, int reg, u16 val)
{
	iowrite16(val, qspi->addr + reg);
}

static void qspi_write32(struct qspi_priv *qspi, int reg, u32 val)
{
	iowrite32(val, qspi->addr + reg);
}

static u8 qspi_read8(struct qspi_priv *qspi, int reg)
{
	return ioread8(qspi->addr + reg);
}

static u16 qspi_read16(struct qspi_priv *qspi, int reg)
{
	return ioread16(qspi->addr + reg);
}

static u32 qspi_read32(struct qspi_priv *qspi, int reg)
{
	return ioread32(qspi->addr + reg);
}

#define qspi_update8(spi, reg, mask, val) \
	qspi_write8(spi, reg, (qspi_read8(qspi, reg) & ~mask) | val);

static void qspi_enable_irq(struct qspi_priv *qspi, u8 enable)
{
	qspi_write8(qspi, QSPI_SPCR, qspi_read8(qspi, QSPI_SPCR) | enable);
}

static void qspi_disable_irq(struct qspi_priv *qspi, u8 disable)
{
	qspi_write8(qspi, QSPI_SPCR, qspi_read8(qspi, QSPI_SPCR) & ~disable);
}

static unsigned char qspi_calc_spbr(struct qspi_priv *qspi,
					struct spi_transfer *t)
{
	u32 target_rate;
	int spbr;

	target_rate = t ? t->speed_hz : 0;
	if (!target_rate)
		target_rate = qspi->max_speed_hz;

	/* BRDV0 and BRDV1 bits in SPCMD register should be 0 */
	spbr = DIV_ROUND_UP(clk_get_rate(qspi->clk), 2 * target_rate) - 1;
	spbr = clamp(spbr, 0, 255);

	return (unsigned char)spbr;
}

static int qspi_wait_for_interrupt(struct qspi_priv *qspi, u8 wait_mask,
				   u8 enable_bit)
{
	int ret;

	qspi_enable_irq(qspi, enable_bit);
	ret = wait_event_timeout(qspi->wait,
		qspi_read8(qspi, QSPI_SPSR) & wait_mask, QSPI_TIMEOUT);
	if (ret == 0) {
		dev_err(&qspi->master->dev, "interrupt timeout\n");
		ret = -ETIMEDOUT;
	}

	return ret;
}

#define qspi_wait_tr(spi) \
	qspi_wait_for_interrupt(spi, QSPI_SPSR_SPTEF_MASK, QSPI_SPCR_SPTIE)

#define qspi_wait_rc(spi) \
	qspi_wait_for_interrupt(spi, QSPI_SPSR_SPRFF_MASK, QSPI_SPCR_SPRIE)

static irqreturn_t qspi_irq(int irq, void *_sr)
{
	struct qspi_priv *qspi = (struct qspi_priv *)_sr;
	unsigned long spsr;
	irqreturn_t ret = IRQ_NONE;
	unsigned char disable_irq = 0;

	spsr = qspi_read8(qspi, QSPI_SPSR);
	if (spsr & QSPI_SPSR_SPRFF_FULL)
		disable_irq |= QSPI_SPCR_SPRIE;
	if (spsr & QSPI_SPSR_SPTEF_EMPTY)
		disable_irq |= QSPI_SPCR_SPTIE;

	if (disable_irq) {
		ret = IRQ_HANDLED;
		qspi_disable_irq(qspi, disable_irq);
		wake_up(&qspi->wait);
	}

	return ret;
}

static void qspi_hw_setup(struct qspi_priv *qspi)
{
	u16 spcmd;

	/* Disables SPI function */
	qspi_write8(qspi, QSPI_SPCR, QSPI_SPCR_MSTR);

	/* SSL signal low-active (default values) */
	qspi_write8(qspi, QSPI_SSLP, 0x00);

	/* Sets output values equal to previous transfer,
	 * and normal mode (default values) */
	qspi_write8(qspi, QSPI_SPPCR, 0x06);

	/* Sets the transfer bit rate */
	qspi_write8(qspi, QSPI_SPBR, qspi_calc_spbr(qspi, NULL));

	/* Disables dummy data transmission (default values) */
	qspi_write8(qspi, QSPI_SPDCR, 0x00);

	/* Sets the clock delay to 1.5 SPCLK cycles (default values) */
	qspi_write8(qspi, QSPI_SPCKD, 0x00);

	/* Sets 1 SPCLK cycles (default values) */
	qspi_write8(qspi, QSPI_SSLND, 0x00);

	/* Sets the next-access delay to 1 SPCLK cycles (default values) */
	qspi_write8(qspi, QSPI_SPND, 0x00);

	/* Sets the command register */
	/* Enables delay settings, keeps SSL signal */
	spcmd = QSPI_SPCMD_SXXDEN | QSPI_SPCMD_SSLKP;
	if (qspi->spi->mode & SPI_CPHA)
		/* data shift on odd edge, data latch on even edge */
		spcmd |= QSPI_SPCMD_CPHA;
	if (qspi->spi->mode & SPI_CPOL)
		/* negative polarity */
		spcmd |= QSPI_SPCMD_CPOL;

	qspi_write16(qspi, QSPI_SPCMD0, spcmd);
	qspi_write16(qspi, QSPI_SPCMD1, spcmd);
	/* Resets transfer data length */
	qspi_write32(qspi, QSPI_SPBMUL0, 0);
	qspi_write32(qspi, QSPI_SPBMUL1, 0);

	/* Resets transmit and receive buffer */
	qspi_write8(qspi, QSPI_SPBFCR, QSPI_SPBFCR_TXRST | QSPI_SPBFCR_RXRST);
	/* Sets transmit and receive buffer to allow normal operation */
	qspi_write8(qspi, QSPI_SPBFCR, 0x00);

	/* Sets sequence control to 0 (default values) */
	qspi_write8(qspi, QSPI_SPSCR, 0x00);

	/* Enables SPI function in a master mode */
	qspi_write8(qspi, QSPI_SPCR, QSPI_SPCR_SPE | QSPI_SPCR_MSTR);
}

static int qspi_init(struct qspi_priv *qspi,
			  struct spi_message *msg)
{
	struct device *dev = qspi->dev;
	struct spi_transfer *t = NULL, *t0 = NULL, *t1 = NULL;
	u16 spclk = 0;
	u16 spcmd0 = 0, spcmd1 = 0;
	u32 spbmul0 = 0, spbmul1 = 0;
	int seqno = 0;	int spi_mode = 0;

	/* Gets spi_transfer pointers */
	list_for_each_entry(t, &msg->transfers, transfer_list) {
		if (seqno == 0)
			t0 = t;
		else if (seqno == 1)
			t1 = t;
		else
			dev_err(dev, "too many sequences\n");
		seqno++;
	}

	/* Disables SPI function */
	qspi_write8(qspi, QSPI_SPCR, QSPI_SPCR_MSTR);

	/* Sets SPCLK phase and polarity settings */
	if (qspi->spi->mode & SPI_CPHA)
		/* data shift on odd edge, data latch on even edge */
		spclk |= QSPI_SPCMD_CPHA;
	if (qspi->spi->mode & SPI_CPOL)
		/* negative polarity */
		spclk |= QSPI_SPCMD_CPOL;

	/**************************************/
	/* Sets the command register 0 and 1  */
	/* These depend on SPI Flash memory   */
	/**************************************/

	/* Checks the instruction/command code */
	if (t0 && t0->tx_buf) {
		switch (*(u8 *)(t0->tx_buf)) {
		case SPI_FLASH_READ_COM:
		case SPI_FLASH_4READ_COM:
		case SPI_FLASH_FAST_READ_COM:
		case SPI_FLASH_4FAST_READ_COM:
		case SPI_FLASH_PAGE_COM:
		case SPI_FLASH_4PAGE_COM:
			spi_mode = QSPI_SINGLE_RW_MODE;
			/* Enables clock delay setting, keeps SSL signal */
			spcmd0 = QSPI_SPCMD_SCKDEN | QSPI_SPCMD_SSLKP | spclk;
			/* Negates all SSL signal */
			spcmd1 = QSPI_SPCMD_SLNDEN | QSPI_SPCMD_SPNDEN | spclk;
			spcmd1 |= QSPI_SPCMD_MOD_SINGLE;
			break;
		case SPI_FLASH_DUAL_READ_COM:
		case SPI_FLASH_4DUAL_READ_COM:
			spi_mode = QSPI_DUAL_RW_MODE;
			/* Enables clock delay setting, keeps SSL signal */
			spcmd0 = QSPI_SPCMD_SCKDEN | QSPI_SPCMD_SSLKP | spclk;
			/* Negates all SSL signal */
			spcmd1 = QSPI_SPCMD_SLNDEN | QSPI_SPCMD_SPNDEN | spclk;
			spcmd1 |= QSPI_SPCMD_MOD_DUAL | QSPI_SPCMD_SPRW_READ;
			break;
		case SPI_FLASH_QUAD_READ_COM:
		case SPI_FLASH_4QUAD_READ_COM:
			spi_mode = QSPI_QUAD_RW_MODE;
			/* Enables clock delay setting, keeps SSL signal */
			spcmd0 = QSPI_SPCMD_SCKDEN | QSPI_SPCMD_SSLKP | spclk;
			/* Negates all SSL signal */
			spcmd1 = QSPI_SPCMD_SXXDEN | spclk;
			spcmd1 |= QSPI_SPCMD_MOD_QUAD | QSPI_SPCMD_SPRW_READ;
			break;
		case SPI_FLASH_QUAD_PAGE_COM:
		case SPI_FLASH_QUAD_PAGE2_COM:
		case SPI_FLASH_4QUAD_PAGE_COM:
			spi_mode = QSPI_QUAD_RW_MODE;
			/* Enables clock delay setting, keeps SSL signal */
			spcmd0 = QSPI_SPCMD_SXXDEN | QSPI_SPCMD_SSLKP | spclk;
			/* Negates all SSL signal */
			spcmd1 = QSPI_SPCMD_SLNDEN | QSPI_SPCMD_SPNDEN | spclk;
			spcmd1 |= QSPI_SPCMD_MOD_QUAD | QSPI_SPCMD_SPRW_WRITE;
			break;
		/* These commands are not supported */
		case SPI_FLASH_DIO_READ_COM:
		case SPI_FLASH_4DIO_READ_COM:
		case SPI_FLASH_DDRDIO_READ_COM:
		case SPI_FLASH_4DDRDIO_READ_COM:
		case SPI_FLASH_QIO_READ_COM:
		case SPI_FLASH_4QIO_READ_COM:
		case SPI_FLASH_DDRQIO_READ_COM:
		case SPI_FLASH_4DDRQIO_READ_COM:
			dev_err(&qspi->master->dev,
				"not supported spi flash command (%x)\n",
				*(u8 *)(t0->tx_buf));
			spi_mode = -EINVAL;
		default:
			spi_mode = QSPI_NON_SEQUENTIAL;
			/* Enables clock delay setting, keeps SSL signal */
			spcmd0 = QSPI_SPCMD_SXXDEN | QSPI_SPCMD_SSLKP | spclk;
			/* Enables clock delay setting, keeps SSL signal */
			spcmd1 = QSPI_SPCMD_SCKDEN | QSPI_SPCMD_SSLKP | spclk;
			spcmd1 |= QSPI_SPCMD_MOD_SINGLE;
			break;
		}
		spbmul0 = t0->len;
	}

	/* Gets number of data to be read/write */
	if (t1) {
		if (t1->bits_per_word == 32) {
			spcmd1 |= QSPI_SPCMD_SPB_32;
			spbmul1 = t1->len/4;
		} else if (t1->bits_per_word == 16) {
			spcmd1 |= QSPI_SPCMD_SPB_16;
			spbmul1 = t1->len/2;
		} else {
			spcmd1 |= QSPI_SPCMD_SPB_8;
			spbmul1 = t1->len;
		}
	}

	if (spi_mode == QSPI_NON_SEQUENTIAL) {
		/* Sets sequence control to 0 */
		qspi_write8(qspi, QSPI_SPSCR, 0x00);
	} else {
		/* Sets sequence control to 1 */
		qspi_write8(qspi, QSPI_SPSCR, 0x01);
	}

	/* First command register is used to transfer command and address */
	/* It is always in single SPI mode */
	qspi_write16(qspi, QSPI_SPCMD0, spcmd0);
	/* Transfer data length */
	qspi_write32(qspi, QSPI_SPBMUL0, spbmul0);

	/* Second command register is used to transfer or receive data */
	qspi_write16(qspi, QSPI_SPCMD1, spcmd1);
	/* Transfer data length */
	qspi_write32(qspi, QSPI_SPBMUL1, spbmul1);

	dev_dbg(qspi->dev,
		"spimode=%d, cmd0=%04x, mul0=%d, cmd1=%04x, mul1=%d\n",
		spi_mode, spcmd0, spbmul0, spcmd1, spbmul1);

	/* Resets transmit and receive buffer */
	qspi_write8(qspi, QSPI_SPBFCR, QSPI_SPBFCR_TXRST | QSPI_SPBFCR_RXRST);
	/* Sets transmit and receive buffer to allow normal operation */
	/* transmit buffer data triggering number set to 0 bytes */
	/* receive buffer data triggering number set to 32 bytes */
	qspi_write8(qspi, QSPI_SPBFCR,
		QSPI_SPBFCR_TXTRG_0B | QSPI_SPBFCR_RXTRG_32B);

	/* Enables SPI function in a master mode */
	qspi_write8(qspi, QSPI_SPCR, QSPI_SPCR_SPE | QSPI_SPCR_MSTR);

	return spi_mode;
}

static int qspi_single_tx_msg8(struct qspi_priv *qspi, unsigned count,
				  const u8 *txbuf, u8 *rxbuf)
{
	struct device *dev = &qspi->master->dev;
	unsigned remain, i, n;
	int ret = 0;

	dev_dbg(qspi->dev, "%s count=%d, txbuf=%lx, rxbuf=%lx\n",
		__func__, count, (ulong)txbuf, (ulong)rxbuf);

	remain = count;
	while (remain) {

		/* Wait transmit */
		if (qspi_wait_tr(qspi) < 0) {
			dev_err(dev, "tx empty timeout\n");
			ret = -ETIMEDOUT;
			break;
		}

		n = min(remain, QSPI_BUFFER_SIZE);

		if (n >= QSPI_BUFFER_SIZE) {
			/* sets triggering number to 32 bytes */
			qspi_update8(qspi, QSPI_SPBFCR, QSPI_SPBFCR_RXTRG_MASK,
				QSPI_SPBFCR_RXTRG_32B);
		} else {
			/* sets triggering number to 1 byte */
			qspi_update8(qspi, QSPI_SPBFCR, QSPI_SPBFCR_RXTRG_MASK,
				QSPI_SPBFCR_RXTRG_1B);
		}

		for (i = 0; i < n; i++) {
			/* if tranmit data available */
			if (txbuf) {
				qspi_write8(qspi, QSPI_SPDR, *txbuf++);
			} else {
				/* dummy write to generate clock */
				qspi_write8(qspi, QSPI_SPDR, 0x00);
			}
		}

		if (n >= QSPI_BUFFER_SIZE) {
			/* Wait receive */
			if (qspi_wait_rc(qspi) < 0) {
				dev_err(dev, "rx full timeout\n");
				ret = -ETIMEDOUT;
				break;
			}

			for (i = 0; i < n; i++) {
				/* if need to read */
				if (rxbuf) {
					*rxbuf++ = qspi_read8(qspi, QSPI_SPDR);
				} else {
					/* dummy read */
					qspi_read8(qspi, QSPI_SPDR);
				}
			}
		} else {
			for (i = 0; i < n; i++) {
				/* Wait receive */
				if (qspi_wait_rc(qspi) < 0) {
					dev_err(dev, "rx full timeout\n");
					ret = -ETIMEDOUT;
					break;
				}
				/* if need to read */
				if (rxbuf) {
					*rxbuf++ = qspi_read8(qspi, QSPI_SPDR);
				} else {
					/* dummy read */
					qspi_read8(qspi, QSPI_SPDR);
				}
			}
		}
		remain -= n;
	}

	return ret;
}

static int qspi_single_tx_msg16(struct qspi_priv *qspi, unsigned count,
				  const u16 *txbuf, u16 *rxbuf)
{
	struct device *dev = &qspi->master->dev;
	unsigned remain, i, n;
	int ret = 0;

	dev_dbg(qspi->dev, "%s count=%d, txbuf=%lx, rxbuf=%lx\n",
		__func__, count, (ulong)txbuf, (ulong)rxbuf);

	remain = count;
	while (remain) {

		/* Wait transmit */
		if (qspi_wait_tr(qspi) < 0) {
			dev_err(dev, "tx empty timeout\n");
			ret = -ETIMEDOUT;
			break;
		}

		n = min(remain, QSPI_BUFFER_SIZE/2);

		if (n >= (QSPI_BUFFER_SIZE/2)) {
			/* sets triggering number to 32 bytes */
			qspi_update8(qspi, QSPI_SPBFCR, QSPI_SPBFCR_RXTRG_MASK,
				QSPI_SPBFCR_RXTRG_32B);
		} else {
			/* sets triggering number to 2 bytes */
			qspi_update8(qspi, QSPI_SPBFCR, QSPI_SPBFCR_RXTRG_MASK,
				QSPI_SPBFCR_RXTRG_2B);
		}

		for (i = 0; i < n; i++) {
			/* if tranmit data available */
			if (txbuf) {
				qspi_write16(qspi, QSPI_SPDR, *txbuf++);
			} else {
				/* dummy write to generate clock */
				qspi_write16(qspi, QSPI_SPDR, 0x00);
			}
		}

		if (n >= (QSPI_BUFFER_SIZE/2)) {
			/* Wait receive */
			if (qspi_wait_rc(qspi) < 0) {
				dev_err(dev, "rx full timeout\n");
				ret = -ETIMEDOUT;
				break;
			}

			for (i = 0; i < n; i++) {
				/* if need to read */
				if (rxbuf) {
					*rxbuf++ = qspi_read16(qspi, QSPI_SPDR);
				} else {
					/* dummy read */
					qspi_read16(qspi, QSPI_SPDR);
				}
			}
		} else {
			for (i = 0; i < n; i++) {
				/* Wait receive */
				if (qspi_wait_rc(qspi) < 0) {
					dev_err(dev, "rx full timeout\n");
					ret = -ETIMEDOUT;
					break;
				}

				/* if need to read */
				if (rxbuf) {
					*rxbuf++ = qspi_read16(qspi, QSPI_SPDR);
				} else {
					/* dummy read */
					qspi_read16(qspi, QSPI_SPDR);
				}
			}
		}
		remain -= n;
	}

	return ret;
}

static int qspi_single_tx_msg32(struct qspi_priv *qspi, unsigned count,
				  const u32 *txbuf, u32 *rxbuf)
{
	struct device *dev = &qspi->master->dev;
	unsigned remain, i, n;
	int ret = 0;

	dev_dbg(qspi->dev, "%s count=%d, txbuf=%lx, rxbuf=%lx\n",
		__func__, count, (ulong)txbuf, (ulong)rxbuf);

	remain = count;
	while (remain) {

		/* Wait transmit */
		if (qspi_wait_tr(qspi) < 0) {
			dev_err(dev, "tx empty timeout\n");
			ret = -ETIMEDOUT;
			break;
		}

		n = min(remain, QSPI_BUFFER_SIZE/4);

		if (n >= (QSPI_BUFFER_SIZE/4)) {
			/* sets triggering number to 32 bytes */
			qspi_update8(qspi, QSPI_SPBFCR, QSPI_SPBFCR_RXTRG_MASK,
				QSPI_SPBFCR_RXTRG_32B);
		} else {
			/* sets triggering number to 4 bytes */
			qspi_update8(qspi, QSPI_SPBFCR, QSPI_SPBFCR_RXTRG_MASK,
				QSPI_SPBFCR_RXTRG_4B);
		}

		for (i = 0; i < n; i++) {
			/* if tranmit data available */
			if (txbuf) {
				qspi_write32(qspi, QSPI_SPDR, *txbuf++);
			} else {
				/* dummy write to generate clock */
				qspi_write32(qspi, QSPI_SPDR, 0x00);
			}
		}

		if (n >= (QSPI_BUFFER_SIZE/4)) {
			/* Wait receive */
			if (qspi_wait_rc(qspi) < 0) {
				dev_err(dev, "rx full timeout\n");
				ret = -ETIMEDOUT;
				break;
			}

			for (i = 0; i < n; i++) {
				/* if need to read */
				if (rxbuf) {
					*rxbuf++ = qspi_read32(qspi, QSPI_SPDR);
				} else {
					/* dummy read */
					qspi_read32(qspi, QSPI_SPDR);
				}
			}
		} else {

			for (i = 0; i < n; i++) {
				/* Wait receive */
				if (qspi_wait_rc(qspi) < 0) {
					dev_err(dev, "rx full timeout\n");
					ret = -ETIMEDOUT;
					break;
				}

				/* if need to read */
				if (rxbuf) {
					*rxbuf++ = qspi_read32(qspi, QSPI_SPDR);
				} else {
					/* dummy read */
					qspi_read32(qspi, QSPI_SPDR);
				}
			}
		}
		remain -= n;
	}

	return ret;
}

static int qspi_quad_tx_msg8(struct qspi_priv *qspi, unsigned count,
				  const u8 *txbuf, u8 *rxbuf)
{
	struct device *dev = &qspi->master->dev;
	unsigned remain, i, n;
	int ret = 0;

	dev_dbg(qspi->dev, "%s count=%d, txbuf=%lx, rxbuf=%lx\n",
		__func__, count, (ulong)txbuf, (ulong)rxbuf);

	/* if tranmit data available */
	if (txbuf) {
		remain = count;
		while (remain) {

			/* Wait transmit */
			if (qspi_wait_tr(qspi) < 0) {
				dev_err(dev, "tx empty timeout\n");
				return -ETIMEDOUT;
			}

			n = min(remain, QSPI_BUFFER_SIZE);

			for (i = 0; i < n; i++)
				qspi_write8(qspi, QSPI_SPDR, *txbuf++);
			remain -= n;
		}
		/* Wait transmit */
		if (qspi_wait_tr(qspi) < 0) {
			dev_err(dev, "tx empty timeout\n");
			return -ETIMEDOUT;
		}
	}

	/* if need to read */
	if (rxbuf) {
		remain = count;
		while (remain) {

			n = min(remain, QSPI_BUFFER_SIZE);

			if (n >= QSPI_BUFFER_SIZE) {
				/* sets triggering number to 32 bytes */
				qspi_update8(qspi, QSPI_SPBFCR,
					QSPI_SPBFCR_RXTRG_MASK,
					QSPI_SPBFCR_RXTRG_32B);
			} else {
				/* sets triggering number to 1 byte */
				qspi_update8(qspi, QSPI_SPBFCR,
					QSPI_SPBFCR_RXTRG_MASK,
					QSPI_SPBFCR_RXTRG_1B);
			}

			if (n >= QSPI_BUFFER_SIZE) {
				/* Wait receive */
				if (qspi_wait_rc(qspi) < 0) {
					dev_err(dev, "rx full timeout\n");
					ret = -ETIMEDOUT;
					break;
				}

				for (i = 0; i < n; i++)
					*rxbuf++ = qspi_read8(qspi, QSPI_SPDR);
			} else {
				for (i = 0; i < n; i++) {
					/* Wait receive */
					if (qspi_wait_rc(qspi) < 0) {
						dev_err(dev, "rx full timeout\n");
						ret = -ETIMEDOUT;
						break;
					}
					*rxbuf++ = qspi_read8(qspi, QSPI_SPDR);
				}
			}
			remain -= n;
		}
	}

	return ret;
}

static int qspi_quad_tx_msg16(struct qspi_priv *qspi, unsigned count,
				  const u16 *txbuf, u16 *rxbuf)
{
	struct device *dev = &qspi->master->dev;
	unsigned remain, i, n;
	int ret = 0;

	dev_dbg(qspi->dev, "%s count=%d, txbuf=%lx, rxbuf=%lx\n",
		__func__, count, (ulong)txbuf, (ulong)rxbuf);

	/* if tranmit data available */
	if (txbuf) {
		remain = count;
		while (remain) {

			/* Wait transmit */
			if (qspi_wait_tr(qspi) < 0) {
				dev_err(dev, "tx empty timeout\n");
				return -ETIMEDOUT;
			}

			n = min(remain, QSPI_BUFFER_SIZE/2);

			for (i = 0; i < n; i++)
				qspi_write16(qspi, QSPI_SPDR, *txbuf++);

			remain -= n;
		}
		/* Wait transmit */
		if (qspi_wait_tr(qspi) < 0) {
			dev_err(dev, "tx empty timeout\n");
			return -ETIMEDOUT;
		}
	}

	/* if need to read */
	if (rxbuf) {
		remain = count;
		while (remain) {

			n = min(remain, QSPI_BUFFER_SIZE/2);

			if (n >= (QSPI_BUFFER_SIZE/2)) {
				/* sets triggering number to 32 bytes */
				qspi_update8(qspi, QSPI_SPBFCR,
					QSPI_SPBFCR_RXTRG_MASK,
					QSPI_SPBFCR_RXTRG_32B);
			} else {
				/* sets triggering number to 2 bytes */
				qspi_update8(qspi, QSPI_SPBFCR,
					QSPI_SPBFCR_RXTRG_MASK,
					QSPI_SPBFCR_RXTRG_2B);
			}

			if (n >= (QSPI_BUFFER_SIZE/2)) {
				/* Wait receive */
				if (qspi_wait_rc(qspi) < 0) {
					dev_err(dev, "rx full timeout\n");
					ret = -ETIMEDOUT;
					break;
				}

				for (i = 0; i < n; i++)
					*rxbuf++ = qspi_read16(qspi, QSPI_SPDR);
			} else {
				for (i = 0; i < n; i++) {
					/* Wait receive */
					if (qspi_wait_rc(qspi) < 0) {
						dev_err(dev, "rx full timeout\n");
						ret = -ETIMEDOUT;
						break;
					}
					*rxbuf++ = qspi_read16(qspi, QSPI_SPDR);
				}
			}
			remain -= n;
		}
	}

	return ret;
}

static int qspi_quad_tx_msg32(struct qspi_priv *qspi, unsigned count,
				  const u32 *txbuf, u32 *rxbuf)
{
	struct device *dev = &qspi->master->dev;
	unsigned remain, i, n;
	int ret = 0;

	dev_dbg(qspi->dev, "%s count=%d, txbuf=%lx, rxbuf=%lx\n",
		__func__, count, (ulong)txbuf, (ulong)rxbuf);

	/* if tranmit data available */
	if (txbuf) {
		remain = count;
		while (remain) {

			/* Wait transmit */
			if (qspi_wait_tr(qspi) < 0) {
				dev_err(dev, "tx empty timeout\n");
				return -ETIMEDOUT;
			}

			n = min(remain, QSPI_BUFFER_SIZE/4);

			for (i = 0; i < n; i++)
				qspi_write32(qspi, QSPI_SPDR, *txbuf++);

			remain -= n;
		}
		/* Wait transmit */
		if (qspi_wait_tr(qspi) < 0) {
			dev_err(dev, "tx empty timeout\n");
			return -ETIMEDOUT;
		}
	}

	/* if need to read */
	if (rxbuf) {
		remain = count;
		while (remain) {

			n = min(remain, QSPI_BUFFER_SIZE/4);

			if (n >= (QSPI_BUFFER_SIZE/4)) {
				/* sets triggering number to 32 bytes */
				qspi_update8(qspi, QSPI_SPBFCR,
					QSPI_SPBFCR_RXTRG_MASK,
					QSPI_SPBFCR_RXTRG_32B);
			} else {
				/* sets triggering number to 4 bytes */
				qspi_update8(qspi, QSPI_SPBFCR,
					QSPI_SPBFCR_RXTRG_MASK,
					QSPI_SPBFCR_RXTRG_4B);
			}

			if (n >= (QSPI_BUFFER_SIZE/4)) {
				/* Wait receive */
				if (qspi_wait_rc(qspi) < 0) {
					dev_err(dev, "rx full timeout\n");
					ret = -ETIMEDOUT;
					break;
				}

				for (i = 0; i < n; i++)
					*rxbuf++ = qspi_read32(qspi, QSPI_SPDR);
			} else {
				for (i = 0; i < n; i++) {
					/* Wait receive */
					if (qspi_wait_rc(qspi) < 0) {
						dev_err(dev, "rx full timeout\n");
						ret = -ETIMEDOUT;
						break;
					}
					*rxbuf++ = qspi_read32(qspi, QSPI_SPDR);
				}
			}
			remain -= n;
		}
	}

	return ret;
}


/*
 *		spi master function
 */
static int qspi_prepare_transfer(struct spi_master *master)
{
	struct qspi_priv *qspi = spi_master_get_devdata(master);

	pm_runtime_get_sync(qspi->dev);
	return 0;
}

static int qspi_unprepare_transfer(struct spi_master *master)
{
	struct qspi_priv *qspi = spi_master_get_devdata(master);

	pm_runtime_put_sync(qspi->dev);
	return 0;
}

static int qspi_transfer_one_message(struct spi_master *master,
				     struct spi_message *msg)
{
	struct qspi_priv *qspi = spi_master_get_devdata(master);
	struct spi_transfer *t;
	int spi_mode = 0;
	int seqno;
	int ret;

	dev_dbg(qspi->dev, "%s\n", __func__);

	spi_mode = qspi_init(qspi, msg);
	if (spi_mode < 0)
		return spi_mode;

	seqno = 0;
	ret = 0;
	list_for_each_entry(t, &msg->transfers, transfer_list) {

		/* single mode transfer */
		if (spi_mode == QSPI_NON_SEQUENTIAL ||
			spi_mode == QSPI_SINGLE_RW_MODE ||
			seqno == 0) {
			if (t->bits_per_word == 32)
				ret = qspi_single_tx_msg32(qspi, t->len / 4,
					t->tx_buf, t->rx_buf);
			else if (t->bits_per_word == 16)
				ret = qspi_single_tx_msg16(qspi, t->len / 2,
					t->tx_buf, t->rx_buf);
			else
				ret = qspi_single_tx_msg8(qspi, t->len,
					t->tx_buf, t->rx_buf);
		} else {
		/* dual and quad modes are same transfer procedure */
			if (t->bits_per_word == 32)
				ret = qspi_quad_tx_msg32(qspi, t->len / 4,
					t->tx_buf, t->rx_buf);
			else if (t->bits_per_word == 16)
				ret = qspi_quad_tx_msg16(qspi, t->len / 2,
					t->tx_buf, t->rx_buf);
			else
				ret = qspi_quad_tx_msg8(qspi, t->len,
					t->tx_buf, t->rx_buf);
		}

		if (ret)
			break;

		msg->actual_length += t->len;

		if (t->delay_usecs)
			udelay(t->delay_usecs);

		seqno++;
	}

	msg->status = ret;
	spi_finalize_current_message(master);

	return ret;
}

static int qspi_setup(struct spi_device *spi)
{
	struct qspi_priv *qspi = spi_master_get_devdata(spi->master);
	struct device *dev = qspi->dev;

	if (!spi->bits_per_word)
		spi->bits_per_word = 8;

	if ((spi->bits_per_word < 8) || (spi->bits_per_word > 32)) {
		dev_err(dev, "%d bits_per_word is not supported\n",
			spi->bits_per_word);
		return -EIO;
	}

	qspi->spi = spi;
	qspi->max_speed_hz = spi->max_speed_hz;

	qspi_hw_setup(qspi);

	dev_dbg(dev, "%s setup\n", spi->modalias);

	return 0;
}

static void qspi_cleanup(struct spi_device *spi)
{
	struct qspi_priv *qspi = spi_master_get_devdata(spi->master);
	struct device *dev = qspi->dev;

	dev_dbg(dev, "%s cleanup\n", spi->modalias);
}


static int __devinit qspi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct spi_master *master;
	struct qspi_priv *qspi;
	struct clk *clk;
	int ret, irq;

	/* get base addr */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "invalid resource\n");
		return -EINVAL;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "platform_get_irq failed\n");
		return -ENODEV;
	}

	master = spi_alloc_master(&pdev->dev, sizeof(*qspi));
	if (!master) {
		dev_err(&pdev->dev, "spi_alloc_master error.\n");
		return -ENOMEM;
	}

	clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "qspi is required\n");
		ret = PTR_ERR(clk);
		goto error0;
	}

	qspi = spi_master_get_devdata(master);
	dev_set_drvdata(&pdev->dev, qspi);

	/* init qspi */
	qspi->master	= master;
	qspi->dev	= &pdev->dev;
	qspi->clk	= clk;
	qspi->addr	= devm_ioremap(qspi->dev,
				       res->start, resource_size(res));
	if (!qspi->addr) {
		dev_err(&pdev->dev, "ioremap error.\n");
		ret = -ENOMEM;
		goto error1;
	}
	init_waitqueue_head(&qspi->wait);

	master->num_chipselect	= 1;
	master->bus_num		= pdev->id;
	master->setup		= qspi_setup;
	master->cleanup		= qspi_cleanup;
	master->mode_bits	= SPI_CPOL | SPI_CPHA;
	master->prepare_transfer_hardware	= qspi_prepare_transfer;
	master->transfer_one_message		= qspi_transfer_one_message;
	master->unprepare_transfer_hardware	= qspi_unprepare_transfer;

	ret = request_irq(irq, qspi_irq, 0, dev_name(&pdev->dev), qspi);
	if (ret) {
		dev_dbg(&pdev->dev, "request_irq failed\n");
		goto error2;
	}

	ret = spi_register_master(master);
	if (ret < 0) {
		dev_err(&pdev->dev, "spi_register_master error.\n");
		goto error3;
	}

	pm_runtime_enable(&pdev->dev);

	dev_info(&pdev->dev, "probed\n");

	return 0;

 error3:
	free_irq(irq, qspi);
 error2:
	devm_iounmap(qspi->dev, qspi->addr);
 error1:
	clk_put(clk);
 error0:
	spi_master_put(master);

	return ret;
}

static int __devexit qspi_remove(struct platform_device *pdev)
{
	struct qspi_priv *qspi = dev_get_drvdata(&pdev->dev);

	pm_runtime_disable(&pdev->dev);

	clk_put(qspi->clk);
	free_irq(platform_get_irq(pdev, 0), qspi);
	spi_unregister_master(qspi->master);
	devm_iounmap(qspi->dev, qspi->addr);

	return 0;
}

#ifdef CONFIG_PM
static int qspi_suspend(struct device *dev)
{
	struct qspi_priv *qspi = dev_get_drvdata(dev);
	int ret;

	ret = spi_master_suspend(qspi->master);
	if (ret) {
		dev_warn(dev, "cannot suspend master\n");
		return ret;
	}

	dev_dbg(dev, "suspended\n");
	return 0;
}

static int qspi_resume(struct device *dev)
{
	struct qspi_priv *qspi = dev_get_drvdata(dev);
	int ret;

	/* Start the queue running */
	ret = spi_master_resume(qspi->master);
	if (ret)
		dev_err(dev, "problem starting queue (%d)\n", ret);
	else
		dev_dbg(dev, "resumed\n");

	return ret;
}
#endif	/* CONFIG_PM */

#ifdef CONFIG_PM_RUNTIME
static int qspi_runtime_suspend(struct device *dev)
{
	struct qspi_priv *qspi = dev_get_drvdata(dev);

	clk_disable(qspi->clk);

	return 0;
}

static int qspi_runtime_resume(struct device *dev)
{
	struct qspi_priv *qspi = dev_get_drvdata(dev);

	clk_enable(qspi->clk);

	return 0;
}
#endif

static const struct dev_pm_ops qspi_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(qspi_suspend, qspi_resume)
	SET_RUNTIME_PM_OPS(qspi_runtime_suspend, qspi_runtime_resume, NULL)
};

static struct platform_driver qspi_driver = {
	.probe = qspi_probe,
	.remove = __devexit_p(qspi_remove),
	.driver = {
		.name = "qspi",
		.owner = THIS_MODULE,
		.pm = &qspi_dev_pm_ops,
	},
};
module_platform_driver(qspi_driver);

MODULE_DESCRIPTION("qspi bus driver");
MODULE_AUTHOR("Renesas Electronics Corporation");
MODULE_LICENSE("GPL v2");
