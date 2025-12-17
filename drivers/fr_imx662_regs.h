/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Framos. All rights reserved.
 *
 * fr_imx662_regs.h - imx662 sensor mode tables
 */

#define STANDBY			0x3000
#define REGHOLD			0x3001
#define XMSTA			0x3002

#define INCK_SEL		0x3014
#define DATARATE_SEL		0x3015
#define WINMODE			0x3018

#define WDMODE			0x301A
#define ADDMODE			0x301B
#define VCMODE			0x301E

#define HREVERSE		0x3020
#define VREVERSE		0x3021
#define ADBIT			0x3022
#define MDBIT			0x3023
#define VMAX_LOW		0x3028
#define VMAX_MID		0x3029
#define VMAX_HIGH		0x302A
#define HMAX_LOW		0x302C
#define HMAX_HIGH		0x302D

#define PIX_HST_LOW		0x303C
#define PIX_HST_HIGH		0x303D
#define PIX_HWIDTH_LOW		0x303E
#define PIX_HWIDTH_HIGH		0x303F

#define LANEMODE		0x3040

#define PIX_VST_LOW		0x3044
#define PIX_VST_HIGH		0x3045
#define PIX_VWIDTH_LOW		0x3046
#define PIX_VWIDTH_HIGH		0x3047

#define SHR0_LOW		0x3050
#define SHR0_MID		0x3051
#define SHR0_HIGH		0x3052

#define GAIN_LOW		0x3070
#define GAIN_HIGH		0x3071

#define XVS_XHS_DRV		0x30A6

#define EXTMODE			0x30CE
#define BLKLEVEL_LOW		0x30DC
#define BLKLEVEL_HIGH		0x30DD

#define TPG_EN_DUOUT		0x30E0
#define TPG_PATSEL_DUOUT	0x30E2
#define TPG_COLORWIDTH		0x30E4
#define TESTCLKEN		0x4900

#define IMX662_DEFAULT_WIDTH		1920
#define IMX662_DEFAULT_HEIGHT		1080
#define IMX662_1280x720_WIDTH		1280
#define IMX662_1280x720_HEIGHT		720
#define IMX662_640x480_WIDTH		640
#define IMX662_640x480_HEIGHT		480
#define IMX662_MODE_BINNING_H2V2_WIDTH	968
#define IMX662_MODE_BINNING_H2V2_HEIGHT	550


struct imx662_reg {
	u16 address;
	u8 val;
};

#define IMX662_MIN_FRAME_LENGTH_DELTA	70

#define IMX662_TO_LOW_BYTE(x) (x & 0xFF)
#define IMX662_TO_MID_BYTE(x) (x >> 8)

static const struct imx662_reg mode_common_regs[] = {

	{LANEMODE,		0x03},
	{INCK_SEL,		0x01},

	{0x3444,		0xAC},
	{0x3460,		0x21},
	{0x3492,		0x08},
	{0x3A50,		0x62},
	{0x3A51,		0x01},
	{0x3A52,		0x19},
	{0x3B00,		0x39},
	{0x3B23,		0x2D},
	{0x3B45,		0x04},
	{0x3C0A,		0x1F},
	{0x3C0B,		0x1E},
	{0x3C38,		0x21},

	{0x3C44,		0x00},
	{0x3CB6,		0xD8},
	{0x3CC4,		0xDA},
	{0x3E24,		0x79},
	{0x3E2C,		0x15},
	{0x3EDC,		0x2D},
	{0x4498,		0x05},
	{0x449C,		0x19},
	{0x449D,		0x00},
	{0x449E,		0x32},
	{0x449F,		0x01},
	{0x44A0,		0x92},
	{0x44A2,		0x91},
	{0x44A4,		0x8C},
	{0x44A6,		0x87},
	{0x44A8,		0x82},
	{0x44AA,		0x78},
	{0x44AC,		0x6E},
	{0x44AE,		0x69},
	{0x44B0,		0x92},
	{0x44B2,		0x91},
	{0x44B4,		0x8C},
	{0x44B6,		0x87},
	{0x44B8,		0x82},
	{0x44BA,		0x78},
	{0x44BC,		0x6E},
	{0x44BE,		0x69},
	{0x44C0,		0x7F},
	{0x44C1,		0x01},
	{0x44C2,		0x7F},
	{0x44C3,		0x01},
	{0x44C4,		0x7A},
	{0x44C5,		0x01},
	{0x44C6,		0x7A},
	{0x44C7,		0x01},
	{0x44C8,		0x70},
	{0x44C9,		0x01},
	{0x44CA,		0x6B},
	{0x44CB,		0x01},
	{0x44CC,		0x6B},
	{0x44CD,		0x01},
	{0x44CE,		0x5C},
	{0x44CF,		0x01},
	{0x44D0,		0x7F},
	{0x44D1,		0x01},
	{0x44D2,		0x7F},
	{0x44D3,		0x01},
	{0x44D4,		0x7A},
	{0x44D5,		0x01},
	{0x44D6,		0x7A},
	{0x44D7,		0x01},
	{0x44D8,		0x70},
	{0x44D9,		0x01},
	{0x44DA,		0x6B},
	{0x44DB,		0x01},
	{0x44DC,		0x6B},
	{0x44DD,		0x01},
	{0x44DE,		0x5C},
	{0x44DF,		0x01},
	{0x4534,		0x1C},
	{0x4535,		0x03},
	{0x4538,		0x1C},
	{0x4539,		0x1C},
	{0x453A,		0x1C},
	{0x453B,		0x1C},
	{0x453C,		0x1C},
	{0x453D,		0x1C},
	{0x453E,		0x1C},
	{0x453F,		0x1C},
	{0x4540,		0x1C},
	{0x4541,		0x03},
	{0x4542,		0x03},
	{0x4543,		0x03},
	{0x4544,		0x03},
	{0x4545,		0x03},
	{0x4546,		0x03},
	{0x4547,		0x03},
	{0x4548,		0x03},
	{0x4549,		0x03},

};

