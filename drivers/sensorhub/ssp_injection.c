/*
 *  Copyright (C) 2018, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "ssp_injection.h"
#include "ssp_data.h"
#include "ssp_iio.h"
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define INJECTION_MODE_SENSOR_DATA				0
#define INJECTION_MODE_ADDITIONAL_INFO	 		1

enum {
	BRIGHTNESS_LEVEL1 = 1,
	BRIGHTNESS_LEVEL2,
	BRIGHTNESS_LEVEL3,
	BRIGHTNESS_LEVEL4,
	BRIGHTNESS_LEVEL5
};

static int ssp_inject_sensor_data(struct ssp_data *data,
                                          const char *buf, int count)
{
	ssp_infof("");
	return 0;
}

static int ssp_inject_additional_info(struct ssp_data *data,
                                          const char *buf, int count)
{
	int ret = 0;
	char type;

	type = buf[0];

#ifdef CONFIG_SENSORS_SSP_LIGHT
	if(type == SENSOR_TYPE_LIGHT)
	{
		int cur_level = 0;
		int cal_brightness = 0;
		int32_t brightness;
		int i;
		if(count < 5) {
			ssp_errf("brightness length error %d", count);
			return -EINVAL;
		}
		brightness = *((int32_t*)(buf + 1));
		cal_brightness = brightness / 10;
		cal_brightness *= 10;

		//ssp_errf("br %d, cal_br %d", brightness, cal_brightness);

		// set current level for changing itime

		for(i=0 ; i < data->brightness_array_len; i++) {
			if(brightness <= data->brightness_array[i]) {
				cur_level = i+1;
				//ssp_infof(" brightness %d <= %d , level %d", brightness, data->brightness_array[i], cur_level);
				break;
			}
		}

		if(data->last_brightness_level != cur_level) {
			data->brightness = brightness;

			// update last level
			data->last_brightness_level = cur_level;

			set_light_brightness(data);
			data->brightness = cal_brightness;
		} else if(data->brightness != cal_brightness) {
			data->brightness = brightness;
			set_light_brightness(data);
			data->brightness = cal_brightness;
		}
	}
	else if(type == SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS)
	{
#ifdef CONFIG_SENSORS_SSP_CAMERALIGHT_FOR_TAB
	        int light_lux = data->report_ab_lux;
#endif
		if(count < 5) {
			ssp_errf("camera lux length error %d", count);
			return -EINVAL;
		}
		data->camera_lux = *((int32_t*)(buf + 1));
		ssp_infof("cam_lux %d", data->camera_lux);

#ifdef CONFIG_SENSORS_SSP_CAMERALIGHT_FOR_TAB
		if(data->camera_lux < 100) {
			int report_value = light_lux;

			if((data->camera_lux == 50 && light_lux <=5) ||(data->camera_lux == 30 && light_lux <=1))
				report_value = data->camera_lux;

			report_camera_lux_data(data, report_value);
			data->low_lux_mode = 2;
		} else {
			if(data->low_lux_mode == 2 && light_lux <= data->camera_lux_hysteresis[0]) {
				//update camera lux
				report_camera_lux_data(data, data->camera_lux);
				ssp_infof("Light AB Sensor : report cam lux %d, light lux(%d), mode %d",
				data->camera_lux, light_lux, data->low_lux_mode);
			}
			else {
				//do not report
				data->low_lux_mode = 1;
				ssp_infof("Light AB Sensor : cam lux is %d, so do not report light lux(%d), mode %d",
				data->camera_lux, light_lux, data->low_lux_mode);
			}
		}

#else
		if(data->camera_lux_en) {
			report_camera_lux_data(data, data->camera_lux);
		}
#endif
	}
#endif
#ifdef CONFIG_SENSORS_SSP_ACCELOMETER
	if (type == SENSOR_TYPE_DEVICE_ORIENTATION) {
		if (count < 2) {
			ssp_errf("orientation mode length error %d", count);
			return -EINVAL;
		}
		data->orientation_mode = buf[1];
		set_device_orientation_mode(data);
	}
	else if (type == SENSOR_TYPE_SAR_BACKOFF_MOTION) {
		int32_t sbm_value;
		if (count < 5) {
			ssp_errf("sbm reset value length error %d", count);
			return -EINVAL;
		}
		sbm_value = *((int32_t *)(buf + 1));

		set_sar_backoff_motion_reset_value(data, sbm_value);
	}
#endif

	return ret;
}


static ssize_t ssp_injection_write(struct file *file, const char __user *buf,
                                   size_t count, loff_t *pos)
{
	struct ssp_data *data = container_of(file->private_data, struct ssp_data, injection_device);
	int ret = 0;
	char *buffer;

	if (!is_sensorhub_working(data)) {
		ssp_errf("stop inject data(is not working)");
		return -EIO;
	}

	if (unlikely(count < 2)) {
		ssp_errf("inject data length err(%d)", (int)count);
		return -EINVAL;
	}

	buffer = kzalloc(count * sizeof(char), GFP_KERNEL);

	ret = copy_from_user(buffer, buf, count);
	if (unlikely(ret)) {
		ssp_errf("memcpy for kernel buffer err");
		ret = -EFAULT;
		goto exit;
	}

	if (buffer[0] == INJECTION_MODE_ADDITIONAL_INFO) {
		ret = ssp_inject_additional_info(data, &buffer[1], count-1);
	} else {
		ret = ssp_inject_sensor_data(data, &buffer[1], count-1);
	}

	if (unlikely(ret < 0)) {
		ssp_errf("inject data err(%d)", ret);
		if (ret == ERROR) {
			ret = -EIO;
		}

		else if (ret == FAIL) {
			ret = -EAGAIN;
		}

		goto exit;
	}

	ret = count;

exit:
	kfree(buffer);
	return ret;
}

static struct file_operations ssp_injection_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.write = ssp_injection_write,
};

int ssp_injection_initialize(struct ssp_data *data)
{
	int ret;

	/* register injection misc device */
	data->injection_device.minor = MISC_DYNAMIC_MINOR;
	data->injection_device.name = "ssp_data_injection";
	data->injection_device.fops = &ssp_injection_fops;

	ret = misc_register(&data->injection_device);
	if (ret < 0) {
		ssp_errf("register injection misc device err(%d)", ret);
	}

	return ret;
}

void ssp_injection_remove(struct ssp_data *data)
{
	ssp_injection_fops.write = NULL;
	misc_deregister(&data->injection_device);
}

MODULE_DESCRIPTION("Seamless Sensor Platform(SSP) injection driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");

