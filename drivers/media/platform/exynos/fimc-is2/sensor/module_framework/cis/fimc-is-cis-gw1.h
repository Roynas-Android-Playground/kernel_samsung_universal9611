/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_CIS_GW1_H
#define FIMC_IS_CIS_GW1_H

#include "fimc-is-cis.h"

#define EXT_CLK_Mhz (26)
#define USE_GROUP_PARAM_HOLD	(0)
#define TOTAL_NUM_OF_IVTPX_CHANNEL                (1)

#define SENSOR_GW1_MAX_WIDTH		(9248)
#define SENSOR_GW1_MAX_HEIGHT		(6936)

/* TODO: Check below values are valid */
#define SENSOR_GW1_FINE_INTEGRATION_TIME_MIN                    (0x0)
#define SENSOR_GW1_FINE_INTEGRATION_TIME_MAX_MARGIN             (0x0)
#define SENSOR_GW1_COARSE_INTEGRATION_TIME_MIN                  (0x4)
#define SENSOR_GW1_COARSE_INTEGRATION_TIME_MAX_MARGIN           (0x4)
#define SENSOR_GW1_MAX_COARSE_INTEGRATION_TIME                  (65503)
#define SENSOR_GW1_MAX_CIT_LSHIFT_VALUE                         (0xB)

/* GW1 Regsister Address */
#define SENSOR_GW1_DIRECT_PAGE_ACCESS_MARK      (0xFCFC)
#define SENSOR_GW1_DIRECT_DEFALT_PAGE_ADDR      (0x4000)
#define SENSOR_GW1_INDIRECT_WRITE_PAGE_ADDR     (0x6028)
#define SENSOR_GW1_INDIRECT_WRITE_OFFSET_ADDR   (0x602A)
#define SENSOR_GW1_INDIRECT_WRITE_DATA_ADDR     (0x6F12)
#define SENSOR_GW1_INDIRECT_READ_PAGE_ADDR      (0x602C)
#define SENSOR_GW1_INDIRECT_READ_OFFSET_ADDR    (0x602E)

#define SENSOR_GW1_MODEL_ID_ADDR                (0x0000)
#define SENSOR_GW1_REVISION_NUM_ADDR            (0x0002)
#define SENSOR_GW1_FRAME_COUNT_ADDR             (0x0005)
#define SENSOR_GW1_SETUP_MODE_SELECT_ADDR       (0x0100)
#define SENSOR_GW1_GROUP_PARAM_HOLD_ADDR        (0x0104)
#define SENSOR_GW1_EXTCLK_FREQ_ADDR             (0x0136)
#define SENSOR_GW1_FINE_INTEG_TIME_ADDR         (0x0200)
#define SENSOR_GW1_CORASE_INTEG_TIME_ADDR       (0x0202)
#define SENSOR_GW1_AGAIN_CODE_GLOBAL_ADDR       (0x0204)
#define SENSOR_GW1_DGAIN_GLOBAL_ADDR            (0x020E)
#define SENSOR_GW1_FRAME_LENGTH_LINE_ADDR       (0x0340)
#define SENSOR_GW1_FRM_LENGTH_LINE_LSHIFT_ADDR  (0x0702)
#define SENSOR_GW1_CIT_LSHIFT_ADDR              (0x0704)
#define SENSOR_GW1_LINE_LENGTH_PCK_ADDR         (0x0342)
#define SENSOR_GW1_WBGAIN_RED                   (0x0D82)
#define SENSOR_GW1_WBGAIN_GREEN                 (0x0D84)
#define SENSOR_GW1_WBGAIN_BLUE                  (0x0D86)

/* GW1 Debug Define */
#define SENSOR_GW1_DEBUG_INFO    (0)

enum sensor_gw1_mode_enum {
	SENSOR_GW1_FULL_REMOSAIC_9248X6936_13FPS = 0,
	SENSOR_GW1_2X2BIN_4624X3468_30FPS,
	SENSOR_GW1_2X2BIN_4624X2604_30FPS,
	SENSOR_GW1_2X2BIN_4000X3000_30FPS,
	SENSOR_GW1_2X2BIN_3296x1856_60FPS,
	SENSOR_GW1_4X4BIN_1280x720_240FPS,
	SENSOR_GW1_4X4BIN_2000x1128_240FPS,
	SENSOR_GW1_4X4BIN_2304x1728_120FPS,
};

/* IS_REMOSAIC_MODE(struct fimc_is_cis *cis) */
#define IS_REMOSAIC(mode) ({						\
	typecheck(u32, mode) && (mode == SENSOR_GW1_FULL_REMOSAIC_9248X6936_13FPS);\
})

#define IS_REMOSAIC_MODE(cis) ({					\
	u32 m;								\
	typecheck(struct fimc_is_cis *, cis);				\
	m = cis->cis_data->sens_config_index_cur;			\
	(m == SENSOR_GW1_FULL_REMOSAIC_9248X6936_13FPS); \
})

/* Related EEPROM CAL */
#define SENSOR_GW1_XTC_CAL_BASE_REAR	(0x0D56)
#define SENSOR_GW1_XTC_PAGE_ADDR	(0x2001)
#define SENSOR_GW1_XTC_REG_ADDR		(0x3948)
#define SENSOR_GW1_XTC_CAL_SIZE		(1744)

#define SENSOR_GW1_XTC_DUMP_NAME	"/data/vendor/camera/GW1_CROSSTALK_DUMP.bin"

/* Related Function Option */
#define SENSOR_GW1_WRITE_XTC_CAL	(1)
#define SENSOR_GW1_CAL_DEBUG		(0)

#ifdef SENSOR_GW1_WRITE_XTC_CAL
const u32 sensor_gw1_xtc_additional_setting[] = {
	0x6028, 0x4000, 0x02,
	0x602A, 0x0FE8, 0x02,
	0x6F12, 0x1000, 0x02,
	0x6028, 0x2000, 0x02,
	0x602A, 0x5056, 0x02,
	0x6F12, 0x1000, 0x02,
	0x6028, 0x2000, 0x02,
	0x602A, 0x5058, 0x02,
	0x6F12, 0x2000, 0x02,
	0x6028, 0x2000, 0x02,
	0x602A, 0x505A, 0x02,
	0x6F12, 0x5000, 0x02,
	0x6028, 0x2000, 0x02,
	0x602A, 0x505C, 0x02,
	0x6F12, 0x6000, 0x02,
	0x6028, 0x4000, 0x02,
	0x602A, 0x0100, 0x02,
	0x6F12, 0x00, 0x01,
	0x6028, 0x2000, 0x02,
	0x602A, 0x3850, 0x02,
	0x6F12, 0x01, 0x01,
	0x6028, 0x4000, 0x02,
	0x602A, 0x0B00, 0x02,
	0x6F12, 0x01, 0x01,
};
#endif
#endif
