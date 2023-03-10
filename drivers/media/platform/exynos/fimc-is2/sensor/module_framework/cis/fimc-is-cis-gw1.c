/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/syscalls.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-fimc-is-sensor.h>
#include "fimc-is-hw.h"
#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-cis-gw1.h"
#include "fimc-is-cis-gw1-setA.h"
#include "fimc-is-cis-gw1-setB.h"

#include "fimc-is-helper-i2c.h"
#ifdef CONFIG_VENDER_MCD_V2
#include "fimc-is-sec-define.h"
#endif

#include "interface/fimc-is-interface-library.h"

#define SENSOR_NAME "S5KGW1"
/* #define DEBUG_GW1_PLL */

static const struct v4l2_subdev_ops subdev_ops;

static const u32 *sensor_gw1_global;
static u32 sensor_gw1_global_size;
static const u32 **sensor_gw1_setfiles;
static const u32 *sensor_gw1_setfile_sizes;
static u32 sensor_gw1_max_setfile_num;
static const struct sensor_pll_info_compact **sensor_gw1_pllinfos;
static bool sensor_gw1_cal_write_flag;

#ifdef CONFIG_VENDER_MCD_V2
extern struct fimc_is_lib_support gPtr_lib_support;
#endif

static void sensor_gw1_set_integration_max_margin(u32 mode, cis_shared_data *cis_data)
{
	FIMC_BUG_VOID(!cis_data);

	cis_data->max_margin_coarse_integration_time = SENSOR_GW1_COARSE_INTEGRATION_TIME_MAX_MARGIN;
	dbg_sensor(1, "max_margin_coarse_integration_time(%d)\n",
		cis_data->max_margin_coarse_integration_time);
}

static void sensor_gw1_set_integration_min(u32 mode, cis_shared_data *cis_data)
{
	FIMC_BUG_VOID(!cis_data);

	cis_data->min_coarse_integration_time = SENSOR_GW1_COARSE_INTEGRATION_TIME_MIN;
	dbg_sensor(1, "min_coarse_integration_time(%d)\n",
		cis_data->min_coarse_integration_time);
}

#ifdef USE_AP_PDAF
static void sensor_gw1_cis_set_paf_stat_enable(u32 mode, cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	switch (mode) {
	case SENSOR_GW1_2X2BIN_4624X3468_30FPS:
	case SENSOR_GW1_2X2BIN_4624X2604_30FPS:
	case SENSOR_GW1_2X2BIN_4000X3000_30FPS:
	case SENSOR_GW1_2X2BIN_3296x1856_60FPS:
		cis_data->is_data.paf_stat_enable = true;
		break;
	default:
		cis_data->is_data.paf_stat_enable = false;
		break;
	}
}
#endif

#if SENSOR_GW1_DEBUG_INFO
static int sensor_gw1_cis_fine_integration_value(struct fimc_is_cis *cis)
{
	int ret = 0;
	u8 fine_integ_min = 0;
	u8 fine_integ_max_margin = 0;
	struct i2c_client *client;

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = fimc_is_sensor_write16(client, SENSOR_GW1_DIRECT_PAGE_ACCESS_MARK, SENSOR_GW1_DIRECT_DEFALT_PAGE_ADDR);
	if (ret < 0) {
		err("i2c fail addr(%x, %x) ,ret = %d\n",
			SENSOR_GW1_DIRECT_PAGE_ACCESS_MARK, SENSOR_GW1_DIRECT_DEFALT_PAGE_ADDR, ret);
		goto i2c_err;
	}
	ret = fimc_is_sensor_read8(client, 0x1008, &fine_integ_min);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto i2c_err;
	}
	ret = fimc_is_sensor_read8(client, 0x100A, &fine_integ_max_margin);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto i2c_err;
	}

	pr_info("%s : fine integration value. (%#x , %#x)\n",
		__func__, fine_integ_min, fine_integ_max_margin);

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}
#endif

static void sensor_gw1_cis_data_calculation(const struct sensor_pll_info_compact *pll_info, cis_shared_data *cis_data)
{
	u32 total_pixels = 0;
	u32 pixel_rate = 0;
	u32 frame_rate = 0, max_fps = 0, frame_valid_us = 0;

	FIMC_BUG_VOID(!pll_info);

	/* 1. get pclk value from pll info */
	pixel_rate = pll_info->pclk * TOTAL_NUM_OF_IVTPX_CHANNEL;
	total_pixels = pll_info->frame_length_lines * pll_info->line_length_pck;

	/* 2. FPS calculation */
	frame_rate = pixel_rate / (pll_info->frame_length_lines * pll_info->line_length_pck);

	/* 3. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = (1 * 1000 * 1000) / frame_rate;
	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;
#ifdef REAR_SUB_CAMERA
	cis_data->min_sync_frame_us_time = cis_data->min_frame_us_time;
#endif

	dbg_sensor(1, "frame_duration(%d) - frame_rate (%d) = pixel_rate(%u) / "
		KERN_CONT "(pll_info->frame_length_lines(%d) * pll_info->line_length_pck(%d))\n",
		cis_data->min_frame_us_time, frame_rate, pixel_rate, pll_info->frame_length_lines, pll_info->line_length_pck);


	/* calcurate max fps */
	max_fps = (pixel_rate * 10) / (pll_info->frame_length_lines * pll_info->line_length_pck);
	max_fps = (max_fps % 10 >= 5 ? frame_rate + 1 : frame_rate);

	cis_data->pclk = pixel_rate;
	cis_data->max_fps = max_fps;
	cis_data->frame_length_lines = pll_info->frame_length_lines;
	cis_data->line_length_pck = pll_info->line_length_pck;
	cis_data->line_readOut_time = sensor_cis_do_div64((u64)cis_data->line_length_pck * (u64)(1000 * 1000 * 1000), cis_data->pclk);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
	cis_data->max_coarse_integration_time =
		SENSOR_GW1_MAX_COARSE_INTEGRATION_TIME - SENSOR_GW1_COARSE_INTEGRATION_TIME_MAX_MARGIN;

	cis_data->stream_on = false;

	/* Frame valid time calculation */
	frame_valid_us = sensor_cis_do_div64((u64)cis_data->cur_height * (u64)cis_data->line_length_pck * (u64)(1000 * 1000), cis_data->pclk);
	cis_data->frame_valid_us_time = (int)frame_valid_us;

	dbg_sensor(1, "%s\n", __func__);
	dbg_sensor(1, "Sensor size(%d x %d) setting: SUCCESS!\n", cis_data->cur_width, cis_data->cur_height);
	dbg_sensor(1, "Frame Valid(%d us)\n", frame_valid_us);
	dbg_sensor(1, "rolling_shutter_skew: %lld\n", cis_data->rolling_shutter_skew);

	dbg_sensor(1, "Fps: %d, max fps(%d)\n", frame_rate, cis_data->max_fps);
	dbg_sensor(1, "min_frame_time(%d us)\n", cis_data->min_frame_us_time);
	dbg_sensor(1, "Pixel rate(%d KHz)\n", cis_data->pclk / 1000);
	dbg_sensor(1, "Line readout Time: %d\n", cis_data->line_readOut_time);

	/* Constant values */
	cis_data->min_fine_integration_time = SENSOR_GW1_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_margin_fine_integration_time = SENSOR_GW1_FINE_INTEGRATION_TIME_MAX_MARGIN;
	cis_data->max_fine_integration_time = cis_data->line_length_pck;

	/* Frame period calculation */
	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld) resolution(%d x %d)\n", __func__,
		cis_data->frame_time, cis_data->rolling_shutter_skew, cis_data->cur_width, cis_data->cur_height);

	info("%s: done", __func__);
}

