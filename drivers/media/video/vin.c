/*
 * drivers/media/video/vin.c
 *     V4L2 Driver for Video Input Unit interface
 *
 * Copyright (C) 2011-2013 Renesas Electronics Corporation
 *
 * This file is based on the drivers/media/video/sh_mobile_ceu_camera.c
 *
 * Copyright (C) 2008 Magnus Damm
 *
 * Based on V4L2 Driver for PXA camera host - "pxa_camera.c",
 *
 * Copyright (C) 2006, Sascha Hauer, Pengutronix
 * Copyright (C) 2008, Guennadi Liakhovetski <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/clk.h>

#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/soc_camera.h>
#include <media/vin.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-mediabus.h>
#include <media/soc_mediabus.h>

/* support to check initial value of VIN reg (0:Not support, 1:support)*/
#define VIN_SUPPORT_TO_CHECK_REGS 0
/* support to detect error interrupt (0:Disable, 1:Enable)*/
#define VIN_SUPPORT_ERR_INT 0


/* register offsets for VIN */
#define V0MC 0x0000
#define V0MS 0x0004
#define V0FC 0x0008
#define V0SLPrC 0x000C
#define V0ELPrC 0x0010
#define V0SPPrC 0x0014
#define V0EPPrC 0x0018
#define V0SLPoC 0x001C
#define V0ELPoC 0x0020
#define V0SPPoC 0x0024
#define V0EPPoC 0x0028
#define V0IS 0x002C
#define V0MB1 0x0030
#define V0MB2 0x0034
#define V0MB3 0x0038
#define V0LC 0x003C
#define V0IE 0x0040
#define V0INTS 0x0044
#define V0SI 0x0048
#define V0MTC 0x004C
#define V0YS 0x0050
#define V0XS 0x0054
#define V0DMR 0x0058
#define V0DMR2 0x005C
#define V0UVAOF 0x0060
#define V0CSCC1 0x0064
#define V0CSCC2 0x0068
#define V0CSCC3 0x006C
#define V0C1A 0x0080
#define V0C1B 0x0084
#define V0C1C 0x0088
#define V0C2A 0x0090
#define V0C2B 0x0094
#define V0C2C 0x0098
#define V0C3A 0x00A0
#define V0C3B 0x00A4
#define V0C3C 0x00A8
#define V0C4A 0x00B0
#define V0C4B 0x00B4
#define V0C4C 0x00B8
#define V0C5A 0x00C0
#define V0C5B 0x00C4
#define V0C5C 0x00C8
#define V0C6A 0x00D0
#define V0C6B 0x00D4
#define V0C6C 0x00D8
#define V0C7A 0x00E0
#define V0C7B 0x00E4
#define V0C7C 0x00E8
#define V0C8A 0x00F0
#define V0C8B 0x00F4
#define V0C8C 0x00F8

#define BUF_OFF		0x04
#define MB_NUM		3
#define SINGLE_BUF	0
#define MB_MASK		0x18
#define CONT_TRANS	4

#undef DEBUG_GEOMETRY
#ifdef DEBUG_GEOMETRY
#define dev_geo	dev_info
#else
#define dev_geo	dev_dbg
#endif

struct VIN_COEFF {
	unsigned short XS_Value;
	unsigned long CoeffSet[24];
};

