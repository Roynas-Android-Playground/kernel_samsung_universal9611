/*
 * Samsung Exynos5 SoC series Companion driver
 *
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/firmware.h>

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
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-companion.h"

int sensor_companion_set_registers(struct v4l2_subdev *subdev, const u32 *regs, const u32 size)
{
	int ret = 0;
	int i = 0;
	struct fimc_is_companion *companion;
	struct i2c_client *client;
	int index_str = 0, index_next = 0;
	int burst_num = 1;
	u16 *addr_str = NULL;

	FIMC_BUG(!subdev);
	FIMC_BUG(!regs);

	companion = (struct fimc_is_companion *)v4l2_get_subdevdata(subdev);

	if (!companion) {
		err("companion is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = companion->client;
	if (unlikely(!client)) {
		err("companion client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* Need to delay for sensor setting */
	usleep_range(3000, 3000);

	for (i = 0; i < size; i += I2C_NEXT) {
		switch (regs[i + I2C_ADDR]) {
		case I2C_MODE_BURST_ADDR:
			index_str = i;
			break;
		case I2C_MODE_BURST_DATA:
			index_next = i + I2C_NEXT;
			if ((index_next < size) && (I2C_MODE_BURST_DATA == regs[index_next + I2C_ADDR])) {
				burst_num++;
				break;
			}

			addr_str = (u16 *)&regs[index_str + I2C_NEXT + I2C_DATA];
			I2C_MUTEX_LOCK(companion->i2c_lock);
			ret = fimc_is_sensor_write16_burst(client, regs[index_str + I2C_DATA], addr_str, burst_num);
			I2C_MUTEX_UNLOCK(companion->i2c_lock);
			if (ret < 0) {
				err("fimc_is_sensor_write16_burst fail, ret(%d), addr(%#x), data(%#x)",
						ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
			}
			burst_num = 1;
			break;
		case I2C_MODE_DELAY:
			usleep_range(regs[i + I2C_DATA], regs[i + I2C_DATA]);
			break;
		default:
			if (regs[i + I2C_BYTE] == 0x1) {
				I2C_MUTEX_LOCK(companion->i2c_lock);
				ret = fimc_is_sensor_write8(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				I2C_MUTEX_UNLOCK(companion->i2c_lock);
				if (ret < 0) {
					err("fimc_is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)",
							ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				}
			} else if (regs[i + I2C_BYTE] == 0x2) {
				I2C_MUTEX_LOCK(companion->i2c_lock);
				ret = fimc_is_sensor_write16(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				I2C_MUTEX_UNLOCK(companion->i2c_lock);
				if (ret < 0) {
					err("fimc_is_sensor_write16 fail, ret(%d), addr(%#x), data(%#x)",
						ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				}
			}
		}
	}

	dbg_sensor(1, "[%s] sensor setting done\n", __func__);

p_err:
	return ret;
}

int sensor_companion_dump_registers(struct v4l2_subdev *subdev, const u32 *regs, const u32 size)
{
	int ret = 0;
	int i = 0;
	struct fimc_is_companion *companion;
	struct i2c_client *client;
	u8 data8 = 0;
	u16 data16 = 0;

	FIMC_BUG(!subdev);
	FIMC_BUG(!regs);

	companion = (struct fimc_is_companion *)v4l2_get_subdevdata(subdev);
	if (!companion) {
		err("companion is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = companion->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < size; i += I2C_NEXT) {
		if (regs[i + I2C_BYTE] == 0x2 && regs[i + I2C_ADDR] == 0x6028) {
			I2C_MUTEX_LOCK(companion->i2c_lock);
			ret = fimc_is_sensor_write16(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
			I2C_MUTEX_UNLOCK(companion->i2c_lock);
		}

		if (regs[i + I2C_BYTE] == 0x1) {
			I2C_MUTEX_LOCK(companion->i2c_lock);
			ret = fimc_is_sensor_read8(client, regs[i + I2C_ADDR], &data8);
			I2C_MUTEX_UNLOCK(companion->i2c_lock);
			if (ret < 0) {
				err("fimc_is_sensor_read8 fail, ret(%d), addr(%#x)",
						ret, regs[i + I2C_ADDR]);
			}
			pr_err("[SEN:DUMP] [0x%04X, 0x%04X\n", regs[i + I2C_ADDR], data8);
		} else {
			I2C_MUTEX_LOCK(companion->i2c_lock);
			ret = fimc_is_sensor_read16(client, regs[i + I2C_ADDR], &data16);
			I2C_MUTEX_UNLOCK(companion->i2c_lock);
			if (ret < 0) {
				err("fimc_is_sensor_read6 fail, ret(%d), addr(%#x)",
						ret, regs[i + I2C_ADDR]);
			}
			pr_err("[COMPANION:DUMP] [0x%04X, 0x%04X\n", regs[i + I2C_ADDR], data16);
		}
	}

p_err:
	return ret;
}

struct fimc_is_device_sensor_peri *find_peri_by_companion_id(struct fimc_is_device_sensor *device,
							u32 companion)
{
	u32 mindex = 0, mmax = 0;
	struct fimc_is_module_enum *module_enum = NULL;
	struct fimc_is_resourcemgr *resourcemgr = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	FIMC_BUG_NULL(!device);
	resourcemgr = device->resourcemgr;
	module_enum = device->module_enum;
	FIMC_BUG_NULL(!module_enum);

	FIMC_BUG_NULL(!resourcemgr);

	mmax = atomic_read(&device->module_count);
	for (mindex = 0; mindex < mmax; mindex++) {
		if (module_enum[mindex].ext.companion_con.product_name == companion) {
			sensor_peri = (struct fimc_is_device_sensor_peri *)module_enum[mindex].private_data;
			break;
		}
	}

	if (mindex >= mmax) {
		err("companion (%d) is not found", device, companion);
	}

	return sensor_peri;
}
