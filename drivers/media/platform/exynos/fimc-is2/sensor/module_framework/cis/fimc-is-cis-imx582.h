/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_CIS_IMX582_H
#define FIMC_IS_CIS_IMX582_H

#include "fimc-is-cis.h"

#define EXT_CLK_Mhz (26)
#define SENSOR_IMX582_240FPS_FRAME_DURATION_US    (4166)

#define SENSOR_IMX582_MAX_WIDTH          (8000 + 0)
#define SENSOR_IMX582_MAX_HEIGHT         (6000 + 0)

/* Related Sensor Parameter */
#define USE_GROUP_PARAM_HOLD                      (1)
#define TOTAL_NUM_OF_IVTPX_CHANNEL                (4)

#define SENSOR_IMX582_FINE_INTEGRATION_TIME                    (6320)    //FINE_INTEG_TIME is a fixed value (0x0200: 16bit - read value is 0x18b0)
#define SENSOR_IMX582_COARSE_INTEGRATION_TIME_MIN              (5)
#define SENSOR_IMX582_COARSE_INTEGRATION_TIME_MIN_FOR_PDAF     (6)
#define SENSOR_IMX582_COARSE_INTEGRATION_TIME_MIN_FOR_V2H2     (6)
#define SENSOR_IMX582_COARSE_INTEGRATION_TIME_MAX_MARGIN       (48)
#define SENSOR_IMX582_MAX_COARSE_INTEG_WITH_FRM_LENGTH_CTRL    (65535)
#define SENSOR_IMX582_MAX_CIT_LSHIFT_VALUE        (0x7)

#define SENSOR_IMX582_OTP_PAGE_SETUP_ADDR         (0x0A02)
#define SENSOR_IMX582_OTP_READ_TRANSFER_MODE_ADDR (0x0A00)
#define SENSOR_IMX582_OTP_STATUS_REGISTER_ADDR    (0x0A01)
#define SENSOR_IMX582_OTP_CHIP_REVISION_ADDR      (0x0018)

#define SENSOR_IMX582_CIT_LSHIFT_ADDR             (0x3100)
#define SENSOR_IMX582_FRAME_LENGTH_LINE_ADDR      (0x0340)
#define SENSOR_IMX582_LINE_LENGTH_PCK_ADDR        (0x0342)
#define SENSOR_IMX582_FINE_INTEG_TIME_ADDR        (0x0200)
#define SENSOR_IMX582_COARSE_INTEG_TIME_ADDR      (0x0202)
#define SENSOR_IMX582_ANALOG_GAIN_ADDR            (0x0204)
#define SENSOR_IMX582_DIG_GAIN_ADDR               (0x020E)

#define SENSOR_IMX582_MIN_ANALOG_GAIN_SET_VALUE   (112)
#define SENSOR_IMX582_MAX_ANALOG_GAIN_SET_VALUE   (1008)
#define SENSOR_IMX582_MIN_DIGITAL_GAIN_SET_VALUE  (0x0100)
#define SENSOR_IMX582_MAX_DIGITAL_GAIN_SET_VALUE  (0x0FFF)

#define SENSOR_IMX582_ABS_GAIN_GR_SET_ADDR        (0x0B8E)
#define SENSOR_IMX582_ABS_GAIN_R_SET_ADDR         (0x0B90)
#define SENSOR_IMX582_ABS_GAIN_B_SET_ADDR         (0x0B92)
#define SENSOR_IMX582_ABS_GAIN_GB_SET_ADDR        (0x0B94)


/* Related EEPROM CAL */
#define SENSOR_IMX582_LRC_CAL_BASE_REAR           (0x0970)
#define SENSOR_IMX582_LRC_CAL_SIZE                (384)
#define SENSOR_IMX582_LRC_REG_ADDR                (0x7b10)

#define SENSOR_IMX582_QUAD_SENS_CAL_BASE_REAR     (0x0CF0)
#define SENSOR_IMX582_QUAD_SENS_CAL_SIZE          (2304)
#define SENSOR_IMX582_QUAD_SENS_REG_ADDR          (0xC500)

#define SENSOR_IMX582_EMBEDDED_DATA_LINE_CONTROL  (0xBCF1)