static int sensor_gw1_wait_stream_off_status(cis_shared_data *cis_data)
{
	int ret = 0;
	u32 timeout = 0;

	FIMC_BUG(!cis_data);

#define STREAM_OFF_WAIT_TIME 500
	while (timeout < STREAM_OFF_WAIT_TIME) {
		if (cis_data->is_active_area == false &&
				cis_data->stream_on == false) {
			pr_debug("actual stream off\n");
			break;
		}
		timeout++;
	}

	if (timeout == STREAM_OFF_WAIT_TIME) {
		pr_err("actual stream off wait timeout\n");
		ret = -1;
	}

	return ret;
}

int sensor_gw1_cis_check_rev(struct fimc_is_cis *cis)
{
	int ret = 0;
	u8 rev8 = 0;
	u8 data1 = 0;
	u8 data2 = 0;
	struct i2c_client *client;

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

/* S5KGW1 CHIP REV Information  ***
 *  [ EVT 0.1 for Development ]
 *   -  ROM Revision Sample
 *  0x40000002 : 0xA1
 *  0x4000000D : 0x90
 *  0x4000000E : 0x13
 *
 *  [ EVT 0.2 for Manufacturing ]
 *   - ROM Revision + OTP Write
 *  0x40000002 : 0xA2
 *  0x4000000D : 0x90
 *  0x4000000E : 0x13
 *****************************/

	ret = fimc_is_sensor_write16(client, SENSOR_GW1_DIRECT_PAGE_ACCESS_MARK, SENSOR_GW1_DIRECT_DEFALT_PAGE_ADDR);
	if (ret < 0) {
		err("i2c fail addr(%x, %x) ,ret = %d\n",
			SENSOR_GW1_DIRECT_PAGE_ACCESS_MARK, SENSOR_GW1_DIRECT_DEFALT_PAGE_ADDR, ret);
		goto i2c_err;
	}
	ret = fimc_is_sensor_read8(client, SENSOR_GW1_REVISION_NUM_ADDR, &rev8);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto i2c_err;
	}
	cis->cis_data->cis_rev = rev8;

	ret = fimc_is_sensor_read8(client, 0x000D, &data1);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto i2c_err;
	}

	ret = fimc_is_sensor_read8(client, 0x000E, &data2);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto i2c_err;
	}

	pr_info("%s : rev checking. Rev. %#x (%#x , %#x)\n",
		__func__, cis->cis_data->cis_rev, data1, data2);

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gw1_cis_check_rev_on_init(struct v4l2_subdev *subdev)
{
#if 0
	int ret = 0;
	struct i2c_client *client;
	struct fimc_is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_gw1_cis_check_rev(cis);

	return ret;
#endif
	return 0;
}

#if SENSOR_GW1_CAL_DEBUG
int sensor_gw1_cis_cal_dump(char* name, char *buf, size_t size)
{
	int ret = 0;

	struct file *fp;
	ssize_t tx = -ENOENT;
	int fd, old_mask;
	loff_t pos = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	old_mask = sys_umask(0);

	sys_rmdir(name);
	fd = sys_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
	if (fd < 0) {
		err("open file error(%d): %s", fd, name);
		sys_umask(old_mask);
		set_fs(old_fs);
		ret = -EINVAL;
		goto p_err;
	}

	fp = fget(fd);
	if (fp) {
		tx = vfs_write(fp, buf, size, &pos);
		if (tx != size) {
			err("fail to write %s. ret %zd", name, tx);
			ret = -ENOENT;
		}

		vfs_fsync(fp, 0);
		fput(fp);
	} else {
		err("fail to get file *: %s", name);
	}

	sys_close(fd);
	sys_umask(old_mask);
	set_fs(old_fs);

p_err:
	return ret;
}
#endif

