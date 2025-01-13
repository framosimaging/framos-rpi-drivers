// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Framos. All rights reserved.
 *
 * fr_imx900.c - Framos fr_imx900.c driver
 */

//#define DEBUG 1

#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mediabus.h>

#include "fr_imx900_regs.h"
#include "fr_max96792.h"
#include "fr_max96793.h"

#define IMX900_K_FACTOR				1000LL
#define IMX900_M_FACTOR				1000000LL
#define IMX900_G_FACTOR				1000000000LL
#define IMX900_T_FACTOR				1000000000000LL

#define IMX900_XCLK_FREQ			74250000

#define GMSL_LINK_FREQ_1500			(1500000000/2)
#define IMX900_LINK_FREQ_1485			(1485000000/2)
#define	IMX900_LINK_FREQ_1188			(1188000000/2)
#define IMX900_LINK_FREQ_891			(891000000/2)

#define IMX900_MODE_STANDBY			0x01
#define IMX900_MODE_STREAMING			0x00

#define IMX900_MIN_INTEGRATION_LINES		1

#define IMX900_ANA_GAIN_MIN			0
#define IMX900_ANA_GAIN_MAX			480
#define IMX900_ANA_GAIN_STEP			1
#define IMX900_ANA_GAIN_DEFAULT			0

#define IMX900_BLACK_LEVEL_MIN			0
#define IMX900_BLACK_LEVEL_STEP			1
#define IMX900_MAX_BLACK_LEVEL_8BPP		255
#define IMX900_MAX_BLACK_LEVEL_10BPP		1023
#define IMX900_MAX_BLACK_LEVEL_12BPP		4095
#define IMX900_DEFAULT_BLACK_LEVEL_8BPP		15
#define IMX900_DEFAULT_BLACK_LEVEL_10BPP	60
#define IMX900_DEFAULT_BLACK_LEVEL_12BPP	240

#define IMX900_EMBEDDED_LINE_WIDTH		16384
#define IMX900_NUM_EMBEDDED_LINES		1

enum pad_types {
	IMAGE_PAD,
	METADATA_PAD,
	NUM_PADS
};

#define IMX900_NATIVE_WIDTH		2064U
#define IMX900_NATIVE_HEIGHT		1552U
#define IMX900_PIXEL_ARRAY_LEFT		0U
#define IMX900_PIXEL_ARRAY_TOP		0U
#define IMX900_PIXEL_ARRAY_WIDTH	2064U
#define IMX900_PIXEL_ARRAY_HEIGHT	1552U

#define V4L2_CID_FRAME_RATE		(V4L2_CID_USER_IMX_BASE + 1)
#define V4L2_CID_OPERATION_MODE		(V4L2_CID_USER_IMX_BASE + 2)
#define V4L2_CID_GLOBAL_SHUTTER_MODE	(V4L2_CID_USER_IMX_BASE + 3)

struct imx900_reg_list {

	unsigned int num_of_regs;
	const struct imx900_reg *regs;
};

struct imx900_mode {

	unsigned int width;
	unsigned int height;
	unsigned int pixel_rate;
	unsigned int min_fps;
	unsigned int type;
	struct v4l2_rect crop;
	struct imx900_reg_list reg_list;
	struct imx900_reg_list reg_list_format;
};

static const s64 imx900_link_freq_menu[] = {

	[_GMSL_LINK_FREQ_1500] = GMSL_LINK_FREQ_1500,
	[_IMX900_LINK_FREQ_1485] = IMX900_LINK_FREQ_1485,
	[_IMX900_LINK_FREQ_1188] = IMX900_LINK_FREQ_1188,
	[_IMX900_LINK_FREQ_891] = IMX900_LINK_FREQ_891,
};

static const struct imx900_mode modes_12bit[] = {
	{
		/* All pixel mode */
		.width = IMX900_DEFAULT_WIDTH,
		.height = IMX900_DEFAULT_HEIGHT,
		.type = IMX900_MODE_2064x1552_12BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 0,
			.top = 0,
			.width = IMX900_DEFAULT_WIDTH,
			.height = IMX900_DEFAULT_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_2064x1552),
			.regs = mode_2064x1552,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw12_framefmt_regs),
			.regs = raw12_framefmt_regs,
		},
	},
	{
		/* Crop mode */
		.width = IMX900_ROI_MODE_WIDTH,
		.height = IMX900_ROI_MODE_HEIGHT,
		.type = IMX900_MODE_ROI_1920x1080_12BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 72,
			.top = 240,
			.width = IMX900_ROI_MODE_WIDTH,
			.height = IMX900_ROI_MODE_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1920x1080),
			.regs = mode_1920x1080,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw12_framefmt_regs),
			.regs = raw12_framefmt_regs,
		},
	},
	{
		/* Subsampling 1/2 mode */
		.width = IMX900_SUBSAMPLING2_MODE_WIDTH,
		.height = IMX900_SUBSAMPLING2_MODE_HEIGHT,
		.type = IMX900_MODE_SUB2_1032x776_12BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 0,
			.top = 0,
			.width = IMX900_SUBSAMPLING2_MODE_WIDTH,
			.height = IMX900_SUBSAMPLING2_MODE_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1032x776),
			.regs = mode_1032x776,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw12_framefmt_regs),
			.regs = raw12_framefmt_regs,
		},
	},
	{
		/* Subsampling 1/10 mode */
		.width = IMX900_SUBSAMPLING10_MODE_WIDTH,
		.height = IMX900_SUBSAMPLING10_MODE_HEIGHT,
		.type = IMX900_MODE_SUB10_2064x154_12BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 0,
			.top = 0,
			.width = IMX900_SUBSAMPLING10_MODE_WIDTH,
			.height = IMX900_SUBSAMPLING10_MODE_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_2064x154),
			.regs = mode_2064x154,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw12_framefmt_regs),
			.regs = raw12_framefmt_regs,
		},
	},
	{
		/* Binning crop mode */
		.width = IMX900_BINNING_CROP_MODE_WIDTH,
		.height = IMX900_BINNING_CROP_MODE_HEIGHT,
		.type = IMX900_MODE_BIN_CROP_1024x720_12BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 0,
			.top = 0,
			.width = IMX900_BINNING_CROP_MODE_WIDTH,
			.height = IMX900_BINNING_CROP_MODE_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1024x720),
			.regs = mode_1024x720,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw12_framefmt_regs),
			.regs = raw12_framefmt_regs,
		},
	},
};

static const struct imx900_mode modes_10bit[] = {
	{
		/* All pixel mode */
		.width = IMX900_DEFAULT_WIDTH,
		.height = IMX900_DEFAULT_HEIGHT,
		.type = IMX900_MODE_2064x1552_10BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 0,
			.top = 0,
			.width = IMX900_DEFAULT_WIDTH,
			.height = IMX900_DEFAULT_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_2064x1552),
			.regs = mode_2064x1552,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw10_framefmt_regs),
			.regs = raw10_framefmt_regs,
		},
	},
	{
		/* Crop mode */
		.width = IMX900_ROI_MODE_WIDTH,
		.height = IMX900_ROI_MODE_HEIGHT,
		.type = IMX900_MODE_ROI_1920x1080_10BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 72,
			.top = 240,
			.width = IMX900_ROI_MODE_WIDTH,
			.height = IMX900_ROI_MODE_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1920x1080),
			.regs = mode_1920x1080,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw10_framefmt_regs),
			.regs = raw10_framefmt_regs,
		},
	},
	{
		/* Subsampling 1/2 mode */
		.width = IMX900_SUBSAMPLING2_MODE_WIDTH,
		.height = IMX900_SUBSAMPLING2_MODE_HEIGHT,
		.type = IMX900_MODE_SUB2_1032x776_10BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 0,
			.top = 0,
			.width = IMX900_SUBSAMPLING2_MODE_WIDTH,
			.height = IMX900_SUBSAMPLING2_MODE_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1032x776),
			.regs = mode_1032x776,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw10_framefmt_regs),
			.regs = raw10_framefmt_regs,
		},
	},
	{
		/* Subsampling 1/10 mode */
		.width = IMX900_SUBSAMPLING10_MODE_WIDTH,
		.height = IMX900_SUBSAMPLING10_MODE_HEIGHT,
		.type = IMX900_MODE_SUB10_2064x154_10BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 0,
			.top = 0,
			.width = IMX900_SUBSAMPLING10_MODE_WIDTH,
			.height = IMX900_SUBSAMPLING10_MODE_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_2064x154),
			.regs = mode_2064x154,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw10_framefmt_regs),
			.regs = raw10_framefmt_regs,
		},
	},
	{
		/* Binning crop mode */
		.width = IMX900_BINNING_CROP_MODE_WIDTH,
		.height = IMX900_BINNING_CROP_MODE_HEIGHT,
		.type = IMX900_MODE_BIN_CROP_1024x720_10BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 0,
			.top = 0,
			.width = IMX900_BINNING_CROP_MODE_WIDTH,
			.height = IMX900_BINNING_CROP_MODE_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1024x720),
			.regs = mode_1024x720,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw10_framefmt_regs),
			.regs = raw10_framefmt_regs,
		},
	},
};

