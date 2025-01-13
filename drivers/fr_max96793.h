/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2024, Framos. All rights reserved.
 *
 * max96793.c - max96793 GMSL Serializer header
 */

#ifndef __fr_MAX96793_H__
#define __fr_MAX96793_H__

#include "gmsl-link.h"

#define GMSL_CSI_1X4_MODE		0x1
#define GMSL_CSI_2X4_MODE		0x2
#define GMSL_CSI_2X2_MODE		0x3
#define GMSL_CSI_4X2_MODE		0x4

#define GMSL_CSI_PORT_A			0x0
#define GMSL_CSI_PORT_B			0x1
#define GMSL_CSI_PORT_C			0x2
#define GMSL_CSI_PORT_D			0x3
#define GMSL_CSI_PORT_E			0x4
#define GMSL_CSI_PORT_F			0x5

#define GMSL_SERDES_CSI_LINK_A		0x1
#define GMSL_SERDES_CSI_LINK_B		0x2

#define GMSL_CSI_DT_RAW_12		0x2C
#define GMSL_CSI_DT_UED_U1		0x30
#define GMSL_CSI_DT_EMBED		0x12

#define GMSL_ST_ID_UNUSED		0xFF
#define GMSL_DEV_MAX_NUM_DATA_STREAMS	4

int max96793_setup_control(struct device *dev);

int max96793_reset_control(struct device *dev);

int max96793_sdev_pair(struct device *dev, struct gmsl_link_ctx *g_ctx);

int max96793_sdev_unpair(struct device *dev, struct device *s_dev);

int max96793_setup_streaming(struct device *dev, u32 code);

int max96793_gmsl3_setup(struct device *dev);

int max96793_gpio10_xtrig1_setup(struct device *dev, char *image_sensor_type);

int max96793_xvs_setup(struct device *dev, bool direction);

int max96793_bypassPCLK_dis(struct device *dev);

enum {
	max96793_OUT,
	max96793_IN,
};

#endif