int sensor_gw1_cis_XTCCal_write(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	int position;
	ulong cal_addr;
	u8 cal_data[SENSOR_GW1_XTC_CAL_SIZE] = {0, };

#ifdef CONFIG_VENDER_MCD_V2
	char *rom_cal_buf = NULL;
#else
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	sensor_peri = container_of(cis, struct fimc_is_device_sensor_peri, cis);
	FIMC_BUG(!sensor_peri);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	position = sensor_peri->module->position;

#ifdef CONFIG_VENDER_MCD_V2
	ret = fimc_is_sec_get_cal_buf(position, &rom_cal_buf);

	if (ret < 0) {
		goto p_err;
	}

	cal_addr = (ulong)rom_cal_buf;
	if (position == SENSOR_POSITION_REAR) {
		cal_addr += SENSOR_GW1_XTC_CAL_BASE_REAR;
	} else {
		err("cis_gw1 position(%d) is invalid!\n", position);
		goto p_err;
	}
#else
	if (position == SENSOR_POSITION_REAR){
		cal_addr = lib->minfo->kvaddr_cal[position] + SENSOR_GW1_XTC_CAL_BASE_REAR;
	}else {
		err("cis_gw1 position(%d) is invalid!\n", position);
		goto p_err;
	}
#endif

	memcpy(cal_data, (u16 *)cal_addr, SENSOR_GW1_XTC_CAL_SIZE);

#if SENSOR_GW1_CAL_DEBUG
	ret = sensor_gw1_cis_cal_dump(SENSOR_GW1_XTC_DUMP_NAME, (char *)cal_data, (size_t)SENSOR_GW1_XTC_CAL_SIZE);
	if (ret < 0) {
		err("cis_gw1 XTC Cal dump fail(%d)!\n", ret);
	}
#endif

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* page select */
	ret = fimc_is_sensor_write16(client, SENSOR_GW1_INDIRECT_WRITE_PAGE_ADDR, SENSOR_GW1_XTC_PAGE_ADDR);
	if (ret < 0) {
		goto i2c_err;
	}
	/* write register address ( in auto increment reg) */
	ret = fimc_is_sensor_write16(client, SENSOR_GW1_INDIRECT_WRITE_OFFSET_ADDR, SENSOR_GW1_XTC_REG_ADDR);
	if (ret < 0) {
		goto i2c_err;
	}
	/* write data in write register */
	ret = fimc_is_sensor_write8_sequential(client, SENSOR_GW1_INDIRECT_WRITE_DATA_ADDR, cal_data, SENSOR_GW1_XTC_CAL_SIZE);
	if (ret < 0) {
		goto i2c_err;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	/* load XTC additional settings */
	ret = sensor_cis_set_registers(subdev, sensor_gw1_xtc_additional_setting, ARRAY_SIZE(sensor_gw1_xtc_additional_setting));
	if (ret < 0) {
		err("sensor_gw1_set_registers fail!!");
		goto p_err;
	}
	pr_info("XTC loaded\n");
	return ret;

i2c_err:
	if (ret < 0) {
		err("cis_gw1 XTC write Error(%d)\n", ret);
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

/* CIS OPS */
int sensor_gw1_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	u32 setfile_index = 0;
	cis_setting_info setinfo;
#ifdef USE_CAMERA_HW_BIG_DATA
	struct cam_hw_param *hw_param = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
#endif

	setinfo.param = NULL;
	setinfo.return_value = 0;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	FIMC_BUG(!cis->cis_data);
	memset(cis->cis_data, 0, sizeof(cis_shared_data));
	cis->rev_flag = false;

	probe_info("%s GW1 init\n", __func__);

	ret = sensor_gw1_cis_check_rev(cis);
	if (ret < 0) {
#ifdef USE_CAMERA_HW_BIG_DATA
		sensor_peri = container_of(cis, struct fimc_is_device_sensor_peri, cis);
		if (sensor_peri)
			fimc_is_sec_get_hw_param(&hw_param, sensor_peri->module->position);
		if (hw_param)
			hw_param->i2c_sensor_err_cnt++;
#endif
		warn("sensor_gw1_check_rev is fail when cis init");
		cis->rev_flag = true;

		ret = 0;
		goto p_err;
	}

	/***********************************************************************
	***** Check that XTC Cal is written for Remosaic Capture.
	***** false : Not yet write the XTC
	***** true  : Written the XTC Or Skip
	***********************************************************************/
	sensor_gw1_cal_write_flag = false;

	if (cis->cis_data->cis_rev <= 0xA1) {
		probe_info("%s setfile_A\n", __func__);
		sensor_gw1_global = sensor_gw1_setfile_A_global;
		sensor_gw1_global_size = ARRAY_SIZE(sensor_gw1_setfile_A_global);
		sensor_gw1_setfiles = sensor_gw1_setfiles_A;
		sensor_gw1_setfile_sizes = sensor_gw1_setfile_A_sizes;
		sensor_gw1_pllinfos = sensor_gw1_pllinfos_A;
		sensor_gw1_max_setfile_num = ARRAY_SIZE(sensor_gw1_setfiles_A);
	} else {
		probe_info("%s setfile_B", __func__);
		sensor_gw1_global = sensor_gw1_setfile_B_global;
		sensor_gw1_global_size = ARRAY_SIZE(sensor_gw1_setfile_B_global);
		sensor_gw1_setfiles = sensor_gw1_setfiles_B;
		sensor_gw1_setfile_sizes = sensor_gw1_setfile_B_sizes;
		sensor_gw1_pllinfos = sensor_gw1_pllinfos_B;
		sensor_gw1_max_setfile_num = ARRAY_SIZE(sensor_gw1_setfiles_B);
	}

	cis->cis_data->product_name = cis->id;
	cis->cis_data->cur_width = SENSOR_GW1_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_GW1_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;

	sensor_gw1_cis_data_calculation(sensor_gw1_pllinfos[setfile_index], cis->cis_data);
	sensor_gw1_set_integration_max_margin(setfile_index, cis->cis_data);
	sensor_gw1_set_integration_min(setfile_index, cis->cis_data);

#if SENSOR_GW1_DEBUG_INFO
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min dgain : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max dgain : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
#endif

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}
int sensor_gw1_cis_dump_registers(struct v4l2_subdev *subdev, const u32 *regs, const u32 size)
{
	int ret = 0;
	int i = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	u8 data8 = 0;
	u16 data16 = 0;

	FIMC_BUG(!subdev);
	FIMC_BUG(!regs);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}


	for (i = 0; i < size; i += I2C_NEXT) {
		if (regs[i + I2C_BYTE] == 0x2 && regs[i + I2C_ADDR] == 0x6028) {
			pr_err("[SEN:DUMP WRITE] [0x%04X, 0x%04X\n", regs[i + I2C_ADDR], regs[i + I2C_DATA]);
			ret = fimc_is_sensor_write16(client, SENSOR_GW1_INDIRECT_READ_PAGE_ADDR,  regs[i + I2C_DATA]);
		}
		if (regs[i + I2C_BYTE] == 0x2 && regs[i + I2C_ADDR] == 0x602A) {
			pr_err("[SEN:DUMP WRITE] [0x%04X, 0x%04X\n", regs[i + I2C_ADDR], regs[i + I2C_DATA]);
			 ret = fimc_is_sensor_write16(client, SENSOR_GW1_INDIRECT_READ_OFFSET_ADDR, regs[i + I2C_DATA]);
		}

		if (regs[i + I2C_BYTE] == 0x1) {
			ret = fimc_is_sensor_read8(client, regs[i + I2C_ADDR], &data8);
			if (ret < 0) {
				err("fimc_is_sensor_read8 fail, ret(%d), addr(%#x)",
						ret, regs[i + I2C_ADDR]);
			}
			pr_err("[SEN:DUMP]111111 [0x%04X, 0x%04X\n", regs[i + I2C_ADDR], data8);
		} else {

			ret = fimc_is_sensor_read16(client, regs[i + I2C_ADDR], &data16);
			if (ret < 0) {
				err("fimc_is_sensor_read6 fail, ret(%d), addr(%#x)",
						ret, regs[i + I2C_ADDR]);
			}
			pr_err("[SEN:DUMP] 222222[0x%04X, 0x%04X\n", regs[i + I2C_ADDR], data16);
		}
	}

p_err:
	return ret;
}

int sensor_gw1_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client = NULL;
	u8 data8 = 0;
	u16 data16 = 0;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = fimc_is_sensor_write16(client, SENSOR_GW1_DIRECT_PAGE_ACCESS_MARK, SENSOR_GW1_DIRECT_DEFALT_PAGE_ADDR);
	if (ret < 0) {
		err("i2c fail addr(%x, %x) ,ret = %d\n",
			SENSOR_GW1_DIRECT_PAGE_ACCESS_MARK, SENSOR_GW1_DIRECT_DEFALT_PAGE_ADDR, ret);
		goto i2c_err;
	}

	pr_info("[%s] *******************************\n", __func__);
	ret = fimc_is_sensor_read16(client, SENSOR_GW1_MODEL_ID_ADDR, &data16);
	if (unlikely(!ret)) pr_info("[SEN:INFO]model_id(0x%x)\n", data16);
	else goto i2c_err;
	ret = fimc_is_sensor_read8(client, SENSOR_GW1_REVISION_NUM_ADDR, &data8);
	if (unlikely(!ret)) pr_info("[SEN:INFO]revision_number(0x%x)\n", data8);
	else goto i2c_err;
	ret = fimc_is_sensor_read8(client, SENSOR_GW1_FRAME_COUNT_ADDR, &data8);
	if (unlikely(!ret)) pr_info("[SEN:INFO]frame_count(0x%x)\n", data8);
	else goto i2c_err;
	ret = fimc_is_sensor_read8(client, SENSOR_GW1_SETUP_MODE_SELECT_ADDR, &data8);
	if (unlikely(!ret)) pr_info("[SEN:INFO]setup_mode(streaming)(0x%x)\n", data8);
	else goto i2c_err;
	ret = fimc_is_sensor_read16(client, SENSOR_GW1_EXTCLK_FREQ_ADDR, &data16);
	if (unlikely(!ret)) pr_info("[SEN:INFO]extclk_feq_mhz(0x%x)\n", data16);
	else goto i2c_err;
	ret = fimc_is_sensor_read16(client, SENSOR_GW1_CORASE_INTEG_TIME_ADDR, &data16);
	if (unlikely(!ret)) pr_info("[SEN:INFO]coarse_integration_time(0x%x)\n", data16);
	else goto i2c_err;
	ret = fimc_is_sensor_read16(client, SENSOR_GW1_AGAIN_CODE_GLOBAL_ADDR, &data16);
	if (unlikely(!ret)) pr_info("[SEN:INFO]gain_code_global(0x%x)\n", data16);
	else goto i2c_err;
	ret = fimc_is_sensor_read16(client, SENSOR_GW1_FRAME_LENGTH_LINE_ADDR, &data16);
	if (unlikely(!ret)) pr_info("[SEN:INFO]frame_length_line(0x%x)\n", data16);
	else goto i2c_err;
	ret = fimc_is_sensor_read16(client, SENSOR_GW1_LINE_LENGTH_PCK_ADDR, &data16);
	if (unlikely(!ret)) pr_info("[SEN:INFO]line_length_pck(0x%x)\n", data8);
	else goto i2c_err;

	pr_info("[%s] *************Mode settings dump start******************\n", __func__);
	 sensor_gw1_cis_dump_registers(subdev, sensor_gw1_setfiles[0], sensor_gw1_setfile_sizes[0]);
	pr_info("[%s] *************Mode settings dump end******************\n", __func__);

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

static int sensor_gw1_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{
#if USE_GROUP_PARAM_HOLD
	int ret = 0;
	struct fimc_is_cis *cis = NULL;
	struct i2c_client *client = NULL;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (hold == cis->cis_data->group_param_hold) {
		pr_debug("already group_param_hold (%d)\n", cis->cis_data->group_param_hold);
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = fimc_is_sensor_write8(client, SENSOR_GW1_GROUP_PARAM_HOLD_ADDR, hold);
	if (ret < 0)
		goto i2c_err;

	cis->cis_data->group_param_hold = hold;
	ret = 1;

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
p_err:
	return ret;
#else

	return 0;
#endif
}

/* Input
 *	hold : true - hold, flase - no hold
 * Output
 *      return: 0 - no effect(already hold or no hold)
 *		positive - setted by request
 *		negative - ERROR value
 */
int sensor_gw1_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	ret = sensor_gw1_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err;

p_err:
	return ret;
}

int sensor_gw1_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* setfile global setting is at camera entrance */
	info("[%s] global setting start\n", __func__);
	ret = sensor_cis_set_registers(subdev, sensor_gw1_global, sensor_gw1_global_size);
	if (ret < 0) {
		err("sensor_gw1_set_registers fail!!");
		goto p_err;
	}

	dbg_sensor(1, "[%s] global setting done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	// Check that XTC Cal is written for Remosaic Capture.
	// false : XTC not yet written
	// true  : XTC written
	sensor_gw1_cal_write_flag = false;

	return ret;
}

int sensor_gw1_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;
	struct fimc_is_device_sensor *device;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	device = (struct fimc_is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	if (unlikely(!device)) {
		err("device sensor is null");
		ret = -EINVAL;
		goto p_err;
	}

	if (mode >= sensor_gw1_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	/* If check_rev of S5KGW1 fail when cis_init, one more check_rev in mode_change */
	if (cis->rev_flag == true) {
		cis->rev_flag = false;
		ret = sensor_gw1_cis_check_rev(cis);
		if (ret < 0) {
			err("sensor_gw1_check_rev is fail");
			goto p_err;
		}
	}

#if SENSOR_GW1_WRITE_XTC_CAL
	if (IS_REMOSAIC(mode) && sensor_gw1_cal_write_flag == false) {
		sensor_gw1_cal_write_flag = true;

		info("[%s] mode is XTC Remosaic Mode! Write XTC data.\n", __func__);

		ret = sensor_gw1_cis_XTCCal_write(subdev);
		if (ret < 0) {
			err("sensor_gw1_XTC_Cal_write fail!! (%d)", ret);
			goto p_err;
		}
	}
#endif
#ifdef USE_AP_PDAF
	sensor_gw1_cis_set_paf_stat_enable(mode, cis->cis_data);
#endif
	I2C_MUTEX_LOCK(cis->i2c_lock);

	info("[%s] mode setting start\n", __func__);
	ret = sensor_cis_set_registers(subdev, sensor_gw1_setfiles[mode], sensor_gw1_setfile_sizes[mode]);
	if (ret < 0) {
		err("sensor_gw1_set_registers fail!!");
		goto i2c_err;
	}

	dbg_sensor(1, "[%s] mode changed(%d)\n", __func__, mode);

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

#if SENSOR_GW1_DEBUG_INFO
	sensor_gw1_cis_fine_integration_value(cis);
#endif

p_err:
	return ret;
}

/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_gw1_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
	bool binning = false;
	u32 ratio_w = 0, ratio_h = 0, start_x = 0, start_y = 0, end_x = 0, end_y = 0;
	u32 even_x= 0, odd_x = 0, even_y = 0, odd_y = 0;
	struct i2c_client *client = NULL;
	struct fimc_is_cis *cis = NULL;
#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif
	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	if (unlikely(!cis_data)) {
		err("cis data is NULL");
		if (unlikely(!cis->cis_data)) {
			ret = -EINVAL;
			goto p_err;
		} else {
			cis_data = cis->cis_data;
		}
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* Wait actual stream off */
	ret = sensor_gw1_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off\n");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (SENSOR_GW1_MAX_WIDTH / cis_data->cur_width);
		ratio_h = (SENSOR_GW1_MAX_HEIGHT / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > SENSOR_GW1_MAX_WIDTH) ||
		((cis_data->cur_height * ratio_h) > SENSOR_GW1_MAX_HEIGHT)) {
		err("Config max sensor size over~!!\n");
		ret = -EINVAL;
		goto p_err;
	}

	/* 1. page_select */
	ret = fimc_is_sensor_write16(client, 0x6028, 0x2000);
	if (ret < 0)
		 goto p_err;

	/* 2. pixel address region setting */
	start_x = ((SENSOR_GW1_MAX_WIDTH - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((SENSOR_GW1_MAX_HEIGHT - cis_data->cur_height * ratio_h) / 2) & (~0x1);
	end_x = start_x + (cis_data->cur_width * ratio_w - 1);
	end_y = start_y + (cis_data->cur_height * ratio_h - 1);

	if (!(end_x & (0x1)) || !(end_y & (0x1))) {
		err("Sensor pixel end address must odd\n");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_sensor_write16(client, 0x0344, start_x);
	if (ret < 0)
		 goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0346, start_y);
	if (ret < 0)
		 goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0348, end_x);
	if (ret < 0)
		 goto p_err;
	ret = fimc_is_sensor_write16(client, 0x034A, end_y);
	if (ret < 0)
		 goto p_err;

	/* 3. output address setting */
	ret = fimc_is_sensor_write16(client, 0x034C, cis_data->cur_width);
	if (ret < 0)
		 goto p_err;
	ret = fimc_is_sensor_write16(client, 0x034E, cis_data->cur_height);
	if (ret < 0)
		 goto p_err;

	/* If not use to binning, sensor image should set only crop */
	if (!binning) {
		dbg_sensor(1, "Sensor size set is not binning\n");
		goto p_err;
	}

	/* 4. sub sampling setting */
	even_x = 1;	/* 1: not use to even sampling */
	even_y = 1;
	odd_x = (ratio_w * 2) - even_x;
	odd_y = (ratio_h * 2) - even_y;

	ret = fimc_is_sensor_write16(client, 0x0380, even_x);
	if (ret < 0)
		 goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0382, odd_x);
	if (ret < 0)
		 goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0384, even_y);
	if (ret < 0)
		 goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0386, odd_y);
	if (ret < 0)
		 goto p_err;

	/* 5. binnig setting */
	ret = fimc_is_sensor_write8(client, 0x0900, binning);	/* 1:  binning enable, 0: disable */
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write8(client, 0x0901, (ratio_w << 4) | ratio_h);
	if (ret < 0)
		goto p_err;

	/* 6. scaling setting: but not use */
	/* scaling_digital_scaling */
	ret = fimc_is_sensor_write16(client, 0x0402, 0x1010);
	if (ret < 0)
		goto p_err;
	/* scaling_hbin_digital_binning_factor */
	ret = fimc_is_sensor_write16(client, 0x0404, 0x0010);
	if (ret < 0)
		goto p_err;
	/* scaling_tetracell_digital_binning_factor */
	ret = fimc_is_sensor_write16(client, 0x0400, 0x1010);
	if (ret < 0)
		goto p_err;

	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_gw1_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	sensor_gw1_cis_group_param_hold(subdev, 0x01);
	if (ret < 0)
		err("group_param_hold_func failed at stream on");

	I2C_MUTEX_LOCK(cis->i2c_lock);