static const struct imx900_mode modes_8bit[] = {
	{
		/* All pixel mode */
		.width = IMX900_DEFAULT_WIDTH,
		.height = IMX900_DEFAULT_HEIGHT,
		.type = IMX900_MODE_2064x1552_8BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 0,
			.top = 0,
			.width = IMX900_DEFAULT_WIDTH,
			.height = IMX900_DEFAULT_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_2064x1552),
			.regs = mode_2064x1552,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw8_framefmt_regs),
			.regs = raw8_framefmt_regs,
		},
	},
	{
		/* Crop mode */
		.width = IMX900_ROI_MODE_WIDTH,
		.height = IMX900_ROI_MODE_HEIGHT,
		.type = IMX900_MODE_ROI_1920x1080_8BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 72,
			.top = 240,
			.width = IMX900_ROI_MODE_WIDTH,
			.height = IMX900_ROI_MODE_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1920x1080),
			.regs = mode_1920x1080,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw8_framefmt_regs),
			.regs = raw8_framefmt_regs,
		},
	},
	{
		/* Subsampling 1/2 mode */
		.width = IMX900_SUBSAMPLING2_MODE_WIDTH,
		.height = IMX900_SUBSAMPLING2_MODE_HEIGHT,
		.type = IMX900_MODE_SUB2_1032x776_8BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 0,
			.top = 0,
			.width = IMX900_SUBSAMPLING2_MODE_WIDTH,
			.height = IMX900_SUBSAMPLING2_MODE_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1032x776),
			.regs = mode_1032x776,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw8_framefmt_regs),
			.regs = raw8_framefmt_regs,
		},
	},
	{
		/* Subsampling 1/10 mode */
		.width = IMX900_SUBSAMPLING10_MODE_WIDTH,
		.height = IMX900_SUBSAMPLING10_MODE_HEIGHT,
		.type = IMX900_MODE_SUB10_2064x154_8BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 0,
			.top = 0,
			.width = IMX900_SUBSAMPLING10_MODE_WIDTH,
			.height = IMX900_SUBSAMPLING10_MODE_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_2064x154),
			.regs = mode_2064x154,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw8_framefmt_regs),
			.regs = raw8_framefmt_regs,
		},
	},
	{
		/* Binning crop mode */
		.width = IMX900_BINNING_CROP_MODE_WIDTH,
		.height = IMX900_BINNING_CROP_MODE_HEIGHT,
		.type = IMX900_MODE_BIN_CROP_1024x720_8BPP,
		.min_fps = 1000000,
		.crop = {
			.left = 0,
			.top = 0,
			.width = IMX900_BINNING_CROP_MODE_WIDTH,
			.height = IMX900_BINNING_CROP_MODE_HEIGHT,
		},
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1024x720),
			.regs = mode_1024x720,
		},
		.reg_list_format = {
			.num_of_regs = ARRAY_SIZE(raw8_framefmt_regs),
			.regs = raw8_framefmt_regs,
		},
	},
};

static const u32 codes[] = {

	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,

	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,

	MEDIA_BUS_FMT_SGBRG8_1X8,
	MEDIA_BUS_FMT_SRGGB8_1X8,

};

static const u32 codes_mono[] = {

	MEDIA_BUS_FMT_Y12_1X12,

	MEDIA_BUS_FMT_Y10_1X10,

	MEDIA_BUS_FMT_Y8_1X8

};

struct imx900 {
	struct v4l2_subdev sd;
	struct media_pad pad[NUM_PADS];

	unsigned int fmt_code;

	struct gpio_desc *reset_gpio;
	struct gpio_desc *xmaster;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *framerate;
	struct v4l2_ctrl *operation_mode;
	struct v4l2_ctrl *shutter_mode;
	struct v4l2_ctrl *vflip;
	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *blklvl;

	u8 chromacity;
	u8 linkfreq;
	u64 line_time;
	u32 frame_length;
	u32 min_frame_length_delta;
	u32 min_shs_length;
	u32 hmax;
	u32 pixel_rate_calc;

	const char *gmsl;
	struct device *ser_dev;
	struct device *dser_dev;
	struct gmsl_link_ctx g_ctx;

	const struct imx900_mode *mode;
	struct mutex mutex;
	bool streaming;
};

static inline struct imx900 *to_imx900(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct imx900, sd);
}

static inline void get_mode_table(struct imx900 *imx900,
				  unsigned int code,
				  const struct imx900_mode **mode_list,
				  unsigned int *num_modes)
{
	switch (code) {
	case MEDIA_BUS_FMT_SGBRG12_1X12:
		*mode_list = &modes_12bit[2];
		*num_modes = ARRAY_SIZE(modes_12bit) - 3;
		break;
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		*mode_list = modes_12bit;
		*num_modes = ARRAY_SIZE(modes_12bit) - 3;
		break;
	case MEDIA_BUS_FMT_Y12_1X12:
		*mode_list = modes_12bit;
		*num_modes = ARRAY_SIZE(modes_12bit);
		break;
	case MEDIA_BUS_FMT_SGBRG10_1X10:
		*mode_list = &modes_10bit[2];
		*num_modes = ARRAY_SIZE(modes_10bit) - 3;
		break;
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		*mode_list = modes_10bit;
		*num_modes = ARRAY_SIZE(modes_10bit) - 3;
		break;
	case MEDIA_BUS_FMT_Y10_1X10:
		*mode_list = modes_10bit;
		*num_modes = ARRAY_SIZE(modes_10bit);
		break;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
		*mode_list = &modes_8bit[2];
		*num_modes = ARRAY_SIZE(modes_8bit) - 3;
		break;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		*mode_list = modes_8bit;
		*num_modes = ARRAY_SIZE(modes_8bit) - 3;
		break;
	case MEDIA_BUS_FMT_Y8_1X8:
		*mode_list = modes_8bit;
		*num_modes = ARRAY_SIZE(modes_8bit);
		break;
	default:
		*mode_list = NULL;
		*num_modes = 0;
	}

}

static const char * const imx900_test_pattern_menu[] = {

	[0] = "Disabled",
	[1] = "Sequence Pattern 1",
	[2] = "Sequence Pattern 2",
	[3] = "Gradation Pattern",
	[4] = "Color Bar Horizontally",
	[5] = "Color Bar Vertically",

};

static const char * const imx900_operation_mode_menu[] = {

	[MASTER_MODE] = "Master Mode",
	[SLAVE_MODE] = "Slave Mode",

};

static const char * const imx900_global_shutter_menu[] = {

	[NORMAL_MODE] = "Normal Mode",
	[SEQUENTIAL_TRIGGER_MODE] = "Sequential Trigger Mode",
	[FAST_TRIGGER_MODE] = "Fast Trigger Mode",

};

static int imx900_read_reg(struct imx900 *imx900, u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct i2c_msg msgs[2];
	u8 addr_buf[2] = { reg >> 8, reg & 0xff };
	u8 data_buf[4] = { 0, };
	int ret;

	if (len > 4)
		return -EINVAL;

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = ARRAY_SIZE(addr_buf);
	msgs[0].buf = addr_buf;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_buf[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = get_unaligned_be32(data_buf);

	return 0;
}

static int imx900_write_reg(struct imx900 *imx900, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	u8 buf[6];

	if (len > 4)
		return -EINVAL;

	put_unaligned_be16(reg, buf);
	put_unaligned_le32(val, buf + 2);

	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

static int imx900_write_hold_reg(struct imx900 *imx900, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	int ret;

	ret = imx900_write_reg(imx900, REGHOLD, 1, 0x01);
	if (ret) {
		dev_err(dev, "%s failed to write reghold register\n", __func__);
		return ret;
	}

	ret = imx900_write_reg(imx900, reg, len, val);
	if (ret)
		goto reghold_off;

	ret = imx900_write_reg(imx900, REGHOLD, 1, 0x00);
	if (ret) {
		dev_err(dev, "%s failed to write reghold register\n", __func__);
		return ret;
	}

	return 0;

reghold_off:
	ret = imx900_write_reg(imx900, REGHOLD, 1, 0x00);
	if (ret) {
		dev_err(dev, "%s failed to write reghold register\n", __func__);
		return ret;
	}
	return ret;

}

static int imx900_write_table(struct imx900 *imx900,
				 const struct imx900_reg *regs, u32 len)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = imx900_write_reg(imx900, regs[i].address, 1, regs[i].val);
		if (ret) {
			dev_err_ratelimited(&client->dev,
					"Failed to write reg 0x%4.4x. error = %d\n",
					regs[i].address, ret);

			return ret;
		}
	}

	return 0;
}

static u32 imx900_get_format_code(struct imx900 *imx900, u32 code)
{
	unsigned int i;

	lockdep_assert_held(&imx900->mutex);

	if (imx900->chromacity == IMX900_COLOR) {
		for (i = 0; i < ARRAY_SIZE(codes); i++)
			if (codes[i] == code)
				break;

		if (i >= ARRAY_SIZE(codes))
			i = 0;

		return codes[i];
	}

	for (i = 0; i < ARRAY_SIZE(codes_mono); i++)
		if (codes_mono[i] == code)
			break;

	if (i >= ARRAY_SIZE(codes_mono))
		i = 0;

	return codes_mono[i];

}

static int imx900_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx900 *imx900 = to_imx900(sd);
	struct v4l2_mbus_framefmt *try_fmt_img =
		v4l2_subdev_get_try_format(sd, fh->state, IMAGE_PAD);
	struct v4l2_mbus_framefmt *try_fmt_meta =
		v4l2_subdev_get_try_format(sd, fh->state, METADATA_PAD);
	struct v4l2_rect *try_crop;

	mutex_lock(&imx900->mutex);

	try_fmt_img->width = modes_12bit[0].width;
	try_fmt_img->height = modes_12bit[0].height;
	if (imx900->chromacity == IMX900_COLOR) {
		try_fmt_img->code = imx900_get_format_code(imx900,
						MEDIA_BUS_FMT_SRGGB12_1X12);
	} else {
		try_fmt_img->code = imx900_get_format_code(imx900,
						MEDIA_BUS_FMT_Y12_1X12);
	}
	try_fmt_img->field = V4L2_FIELD_NONE;

	try_fmt_meta->width = IMX900_EMBEDDED_LINE_WIDTH;
	try_fmt_meta->height = IMX900_NUM_EMBEDDED_LINES;
	try_fmt_meta->code = MEDIA_BUS_FMT_SENSOR_DATA;
	try_fmt_meta->field = V4L2_FIELD_NONE;

	try_crop = v4l2_subdev_get_try_crop(sd, fh->state, IMAGE_PAD);
	try_crop->left = IMX900_PIXEL_ARRAY_LEFT;
	try_crop->top = IMX900_PIXEL_ARRAY_TOP;
	try_crop->width = IMX900_PIXEL_ARRAY_WIDTH;
	try_crop->height = IMX900_PIXEL_ARRAY_HEIGHT;

	mutex_unlock(&imx900->mutex);

	return 0;
}

static int imx900_chromacity_mode(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	int ret;
	u32 chromacity;

	ret = imx900_write_reg(imx900, STANDBY, 1, 0x00);
	if (ret) {
		dev_err(dev, "%s: error canceling standby mode\n", __func__);
		return ret;
	}

	usleep_range(15000, 20000);

	ret = imx900_read_reg(imx900, CHROMACITY, 1, &chromacity);
	if (ret) {
		dev_err(dev, "%s: error reading chromacity information register\n",
									__func__);
		return ret;
	}

	ret = imx900_write_reg(imx900, STANDBY, 1, 0x01);
	if (ret) {
		dev_err(dev, "%s: error setting standby mode\n", __func__);
		return ret;
	}

	chromacity = chromacity >> 7;
	imx900->chromacity = chromacity;

	dev_dbg(dev, "%s: sensor is color(0)/monochrome(1): %d\n",
							__func__, chromacity);

	return ret;
}