static const struct VIN_COEFF VinCoeffSet[] = {
	{ 0x0000, {
		0x00000000,		0x00000000,		0x00000000,
		0x00000000,		0x00000000,		0x00000000,
		0x00000000,		0x00000000,		0x00000000,
		0x00000000,		0x00000000,		0x00000000,
		0x00000000,		0x00000000,		0x00000000,
		0x00000000,		0x00000000,		0x00000000,
		0x00000000,		0x00000000,		0x00000000,
		0x00000000,		0x00000000,		0x00000000 },
	},
	{ 0x1000, {
		0x000fa400,		0x000fa400,		0x09625902,
		0x000003f8,		0x00000403,		0x3de0d9f0,
		0x001fffed,		0x00000804,		0x3cc1f9c3,
		0x001003de,		0x00000c01,		0x3cb34d7f,
		0x002003d2,		0x00000c00,		0x3d24a92d,
		0x00200bca,		0x00000bff,		0x3df600d2,
		0x002013cc,		0x000007ff,		0x3ed70c7e,
		0x00100fde,		0x00000000,		0x3f87c036 },
	},
	{ 0x1200, {
		0x002ffff1,		0x002ffff1,		0x02a0a9c8,
		0x002003e7,		0x001ffffa,		0x000185bc,
		0x002007dc,		0x000003ff,		0x3e52859c,
		0x00200bd4,		0x00000002,		0x3d53996b,
		0x00100fd0,		0x00000403,		0x3d04ad2d,
		0x00000bd5,		0x00000403,		0x3d35ace7,
		0x3ff003e4,		0x00000801,		0x3dc674a1,
		0x3fffe800,		0x00000800,		0x3e76f461 },
	},
	{ 0x1400, {
		0x00100be3,		0x00100be3,		0x04d1359a,
		0x00000fdb,		0x002003ed,		0x0211fd93,
		0x00000fd6,		0x002003f4,		0x0002d97b,
		0x000007d6,		0x002ffffb,		0x3e93b956,
		0x3ff003da,		0x001003ff,		0x3db49926,
		0x3fffefe9,		0x00100001,		0x3d655cee,
		0x3fffd400,		0x00000003,		0x3d65f4b6,
		0x000fb421,		0x00000402,		0x3dc6547e },
	},
	{ 0x1600, {
		0x00000bdd,		0x00000bdd,		0x06519578,
		0x3ff007da,		0x00000be3,		0x03c24973,
		0x3ff003d9,		0x00000be9,		0x01b30d5f,
		0x3ffff7df,		0x001003f1,		0x0003c542,
		0x000fdfec,		0x001003f7,		0x3ec4711d,
		0x000fc400,		0x002ffffd,		0x3df504f1,
		0x001fa81a,		0x002ffc00,		0x3d957cc2,
		0x002f8c3c,		0x00100000,		0x3db5c891 },
	},
	{ 0x1800, {
		0x3ff003dc,		0x3ff003dc,		0x0791e558,
		0x000ff7dd,		0x3ff007de,		0x05328554,
		0x000fe7e3,		0x3ff00be2,		0x03232546,
		0x000fd7ee,		0x000007e9,		0x0143bd30,
		0x001fb800,		0x000007ee,		0x00044511,
		0x002fa015,		0x000007f4,		0x3ef4bcee,
		0x002f8832,		0x001003f9,		0x3e4514c7,
		0x001f7853,		0x001003fd,		0x3de54c9f },
	},
	{ 0x1a00, {
		0x000fefe0,		0x000fefe0,		0x08721d3c,
		0x001fdbe7,		0x000ffbde,		0x0652a139,
		0x001fcbf0,		0x000003df,		0x0463292e,
		0x002fb3ff,		0x3ff007e3,		0x0293a91d,
		0x002f9c12,		0x3ff00be7,		0x01241905,
		0x001f8c29,		0x000007ed,		0x3fe470eb,
		0x000f7c46,		0x000007f2,		0x3f04b8ca,
		0x3fef7865,		0x000007f6,		0x3e74e4a8 },
	},
	{ 0x1c00, {
		0x001fd3e9,		0x001fd3e9,		0x08f23d26,
		0x002fbff3,		0x001fe3e4,		0x0712ad23,
		0x002fa800,		0x000ff3e0,		0x05631d1b,
		0x001f9810,		0x000ffbe1,		0x03b3890d,
		0x000f8c23,		0x000003e3,		0x0233e8fa,
		0x3fef843b,		0x000003e7,		0x00f430e4,
		0x3fbf8456,		0x3ff00bea,		0x00046cc8,
		0x3f8f8c72,		0x3ff00bef,		0x3f3490ac },
	},
	{ 0x1e00, {
		0x001fbbf4,		0x001fbbf4,		0x09425112,
		0x001fa800,		0x002fc7ed,		0x0792b110,
		0x000f980e,		0x001fdbe6,		0x0613110a,
		0x3fff8c20,		0x001fe7e3,		0x04a368fd,
		0x3fcf8c33,		0x000ff7e2,		0x0343b8ed,
		0x3f9f8c4a,		0x000fffe3,		0x0203f8da,
		0x3f5f9c61,		0x000003e6,		0x00e428c5,
		0x3f1fb07b,		0x000003eb,		0x3fe440af },
	},
	{ 0x2000, {
		0x000fa400,		0x000fa400,		0x09625902,
		0x3fff980c,		0x001fb7f5,		0x0812b0ff,
		0x3fdf901c,		0x001fc7ed,		0x06b2fcfa,
		0x3faf902d,		0x001fd3e8,		0x055348f1,
		0x3f7f983f,		0x001fe3e5,		0x04038ce3,
		0x3f3fa454,		0x001fefe3,		0x02e3c8d1,
		0x3f0fb86a,		0x001ff7e4,		0x01c3e8c0,
		0x3ecfd880,		0x000fffe6,		0x00c404ac },
	},
	{ 0x2200, {
		0x3fdf9c0b,		0x3fdf9c0b,		0x09725cf4,
		0x3fbf9818,		0x3fffa400,		0x0842a8f1,
		0x3f8f9827,		0x000fb3f7,		0x0702f0ec,
		0x3f5fa037,		0x000fc3ef,		0x05d330e4,
		0x3f2fac49,		0x001fcfea,		0x04a364d9,
		0x3effc05c,		0x001fdbe7,		0x038394ca,
		0x3ecfdc6f,		0x001fe7e6,		0x0273b0bb,
		0x3ea00083,		0x001fefe6,		0x0183c0a9 },
	},
	{ 0x2400, {
		0x3f9fa014,		0x3f9fa014,		0x098260e6,
		0x3f7f9c23,		0x3fcf9c0a,		0x08629ce5,
		0x3f4fa431,		0x3fefa400,		0x0742d8e1,
		0x3f1fb440,		0x3fffb3f8,		0x062310d9,
		0x3eefc850,		0x000fbbf2,		0x050340d0,
		0x3ecfe062,		0x000fcbec,		0x041364c2,
		0x3ea00073,		0x001fd3ea,		0x03037cb5,
		0x3e902086,		0x001fdfe8,		0x022388a5 },
	},
	{ 0x2600, {
		0x3f5fa81e,		0x3f5fa81e,		0x096258da,
		0x3f3fac2b,		0x3f8fa412,		0x088290d8,
		0x3f0fbc38,		0x3fafa408,		0x0772c8d5,
		0x3eefcc47,		0x3fcfa800,		0x0672f4ce,
		0x3ecfe456,		0x3fefaffa,		0x05531cc6,
		0x3eb00066,		0x3fffbbf3,		0x047334bb,
		0x3ea01c77,		0x000fc7ee,		0x039348ae,
		0x3ea04486,		0x000fd3eb,		0x02b350a1 },
	},
	{ 0x2800, {
		0x3f2fb426,		0x3f2fb426,		0x094250ce,
		0x3f0fc032,		0x3f4fac1b,		0x086284cd,
		0x3eefd040,		0x3f7fa811,		0x0782acc9,
		0x3ecfe84c,		0x3f9fa807,		0x06a2d8c4,
		0x3eb0005b,		0x3fbfac00,		0x05b2f4bc,
		0x3eb0186a,		0x3fdfb3fa,		0x04c308b4,
		0x3eb04077,		0x3fefbbf4,		0x03f31ca8,
		0x3ec06884,		0x000fbff2,		0x03031c9e },
	},
	{ 0x2a00, {
		0x3f0fc42d,		0x3f0fc42d,		0x090240c4,
		0x3eefd439,		0x3f2fb822,		0x08526cc2,
		0x3edfe845,		0x3f4fb018,		0x078294bf,
		0x3ec00051,		0x3f6fac0f,		0x06b2b4bb,
		0x3ec0185f,		0x3f8fac07,		0x05e2ccb4,
		0x3ec0386b,		0x3fafac00,		0x0502e8ac,
		0x3ed05c77,		0x3fcfb3fb,		0x0432f0a3,
		0x3ef08482,		0x3fdfbbf6,		0x0372f898 },
	},
	{ 0x2c00, {
		0x3eefdc31,		0x3eefdc31,		0x08e238b8,
		0x3edfec3d,		0x3f0fc828,		0x082258b9,
		0x3ed00049,		0x3f1fc01e,		0x077278b6,
		0x3ed01455,		0x3f3fb815,		0x06c294b2,
		0x3ed03460,		0x3f5fb40d,		0x0602acac,
		0x3ef0506c,		0x3f7fb006,		0x0542c0a4,
		0x3f107476,		0x3f9fb400,		0x0472c89d,
		0x3f309c80,		0x3fbfb7fc,		0x03b2cc94 },
	},
	{ 0x2e00, {
		0x3eefec37,		0x3eefec37,		0x088220b0,
		0x3ee00041,		0x3effdc2d,		0x07f244ae,
		0x3ee0144c,		0x3f0fd023,		0x07625cad,
		0x3ef02c57,		0x3f1fc81a,		0x06c274a9,
		0x3f004861,		0x3f3fbc13,		0x060288a6,
		0x3f20686b,		0x3f5fb80c,		0x05529c9e,
		0x3f408c74,		0x3f6fb805,		0x04b2ac96,
		0x3f80ac7e,		0x3f8fb800,		0x0402ac8e },
	},
	{ 0x3000, {
		0x3ef0003a,		0x3ef0003a,		0x084210a6,
		0x3ef01045,		0x3effec32,		0x07b228a7,
		0x3f00284e,		0x3f0fdc29,		0x073244a4,
		0x3f104058,		0x3f0fd420,		0x06a258a2,
		0x3f305c62,		0x3f2fc818,		0x0612689d,
		0x3f508069,		0x3f3fc011,		0x05728496,
		0x3f80a072,		0x3f4fc00a,		0x04d28c90,
		0x3fc0c07b,		0x3f6fbc04,		0x04429088 },
	},
	{ 0x3200, {
		0x3f00103e,		0x3f00103e,		0x07f1fc9e,
		0x3f102447,		0x3f000035,		0x0782149d,
		0x3f203c4f,		0x3f0ff02c,		0x07122c9c,
		0x3f405458,		0x3f0fe424,		0x06924099,
		0x3f607061,		0x3f1fd41d,		0x06024c97,
		0x3f909068,		0x3f2fcc16,		0x05726490,
		0x3fc0b070,		0x3f3fc80f,		0x04f26c8a,
		0x0000d077,		0x3f4fc409,		0x04627484 },
	},
	{ 0x3400, {
		0x3f202040,		0x3f202040,		0x07a1e898,
		0x3f303449,		0x3f100c38,		0x0741fc98,
		0x3f504c50,		0x3f10002f,		0x06e21495,
		0x3f706459,		0x3f1ff028,		0x06722492,
		0x3fa08060,		0x3f1fe421,		0x05f2348f,
		0x3fd09c67,		0x3f1fdc19,		0x05824c89,
		0x0000bc6e,		0x3f2fd014,		0x04f25086,
		0x0040dc74,		0x3f3fcc0d,		0x04825c7f },
	},
	{ 0x3600, {
		0x3f403042,		0x3f403042,		0x0761d890,
		0x3f504848,		0x3f301c3b,		0x0701f090,
		0x3f805c50,		0x3f200c33,		0x06a2008f,
		0x3fa07458,		0x3f10002b,		0x06520c8d,
		0x3fd0905e,		0x3f1ff424,		0x05e22089,
		0x0000ac65,		0x3f1fe81d,		0x05823483,
		0x0030cc6a,		0x3f2fdc18,		0x04f23c81,
		0x0080e871,		0x3f2fd412,		0x0482407c },
	},
	{ 0x3800, {
		0x3f604043,		0x3f604043,		0x0721c88a,
		0x3f80544a,		0x3f502c3c,		0x06d1d88a,
		0x3fb06851,		0x3f301c35,		0x0681e889,
		0x3fd08456,		0x3f30082f,		0x0611fc88,
		0x00009c5d,		0x3f200027,		0x05d20884,
		0x0030b863,		0x3f2ff421,		0x05621880,
		0x0070d468,		0x3f2fe81b,		0x0502247c,
		0x00c0ec6f,		0x3f2fe015,		0x04a22877 },
	},
	{ 0x3a00, {
		0x3f904c44,		0x3f904c44,		0x06e1b884,
		0x3fb0604a,		0x3f70383e,		0x0691c885,
		0x3fe07451,		0x3f502c36,		0x0661d483,
		0x00009055,		0x3f401831,		0x0601ec81,
		0x0030a85b,		0x3f300c2a,		0x05b1f480,
		0x0070c061,		0x3f300024,		0x0562047a,
		0x00b0d867,		0x3f3ff41e,		0x05020c77,
		0x00f0f46b,		0x3f2fec19,		0x04a21474 },
	},
	{ 0x3c00, {
		0x3fb05c43,		0x3fb05c43,		0x06c1b07e,
		0x3fe06c4b,		0x3f902c3f,		0x0681c081,
		0x0000844f,		0x3f703838,		0x0631cc7d,
		0x00309855,		0x3f602433,		0x05d1d47e,
		0x0060b459,		0x3f50142e,		0x0581e47b,
		0x00a0c85f,		0x3f400828,		0x0531f078,
		0x00e0e064,		0x3f300021,		0x0501fc73,
		0x00b0fc6a,		0x3f3ff41d,		0x04a20873 },
	},
	{ 0x3e00, {
		0x3fe06444,		0x3fe06444,		0x0681a07a,
		0x00007849,		0x3fc0503f,		0x0641b07a,
		0x0020904d,		0x3fa0403a,		0x05f1c07a,
		0x0060a453,		0x3f803034,		0x05c1c878,
		0x0090b858,		0x3f70202f,		0x0571d477,
		0x00d0d05d,		0x3f501829,		0x0531e073,
		0x0110e462,		0x3f500825,		0x04e1e471,
		0x01510065,		0x3f40001f,		0x04a1f06d },
	},
	{ 0x4000, {
		0x00007044,		0x00007044,		0x06519476,
		0x00208448,		0x3fe05c3f,		0x0621a476,
		0x0050984d,		0x3fc04c3a,		0x05e1b075,
		0x0080ac52,		0x3fa03c35,		0x05a1b875,
		0x00c0c056,		0x3f803030,		0x0561c473,
		0x0100d45b,		0x3f70202b,		0x0521d46f,
		0x0140e860,		0x3f601427,		0x04d1d46e,
		0x01810064,		0x3f500822,		0x0491dc6b },
	},
	{ 0x5000, {
		0x0110a442,		0x0110a442,		0x0551545e,
		0x0140b045,		0x00e0983f,		0x0531585f,
		0x0160c047,		0x00c08c3c,		0x0511645e,
		0x0190cc4a,		0x00908039,		0x04f1685f,
		0x01c0dc4c,		0x00707436,		0x04d1705e,
		0x0200e850,		0x00506833,		0x04b1785b,
		0x0230f453,		0x00305c30,		0x0491805a,
		0x02710056,		0x0010542d,		0x04718059 },
	},
	{ 0x6000, {
		0x01c0bc40,		0x01c0bc40,		0x04c13052,
		0x01e0c841,		0x01a0b43d,		0x04c13851,
		0x0210cc44,		0x0180a83c,		0x04a13453,
		0x0230d845,		0x0160a03a,		0x04913c52,
		0x0260e047,		0x01409838,		0x04714052,
		0x0280ec49,		0x01208c37,		0x04514c50,
		0x02b0f44b,		0x01008435,		0x04414c50,
		0x02d1004c,		0x00e07c33,		0x0431544f },
	},
	{ 0x7000, {
		0x0230c83e,		0x0230c83e,		0x04711c4c,
		0x0250d03f,		0x0210c43c,		0x0471204b,
		0x0270d840,		0x0200b83c,		0x0451244b,
		0x0290dc42,		0x01e0b43a,		0x0441244c,
		0x02b0e443,		0x01c0b038,		0x0441284b,
		0x02d0ec44,		0x01b0a438,		0x0421304a,
		0x02f0f445,		0x0190a036,		0x04213449,
		0x0310f847,		0x01709c34,		0x04213848 },
	},
	{ 0x8000, {
		0x0280d03d,		0x0280d03d,		0x04310c48,
		0x02a0d43e,		0x0270c83c,		0x04311047,
		0x02b0dc3e,		0x0250c83a,		0x04311447,
		0x02d0e040,		0x0240c03a,		0x04211446,
		0x02e0e840,		0x0220bc39,		0x04111847,
		0x0300e842,		0x0210b438,		0x04012445,
		0x0310f043,		0x0200b037,		0x04012045,
		0x0330f444,		0x01e0ac36,		0x03f12445 },
	},
	{ 0xefff, {
		0x0340dc3a,		0x0340dc3a,		0x03b0ec40,
		0x0340e03a,		0x0330e039,		0x03c0f03e,
		0x0350e03b,		0x0330dc39,		0x03c0ec3e,
		0x0350e43a,		0x0320dc38,		0x03c0f43e,
		0x0360e43b,		0x0320d839,		0x03b0f03e,
		0x0360e83b,		0x0310d838,		0x03c0fc3b,
		0x0370e83b,		0x0310d439,		0x03a0f83d,
		0x0370e83c,		0x0300d438,		0x03b0fc3c },
	}
};

#define VIN_COEFF_SET_COUNT (sizeof(VinCoeffSet) / sizeof(struct VIN_COEFF))

enum vin_capture_status {
	STOPPED,
	RUNNING,
	STOPPING,
};

/* per video frame buffer */
struct vin_buffer {
	struct vb2_buffer vb; /* v4l buffer must be first */
	struct list_head queue;
};

struct vin_dev {
	struct soc_camera_host ici;
	struct soc_camera_device *icd;

	unsigned int irq;
	void __iomem *base;
	size_t video_limit;

	spinlock_t lock;		/* Protects video buffer lists */
	struct list_head capture;
	struct vb2_alloc_ctx *alloc_ctx;

	struct vin_info *pdata;

	enum v4l2_field field;
	int sequence;

	struct clk *vinclk;

	struct vb2_buffer *queue_buf[MB_NUM];
	unsigned int mb_cnt;
	unsigned int vb_count;
	unsigned int set_pos;
	unsigned int get_pos;
	enum vin_capture_status capture_status;
	unsigned int request_to_stop;
	struct completion capture_stop;
};

struct vin_cam {
	/* VIN offsets within scaled by the VIN camera output */
	unsigned int vin_left;
	unsigned int vin_top;
	/* Client output, as seen by the VIN */
	unsigned int width;
	unsigned int height;
	/* User window from S_FMT */
	unsigned int out_width;
	unsigned int out_height;
	/*
	 * User window from S_CROP / G_CROP, produced by client cropping and
	 * scaling, VIN scaling and VIN cropping, mapped back onto the client
	 * input window
	 */
	struct v4l2_rect subrect;
	/* Camera cropping rectangle */
	struct v4l2_rect rect;
	const struct soc_mbus_pixelfmt *extra_fmt;
	enum v4l2_mbus_pixelcode code;
};

static struct vin_buffer *to_vin_vb(struct vb2_buffer *vb)
{
	return container_of(vb, struct vin_buffer, vb);
}

static struct soc_camera_device *q_to_icd(struct vb2_queue *q)
{
	return container_of(q, struct soc_camera_device, vb2_vidq);
}

