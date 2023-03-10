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
#include "fimc-is-companion-gc607.h"
#include "fimc-is-device-companion.h"

#include "fimc-is-companion-gc607-setA.h"
#include "fimc-is-companion-gc607-setB.h"
#include "fimc-is-companion.h"

#include "fimc-is-helper-i2c.h"
#include "fimc-is-vender-specific.h"

#define SENSOR_NAME "GC607_COMPANION"

static const struct v4l2_subdev_ops subdev_ops;

static const u32 *sensor_gc607_companion_global;
static u32 sensor_gc607_companion_global_size;
static const u32 **sensor_gc607_companion_setfiles;
static const u32 *sensor_gc607_companion_setfile_sizes;
//static const struct sensor_pll_info_compact **sensor_gc607_companion_pllinfos;
static u32 sensor_gc607_companion_max_setfile_num;

/* COMPANION OPS */
int sensor_gc607_companion_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_companion *companion;

	FIMC_BUG(!subdev);

	companion = (struct fimc_is_companion *)v4l2_get_subdevdata(subdev);
	if (!companion) {
		err("companion is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	FIMC_BUG(!companion);

	info("[%s] start\n", __func__);

p_err:
	return ret;
}

int sensor_gc607_companion_deinit(struct v4l2_subdev *subdev)
{
	int ret = 0;

	FIMC_BUG(!subdev);

	info("[%s] companion deinit\n", __func__);

	return ret;
}


int sensor_gc607_companion_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_companion *companion;
	struct i2c_client *client = NULL;

	FIMC_BUG(!subdev);

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
	sensor_companion_dump_registers(subdev, sensor_gc607_companion_setfiles[0], sensor_gc607_companion_setfile_sizes[0]);

	pr_err("[SEN:DUMP] *******************************\n");

p_err:
	return ret;
}

int sensor_gc607_companion_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_companion *companion = NULL;

	FIMC_BUG(!subdev);

	companion = (struct fimc_is_companion *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!companion);

	info("[%s] global setting start\n", __func__);

	/* setfile global setting is at camera entrance */
	ret = sensor_companion_set_registers(subdev, sensor_gc607_companion_global, sensor_gc607_companion_global_size);
	if (ret < 0) {
		err("sensor_gc607_companion_set_registers fail!!");
		goto p_err;
	}

	dbg_sensor(2, "[%s] global setting done\n", __func__);

p_err:
	return ret;
}

int sensor_gc607_companion_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct fimc_is_companion *companion = NULL;
	struct i2c_client *client;

	FIMC_BUG(!subdev);

	companion = (struct fimc_is_companion *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!companion);

	client = companion->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	if (mode > sensor_gc607_companion_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	dbg_sensor(2, "[%s] mode changed start(%d)\n", __func__, mode);

	ret = sensor_companion_set_registers(subdev, sensor_gc607_companion_setfiles[mode], sensor_gc607_companion_setfile_sizes[mode]);
	if (ret < 0) {
		err("sensor_gc607_companion_set_registers fail!!");
		goto p_err;
	}

	I2C_MUTEX_LOCK(companion->i2c_lock);
	ret = fimc_is_sensor_write8(client, COMPANION_EMBEDDED_EN_REG, 0x00);
	I2C_MUTEX_UNLOCK(companion->i2c_lock);
	if (ret < 0) {
		err("gc607 write to register fail!!");
		goto p_err;
	}

	dbg_sensor(2, "[%s] mode changed end(%d)\n", __func__, mode);

p_err:
	return ret;
}

static struct fimc_is_companion_ops companion_ops_gc607 = {
	.companion_init = sensor_gc607_companion_init,
	.companion_deinit = sensor_gc607_companion_deinit,
	.companion_log_status = sensor_gc607_companion_log_status,
	.companion_set_global_setting = sensor_gc607_companion_set_global_setting,
	.companion_mode_change = sensor_gc607_companion_mode_change,
};