static int imx900_set_exposure(struct imx900 *imx900, u64 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	const struct imx900_mode *mode = imx900->mode;
	u64 exposure;
	int ret;

	exposure = imx900->vblank->val + mode->height - val;

	ret = imx900_write_hold_reg(imx900, SHS_LOW, 3, exposure);
	if (ret) {
		dev_err(dev, "%s failed to set exposure\n", __func__);
		return ret;
	}

	return ret;
}

static void imx900_adjust_min_shs_length(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	const struct imx900_mode *mode = imx900->mode;

	switch (mode->type) {
	case IMX900_MODE_2064x1552_12BPP:
	case IMX900_MODE_ROI_1920x1080_12BPP:
	case IMX900_MODE_SUB10_2064x154_12BPP:
		imx900->min_shs_length = 51;
		break;
	case IMX900_MODE_SUB2_1032x776_12BPP:
	case IMX900_MODE_BIN_CROP_1024x720_12BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->min_shs_length = 51;
		else
			imx900->min_shs_length = 102;
		break;
	case IMX900_MODE_2064x1552_10BPP:
	case IMX900_MODE_SUB10_2064x154_10BPP:
		imx900->min_shs_length = 62;
		break;
	case IMX900_MODE_ROI_1920x1080_10BPP:
		imx900->min_shs_length = 82;
		break;
	case IMX900_MODE_SUB2_1032x776_10BPP:
	case IMX900_MODE_BIN_CROP_1024x720_10BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->min_shs_length = 85;
		else
			imx900->min_shs_length = 142;
		break;
	case IMX900_MODE_2064x1552_8BPP:
	case IMX900_MODE_ROI_1920x1080_8BPP:
	case IMX900_MODE_SUB10_2064x154_8BPP:
		imx900->min_shs_length = 75;
		break;
	case IMX900_MODE_SUB2_1032x776_8BPP:
	case IMX900_MODE_BIN_CROP_1024x720_8BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->min_shs_length = 92;
		else
			imx900->min_shs_length = 128;
		break;
	}

	dev_dbg(dev, "%s: adjusted min_shs_length: %d\n", __func__,
							imx900->min_shs_length);
}

static void imx900_adjust_exposure_range(struct imx900 *imx900)
{
	const struct imx900_mode *mode = imx900->mode;
	u64 exposure_max;

	imx900_adjust_min_shs_length(imx900);
	exposure_max = imx900->vblank->val + mode->height - imx900->min_shs_length;

	__v4l2_ctrl_modify_range(imx900->exposure, IMX900_MIN_INTEGRATION_LINES,
				exposure_max, 1,
				exposure_max);
}

static int imx900_set_frame_rate(struct imx900 *imx900, u64 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	int ret;

	ret = imx900_write_hold_reg(imx900, VMAX_LOW, 3, imx900->frame_length);

	if (ret) {
		dev_err(dev, "%s failed to set frame rate\n", __func__);
		return ret;
	}

	return ret;

}

static void imx900_update_frame_rate(struct imx900 *imx900, u64 val)
{

	const struct imx900_mode *mode = imx900->mode;
	u32 update_vblank;

	imx900->frame_length = (IMX900_M_FACTOR * IMX900_G_FACTOR) /
						(val * imx900->line_time);

	update_vblank = imx900->frame_length - mode->height;

	__v4l2_ctrl_modify_range(imx900->vblank, update_vblank,
				 update_vblank, 1, update_vblank);

	__v4l2_ctrl_s_ctrl(imx900->vblank, update_vblank);

}

static void imx900_adjust_hmax_register(struct imx900 *imx900)
{
	const struct imx900_mode *mode = imx900->mode;

	switch (mode->type) {
	case IMX900_MODE_2064x1552_12BPP:
	case IMX900_MODE_ROI_1920x1080_12BPP:
	case IMX900_MODE_SUB10_2064x154_12BPP:
		imx900->hmax = 0x262;
		break;
	case IMX900_MODE_SUB2_1032x776_12BPP:
	case IMX900_MODE_BIN_CROP_1024x720_12BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->hmax = 0x262;
		else
			imx900->hmax = 0x131;
		break;
	case IMX900_MODE_2064x1552_10BPP:
	case IMX900_MODE_SUB10_2064x154_10BPP:
		imx900->hmax = 0x1F3;
		break;
	case IMX900_MODE_ROI_1920x1080_10BPP:
		imx900->hmax = 0x17A;
		break;
	case IMX900_MODE_SUB2_1032x776_10BPP:
	case IMX900_MODE_BIN_CROP_1024x720_10BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->hmax = 0x16C;
		else
			imx900->hmax = 0xD8;
		break;
	case IMX900_MODE_2064x1552_8BPP:
	case IMX900_MODE_ROI_1920x1080_8BPP:
	case IMX900_MODE_SUB10_2064x154_8BPP:
		imx900->hmax = 0x19C;
		break;
	case IMX900_MODE_SUB2_1032x776_8BPP:
	case IMX900_MODE_BIN_CROP_1024x720_8BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->hmax = 0x152;
		else
			imx900->hmax = 0xF0;
		break;
	}
}

static void imx900_adjust_pixel_rate(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	const struct imx900_mode *mode = imx900->mode;
	struct device *dev = &client->dev;

	switch (mode->type) {
	case IMX900_MODE_2064x1552_12BPP:
	case IMX900_MODE_SUB10_2064x154_12BPP:
		imx900->pixel_rate_calc = 251232786;
		break;
	case IMX900_MODE_ROI_1920x1080_12BPP:
		imx900->pixel_rate_calc = 233704918;
		break;
	case IMX900_MODE_SUB2_1032x776_12BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->pixel_rate_calc = 125616393;
		else
			imx900->pixel_rate_calc = 251232787;
		break;
	case IMX900_MODE_BIN_CROP_1024x720_12BPP:
		imx900->pixel_rate_calc = 249285246;
		break;
	case IMX900_MODE_2064x1552_10BPP:
	case IMX900_MODE_SUB10_2064x154_10BPP:
		imx900->pixel_rate_calc = 307118236;
		break;
	case IMX900_MODE_ROI_1920x1080_10BPP:
		imx900->pixel_rate_calc = 377142857;
		break;
	case IMX900_MODE_SUB2_1032x776_10BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->pixel_rate_calc = 210510989;
		else
			imx900->pixel_rate_calc = 354750000;
		break;
	case IMX900_MODE_BIN_CROP_1024x720_10BPP:
		imx900->pixel_rate_calc = 352000000;
		break;
	case IMX900_MODE_2064x1552_8BPP:
	case IMX900_MODE_SUB10_2064x154_8BPP:
		imx900->pixel_rate_calc = 371970874;
		break;
	case IMX900_MODE_ROI_1920x1080_8BPP:
		imx900->pixel_rate_calc = 346019417;
		break;
	case IMX900_MODE_SUB2_1032x776_8BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->pixel_rate_calc = 226704142;
		else
			imx900->pixel_rate_calc = 319275000;
		break;
	case IMX900_MODE_BIN_CROP_1024x720_8BPP:
		imx900->pixel_rate_calc = 316800000;
		break;
	}

	__v4l2_ctrl_modify_range(imx900->pixel_rate,
				 imx900->pixel_rate_calc,
				 imx900->pixel_rate_calc,
				 1, imx900->pixel_rate_calc);

	dev_dbg(dev, "%s: pixel rate: %d\n", __func__, imx900->pixel_rate_calc);

}

static int imx900_set_hmax_register(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	int ret;

	ret = imx900_write_hold_reg(imx900, HMAX_LOW, 2, imx900->hmax);
	if (ret)
		dev_err(dev, "%s failed to write HMAX register\n", __func__);

	dev_dbg(dev, "%s: hmax: 0x%x\n", __func__, imx900->hmax);

	return ret;

}

static void imx900_adjust_link_frequency(struct imx900 *imx900)
{

	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;

	switch (imx900->mode->type) {
	case IMX900_MODE_2064x1552_12BPP:
	case IMX900_MODE_ROI_1920x1080_12BPP:
	case IMX900_MODE_SUB2_1032x776_12BPP:
	case IMX900_MODE_SUB10_2064x154_12BPP:
	case IMX900_MODE_BIN_CROP_1024x720_12BPP:
		imx900->linkfreq = _IMX900_LINK_FREQ_1485;
		break;
	case IMX900_MODE_ROI_1920x1080_10BPP:
	case IMX900_MODE_BIN_CROP_1024x720_10BPP:
		imx900->linkfreq = _IMX900_LINK_FREQ_1188;
		break;
	case IMX900_MODE_SUB2_1032x776_10BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->linkfreq = _IMX900_LINK_FREQ_1485;
		else
			imx900->linkfreq = _IMX900_LINK_FREQ_1188;
		break;
	case IMX900_MODE_2064x1552_10BPP:
	case IMX900_MODE_SUB10_2064x154_10BPP:
	case IMX900_MODE_2064x1552_8BPP:
	case IMX900_MODE_ROI_1920x1080_8BPP:
	case IMX900_MODE_SUB10_2064x154_8BPP:
	case IMX900_MODE_BIN_CROP_1024x720_8BPP:
		imx900->linkfreq = _IMX900_LINK_FREQ_891;
		break;
	case IMX900_MODE_SUB2_1032x776_8BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->linkfreq = _IMX900_LINK_FREQ_1485;
		else
			imx900->linkfreq = _IMX900_LINK_FREQ_891;
		break;
	}

	if (!(strcmp(imx900->gmsl, "gmsl")))
		__v4l2_ctrl_s_ctrl(imx900->link_freq, _GMSL_LINK_FREQ_1500);
	else
		__v4l2_ctrl_s_ctrl(imx900->link_freq, imx900->linkfreq);

	dev_dbg(dev, "%s: linkfreq: %lld\n", __func__,
					imx900_link_freq_menu[imx900->linkfreq]);

}

