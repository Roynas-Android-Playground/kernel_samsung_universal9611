/*
 *  Copyright (C) 2019, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include <linux/slab.h>
#include "../../ssp.h"
#include "../ssp_factory.h"
#include "../../ssp_comm.h"
#include "../../ssp_cmd_define.h"


/*************************************************************************/
/* factory Test                                                          */
/*************************************************************************/
ssize_t get_light_stk3x6x_name(char *buf)
{
#ifdef CONFIG_SENSORS_SSP_PROXIMITY_STK3X6X	
	return sprintf(buf, "%s\n", "STK33617");
#else
	return sprintf(buf, "%s\n", "STK31610");
#endif
}

ssize_t get_light_stk3x6x_vendor(char *buf)
{
	return sprintf(buf, "%s\n", "Sitronix");
}

ssize_t get_light_stk3x6x_lux(struct ssp_data *data, char *buf)
{
	return sprintf(buf, "%u,%u,%u,%u,%u,%u\n",
	               data->buf[SENSOR_TYPE_LIGHT].r, data->buf[SENSOR_TYPE_LIGHT].g,
	               data->buf[SENSOR_TYPE_LIGHT].b, data->buf[SENSOR_TYPE_LIGHT].w,
	               data->buf[SENSOR_TYPE_LIGHT].a_time, data->buf[SENSOR_TYPE_LIGHT].a_gain);
}

ssize_t get_ams_light_stk3x6x_data(struct ssp_data *data, char *buf)
{
	return sprintf(buf, "%u,%u,%u,%u,%u,%u\n",
	               data->buf[SENSOR_TYPE_LIGHT].r, data->buf[SENSOR_TYPE_LIGHT].g,
	               data->buf[SENSOR_TYPE_LIGHT].b, data->buf[SENSOR_TYPE_LIGHT].w,
	               data->buf[SENSOR_TYPE_LIGHT].a_time, data->buf[SENSOR_TYPE_LIGHT].a_gain);
}

ssize_t get_light_stk3x6x_coefficient(struct ssp_data *data, char *buf)
{
	int ret = 0;
	char *coef_buf = NULL;
	int coef_buf_length = 0;
	int temp_coef[7] = {0, };

	ret = ssp_send_command(data, CMD_GETVALUE, SENSOR_TYPE_LIGHT, LIGHT_COEF,
	                       1000, NULL, 0, &coef_buf, &coef_buf_length);

	if (ret != SUCCESS) {
		ssp_errf("ssp_send_command Fail %d", ret);
		if (coef_buf != NULL) {
			kfree(coef_buf);
		}
		return FAIL;
	}

	if (coef_buf == NULL) {
		ssp_errf("buffer is null");
		return -EINVAL;
	}

	if (coef_buf_length != 28) {
		ssp_errf("buffer length error %d", coef_buf_length);
		if (coef_buf != NULL) {
			kfree(coef_buf);
		}
		return -EINVAL;
	}

	memcpy(temp_coef, coef_buf, sizeof(temp_coef));

	pr_info("[SSP] %s - %d %d %d %d %d %d %d\n", __func__,
	        temp_coef[0], temp_coef[1], temp_coef[2], temp_coef[3], temp_coef[4], temp_coef[5],
	        temp_coef[6]);

	ret = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d\n",
	               temp_coef[0], temp_coef[1], temp_coef[2], temp_coef[3], temp_coef[4], temp_coef[5],
	               temp_coef[6]);

	if (coef_buf != NULL) {
		kfree(coef_buf);
	}

	return ret;
}

ssize_t get_light_stk3x6x_circle(struct ssp_data *data, char *buf)
{
	return sprintf(buf, "%u.%u %u.%u %u.%u\n",
	              data->light_position[0], data->light_position[1],
	              data->light_position[2], data->light_position[3],
	              data->light_position[4], data->light_position[5]);
}

struct  light_sensor_operations light_stk3x6x_ops = {
	.get_light_name = get_light_stk3x6x_name,
	.get_light_vendor = get_light_stk3x6x_vendor,
	.get_lux = get_light_stk3x6x_lux,
	.get_light_data = get_ams_light_stk3x6x_data,
	.get_light_coefficient = get_light_stk3x6x_coefficient,
	.get_light_circle = get_light_stk3x6x_circle,
};

struct light_sensor_operations* get_light_stk3x6x_function_pointer(struct ssp_data *data)
{
	return &light_stk3x6x_ops;
}