static void vin_write(struct vin_dev *priv,
		      unsigned long reg_offs, u32 data)
{
	iowrite32(data, priv->base + reg_offs);
}

static u32 vin_read(struct vin_dev *priv, unsigned long reg_offs)
{
	return ioread32(priv->base + reg_offs);
}

static int vin_soft_reset(struct vin_dev *pcdev)
{
	/* clear V0INTS  */
	vin_write(pcdev, V0INTS, 0);

	return 0;
}

static u32 vin_get_status(struct vin_dev *priv)
{
	u32 status = 0;

	status = vin_read(priv, V0MS);

	return status;
}

static u32 vin_is_active(struct vin_dev *priv)
{
	u32 status = 0;

	status = vin_read(priv, V0MS) & 0x01;

	return status;
}

/*
 *  Videobuf operations
 */

/*
 * .queue_setup() is called to check, whether the driver can accept the
 *		  requested number of buffers and to fill in plane sizes
 *		  for the current frame format if required
 */
static int vin_videobuf_setup(struct vb2_queue *vq,
			const struct v4l2_format *fmt,
			unsigned int *count, unsigned int *num_planes,
			unsigned int sizes[], void *alloc_ctxs[])
{
	struct soc_camera_device *icd = q_to_icd(vq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct vin_dev *pcdev = ici->priv;
	int bytes_per_line;
	unsigned int height;

	if (fmt) {
		const struct soc_camera_format_xlate *xlate;
		xlate = soc_camera_xlate_by_fourcc(icd,
						fmt->fmt.pix.pixelformat);
		if (!xlate)
			return -EINVAL;
		bytes_per_line = soc_mbus_bytes_per_line(fmt->fmt.pix.width,
							 xlate->host_fmt);
		height = fmt->fmt.pix.height;
	} else {
		/* Called from VIDIOC_REQBUFS or in compatibility mode */
		bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);
		height = icd->user_height;
	}
	if (bytes_per_line < 0)
		return bytes_per_line;

	sizes[0] = bytes_per_line * height;

	alloc_ctxs[0] = pcdev->alloc_ctx;

	if (!vq->num_buffers)
		pcdev->sequence = 0;

	if (!*count)
		*count = 2;

	/* If *num_planes != 0, we have already verified *count. */
	if (pcdev->video_limit && !*num_planes) {
		size_t size = PAGE_ALIGN(sizes[0]) * *count;

		if (size > pcdev->video_limit)
			*count = pcdev->video_limit / PAGE_ALIGN(sizes[0]);
	}

	*num_planes = 1;

	pcdev->vb_count = *count;

	dev_dbg(icd->parent, "count=%d, size=%u\n", *count, sizes[0]);

	return 0;
}

#define VIN_VNIE_FIE2  (1 << 31) /* Field Interrupt Enable 2 */
#define VIN_VNIE_VFE   (1 << 17)
				/* Vsync Falling edge detect interrupt Enable */
#define VIN_VNIE_VRE   (1 << 16)
				/* Vsync Rising edge detect interrupt Enable */
#define VIN_VNIE_FIE   (1 << 4)  /* Field Interrupt Enable */
#define VIN_VNIE_CEE   (1 << 3)  /* Correct Error interrupt Enable */
#define VIN_VNIE_SIE   (1 << 2)  /* Scanline Interrupt Enable */
#define VIN_VNIE_EFE   (1 << 1)  /* End of Frame interrupt Enable */
#define VIN_VNIE_FOE   (1 << 0)  /* Fifo Over flow interrupt Enable */

#define VIN_VNIE_MASK        (VIN_VNIE_EFE)
#define VIN_VNIE_ERROR_MASK  (VIN_VNIE_CEE | VIN_VNIE_FOE)

/* VnMC */
#define VIN_VNMC_FOC         (0x00200000) /* Field Order Control */
#define VIN_VNMC_YCAL        (0x00080000) /* YCbCr-422 input data ALignment */
#define VIN_VNMC_VUP         (0x00000400) /* Vin register UPdate control */

#define VIN_VNMC_IM_MASK     (0x00000018) /* Interlace Mode */
#define VIN_VNMC_IM_ODD      (0x00000000)
#define VIN_VNMC_IM_ODD_EVEN (0x00000008)
#define VIN_VNMC_IM_EVEN     (0x00000010)
#define VIN_VNMC_IM_FULL     (0x00000018)

#define VIN_VNMC_INF_BT656_8   (0x00000000)
#define VIN_VNMC_INF_BT709_24  (0x00060000)

#define VIN_VNMC_FIELD_MASK  (VIN_VNMC_FOC | VIN_VNMC_IM_MASK)

#define VIN_VNMC_BPS         (0x00000002)
			/* ycbcr-422 -> ycbcr-422 convert ByPaSs mode*/
#define VIN_VNMC_ME          (0x00000001) /* Module Enable */

/* VnMS */
#define VIN_VNMS_FBS         (0x00000018)  /* Frame Buffer Status */
#define VIN_VNMS_FS          (0x00000004)  /* Field Status */
#define VIN_VNMS_AV          (0x00000002)  /* Active Video status */
#define VIN_VNMS_CA          (0x00000001)  /* video Capture Active Status */

/* VnFC */
#define VIN_VNFC_C_FRAME     (0x00000002)  /* Continuous frame Capture mode */
#define VIN_VNFC_S_FRAME     (0x00000001)  /* Single frame Capture mode */

/* VnDMR */
#define VIN_VNDMR_EVA        (0x00010000)  /* Even field Address offset */
#define VIN_VNDMR_BPSM       (0x00000010)  /* Byte Position Swap Mode */
#define VIN_VNDMR_DTMD_YCSEP    (0x00000002)  /* transfer: YC separate */
#define VIN_VNDMR_DTMD_ARGB1555 (0x00000001)  /* transfer: ARGB1555 */

/* VnDMR2 */
#define VIN_VNDMR2_FPS             (0x80000000)  /* Field Polarity Select */
#define VIN_VNDMR2_VPS             (0x40000000)  /* Vsync Polarity Select */
#define VIN_VNDMR2_VPS_ACTIVE_LOW  (0x00000000)
#define VIN_VNDMR2_VPS_ACTIVE_HIGH (VIN_VNDMR2_VPS)
#define VIN_VNDMR2_HPS             (0x20000000)  /* Hsync Polarity Select */
#define VIN_VNDMR2_HPS_ACTIVE_LOW  (0x00000000)
#define VIN_VNDMR2_HPS_ACTIVE_HIGH (VIN_VNDMR2_HPS)
#define VIN_VNDMR2_CES             (0x10000000)
		/* Clock Enable polarity Select */
#define VIN_VNDMR2_FTEV            (0x00020000)
		/* Field Toggle Enable of Vsync */
#define VIN_VNDMR2_VLV_1           (0x00001000)
		/* FVSYNC Field Toggle Mode Transition Period */

static inline bool is_continuous_transfer(struct vin_dev *pcdev)
{
	return (pcdev->vb_count >= CONT_TRANS);
}

/*
 * return value doesn't reflex the success/failure to queue the new buffer,
 * but rather the status of the previous buffer.
 */
static int vin_capture(struct vin_dev *pcdev)
{
	struct soc_camera_device *icd = pcdev->icd;
	struct vin_cam *cam = icd->host_priv;
	u32 status_of_int;
	u32 mc, dmr;
	int ret = 0;

	status_of_int = vin_read(pcdev, V0INTS);

	/* clear interrupt */
	vin_write(pcdev, V0INTS, status_of_int);

#if VIN_SUPPORT_ERR_INT
	/*
	 * When a CEE or FOE interrupt occurs, a capture end interrupt does not
	 * occur and the image of that frame is not captured correctly. So, soft
	 * reset is needed here.
	 */
	if (status_of_int & VIN_VNIE_ERROR_MASK) {
		vin_soft_reset(pcdev);
		ret = -EIO;
	}
#endif /* VIN_SUPPORT_ERR_INT */

	/* disable interrupt */
	vin_write(pcdev, V0IE, 0x00000000);

	/* set priority for memory transfer */
	vin_write(pcdev, V0MTC, 0x0a090008);

	switch (pcdev->field) {
	case V4L2_FIELD_TOP:
		mc = VIN_VNMC_IM_ODD;
		break;
	case V4L2_FIELD_BOTTOM:
		mc = VIN_VNMC_IM_EVEN;
		break;
	case V4L2_FIELD_INTERLACED:
	case V4L2_FIELD_INTERLACED_TB:
		mc = VIN_VNMC_IM_FULL;
		break;
	case V4L2_FIELD_INTERLACED_BT:
		mc = VIN_VNMC_IM_FULL | VIN_VNMC_FOC;
		break;
	default:
		mc = VIN_VNMC_IM_ODD;
		break;
	}

	/* start capture */
	switch (icd->current_fmt->host_fmt->fourcc) {
	case V4L2_PIX_FMT_YUYV:
		dmr = VIN_VNDMR_BPSM;
		mc |= VIN_VNMC_VUP | VIN_VNMC_BPS;
		break;
	case V4L2_PIX_FMT_UYVY:
		dmr = 0;
		mc |= VIN_VNMC_VUP | VIN_VNMC_BPS;
		break;
	case V4L2_PIX_FMT_RGB555X:
		dmr = VIN_VNDMR_DTMD_ARGB1555;
		mc |= VIN_VNMC_VUP;
		break;
	case V4L2_PIX_FMT_RGB565:
		dmr = 0;
		mc |= VIN_VNMC_VUP;
		break;
	case V4L2_PIX_FMT_NV16:
		vin_write(pcdev, V0UVAOF,
			  ((cam->width * cam->height) + 0x7f) & ~0x7f);
		dmr = VIN_VNDMR_DTMD_YCSEP;
		mc |= VIN_VNMC_VUP | VIN_VNMC_BPS;
		break;
	default:
	printk(KERN_ALERT "<WARNNING msg=\"Invalid fourcc\" fourcc=\"0x%x\"/>\n",
		icd->current_fmt->host_fmt->fourcc);
		dmr = vin_read(pcdev, V0DMR);
		mc = vin_read(pcdev, V0MC);
		break;
	}

	switch (pcdev->pdata->input) {
	case VIN_INPUT_ITUR_BT656_8BIT:
		mc |= VIN_VNMC_INF_BT656_8;
		break;
	case VIN_INPUT_ITUR_BT709_24BIT:
		mc |= VIN_VNMC_INF_BT709_24;
		mc ^= VIN_VNMC_BPS;
		break;
	}

	/* enable interrupt */
	vin_write(pcdev, V0IE, VIN_VNIE_MASK);

	/* start capturing */
	vin_write(pcdev, V0DMR, dmr);
	vin_write(pcdev, V0MC, mc | VIN_VNMC_ME);

	if (is_continuous_transfer(pcdev)) {
		/* continuous transfer ON */
		vin_write(pcdev, V0FC, VIN_VNFC_C_FRAME);
	} else {
		/* single transfer ON */
		vin_write(pcdev, V0FC, VIN_VNFC_S_FRAME);
	}

	return ret;
}

static void vin_deinit_capture(struct vin_dev *pcdev)
{
	/* continuous & single transfer OFF */
	vin_write(pcdev, V0FC, 0);

	/* disable capture (release DMA buffer), reset */
	vin_write(pcdev, V0MC, vin_read(pcdev, V0MC) & ~VIN_VNMC_ME);

	/* update the status if stopped already */
	if ((vin_read(pcdev, V0MS) & VIN_VNMS_CA) == 0)
		pcdev->capture_status = STOPPED;
}

static int vin_videobuf_prepare(struct vb2_buffer *vb)
{
	struct vin_buffer *buf = to_vin_vb(vb);

	/* Added list head initialization on alloc */
	WARN(!list_empty(&buf->queue), "Buffer %p on queue!\n", vb);

	return 0;
}