static int imx900_set_data_rate(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	int ret;

	switch (imx900->linkfreq) {
	case _IMX900_LINK_FREQ_1485:
		ret = imx900_write_table(imx900, imx900_1485_mbps,
				ARRAY_SIZE(imx900_1485_mbps));
		if (ret) {
			dev_err(dev, "%s failed to write datarate reg.\n",
								__func__);
			return ret;
		}
		break;
	case _IMX900_LINK_FREQ_1188:
		ret = imx900_write_table(imx900, imx900_1188_mbps,
				ARRAY_SIZE(imx900_1188_mbps));
		if (ret) {
			dev_err(dev, "%s failed to write datarate reg.\n",
								__func__);
			return ret;
		}
		break;
	case _IMX900_LINK_FREQ_891:
		ret = imx900_write_table(imx900, imx900_891_mbps,
				ARRAY_SIZE(imx900_891_mbps));
		if (ret) {
			dev_err(dev, "%s failed to write datarate reg.\n",
								__func__);
			return ret;
		}
		break;
	default:
		dev_err(dev, "%s datarate reg not set!\n", __func__);
		return 1;
	}

	return ret;
}

static void imx900_adjust_min_frame_length_delta(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	const struct imx900_mode *mode = imx900->mode;

	switch (mode->type) {
	case IMX900_MODE_2064x1552_12BPP:
	case IMX900_MODE_ROI_1920x1080_12BPP:
		imx900->min_frame_length_delta = 137;
		break;
	case IMX900_MODE_SUB2_1032x776_12BPP:
	case IMX900_MODE_BIN_CROP_1024x720_12BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->min_frame_length_delta = 115;
		else
			imx900->min_frame_length_delta = 200;
		break;
	case IMX900_MODE_SUB10_2064x154_12BPP:
		imx900->min_frame_length_delta = 115;
		break;
	case IMX900_MODE_2064x1552_10BPP:
		imx900->min_frame_length_delta = 155;
		break;
	case IMX900_MODE_ROI_1920x1080_10BPP:
		imx900->min_frame_length_delta = 186;
		break;
	case IMX900_MODE_SUB2_1032x776_10BPP:
	case IMX900_MODE_BIN_CROP_1024x720_10BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->min_frame_length_delta = 169;
		else
			imx900->min_frame_length_delta = 264;
		break;
	case IMX900_MODE_SUB10_2064x154_10BPP:
		imx900->min_frame_length_delta = 133;
		break;
	case IMX900_MODE_2064x1552_8BPP:
	case IMX900_MODE_ROI_1920x1080_8BPP:
		imx900->min_frame_length_delta = 175;
		break;
	case IMX900_MODE_SUB2_1032x776_8BPP:
	case IMX900_MODE_BIN_CROP_1024x720_8BPP:
		if (imx900->chromacity == IMX900_COLOR)
			imx900->min_frame_length_delta = 181;
		else
			imx900->min_frame_length_delta = 242;
		break;
	case IMX900_MODE_SUB10_2064x154_8BPP:
		imx900->min_frame_length_delta = 153;
		break;
	}

	dev_dbg(dev, "%s: adjusted min_frame_length_delta: %d\n", __func__,
						imx900->min_frame_length_delta);

	__v4l2_ctrl_modify_range(imx900->vblank,
				 imx900->min_frame_length_delta,
				 imx900->min_frame_length_delta,
				 1, imx900->min_frame_length_delta);

	dev_dbg(dev, "%s: vblank: %d\n", __func__, imx900->min_frame_length_delta);

}

static int imx900_set_mode_additional(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	const struct imx900_mode *mode = imx900->mode;
	int ret;

	switch (mode->type) {
	case IMX900_MODE_2064x1552_12BPP:
	case IMX900_MODE_2064x1552_10BPP:
	case IMX900_MODE_2064x1552_8BPP:
	case IMX900_MODE_ROI_1920x1080_12BPP:
	case IMX900_MODE_ROI_1920x1080_10BPP:
	case IMX900_MODE_ROI_1920x1080_8BPP:
		ret = imx900_write_table(imx900, mode_allPixel_roi,
			ARRAY_SIZE(mode_allPixel_roi));
		break;
	case IMX900_MODE_SUB2_1032x776_12BPP:
	case IMX900_MODE_SUB2_1032x776_10BPP:
	case IMX900_MODE_SUB2_1032x776_8BPP:
	case IMX900_MODE_BIN_CROP_1024x720_12BPP:
	case IMX900_MODE_BIN_CROP_1024x720_10BPP:
	case IMX900_MODE_BIN_CROP_1024x720_8BPP:
		if (imx900->chromacity == IMX900_COLOR)
			ret = imx900_write_table(imx900, mode_subg2_color,
				ARRAY_SIZE(mode_subg2_color));
		else
			ret = imx900_write_table(imx900, mode_sub2_binning_mono,
				ARRAY_SIZE(mode_sub2_binning_mono));
		break;
	case IMX900_MODE_SUB10_2064x154_12BPP:
	case IMX900_MODE_SUB10_2064x154_10BPP:
	case IMX900_MODE_SUB10_2064x154_8BPP:
		ret = imx900_write_table(imx900, mode_sub10,
			ARRAY_SIZE(mode_sub10));
		break;
	}

	if (ret) {
		dev_err(dev, "%s error setting mode additional table\n",
								__func__);
		return ret;
	}

	return ret;
}

static int imx900_set_dep_registers(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	const struct imx900_mode *mode = imx900->mode;
	int ret;

	switch (mode->type) {
	case IMX900_MODE_2064x1552_12BPP:
	case IMX900_MODE_ROI_1920x1080_12BPP:
	case IMX900_MODE_SUB10_2064x154_12BPP:
		ret = imx900_write_table(imx900, allpix_roi_sub10_1485MBPS_1x12_4lane,
			ARRAY_SIZE(allpix_roi_sub10_1485MBPS_1x12_4lane));
		break;
	case IMX900_MODE_SUB2_1032x776_12BPP:
	case IMX900_MODE_BIN_CROP_1024x720_12BPP:
		if (imx900->chromacity == IMX900_COLOR)
			ret = imx900_write_table(imx900, sub2_color_1485MBPS_1x12_4lane,
				ARRAY_SIZE(sub2_color_1485MBPS_1x12_4lane));
		else
			ret = imx900_write_table(imx900, sub2_binning_mono_1485MBPS_1x12_4lane,
				ARRAY_SIZE(sub2_binning_mono_1485MBPS_1x12_4lane));
		break;
	case IMX900_MODE_2064x1552_10BPP:
	case IMX900_MODE_SUB10_2064x154_10BPP:
		ret = imx900_write_table(imx900, allpix_roi_sub10_891MBPS_1x10_4lane,
			ARRAY_SIZE(allpix_roi_sub10_891MBPS_1x10_4lane));
		break;
	case IMX900_MODE_ROI_1920x1080_10BPP:
		ret = imx900_write_table(imx900, allpix_roi_sub10_1188MBPS_1x10_4lane,
			ARRAY_SIZE(allpix_roi_sub10_1188MBPS_1x10_4lane));
		break;
	case IMX900_MODE_SUB2_1032x776_10BPP:
	case IMX900_MODE_BIN_CROP_1024x720_10BPP:
		if (imx900->chromacity == IMX900_COLOR)
			ret = imx900_write_table(imx900, sub2_color_1485MBPS_1x10_4lane,
				ARRAY_SIZE(sub2_color_1485MBPS_1x10_4lane));
		else
			ret = imx900_write_table(imx900, sub2_binning_mono_1188MBPS_1x10_4lane,
				ARRAY_SIZE(sub2_binning_mono_1188MBPS_1x10_4lane));
		break;
	case IMX900_MODE_2064x1552_8BPP:
	case IMX900_MODE_ROI_1920x1080_8BPP:
	case IMX900_MODE_SUB10_2064x154_8BPP:
		ret = imx900_write_table(imx900, allpix_roi_sub10_891MBPS_1x8_4lane,
			ARRAY_SIZE(allpix_roi_sub10_891MBPS_1x8_4lane));
		break;
	case IMX900_MODE_SUB2_1032x776_8BPP:
	case IMX900_MODE_BIN_CROP_1024x720_8BPP:
		if (imx900->chromacity == IMX900_COLOR)
			ret = imx900_write_table(imx900, sub2_color_1485MBPS_1x8_4lane,
				ARRAY_SIZE(sub2_color_1485MBPS_1x8_4lane));
		else
			ret = imx900_write_table(imx900, sub2_binning_mono_891MBPS_1x8_4lane,
				ARRAY_SIZE(sub2_binning_mono_891MBPS_1x8_4lane));
		break;
	}

	if (ret) {
		dev_err(dev, "%s error setting dep register table\n", __func__);
		return ret;
	}

	return ret;
}

static int imx900_set_test_pattern(struct imx900 *imx900, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	int ret;

	if (val) {
		ret = imx900_write_reg(imx900, 0x3550, 1, 0x07);
		if (ret)
			goto fail;
		if (val == 4) {
			ret = imx900_write_reg(imx900, 0x3551, 1, 0x0A);
			if (ret)
				goto fail;
		} else if (val == 5) {
			ret = imx900_write_reg(imx900, 0x3551, 1, 0x0B);
			if (ret)
				goto fail;
		} else {
			ret = imx900_write_reg(imx900, 0x3551, 1, (u8)(val));
			if (ret)
				goto fail;
		}
	} else {
		ret = imx900_write_reg(imx900, 0x3550, 1, 0x06);
		if (ret)
			goto fail;
	}

	return 0;

fail:
	dev_err(dev, "%s: error setting test pattern\n", __func__);
	return ret;

}

static void imx900_update_blklvl_range(struct imx900 *imx900)
{
	switch (imx900->fmt_code) {
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_Y12_1X12:
		__v4l2_ctrl_modify_range(imx900->blklvl, IMX900_BLACK_LEVEL_MIN,
					IMX900_MAX_BLACK_LEVEL_12BPP,
					IMX900_BLACK_LEVEL_STEP,
					IMX900_DEFAULT_BLACK_LEVEL_12BPP);
		__v4l2_ctrl_s_ctrl(imx900->blklvl, IMX900_DEFAULT_BLACK_LEVEL_12BPP);
		break;
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_Y10_1X10:
		__v4l2_ctrl_modify_range(imx900->blklvl, IMX900_BLACK_LEVEL_MIN,
					IMX900_MAX_BLACK_LEVEL_10BPP,
					IMX900_BLACK_LEVEL_STEP,
					IMX900_DEFAULT_BLACK_LEVEL_10BPP);
		__v4l2_ctrl_s_ctrl(imx900->blklvl, IMX900_DEFAULT_BLACK_LEVEL_10BPP);
		break;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_Y8_1X8:
		__v4l2_ctrl_modify_range(imx900->blklvl, IMX900_BLACK_LEVEL_MIN,
					IMX900_MAX_BLACK_LEVEL_8BPP,
					IMX900_BLACK_LEVEL_STEP,
					IMX900_DEFAULT_BLACK_LEVEL_8BPP);
		__v4l2_ctrl_s_ctrl(imx900->blklvl, IMX900_DEFAULT_BLACK_LEVEL_8BPP);
		break;
	}

}

