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
ssize_t get_light_cm32183_name(char *buf)
{
	return sprintf(buf, "%s\n", "CM32183");
}

ssize_t get_light_cm32183_vendor(char *buf)
{
	return sprintf(buf, "%s\n", "Capella");
}

ssize_t get_light_cm32183_lux(struct ssp_data *data, char *buf)
{
	return sprintf(buf, "%u,%u,%u,%u,%u,%u\n",
	               data->buf[SENSOR_TYPE_LIGHT].r, data->buf[SENSOR_TYPE_LIGHT].g,
	               data->buf[SENSOR_TYPE_LIGHT].b, data->buf[SENSOR_TYPE_LIGHT].w,
	               data->buf[SENSOR_TYPE_LIGHT].a_time, data->buf[SENSOR_TYPE_LIGHT].a_gain);
}

ssize_t get_ams_light_cm32183_data(struct ssp_data *data, char *buf)
{
	return sprintf(buf, "%u,%u,%u,%u,%u,%u\n",
	               data->buf[SENSOR_TYPE_LIGHT].r, data->buf[SENSOR_TYPE_LIGHT].g,
	               data->buf[SENSOR_TYPE_LIGHT].b, data->buf[SENSOR_TYPE_LIGHT].w,
	               data->buf[SENSOR_TYPE_LIGHT].a_time, data->buf[SENSOR_TYPE_LIGHT].a_gain);
}

struct  light_sensor_operations light_cm32183_ops = {
	.get_light_name = get_light_cm32183_name,
	.get_light_vendor = get_light_cm32183_vendor,
	.get_lux = get_light_cm32183_lux,
	.get_light_data = get_ams_light_cm32183_data,
};

struct light_sensor_operations* get_light_cm32183_function_pointer(struct ssp_data *data)
{
	return &light_cm32183_ops;
}