#define SENSOR_IMX582_LRC_DUMP_NAME               "/data/vendor/camera/IMX582_LRC_DUMP.bin"
#define SENSOR_IMX582_QSC_DUMP_NAME               "/data/vendor/camera/IMX582_QSC_DUMP.bin"


/* Related Function Option */
#define SENSOR_IMX582_WRITE_PDAF_CAL              (1)
#define SENSOR_IMX582_WRITE_SENSOR_CAL            (1)
#define SENSOR_IMX582_CAL_DEBUG                   (0)
#define SENSOR_IMX582_DEBUG_INFO                  (0)


/*
 * [Mode Information]
 *
 * Reference File : IMX582_SEC-DPHY-26MHz_RegisterSetting_ver9.00-5.10_b3_MP0_200508.xlsx
 * Update Data    : 2020-05-12
 * Author         : takkyoum.kim
 *
 * - Global Setting -
 *
 * - 2x2 BIN For Still Preview / Capture -
 *    [  0 ] REG_H_2_t   : 2x2 Binning Full 4000x3000 30fps    : Single Still Preview/Capture (4:3)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *    [  1 ] REG_I_2_t   : 2x2 Binning Crop 4000X2256 30fps    : Single Still Preview/Capture (16:9)   ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *    [  2 ] REG_J_2     : 2x2 Binning Crop 4000X1952 30fps    : Single Still Preview/Capture (18.5:9) ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *    [  3 ] REG_K_2     : 2x2 Binning Crop 4000X1844 30fps    : Single Still Preview/Capture (19.5:9) ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *    [  4 ] REG_L_2     : 2x2 Binning Crop 4000X1800 30fps    : Single Still Preview/Capture (20:9)   ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *    [  5 ] REG_M_2     : 2x2 Binning Crop 3664X3000 30fps    : MMS (11:9)                            ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *    [  6 ] REG_N_2     : 2x2 Binning Crop 3000X3000 30fps    : Single Still Preview/Capture (1:1)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *    [  7 ] REG_U       : 2x2 Binning Crop 3264X2448 30fps    : Single Still Capture (4:3)            ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *
 * - 2x2 BIN V2H2 For HighSpeed Recording/FastAE-
 *    [  8 ] REG_O_2     : 2x2 Binning V2H2 2000X1500 120fps   : FAST AE (4:3)                         ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 1848
 *    [  9 ] REG_P_2     : 2x2 Binning V2H2 2000X1128 120fps   : High Speed Recording (16:9)           ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 1848
 *
 * - 2x2 BIN V2H2 For HighSpeed Recording -
 *    [ 10 ] REG_Q_2     : 2x2 Binning V2H2 1296X736  240fps   : High Speed Recording (16:9)           ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 1848
 *    [ 11 ] REG_R_2     : 2x2 Binning V2H2 2000X1128 240fps   : High Speed Recording (16:9)           ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 1848
 *
 * - 2x2 BIN FHD Recording
 *    [ 12 ] REG_S       : 2x2 Binning V2H2 4000x2256  60fps   : FHD Recording (16:9)                  ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *
 * - Remosaic For Single Still Remosaic Capture -
 *    [ 13 ] REG_G       : Remosaic Full 8000x6000 15fps        : Single Still Remosaic Capture (4:3)   ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2184
 *    [ 14 ] REG_T_t     : Remosaic Crop 4000x3000 30fps        : Single Still Remosaic Capture (4:3)   ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2184
 *    [ 15 ] REG_A_A_t   : Remosaic Crop 4000x2256 30fps        : Single Still Remosaic Capture (16:9)  ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 */

enum sensor_imx582_mode_enum {

	/* 2x2 Binning 30Fps */
	SENSOR_IMX582_2X2BIN_FULL_4000X3000_30FPS = 0,
	SENSOR_IMX582_2X2BIN_CROP_4000X2256_30FPS,
	SENSOR_IMX582_2X2BIN_CROP_4000X1952_30FPS,
	SENSOR_IMX582_2X2BIN_CROP_4000X1844_30FPS,
	SENSOR_IMX582_2X2BIN_CROP_4000X1800_30FPS,
	SENSOR_IMX582_2X2BIN_CROP_3664X3000_30FPS,
	SENSOR_IMX582_2X2BIN_CROP_3000X3000_30FPS,
	SENSOR_IMX582_2X2BIN_CROP_3264x2448_30FPS,