static int imx900_set_blklvl(struct imx900 *imx900, u64 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	int ret;

	ret = imx900_write_hold_reg(imx900, BLKLEVEL_LOW, 2, val);

	if (ret) {
		dev_err(dev, "%s failed to adjust blklvl register\n",
								__func__);
	}

	dev_dbg(dev, "%s: blklvl value: %lld\n", __func__, val);

	return ret;
}

static int imx900_set_operation_mode(struct imx900 *imx900, u32 val)
{
	gpiod_set_raw_value_cansleep(imx900->xmaster, val);

	return 0;
}

static int imx900_set_pixel_format(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	int ret;
	u8 adbit_monosel;

	switch (imx900->fmt_code) {
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_Y8_1X8:
		adbit_monosel = (imx900->chromacity == IMX900_COLOR) ? 0x21 : 0x25;
		ret = imx900_write_reg(imx900, ADBIT_MONOSEL, 1, adbit_monosel);
		if (ret) {
			dev_err(dev, "%s: error setting chromacity pixel format\n",
									__func__);
		return ret;
		}
		break;
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_Y10_1X10:
		adbit_monosel = (imx900->chromacity == IMX900_COLOR) ? 0x01 : 0x05;
		ret = imx900_write_reg(imx900, ADBIT_MONOSEL, 1, adbit_monosel);
		if (ret) {
			dev_err(dev, "%s: error setting chromacity pixel format\n",
									__func__);
		return ret;
		}
		break;
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_Y12_1X12:
		adbit_monosel = (imx900->chromacity == IMX900_COLOR) ? 0x11 : 0x15;
		ret = imx900_write_reg(imx900, ADBIT_MONOSEL, 1, adbit_monosel);
		if (ret) {
			dev_err(dev, "%s: error setting chromacity pixel format\n",
									__func__);
		return ret;
		}
		break;
	default:
	dev_err(dev, "%s: unknown pixel format\n", __func__);
		return -EINVAL;
	}

	dev_dbg(dev, "%s: Sensor pixel format value: 0x%x\n", __func__,
								adbit_monosel);

	return ret;
}

static int imx900_set_shutter_mode(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	const struct imx900_mode *mode = imx900->mode;
	int ret = 0;
	u8 trigen = 0;
	u8 vint_en = 0;

	switch (imx900->operation_mode->val) {
	case MASTER_MODE:
		if (imx900->shutter_mode->val == NORMAL_MODE) {
			trigen = 0;
			vint_en = 2;
			dev_dbg(dev, "%s: Sensor is in Normal Exposure Mode\n",
									__func__);
		} else if (imx900->shutter_mode->val == FAST_TRIGGER_MODE) {
			trigen = 10;
			dev_dbg(dev, "%s: Sensor is in Fast Trigger Mode\n",
									__func__);
		} else {
			dev_warn(dev, "%s: Sequential Trigger Mode not supported in Master mode, switchig to default\n",
									__func__);
			imx900->shutter_mode->cur.val = NORMAL_MODE;
		}
		break;
	case SLAVE_MODE:
		if (imx900->shutter_mode->val == NORMAL_MODE) {
			trigen = 0;
			vint_en = 2;
			dev_dbg(dev, "%s: Sensor is in Normal Exposure Mode\n",
									__func__);
		} else if (imx900->shutter_mode->val == SEQUENTIAL_TRIGGER_MODE) {
			trigen = 9;
			vint_en = 1;
			dev_dbg(dev, "%s: Sensor is in Sequential Trigger Mode\n",
									__func__);
		} else {
			dev_warn(dev,
				"%s: Fast Trigger Mode not supported in Slave mode, switchig to default\n",
									__func__);
			imx900->shutter_mode->cur.val = NORMAL_MODE;
		}
		break;
	default:
		dev_err(dev, "%s: unknown Shutter mode.\n", __func__);
		return -EINVAL;
	}

	switch (mode->type) {
	case IMX900_MODE_2064x1552_12BPP:
	case IMX900_MODE_2064x1552_10BPP:
	case IMX900_MODE_2064x1552_8BPP:
		vint_en |= 0x1C;
		break;
	case IMX900_MODE_ROI_1920x1080_12BPP:
	case IMX900_MODE_ROI_1920x1080_10BPP:
	case IMX900_MODE_ROI_1920x1080_8BPP:
		vint_en |= 0x1C;
		break;
	case IMX900_MODE_SUB2_1032x776_12BPP:
	case IMX900_MODE_SUB2_1032x776_10BPP:
	case IMX900_MODE_SUB2_1032x776_8BPP:
		vint_en |= (imx900->chromacity == IMX900_COLOR) ? 0x14 : 0x18;
		break;
	case IMX900_MODE_SUB10_2064x154_12BPP:
	case IMX900_MODE_SUB10_2064x154_10BPP:
	case IMX900_MODE_SUB10_2064x154_8BPP:
		vint_en |= 0x14;
		break;
	case IMX900_MODE_BIN_CROP_1024x720_12BPP:
	case IMX900_MODE_BIN_CROP_1024x720_10BPP:
	case IMX900_MODE_BIN_CROP_1024x720_8BPP:
		vint_en |= 0x18;
		break;
	}

	ret = imx900_write_reg(imx900, TRIGMODE, 1, trigen);
	ret |= imx900_write_reg(imx900, VINT_EN, 1, vint_en);
	if (ret) {
		dev_err(dev, "%s: error setting Shutter mode\n", __func__);
		return ret;
	}

	return 0;
}

static int imx900_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx900 *imx900 =
		container_of(ctrl->handler, struct imx900, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_FRAME_RATE:
		imx900_update_frame_rate(imx900, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		imx900_adjust_exposure_range(imx900);
		break;
	}

	if (pm_runtime_get_if_in_use(&client->dev) == 0)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret = imx900_write_hold_reg(imx900, GAIN_LOW, 2, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret = imx900_set_exposure(imx900, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		imx900_set_test_pattern(imx900, ctrl->val);
		break;
	case V4L2_CID_FRAME_RATE:
		ret = imx900_set_frame_rate(imx900, ctrl->val);
		break;
	case V4L2_CID_BLACK_LEVEL:
		ret = imx900_set_blklvl(imx900, ctrl->val);
		break;
	case V4L2_CID_OPERATION_MODE:
		ret = imx900_set_operation_mode(imx900, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx900_ctrl_ops = {
	.s_ctrl = imx900_set_ctrl,
};

static int imx900_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx900 *imx900 = to_imx900(sd);

	if (code->pad >= NUM_PADS)
		return -EINVAL;

	if (code->pad == IMAGE_PAD) {
		if (imx900->chromacity == IMX900_COLOR) {
			if (code->index >= (ARRAY_SIZE(codes)))
				return -EINVAL;

			code->code = imx900_get_format_code(imx900,
							codes[code->index]);
		} else {
			if (code->index >= (ARRAY_SIZE(codes_mono)))
				return -EINVAL;

			code->code = imx900_get_format_code(imx900,
							codes_mono[code->index]);
		}
	} else {
		if (code->index > 0)
			return -EINVAL;

		code->code = MEDIA_BUS_FMT_SENSOR_DATA;
	}

	return 0;
}

static int imx900_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx900 *imx900 = to_imx900(sd);

	if (fse->pad >= NUM_PADS)
		return -EINVAL;

	if (fse->pad == IMAGE_PAD) {
		const struct imx900_mode *mode_list;
		unsigned int num_modes;

		get_mode_table(imx900, fse->code, &mode_list, &num_modes);

		if (fse->index >= num_modes)
			return -EINVAL;

		if (fse->code != imx900_get_format_code(imx900, fse->code))
			return -EINVAL;

		fse->min_width = mode_list[fse->index].width;
		fse->max_width = fse->min_width;
		fse->min_height = mode_list[fse->index].height;
		fse->max_height = fse->min_height;
	} else {
		if (fse->code != MEDIA_BUS_FMT_SENSOR_DATA || fse->index > 0)
			return -EINVAL;

		fse->min_width = IMX900_EMBEDDED_LINE_WIDTH;
		fse->max_width = fse->min_width;
		fse->min_height = IMX900_NUM_EMBEDDED_LINES;
		fse->max_height = fse->min_height;
	}

	return 0;
}

static void imx900_reset_colorspace(struct v4l2_mbus_framefmt *fmt)
{
	fmt->colorspace = V4L2_COLORSPACE_RAW;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_MAP_QUANTIZATION_DEFAULT(true,
							  fmt->colorspace,
							  fmt->ycbcr_enc);
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
}

static void imx900_update_image_pad_format(struct imx900 *imx900,
						const struct imx900_mode *mode,
						struct v4l2_subdev_format *fmt)
{
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	imx900_reset_colorspace(&fmt->format);
}

static void imx900_update_metadata_pad_format(struct v4l2_subdev_format *fmt)
{
	fmt->format.width = IMX900_EMBEDDED_LINE_WIDTH;
	fmt->format.height = IMX900_NUM_EMBEDDED_LINES;
	fmt->format.code = MEDIA_BUS_FMT_SENSOR_DATA;
	fmt->format.field = V4L2_FIELD_NONE;
}

static int imx900_get_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct imx900 *imx900 = to_imx900(sd);

	if (fmt->pad >= NUM_PADS)
		return -EINVAL;

	mutex_lock(&imx900->mutex);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(&imx900->sd, sd_state,
							fmt->pad);
		try_fmt->code = fmt->pad == IMAGE_PAD ?
				imx900_get_format_code(imx900, try_fmt->code) :
				MEDIA_BUS_FMT_SENSOR_DATA;
		fmt->format = *try_fmt;
	} else {
		if (fmt->pad == IMAGE_PAD) {
			imx900_update_image_pad_format(imx900, imx900->mode,
								fmt);
			fmt->format.code = imx900_get_format_code(imx900,
								imx900->fmt_code);
		} else {
			imx900_update_metadata_pad_format(fmt);
		}
	}

	mutex_unlock(&imx900->mutex);
	return 0;
}

