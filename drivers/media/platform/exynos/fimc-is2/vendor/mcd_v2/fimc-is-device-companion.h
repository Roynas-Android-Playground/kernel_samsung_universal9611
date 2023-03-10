/*
 * Samsung Exynos5 SoC series FIMC-IS OIS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_DEVICE_COMPANION_H
#define FIMC_IS_DEVICE_COMPANION_H

struct fimc_is_device_companion {
	struct v4l2_device				v4l2_dev;
	struct platform_device				*pdev;
	unsigned long					state;
	struct exynos_platform_fimc_is_sensor		*pdata;
	struct i2c_client				*client;
	struct fimc_is_companion_ops	*companion_ops;
	int companion_en;
	bool companion_hsi2c_status;
	bool not_crc_bin;
};

#endif
