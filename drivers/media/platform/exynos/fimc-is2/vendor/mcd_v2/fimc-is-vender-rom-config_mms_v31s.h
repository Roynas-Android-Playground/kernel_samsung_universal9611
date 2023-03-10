#ifndef FIMC_IS_VENDER_ROM_CONFIG_MMS_V31S_H
#define FIMC_IS_VENDER_ROM_CONFIG_MMS_V31S_H

#include "fimc-is-eeprom-uw-3l6_v001.h"
#include "fimc-is-eeprom-rear-imx682_v001.h"
#include "fimc-is-eeprom-rear4-gc5035_0m31_v001.h"
#include "fimc-is-eeprom-front-imx616_v001.h"
#include "fimc-is-otprom-rear2-gc5035_v001.h"

const struct fimc_is_vender_rom_addr *vender_rom_addr[SENSOR_POSITION_MAX] = {
	&rear_imx682_cal_addr,          //[0] SENSOR_POSITION_REAR
	&front_imx616_cal_addr,         //[1] SENSOR_POSITION_FRONT
	&rear2_gc5035_otp_cal_addr,     //[2] SENSOR_POSITION_REAR2
	NULL,                           //[3] SENSOR_POSITION_FRONT2
	&uw_3l6_cal_addr,               //[4] SENSOR_POSITION_REAR3
	NULL,                           //[5] SENSOR_POSITION_FRONT3
	&rear4_gc5035_cal_addr,         //[6] SENSOR_POSITION_REAR4
	NULL,                           //[7] SENSOR_POSITION_FRONT4
	NULL,                           //[8] SP_REAR_TOF
	NULL                            //[9] SP_FRONT_TOF
};

#endif /*FIMC_IS_VENDER_ROM_CONFIG_MMS_V31S_H*/