static void imx900_set_limits(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	const struct imx900_mode *mode = imx900->mode;
	u64 max_framerate;

	dev_dbg(dev, "%s: mode: %dx%d\n", __func__, mode->width, mode->height);

	imx900_adjust_hmax_register(imx900);

	imx900_adjust_min_frame_length_delta(imx900);

	imx900_adjust_pixel_rate(imx900);

	imx900_adjust_link_frequency(imx900);

	imx900->line_time = (imx900->hmax*IMX900_G_FACTOR) / (IMX900_XCLK_FREQ);
	dev_dbg(dev, "%s: line time: %lld\n", __func__, imx900->line_time);

	imx900->frame_length = mode->height + imx900->min_frame_length_delta;
	dev_dbg(dev, "%s: frame length: %d\n", __func__, imx900->frame_length);

	max_framerate = (IMX900_G_FACTOR * IMX900_M_FACTOR) /
				(imx900->frame_length * imx900->line_time);

	__v4l2_ctrl_modify_range(imx900->framerate, mode->min_fps,
				 max_framerate, 1, max_framerate);
	dev_dbg(dev, "%s: max framerate: %lld\n", __func__, max_framerate);

	imx900_update_blklvl_range(imx900);

	__v4l2_ctrl_s_ctrl(imx900->framerate, max_framerate);
}

static int imx900_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt;
	const struct imx900_mode *mode;
	struct imx900 *imx900 = to_imx900(sd);

	if (fmt->pad >= NUM_PADS)
		return -EINVAL;

	mutex_lock(&imx900->mutex);

	if (fmt->pad == IMAGE_PAD) {
		const struct imx900_mode *mode_list;
		unsigned int num_modes;

		fmt->format.code = imx900_get_format_code(imx900, fmt->format.code);

		get_mode_table(imx900, fmt->format.code, &mode_list, &num_modes);

		mode = v4l2_find_nearest_size(mode_list,
						num_modes,
						width, height,
						fmt->format.width,
						fmt->format.height);
		imx900_update_image_pad_format(imx900, mode, fmt);

		if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
			framefmt = v4l2_subdev_get_try_format(sd, sd_state,
								fmt->pad);
			*framefmt = fmt->format;
		} else if (imx900->mode != mode) {
			imx900->mode = mode;
			imx900->fmt_code = fmt->format.code;
			imx900_set_limits(imx900);
		}
	} else {
		if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
			framefmt = v4l2_subdev_get_try_format(sd, sd_state,
								fmt->pad);
			*framefmt = fmt->format;
		} else {
			imx900_update_metadata_pad_format(fmt);
		}
	}

	mutex_unlock(&imx900->mutex);

	return 0;
}

static const struct v4l2_rect *
__imx900_get_pad_crop(struct imx900 *imx900,
			struct v4l2_subdev_state *sd_state,
			unsigned int pad, enum v4l2_subdev_format_whence which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_crop(&imx900->sd, sd_state, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &imx900->mode->crop;
	}

	return NULL;
}

static int imx900_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP: {
		struct imx900 *imx900 = to_imx900(sd);

		mutex_lock(&imx900->mutex);
		sel->r = *__imx900_get_pad_crop(imx900, sd_state, sel->pad,
						sel->which);
		mutex_unlock(&imx900->mutex);

		return 0;
	}

	case V4L2_SEL_TGT_NATIVE_SIZE:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = IMX900_NATIVE_WIDTH;
		sel->r.height = IMX900_NATIVE_HEIGHT;

		return 0;

	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r.left = IMX900_PIXEL_ARRAY_LEFT;
		sel->r.top = IMX900_PIXEL_ARRAY_TOP;
		sel->r.width = IMX900_PIXEL_ARRAY_WIDTH;
		sel->r.height = IMX900_PIXEL_ARRAY_HEIGHT;

		return 0;
	}

	return -EINVAL;
}

static int imx900_set_mode(struct imx900 *imx900)
{

	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	const struct imx900_reg_list *reg_list;
	int ret;

	ret = imx900_write_table(imx900, mode_common_regs,
					ARRAY_SIZE(mode_common_regs));

	if (ret) {
		dev_err(dev, "%s failed to set common settings\n", __func__);
		return ret;
	}

	reg_list = &imx900->mode->reg_list;
	ret = imx900_write_table(imx900, reg_list->regs, reg_list->num_of_regs);
	if (ret) {
		dev_err(dev, "%s failed to set mode\n", __func__);
		return ret;
	}

	reg_list = &imx900->mode->reg_list_format;
	ret = imx900_write_table(imx900, reg_list->regs, reg_list->num_of_regs);
	if (ret) {
		dev_err(dev, "%s failed to set frame format\n", __func__);
		return ret;
	}

	ret = imx900_set_hmax_register(imx900);
	if (ret) {
		dev_err(dev, "%s failed to write hmax register\n", __func__);
		return ret;
	}

	ret = imx900_set_data_rate(imx900);
	if (ret) {
		dev_err(dev, "%s failed to set data rate\n", __func__);
		return ret;
	}

	ret = imx900_set_mode_additional(imx900);
	if (ret) {
		dev_err(dev, "%s failed to write mode additional regs\n",
								__func__);
		return ret;
	}

	ret = imx900_set_dep_registers(imx900);
	if (ret) {
		dev_err(dev, "%s: unable to write dep registers to image sensor\n",
								__func__);
		return ret;
	}

	ret = imx900_set_pixel_format(imx900);
	if (ret) {
		dev_err(dev, "%s: unable to write format to image sensor\n",
								__func__);
		return ret;
	}

	ret = imx900_set_shutter_mode(imx900);
	if (ret) {
		dev_err(dev, "%s: unable to set shutter mode\n", __func__);
		return ret;
	}

	return ret;
}

static int imx900_start_streaming(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	int ret;

	if (!(strcmp(imx900->gmsl, "gmsl"))) {
		ret = max96793_setup_streaming(imx900->ser_dev, imx900->fmt_code);
		if (ret) {
			dev_err(dev, "%s: Unable to setup streaming for serializer max96793\n",
								__func__);
			return ret;
		}
		ret = max96792_setup_streaming(imx900->dser_dev,
							&client->dev);
		if (ret) {
			dev_err(dev, "%s: Unable to setup streaming for deserializer max96792\n",
								__func__);
			return ret;
		}
		ret = max96792_start_streaming(imx900->dser_dev, &client->dev);
		if (ret) {
			dev_err(dev, "%s: Unable to start gmsl streaming\n",
								__func__);
			return ret;
		}
	}

	ret = imx900_set_mode(imx900);
	if (ret) {
		dev_err(dev, "%s failed to set mode start stream\n", __func__);
		return ret;
	}

	ret = __v4l2_ctrl_handler_setup(imx900->sd.ctrl_handler);
	if (ret)
		return ret;

	ret = imx900_write_reg(imx900, STANDBY, 1, IMX900_MODE_STREAMING);

	if (ret) {
		dev_err(dev, "%s failed to set STANDBY start stream\n", __func__);
		return ret;
	}

	usleep_range(15000, 20000);

	if (imx900->operation_mode->val == MASTER_MODE)
		ret = imx900_write_reg(imx900, XMSTA, 1, 0x00);
	else
		ret = imx900_write_reg(imx900, XMSTA, 1, 0x01);

	if (ret) {
		dev_err(dev, "%s failed to set XMSTA start stream\n", __func__);
		return ret;
	}

	return ret;
}

static void imx900_stop_streaming(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	int ret;

	if (!(strcmp(imx900->gmsl, "gmsl"))) {
		max96793_bypassPCLK_dis(imx900->ser_dev);
		max96792_stop_streaming(imx900->dser_dev, &client->dev);
	}

	ret = imx900_write_reg(imx900, XMSTA, 1, 0x01);
	if (ret)
		dev_err(dev, "%s failed to set XMSTA stop stream\n", __func__);

	ret = imx900_write_reg(imx900, STANDBY, 1, IMX900_MODE_STANDBY);
	if (ret)
		dev_err(dev, "%s failed to set stream\n", __func__);

	usleep_range(imx900->frame_length * imx900->line_time / IMX900_K_FACTOR,
		imx900->frame_length * imx900->line_time / IMX900_K_FACTOR + 1000);

}

static int imx900_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx900 *imx900 = to_imx900(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	mutex_lock(&imx900->mutex);
	if (imx900->streaming == enable) {
		mutex_unlock(&imx900->mutex);
		return 0;
	}

	if (enable) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto err_unlock;
		}

		ret = imx900_start_streaming(imx900);
		if (ret)
			goto err_rpm_put;
	} else {
		imx900_stop_streaming(imx900);
		pm_runtime_put(&client->dev);
	}

	imx900->streaming = enable;

	__v4l2_ctrl_grab(imx900->vflip, enable);
	__v4l2_ctrl_grab(imx900->hflip, enable);
	__v4l2_ctrl_grab(imx900->operation_mode, enable);
	__v4l2_ctrl_grab(imx900->shutter_mode, enable);

	mutex_unlock(&imx900->mutex);

	return ret;

err_rpm_put:
	pm_runtime_put(&client->dev);
err_unlock:
	mutex_unlock(&imx900->mutex);

	return ret;
}

static int imx900_gmsl_serdes_setup(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	int ret = 0;
	int des_err = 0;

	if (!imx900 || !imx900->ser_dev || !imx900->dser_dev || !client)
		return -EINVAL;

	dev_dbg(dev, "enter %s function\n", __func__);

	mutex_lock(&imx900->mutex);

	ret = max96792_reset_control(imx900->dser_dev, &client->dev);

	ret = max96792_gmsl3_setup(imx900->dser_dev);
	if (ret) {
		dev_err(dev, "deserializer gmsl setup failed\n");
		goto error;
	}

	ret = max96793_gmsl3_setup(imx900->ser_dev);
	if (ret) {
		dev_err(dev, "serializer gmsl setup failed\n");
		goto error;
	}

	dev_dbg(dev, "%s: max96792_setup_link\n", __func__);
	ret = max96792_setup_link(imx900->dser_dev, &client->dev);
	if (ret) {
		dev_err(dev, "gmsl deserializer link config failed\n");
		goto error;
	}

	dev_dbg(dev, "%s: max96793_setup_control\n", __func__);
	ret = max96793_setup_control(imx900->ser_dev);

	if (ret)
		dev_err(dev, "gmsl serializer setup failed\n");

	ret = max96793_gpio10_xtrig1_setup(imx900->ser_dev, "mipi");
	if (ret) {
		dev_err(dev, "gmsl serializer gpio10/xtrig1 pin config failed\n");
		goto error;
	}

	dev_dbg(dev, "%s: max96792_setup_control\n", __func__);
	des_err = max96792_setup_control(imx900->dser_dev, &client->dev);
	if (des_err)
		dev_err(dev, "gmsl deserializer setup failed\n");

error:
	mutex_unlock(&imx900->mutex);
	return ret;
}

