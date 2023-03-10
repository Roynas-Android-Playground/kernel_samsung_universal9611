/*
 *  Copyright (C) 2017, Samsung Electronics Co. Ltd. All Rights Reserved.
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


#ifndef FINGERPRINT_H_
#define FINGERPRINT_H_

#undef DEBUG /* If you need pr_debug logs, changes this definition */
#define pr_fmt(fmt) "fps_%s: " fmt, __func__
#include <linux/clk.h>
#include "fingerprint_sysfs.h"

/* fingerprint debug timer */
#define FPSENSOR_DEBUG_TIMER_SEC (10 * HZ)

#if defined(CONFIG_FINGERPRINT_SECURE) && !defined(CONFIG_SEC_FACTORY)
#define ENABLE_SENSORS_FPRINT_SECURE
#endif

/* For Sensor Type Check */
enum {
	SENSOR_OOO = -2,
	SENSOR_UNKNOWN,
	SENSOR_FAILED,
	SENSOR_VIPER,
	SENSOR_RAPTOR,
	SENSOR_EGIS,
	SENSOR_VIPER_WOG,
	SENSOR_NAMSAN,
	SENSOR_GOODIX,
	SENSOR_QBT2000,
	SENSOR_EGISOPTICAL,
	SENSOR_GOODIXOPTICAL,
	SENSOR_MAXIMUM,
};

#define SENSOR_STATUS_SIZE 12
static char sensor_status[SENSOR_STATUS_SIZE][10] = {"ooo", "unknown", "failed",
	"viper", "raptor", "egis", "viper_wog", "namsan", "goodix", "qbt2000", "et7xx", "goodixopt"};
/* For Finger Detect Mode */

enum {
	DETECT_NORMAL = 0,
	DETECT_ADM,			/* Always on Detect Mode */
};

#endif