static const struct imx662_reg raw12_framefmt_regs[] = {

	{ADBIT,			0x01},
	{MDBIT,			0x01},

	{0x3A50,		0xFF},
	{0x3A51,		0x03},
	{0x3A52,		0x00},

};

static const struct imx662_reg raw12_h2v2_framefmt_regs[] = {

	{ADBIT,			0x00},
	{MDBIT,			0x01},

	{0x3A50,		0x62},
	{0x3A51,		0x01},
	{0x3A52,		0x19},

};

static const struct imx662_reg raw10_framefmt_regs[] = {

	{ADBIT,			0x00},
	{MDBIT,			0x00},

	{0x3A50,		0x62},
	{0x3A51,		0x01},
	{0x3A52,		0x19},

};

static const struct imx662_reg mode_1920x1080[] = {

	{WINMODE,		0x04},
	{ADDMODE,		0x00},
	{WDMODE,		0x00},
	{VCMODE,		0x01},

	{PIX_HST_HIGH,		IMX662_TO_MID_BYTE(8)},
	{PIX_HST_LOW,		IMX662_TO_LOW_BYTE(8)},
	{PIX_HWIDTH_HIGH,	IMX662_TO_MID_BYTE(IMX662_DEFAULT_WIDTH)},
	{PIX_HWIDTH_LOW,	IMX662_TO_LOW_BYTE(IMX662_DEFAULT_WIDTH)},

	{PIX_VST_HIGH,		IMX662_TO_MID_BYTE(12)},
	{PIX_VST_LOW,		IMX662_TO_LOW_BYTE(12)},
	{PIX_VWIDTH_HIGH,	IMX662_TO_MID_BYTE(IMX662_DEFAULT_HEIGHT)},
	{PIX_VWIDTH_LOW,	IMX662_TO_LOW_BYTE(IMX662_DEFAULT_HEIGHT)},

};

static const struct imx662_reg mode_crop_1280x720[] = {

	{WINMODE,		0x04},
	{ADDMODE,		0x00},
	{WDMODE,		0x00},
	{VCMODE,		0x01},

	{PIX_HST_HIGH,		IMX662_TO_MID_BYTE(328)},
	{PIX_HST_LOW,		IMX662_TO_LOW_BYTE(328)},
	{PIX_HWIDTH_HIGH,	IMX662_TO_MID_BYTE(IMX662_1280x720_WIDTH)},
	{PIX_HWIDTH_LOW,	IMX662_TO_LOW_BYTE(IMX662_1280x720_WIDTH)},

	{PIX_VST_HIGH,		IMX662_TO_MID_BYTE(192)},
	{PIX_VST_LOW,		IMX662_TO_LOW_BYTE(192)},
	{PIX_VWIDTH_HIGH,	IMX662_TO_MID_BYTE(IMX662_1280x720_HEIGHT)},
	{PIX_VWIDTH_LOW,	IMX662_TO_LOW_BYTE(IMX662_1280x720_HEIGHT)},

};

static const struct imx662_reg mode_crop_640x480[] = {

	{WINMODE,		0x04},
	{ADDMODE,		0x00},
	{WDMODE,		0x00},
	{VCMODE,		0x01},

	{PIX_HST_HIGH,		IMX662_TO_MID_BYTE(640)},
	{PIX_HST_LOW,		IMX662_TO_LOW_BYTE(640)},
	{PIX_HWIDTH_HIGH,	IMX662_TO_MID_BYTE(IMX662_640x480_WIDTH)},
	{PIX_HWIDTH_LOW,	IMX662_TO_LOW_BYTE(IMX662_640x480_WIDTH)},

	{PIX_VST_HIGH,		IMX662_TO_MID_BYTE(312)},
	{PIX_VST_LOW,		IMX662_TO_LOW_BYTE(312)},
	{PIX_VWIDTH_HIGH,	IMX662_TO_MID_BYTE(IMX662_640x480_HEIGHT)},
	{PIX_VWIDTH_LOW,	IMX662_TO_LOW_BYTE(IMX662_640x480_HEIGHT)},

};

static const struct imx662_reg mode_h2v2_binning[] = {

	{WINMODE,		0x00},
	{ADDMODE,		0x01},
	{WDMODE,		0x00},
	{VCMODE,		0x01},

};

static const struct imx662_reg mode_enable_pattern_generator[] = {

	{BLKLEVEL_LOW,		0x00},
	{TPG_EN_DUOUT,		0x01},
	{TPG_COLORWIDTH,	0x00},
	{TESTCLKEN,		0x0A},

};

static const struct imx662_reg mode_disable_pattern_generator[] = {

	{BLKLEVEL_LOW,		0x32},
	{TPG_EN_DUOUT,		0x00},
	{TPG_COLORWIDTH,	0x00},
	{TESTCLKEN,		0x02},

};

enum {
	_GMSL_LINK_FREQ_1500,
	_IMX662_LINK_FREQ_720,
	_IMX662_LINK_FREQ_594,
} link_freq;

enum {
	MASTER_MODE,
	SLAVE_MODE,
} operation_mode;

enum {
	NO_SYNC,
	INTERNAL_SYNC,
	EXTERNAL_SYNC,
} sync_mode;