static void imx900_gmsl_serdes_reset(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);

	mutex_lock(&imx900->mutex);

	max96793_reset_control(imx900->ser_dev);
	max96792_reset_control(imx900->dser_dev, &client->dev);
	max96792_power_off(imx900->dser_dev, &imx900->g_ctx);

	mutex_unlock(&imx900->mutex);
}

static int imx900_power_on(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx900 *imx900 = to_imx900(sd);

	if (strcmp(imx900->gmsl, "gmsl")) {
		gpiod_set_value_cansleep(imx900->reset_gpio, 1);
		usleep_range(25000, 30000);
	} else {
		dev_info(dev, "%s: max96792_power_on\n", __func__);
		max96792_power_on(imx900->dser_dev, &imx900->g_ctx);
	}

	return 0;
}

static int imx900_power_off(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx900 *imx900 = to_imx900(sd);

	mutex_lock(&imx900->mutex);
	if (strcmp(imx900->gmsl, "gmsl")) {
		gpiod_set_value_cansleep(imx900->reset_gpio, 0);
	} else {
		dev_info(dev, "%s: max96792_power_off\n", __func__);
		max96792_power_off(imx900->dser_dev, &imx900->g_ctx);
	}
	mutex_unlock(&imx900->mutex);

	return 0;
}

static int __maybe_unused imx900_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx900 *imx900 = to_imx900(sd);

	if (imx900->streaming)
		imx900_stop_streaming(imx900);

	return 0;
}

static int __maybe_unused imx900_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx900 *imx900 = to_imx900(sd);
	int ret;

	if (imx900->streaming) {
		ret = imx900_start_streaming(imx900);
		if (ret)
			goto error;
	}

	return 0;

error:
	imx900_stop_streaming(imx900);
	imx900->streaming = 0;
	return ret;
}

static int imx900_communication_verify(struct imx900 *imx900)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	int ret;
	u32 val;

	ret = imx900_read_reg(imx900, VMAX_LOW, 3, &val);

	if (ret) {
		dev_err(dev, "%s unable to communicate with sensor\n", __func__);
		return ret;
	}

	ret = imx900_chromacity_mode(imx900);
	if (ret) {
		dev_err(dev, "%s: unable to get chromacity information\n",
								__func__);
		return ret;
	}

	if (imx900->chromacity == IMX900_COLOR)
		dev_info(dev, "Detected imx900 sensor - Color\n");
	else
		dev_info(dev, "Detected imx900 sensor - Mono\n");

	return 0;
}

static const struct v4l2_subdev_core_ops imx900_core_ops = {
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops imx900_video_ops = {
	.s_stream = imx900_set_stream,
};

static const struct v4l2_subdev_pad_ops imx900_pad_ops = {
	.enum_mbus_code = imx900_enum_mbus_code,
	.get_fmt = imx900_get_pad_format,
	.set_fmt = imx900_set_pad_format,
	.get_selection = imx900_get_selection,
	.enum_frame_size = imx900_enum_frame_size,
};

static const struct v4l2_subdev_ops imx900_subdev_ops = {
	.core = &imx900_core_ops,
	.video = &imx900_video_ops,
	.pad = &imx900_pad_ops,
};

static const struct v4l2_subdev_internal_ops imx900_internal_ops = {
	.open = imx900_open,
};

static struct v4l2_ctrl_config imx900_ctrl_framerate[] = {
	{
		.ops = &imx900_ctrl_ops,
		.id = V4L2_CID_FRAME_RATE,
		.name = "Frame rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 1,
		.max = 0xFFFF,
		.def = 0xFFFF,
		.step = 1,
	},
};

static struct v4l2_ctrl_config imx900_ctrl_operation_mode[] = {
	{
		.ops = &imx900_ctrl_ops,
		.id = V4L2_CID_OPERATION_MODE,
		.name = "Operation mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.min = MASTER_MODE,
		.def = MASTER_MODE,
		.max = SLAVE_MODE,
		.qmenu = imx900_operation_mode_menu,
	},
};

static struct v4l2_ctrl_config imx900_ctrl_global_shutter_mode[] = {
	{
		.ops = &imx900_ctrl_ops,
		.id = V4L2_CID_GLOBAL_SHUTTER_MODE,
		.name = "Global shutter mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.min = NORMAL_MODE,
		.def = NORMAL_MODE,
		.max = FAST_TRIGGER_MODE,
		.qmenu = imx900_global_shutter_menu,
	},
};

static int imx900_init_controls(struct imx900 *imx900)
{
	struct v4l2_ctrl_handler *ctrl_hdlr;
	struct i2c_client *client = v4l2_get_subdevdata(&imx900->sd);
	struct device *dev = &client->dev;
	struct v4l2_fwnode_device_properties props;
	int ret;

	ctrl_hdlr = &imx900->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 16);
	if (ret)
		return ret;

	mutex_init(&imx900->mutex);
	ctrl_hdlr->lock = &imx900->mutex;

	imx900->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &imx900_ctrl_ops,
						V4L2_CID_PIXEL_RATE, 0, 0, 1, 0);
	if (imx900->pixel_rate)
		imx900->pixel_rate->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	imx900->link_freq =
		v4l2_ctrl_new_int_menu(ctrl_hdlr, &imx900_ctrl_ops,
					V4L2_CID_LINK_FREQ,
					ARRAY_SIZE(imx900_link_freq_menu) - 1, 0,
					imx900_link_freq_menu);
	if (imx900->link_freq)
		imx900->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	imx900->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &imx900_ctrl_ops,
					V4L2_CID_VBLANK, 0, 0, 1, 0);

	imx900->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &imx900_ctrl_ops,
					V4L2_CID_HBLANK, 0, 0, 1, 0);

	if (imx900->hblank)
		imx900->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	imx900->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &imx900_ctrl_ops,
					V4L2_CID_EXPOSURE,
					IMX900_MIN_INTEGRATION_LINES,
					0xFF, 1, 0xFF);

	imx900->framerate = v4l2_ctrl_new_custom(ctrl_hdlr,
					imx900_ctrl_framerate, NULL);

	imx900->operation_mode = v4l2_ctrl_new_custom(ctrl_hdlr,
					imx900_ctrl_operation_mode, NULL);

	imx900->shutter_mode = v4l2_ctrl_new_custom(ctrl_hdlr,
					imx900_ctrl_global_shutter_mode, NULL);

	imx900->blklvl = v4l2_ctrl_new_std(ctrl_hdlr, &imx900_ctrl_ops,
					V4L2_CID_BLACK_LEVEL,
					IMX900_BLACK_LEVEL_MIN, 0xFF,
					IMX900_BLACK_LEVEL_STEP, 0xFF);

	v4l2_ctrl_new_std(ctrl_hdlr, &imx900_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
					IMX900_ANA_GAIN_MIN,
					IMX900_ANA_GAIN_MAX,
					IMX900_ANA_GAIN_STEP,
					IMX900_ANA_GAIN_DEFAULT);

	imx900->hflip = v4l2_ctrl_new_std(ctrl_hdlr, &imx900_ctrl_ops,
					  V4L2_CID_HFLIP, 0, 1, 1, 0);

	imx900->vflip = v4l2_ctrl_new_std(ctrl_hdlr, &imx900_ctrl_ops,
					  V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (imx900->vflip)
		imx900->vflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &imx900_ctrl_ops,
					V4L2_CID_TEST_PATTERN,
					ARRAY_SIZE(imx900_test_pattern_menu) - 1,
					0, 0, imx900_test_pattern_menu);

	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(dev, "%s control init failed (%d)\n", __func__, ret);
		goto error;
	}

	ret = v4l2_fwnode_device_parse(&client->dev, &props);
	if (ret)
		goto error;

	ret = v4l2_ctrl_new_fwnode_properties(ctrl_hdlr, &imx900_ctrl_ops, &props);
	if (ret)
		goto error;

	imx900->sd.ctrl_handler = ctrl_hdlr;

	mutex_lock(&imx900->mutex);

	imx900_set_limits(imx900);

	mutex_unlock(&imx900->mutex);

	return 0;

error:
	v4l2_ctrl_handler_free(ctrl_hdlr);
	mutex_destroy(&imx900->mutex);

	return ret;
}

static void imx900_free_controls(struct imx900 *imx900)
{
	v4l2_ctrl_handler_free(imx900->sd.ctrl_handler);
	mutex_destroy(&imx900->mutex);
}

static int imx900_check_hwcfg(struct device *dev, struct i2c_client *client)
{
	struct device_node *node = client->dev.of_node;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx900 *imx900 = to_imx900(sd);
	struct fwnode_handle *endpoint;
	struct v4l2_fwnode_endpoint ep_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	const char *gmsl;
	int ret = -EINVAL;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint) {
		dev_err(dev, "endpoint node not found\n");
		return -EINVAL;
	}

	if (v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep_cfg)) {
		dev_err(dev, "could not parse endpoint\n");
		goto error_out;
	}

	if (ep_cfg.bus.mipi_csi2.num_data_lanes != 4) {
		dev_err(dev, "only 4 data lanes are currently supported\n");
		goto error_out;
	}

	if (!ep_cfg.nr_of_link_frequencies) {
		dev_err(dev, "link-frequency property not found in DT\n");
		goto error_out;
	}

	if (ep_cfg.nr_of_link_frequencies != ARRAY_SIZE(imx900_link_freq_menu)) {
		dev_err(dev, "Link frequency missing in dtree\n");
		goto error_out;
	}

	for (int i = 0; i < ARRAY_SIZE(imx900_link_freq_menu); i++) {
		if (ep_cfg.link_frequencies[i] != imx900_link_freq_menu[i]) {
			dev_err(dev, "no supported link freq found\n");
			goto error_out;
		}
	}

	ret = of_property_read_string(node, "gmsl", &gmsl);
	if (ret) {
		dev_warn(dev, "initializing mipi...\n");
		imx900->gmsl = "mipi";
	} else if (!strcmp(gmsl, "gmsl")) {
		dev_warn(dev, "initializing GMSL...\n");
		imx900->gmsl = "gmsl";
	}

	ret = 0;