static void vin_videobuf_queue(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = q_to_icd(vb->vb2_queue);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct vin_dev *pcdev = ici->priv;
	struct vin_buffer *buf = to_vin_vb(vb);
	unsigned long size;
	unsigned long flags;
	int bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);
	dma_addr_t phys_addr_top;
	int n_slots;

	if (bytes_per_line < 0)
		goto error;

	size = icd->user_height * bytes_per_line;

	if (vb2_plane_size(vb, 0) < size) {
		dev_err(icd->parent, "Buffer #%d too small (%lu < %lu)\n",
			vb->v4l2_buf.index, vb2_plane_size(vb, 0), size);
		goto error;
	}

	vb2_set_plane_payload(vb, 0, size);

	dev_dbg(icd->parent, "%s (vb=0x%p) 0x%p %lu\n", __func__,
		vb, vb2_plane_vaddr(vb, 0), vb2_get_plane_payload(vb, 0));

#ifdef DEBUG
	/*
	 * This can be useful if you want to see if we actually fill
	 * the buffer with something
	 */
	if (vb2_plane_vaddr(vb, 0))
		memset(vb2_plane_vaddr(vb, 0), 0xaa,
				vb2_get_plane_payload(vb, 0));
#endif

	spin_lock_irqsave(&pcdev->lock, flags);

	n_slots = is_continuous_transfer(pcdev) ? MB_NUM : 1;
	if (pcdev->mb_cnt >= n_slots) { /* add queue */
		vb->state = VIDEOBUF_QUEUED;
		list_add_tail(&buf->queue, &pcdev->capture);
	} else {
		int slot = (pcdev->set_pos + 1) % n_slots;

		phys_addr_top = vb2_dma_contig_plane_dma_addr(vb, 0);
		vin_write(pcdev, (V0MB1 + (BUF_OFF * slot)),
			 phys_addr_top);
		pcdev->queue_buf[slot] = vb;
		pcdev->queue_buf[slot]->state = VIDEOBUF_ACTIVE;
		pcdev->set_pos = slot;
		pcdev->mb_cnt++;
	}

	if ((pcdev->capture_status != RUNNING) &&
	    (pcdev->mb_cnt >= n_slots)) {
		pcdev->request_to_stop = 0;
		init_completion(&pcdev->capture_stop);
		pcdev->capture_status = RUNNING;
		vin_capture(pcdev);
	}

	spin_unlock_irqrestore(&pcdev->lock, flags);

	return;

error:
	vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
}

static void vin_videobuf_release(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = q_to_icd(vb->vb2_queue);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct vin_dev *pcdev = ici->priv;
	struct vin_buffer *buf = to_vin_vb(vb);
	unsigned long flags, i;
	int n_slots;
	int buf_in_use = 0;

	spin_lock_irqsave(&pcdev->lock, flags);

	/* Is the buffer is use by the VIN hardware? */
	for (i = 0; i < MB_NUM; i++) {
		if (pcdev->queue_buf[i] == vb) {
			buf_in_use = 1;
			break;
		}
	}

	if (buf_in_use) {
		while (pcdev->capture_status != STOPPED) {
			pcdev->request_to_stop = 1;

			/* issue stop if running */
			if (pcdev->capture_status == RUNNING) {
				pcdev->capture_status = STOPPING;
				vin_deinit_capture(pcdev);
			}

			/* wait until capturing has been stopped */
			if (pcdev->capture_status == STOPPING) {
				spin_unlock_irqrestore(&pcdev->lock, flags);
				wait_for_completion(&pcdev->capture_stop);
				spin_lock_irqsave(&pcdev->lock, flags);
			}
		}

		if (is_continuous_transfer(pcdev))
			n_slots = MB_NUM;
		else
			n_slots = 1;

		for (i = 0; i < MB_NUM; i++) {
			if (pcdev->queue_buf[i] == vb) {
				vb2_buffer_done(pcdev->queue_buf[i],
					VB2_BUF_STATE_ERROR);
				pcdev->mb_cnt--;
				/* decrement set_pos  */
				pcdev->set_pos = (pcdev->set_pos +
						(n_slots - 1)) % n_slots;
				pcdev->queue_buf[i] = NULL;
				break;
			}
		}
	} else {
		list_del_init(&buf->queue);
	}

	spin_unlock_irqrestore(&pcdev->lock, flags);
}

static int vin_videobuf_init(struct vb2_buffer *vb)
{
	/* This is for locking debugging only */
	INIT_LIST_HEAD(&to_vin_vb(vb)->queue);
	return 0;
}

static int vin_stop_streaming(struct vb2_queue *q)
{
	struct soc_camera_device *icd = q_to_icd(q);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct vin_dev *pcdev = ici->priv;
	struct list_head *buf_head, *tmp;

	spin_lock_irq(&pcdev->lock);

	list_for_each_safe(buf_head, tmp, &pcdev->capture)
		list_del_init(buf_head);

	spin_unlock_irq(&pcdev->lock);

	return vin_soft_reset(pcdev);
}

static struct vb2_ops vin_videobuf_ops = {
	.queue_setup	= vin_videobuf_setup,
	.buf_prepare	= vin_videobuf_prepare,
	.buf_queue		= vin_videobuf_queue,
	.buf_cleanup	= vin_videobuf_release,
	.buf_init		= vin_videobuf_init,
	.wait_prepare	= soc_camera_unlock,
	.wait_finish	= soc_camera_lock,
	.stop_streaming	= vin_stop_streaming,
};

static irqreturn_t vin_irq(int irq, void *data)
{
	struct vin_dev *pcdev = data;
	unsigned long flags;
	u32 status_of_int, ms_fbs;
	dma_addr_t phys_addr_top;
	struct vb2_buffer *queue_vb, *next_queue_vb;
	bool empty = 0, start = 0, stopped;
	int n_slots;

	spin_lock_irqsave(&pcdev->lock, flags);

	/* clear interrupt */
	status_of_int = vin_read(pcdev, V0INTS);
	vin_write(pcdev, V0INTS, status_of_int);

	/* nothing to do if capture status is 'STOPPED'. */
	if (pcdev->capture_status == STOPPED)
		goto done;

	stopped = ((vin_read(pcdev, V0MS) & VIN_VNMS_CA) == 0);

	if (pcdev->request_to_stop == 0) {
		if (is_continuous_transfer(pcdev)) {
			ms_fbs = (vin_get_status(pcdev) & MB_MASK) >> 3;
			n_slots = MB_NUM;
		} else {
			ms_fbs = 3;
			n_slots = 1;
			goto single;
		}

		/* wait until get_pos and mb_status become equal */
		while (ms_fbs < 3 && pcdev->get_pos != ms_fbs) {
single:
			pcdev->get_pos = (pcdev->get_pos + 1) % n_slots;
			queue_vb = pcdev->queue_buf[pcdev->get_pos];
			queue_vb->v4l2_buf.sequence = pcdev->sequence++;
			do_gettimeofday(&queue_vb->v4l2_buf.timestamp);
			vb2_buffer_done(queue_vb, VB2_BUF_STATE_DONE);
			pcdev->mb_cnt--;
			pcdev->queue_buf[pcdev->get_pos] = NULL;

			if (pcdev->capture_status == STOPPING)
				continue;
			/* set next frame addr */
			if (list_empty(&pcdev->capture)) {
				empty = 1;
			} else {
				int slot = (pcdev->set_pos + 1) % n_slots;
				next_queue_vb = &list_entry(pcdev->capture.next,
					struct vin_buffer, queue)->vb;
				list_del_init(&to_vin_vb(next_queue_vb)->queue);
				pcdev->queue_buf[slot] = next_queue_vb;
				pcdev->queue_buf[slot]->state =
					VIDEOBUF_ACTIVE;
				phys_addr_top =
					 vb2_dma_contig_plane_dma_addr(
						next_queue_vb, 0);
				vin_write(pcdev, V0MB1 + (BUF_OFF * slot),
					  phys_addr_top);
				pcdev->set_pos = slot;
				pcdev->mb_cnt++;
				start = 1;
			}
		}

		if (stopped)
			pcdev->capture_status = STOPPED;

		if (empty && pcdev->capture_status == RUNNING) {
			/* stop continuous transfer */
			pcdev->capture_status = STOPPING;
			vin_deinit_capture(pcdev);
		} else if (start && pcdev->capture_status == STOPPED) {
			/* start single transfer */
			pcdev->capture_status = RUNNING;
			vin_capture(pcdev);
		}
	} else if (stopped) {
		pcdev->capture_status = STOPPED;
		pcdev->request_to_stop = 0;
		complete(&pcdev->capture_stop);
	}

done:
	spin_unlock_irqrestore(&pcdev->lock, flags);

	return IRQ_HANDLED;
}

/* Called with .video_lock held */
static int vin_add_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct vin_dev *pcdev = ici->priv;
	int ret;
	u32 ms_fbs;

	if (pcdev->icd)
		return -EBUSY;

	dev_info(icd->parent,
		 "VIN Unit driver attached to camera %d\n",
		 icd->devnum);

#ifdef CONFIG_PM
	pm_runtime_get_sync(ici->v4l2_dev.dev);
#endif /* CONFIG_PM */

	/* adjust get_pos and set_pos
	   to the next of the last terminated position. */
	ms_fbs = (vin_get_status(pcdev) & MB_MASK) >> 3;
	pcdev->set_pos = pcdev->get_pos = (ms_fbs >= 2) ? 2 : ms_fbs;

	ret = vin_soft_reset(pcdev);
	if (!ret)
		pcdev->icd = icd;

	return ret;
}

/* Called with .video_lock held */
static void vin_remove_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct vin_dev *pcdev = ici->priv;
	unsigned long flags, i;

	BUG_ON(icd != pcdev->icd);

	/* disable capture, disable interrupts */
	vin_write(pcdev, V0MC, vin_read(pcdev, V0MC) & ~VIN_VNMC_ME);
	vin_write(pcdev, V0IE, 0x00000000);

	vin_soft_reset(pcdev);

	pcdev->capture_status = STOPPED;

	/* make sure active buffer is canceled */
	spin_lock_irqsave(&pcdev->lock, flags);

	for (i = 0; i < MB_NUM; i++) {
		if (pcdev->queue_buf[i]) {
			list_del_init(&to_vin_vb(pcdev->queue_buf[i])->queue);
			vb2_buffer_done(pcdev->queue_buf[i],
				VB2_BUF_STATE_ERROR);
			pcdev->queue_buf[i] = NULL;
		}
	}

	/* reset mb_cnt since all of the buffers were released. */
	pcdev->mb_cnt = 0;

	spin_unlock_irqrestore(&pcdev->lock, flags);

#ifdef CONFIG_PM
	pm_runtime_put_sync(ici->v4l2_dev.dev);
#endif /* CONFIG_PM */

	dev_info(icd->parent,
		 "VIN Unit driver detached from camera %d\n",
		 icd->devnum);

	pcdev->icd = NULL;
}

