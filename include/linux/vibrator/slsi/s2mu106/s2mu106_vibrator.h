/*
 * s2mu106_haptic.h
 * Samsung S2MU106 Haptic Header
 *
 * Copyright (C) 2018 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __S2MU106_HAPTIC_H
#define __S2MU106_HAPTIC_H __FILE__

#define S2MU106_REG_HAPTIC_INT		0x00
#define S2MU106_REG_HBST_INT		0x01
#define S2MU106_REG_HAPTIC_INT_MASK	0x02
#define S2MU106_REG_HBST_INT_MASK	0x03
#define S2MU106_REG_HBST_STATUS1	0x04
#define S2MU106_REG_PERI_TAR1		0x05
#define S2MU106_REG_PERI_TAR2		0x06
#define S2MU106_REG_DUTY_TAR1		0x07
#define S2MU106_REG_DUTY_TAR2		0x08
#define S2MU106_REG_HAPTIC_MODE		0x09
#define S2MU106_REG_OV_BK_OPTION	0x0A
#define S2MU106_REG_OV_WAVE_NUM		0x0B
#define S2MU106_REG_OV_AMP		0x0C
#define S2MU106_REG_PWM_CNT_NUM		0x10
#define S2MU106_REG_FILTERCOEF1		0x13
#define S2MU106_REG_FILTERCOEF2		0x14
#define S2MU106_REG_FILTERCOEF3		0x15
#define S2MU106_REG_IMPCONF1		0x16
#define S2MU106_REG_IMPCONF2		0x17
#define S2MU106_REG_IMPCONF3		0x18
#define S2MU106_REG_AMPCOEF1		0x19
#define S2MU106_REG_AMPCOEF2		0x1A
#define S2MU106_REG_AMPCOEF3		0x1B
#define S2MU106_REG_HT_OTP0		0x20
#define S2MU106_REG_HT_OTP2		0x22
#define S2MU106_REG_HT_OTP3		0x23
#define S2MU106_REG_HBST_CTRL0		0x2B
#define S2MU106_REG_HBST_CTRL1		0x2C

/* S2MU106_REG_HBST_CTRL1 */
#define HAPTIC_BOOST_VOLTAGE_MASK	0x3F

/* S2MU106_REG_HT_OTP0 */
#define HBST_OK_FORCE_EN		0x01
#define HBST_OK_MASK_EN			0x02

/* S2MU106_REG_HT_OTP2 */
#define VCEN_SEL_MASK			0xC0

/* S2MU106_REG_HT_OTP3 */
#define VCENUP_TRIM_MASK		0x03

/* S2MU106_REG_HBST_CTRL0 */
#define EN_HBST_EXT_MASK		0x01
#define SEL_HBST_HAPTIC_MASK		0x02

/* S2MU106_REG_OV_BK_OPTION */
#define LRA_MODE_SET_MASK		0x80
#define LRA_BST_MODE_SET_MASK		0x10

/* S2MU106_REG_HAPTIC_MODE */
#define LRA_MODE_EN			0x20
#define ERM_HDPWM_MODE_EN		0x41
#define ERM_MODE_ON			0x01
#define HAPTIC_MODE_OFF			0x00

#define TEST_MODE_TIME			10000
#define MAX_INTENSITY			10000
#define MAX_FREQUENCY			5
#define VIB_BUFSIZE                     30
#define PACKET_MAX_SIZE         	1000
#define MAX_STR_LEN_EVENT_CMD		32

#define HAPTIC_ENGINE_FREQ_MIN          1200
#define HAPTIC_ENGINE_FREQ_MAX          3500

#define MULTI_FREQ_DIVIDER              78125000 /* 1s/128(pdiv)=7812500 */

enum s2mu106_haptic_operation_type {
	S2MU106_HAPTIC_ERM_I2C,
	S2MU106_HAPTIC_ERM_GPIO,
	S2MU106_HAPTIC_LRA,
};

enum s2mu106_haptic_pulse_mode {
	S2MU106_EXTERNAL_MODE,
	S2MU106_INTERNAL_MODE,
};

struct s2mu106_haptic_boost {
	/* haptic boost */
	bool en;
	bool automode;
	int level;
};

struct s2mu106_haptic_platform_data {
 	int motor_en;
	struct pwm_device *pwm;
	const char *motor_type;
	int hap_mode;

	int frequency;
	int overdrive_ratio;
	int normal_ratio;
	/* for multi-frequency */
	int freq_nums;
	u32 *freq_array;

	/* haptic boost */
	struct s2mu106_haptic_boost hbst;
};

#endif /* __S2MU106_HAPTIC_H */