error_out:
	v4l2_fwnode_endpoint_free(&ep_cfg);
	fwnode_handle_put(endpoint);

	return ret;
}

static const struct of_device_id imx900_dt_ids[] = {
	{ .compatible = "framos,fr_imx900" },
	{	}
};

static int imx900_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct imx900 *imx900;
	const struct of_device_id *match;
	struct device_node *node = dev->of_node;
	struct device_node *ser_node;
	struct i2c_client *ser_i2c = NULL;
	struct device_node *dser_node;
	struct i2c_client *dser_i2c = NULL;
	struct device_node *gmsl;
	int value = 0xFFFF;
	const char *str_value;
	const char *str_value1[2];
	int i;
	int ret;

	imx900 = devm_kzalloc(&client->dev, sizeof(*imx900), GFP_KERNEL);
	if (!imx900)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&imx900->sd, client, &imx900_subdev_ops);

	match = of_match_device(imx900_dt_ids, dev);
	if (!match)
		return -ENODEV;

	if (imx900_check_hwcfg(dev, client))
		return -EINVAL;

	if (strcmp(imx900->gmsl, "gmsl")) {
		imx900->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
		if (IS_ERR(imx900->reset_gpio)) {
			dev_err(dev, "cannot get reset gpio\n");
			return PTR_ERR(imx900->reset_gpio);
		}
	}

	if (!(strcmp(imx900->gmsl, "gmsl"))) {
		ret = of_property_read_u32(node, "reg", &imx900->g_ctx.sdev_reg);
		if (ret < 0) {
			dev_err(dev, "reg not found\n");
			return ret;
		}

		ret = of_property_read_u32(node, "def-addr", &imx900->g_ctx.sdev_def);
		if (ret < 0) {
			dev_err(dev, "def-addr not found\n");
			return ret;
		}

		ser_node = of_parse_phandle(node, "gmsl-ser-device", 0);
		if (ser_node == NULL) {
			dev_err(dev, "missing %s handle\n", "gmsl-ser-device");
			return ret;
		}

		ret = of_property_read_u32(ser_node, "reg", &imx900->g_ctx.ser_reg);
		if (ret < 0) {
			dev_err(dev, "serializer reg not found\n");
			return ret;
		}

		ser_i2c = of_find_i2c_device_by_node(ser_node);
		of_node_put(ser_node);

		if (ser_i2c == NULL) {
			dev_err(dev, "missing serializer dev handle\n");
			return ret;
		}
		if (ser_i2c->dev.driver == NULL) {
			dev_err(dev, "missing serializer driver\n");
			return ret;
		}

		imx900->ser_dev = &ser_i2c->dev;

		dser_node = of_parse_phandle(node, "gmsl-dser-device", 0);
		if (dser_node == NULL) {
			dev_err(dev, "missing %s handle\n", "gmsl-dser-device");
			return ret;
		}

		dser_i2c = of_find_i2c_device_by_node(dser_node);
		of_node_put(dser_node);

		if (dser_i2c == NULL) {
			dev_err(dev, "missing deserializer dev handle\n");
			return ret;
		}
		if (dser_i2c->dev.driver == NULL) {
			dev_err(dev, "missing deserializer driver\n");
			return ret;
		}

		imx900->dser_dev = &dser_i2c->dev;

		gmsl = of_get_child_by_name(node, "gmsl-link");
		if (gmsl == NULL) {
			dev_err(dev, "missing gmsl-link device node\n");
			ret = -EINVAL;
			return ret;
		}

		ret = of_property_read_string(gmsl, "dst-csi-port", &str_value);
		if (ret < 0) {
			dev_err(dev, "No dst-csi-port found\n");
			return ret;
		}
		imx900->g_ctx.dst_csi_port =
		(!strcmp(str_value, "a")) ? GMSL_CSI_PORT_A : GMSL_CSI_PORT_B;

		ret = of_property_read_string(gmsl, "src-csi-port", &str_value);
		if (ret < 0) {
			dev_err(dev, "No src-csi-port found\n");
			return ret;
		}
		imx900->g_ctx.src_csi_port =
		(!strcmp(str_value, "a")) ? GMSL_CSI_PORT_A : GMSL_CSI_PORT_B;

		ret = of_property_read_string(gmsl, "csi-mode", &str_value);
		if (ret < 0) {
			dev_err(dev, "No csi-mode found\n");
			return ret;
		}

		if (!strcmp(str_value, "1x4")) {
			imx900->g_ctx.csi_mode = GMSL_CSI_1X4_MODE;
		} else if (!strcmp(str_value, "2x4")) {
			imx900->g_ctx.csi_mode = GMSL_CSI_2X4_MODE;
		} else if (!strcmp(str_value, "2x2")) {
			imx900->g_ctx.csi_mode = GMSL_CSI_2X2_MODE;
		} else {
			dev_err(dev, "invalid csi mode\n");
			return ret;
		}

		ret = of_property_read_string(gmsl, "serdes-csi-link", &str_value);
		if (ret < 0) {
			dev_err(dev, "No serdes-csi-link found\n");
			return ret;
		}
		imx900->g_ctx.serdes_csi_link =
		(!strcmp(str_value, "a")) ? GMSL_SERDES_CSI_LINK_A : GMSL_SERDES_CSI_LINK_B;

		ret = of_property_read_u32(gmsl, "st-vc", &value);
		if (ret < 0) {
			dev_err(dev, "No st-vc info\n");
			return ret;
		}

		imx900->g_ctx.st_vc = value;

		ret = of_property_read_u32(gmsl, "vc-id", &value);
		if (ret < 0) {
			dev_err(dev, "No vc-id info\n");
			return ret;
		}
		imx900->g_ctx.dst_vc = value;

		ret = of_property_read_u32(gmsl, "num-lanes", &value);
		if (ret < 0) {
			dev_err(dev, "No num-lanes info\n");
			return ret;
		}

		imx900->g_ctx.num_csi_lanes = value;

		imx900->g_ctx.num_streams =
				of_property_count_strings(gmsl, "streams");
		if (imx900->g_ctx.num_streams <= 0) {
			dev_err(dev, "No streams found\n");
			ret = -EINVAL;
			return ret;
		}

		for (i = 0; i < imx900->g_ctx.num_streams; i++) {
			of_property_read_string_index(gmsl, "streams",
							i, &str_value1[i]);
			if (!str_value1[i]) {
				dev_err(dev, "invalid stream info\n");
				return ret;
			}
			if (!strcmp(str_value1[i], "raw12")) {
				imx900->g_ctx.streams[i].st_data_type = GMSL_CSI_DT_RAW_12;
			} else if (!strcmp(str_value1[i], "embed")) {
				imx900->g_ctx.streams[i].st_data_type = GMSL_CSI_DT_EMBED;
			} else if (!strcmp(str_value1[i], "ued-u1")) {
				imx900->g_ctx.streams[i].st_data_type = GMSL_CSI_DT_UED_U1;
			} else {
				dev_err(dev, "invalid stream data type\n");
				return ret;
			}
		}

		imx900->g_ctx.s_dev = dev;

		ret = max96793_sdev_pair(imx900->ser_dev, &imx900->g_ctx);
		if (ret) {
			dev_err(dev, "gmsl ser pairing failed\n");
			return ret;
		}

		ret = max96792_sdev_register(imx900->dser_dev, &imx900->g_ctx);
		if (ret) {
			dev_err(dev, "gmsl deserializer register failed\n");
			return ret;
		}

		ret = imx900_gmsl_serdes_setup(imx900);
		if (ret) {
			dev_err(dev, "%s gmsl serdes setup failed\n", __func__);
			return ret;
		}

	}

	ret = imx900_power_on(dev);
	if (ret)
		return ret;

	ret = imx900_communication_verify(imx900);
	if (ret)
		goto error_power_off;

	imx900->xmaster = devm_gpiod_get(dev, "xmaster", GPIOD_OUT_HIGH);
	if (IS_ERR(imx900->xmaster)) {
		dev_err(dev, "cannot get xmaster gpio\n");
		return PTR_ERR(imx900->xmaster);
	}

	imx900->mode = &modes_12bit[0];
	if (imx900->chromacity == IMX900_COLOR)
		imx900->fmt_code = MEDIA_BUS_FMT_SRGGB12_1X12;
	else
		imx900->fmt_code = MEDIA_BUS_FMT_Y12_1X12;

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	ret = imx900_init_controls(imx900);
	if (ret)
		goto error_power_off;

	imx900->sd.internal_ops = &imx900_internal_ops;
	imx900->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
				V4L2_SUBDEV_FL_HAS_EVENTS;
	imx900->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	imx900->pad[IMAGE_PAD].flags = MEDIA_PAD_FL_SOURCE;
	imx900->pad[METADATA_PAD].flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&imx900->sd.entity, NUM_PADS, imx900->pad);
	if (ret) {
		dev_err(dev, "failed to init entity pads: %d\n", ret);
		goto error_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor(&imx900->sd);
	if (ret < 0) {
		dev_err(dev, "failed to register sensor sub-device: %d\n", ret);
		goto error_media_entity;
	}

	return 0;

error_media_entity:
	media_entity_cleanup(&imx900->sd.entity);

error_handler_free:
	imx900_free_controls(imx900);

error_power_off:
	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);
	imx900_power_off(&client->dev);

	return ret;
}

static void imx900_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx900 *imx900 = to_imx900(sd);

	if (!(strcmp(imx900->gmsl, "gmsl"))) {
		max96792_sdev_unregister(imx900->dser_dev, &client->dev);
		imx900_gmsl_serdes_reset(imx900);
	}

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	imx900_free_controls(imx900);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		imx900_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);
}

MODULE_DEVICE_TABLE(of, imx900_dt_ids);

static const struct dev_pm_ops imx900_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(imx900_suspend, imx900_resume)
	SET_RUNTIME_PM_OPS(imx900_power_off, imx900_power_on, NULL)
};

static struct i2c_driver imx900_i2c_driver = {
	.driver = {
		.name = "fr_imx900",
		.of_match_table	= imx900_dt_ids,
		.pm = &imx900_pm_ops,
	},
	.probe = imx900_probe,
	.remove = imx900_remove,
};

module_i2c_driver(imx900_i2c_driver);

MODULE_AUTHOR("FRAMOS GmbH");
MODULE_DESCRIPTION("Sony IMX900 sensor driver");
MODULE_LICENSE("GPL v2");