static void setCoeff(struct vin_dev *pcdev, unsigned long xs)
{
	int i;
	struct VIN_COEFF *pPrevSet = NULL;
	struct VIN_COEFF *pSet = NULL;

	/* Search the correspondence coefficient values */
	for (i = 0; i < VIN_COEFF_SET_COUNT; i++) {
		pPrevSet = pSet;
		pSet = (struct VIN_COEFF *) &VinCoeffSet[i];

		if (xs < pSet->XS_Value)
			break;
	}

	/* Use previous value if it's XS value is closer */
	if (pPrevSet != NULL && pSet != NULL) {
		if ((xs - pPrevSet->XS_Value) < (pSet->XS_Value - xs))
			pSet = pPrevSet;
	}

	/* Set coefficient registers */
	vin_write(pcdev, V0C1A, pSet->CoeffSet[0]);
	vin_write(pcdev, V0C1B, pSet->CoeffSet[1]);
	vin_write(pcdev, V0C1C, pSet->CoeffSet[2]);

	vin_write(pcdev, V0C2A, pSet->CoeffSet[3]);
	vin_write(pcdev, V0C2B, pSet->CoeffSet[4]);
	vin_write(pcdev, V0C2C, pSet->CoeffSet[5]);

	vin_write(pcdev, V0C3A, pSet->CoeffSet[6]);
	vin_write(pcdev, V0C3B, pSet->CoeffSet[7]);
	vin_write(pcdev, V0C3C, pSet->CoeffSet[8]);

	vin_write(pcdev, V0C4A, pSet->CoeffSet[9]);
	vin_write(pcdev, V0C4B, pSet->CoeffSet[10]);
	vin_write(pcdev, V0C4C, pSet->CoeffSet[11]);

	vin_write(pcdev, V0C5A, pSet->CoeffSet[12]);
	vin_write(pcdev, V0C5B, pSet->CoeffSet[13]);
	vin_write(pcdev, V0C5C, pSet->CoeffSet[14]);

	vin_write(pcdev, V0C6A, pSet->CoeffSet[15]);
	vin_write(pcdev, V0C6B, pSet->CoeffSet[16]);
	vin_write(pcdev, V0C6C, pSet->CoeffSet[17]);

	vin_write(pcdev, V0C7A, pSet->CoeffSet[18]);
	vin_write(pcdev, V0C7B, pSet->CoeffSet[19]);
	vin_write(pcdev, V0C7C, pSet->CoeffSet[20]);

	vin_write(pcdev, V0C8A, pSet->CoeffSet[21]);
	vin_write(pcdev, V0C8B, pSet->CoeffSet[22]);
	vin_write(pcdev, V0C8C, pSet->CoeffSet[23]);
}

/* rect is guaranteed to not exceed the scaled camera rectangle */
static int vin_set_rect(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct vin_cam *cam = icd->host_priv;
	struct vin_dev *pcdev = ici->priv;
	unsigned int left_offset, top_offset;
	struct v4l2_rect *cam_subrect = &cam->subrect;
	unsigned long value;

	dev_geo(icd->parent, "Crop %ux%u@%u:%u\n",
		icd->user_width, icd->user_height, cam->vin_left, cam->vin_top);

	left_offset	= cam->vin_left;
	top_offset	= cam->vin_top;

	dev_geo(icd->parent, "Cam %ux%u@%u:%u\n",
		cam->width, cam->height, cam->vin_left, cam->vin_top);

	dev_geo(icd->parent, "Cam subrect %ux%u@%u:%u\n",
		cam_subrect->width, cam_subrect->height,
		cam_subrect->left, cam_subrect->top);

	/* Set Pre-Clip with S_CROP area */
	vin_write(pcdev, V0SPPrC, cam_subrect->left);
	vin_write(pcdev, V0EPPrC, cam_subrect->left + cam_subrect->width - 1);
	if ((pcdev->field == V4L2_FIELD_INTERLACED) ||
		(pcdev->field == V4L2_FIELD_INTERLACED_TB) ||
		(pcdev->field == V4L2_FIELD_INTERLACED_BT)) {
		vin_write(pcdev, V0SLPrC, (cam_subrect->top + 1) / 2);
		vin_write(pcdev, V0ELPrC,
			(cam_subrect->top + cam_subrect->height + 1) / 2 - 1);
	} else {
		vin_write(pcdev, V0SLPrC, cam_subrect->top);
		vin_write(pcdev, V0ELPrC,
			cam_subrect->top + cam_subrect->height - 1);
	}

	/* Set Scaling Coefficient Set */
	value = 0;
	if (cam_subrect->height != cam->out_height)
		value = (4096 * cam_subrect->height) / cam->out_height;
	dev_geo(icd->parent, "YS Value: %lx\n", value);
	vin_write(pcdev, V0YS, value);

	value = 0;
	if (cam_subrect->width != cam->out_width)
		value = (4096 * cam_subrect->width) / cam->out_width;

	/* Horizontal enlargement is up to double size */
	if (0 < value  && value < 0x0800)
		value = 0x0800;

	dev_geo(icd->parent, "XS Value: %lx\n", value);
	vin_write(pcdev, V0XS, value);

	/* Horizontal enlargement is carried out */
	/* by scaling down from double size */
	if (value < 0x1000)
		value *= 2;

	setCoeff(pcdev, value);

	/* Set Post-Clip with S_FMT size*/
	vin_write(pcdev, V0SPPoC, 0);
	vin_write(pcdev, V0SLPoC, 0);
	vin_write(pcdev, V0EPPoC, cam->out_width - 1);
	if ((pcdev->field == V4L2_FIELD_INTERLACED) ||
		(pcdev->field == V4L2_FIELD_INTERLACED_TB) ||
		(pcdev->field == V4L2_FIELD_INTERLACED_BT))
		vin_write(pcdev, V0ELPoC, (cam->out_height + 1) / 2 - 1);
	else
		vin_write(pcdev, V0ELPoC, cam->out_height - 1);

	vin_write(pcdev, V0IS, ((cam->out_width + 15) & ~0xf));

	return 0;
}

static u32 capture_save_reset(struct vin_dev *pcdev)
{
	unsigned long timeout = jiffies + 10; /* wait for 100ms */

	u32 vnmc = vin_read(pcdev, V0MC);
	vin_write(pcdev, V0MC, vnmc & ~VIN_VNMC_ME); /* stop capture */

	/*
	 * Wait until the end of the current frame.
	 */
	while (vin_is_active(pcdev) && time_before(jiffies, timeout))
		usleep_range(1000, 1000);

	if (time_after(jiffies, timeout))
		dev_err(pcdev->ici.v4l2_dev.dev,
			"Timeout waiting for frame end! Interface problem?\n");

	return vnmc;
}

static void capture_restore(struct vin_dev *pcdev, u32 vnmc)
{
	/* restore */
	vin_write(pcdev, V0MC, vnmc);
}

#define VIN_BUS_FLAGS (V4L2_MBUS_MASTER |	\
		V4L2_MBUS_PCLK_SAMPLE_RISING |	\
		V4L2_MBUS_HSYNC_ACTIVE_HIGH |	\
		V4L2_MBUS_HSYNC_ACTIVE_LOW |	\
		V4L2_MBUS_VSYNC_ACTIVE_HIGH |	\
		V4L2_MBUS_VSYNC_ACTIVE_LOW |	\
		V4L2_MBUS_DATA_ACTIVE_HIGH)

/* Capture is not running, no interrupts, no locking needed */
static int vin_set_bus_param(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct vin_dev *pcdev = ici->priv;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct v4l2_mbus_config cfg = {.type = V4L2_MBUS_PARALLEL,};
	unsigned long value, common_flags = VIN_BUS_FLAGS;
	u32 capsr = capture_save_reset(pcdev);
	int ret;

	/*
	 * If the client doesn't implement g_mbus_config, we just use our
	 * platform data
	 */
	ret = v4l2_subdev_call(sd, video, g_mbus_config, &cfg);
	if (!ret) {
		common_flags = soc_mbus_config_compatible(&cfg, common_flags);
		if (!common_flags)
			return -EINVAL;
	} else if (ret != -ENOIOCTLCMD) {
		return ret;
	}

	/* Make choises, based on platform preferences */
	if ((common_flags & V4L2_MBUS_HSYNC_ACTIVE_HIGH) &&
	    (common_flags & V4L2_MBUS_HSYNC_ACTIVE_LOW)) {
		if (pcdev->pdata->flags & VIN_FLAG_HSYNC_LOW)
			common_flags &= ~V4L2_MBUS_HSYNC_ACTIVE_HIGH;
		else
			common_flags &= ~V4L2_MBUS_HSYNC_ACTIVE_LOW;
	}

	if ((common_flags & V4L2_MBUS_VSYNC_ACTIVE_HIGH) &&
	    (common_flags & V4L2_MBUS_VSYNC_ACTIVE_LOW)) {
		if (pcdev->pdata->flags & VIN_FLAG_VSYNC_LOW)
			common_flags &= ~V4L2_MBUS_VSYNC_ACTIVE_HIGH;
		else
			common_flags &= ~V4L2_MBUS_VSYNC_ACTIVE_LOW;
	}

	cfg.flags = common_flags;
	ret = v4l2_subdev_call(sd, video, s_mbus_config, &cfg);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return ret;

	value = VIN_VNDMR2_FTEV | VIN_VNDMR2_VLV_1;

	value |= common_flags & V4L2_MBUS_VSYNC_ACTIVE_LOW ?
	  VIN_VNDMR2_VPS_ACTIVE_LOW : VIN_VNDMR2_VPS_ACTIVE_HIGH;
	value |= common_flags & V4L2_MBUS_HSYNC_ACTIVE_LOW ?
	  VIN_VNDMR2_HPS_ACTIVE_LOW : VIN_VNDMR2_HPS_ACTIVE_HIGH;

	/* set Data Mode Register2 */
	vin_write(pcdev, V0DMR2, value);

	ret = vin_set_rect(icd);
	if (ret < 0)
		return ret;

	mdelay(1);

	capture_restore(pcdev, capsr);

	return 0;
}

static int vin_try_bus_param(struct soc_camera_device *icd,
				       unsigned char buswidth)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	unsigned long common_flags = VIN_BUS_FLAGS;
	struct v4l2_mbus_config cfg = {.type = V4L2_MBUS_PARALLEL,};
	int ret;

	ret = v4l2_subdev_call(sd, video, g_mbus_config, &cfg);
	if (!ret)
		common_flags = soc_mbus_config_compatible(&cfg, common_flags);
	else if (ret != -ENOIOCTLCMD)
		return ret;

	if (!common_flags || buswidth > 16 ||
	    (buswidth > 8 && !(common_flags & SOCAM_DATAWIDTH_16)))
		return -EINVAL;

	return 0;
}

static const struct soc_mbus_pixelfmt vin_formats[] = {
	{
		.fourcc			= V4L2_PIX_FMT_NV16,
		.name			= "NV16",
		.bits_per_sample	= 16,
		.packing		= SOC_MBUS_PACKING_NONE,
		.order			= SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc			= V4L2_PIX_FMT_YUYV,
		.name			= "YUYV",
		.bits_per_sample	= 16,
		.packing		= SOC_MBUS_PACKING_NONE,
		.order			= SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc			= V4L2_PIX_FMT_UYVY,
		.name			= "UYVY",
		.bits_per_sample	= 16,
		.packing		= SOC_MBUS_PACKING_NONE,
		.order			= SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc			= V4L2_PIX_FMT_RGB565,
		.name			= "RGB565",
		.bits_per_sample	= 16,
		.packing		= SOC_MBUS_PACKING_NONE,
		.order			= SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc			= V4L2_PIX_FMT_RGB555X,
		.name			= "ARGB1555",
		.bits_per_sample	= 16,
		.packing		= SOC_MBUS_PACKING_NONE,
		.order			= SOC_MBUS_ORDER_LE,
	},
};

static int client_g_rect(struct v4l2_subdev *sd, struct v4l2_rect *rect);