#ifdef SENSOR_GW1_DEBUG_INFO
	{
	u16 pll;
	fimc_is_sensor_read16(client, 0x0300, &pll);
	dbg_sensor(1, "______ vt_pix_clk_div(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x0302, &pll);
	dbg_sensor(1, "______ vt_sys_clk_div(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x0304, &pll);
	dbg_sensor(1, "______ pre_pll_clk_div(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x0306, &pll);
	dbg_sensor(1, "______ pll_multiplier(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x030a, &pll);
	dbg_sensor(1, "______ op_sys_clk_div(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x030c, &pll);
	dbg_sensor(1, "______ op_prepllck_div(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x030e, &pll);
	dbg_sensor(1, "______ op_pll_multiplier(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x0310, &pll);
	dbg_sensor(1, "______ pll_mult_driv(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x0340, &pll);
	dbg_sensor(1, "______ frame_length_lines(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x0342, &pll);
	dbg_sensor(1, "______ line_length_pck(%x)\n", pll);
	}
#endif

	info("[%s] start\n", __func__);

	/* Sensor stream on */
	ret = fimc_is_sensor_write16(client, SENSOR_GW1_SETUP_MODE_SELECT_ADDR, 0x0100);
	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0100, 0x0100, ret);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	sensor_gw1_cis_group_param_hold(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream on");

#if 0 //SENSOR_GW1_DEBUG_INFO
/* For debug - fcount check */
	{
		u16 value, timeout, addr;
		timeout = 0;
		addr = 0x3000; /* smiaRegs_vendor_error_type */
		fimc_is_sensor_read16(client, addr, &value);
		info("%s - 0x%x(0x%x)\n", __func__, addr, value);
		while (timeout < 10) {
			addr = 0x0005; /* smiaRegs_rd_general_frame_count */
			fimc_is_sensor_read16(client, addr, &value);
			info("%s - 0x%x(0x%x)\n", __func__, addr, value);
			msleep(5);
			timeout++;
		}
	}
#endif

	cis_data->stream_on = true;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_gw1_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	ret = sensor_gw1_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* Sensor stream off */
	ret = fimc_is_sensor_write16(client, SENSOR_GW1_SETUP_MODE_SELECT_ADDR, 0x0000);
	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0100, 0x0000, ret);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	info("%s\n", __func__);
	cis_data->stream_on = false;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_gw1_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 coarse_integ_time = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;
	u32 max_frame_us_time = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_duration);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	coarse_integ_time = (u32)(((vt_pic_clk_freq_khz * input_exposure_time) / 1000
									- cis_data->min_fine_integration_time) / line_length_pck);
	
	if (cis->min_fps == cis->max_fps) {
		dbg_sensor(1, "[%s] requested min_fps(%d), max_fps(%d) from HAL\n", __func__, cis->min_fps, cis->max_fps);

		if (coarse_integ_time > cis_data->max_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), coarse(%u) max(%u)\n", cis->id, __func__,
				cis_data->sen_vsync_count, coarse_integ_time, cis_data->max_coarse_integration_time);
			coarse_integ_time = cis_data->max_coarse_integration_time;
		}

		if (coarse_integ_time < cis_data->min_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), coarse(%u) min(%u)\n", cis->id, __func__,
				cis_data->sen_vsync_count, coarse_integ_time, cis_data->min_coarse_integration_time);
			coarse_integ_time = cis_data->min_coarse_integration_time;
		}
	}
	
	frame_length_lines = coarse_integ_time + cis_data->max_margin_coarse_integration_time;
	frame_duration = (u32)(((u64)frame_length_lines * line_length_pck) * 1000 / vt_pic_clk_freq_khz);
	max_frame_us_time = 1000000/cis->min_fps;

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), calculated frame duraion(%d), min_frame_us(%d), max_frame_us_time(%d)\n",
			__func__, cis_data->sen_vsync_count, input_exposure_time, frame_duration, cis_data->min_frame_us_time, max_frame_us_time);

	*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);

	/*
	 * For recording with fixed fps (>= 10fps).
	 * If input exposure is set as 33333us@30fps from ddk,
	 * then calculated frame duration is larger than 33333us because of CIT MARGIN.
	 */
	if ((cis_data->min_frame_us_time <= 100000) && (cis->min_fps == cis->max_fps)) {
		*target_duration = MIN(frame_duration, max_frame_us_time);
	}

	dbg_sensor(1, "[%s] calcurated frame_duration(%d), adjusted frame_duration(%d)\n", __func__, frame_duration, *target_duration);
