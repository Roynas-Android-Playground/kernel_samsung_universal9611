/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is vender functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_VENDER_ROM_CONFIG_H
#define FIMC_IS_VENDER_ROM_CONFIG_H

#include "fimc-is-vender-specific.h"

#if defined(CONFIG_CAMERA_VXP_V00)
#include "fimc-is-vender-rom-config_vxp_v00.h"
#elif defined(CONFIG_CAMERA_AAS_V50)
#include "fimc-is-vender-rom-config_aas_v50.h"
#elif defined(CONFIG_CAMERA_AAS_V51)
#include "fimc-is-vender-rom-config_aas_v51.h"
#elif defined(CONFIG_CAMERA_AAS_V80)
#include "fimc-is-vender-rom-config_aas_v80.h"
#elif defined(CONFIG_CAMERA_AAS_V50S)
#include "fimc-is-vender-rom-config_aas_v50s.h"
#elif defined(CONFIG_CAMERA_FXT_V41)
#include "fimc-is-vender-rom-config_fxt_v41.h"
#elif defined(CONFIG_CAMERA_MMS_V30S)
#include "fimc-is-vender-rom-config_mms_v30s.h"
#elif defined(CONFIG_CAMERA_MMS_V21)
#include "fimc-is-vender-rom-config_mms_v21.h"
#elif defined(CONFIG_CAMERA_MMS_V31)
#include "fimc-is-vender-rom-config_mms_v31.h"
#elif defined(CONFIG_CAMERA_MMS_V31S)
#include "fimc-is-vender-rom-config_mms_v31s.h"
#elif defined(CONFIG_CAMERA_XXS_VPRO)
#include "fimc-is-vender-rom-config_xxs_vpro.h"
#elif defined(CONFIG_CAMERA_ATS_V04)
#include "fimc-is-vender-rom-config_ats_v04.h"
#else

const struct fimc_is_vender_rom_addr *vender_rom_addr[SENSOR_POSITION_MAX] = {
	NULL,		//[0] SENSOR_POSITION_REAR
	NULL,		//[1] SENSOR_POSITION_FRONT
	NULL,		//[2] SENSOR_POSITION_REAR2
	NULL,		//[3] SENSOR_POSITION_FRONT2
	NULL,		//[4] SENSOR_POSITION_REAR3
	NULL,		//[5] SENSOR_POSITION_FRONT3
	NULL,		//[6] SENSOR_POSITION_REAR4
	NULL,		//[7] SENSOR_POSITION_FRONT4
	NULL,		//[8] SENSOR_POSITION_REAR_TOF
	NULL,		//[9] SENSOR_POSITION_FRONT_TOF
};

#endif
#endif