static int vin_get_formats(struct soc_camera_device *icd, unsigned int idx,
				     struct soc_camera_format_xlate *xlate)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct device *dev = icd->parent;
	int ret, k, n;
	int formats = 0;
	struct vin_cam *cam;
	enum v4l2_mbus_pixelcode code;
	const struct soc_mbus_pixelfmt *fmt;

	ret = v4l2_subdev_call(sd, video, enum_mbus_fmt, idx, &code);
	if (ret < 0)
		/* No more formats */
		return 0;

	fmt = soc_mbus_get_fmtdesc(code);
	if (!fmt) {
		dev_err(icd->parent,
			"Invalid format code #%u: %d\n", idx, code);
		return -EINVAL;
	}

	ret = vin_try_bus_param(icd, fmt->bits_per_sample);
	if (ret < 0)
		return 0;

	if (!icd->host_priv) {
		struct v4l2_mbus_framefmt mf;
		struct v4l2_rect rect;
		int shift = 0;

		/* FIXME: subwindow is lost between close / open */

		/* Cache current client geometry */
		ret = client_g_rect(sd, &rect);
		if (ret < 0)
			return ret;

		/* First time */
		ret = v4l2_subdev_call(sd, video, g_mbus_fmt, &mf);
		if (ret < 0)
			return ret;

		while ((mf.width > 2560 || mf.height > 1920) && shift < 4) {
			/* Try 2560x1920, 1280x960, 640x480, 320x240 */
			mf.width	= 2560 >> shift;
			mf.height	= 1920 >> shift;
			ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);
			if (ret < 0)
				return ret;
			shift++;
		}

		if (shift == 4) {
			dev_err(dev,
				"Failed to configure the client below %ux%x\n",
				mf.width, mf.height);
			return -EIO;
		}

		dev_geo(dev, "camera fmt %ux%u\n", mf.width, mf.height);

		cam = kzalloc(sizeof(*cam), GFP_KERNEL);
		if (!cam)
			return -ENOMEM;

		/* We are called with current camera crop,
		   initialise subrect with it */
		cam->rect	= rect;
		cam->subrect	= rect;

		cam->width	= mf.width;
		cam->height	= mf.height;

		cam->out_width	= mf.width;
		cam->out_height	= mf.height;

		icd->host_priv = cam;
	} else {
		cam = icd->host_priv;
	}

	/* Beginning of a pass */
	if (!idx)
		cam->extra_fmt = NULL;

	switch (code) {
	case V4L2_MBUS_FMT_YUYV8_2X8:
	case V4L2_MBUS_FMT_RGB565_2X8_LE:
		if (cam->extra_fmt)
			break;

		/*
		 * Our case is simple so far: for any of the above four camera
		 * formats we add all our four synthesized NV* formats, so,
		 * just marking the device with a single flag suffices. If
		 * the format generation rules are more complex, you would have
		 * to actually hang your already added / counted formats onto
		 * the host_priv pointer and check whether the format you're
		 * going to add now is already there.
		 */
		cam->extra_fmt = vin_formats;

		n = ARRAY_SIZE(vin_formats);
		formats += n;
		for (k = 0; xlate && k < n; k++) {
			xlate->host_fmt	= &vin_formats[k];
			xlate->code	= code;
			xlate++;
			dev_dbg(dev, "Providing format %s using code %d\n",
				vin_formats[k].name, code);
		}
		break;
	default:
		return 0;
	}

	/* Generic pass-through */
	formats++;
	if (xlate) {
		xlate->host_fmt	= fmt;
		xlate->code	= code;
		xlate++;
		dev_dbg(dev, "Providing format %s in pass-through mode\n",
			xlate->host_fmt->name);
	}

	return formats;
}

static void vin_put_formats(struct soc_camera_device *icd)
{
	kfree(icd->host_priv);
	icd->host_priv = NULL;
}

/* Check if any dimension of r1 is smaller than respective one of r2 */
static bool is_smaller(struct v4l2_rect *r1, struct v4l2_rect *r2)
{
	return r1->width < r2->width || r1->height < r2->height;
}

/* Check if r1 fails to cover r2 */
static bool is_inside(struct v4l2_rect *r1, struct v4l2_rect *r2)
{
	return r1->left > r2->left || r1->top > r2->top ||
		r1->left + r1->width < r2->left + r2->width ||
		r1->top + r1->height < r2->top + r2->height;
}

static unsigned int scale_down(unsigned int size, unsigned int scale)
{
	return (size * 4096 + scale / 2) / scale;
}

static unsigned int calc_generic_scale(unsigned int input, unsigned int output)
{
	return (input * 4096 + output / 2) / output;
}

/* Get and store current client crop */
static int client_g_rect(struct v4l2_subdev *sd, struct v4l2_rect *rect)
{
	struct v4l2_crop crop;
	struct v4l2_cropcap cap;
	int ret;

	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = v4l2_subdev_call(sd, video, g_crop, &crop);
	if (!ret) {
		*rect = crop.c;
		return ret;
	}

	/* Camera driver doesn't support .g_crop(), assume default rectangle */
	cap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = v4l2_subdev_call(sd, video, cropcap, &cap);
	if (!ret)
		*rect = cap.defrect;

	return ret;
}

/* Client crop has changed, update our sub-rectangle to remain
   within the area */
static void update_subrect(struct vin_cam *cam)
{
	struct v4l2_rect *rect = &cam->rect, *subrect = &cam->subrect;

	if (rect->width < subrect->width)
		subrect->width = rect->width;

	if (rect->height < subrect->height)
		subrect->height = rect->height;

	if (rect->left > subrect->left)
		subrect->left = rect->left;
	else if (rect->left + rect->width <
		 subrect->left + subrect->width)
		subrect->left = rect->left + rect->width -
			subrect->width;

	if (rect->top > subrect->top)
		subrect->top = rect->top;
	else if (rect->top + rect->height <
		 subrect->top + subrect->height)
		subrect->top = rect->top + rect->height -
			subrect->height;
}

/*
 * The common for both scaling and cropping iterative approach is:
 * 1. try if the client can produce exactly what requested by the user
 * 2. if (1) failed, try to double the client image until we get one big enough
 * 3. if (2) failed, try to request the maximum image
 */
static int client_s_crop(struct soc_camera_device *icd, struct v4l2_crop *crop,
			 struct v4l2_crop *cam_crop)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct v4l2_rect *rect = &crop->c, *cam_rect = &cam_crop->c;
	struct device *dev = sd->v4l2_dev->dev;
	struct vin_cam *cam = icd->host_priv;
	struct v4l2_cropcap cap;
	int ret;
	unsigned int width, height;

	v4l2_subdev_call(sd, video, s_crop, crop);

	ret = client_g_rect(sd, cam_rect);
	if (ret < 0)
		return ret;

	/*
	 * Now cam_crop contains the current camera input rectangle, and it must
	 * be within camera cropcap bounds
	 */
	if (!memcmp(rect, cam_rect, sizeof(*rect))) {
		/* Even if camera S_CROP failed, but camera rectangle matches */
		dev_dbg(dev, "Camera S_CROP successful for %dx%d@%d:%d\n",
			rect->width, rect->height, rect->left, rect->top);
		cam->rect = *cam_rect;
		return 0;
	}

	/* Try to fix cropping, that camera hasn't managed to set */
	dev_geo(dev, "Fix camera S_CROP for %dx%d@%d:%d to %dx%d@%d:%d\n",
		cam_rect->width, cam_rect->height,
		cam_rect->left, cam_rect->top,
		rect->width, rect->height, rect->left, rect->top);

	/* We need sensor maximum rectangle */
	ret = v4l2_subdev_call(sd, video, cropcap, &cap);
	if (ret < 0)
		return ret;

	/*
	 * Popular special case - some cameras can only handle fixed sizes like
	 * QVGA, VGA,... Take care to avoid infinite loop.
	 */
	width = max(cam_rect->width, 2);
	height = max(cam_rect->height, 2);

	/*
	 * Loop as long as sensor is not covering the requested rectangle and
	 * is still within its bounds
	 */
	while (!ret && (is_smaller(cam_rect, rect) ||
			is_inside(cam_rect, rect)) &&
	       (cap.bounds.width > width || cap.bounds.height > height)) {

		width *= 2;
		height *= 2;

		cam_rect->width = width;
		cam_rect->height = height;

		/*
		 * We do not know what capabilities the camera has to set up
		 * left and top borders. We could try to be smarter in iterating
		 * them, e.g., if camera current left is to the right of the
		 * target left, set it to the middle point between the current
		 * left and minimum left. But that would add too much
		 * complexity: we would have to iterate each border separately.
		 * Instead we just drop to the left and top bounds.
		 */
		if (cam_rect->left > rect->left)
			cam_rect->left = cap.bounds.left;

		if (cam_rect->left + cam_rect->width < rect->left + rect->width)
			cam_rect->width = rect->left + rect->width -
				cam_rect->left;

		if (cam_rect->top > rect->top)
			cam_rect->top = cap.bounds.top;

		if (cam_rect->top + cam_rect->height < rect->top + rect->height)
			cam_rect->height = rect->top + rect->height -
				cam_rect->top;

		v4l2_subdev_call(sd, video, s_crop, cam_crop);
		ret = client_g_rect(sd, cam_rect);
		dev_geo(dev, "Camera S_CROP %d for %dx%d@%d:%d\n", ret,
			cam_rect->width, cam_rect->height,
			cam_rect->left, cam_rect->top);
	}

	/* S_CROP must not modify the rectangle */
	if (is_smaller(cam_rect, rect) || is_inside(cam_rect, rect)) {
		/*
		 * The camera failed to configure a suitable cropping,
		 * we cannot use the current rectangle, set to max
		 */
		*cam_rect = cap.bounds;
		v4l2_subdev_call(sd, video, s_crop, cam_crop);
		ret = client_g_rect(sd, cam_rect);
		dev_geo(dev, "Camera S_CROP %d for max %dx%d@%d:%d\n", ret,
			cam_rect->width, cam_rect->height,
			cam_rect->left, cam_rect->top);
	}

	if (!ret) {
		cam->rect = *cam_rect;
		cam->subrect = *rect;

		dev_geo(dev, "Update subrect %dx%d@%d:%d within %dx%d@%d:%d\n",
			cam->subrect.width, cam->subrect.height,
			cam->subrect.left, cam->subrect.top,
			cam->rect.width, cam->rect.height,
			cam->rect.left, cam->rect.top);

		update_subrect(cam);
	}

	return ret;
}

/* Iterative s_mbus_fmt, also updates cached client crop on success */
static int client_s_fmt(struct soc_camera_device *icd,
			struct v4l2_mbus_framefmt *mf, bool vin_can_scale)
{
	struct vin_cam *cam = icd->host_priv;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct device *dev = icd->parent;
	unsigned int width = mf->width, height = mf->height, tmp_w, tmp_h;
	unsigned int max_width, max_height;
	struct v4l2_cropcap cap;
	int ret;

	ret = v4l2_subdev_call(sd, video, s_mbus_fmt, mf);
	if (ret < 0)
		return ret;

	dev_geo(dev, "camera scaled to %ux%u\n", mf->width, mf->height);

	if ((width == mf->width && height == mf->height) || !vin_can_scale)
		goto update_cache;

	cap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = v4l2_subdev_call(sd, video, cropcap, &cap);
	if (ret < 0)
		return ret;

	max_width = min(cap.bounds.width, 2560);
	max_height = min(cap.bounds.height, 1920);

	/* Camera set a format, but geometry is not precise, try to improve */
	tmp_w = mf->width;
	tmp_h = mf->height;

	/* width <= max_width && height <= max_height - guaranteed by try_fmt */
	while ((width > tmp_w || height > tmp_h) &&
	       tmp_w < max_width && tmp_h < max_height) {
		tmp_w = min(2 * tmp_w, max_width);
		tmp_h = min(2 * tmp_h, max_height);
		mf->width = tmp_w;
		mf->height = tmp_h;
		ret = v4l2_subdev_call(sd, video, s_mbus_fmt, mf);
		dev_geo(dev, "Camera scaled to %ux%u\n",
			mf->width, mf->height);
		if (ret < 0) {
			/* This shouldn't happen */
			dev_err(dev, "Client failed to set format: %d\n", ret);
			return ret;
		}
	}

update_cache:
	/* Update cache */
	ret = client_g_rect(sd, &cam->rect);
	if (ret < 0)
		return ret;