#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_gw1_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct fimc_is_long_term_expo_mode *lte_mode;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	lte_mode = &cis->long_term_mode;

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
		frame_duration = cis_data->min_frame_us_time;
	}

	/*
	 * For Long Exposure Mode without stream on_off. (ex. Night HyperLapse)
	 * If frame duration over than 1sec, then it has to be applied CIT shift.
	 * In this case, frame_duration is set in set_exposure_time with CIT shift.
	 */
	if (lte_mode->sen_strm_off_on_enable == false && cis_data ->min_frame_us_time > 1000000) {
		cis_data->cur_frame_us_time = frame_duration;
		dbg_sensor(1, "[MOD:D:%d][%s] Skip set frame duration(%d) for CIT SHIFT.\n",
			cis->id, __func__, frame_duration);
		return ret;
	}

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz(%#x) frame_duration = %d us,"
		KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x)\n",
		cis->id, __func__, vt_pic_clk_freq_khz, frame_duration, line_length_pck, frame_length_lines);

	hold = sensor_gw1_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	if (lte_mode->sen_strm_off_on_enable == false && cis_data->frame_length_lines_shifter > 0) {
		cis_data->frame_length_lines_shifter = 0;
		ret = fimc_is_sensor_write8(client, SENSOR_GW1_CIT_LSHIFT_ADDR, 0);
		ret += fimc_is_sensor_write8(client, SENSOR_GW1_FRM_LENGTH_LINE_LSHIFT_ADDR, 0);
		if (ret < 0)
			goto i2c_err;
	}

	ret = fimc_is_sensor_write16(client, SENSOR_GW1_FRAME_LENGTH_LINE_ADDR, frame_length_lines);
	if (ret < 0)
		goto i2c_err;

	cis_data->cur_frame_us_time = frame_duration;
	cis_data->frame_length_lines = frame_length_lines;
	if (lte_mode->sen_strm_off_on_enable == true) {
		cis_data->max_coarse_integration_time =
			SENSOR_GW1_MAX_COARSE_INTEGRATION_TIME - cis_data->max_margin_coarse_integration_time;
	} else {
		cis_data->max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_gw1_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_gw1_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

	u32 frame_duration = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	if (min_fps > cis_data->max_fps) {
		err("[MOD:D:%d] %s, request FPS is too high(%d), set to max_fps(%d)\n",
			cis->id, __func__, min_fps, cis_data->max_fps);
		min_fps = cis_data->max_fps;
	}

	if (min_fps == 0) {
		err("[MOD:D:%d] %s, request FPS is 0, set to min FPS(1)\n", cis->id, __func__);
		min_fps = 1;
	}

	frame_duration = (1 * 1000 * 1000) / min_fps;

	dbg_sensor(1, "[MOD:D:%d] %s, set FPS(%d), frame duration(%d)\n",
			cis->id, __func__, min_fps, frame_duration);

	ret = sensor_gw1_cis_set_frame_duration(subdev, frame_duration);
	if (ret < 0) {
		err("[MOD:D:%d] %s, set frame duration is fail(%d)\n",
			cis->id, __func__, ret);
		goto p_err;
	}

	cis_data->min_frame_us_time = frame_duration;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_gw1_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct fimc_is_long_term_expo_mode *lte_mode;

	u64 vt_pic_clk_freq_khz = 0;
	u16 short_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u32 target_exp = 0;
	u32 target_frame_duration = 0;
	u16 frame_length_lines = 0;

	unsigned char cit_lshift_val = 0;
	int cit_lshift_count = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_exposure);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	lte_mode = &cis->long_term_mode;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	target_exp = target_exposure->val;
	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz (%d), line_length_pck(%d), min_fine_int (%d)\n",
		cis->id, __func__, vt_pic_clk_freq_khz, line_length_pck, min_fine_int);

	/*
	 * For Long Exposure Mode without stream on_off. (ex. Night Hyper Laps: min exp. is 1.5sec)
	 * If frame duration over than 1sec, then sequence is same as below
	 * 1. set CIT_LSHFIT
	 * 2. set COARSE_INTEGRATION_TIME
	 * 3. set FRM_LENGTH_LINES
	 * 4. set FRM_LENGTH_LINE_LSHIFT
	 */
	if (lte_mode->sen_strm_off_on_enable == false && cis_data ->min_frame_us_time > 1000000) {
		target_frame_duration = cis_data->cur_frame_us_time;
		dbg_sensor(1, "[MOD:D:%d] %s, input frame duration(%d) for CIT SHIFT \n",
			cis->id, __func__, target_frame_duration);

		if (target_frame_duration > 100000) {
			cit_lshift_val = (unsigned char)(target_frame_duration / 100000);
			while(cit_lshift_val > 1) {
				cit_lshift_val /= 2;
				target_frame_duration /= 2;
				target_exp /= 2;
				cit_lshift_count++;
			}

			if (cit_lshift_count > SENSOR_GW1_MAX_CIT_LSHIFT_VALUE)
				cit_lshift_count = SENSOR_GW1_MAX_CIT_LSHIFT_VALUE;
		}

		frame_length_lines = (u16)((vt_pic_clk_freq_khz * target_frame_duration) / (line_length_pck * 1000));

		cis_data->frame_length_lines = frame_length_lines;
		cis_data->frame_length_lines_shifter = cit_lshift_count;
		cis_data->max_coarse_integration_time =
			frame_length_lines - cis_data->max_margin_coarse_integration_time;

		dbg_sensor(1, "[MOD:D:%d] %s, target_frame_duration(%d), frame_length_line(%d), cit_lshift_count(%d)\n",
			cis->id, __func__, target_frame_duration, frame_length_lines, cit_lshift_count);
	}

	short_coarse_int = (u16)(((target_exp * vt_pic_clk_freq_khz) - min_fine_int) / (line_length_pck * 1000));

	if (short_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->max_coarse_integration_time);
		short_coarse_int = cis_data->max_coarse_integration_time;
	}

	if (short_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time);
		short_coarse_int = cis_data->min_coarse_integration_time;
	}

	hold = sensor_gw1_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	if (lte_mode->sen_strm_off_on_enable == false && cis_data ->min_frame_us_time > 1000000) {
		if (cit_lshift_count > 0) {
			ret = fimc_is_sensor_write8(client, SENSOR_GW1_CIT_LSHIFT_ADDR, cit_lshift_count);
			ret += fimc_is_sensor_write8(client, SENSOR_GW1_FRM_LENGTH_LINE_LSHIFT_ADDR, cit_lshift_count);
			if (ret < 0)
				goto i2c_err;
		}
	}

	/* Short exposure */
	ret = fimc_is_sensor_write16(client, SENSOR_GW1_CORASE_INTEG_TIME_ADDR, short_coarse_int);
	if (ret < 0)
		goto i2c_err;

	if (lte_mode->sen_strm_off_on_enable == false && cis_data ->min_frame_us_time > 1000000) {
		ret = fimc_is_sensor_write16(client, SENSOR_GW1_FRAME_LENGTH_LINE_ADDR, frame_length_lines);
		if (ret < 0)
			goto i2c_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_khz (%d),"
		KERN_CONT "line_length_pck(%d), min_fine_int (%d)\n", cis->id, __func__,
		cis_data->sen_vsync_count, vt_pic_clk_freq_khz, line_length_pck, min_fine_int);
	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), frame_length_lines(%#x),"
		KERN_CONT "short_coarse_int %#x\n", cis->id, __func__,
		cis_data->sen_vsync_count, cis_data->frame_length_lines, short_coarse_int);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_gw1_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_gw1_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;
	cis_shared_data *cis_data = NULL;
	u32 min_integration_time = 0;
	u32 min_coarse = 0;
	u32 min_fine = 0;
	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_expo);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_coarse = cis_data->min_coarse_integration_time;
	min_fine = cis_data->min_fine_integration_time;

	min_integration_time = (u32)((u64)((line_length_pck * min_coarse) + min_fine) * 1000 / vt_pic_clk_freq_khz);
	*min_expo = min_integration_time;

	dbg_sensor(1, "[%s] min integration time %d\n", __func__, min_integration_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_gw1_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;
	u32 max_integration_time = 0;
	u32 max_coarse_margin = 0;
	u32 max_fine_margin = 0;
	u32 max_coarse = 0;
	u32 max_fine = 0;
	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_expo);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = cis_data->frame_length_lines;

	max_coarse_margin = cis_data->max_margin_coarse_integration_time;
	max_coarse = frame_length_lines - max_coarse_margin;
	max_fine = cis_data->max_fine_integration_time;
	max_fine_margin = line_length_pck - max_fine;

	max_integration_time = (u32)((u64)((line_length_pck * max_coarse) + max_fine) * 1000 / vt_pic_clk_freq_khz);

	*max_expo = max_integration_time;

	cis_data->max_margin_fine_integration_time = max_fine_margin;
	cis_data->max_coarse_integration_time = max_coarse;

	dbg_sensor(1, "[%s] max integration time %d, max margin fine integration %d, max coarse integration %d\n",
			__func__, max_integration_time, cis_data->max_margin_fine_integration_time, cis_data->max_coarse_integration_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_gw1_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

	u32 again_code = 0;
	u32 again_permile = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_permile);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	again_code = sensor_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0]) {
		again_code = cis_data->max_analog_gain[0];
	} else if (again_code < cis_data->min_analog_gain[0]) {
		again_code = cis_data->min_analog_gain[0];
	}

	again_permile = sensor_cis_calc_again_permile(again_code);

	dbg_sensor(1, "[%s] min again(%d), max(%d), input_again(%d), code(%d), permile(%d)\n", __func__,
			cis_data->max_analog_gain[0],
			cis_data->min_analog_gain[0],
			input_again,
			again_code,
			again_permile);

	*target_permile = again_permile;

	return ret;
}

