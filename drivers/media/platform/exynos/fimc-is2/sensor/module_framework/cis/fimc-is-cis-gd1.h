/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_CIS_GD1_H
#define FIMC_IS_CIS_GD1_H

#include "fimc-is-cis.h"

#define EXT_CLK_Mhz (26)
#define USE_GROUP_PARAM_HOLD	(0)   //TODO: verify the group param parameter
#define TOTAL_NUM_OF_IVTPX_CHANNEL                (4)

#define SENSOR_GD1_MAX_WIDTH		(6528)
#define SENSOR_GD1_MAX_HEIGHT		(4896)

/* TODO: Check below values are valid */
#define SENSOR_GD1_FINE_INTEGRATION_TIME_MIN                    (0x0)
#define SENSOR_GD1_FINE_INTEGRATION_TIME_MAX_MARGIN             (0x0)
#define SENSOR_GD1_COARSE_INTEGRATION_TIME_MIN                  (0x4)
#define SENSOR_GD1_COARSE_INTEGRATION_TIME_MAX_MARGIN           (0x4)

/* GD1 Regsister Address */
#define SENSOR_GD1_DIRECT_PAGE_ACCESS_MARK      (0xFCFC)
#define SENSOR_GD1_DIRECT_DEFALT_PAGE_ADDR      (0x4000)
#define SENSOR_GD1_INDIRECT_WRITE_PAGE_ADDR     (0x6028)
#define SENSOR_GD1_INDIRECT_WRITE_OFFSET_ADDR   (0x602A)
#define SENSOR_GD1_INDIRECT_WRITE_DATA_ADDR     (0x6F12)
#define SENSOR_GD1_INDIRECT_READ_PAGE_ADDR      (0x602C)
#define SENSOR_GD1_INDIRECT_READ_OFFSET_ADDR    (0x602E)

#define SENSOR_GD1_MODEL_ID_ADDR                (0x0000)
#define SENSOR_GD1_REVISION_NUM_ADDR            (0x0002)
#define SENSOR_GD1_FRAME_COUNT_ADDR             (0x0005)
#define SENSOR_GD1_SETUP_MODE_SELECT_ADDR       (0x0100)
#define SENSOR_GD1_GROUP_PARAM_HOLD_ADDR        (0x0104)
#define SENSOR_GD1_EXTCLK_FREQ_ADDR             (0x0136)
#define SENSOR_GD1_FINE_INTEG_TIME_ADDR         (0x0200)
#define SENSOR_GD1_CORASE_INTEG_TIME_ADDR       (0x0202)
#define SENSOR_GD1_AGAIN_CODE_GLOBAL_ADDR       (0x0204)
#define SENSOR_GD1_DGAIN_GLOBAL_ADDR            (0x020E)
#define SENSOR_GD1_FRAME_LENGTH_LINE_ADDR       (0x0340)
#define SENSOR_GD1_LINE_LENGTH_PCK_ADDR         (0x0342)
#define SENSOR_GD1_WBGAIN_RED                   (0x0D82)
#define SENSOR_GD1_WBGAIN_GREEN                 (0x0D84)
#define SENSOR_GD1_WBGAIN_BLUE                  (0x0D86)

/* GD1 Debug Define */
#define SENSOR_GD1_DEBUG_INFO    (0)

enum sensor_gd1_mode_enum {
	SENSOR_GD1_FULL_REMOSAIC_6528X4896_24FPS = 0,
	SENSOR_GD1_2X2BIN_3264X2448_30FPS,
	SENSOR_GD1_2X2BIN_3264X1836_30FPS,
	SENSOR_GD1_2X2BIN_2640X1980_30FPS,
	SENSOR_GD1_2X2BIN_2640X1488_30FPS,
};

/* IS_REMOSAIC_MODE(struct fimc_is_cis *cis) */
#define IS_REMOSAIC(mode) ({						\
	typecheck(u32, mode) && (mode == SENSOR_GD1_FULL_REMOSAIC_6528X4896_24FPS);\
})

#define IS_REMOSAIC_MODE(cis) ({						\
	u32 m;												\
	typecheck(struct fimc_is_cis *, cis);				\
	m = cis->cis_data->sens_config_index_cur;			\
	(m == SENSOR_GD1_FULL_REMOSAIC_6528X4896_24FPS); 	\
})

/* TODO: Check Related EEPROM CAL */
#define SENSOR_GD1_XTC_CAL_BASE_REAR	(0x0D56)
#define SENSOR_GD1_XTC_PAGE_ADDR	(0x2001)
#define SENSOR_GD1_XTC_REG_ADDR		(0x3948)
#define SENSOR_GD1_XTC_CAL_SIZE		(1744)

#define SENSOR_GD1_XTC_DUMP_NAME	"/data/vendor/camera/GD1_CROSSTALK_DUMP.bin"

/* Related Function Option */
#define SENSOR_GD1_WRITE_XTC_CAL	(0)
#define SENSOR_GD1_CAL_DEBUG		(0)

/*TODO */
#ifdef SENSOR_GD1_WRITE_XTC_CAL
const u32 sensor_gd1_xtc_additional_setting[] = {
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