	update_subrect(cam);

	return 0;
}

/**
 * @width	- on output: user width, mapped back to input
 * @height	- on output: user height, mapped back to input
 * @mf		- in- / output camera output window
 */
static int client_scale(struct soc_camera_device *icd,
			struct v4l2_mbus_framefmt *mf,
			unsigned int *width, unsigned int *height,
			bool vin_can_scale)
{
	struct vin_cam *cam = icd->host_priv;
	struct device *dev = icd->parent;
	struct v4l2_mbus_framefmt mf_tmp = *mf;
	unsigned int scale_h, scale_v;
	int ret;

	/*
	 * 5. Apply iterative camera S_FMT for camera user window (also updates
	 *    client crop cache and the imaginary sub-rectangle).
	 */
	ret = client_s_fmt(icd, &mf_tmp, vin_can_scale);
	if (ret < 0)
		return ret;

	dev_geo(dev, "5: camera scaled to %ux%u\n",
		mf_tmp.width, mf_tmp.height);

	/* 6. Retrieve camera output window (g_fmt) */

	/* unneeded - it is already in "mf_tmp" */

	/* 7. Calculate new client scales. */
	/* Should be 4096 if the client does not support scaling */
	scale_h = calc_generic_scale(cam->rect.width, mf_tmp.width);
	scale_v = calc_generic_scale(cam->rect.height, mf_tmp.height);

	mf->width	= mf_tmp.width;
	mf->height	= mf_tmp.height;
	mf->colorspace	= mf_tmp.colorspace;

	/*
	 * 8. Calculate new VIN crop - apply camera scales to previously
	 *    updated "effective" crop.
	 */
	*width = scale_down(cam->subrect.width, scale_h);
	*height = scale_down(cam->subrect.height, scale_v);

	dev_geo(dev, "8: new client sub-window %ux%u\n", *width, *height);

	return 0;
}

/*
 * VIN can crop.
 */
static int vin_set_crop(struct soc_camera_device *icd,
				  struct v4l2_crop *a)
{
	struct v4l2_rect *rect = &a->c;
	struct device *dev = icd->parent;
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct vin_dev *pcdev = ici->priv;
	struct v4l2_crop cam_crop;
	struct vin_cam *cam = icd->host_priv;
	struct v4l2_rect *cam_rect = &cam_crop.c;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct v4l2_mbus_framefmt mf;
	u32 vnmc, i;
	int ret;

	dev_geo(dev, "S_CROP(%ux%u@%u:%u)\n", rect->width, rect->height,
		rect->left, rect->top);

	/* During camera cropping its output window can change too, stop VIN */
	vnmc = capture_save_reset(pcdev);
	dev_dbg(dev, "V0MC 0x%x\n", vnmc);

	/*
	 * 1. - 2. Apply iterative camera S_CROP for new input window, read back
	 * actual camera rectangle.
	 */
	ret = client_s_crop(icd, a, &cam_crop);
	if (ret < 0)
		return ret;

	dev_geo(dev, "1-2: camera cropped to %ux%u@%u:%u\n",
		cam_rect->width, cam_rect->height,
		cam_rect->left, cam_rect->top);

	/* On success cam_crop contains current camera crop */

	/* 3. Retrieve camera output window */
	ret = v4l2_subdev_call(sd, video, g_mbus_fmt, &mf);
	if (ret < 0)
		return ret;

	if (mf.width > 2560 || mf.height > 1920)
		return -EINVAL;

	/* Cache camera output window */
	cam->width	= mf.width;
	cam->height	= mf.height;

	icd->user_width	 = cam->width;
	icd->user_height = cam->height;

	if (rect->left < 0)
		rect->left = 0;
	if (rect->top < 0)
		rect->top = 0;

	cam->vin_left	 = rect->left & ~1;
	cam->vin_top	 = rect->top & ~1;

	cam->subrect = *rect;

	/* 6. Use VIN cropping to crop to the new window. */
	ret = vin_set_rect(icd);
	if (ret < 0)
		return ret;


	dev_geo(dev, "6: VIN cropped to %ux%u@%u:%u\n",
		icd->user_width, icd->user_height,
		cam->vin_left, cam->vin_top);

	/* Restore capture */
	for (i = 0; i < MB_NUM; i++) {
		if ((pcdev->queue_buf[i]) &&
		    (pcdev->capture_status == STOPPED)) {
			vnmc |= VIN_VNMC_ME;
			break;
		}
	}
	capture_restore(pcdev, vnmc);

	/* Even if only camera cropping succeeded */
	return ret;
}

static int vin_get_crop(struct soc_camera_device *icd,
				  struct v4l2_crop *a)
{
	struct vin_cam *cam = icd->host_priv;

	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->c = cam->subrect;

	return 0;
}

/*
 * Calculate real client output window by applying new scales to the current
 * client crop. New scales are calculated from the requested output format and
 * VIN crop, mapped backed onto the client input (subrect).
 */
static void calculate_client_output(struct soc_camera_device *icd,
		struct v4l2_pix_format *pix, struct v4l2_mbus_framefmt *mf)
{
	struct vin_cam *cam = icd->host_priv;
	struct device *dev = icd->parent;
	struct v4l2_rect *cam_subrect = &cam->subrect;
	unsigned int scale_v, scale_h;

	if (cam_subrect->width == cam->rect.width &&
	    cam_subrect->height == cam->rect.height) {
		/* No sub-cropping */
		mf->width	= pix->width;
		mf->height	= pix->height;
		return;
	}
	/* 1.-2. Current camera scales and subwin - cached. */

	dev_geo(dev, "2: subwin %ux%u@%u:%u\n",
		cam_subrect->width, cam_subrect->height,
		cam_subrect->left, cam_subrect->top);

	/*
	 * 3. Calculate new combined scales from input sub-window to requested
	 *    user window.
	 */

	scale_h = calc_generic_scale(cam_subrect->width, pix->width);
	scale_v = calc_generic_scale(cam_subrect->height, pix->height);

	dev_geo(dev, "3: scales %u:%u\n", scale_h, scale_v);

	/*
	 * 4. Calculate client output window by applying combined scales to real
	 *    input window.
	 */
	mf->width	= scale_down(cam->rect.width, scale_h);
	mf->height	= scale_down(cam->rect.height, scale_v);
}

/* Similar to set_crop multistage iterative algorithm */
static int vin_set_fmt(struct soc_camera_device *icd,
				 struct v4l2_format *f)
{
	struct device *dev = icd->parent;
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct vin_dev *pcdev = ici->priv;
	struct vin_cam *cam = icd->host_priv;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	__u32 pixfmt = pix->pixelformat;
	const struct soc_camera_format_xlate *xlate;
	unsigned int vin_sub_width = 0, vin_sub_height = 0;
	int ret;
	bool can_scale;
	enum v4l2_field field;

	dev_geo(dev, "S_FMT(pix=0x%x, %ux%u)\n",
		pixfmt, pix->width, pix->height);

	switch (pix->field) {
	default:
		pix->field = V4L2_FIELD_NONE;
		/* fall-through */
	case V4L2_FIELD_NONE:
	case V4L2_FIELD_TOP:
	case V4L2_FIELD_BOTTOM:
	case V4L2_FIELD_INTERLACED_TB:
	case V4L2_FIELD_INTERLACED_BT:
		field = pix->field;
		break;
	case V4L2_FIELD_INTERLACED:
		field = V4L2_FIELD_INTERLACED_TB;
		break;
	}

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (!xlate) {
		dev_warn(dev, "Format %x not found\n", pixfmt);
		return -EINVAL;
	}

	/* 1.-4. Calculate client output geometry */
	calculate_client_output(icd, pix, &mf);
	mf.field	= pix->field;
	mf.colorspace	= pix->colorspace;
	mf.code		= xlate->code;

	switch (pixfmt) {
	case V4L2_PIX_FMT_NV16:
		can_scale = false;
		break;
	default:
		can_scale = true;
	}

	dev_geo(dev, "4: request camera output %ux%u\n", mf.width, mf.height);

	/* 5. - 9. */
	ret = client_scale(icd, &mf,
		 &vin_sub_width, &vin_sub_height, can_scale);

	dev_geo(dev, "5-9: client scale return %d\n", ret);

	/* Done with the camera. Now see if we can improve the result */

	dev_geo(dev, "Camera %d fmt %ux%u, requested %ux%u\n",
		ret, mf.width, mf.height, pix->width, pix->height);
	if (ret < 0)
		return ret;

	if (mf.code != xlate->code)
		return -EINVAL;

	/* 9. Prepare VIN crop */
	cam->width = mf.width;
	cam->height = mf.height;

	dev_geo(dev, "10: VIN in->out, width: %u -> %u, height: %u -> %u\n",
		vin_sub_width, pix->width,
		vin_sub_height, pix->height);

	cam->out_width = pix->width;
	cam->out_height = pix->height;

	cam->code		= xlate->code;
	icd->current_fmt	= xlate;

	pcdev->field = field;

	return 0;
}

static int vin_try_fmt(struct soc_camera_device *icd,
				 struct v4l2_format *f)
{
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct v4l2_mbus_framefmt mf;
	__u32 pixfmt = pix->pixelformat;
	int width, height;
	int ret;

	dev_geo(icd->parent, "TRY_FMT(pix=0x%x, %ux%u)\n",
		pixfmt, pix->width, pix->height);

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (!xlate) {
		dev_warn(icd->parent, "Format %x not found\n", pixfmt);
		return -EINVAL;
	}

	/* FIXME: calculate using depth and bus width */

	v4l_bound_align_image(&pix->width, 2, 2560, 1,
			      &pix->height, 4, 1920, 2, 0);

	width = pix->width;
	height = pix->height;

	pix->bytesperline = soc_mbus_bytes_per_line(width, xlate->host_fmt);
	if ((int)pix->bytesperline < 0)
		return pix->bytesperline;
	pix->sizeimage = height * pix->bytesperline;

	/* limit to sensor capabilities */
	mf.width	= pix->width;
	mf.height	= pix->height;
	mf.field	= pix->field;
	mf.code		= xlate->code;
	mf.colorspace	= pix->colorspace;

	ret = v4l2_subdev_call(sd, video, try_mbus_fmt, &mf);
	if (ret < 0)
		return ret;

	/* Adjust only if VIN can not scale */
	if (pix->width > (mf.width * 2))
		pix->width	= mf.width * 2;
	if (pix->height > (mf.height * 3))
		pix->height	= mf.height * 3;

	pix->field	= mf.field;
	pix->colorspace	= mf.colorspace;

	switch (pixfmt) {
	case V4L2_PIX_FMT_NV16:
		/* FIXME: check against rect_max after converting soc-camera */
		/* We can scale precisely, need a bigger image from camera */
		if (pix->width < width || pix->height < height) {
			/*
			 * We presume, the sensor behaves sanely, i.e., if
			 * requested a bigger rectangle, it will not return a
			 * smaller one.
			 */
			mf.width = 2560;
			mf.height = 1920;
			ret = v4l2_subdev_call(sd, video, try_mbus_fmt, &mf);
			if (ret < 0) {
				/* Shouldn't actually happen... */
				dev_err(icd->parent,
					"FIXME: client try_fmt() = %d\n", ret);
				return ret;
			}
		}
		/* We will scale exactly */
		if (mf.width > width)
			pix->width = width;
		if (mf.height > height)
			pix->height = height;
	}

	return ret;
}

static unsigned int vin_poll(struct file *file, poll_table *pt)
{
	struct soc_camera_device *icd = file->private_data;

	return vb2_poll(&icd->vb2_vidq, file, pt);
}

