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

#ifndef FIMC_IS_COMPANION_CHIP_H
#define FIMC_IS_COMPANION_CHIP_H

#include "fimc-is-device-sensor-peri.h"

int sensor_companion_set_registers(struct v4l2_subdev *subdev, const u32 *regs, const u32 size);
int sensor_companion_dump_registers(struct v4l2_subdev *subdev, const u32 *regs, const u32 size);
struct fimc_is_device_sensor_peri *find_peri_by_companion_id(struct fimc_is_device_sensor *device,
							u32 companion);

#endif