int sensor_gw1_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	analog_gain = (u16)sensor_cis_calc_again_code(again->val);

	if (analog_gain < cis->cis_data->min_analog_gain[0]) {
		analog_gain = cis->cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis->cis_data->max_analog_gain[0]) {
		analog_gain = cis->cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again = %d us, analog_gain(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count, again->val, analog_gain);

	hold = sensor_gw1_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = fimc_is_sensor_write16(client, SENSOR_GW1_AGAIN_CODE_GLOBAL_ADDR, analog_gain);
	if (ret < 0)
		goto i2c_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_gw1_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_gw1_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	hold = sensor_gw1_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = fimc_is_sensor_read16(client, SENSOR_GW1_AGAIN_CODE_GLOBAL_ADDR, &analog_gain);
	if (ret < 0)
		goto i2c_err;

	*again = sensor_cis_calc_again_permile(analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_gw1_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_gw1_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;
	u16 min_again_code = 0x20;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	cis_data->min_analog_gain[0] = min_again_code;
	cis_data->min_analog_gain[1] = sensor_cis_calc_again_permile(min_again_code);

	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] min_again_code %d, min_again_permile %d\n", __func__,
		cis_data->min_analog_gain[0], cis_data->min_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_gw1_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;
	u16 max_again_code = 0x200;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	cis_data->max_analog_gain[0] = max_again_code;
	cis_data->max_analog_gain[1] = sensor_cis_calc_again_permile(max_again_code);

	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] max_again_code %d, max_again_permile %d\n", __func__,
		cis_data->max_analog_gain[0], cis_data->max_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_gw1_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 short_dgain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	short_dgain = (u16)sensor_cis_calc_dgain_code(dgain->short_val);

	if (short_dgain < cis->cis_data->min_digital_gain[0]) {
		short_dgain = cis->cis_data->min_digital_gain[0];
	}
	if (short_dgain > cis->cis_data->max_digital_gain[0]) {
		short_dgain = cis->cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_short_dgain = %d us, short_dgain(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count, dgain->short_val, short_dgain);

	hold = sensor_gw1_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* Short digital gain */
	ret = fimc_is_sensor_write16(client, SENSOR_GW1_DGAIN_GLOBAL_ADDR, short_dgain);
	if (ret < 0)
		goto i2c_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_gw1_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_gw1_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;

	u16 digital_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	hold = sensor_gw1_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = fimc_is_sensor_read16(client, SENSOR_GW1_DGAIN_GLOBAL_ADDR, &digital_gain);
	if (ret < 0)
		goto i2c_err;

	*dgain = sensor_cis_calc_dgain_permile(digital_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_dgain = %d us, digital_gain(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_gw1_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_gw1_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;
	u16 min_dgain_code = 0x0100;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	cis_data->min_digital_gain[0] = min_dgain_code;
	cis_data->min_digital_gain[1] = sensor_cis_calc_dgain_permile(min_dgain_code);

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(1, "[%s] min_dgain_code %d, min_dgain_permile %d\n", __func__,
		cis_data->min_digital_gain[0], cis_data->min_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_gw1_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;
	u16 max_dgain_code = 0x8000;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	cis_data->max_digital_gain[0] = max_dgain_code;
	cis_data->max_digital_gain[1] = sensor_cis_calc_dgain_permile(max_dgain_code);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] max_dgain_code %d, max_dgain_permile %d\n", __func__,
		cis_data->max_digital_gain[0], cis_data->max_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

void sensor_gw1_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	FIMC_BUG_VOID(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG_VOID(!cis);
	FIMC_BUG_VOID(!cis->cis_data);

	if (mode > sensor_gw1_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	/* If check_rev fail when cis_init, one more check_rev in mode_change */
	if (cis->rev_flag == true) {
		cis->rev_flag = false;
		ret = sensor_gw1_cis_check_rev(cis);
		if (ret < 0) {
			err("sensor_gw1_check_rev is fail: ret(%d)", ret);
			return;
		}
	}

	sensor_gw1_cis_data_calculation(sensor_gw1_pllinfos[mode], cis->cis_data);
}

int sensor_gw1_cis_set_adjust_sync(struct v4l2_subdev *subdev, u32 sync)
{
	int ret = 0;

	return ret;
}

int sensor_gw1_cis_long_term_exposure(struct v4l2_subdev *subdev)
{
/*
	relation between shutter UI and lshift value
	lshift_val = log2(shift)
	+------------+-----------+-------------+
	| Shutter UI | CIT_Shift | exp (in us) |
	+------------+-----------+-------------+
	| 1/10s      | 1         | 100000      |
	+------------+-----------+-------------+
	| 1/8s       | 1         | 125000      |
	+------------+-----------+-------------+
	| 1/6s       | 1         | 166666      |
	+------------+-----------+-------------+
	| 1/4s       | 1         | 250000      |
	+------------+-----------+-------------+
	| 0.3s       | 1         | 300000      |
	+------------+-----------+-------------+
	| 0.5s       | 4         | 125000      |
	+------------+-----------+-------------+
	| 1s         | 8         | 125000      |
	+------------+-----------+-------------+
	| 2s         | 16        | 125000      |
	+------------+-----------+-------------+
	| 4s         | 32        | 125000      |
	+------------+-----------+-------------+
	| 8s         | 64        | 125000      |
	+------------+-----------+-------------+
	| 10s        | 64        | 156250      |
	+------------+-----------+-------------+ 
	*/
	int ret = 0;
	struct fimc_is_cis *cis;
	struct fimc_is_long_term_expo_mode *lte_mode;
	unsigned char cit_lshift_val = 0;
	int cit_lshift_count = 0;
	u32 target_exp = 0;
	int hold = 0;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	lte_mode = &cis->long_term_mode;

	hold = sensor_gw1_cis_group_param_hold(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* LTE mode or normal mode set */
	if (lte_mode->sen_strm_off_on_enable == true) {
		target_exp = lte_mode->expo[0];
		if (target_exp >= 125000 ) {
			cit_lshift_val = (unsigned char)(target_exp / 125000);
			while(cit_lshift_val > 1)
			{
				cit_lshift_val /= 2;
				target_exp /= 2;
				cit_lshift_count++;
			}

			lte_mode->expo[0] = target_exp;

			if (cit_lshift_count > SENSOR_GW1_MAX_CIT_LSHIFT_VALUE)
				cit_lshift_count = SENSOR_GW1_MAX_CIT_LSHIFT_VALUE;
		}
	}

	ret = fimc_is_sensor_write8(cis->client, SENSOR_GW1_CIT_LSHIFT_ADDR, cit_lshift_count);
	ret += fimc_is_sensor_write8(cis->client, SENSOR_GW1_FRM_LENGTH_LINE_LSHIFT_ADDR, cit_lshift_count);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_gw1_cis_group_param_hold(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	info("[%s] sen_strm_enable(%d), cit_lshift_count (%d), target_exp(%d)", __func__,
		lte_mode->sen_strm_off_on_enable, cit_lshift_count, lte_mode->expo[0]);

	if (ret < 0)
		pr_err("ERR[%s]: LTE register setting fail\n", __func__);

	return ret;
}

int sensor_gw1_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
{
#define ABS_WB_GAIN_NUM	3
	int ret = 0;
	int hold = 0;
	int mode = 0;
	int index = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	u16 abs_gains[ABS_WB_GAIN_NUM] = {0, };	/* [0]=R, [1]=G, [2]=B */
	u32 avg_g = 0, div = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	if (!cis->use_wb_gain)
		return ret;

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	mode = cis->cis_data->sens_config_index_cur;

	if (!IS_REMOSAIC_MODE(cis))
		return 0;

	if (wb_gains.gr != wb_gains.gb) {
		err("gr, gb not euqal"); /* check DDK layer */
		return -EINVAL;
	}

	if (wb_gains.gr == 1024)
		div = 4;
	else if (wb_gains.gr == 2048)
		div = 8;
	else {
		err("invalid gr,gb %d", wb_gains.gr); /* check DDK layer */
		return -EINVAL;
	}

	dbg_sensor(1, "[SEN:%d]%s:DDK vlaue: wb_gain_gr(%d), wb_gain_r(%d), wb_gain_b(%d), wb_gain_gb(%d)\n",
		cis->id, __func__, wb_gains.gr, wb_gains.r, wb_gains.b, wb_gains.gb);

	avg_g = (wb_gains.gr + wb_gains.gb) / 2;
	abs_gains[0] = (u16)((wb_gains.r / div) & 0xFFFF);
	abs_gains[1] = (u16)((avg_g / div) & 0xFFFF);
	abs_gains[2] = (u16)((wb_gains.b / div) & 0xFFFF);

	dbg_sensor(1, "[SEN:%d]%s: abs_gain_r(0x%4X), abs_gain_g(0x%4X), abs_gain_b(0x%4X)\n",
		cis->id, __func__, abs_gains[0], abs_gains[1], abs_gains[2]);

	hold = sensor_gw1_cis_group_param_hold(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	for (index=0; index < ABS_WB_GAIN_NUM; index++) {
		ret = fimc_is_sensor_write16(client, (SENSOR_GW1_WBGAIN_RED + (index * 2)), abs_gains[index]);
		if (ret < 0) {
			err("i2c fail addr(%x), val(%04x), ret = %d\n",
				SENSOR_GW1_WBGAIN_RED + (index * 2), abs_gains[index], ret);
			goto i2c_err;
		}
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_gw1_cis_group_param_hold(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_gw1_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u32 wait_cnt = 0, time_out_cnt = 250;
	u8 sensor_fcount = 0;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	if (unlikely(!cis_data)) {
		err("cis_data is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = fimc_is_sensor_read8(client, 0x0005, &sensor_fcount);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0005, sensor_fcount, ret);

	/*
	 * Read sensor frame counter (sensor_fcount address = 0x0005)
	 * stream on (0x00 ~ 0xFE), stream off (0xFF)
	 */
	while (sensor_fcount != 0xFF) {
		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret = fimc_is_sensor_read8(client, 0x0005, &sensor_fcount);
		I2C_MUTEX_UNLOCK(cis->i2c_lock);
		if (ret < 0)
			err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0005, sensor_fcount, ret);

		usleep_range(CIS_STREAM_OFF_WAIT_TIME, CIS_STREAM_OFF_WAIT_TIME);
		wait_cnt++;

		if (wait_cnt >= time_out_cnt) {
			err("[MOD:D:%d] %s, time out, wait_limit(%d) > time_out(%d), sensor_fcount(%d)",
					cis->id, __func__, wait_cnt, time_out_cnt, sensor_fcount);
			ret = -EINVAL;
			goto p_err;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
	}

	msleep(10);

#ifdef CONFIG_SENSOR_RETENTION_USE
	/* retention mode CRC wait calculation */
	usleep_range(5000, 5000);
#endif
p_err:
	info("%s: Wait Stream Off Done", __func__);
	return ret;
}

static struct fimc_is_cis_ops cis_ops_gw1 = {
	.cis_init = sensor_gw1_cis_init,
	.cis_log_status = sensor_gw1_cis_log_status,
	.cis_group_param_hold = sensor_gw1_cis_group_param_hold,
	.cis_set_global_setting = sensor_gw1_cis_set_global_setting,
	.cis_mode_change = sensor_gw1_cis_mode_change,
	.cis_set_size = sensor_gw1_cis_set_size,
	.cis_stream_on = sensor_gw1_cis_stream_on,
	.cis_stream_off = sensor_gw1_cis_stream_off,
	.cis_adjust_frame_duration = sensor_gw1_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_gw1_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_gw1_cis_set_frame_rate,
	.cis_get_min_exposure_time = sensor_gw1_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_gw1_cis_get_max_exposure_time,
	.cis_set_exposure_time = sensor_gw1_cis_set_exposure_time,
	.cis_get_min_analog_gain = sensor_gw1_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_gw1_cis_get_max_analog_gain,
	.cis_adjust_analog_gain = sensor_gw1_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_gw1_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_gw1_cis_get_analog_gain,
	.cis_get_min_digital_gain = sensor_gw1_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_gw1_cis_get_max_digital_gain,
	.cis_set_digital_gain = sensor_gw1_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_gw1_cis_get_digital_gain,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_set_long_term_exposure = sensor_gw1_cis_long_term_exposure,
	.cis_wait_streamoff = sensor_gw1_cis_wait_streamoff,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_data_calculation = sensor_gw1_cis_data_calc,
	.cis_set_adjust_sync = sensor_gw1_cis_set_adjust_sync,
	.cis_check_rev = sensor_gw1_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_set_wb_gains = sensor_gw1_cis_set_wb_gain,
};

int cis_gw1_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	bool use_pdaf = false;
	struct fimc_is_core *core = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct fimc_is_cis *cis = NULL;
	struct fimc_is_device_sensor *device = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	u32 sensor_id = 0;
	char const *setfile;
	struct device *dev;
	struct device_node *dnode;

	FIMC_BUG(!client);
	FIMC_BUG(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &client->dev;
	dnode = dev->of_node;

	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		err("sensor id read is fail(%d)", ret);
		goto p_err;
	}

	probe_info("%s sensor id %d\n", __func__, sensor_id);

	device = &core->sensor[sensor_id];

	sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_S5KGW1);
	if (!sensor_peri) {
		probe_info("sensor peri is not yet probed");
		return -EPROBE_DEFER;
	}

	cis = &sensor_peri->cis;
	if (!cis) {
		err("cis is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_cis = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_cis) {
		probe_err("subdev_cis is NULL");
		ret = -ENOMEM;
		goto p_err;
	}
	sensor_peri->subdev_cis = subdev_cis;

	cis->id = SENSOR_NAME_S5KGW1;
	cis->subdev = subdev_cis;
	cis->device = sensor_id;
	cis->client = client;
	sensor_peri->module->client = cis->client;
	cis->i2c_lock = NULL;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;

	cis->cis_data = kzalloc(sizeof(cis_shared_data), GFP_KERNEL);
	if (!cis->cis_data) {
		err("cis_data is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	cis->cis_ops = &cis_ops_gw1;

	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;

	if (of_property_read_bool(dnode, "use_pdaf")) {
		use_pdaf = true;
	}
	probe_info("%s use_pdaf %d\n", __func__, use_pdaf);

	if (of_property_read_bool(dnode, "sensor_f_number")) {
		ret = of_property_read_u32(dnode, "sensor_f_number", &cis->aperture_num);
		if (ret) {
			warn("f-number read is fail(%d)", ret);
		}
	} else {
		cis->aperture_num = F2_2;
	}
	probe_info("%s f-number %d\n", __func__, cis->aperture_num);

	cis->use_dgain = true;
	cis->hdr_ctrl_by_again = false;
	cis->use_wb_gain = true;
	cis->use_pdaf = use_pdaf;

	v4l2_set_subdevdata(subdev_cis, cis);
	v4l2_set_subdev_hostdata(subdev_cis, device);
	snprintf(subdev_cis->name, V4L2_SUBDEV_NAME_SIZE, "cis-subdev.%d", cis->id);

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A\n", __func__);
		sensor_gw1_global = sensor_gw1_setfile_A_global;
		sensor_gw1_global_size = ARRAY_SIZE(sensor_gw1_setfile_A_global);
		sensor_gw1_setfiles = sensor_gw1_setfiles_A;
		sensor_gw1_setfile_sizes = sensor_gw1_setfile_A_sizes;
		sensor_gw1_pllinfos = sensor_gw1_pllinfos_A;
		sensor_gw1_max_setfile_num = ARRAY_SIZE(sensor_gw1_setfiles_A);
	} else if (strcmp(setfile, "setB") == 0) {
		probe_info("%s setfile_B\n", __func__);
		sensor_gw1_global = sensor_gw1_setfile_B_global;
		sensor_gw1_global_size = ARRAY_SIZE(sensor_gw1_setfile_B_global);
		sensor_gw1_setfiles = sensor_gw1_setfiles_B;
		sensor_gw1_setfile_sizes = sensor_gw1_setfile_B_sizes;
		sensor_gw1_pllinfos = sensor_gw1_pllinfos_B;
		sensor_gw1_max_setfile_num = ARRAY_SIZE(sensor_gw1_setfiles_B);
	} else {
		err("%s setfile index out of bound, take default (setfile_B)", __func__);
		sensor_gw1_global = sensor_gw1_setfile_A_global;
		sensor_gw1_global_size = ARRAY_SIZE(sensor_gw1_setfile_A_global);
		sensor_gw1_setfiles = sensor_gw1_setfiles_A;
		sensor_gw1_setfile_sizes = sensor_gw1_setfile_A_sizes;
		sensor_gw1_pllinfos = sensor_gw1_pllinfos_A;
		sensor_gw1_max_setfile_num = ARRAY_SIZE(sensor_gw1_setfiles_A);
	}

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id sensor_cis_gw1_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-cis-gw1",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_gw1_match);

static const struct i2c_device_id sensor_cis_gw1_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_gw1_driver = {
	.probe	= cis_gw1_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_gw1_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_gw1_idt
};

static int __init sensor_cis_gw1_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_gw1_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_gw1_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_gw1_init);