static int vin_querycap(struct soc_camera_host *ici,
				  struct v4l2_capability *cap)
{
	strlcpy(cap->card, "VIN", sizeof(cap->card));
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	return 0;
}

static int vin_init_videobuf(struct vb2_queue *q,
				       struct soc_camera_device *icd)
{
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_USERPTR;
	q->drv_priv = icd;
	q->ops = &vin_videobuf_ops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->buf_struct_size = sizeof(struct vin_buffer);

	return vb2_queue_init(q);
}

static struct soc_camera_host_ops vin_host_ops = {
	.owner		= THIS_MODULE,
	.add		= vin_add_device,
	.remove		= vin_remove_device,
	.get_formats	= vin_get_formats,
	.put_formats	= vin_put_formats,
	.get_crop	= vin_get_crop,
	.set_crop	= vin_set_crop,
	.set_fmt	= vin_set_fmt,
	.try_fmt	= vin_try_fmt,
	.poll		= vin_poll,
	.querycap	= vin_querycap,
	.set_bus_param	= vin_set_bus_param,
	.init_videobuf2	= vin_init_videobuf,
};

#if VIN_SUPPORT_TO_CHECK_REGS
struct vin_test_of_reg {
	char *name;
	unsigned int attr;
	unsigned int offset;
	unsigned int mask;
	unsigned int value;
};

static int vin_test_of_reg(struct vin_dev *pcdev, int mode)
{
	int index = 0;
	unsigned int real;
	int result = 1;
	void __iomem *base = pcdev->base;
	struct vin_test_of_reg checktable1[] = {
		/* name, attr, offset, mask, value */
	    {"V0MC", 0, V0MC, 0, 0x00000000},
	    {"V0MS", 0, V0MS, 0x00000004, 0x00000018},
	    {"V0FC", 0, V0FC, 0, 0x00000000},
	    {"V0SLPrC", 0, V0SLPrC, 0, 0x00000000},
	    {"V0ELPrC", 0, V0ELPrC, 0, 0x00000000},
	    {"V0SPPrC", 0, V0SPPrC, 0, 0x00000000},
	    {"V0EPPrC", 0, V0EPPrC, 0, 0x00000000},
	    {"V0SLPoC", 0, V0SLPoC, 0, 0x00000000},
	    {"V0ELPoC", 0, V0ELPoC, 0, 0x00000000},
	    {"V0SPPoC", 0, V0SPPoC, 0, 0x00000000},
	    {"V0EPPoC", 0, V0EPPoC, 0, 0x00000000},
	    {"V0IS", 0, V0IS, 0, 0x00000000},
	    {"V0MB1", 0, V0MB1, 0, 0x00000000},
	    {"V0MB2", 0, V0MB2, 0, 0x00000000},
	    {"V0MB3", 0, V0MB3, 0, 0x00000000},
	    {"V0LC", 0, V0LC, 0, 0x00000000},
	    {"V0IE", 0, V0IE, 0, 0x00000000},
	    {"V0INTS", 0, V0INTS, 0xFFFFFFFF, 0x00000000},
	    {"V0SI", 0, V0SI, 0, 0x00000000},
	    {"V0MTC", 0, V0MTC, 0, 0x0A080108},
	    {"V0YS", 0, V0YS, 0, 0x00000000},
	    {"V0XS", 0, V0XS, 0, 0x00000000},
	    {"V0DMR", 0, V0DMR, 0, 0x00000000},
	    {"V0DMR2", 0, V0DMR2, 0, 0x00000000},
	    {"V0CSCC1", 0, V0CSCC1, 0, 0x01291080},
	    {"V0CSCC2", 0, V0CSCC2, 0, 0x019800D0},
	    {"V0CSCC3", 0, V0CSCC3, 0, 0x00640204},
	    {"V0C1A", 0, V0C1A, 0, 0x00000000},
	    {"V0C1B", 0, V0C1B, 0, 0x00000000},
	    {"V0C1C", 0, V0C1C, 0, 0x00000000},
	    {"V0C2A", 0, V0C2A, 0, 0x00000000},
	    {"V0C2B", 0, V0C2B, 0, 0x00000000},
	    {"V0C2C", 0, V0C2C, 0, 0x00000000},
	    {"V0C3A", 0, V0C3A, 0, 0x00000000},
	    {"V0C3B", 0, V0C3B, 0, 0x00000000},
	    {"V0C3C", 0, V0C3C, 0, 0x00000000},
	    {"V0C4A", 0, V0C4A, 0, 0x00000000},
	    {"V0C4B", 0, V0C4B, 0, 0x00000000},
	    {"V0C4C", 0, V0C4C, 0, 0x00000000},
	    {"V0C5A", 0, V0C5A, 0, 0x00000000},
	    {"V0C5B", 0, V0C5B, 0, 0x00000000},
	    {"V0C5C", 0, V0C5C, 0, 0x00000000},
	    {"V0C6A", 0, V0C6A, 0, 0x00000000},
	    {"V0C6B", 0, V0C6B, 0, 0x00000000},
	    {"V0C6C", 0, V0C6C, 0, 0x00000000},
	    {"V0C7A", 0, V0C7A, 0, 0x00000000},
	    {"V0C7B", 0, V0C7B, 0, 0x00000000},
	    {"V0C7C", 0, V0C7C, 0, 0x00000000},
	    {"V0C8A", 0, V0C8A, 0, 0x00000000},
	    {"V0C8B", 0, V0C8B, 0, 0x00000000},
	    {"V0C8C", 0, V0C8C, 0, 0x00000000},
	    {NULL, 0xFFFFFFFF, 0, 0, 0}
	};

	printk(KERN_ALERT "<LOG msg=\"Start vin_test_of_reg\" base=\"0x%x\">\n",
	(unsigned long)base);

	while (checktable1[index].name != NULL) {
		real = vin_read(pcdev, checktable1[index].offset);
		real &= ~checktable1[index].mask;
		if (real != checktable1[index].value) {
			printk(KERN_ALERT "<check type=\"initial value\" name=\"%s\" real=\"0x%x\" expected=\"0x%x\"/>\n",
		checktable1[index].name, real, checktable1[index].value);

			result = 0;
		}
		index++;
	}
	if (result)
		printk(KERN_ALERT "<Result summary=\"Pass\"/>\n");
	else
		printk(KERN_ALERT "<Result summary=\"Fail\"/>\n");

	printk(KERN_ALERT "</LOG>\n");

	return 0;
}
#endif /* VIN_SUPPORT_TO_CHECK_REGS */

static int __devinit vin_probe(struct platform_device *pdev)
{
	struct vin_dev *pcdev;
	struct resource *res;
	void __iomem *base;
	unsigned int irq, i;
	int err = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!res || (int)irq <= 0) {
		dev_err(&pdev->dev, "Not enough VIN platform resources.\n");
		err = -ENODEV;
		goto exit;
	}

	pcdev = kzalloc(sizeof(*pcdev), GFP_KERNEL);
	if (!pcdev) {
		dev_err(&pdev->dev, "Could not allocate pcdev\n");
		err = -ENOMEM;
		goto exit;
	}

	INIT_LIST_HEAD(&pcdev->capture);
	spin_lock_init(&pcdev->lock);

	pcdev->pdata = pdev->dev.platform_data;
	if (!pcdev->pdata) {
		err = -EINVAL;
		dev_err(&pdev->dev, "VIN platform data not set.\n");
		goto exit_kfree;
	}

	base = ioremap_nocache(res->start, resource_size(res));
	if (!base) {
		err = -ENXIO;
		dev_err(&pdev->dev, "Unable to ioremap VIN registers.\n");
		goto exit_kfree;
	}

	pcdev->vinclk = clk_get(&pdev->dev, "vin_clk");
	clk_enable(pcdev->vinclk);

	pcdev->irq = irq;
	pcdev->base = base;
	pcdev->video_limit = 0; /* only enabled if second resource exists */
	pcdev->mb_cnt = 0;
	pcdev->capture_status = STOPPED;
	pcdev->set_pos = 0;
	pcdev->get_pos = 0;
	for (i = 0; i < MB_NUM; i++)
		pcdev->queue_buf[i] = NULL;

#if VIN_SUPPORT_TO_CHECK_REGS
	if (vin_test_of_reg(pcdev, 0) < 0)
		goto exit_iounmap;
#endif /* VIN_SUPPORT_TO_CHECK_REGS */

	/* request irq */
	err = request_irq(pcdev->irq, vin_irq, IRQF_DISABLED,
			  dev_name(&pdev->dev), pcdev);
	if (err) {
		dev_err(&pdev->dev, "Unable to register VIN interrupt.\n");
		goto exit_iounmap;
	}

#ifdef CONFIG_PM
	pm_suspend_ignore_children(&pdev->dev, true);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_resume(&pdev->dev);
#endif /* CONFIG_PM */

	pcdev->ici.priv = pcdev;
	pcdev->ici.v4l2_dev.dev = &pdev->dev;
	pcdev->ici.nr = pdev->id;
	pcdev->ici.drv_name = dev_name(&pdev->dev);
	pcdev->ici.ops = &vin_host_ops;

	pcdev->alloc_ctx = vb2_dma_contig_init_ctx(&pdev->dev);
	if (IS_ERR(pcdev->alloc_ctx)) {
		err = PTR_ERR(pcdev->alloc_ctx);
		goto exit_free_clk;
	}

	err = soc_camera_host_register(&pcdev->ici);
	if (err)
		goto exit_free_ctx;

	return 0;

exit_free_ctx:
	vb2_dma_contig_cleanup_ctx(pcdev->alloc_ctx);
exit_free_clk:
#ifdef CONFIG_PM
	pm_runtime_disable(&pdev->dev);
#endif /* CONFIG_PM */
	free_irq(pcdev->irq, pcdev);
exit_iounmap:
	iounmap(base);
exit_kfree:
	kfree(pcdev);
exit:
	return err;
}

static int __devexit vin_remove(struct platform_device *pdev)
{
	struct soc_camera_host *soc_host = to_soc_camera_host(&pdev->dev);
	struct vin_dev *pcdev = container_of(soc_host,
					struct vin_dev, ici);

	clk_disable(pcdev->vinclk);
	soc_camera_host_unregister(soc_host);
#ifdef CONFIG_PM
	pm_runtime_disable(&pdev->dev);
#endif /* CONFIG_PM */
	free_irq(pcdev->irq, pcdev);
	if (platform_get_resource(pdev, IORESOURCE_MEM, 1))
		dma_release_declared_memory(&pdev->dev);
	iounmap(pcdev->base);
	vb2_dma_contig_cleanup_ctx(pcdev->alloc_ctx);
	kfree(pcdev);

	return 0;
}

#ifdef CONFIG_PM
static int vin_runtime_nop(struct device *dev)
{
	/* Runtime PM callback shared between ->runtime_suspend()
	 * and ->runtime_resume(). Simply returns success.
	 *
	 * This driver re-initializes all registers after
	 * pm_runtime_get_sync() anyway so there is no need
	 * to save and restore registers here.
	 */
	return 0;
}

static const struct dev_pm_ops vin_dev_pm_ops = {
	.runtime_suspend = vin_runtime_nop,
	.runtime_resume = vin_runtime_nop,
};
#endif /* CONFIG_PM */

static struct platform_driver vin_driver = {
	.driver		= {
		.name	= "vin",
#ifdef CONFIG_PM
		.pm	= &vin_dev_pm_ops,
#endif /* CONFIG_PM */
	},
	.probe		= vin_probe,
	.remove		= __devexit_p(vin_remove),
};

module_platform_driver(vin_driver);

MODULE_DESCRIPTION("VIN Unit driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.0.6");
MODULE_ALIAS("platform:vin");
