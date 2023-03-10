/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_DEVICE_HI1336_H
#define FIMC_IS_DEVICE_HI1336_H

#ifdef USE_HI1336_12M_FULL_SETFILE
#define HI1336_ACTIVE_WIDTH     4032
#define HI1336_ACTIVE_HEIGHT    3024
#elif  defined(USE_HI1336B_SETFILE)
#define HI1336_ACTIVE_WIDTH     4208
#define HI1336_ACTIVE_HEIGHT    3120
#else
#define HI1336_ACTIVE_WIDTH     4000
#define HI1336_ACTIVE_HEIGHT    3000
#endif

#endif