	/* 2X2 Binning V2H2 120FPS */
	SENSOR_IMX582_2X2BIN_V2H2_2000X1500_120FPS = 8,
	SENSOR_IMX582_2X2BIN_V2H2_2000X1128_120FPS,

	/* 2X2 Binning V2H2 240FPS */
	SENSOR_IMX582_2X2BIN_V2H2_1296X736_240FPS = 10,
	SENSOR_IMX582_2X2BIN_V2H2_2000X1128_240FPS,

	/* 2X2 Binning FHD Recording*/
	SENSOR_IMX582_2X2BIN_CROP_4000X2256_60FPS = 12,

	/* Remosaic */
	IMX582_MODE_REMOSAIC_START,
	SENSOR_IMX582_REMOSAIC_FULL_8000x6000_15FPS = IMX582_MODE_REMOSAIC_START,
	SENSOR_IMX582_REMOSAIC_CROP_4000x3000_30FPS,
	SENSOR_IMX582_REMOSAIC_CROP_4000x2256_30FPS,
	IMX582_MODE_REMOSAIC_END = SENSOR_IMX582_REMOSAIC_CROP_4000x2256_30FPS,
};

/*
 *    CHIP Revision Confirmation
 *
 *                            Fab1                            Fab2                            Fab3
 ***********************************************************************************************************
 *                            Cut1.0   MP0     MP             Cut1.0   MP0     MP             MP0     MP
 ***********************************************************************************************************
 *    I2C Address             0x02h    0x10h   0x20h          0x08h    0x18h   0x20h          0x1Bh   0x20h
 *    0x0018                                                  0x09h
 ***********************************************************************************************************
 *    Date                    Mar/E    Apr/B   Oct/B          May/E    Jul/B   Oct/B          Sep/E   Oct/B
 ***********************************************************************************************************
 *    Register Ver            Ver5.0   Ver5.0  Ver6.0         Ver5.0   Ver5.0  Ver6.0         Ver6.0  Ver6.0
 */

enum sensor_imx582_chip_id_ver_enum {
	SENSOR_IMX582_CHIP_ID_VER_5_0_0X02 = 0x02,
	SENSOR_IMX582_CHIP_ID_VER_5_0_0X08 = 0x08,
	SENSOR_IMX582_CHIP_ID_VER_5_0_0X09 = 0x09,
	SENSOR_IMX582_CHIP_ID_VER_5_0_0X10 = 0x10,
	SENSOR_IMX582_CHIP_ID_VER_5_0_0X18 = 0x18,
	SENSOR_IMX582_CHIP_ID_VER_6_0_0X1B = 0x1B,
	SENSOR_IMX582_CHIP_ID_VER_6_0_0X20 = 0x20,
};

#endif


/* IS_REMOSAIC_MODE(struct fimc_is_cis *cis) */
#define IS_REMOSAIC(mode) ({						\
	typecheck(u32, mode) && (mode >= IMX582_MODE_REMOSAIC_START) &&	\
	(mode <= IMX582_MODE_REMOSAIC_END);				\
})

#define IS_REMOSAIC_MODE(cis) ({					\
	u32 m;								\
	typecheck(struct fimc_is_cis *, cis);				\
	m = cis->cis_data->sens_config_index_cur;			\
	(m >= IMX582_MODE_REMOSAIC_START) && (m <= IMX582_MODE_REMOSAIC_END); \
})

#define IS_SEAMLESS_MODE_CHANGE(cis) ({						\
	u32 m;													\
	typecheck(struct fimc_is_cis *, cis);							\
															\
	m = cis->cis_data->sens_config_index_cur;					\
	(m == SENSOR_IMX582_2X2BIN_FULL_4000X3000_30FPS			\
	|| m == SENSOR_IMX582_2X2BIN_CROP_4000X2256_30FPS		\
	|| m == SENSOR_IMX582_REMOSAIC_CROP_4000x3000_30FPS	\
	|| m == SENSOR_IMX582_REMOSAIC_CROP_4000x2256_30FPS);	\
})
