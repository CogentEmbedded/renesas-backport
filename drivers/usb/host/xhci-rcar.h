/*
 * drivers/usb/host/xhci-rcar.h
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __XHCI_RCAR_H
#define __XHCI_RCAR_H

#define FIRMWARE_NAME			"UU3DRD1FW_2005L.dlmem"

/*** Register Offset ***/
/* Interrupt Enable */
#define AXI_INT_ENA			0x224

/* xHCI Firmware */
#define AXI_FW_VERSION_OFFSET		0x240
#define AXI_DL_CTRL			0x250
#define AXI_FW_DATA0			0x258

/* LCLK Select */
#define AXI_LCLK			0xA44

/* USB3.0 Configuration */
#define AXI_CONF1			0xA48
#define AXI_CONF2			0xA5C
#define AXI_CONF3			0xAA8

/* USB3.0 Polarity */
#define AXI_RX_POL			0xAB0
#define AXI_TX_POL			0xAB8

/*** Register Settings ***/
/* Interrupt Enable */
#define INT_XHC_ENA			0x00000001
#define INT_PME_ENA			0x00000002
#define INT_HSE_ENA			0x00000004

/* xHCI Firmware */
#define FW_VERSION			0x00200500
#define FW_LOAD_ENABLE			0x00000001
#define FW_SUCCESS			0x00000010
#define FW_SET_DATA0			0x00000100

/* LCLK Select */
#define LCLK_ENA			0x01030001

/* USB3.0 Configuraion */
#define CONF1_VAL			0x00030204
#define CONF2_VAL			0x00030300
#define CONF3_VAL			0x13802007

/* USB3.0 Polarity */
#define RX_POL_VAL			0x00020000
#define TX_POL_VAL			0x00000010

/* Read */
static inline u32 _readl(void *address)
{
	return __raw_readl(address);
}

/* Write */
static inline void _writel(void *address, u32 udata)
{
	__raw_writel(udata, address);
}

/* Set Bit */
static inline void _bitset(void *address, u32 udata)
{
	u32 reg_dt = __raw_readl(address) | (udata);
	__raw_writel(reg_dt, address);
}

/* Clear Bit */
static inline void _bitclr(void *address, u32 udata)
{
	u32 reg_dt = __raw_readl(address) & ~(udata);
	__raw_writel(reg_dt, address);
}

#endif /* __XHCI_RCAR_H */
