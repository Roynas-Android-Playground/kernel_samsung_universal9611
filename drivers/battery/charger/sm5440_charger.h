
/*
 * sm5440_charger.h - SM5440 Charger device driver for SAMSUNG platform
 *
 * Copyright (C) 2019 SiliconMitus Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mutex.h>

#ifndef __SM5440_CHARGER_H__
#define __SM5440_CHARGER_H__

#define PPS_V_STEP				20
#define PPS_C_STEP				50
#define MAX(a, b)				((a > b) ? (a):(b))
#define MIN(a, b)				((a < b) ? (a):(b))

#define SM5440_DC_VBAT_MIN	  3100
#define SM5440_VBUS_OVP_TH	  11000

enum sm5440_int1_desc {
	SM5440_INT1_VOUTOVP			= 1 << 4,
	SM5440_INT1_VBATOVP			= 1 << 3,
	SM5440_INT1_REVBPOCP		= 1 << 1,
	SM5440_INT1_REVBSTOCP		= 1 << 0,
};

enum sm5440_int2_desc {
	SM5440_INT2_IBUSLIM			= 1 << 7,
	SM5440_INT2_VOUTOVP_ALM		= 1 << 4,
	SM5440_INT2_VBATREG			= 1 << 3,
	SM5440_INT2_THEM_REG		= 1 << 1,
	SM5440_INT2_THEM			= 1 << 0,
};

enum sm5440_int3_desc {
	SM5440_INT3_VBUSOVP			= 1 << 7,
	SM5440_INT3_VBUSUVLO		= 1 << 6,
	SM5440_INT3_VBUSPOK			= 1 << 5,
	SM5440_INT3_THEMSHDN_ALM	= 1 << 4,
	SM5440_INT3_THEMSHDN		= 1 << 3,
	SM5440_INT3_STUP_FAIL		= 1 << 2,
	SM5440_INT3_REVBLK			= 1 << 1,
	SM5440_INT3_CFLY_SHORT		= 1 << 0,
};

enum sm5440_int4_desc {
	SM5440_INT4_RVSBSTRDY		= 1 << 6,
	SM5440_INT4_MIDVBUS2VOUT	= 1 << 5,
	SM5440_INT4_SW_RDY			= 1 << 4,
	SM5440_INT4_WDTMROFF		= 1 << 2,
	SM5440_INT4_CHGTMROFF		= 1 << 1,
	SM5440_INT4_ADCUPDATED		= 1 << 0,
};


enum sm5440_reg_addr {
	SM5440_REG_INT1			= 0x00,
	SM5440_REG_INT2			= 0x01,
	SM5440_REG_INT3		 	= 0x02,
	SM5440_REG_INT4			= 0X03,
	SM5440_REG_MSK1			= 0X04,
	SM5440_REG_MSK2			= 0X05,
	SM5440_REG_MSK3			= 0X06,
	SM5440_REG_MSK4			= 0X07,
	SM5440_REG_STATUS1		= 0X08,
	SM5440_REG_STATUS2		= 0X09,
	SM5440_REG_STATUS3		= 0X0A,
	SM5440_REG_STATUS4		= 0X0B,
	SM5440_REG_CNTL1		= 0X0C,
	SM5440_REG_CNTL2		= 0X0D,
	SM5440_REG_CNTL3		= 0X0E,
	SM5440_REG_CNTL4		= 0X0F,
	SM5440_REG_CNTL5		= 0X10,
	SM5440_REG_CNTL6		= 0X11,
	SM5440_REG_CNTL7		= 0X12,
	SM5440_REG_VBUSCNTL		= 0X13,
	SM5440_REG_VBATCNTL		= 0X14,
	SM5440_REG_VOUTCNTL		= 0X15,
	SM5440_REG_IBUSCNTL		= 0X16,
	SM5440_REG_PRTNCNTL		= 0X19,
	SM5440_REG_THEMCNTL1	= 0X1A,
	SM5440_REG_THEMCNTL2	= 0X1B,
	SM5440_REG_ADCCNTL1		= 0X1C,
	SM5440_REG_ADCCNTL2		= 0X1D,
	SM5440_REG_ADC_VBUS1	= 0X1E,
	SM5440_REG_ADC_VBUS2	= 0X1F,
	SM5440_REG_ADC_VOUT1	= 0X20,
	SM5440_REG_ADC_VOUT2	= 0X21,
	SM5440_REG_ADC_IBUS1	= 0X22,
	SM5440_REG_ADC_IBUS2	= 0X23,
	SM5440_REG_ADC_THEM1	= 0X24,
	SM5440_REG_ADC_THEM2	= 0X25,
	SM5440_REG_ADC_DIETEMP  = 0X26,
	SM5440_REG_ADC_VBAT1	= 0X27,
	SM5440_REG_ADC_VBAT2	= 0X28,
	SM5440_REG_DEVICEID		= 0X2B,
};

enum sm5440_adc_mode {
	SM5440_ADC_MODE_ONESHOT		= 0x0,
	SM5440_ADC_MODE_CONTINUOUS	= 0x1,
	SM5440_ADC_MODE_OFF			= 0x2,
};

enum sm5440_adc_channel {
	SM5440_ADC_THEM			= 0x0,
	SM5440_ADC_DIETEMP,
	SM5440_ADC_VBAT1,
	SM5440_ADC_VOUT,
	SM5440_ADC_IBUS,
	SM5440_ADC_VBUS,
};

enum sm5440_charging_loop {
	LOOP_IBUSLIM			= (0x1 << 7),
	LOOP_VBATREG			= (0x1 << 3),
	LOOP_INACTIVE			= (0x0),
};

enum sm5440_work_delay_type {
	DELAY_NONE				= 0,
	DELAY_PPS_UPDATE		= 250,
	DELAY_ADC_UPDATE		= 1100,
	DELAY_RETRY				= 2000,
	DELAY_CHG_LOOP			= 7500,
};

enum sm5440_dc_state {
	/* SEC_DIRECT_CHG_MODE_DIRECT_OFF */
	SM5440_DC_CHG_OFF	= 0x0,
	SM5440_DC_ERR,
	/* SEC_DIRECT_CHG_MODE_DIRECT_DONE */
	SM5440_DC_EOC,
	/* SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT */
	SM5440_DC_CHECK_VBAT,
	/* SEC_DIRECT_CHG_MODE_DIRECT_PRESET */
	SM5440_DC_PRESET,
	/* SEC_DIRECT_CHG_MODE_DIRECT_ON_ADJUST */
	SM5440_DC_PRE_CC,
	SM5440_DC_UPDAT_BAT,
	/* SEC_DIRECT_CHG_MODE_DIRECT_ON */
	SM5440_DC_CC,
	SM5440_DC_CV,
};