int gc607_companion_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct fimc_is_core *core = NULL;
	struct v4l2_subdev *subdev_companion = NULL;
	struct fimc_is_companion *companion = NULL;
	struct fimc_is_device_companion *companion_device = NULL;
	struct fimc_is_device_sensor *device = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	struct device *dev;
	struct device_node *dnode;
	u32 sensor_id = 0;
	char const *setfile;

	FIMC_BUG(!client);
	FIMC_BUG(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		return -ENOMEM;
	}

	subdev_companion = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);

	if (!subdev_companion) {
		err("subdev_companion is NULL");
		return -EPROBE_DEFER;
	}

	companion_device = kzalloc(sizeof(struct fimc_is_device_companion), GFP_KERNEL);
	if (!companion_device) {
		err("fimc_is_device_companion is NULL");
		return -ENOMEM;
	}
	companion_device->companion_hsi2c_status = false;
	companion_device->companion_ops = &companion_ops_gc607;

	dev = &client->dev;
	dnode = dev->of_node;

	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	probe_info("%s companion sensor_id %d\n", __func__, sensor_id);

	device = &core->sensor[sensor_id];

	sensor_peri = find_peri_by_companion_id(device, COMPANION_NAME_GC607);

	if (!sensor_peri) {
		probe_info("sensor peri is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	companion = kzalloc(sizeof(struct fimc_is_companion), GFP_KERNEL);
	if (!companion) {
		probe_err("companion is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	companion->id = COMPANION_NAME_GC607;
	companion->subdev = subdev_companion;
	companion->device = 0;
	companion->client = client;

	companion->companion_ops = &companion_ops_gc607;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A\n", __func__);
		sensor_gc607_companion_global = sensor_gc607_companion_setfile_A_Global;
		sensor_gc607_companion_global_size = ARRAY_SIZE(sensor_gc607_companion_setfile_A_Global);
		sensor_gc607_companion_setfiles = sensor_gc607_companion_setfiles_A;
		sensor_gc607_companion_setfile_sizes = sensor_gc607_companion_setfile_A_sizes;
		sensor_gc607_companion_max_setfile_num = ARRAY_SIZE(sensor_gc607_companion_setfiles_A);
	} else if (strcmp(setfile, "setB") == 0) {
		probe_info("%s setfile_B\n", __func__);
		sensor_gc607_companion_global = sensor_gc607_companion_setfile_B_Global;
		sensor_gc607_companion_global_size = ARRAY_SIZE(sensor_gc607_companion_setfile_B_Global);
		sensor_gc607_companion_setfiles = sensor_gc607_companion_setfiles_B;
		sensor_gc607_companion_setfile_sizes = sensor_gc607_companion_setfile_B_sizes;
		sensor_gc607_companion_max_setfile_num = ARRAY_SIZE(sensor_gc607_companion_setfiles_B);
	} else {
		err("%s setfile index out of bound, take default (setfile_A)", __func__);
		sensor_gc607_companion_global = sensor_gc607_companion_setfile_A_Global;
		sensor_gc607_companion_global_size = ARRAY_SIZE(sensor_gc607_companion_setfile_A_Global);
		sensor_gc607_companion_setfiles = sensor_gc607_companion_setfiles_A;
		sensor_gc607_companion_setfile_sizes = sensor_gc607_companion_setfile_A_sizes;
		sensor_gc607_companion_max_setfile_num = ARRAY_SIZE(sensor_gc607_companion_setfiles_A);
	}

	sensor_peri->companion = companion;
	sensor_peri->subdev_companion = subdev_companion;

	device->subdev_companion = subdev_companion;
	device->companion = companion;

	v4l2_i2c_subdev_init(subdev_companion, client, &subdev_ops);
	v4l2_set_subdevdata(subdev_companion, companion);
	v4l2_set_subdev_hostdata(subdev_companion, companion_device);
	i2c_set_clientdata(client, companion_device);
	snprintf(subdev_companion->name, V4L2_SUBDEV_NAME_SIZE, "companion->subdev.%d", companion->id);

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id gc607_companion_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-companion-gc607",
	},
	{},
};
MODULE_DEVICE_TABLE(of, gc607_companion_match);

static const struct i2c_device_id gc607_companion_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver gc607_companion_driver = {
	.probe	= gc607_companion_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = gc607_companion_match,
		.suppress_bind_attrs = true,
	},
	.id_table = gc607_companion_idt,
};

static int __init gc607_companion_init(void)
{
	int ret;

	ret = i2c_add_driver(&gc607_companion_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			gc607_companion_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(gc607_companion_init);