enum sm5440_err_index {
	SM5440_ERR_REVBSTOCP		= (0x1 << 0),
	SM5440_ERR_REVBPOCP			= (0x1 << 1),
	SM5440_ERR_VBATOVP			= (0x1 << 3),
	SM5440_ERR_VOUTOVP			= (0x1 << 4),
	SM5440_ERR_IBATOCP			= (0x1 << 6),
	SM5440_ERR_IBUSOCP			= (0x1 << 7),
	SM5440_ERR_CFLY_SHORT		= (0x1 << 8),
	SM5440_ERR_REVBLK			= (0x1 << 9),
	SM5440_ERR_STUP_FAIL		= (0x1 << 10),
	SM5440_ERR_VBUSOVP			= (0x1 << 15),
	SM5440_ERR_INVAL_VBAT		= (0x1 << 16),
	SM5440_ERR_SEND_PD_MSG		= (0x1 << 17),
	SM5440_ERR_FAIL_ADJUST		= (0x1 << 18),
	SM5440_ERR_UNKNOWN			= (0x1 << 31),
};

enum sm5440_wdt_tmr {
	WDT_TIMER_S_0P5		= 0x0,
	WDT_TIMER_S_1		= 0x1,
	WDT_TIMER_S_2		= 0x2,
	WDT_TIMER_S_4		= 0x3,
	WDT_TIMER_S_30		= 0x4,
	WDT_TIMER_S_60		= 0x5,
	WDT_TIMER_S_90		= 0x6,
	WDT_TIMER_S_120		= 0x7,
};

enum sm5440_ibus_ocp {
	IBUS_OCP_100		= 0x0,
	IBUS_OCP_200		= 0x1,
	IBUS_OCP_300		= 0x2,
	IBUS_OCP_400		= 0x3,
};

enum sm5440_op_mode {
	OP_MODE_CHG_OFF		= 0x0,
	OP_MODE_CHG_ON		= 0x1,
	OP_MODE_REV_BYPASS	= 0x2,
	OP_MODE_REV_BOOST	= 0x3,
};

struct sm5440_platform_data {
	int irq_gpio;
	u32 ta_min_current;
	u32 ta_min_voltage;
	u32 dc_min_vbat;
	u32 pps_lr;
	u32 rpara;
	u32 rsns;
	u32 rpcm;
	u32 cv_gl_offset;
	u32 ci_gl_offset;
	u32 cc_gl_offset;
	u32 freq;
	u8 rev_id;

	struct {
		u32 chg_float_voltage;
		char *sec_dc_name;
	}battery;
};

struct sm5440_charger {
	struct device *dev;
	struct i2c_client *i2c;
	struct sm5440_platform_data *pdata;
	struct power_supply	*psy_chg;

	struct mutex st_lock;
	struct mutex pd_lock;
	struct mutex i2c_lock;
	struct wakeup_source	wake_lock;

	/* for direct-charging state machine */
	struct workqueue_struct *dc_wqueue;

	struct delayed_work check_vbat_work;
	struct delayed_work preset_dc_work;

	struct delayed_work pre_cc_work;
	struct delayed_work cc_work;
	struct delayed_work cv_work;

	struct delayed_work update_bat_work;
	struct delayed_work error_work;

	struct delayed_work adc_work;

	/* for state machine control */
	struct {
		bool pps_cl;
		bool pps_c_up;
		bool pps_c_down;
		bool pps_v_up;
		bool pps_v_down;
		int pps_v_offset;
		int pps_c_offset;
		u16 prev_adc_ibus;
		u16 prev_adc_vbus;
	}wq;

	int irq;

	struct {
		u8 state;
		u32 err;
		u32 vbat_reg;
		u32 ibat_reg;
		u32 ibus_lim;

		u32 cv_gl;
		u32 ci_gl;
		u32 cc_gl;
	}chg;

	struct {
		u32 pdo_pos;
		u32 pps_v_max;
		u32 pps_v_max_1;
		u32 pps_c_max;
		u32 pps_p_max;
		u32 pps_v;
		u32 pps_c;
	}ta;

	/* for reverse-boost */
	bool rev_boost;

	bool cable_online;
	bool vbus_in;
	bool req_update_vbat;
	bool req_update_ibus;
	bool req_update_ibat;
	u32 target_vbat;
	u32 target_ibat;
	u32 target_ibus;
	u32 max_vbat;
	u8 adc_mode;

	/* debug */
	struct dentry *debug_root;
	u32 debug_address;
};


#endif  /* __SM5440_CHARGER_H__ */
