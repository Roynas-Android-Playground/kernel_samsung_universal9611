/*
 * sm5440_charger.c - SM5440 Charger device driver for SAMSUNG platform
 *
 * Copyright (C) 2019 SiliconMitus Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/sec_batt.h>
#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#endif /* CONFIG_OF */
#include "../common/include/sec_charging_common.h"
#include "../common/include/sec_direct_charger.h"
#include "sm5440_charger.h"

//#define COMPARE_FG_SENS 	1
#define APPLY_SW_OCP		1	   /* SM5440 can't used HW_OCP, if you want used SW_OCP W/A, please keep it. */
#define SUPPORT_PPS_REMAIN	1
/* for DC workqueue control */
static int cc_cnt = 0;
static int cv_cnt = 0;

char *sm5440_dc_state_str[] = {
	"NO_CHARGING",
	"DC_ERR",
	"CHARGING_DONE",
	"CHECK_VBAT",
	"PRESET_DC",
    "ADJUST_CC",
	"UPDATE_BAT",
	"CC_MODE",
	"CV_MODE"
};

static u32 sm5440_pps_v(u32 vol)
{
	u32 ret;

	if ((vol%PPS_V_STEP) >= (PPS_V_STEP / 2)) {
		vol += PPS_V_STEP;
	}

	ret = (vol / PPS_V_STEP) * PPS_V_STEP;

	return ret;
}

static u32 sm5440_pps_c(u32 cur)
{
	u32 ret;

	if ((cur % PPS_C_STEP) >= (PPS_C_STEP / 2)) {
		cur += PPS_C_STEP;
	}

	ret = (cur / PPS_C_STEP) * PPS_C_STEP;

	return ret;
}

static int sm5440_read_reg(struct sm5440_charger *sm5440, u8 reg, u8 *dest)
{
	int ret;

	ret = i2c_smbus_read_byte_data(sm5440->i2c, reg);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: fail to i2c_read(ret=%d)\n", __func__, ret);
		return ret;
	}
	*dest = (ret & 0xff);

	return 0;
}

int sm5440_bulk_read(struct sm5440_charger *sm5440, u8 reg, int count, u8 *buf)
{
	int ret;

	ret = i2c_smbus_read_i2c_block_data(sm5440->i2c, reg, count, buf);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static int sm5440_write_reg(struct sm5440_charger *sm5440, u8 reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(sm5440->i2c, reg, value);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: fail to i2c_write(ret=%d)\n", __func__, ret);
	}

	return ret;
}

static int sm5440_update_reg(struct sm5440_charger *sm5440, u8 reg,
		u8 val, u8 mask, u8 pos)
{
	int ret;

	mutex_lock(&sm5440->i2c_lock);

	ret = i2c_smbus_read_byte_data(sm5440->i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) << pos | (old_val & ~(mask << pos));
		ret = i2c_smbus_write_byte_data(sm5440->i2c, reg, new_val);
	}

	mutex_unlock(&sm5440->i2c_lock);

	return ret;
}

static u32 sm5440_get_vbatreg(struct sm5440_charger *sm5440)
{
	u8 reg;

	sm5440_read_reg(sm5440, SM5440_REG_VBATCNTL, &reg);

	return 3800 + (((reg & 0x3F) * 125) / 10);
}

static u32 sm5440_get_ibuslim(struct sm5440_charger *sm5440)
{
	u8 reg;

	sm5440_read_reg(sm5440, SM5440_REG_IBUSCNTL, &reg);

	return (reg & 0x7F) * 50;
}

static int sm5440_set_vbatreg(struct sm5440_charger *sm5440, u32 vbatreg)
{
	u8 reg = ((vbatreg - 3800) * 10) / 125;

	return sm5440_update_reg(sm5440, SM5440_REG_VBATCNTL, reg, 0x3F, 0);
}

static int sm5440_set_ibuslim(struct sm5440_charger *sm5440, u32 ibuslim)
{
	u8 reg = ibuslim / 50;

	return sm5440_write_reg(sm5440, SM5440_REG_IBUSCNTL, reg);
}

static int sm5440_set_freq(struct sm5440_charger *sm5440, u32 freq)
{
	u8 reg = ((freq - 250) / 50) & 0x1F;

	return sm5440_update_reg(sm5440, SM5440_REG_CNTL7, reg, 0x1F, 0);
}

static int sm5440_set_op_mode(struct sm5440_charger *sm5440, u8 op_mode)
{
	int ret;

	ret = sm5440_update_reg(sm5440, SM5440_REG_CNTL5, op_mode, 0x3, 2);

	return ret;
}

static u8 sm5440_get_op_mode(struct sm5440_charger *sm5440)
{
	u8 reg;

	sm5440_read_reg(sm5440, SM5440_REG_CNTL5, &reg);

	return (reg >> 2) & 0x3;
}

#if defined(COMPARE_FG_SENS)
static int get_battery_vnow(struct sm5440_charger *sm5440)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret;

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		dev_err(sm5440->dev, "fail to find battery power supply\n");
		return -EINVAL;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	power_supply_put(psy);
	if (ret < 0) {
		dev_err(sm5440->dev, "fail to get battery VNOW\n");
		return -EINVAL;
	}

	return val.intval / 1000;
}

static int get_battery_inow(struct sm5440_charger *sm5440)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret;

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		dev_err(sm5440->dev, "fail to find battery power supply\n");
		return -EINVAL;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	power_supply_put(psy);
	if (ret < 0) {
		dev_err(sm5440->dev, "fail to get battery INOW\n");
		return -EINVAL;
	}

	return val.intval;
}
#endif

static int sm5440_convert_adc(struct sm5440_charger *sm5440, u8 index)
{
	u8 reg1, reg2;
	int adc;

	if (sm5440->chg.state < SM5440_DC_CHECK_VBAT) {
		/* Didn't worked ADC block during on CHG-OFF status */
		return 0;
	}

	switch (index) {
		case SM5440_ADC_THEM:
			sm5440_read_reg(sm5440, SM5440_REG_ADC_THEM1, &reg1);
			sm5440_read_reg(sm5440, SM5440_REG_ADC_THEM2, &reg2);
			adc = ((((int)reg1 << 5) | (reg2 >> 3)) * 250) / 1000;
			break;
		case SM5440_ADC_DIETEMP:	/* unit - C x 10 */
			sm5440_read_reg(sm5440, SM5440_REG_ADC_DIETEMP, &reg1);
			adc = 225 + (u16)reg1 * 5;
			break;
		case SM5440_ADC_VBAT1:
			sm5440_read_reg(sm5440, SM5440_REG_ADC_VBAT1, &reg1);
			sm5440_read_reg(sm5440, SM5440_REG_ADC_VBAT2, &reg2);
			adc = 2048 + (((((int)reg1 << 5) | (reg2 >> 3)) * 500) / 1000);
			break;
		case SM5440_ADC_VOUT:
			sm5440_read_reg(sm5440, SM5440_REG_ADC_VOUT1, &reg1);
			sm5440_read_reg(sm5440, SM5440_REG_ADC_VOUT2, &reg2);
			adc = 2048 + (((((int)reg1 << 5) | (reg2 >> 3)) * 500) / 1000);
			break;
		case SM5440_ADC_IBUS:
			sm5440_read_reg(sm5440, SM5440_REG_ADC_IBUS1, &reg1);
			sm5440_read_reg(sm5440, SM5440_REG_ADC_IBUS2, &reg2);
			adc = ((((int)reg1 << 5) | (reg2 >> 3)) * 625) / 1000;
			break;
		case SM5440_ADC_VBUS:
			sm5440_read_reg(sm5440, SM5440_REG_ADC_VBUS1, &reg1);
			sm5440_read_reg(sm5440, SM5440_REG_ADC_VBUS2, &reg2);
			adc = 4096 + (((u16)reg1 << 5) | (reg2 >> 3));
			break;
		default:
			adc = 0;
	}

	return adc;
}

static int sm5440_get_adc_values(struct sm5440_charger *sm5440, const char *str,
		int *vbus, int *ibus, int *vout, int *vbat,
		int *them, int *dietemp)
{
	int adc_vbus, adc_ibus, adc_vout, adc_vbat, adc_them, adc_dietemp;
#if defined(COMPARE_FG_SENS)
	int vnow, inow;
#endif

	adc_vbus = sm5440_convert_adc(sm5440, SM5440_ADC_VBUS);
	adc_ibus = sm5440_convert_adc(sm5440, SM5440_ADC_IBUS);
	adc_vout = sm5440_convert_adc(sm5440, SM5440_ADC_VOUT);
	adc_vbat = sm5440_convert_adc(sm5440, SM5440_ADC_VBAT1);
	adc_them = sm5440_convert_adc(sm5440, SM5440_ADC_THEM);
	adc_dietemp = sm5440_convert_adc(sm5440, SM5440_ADC_DIETEMP);

#if defined(COMPARE_FG_SENS)
	vnow = get_battery_vnow(sm5440);
	inow = get_battery_inow(sm5440);
	dev_info(sm5440->dev, "%s:vbus:%d:ibus:%d:vout:%d:vbat:%d:vnow:%d:inow:%d:them:%d:dietemp:%d (mode=%d)\n",
			str, adc_vbus, adc_ibus, adc_vout, adc_vbat, vnow, inow, adc_them, adc_dietemp, sm5440->adc_mode);
#else
	dev_info(sm5440->dev, "%s:vbus:%d:ibus:%d:vout:%d:vbat:%d:them:%d:dietemp:%d (mode=%d)\n",
			str, adc_vbus, adc_ibus, adc_vout, adc_vbat, adc_them, adc_dietemp, sm5440->adc_mode);
#endif

	if (vbus) { *vbus = adc_vbus; }
	if (ibus) { *ibus = adc_ibus; }
	if (vout) { *vout = adc_vout; }
	if (vbat) { *vbat = adc_vbat; }
	if (them) { *them = adc_them; }
	if (dietemp) { *dietemp = adc_dietemp; }

	return 0;
}

static void sm5440_print_regmap(struct sm5440_charger *sm5440)
{
	const u8 print_reg_num = SM5440_REG_DEVICEID - SM5440_REG_INT4;
	u8 regs[64] = {0x0, };
	char temp_buf[128] = {0,};
	int i;

	sm5440_bulk_read(sm5440, SM5440_REG_MSK1, 0x1F, regs);
	sm5440_bulk_read(sm5440, SM5440_REG_MSK1 + 0x1F,
			print_reg_num - 0x1F, regs + 0x1F);

	for (i = 0; i < print_reg_num; ++i) {
		sprintf(temp_buf+strlen(temp_buf), "0x%02X[0x%02X],", SM5440_REG_MSK1 + i, regs[i]);
		if (((i+1) % 10 == 0) || ((i+1) == print_reg_num)) {
			pr_info("sm5440-charger: regmap: %s\n", temp_buf);
			memset(temp_buf, 0x0, sizeof(temp_buf));
		}
	}
}

static int sm5440_enable_chg_timer(struct sm5440_charger *sm5440, bool enable)
{
	int ret;

	ret = sm5440_update_reg(sm5440, SM5440_REG_CNTL3, enable, 0x1, 2);

	return ret;
}

static int sm5440_set_wdt_timer(struct sm5440_charger *sm5440, u8 tmr)
{
	int ret;

	ret = sm5440_update_reg(sm5440, SM5440_REG_CNTL1, tmr, 0x7, 4);

	return ret;
}

static int sm5440_enable_wdt(struct sm5440_charger *sm5440, bool enable)
{
	int ret;

	ret = sm5440_update_reg(sm5440, SM5440_REG_CNTL1, enable, 0x1, 7);

	return ret;
}

static int sm5440_set_adc_rate(struct sm5440_charger *sm5440, u8 rate)
{
	int ret;

	ret = sm5440_update_reg(sm5440, SM5440_REG_ADCCNTL1, rate, 0x1, 1);

	return ret;
}

static int sm5440_enable_adc(struct sm5440_charger *sm5440, bool enable)
{
	int ret;

	ret = sm5440_update_reg(sm5440, SM5440_REG_ADCCNTL1, enable, 0x1, 0);

	return ret;
}

static int sm5440_setup_adc(struct sm5440_charger *sm5440, u8 mode)
{
	switch (mode) {
		case SM5440_ADC_MODE_ONESHOT:
			sm5440_enable_adc(sm5440, 0);
			msleep(10);
			sm5440_set_adc_rate(sm5440, SM5440_ADC_MODE_ONESHOT);
			schedule_delayed_work(&sm5440->adc_work, msecs_to_jiffies(200));
			sm5440_enable_adc(sm5440, 1);
			break;
		case SM5440_ADC_MODE_CONTINUOUS:
			cancel_delayed_work(&sm5440->adc_work);
			sm5440_enable_adc(sm5440, 0);
			msleep(50);
			sm5440_set_adc_rate(sm5440, SM5440_ADC_MODE_CONTINUOUS);
			sm5440_enable_adc(sm5440, 1);
			break;
		case SM5440_ADC_MODE_OFF:
		default:
			cancel_delayed_work(&sm5440->adc_work);
			sm5440_enable_adc(sm5440, 0);
			break;
	}
	sm5440->adc_mode = mode;

	return 0;
}

static int sm5440_get_apdo_max_power(struct sm5440_charger *sm5440)
{
	int ret, cnt;

	sm5440->ta.pdo_pos = 0;		 /* set '0' else return error */
	sm5440->ta.pps_v_max = 10000;   /* request voltage level */
	sm5440->ta.pps_c_max = 0;
	sm5440->ta.pps_p_max = 0;

	for (cnt = 0; cnt < 3; ++cnt) {
		ret = sec_pd_get_apdo_max_power(&sm5440->ta.pdo_pos, &sm5440->ta.pps_v_max,
				&sm5440->ta.pps_c_max, &sm5440->ta.pps_p_max);
		if (ret < 0) {
			dev_err(sm5440->dev, "%s: error:sec_pd_get_apdo_max_power, RETRY=%d\n",
					__func__, cnt);
		} else {
			break;
		}
	}

	if (cnt == 3) {
		dev_err(sm5440->dev, "%s: fail to get apdo_max_power(ret=%d)\n",
				__func__, ret);
	}

	dev_info(sm5440->dev, "%s: pdo_pos:%d, max_vol:%dmV, max_cur:%dmA, max_pwr:%dmW\n",
			__func__, sm5440->ta.pdo_pos, sm5440->ta.pps_v_max, sm5440->ta.pps_c_max,
			sm5440->ta.pps_p_max);

	return ret;
}

static int sm5440_report_dc_state(struct sm5440_charger *sm5440)
{
	union power_supply_propval val = {0,};
	static int prev_val = SEC_DIRECT_CHG_MODE_DIRECT_OFF;

	switch (sm5440->chg.state) {
		case SM5440_DC_CHG_OFF:
		case SM5440_DC_ERR:
			val.intval = SEC_DIRECT_CHG_MODE_DIRECT_OFF;
			break;
		case SM5440_DC_EOC:
			val.intval = SEC_DIRECT_CHG_MODE_DIRECT_DONE;
			break;
		case SM5440_DC_CHECK_VBAT:
			val.intval = SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT;
			break;
		case SM5440_DC_PRESET:
			val.intval = SEC_DIRECT_CHG_MODE_DIRECT_PRESET;
			break;

		case SM5440_DC_PRE_CC:
		case SM5440_DC_UPDAT_BAT:
			val.intval = SEC_DIRECT_CHG_MODE_DIRECT_ON_ADJUST;
			break;
		case SM5440_DC_CC:
		case SM5440_DC_CV:
			val.intval = SEC_DIRECT_CHG_MODE_DIRECT_ON;
			break;
		default:
			return -EINVAL;
	}

	if (prev_val != val.intval) {
		psy_do_property(sm5440->pdata->battery.sec_dc_name, set,
				POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, val);
	}

	return 0;
}

static int sm5440_request_state_work(struct sm5440_charger *sm5440,
		u8 state, u32 delay)
{
	int ret = 0;

	mutex_lock(&sm5440->st_lock);

	switch (state) {
		case SM5440_DC_CHECK_VBAT:
			queue_delayed_work(sm5440->dc_wqueue, &sm5440->check_vbat_work,
					msecs_to_jiffies(delay));
			break;
		case SM5440_DC_PRESET:
			queue_delayed_work(sm5440->dc_wqueue, &sm5440->preset_dc_work,
					msecs_to_jiffies(delay));
			break;
		case SM5440_DC_PRE_CC:
			queue_delayed_work(sm5440->dc_wqueue, &sm5440->pre_cc_work,
					msecs_to_jiffies(delay));
			break;
		case SM5440_DC_CC:
			queue_delayed_work(sm5440->dc_wqueue, &sm5440->cc_work,
					msecs_to_jiffies(delay));
			break;
		case SM5440_DC_CV:
			queue_delayed_work(sm5440->dc_wqueue, &sm5440->cv_work,
					msecs_to_jiffies(delay));
			break;
		case SM5440_DC_UPDAT_BAT:
			queue_delayed_work(sm5440->dc_wqueue, &sm5440->update_bat_work,
					msecs_to_jiffies(delay));
			break;
		case SM5440_DC_ERR:
			queue_delayed_work(sm5440->dc_wqueue, &sm5440->error_work,
					msecs_to_jiffies(delay));
			break;
		default:
			dev_err(sm5440->dev, "%s: invalid state(%d)\n", __func__, state);
			ret = -EINVAL;
			break;
	}

	mutex_unlock(&sm5440->st_lock);

	return ret;
}

#if 0
static int sm5440_sw_reset(struct sm5440_charger *sm5440)
{
	u8 i, reg;

	sm5440_write_reg(sm5440, SM5440_REG_CNTL1, 0x1);		/* Do SW RESET */

	for (i=0; i < 0xff; ++i) {
		msleep(1);
		sm5440_read_reg(sm5440, SM5440_REG_CNTL1, &reg);
		if (!(reg & 0x1)) {
			break;
		}
	}

	if (i == 0xff) {
		return -EBUSY;
	}
	return 0;
}

static int sm5440_reverse_boost_enable(struct sm5440_charger *sm5440, bool enable)
{
	u8 i, reg;

	if (enable) {
		sm5440_set_op_mode(sm5440, OP_MODE_REV_BYPASS);
		for (i=0; i < 10; ++i) {
			msleep(10);
			sm5440_read_reg(sm5440, SM5440_REG_STATUS4, &reg);
			if (reg & (0x1 << 6)) {
				break;
			}
		}

		if (i == 10) {
			dev_err(sm5440->dev, "%s: fail to reverse boost enable\n", __func__);
			sm5440->rev_boost = 0;
			return -EINVAL;
		}
		sm5440->rev_boost = enable;
		dev_info(sm5440->dev, "%s: ON\n", __func__);
	} else {
		sm5440_set_op_mode(sm5440, OP_MODE_CHG_OFF);
		sm5440->rev_boost = enable;
		dev_info(sm5440->dev, "%s: OFF\n", __func__);
	}

	return 0;
}
#endif

static int sm5440_set_ENHIZ(struct sm5440_charger *sm5440, bool vbus, bool chg_en)
{   
	u8 enhiz = 0;   /* case : chg_on or chg_off & vbus_uvlo */

	if (vbus && !(chg_en)) {
		enhiz = 1;              /* case : chg_off & vbus_pok */
	}

	dev_info(sm5440->dev, "%s: vbus=%d, chg_en=%d, enhiz=%d\n", 
			__func__, vbus, chg_en, enhiz);
	sm5440_update_reg(sm5440, SM5440_REG_CNTL6, enhiz, 0x1, 7);

	return 0;
}
static int sm5440_enable_charging(struct sm5440_charger *sm5440, bool enable)
{
	sm5440_set_ENHIZ(sm5440, sm5440->vbus_in, enable);
	if (enable) {
		sm5440_set_op_mode(sm5440, OP_MODE_CHG_ON);
	} else {
		sm5440_set_op_mode(sm5440, OP_MODE_CHG_OFF);
	}
	sm5440_enable_wdt(sm5440, enable);

	return 0;
}

static int sm5440_setup_pps_work_charging_config(struct sm5440_charger *sm5440)
{
	sm5440->wq.pps_cl = 0;
	sm5440->wq.pps_c_down = 0;
	sm5440->wq.pps_c_up = 0;
	sm5440->wq.pps_v_down = 0;
	sm5440->wq.pps_v_up = 0;
	sm5440->wq.prev_adc_ibus = 0;
	sm5440->wq.prev_adc_vbus = 0;
	cc_cnt = 0;
	cv_cnt = 0;

	sm5440->chg.cv_gl = sm5440->target_vbat;
	sm5440->chg.ci_gl = sm5440_pps_c(MIN(sm5440->ta.pps_c_max, (sm5440->target_ibus * 100) / 100));
	sm5440->chg.cc_gl = sm5440_pps_c(sm5440->target_ibus * 2);
	dev_info(sm5440->dev, "%s: CV_GL=%dmV, CI_GL=%dmA, CC_GL=%dmA\n",
			__func__, sm5440->chg.cv_gl, sm5440->chg.ci_gl, sm5440->chg.cc_gl);

	sm5440->chg.vbat_reg = sm5440->chg.cv_gl + sm5440->pdata->cv_gl_offset;
	if (sm5440->chg.ci_gl <= sm5440->pdata->ta_min_current) {
		sm5440->chg.ibus_lim = sm5440->chg.ci_gl + (sm5440->pdata->ci_gl_offset * 2);
	} else {
		sm5440->chg.ibus_lim = sm5440->chg.ci_gl + sm5440->pdata->ci_gl_offset;
	}
	sm5440->chg.ibat_reg = sm5440->chg.cc_gl + sm5440->pdata->cc_gl_offset;
	sm5440_set_ibuslim(sm5440, sm5440->chg.ibus_lim);
	sm5440_set_vbatreg(sm5440, sm5440->chg.vbat_reg);
	dev_info(sm5440->dev, "%s: vbat_reg=%dmV, ibus_lim=%dmA, ibat_reg=%dmA\n",
			__func__, sm5440->chg.vbat_reg, sm5440->chg.ibus_lim,
			sm5440->chg.ibat_reg);

	return 0;
}

static int sm5440_terminate_charging_work(struct sm5440_charger *sm5440)
{
	flush_workqueue(sm5440->dc_wqueue);

	cancel_delayed_work_sync(&sm5440->check_vbat_work);
	cancel_delayed_work_sync(&sm5440->preset_dc_work);
	cancel_delayed_work_sync(&sm5440->pre_cc_work);
	cancel_delayed_work_sync(&sm5440->cc_work);
	cancel_delayed_work_sync(&sm5440->cv_work);
	cancel_delayed_work_sync(&sm5440->update_bat_work);
	cancel_delayed_work_sync(&sm5440->error_work);

	sm5440_enable_charging(sm5440, 0);

	return 0;
}

static bool sm5440_check_charging_enable(struct sm5440_charger *sm5440)
{
	u8 op_mode = sm5440_get_op_mode(sm5440);

	if (op_mode == 0x1 && sm5440->chg.state > SM5440_DC_EOC) {
		return true;
	}

	return false;
}

static int sm5440_start_charging(struct sm5440_charger *sm5440)
{
	int ret;

	if (!sm5440->cable_online) {
		dev_err(sm5440->dev, "%s: can't detect valid cable connection(online=%d)\n",
				__func__, sm5440->cable_online);
		return -ENODEV;
	}
	ret = sm5440_get_apdo_max_power(sm5440);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: fail to get APDO(ret=%d)\n", __func__, ret);
		return ret;
	}

	if (sm5440->chg.state >= SM5440_DC_CHECK_VBAT) {
		dev_err(sm5440->dev, "%s: already work on dc (state=%d)\n",
				__func__, sm5440->chg.state);
		return -EINVAL;
	}

	sm5440_set_wdt_timer(sm5440, WDT_TIMER_S_30);
	sm5440_enable_chg_timer(sm5440, false);
	sm5440_setup_adc(sm5440, SM5440_ADC_MODE_ONESHOT);

	__pm_stay_awake(&sm5440->wake_lock);
	mutex_lock(&sm5440->st_lock);
	sm5440->chg.state = SM5440_DC_CHECK_VBAT;   /* Pre-update chg.state */
	mutex_unlock(&sm5440->st_lock);
	sm5440_request_state_work(sm5440, SM5440_DC_CHECK_VBAT, DELAY_PPS_UPDATE);

	dev_info(sm5440->dev, "%s: done\n", __func__);

	return 0;
}

static int sm5440_stop_charging(struct sm5440_charger *sm5440)
{
    mutex_lock(&sm5440->st_lock);
    sm5440->chg.state = SM5440_DC_CHG_OFF;
    mutex_unlock(&sm5440->st_lock);
    sm5440_terminate_charging_work(sm5440);
    __pm_relax(&sm5440->wake_lock);

	sm5440_setup_adc(sm5440, SM5440_ADC_MODE_OFF);

	sm5440_report_dc_state(sm5440);

	dev_info(sm5440->dev, "%s: done\n", __func__);

	return 0;
}

static int sm5440_update_vbat_reg(struct sm5440_charger *sm5440)
{
	int adc_vbat;
	int ret = 0;

	if (sm5440->chg.state < SM5440_DC_CHECK_VBAT) {
		sm5440_set_vbatreg(sm5440, sm5440->target_vbat);
		sm5440->chg.vbat_reg = sm5440->target_vbat;
		dev_info(sm5440->dev, "%s: VBAT updated on NOT_CHARGING\n", __func__);
	} else {
		adc_vbat = sm5440_convert_adc(sm5440, SM5440_ADC_VBAT1);
		if (sm5440->target_vbat > adc_vbat) {
			mutex_lock(&sm5440->st_lock);
			sm5440->req_update_vbat = 1;
			mutex_unlock(&sm5440->st_lock);
			dev_info(sm5440->dev, "%s: request VBAT update on DC work\n", __func__);
		} else {
			dev_err(sm5440->dev, "%s: vbat(%dmV) less then adc_vbat(%dmV)\n",
					__func__, sm5440->target_vbat, adc_vbat);
			sm5440->target_vbat = sm5440->chg.vbat_reg;
			ret = -EINVAL;
		}
	}

	return ret;
}

static int sm5440_update_ibus_reg(struct sm5440_charger *sm5440)
{
	if (sm5440->chg.state < SM5440_DC_CHECK_VBAT) {
		dev_info(sm5440->dev, "%s: ibus will be update at start of charging\n"
				, __func__);
	} else {
		mutex_lock(&sm5440->st_lock);
		if (sm5440->chg.ibus_lim != sm5440_pps_c(sm5440->target_ibus) + sm5440->pdata->ci_gl_offset) {
			sm5440->req_update_ibus = 1;
			dev_info(sm5440->dev, "%s: request IBUS update on DC work\n", __func__);
		}
		mutex_unlock(&sm5440->st_lock);
	}

	return 0;
}

static int sm5440_check_error_state(struct sm5440_charger *sm5440, u8 retry_state)
{
	int adc_vbat;
	u8 st[5], op_mode;

	sm5440_bulk_read(sm5440, SM5440_REG_STATUS1, 4, st);
	dev_info(sm5440->dev, "%s: status 0x%x:0x%x:0x%x:0x%x\n",
			__func__, st[0], st[1], st[2], st[3]);

	if (sm5440->chg.state == SM5440_DC_ERR) {
		dev_err(sm5440->dev, "%s: already occurred error (err=0x%x)\n",
				__func__, sm5440->chg.err);
		return -EINVAL;
	}

	op_mode = sm5440_get_op_mode(sm5440);
	if (op_mode == 0x0) {
		sm5440_bulk_read(sm5440, SM5440_REG_INT1, 4, st);
		dev_info(sm5440->dev, "%s: int 0x%x:0x%x:0x%x:0x%x\n",
				__func__, st[0], st[1], st[2], st[3]);
		dev_err(sm5440->dev, "%s: disabled chg, but not detect error\n",
				__func__);
		sm5440->chg.err = SM5440_ERR_UNKNOWN;
		sm5440_request_state_work(sm5440, SM5440_DC_ERR, DELAY_NONE);
		return -EINVAL;
	}

	adc_vbat = sm5440_convert_adc(sm5440, SM5440_ADC_VBAT1);
	if (adc_vbat <= SM5440_DC_VBAT_MIN ) {
		dev_err(sm5440->dev, "%s: abnormal adc_vbat(%d)\n", __func__, adc_vbat);
		sm5440->chg.err = SM5440_ERR_INVAL_VBAT;
		sm5440_request_state_work(sm5440, SM5440_DC_ERR, DELAY_NONE);
		return -EINVAL;
	}

	if (st[2] & SM5440_INT3_VBUSUVLO) {
		dev_info(sm5440->dev, "%s: vbus uvlo. wait 2sec\n", __func__);
		sm5440_request_state_work(sm5440, retry_state, DELAY_RETRY);
		return -EINVAL;
	}

	return 0;
}

static int sm5440_send_pd_message(struct sm5440_charger *sm5440)
{
	int ret;

	if (sm5440->chg.state < SM5440_DC_CHECK_VBAT) {
		return -EINVAL;
	}

	mutex_lock(&sm5440->pd_lock);

	dev_info(sm5440->dev, "DC_WORK:%s: pdo_idx=%d, pps_vol=%dmV(max=%dmV) pps_cur=%dmA(max=%dmA)\n",
			__func__, sm5440->ta.pdo_pos, sm5440->ta.pps_v, sm5440->ta.pps_v_max,
			sm5440->ta.pps_c, sm5440->ta.pps_c_max);

	ret = sec_pd_select_pps(sm5440->ta.pdo_pos, sm5440->ta.pps_v, sm5440->ta.pps_c);
	if (ret == -EBUSY) {
		dev_err(sm5440->dev, "DC_WORK:%s: request again\n", __func__);
		msleep(100);
		ret = sec_pd_select_pps(sm5440->ta.pdo_pos, sm5440->ta.pps_v, sm5440->ta.pps_c);
	}

	mutex_unlock(&sm5440->pd_lock);

	if (ret < 0) {
		dev_err(sm5440->dev, "DC_WORK:%s: fail to send pd message(ret=%d)\n", __func__, ret);
		sm5440->chg.err = SM5440_ERR_SEND_PD_MSG;
		sm5440_request_state_work(sm5440, SM5440_DC_ERR, DELAY_NONE);
	}

	return ret;
}

static int psy_chg_get_online(struct sm5440_charger *sm5440)
{
	u8 reg;

	sm5440_read_reg(sm5440,SM5440_REG_STATUS3, &reg);
	dev_info(sm5440->dev, "%s: STATUS3=0x%x\n", __func__, reg);

	return (reg >> 5) & 0x1;
}

static int psy_chg_get_status(struct sm5440_charger *sm5440)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 st[5];

	sm5440_bulk_read(sm5440, SM5440_REG_STATUS1, 4, st);
	dev_info(sm5440->dev, "%s: status 0x%x:0x%x:0x%x:0x%x:0x%x\n",
			__func__, st[0], st[1], st[2], st[3], st[4]);

	if (sm5440_check_charging_enable(sm5440)) {
		status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		if ((st[2] << 5) & 0x1) { /* check vbus-pok */
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		}
	}

	return status;
}

static int psy_chg_get_health(struct sm5440_charger *sm5440)
{
	u8 reg, op_mode;
	int health = POWER_SUPPLY_HEALTH_UNKNOWN;
	int adc_vbus;

	if (sm5440->chg.state == SM5440_DC_ERR) {
		health = POWER_SUPPLY_HEALTH_DC_ERR;
		dev_info(sm5440->dev, "%s: chg_state=%d, health=%d\n",
				__func__, sm5440->chg.state, health);
	} else {
		op_mode = sm5440_get_op_mode(sm5440);
		if (op_mode == 0x0) {
			adc_vbus = sm5440_convert_adc(sm5440, SM5440_ADC_VBUS);
			if (adc_vbus < 3800) {
				health = POWER_SUPPLY_HEALTH_DC_ERR;
			} else {
				health = POWER_SUPPLY_HEALTH_GOOD;
			}
			dev_info(sm5440->dev, "%s: op_mode=%d, adc_vbus=%d, health=%d\n",
					__func__, op_mode, adc_vbus, health);
		} else {
			sm5440_read_reg(sm5440, SM5440_REG_STATUS3, &reg);
			if ((reg >> 5) & 0x1) {
				health = POWER_SUPPLY_HEALTH_GOOD;
			} else if (((reg >> 6) & 0x1) || ((reg >> 7) & 0x1)) {
				health = POWER_SUPPLY_HEALTH_DC_ERR;
			}
			dev_info(sm5440->dev, "%s: op_mode=%d, status3=0x%x, health=%d\n",
					__func__, op_mode, reg, health);
		}
	}

	return health;
}

static int psy_chg_get_chg_vol(struct sm5440_charger *sm5440)
{
	u32 chg_vol = sm5440_get_vbatreg(sm5440);

	dev_info(sm5440->dev, "%s: VBAT_REG=%dmV\n", __func__, chg_vol);

	return chg_vol;
}

static bool psy_chg_get_chg_enable(struct sm5440_charger *sm5440)
{
	return sm5440_check_charging_enable(sm5440);
}

static int psy_chg_get_input_curr(struct sm5440_charger *sm5440)
{
	u32 input_curr = sm5440_get_ibuslim(sm5440);

	dev_info(sm5440->dev, "%s: IBUSLIM=%dmA\n", __func__, input_curr);

	return input_curr;
}

static int psy_chg_get_ext_monitor_work(struct sm5440_charger *sm5440)
{
	if (sm5440->chg.state < SM5440_DC_CHECK_VBAT) {
		dev_info(sm5440->dev, "%s: charging off state (state=%d)\n",
				__func__, sm5440->chg.state);
		return -EINVAL;
	}

	sm5440_get_adc_values(sm5440, "Monitor", NULL, NULL, NULL, NULL, NULL, NULL);
	sm5440_print_regmap(sm5440);

	return 0;
}

static int psy_chg_get_ext_measure_input(struct sm5440_charger *sm5440, int index)
{
	int adc;

	switch (index) {
		case SEC_BATTERY_IIN_MA:
			adc = sm5440_convert_adc(sm5440, SM5440_ADC_IBUS);
			break;
		case SEC_BATTERY_IIN_UA:
			adc = sm5440_convert_adc(sm5440, SM5440_ADC_IBUS) * 1000;
			break;
		case SEC_BATTERY_VIN_MA:
			adc = sm5440_convert_adc(sm5440, SM5440_ADC_VBUS);
			break;
		case SEC_BATTERY_VIN_UA:
			adc = sm5440_convert_adc(sm5440, SM5440_ADC_VBUS) * 1000;
			break;
		default:
			adc = 0;
			break;
	}

	dev_info(sm5440->dev, "%s: index=%d, adc=%d\n", __func__, index, adc);

	return adc;
}

static int sm5440_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct sm5440_charger *sm5440 = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp =
		(enum power_supply_ext_property) psp;
	int adc_them;

	dev_info(sm5440->dev, "%s: psp=%d\n", __func__, psp);

	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = psy_chg_get_online(sm5440);
			break;
		case POWER_SUPPLY_PROP_STATUS:
			val->intval = psy_chg_get_status(sm5440);
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			val->intval = psy_chg_get_health(sm5440);
			break;
		case POWER_SUPPLY_PROP_CHARGING_ENABLED:
			val->intval = psy_chg_get_chg_enable(sm5440);
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
			val->intval = psy_chg_get_chg_vol(sm5440);
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		case POWER_SUPPLY_PROP_CURRENT_AVG:
			val->intval = psy_chg_get_input_curr(sm5440);
			break;
		case POWER_SUPPLY_PROP_CURRENT_MAX: /* get input current which was set */
			val->intval = sm5440->chg.ibus_lim;
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW: /* get charge current which was set */
			val->intval = sm5440->chg.ibat_reg;
			break;
		case POWER_SUPPLY_PROP_TEMP:
			adc_them = sm5440_convert_adc(sm5440, SM5440_ADC_THEM);
			val->intval = adc_them * 1000;
			break;
		case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
			switch (ext_psp) {
				case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
					psy_chg_get_ext_monitor_work(sm5440);
					break;
				case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
					val->intval = psy_chg_get_ext_measure_input(sm5440, val->intval);
					break;
				case POWER_SUPPLY_EXT_PROP_MEASURE_SYS:
					dev_err(sm5440->dev, "%s: need to works\n", __func__);
					/**
					 *  Need to check operation details.. by SEC.
					 */
					val->intval = -EINVAL;
					break;
				case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_CHG_STATUS:
					val->strval = sm5440_dc_state_str[sm5440->chg.state];
					pr_info("%s: CHARGER_STATUS(%s)\n", __func__, val->strval);
					break;

				default:
					return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int psy_chg_set_online(struct sm5440_charger *sm5440, int online)
{
	int ret = 0;

	dev_info(sm5440->dev, "%s: online=%d\n", __func__, online);

	if (online == 0) {
		sm5440->cable_online = false;
		if (sm5440->chg.state > SM5440_DC_EOC) {
			sm5440_stop_charging(sm5440);
		}
	} else {
		sm5440->cable_online = true;
	}
	sm5440->vbus_in = sm5440->cable_online;
	sm5440_set_ENHIZ(sm5440, sm5440->vbus_in, sm5440_check_charging_enable(sm5440));

	return ret;
}

static int psy_chg_set_const_chg_voltage(struct sm5440_charger *sm5440, int vbat)
{
	int ret = 0;

	dev_info(sm5440->dev, "%s: vbat [%dmV] to [%dmV]\n",
			__func__, sm5440->target_vbat, vbat);

	if (sm5440->target_vbat != vbat) {
		sm5440->target_vbat = vbat;
		ret = sm5440_update_vbat_reg(sm5440);
	}

	return ret;
}

static int psy_chg_set_chg_curr(struct sm5440_charger *sm5440, int ibat)
{
	int ret = 0;

	dev_info(sm5440->dev, "%s: ibat [%dmA] to [%dmA]\n",
			__func__, sm5440->target_ibat, ibat);

	if (sm5440->target_ibat != ibat) {
		sm5440->target_ibat = ibat;
		dev_info(sm5440->dev, "%s: if want to control ibat, need to update code\n",
				__func__);
		/**
		 * Need to work, if you want used by Charging current loop
		 * control.
		 */
	}

	return ret;
}

static int psy_chg_set_input_curr(struct sm5440_charger *sm5440, int ibus)
{
	int ret = 0;

	dev_info(sm5440->dev, "%s: ibus [%dmA] to [%dmA]\n",
			__func__, sm5440->target_ibus, ibus);

	if (sm5440->target_ibus != ibus) {
		sm5440->target_ibus = ibus;
		if (sm5440->target_ibus < sm5440->pdata->ta_min_current) {
			dev_info(sm5440->dev, "%s: can't used less then ta_min_current(%dmA)\n",
					__func__, sm5440->pdata->ta_min_current);
			sm5440->target_ibus = sm5440->pdata->ta_min_current;
		}
		ret = sm5440_update_ibus_reg(sm5440);
	}

	return ret;
}

static int psy_chg_ext_direct_float_max(struct sm5440_charger *sm5440, int max_vbat)
{
	dev_info(sm5440->dev, "%s: max_vbat [%dmA] to [%dmA]\n",
			__func__, sm5440->max_vbat, max_vbat);

	sm5440->max_vbat = max_vbat;

	return 0;

}

static int sm5440_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct sm5440_charger *sm5440 = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp =
		(enum power_supply_ext_property) psp;

	int ret = 0;

	dev_info(sm5440->dev, "%s: psp=%d, val-intval=%d\n", __func__, psp, val->intval);

	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			psy_chg_set_online(sm5440, val->intval);
			break;
		case POWER_SUPPLY_PROP_CHARGING_ENABLED:
			if (val->intval) {
				ret = sm5440_start_charging(sm5440);
			} else {
				ret = sm5440_stop_charging(sm5440);
			}
			sm5440_print_regmap(sm5440);
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
			psy_chg_set_const_chg_voltage(sm5440, val->intval);
			break;
		case POWER_SUPPLY_PROP_CURRENT_AVG:
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			psy_chg_set_chg_curr(sm5440, val->intval);
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		case POWER_SUPPLY_PROP_CURRENT_MAX:
			psy_chg_set_input_curr(sm5440, val->intval);
			break;
		case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
			switch (ext_psp) {
				case POWER_SUPPLY_EXT_PROP_DIRECT_VOLTAGE_MAX:
					psy_chg_set_const_chg_voltage(sm5440, val->intval);
					break;
				case POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX:
					psy_chg_set_input_curr(sm5440, val->intval);
					break;
				case POWER_SUPPLY_EXT_PROP_DIRECT_FLOAT_MAX:
					psy_chg_ext_direct_float_max(sm5440, val->intval);
					break;
				case POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL:
					/**
					 *  Need to check operation details.. by SEC.
					 */
					dev_err(sm5440->dev, "%s: need to works\n", __func__);
					break;
				default:
					return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
	}

	if (ret < 0) {
		return ret;
	}

	return 0;
}

static char *sm5440_supplied_to[] = {
	"sm5440-charger",
};

static enum power_supply_property sm5440_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static const struct power_supply_desc sm5440_charger_power_supply_desc = {
	.name		= "sm5440-charger",
	.type		= POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property	= sm5440_chg_get_property,
	.set_property 	= sm5440_chg_set_property,
	.properties	= sm5440_charger_props,
	.num_properties	= ARRAY_SIZE(sm5440_charger_props),
};

static u8 check_loop_status(struct sm5440_charger *sm5440)
{
	u8 reg;

	sm5440_read_reg(sm5440, SM5440_REG_STATUS2, &reg);
	reg = reg & 0xC8;

	dev_info(sm5440->dev, "%s = 0x%x\n", __func__, reg);

	return reg;
}

static int update_work_state(struct sm5440_charger *sm5440, u8 state)
{
	int ret = 0;

	if (sm5440->chg.state == SM5440_DC_CHG_OFF) {
		dev_info(sm5440->dev, "%s: detected chg_off, terminate work\n", __func__);
		return -EBUSY;
	}

	if (sm5440->chg.state > SM5440_DC_PRESET &&
		sm5440->req_update_vbat | sm5440->req_update_ibus | sm5440->req_update_ibat) {
		dev_info(sm5440->dev, "%s: changed chg param, request: update_bat\n", __func__);
		sm5440_request_state_work(sm5440, SM5440_DC_UPDAT_BAT, DELAY_NONE);
		ret = -EINVAL;
	} else {
		if (sm5440->chg.state != state) {
			mutex_lock(&sm5440->st_lock);
			sm5440->chg.state = state;
			mutex_unlock(&sm5440->st_lock);
			sm5440_report_dc_state(sm5440);
		}
	}

	return ret;
}

#if APPLY_SW_OCP
#define MAX_CNT	3
static int sm5440_check_ocp(struct sm5440_charger *sm5440)
{
	int i, ret = 0;
	u8 reg;


	for (i=0; i < MAX_CNT; ++i) {
		sm5440_read_reg(sm5440, SM5440_REG_STATUS2, &reg);
		reg = reg & 0x82;   /* IBUSLIM | THEM_REG */
		if (reg == LOOP_IBUSLIM) {
			dev_err(sm5440->dev, "%s: IBUSLIM enabled(i=%d)\n", __func__, i);
			msleep(1000);
		} else {
			break;
		}
	}

	if (i == MAX_CNT) {
		dev_err(sm5440->dev, "%s: Detected SW/IBUSOCP\n", __func__);
		sm5440->chg.err = SM5440_ERR_IBUSOCP;
		sm5440_request_state_work(sm5440, SM5440_DC_ERR, DELAY_NONE);
		ret = -EIO;
	}

	return ret;
}
#endif

static void sm5440_check_vbat_work(struct work_struct *work)
{
	struct sm5440_charger *sm5440 = container_of(work, struct sm5440_charger,
			check_vbat_work.work);
	union power_supply_propval val;
	int adc_vbat;
	int ret;

	dev_info(sm5440->dev, "DC_WORK:%s\n", __func__);

	ret = update_work_state(sm5440, SM5440_DC_CHECK_VBAT);
	if (ret < 0) {
		return;
	}

	ret = psy_do_property(sm5440->pdata->battery.sec_dc_name, get,
			POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);
	if (ret < 0) {
		dev_err(sm5440->dev, "DC_WORK:%s: %s is not ready to work (wait 1sec)\n",
				__func__, sm5440->pdata->battery.sec_dc_name);
		sm5440_request_state_work(sm5440, SM5440_DC_CHECK_VBAT, DELAY_ADC_UPDATE);

		return;
	}

	if (val.intval == 0) {  /* already disabled switching charger */
		dev_info(sm5440->dev, "DC_WORK:%s: request: check_vbat -> preset\n", __func__);
		sm5440_request_state_work(sm5440, SM5440_DC_PRESET, DELAY_NONE);
	} else {
		adc_vbat = sm5440_convert_adc(sm5440, SM5440_ADC_VBAT1);
		dev_info(sm5440->dev, "DC_WORK:%s: adc:vbat=%dmV\n", __func__, adc_vbat);

		if (adc_vbat > sm5440->pdata->dc_min_vbat) {
			dev_err(sm5440->dev, "DC_WORK:%s: set_prop - disable sw_chg\n",
					__func__);
			val.intval = 0;
			psy_do_property(sm5440->pdata->battery.sec_dc_name, set,
					POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);
		}

		dev_err(sm5440->dev, "DC_WORK:%s: sw_chg not disabled yet (wait 1sec)\n",
				__func__);
		sm5440_request_state_work(sm5440, SM5440_DC_CHECK_VBAT, DELAY_NONE);
	}
}

static u32 get_pps_cl_v_offset(struct sm5440_charger *sm5440, u32 ci_val)
{
	u32 v_offset;

	v_offset = (ci_val * (sm5440->pdata->pps_lr + sm5440->pdata->rpara +
					sm5440->pdata->rsns * 2 + sm5440->pdata->rpcm * 2)) / 1000000;

	return v_offset;
}

static u32 get_v_init_offset(struct sm5440_charger *sm5440)
{
	u32 v_init_offset;

	v_init_offset = (sm5440->ta.pps_c * (sm5440->pdata->pps_lr + sm5440->pdata->rpara +
						(sm5440->pdata->rsns * 2) + (sm5440->pdata->rpcm * 2))) / 1000000;

	v_init_offset += 200;   /* add to extra offset */

	dev_info(sm5440->dev, "%s: ci_gl=%d, v_init_offset=%d\n", __func__, sm5440->chg.ci_gl, v_init_offset);

	return v_init_offset;
}

#if 0
static void sm5440_factory_preset_dc_work(struct work_struct *work)
{
	struct sm5440_charger *sm5440 = container_of(work, struct sm5440_charger,
			preset_dc_work.work);
	int adc_vbat, ret;
	u32 v_init_offset;

	dev_info(sm5440->dev, "DC_WORK:%s\n", __func__);

	ret = update_work_state(sm5440, SM5440_DC_PRESET);
	if (ret < 0) {
		return;
	}

	adc_vbat = sm5440_convert_adc(sm5440, SM5440_ADC_VBAT1);
	if (adc_vbat > sm5440->chg.vbat_reg) {
		dev_err(sm5440->dev, "DC_WORK:%s: adc_vbat(%dmV) > vbat_reg, can't start dc\n",
				__func__, adc_vbat);
		sm5440->chg.err = SM5440_ERR_INVAL_VBAT;
		sm5440_request_state_work(sm5440, SM5440_DC_ERR, DELAY_NONE);
		return;
	}

	sm5440->ta.pps_c = sm5440_pps_c(MIN(sm5440->ta.pps_c_max, sm5440->target_ibus) / 2);
	if (sm5440->ta.pps_c < sm5440->pdata->ta_min_current) {
		dev_info(sm5440->dev, "DC_WORK:%s: sm5440_pps_c(%dmA) less then ta_c_min\n", __func__, sm5440->ta.pps_c);
		sm5440->ta.pps_c = sm5440->pdata->ta_min_current;
	}
	v_init_offset = get_v_init_offset(sm5440);
	sm5440->ta.pps_v = sm5440_pps_v(MAX(2 * adc_vbat + v_init_offset, sm5440->pdata->ta_min_voltage));
	ret = sm5440_send_pd_message(sm5440);
	if (ret < 0) {
		return;
	}
	msleep(DELAY_PPS_UPDATE);

	sm5440_setup_pps_work_charging_config(sm5440);
	sm5440_enable_charging(sm5440, 1);

	dev_info(sm5440->dev, "DC_WORK:%s: enable Direct-charging\n", __func__);

	sm5440_print_regmap(sm5440);

	sm5440->ta.pps_v = (sm5440->target_vbat * 2) + get_pps_cl_v_offset(sm5440, sm5440->chg.ci_gl);
	sm5440->ta.pps_v = sm5440_pps_v(MIN(sm5440->ta.pps_v, SM5440_VBUS_OVP_TH - 500));
	sm5440->ta.pps_c = sm5440_pps_c(((sm5440->chg.ci_gl * 90) / 100) - (PPS_C_STEP * 2));
	sm5440->wq.pps_cl = 1;

	ret = sm5440_send_pd_message(sm5440);
	if (ret < 0) {
		return;
	}
	msleep(DELAY_PPS_UPDATE);

	/* Pre-update PRE_CC state. for check to charging initial error case */
	ret = update_work_state(sm5440, SM5440_DC_PRE_CC);
	if (ret < 0) {
		return;
	}

	dev_info(sm5440->dev, "DC_WORK:%s: request: preset -> pre_cc\n", __func__);

	sm5440_request_state_work(sm5440, SM5440_DC_PRE_CC, DELAY_PPS_UPDATE);
}
#else
static int adjust_pps_v(struct sm5440_charger *sm5440, int pps_v_original)
{
	int i, adc_vbus, ret;

	adc_vbus = sm5440_convert_adc(sm5440, SM5440_ADC_VBUS);
	dev_info(sm5440->dev, "DC_WORK:%s: adc_vbus=%dmV, pps_v_original=%dmV\n",
			__func__, adc_vbus, pps_v_original);

	if (adc_vbus >= pps_v_original - PPS_V_STEP) {
		/* case ADC_VBUS higher than PPS_V (adjustment max offset = 100mV) */
		sm5440->wq.pps_v_offset = MIN((((adc_vbus - pps_v_original) / PPS_V_STEP) * PPS_V_STEP), 100) * (-1);
	} else {
		/* case ADC_VBUS lower than PPS_V (adjustment max offset = 100mV) */
		for (i = 0; i < 0xff; ++i) {
			if (adc_vbus >= pps_v_original - PPS_V_STEP) {
				break;
			}
			sm5440->ta.pps_v += PPS_V_STEP;
			ret = sm5440_send_pd_message(sm5440);
			if (ret < 0) {
				return -1;
			}
			msleep(DELAY_PPS_UPDATE);
			adc_vbus = sm5440_convert_adc(sm5440, SM5440_ADC_VBUS);
			dev_info(sm5440->dev, "DC_WORK:%s: adc_vbus=%dmV, pps_v_original=%dmV\n",
					__func__, adc_vbus, pps_v_original);
		}
		if (i == 0xff) {
			dev_err(sm5440->dev, "DC_WORK:%s: fail to adjustment\n", __func__);
			sm5440->chg.err = SM5440_ERR_FAIL_ADJUST;
			sm5440_request_state_work(sm5440, SM5440_DC_ERR, DELAY_NONE);
			return -1;
		}
		sm5440->wq.pps_v_offset = MIN((sm5440->ta.pps_v - pps_v_original), 100);
	}

	dev_info(sm5440->dev, "DC_WORK:%s: pps_v_offset=%dmV\n", __func__, sm5440->wq.pps_v_offset);
	return 0;
}

static void sm5440_preset_dc_work(struct work_struct *work)
{
	struct sm5440_charger *sm5440 = container_of(work, struct sm5440_charger,
				preset_dc_work.work);
	int adc_vbat, ret, delay = DELAY_PPS_UPDATE;
	u32 v_init_offset;

	dev_info(sm5440->dev, "DC_WORK:%s\n", __func__);

	ret = update_work_state(sm5440, SM5440_DC_PRESET);
	if (ret < 0) {
		return;
	}

	adc_vbat = sm5440_convert_adc(sm5440, SM5440_ADC_VBAT1);
	if (adc_vbat > sm5440->chg.vbat_reg + PPS_V_STEP) {     /* Debounce ADC accuracy */
		dev_err(sm5440->dev, "DC_WORK:%s: adc_vbat(%dmV) > vbat_reg, can't start dc\n",
				__func__, adc_vbat);
		sm5440->chg.err = SM5440_ERR_INVAL_VBAT;
		sm5440_request_state_work(sm5440, SM5440_DC_ERR, DELAY_NONE);
		return;
	}

	sm5440->ta.pps_v_max_1 = sm5440->ta.pps_p_max / sm5440->ta.pps_c_max;
	dev_info(sm5440->dev, "DC_WORK:%s: adc_vbat=%dmV, ta_min_v=%dmV, pps_v_max_1=%dmA, pps_c_max=%dmA, target_ibus=%dmA\n",
			 __func__, adc_vbat, sm5440->pdata->ta_min_voltage, sm5440->ta.pps_v_max_1,
			 sm5440->ta.pps_c_max, sm5440->target_ibus);

	sm5440->ta.pps_c = sm5440_pps_c(MIN(sm5440->ta.pps_c_max, ((sm5440->target_ibus * 50) / 100)));
	if (sm5440->ta.pps_c < sm5440->pdata->ta_min_current) {
		dev_info(sm5440->dev, "DC_WORK:%s: sm5440_pps_c(%dmA) less then ta_c_min\n", __func__, sm5440->ta.pps_c);
		sm5440->ta.pps_c = sm5440->pdata->ta_min_current;
	}
	v_init_offset = get_v_init_offset(sm5440);
	sm5440->ta.pps_v = sm5440_pps_v(MAX(2 * adc_vbat + v_init_offset, sm5440->pdata->ta_min_voltage));
	ret = sm5440_send_pd_message(sm5440);
	if (ret < 0) {
		return;
	}
	msleep(DELAY_PPS_UPDATE);

	if (sm5440_get_op_mode(sm5440) == 0x0) {
		ret = adjust_pps_v(sm5440, sm5440->ta.pps_v);
		if (ret < 0) {
			return;
		}
	}
	if (sm5440->target_ibus < sm5440->chg.ci_gl) {
		delay = DELAY_ADC_UPDATE;
	}

	sm5440_setup_pps_work_charging_config(sm5440);
	sm5440_enable_charging(sm5440, 1);
	/* Pre-update PRE_CC state. for check to charging initial error case */
	ret = update_work_state(sm5440, SM5440_DC_PRE_CC);
	if (ret < 0) {
		return;
	}
	dev_info(sm5440->dev, "DC_WORK:%s: enable Direct-charging\n", __func__);

	sm5440_print_regmap(sm5440);

	dev_info(sm5440->dev, "DC_WORK:%s: request: preset -> pre_cc\n", __func__);

	sm5440_request_state_work(sm5440, SM5440_DC_PRE_CC, delay);
}
#endif

static int sm5440_pre_cc_check_limitation(struct sm5440_charger *sm5440, int adc_ibus, int adc_vbus)
{
	u32 calc_pps_v, calc_reg_v;
	int ret = 0;

	if (adc_ibus == sm5440->wq.prev_adc_ibus && adc_vbus == sm5440->wq.prev_adc_vbus) {
		dev_info(sm5440->dev, "DC_WORK:%s: adc didn't update yet\n", __func__);
		/* if continuos adc didn't update yet */
		return 0;
	}

	if ((adc_ibus > sm5440->ta.pps_c - PPS_C_STEP) &&
		(adc_vbus < sm5440->wq.prev_adc_vbus + (PPS_V_STEP/2)) &&
		(adc_ibus < sm5440->wq.prev_adc_ibus + (PPS_C_STEP)) &&
		(sm5440->wq.pps_c_down == 0 && sm5440->wq.pps_cl == 0)) {
		ret = 1;
	} else if (sm5440->chg.ci_gl == sm5440->pdata->ta_min_current) {     /* Case: didn't reduce PPS_C than TA_MIN_C */
		ret = 1;
	}

	if (ret) {
		calc_reg_v = get_pps_cl_v_offset(sm5440, sm5440->chg.ci_gl);
		calc_pps_v = (sm5440->target_vbat * 2) + calc_reg_v + sm5440->wq.pps_v_offset;
		sm5440->ta.pps_v = sm5440_pps_v(MIN(calc_pps_v, SM5440_VBUS_OVP_TH - 500));
		dev_info(sm5440->dev, "DC_WORK:%s: PPS_LR=%d, RPARA=%d, RSNS=%d, RPCM=%d, calc_reg_v=%dmV, calc_pps_v=%dmV\n",
					__func__, sm5440->pdata->pps_lr, sm5440->pdata->rpara, sm5440->pdata->rsns,
					sm5440->pdata->rpcm, calc_reg_v, calc_pps_v);
		sm5440->ta.pps_c = sm5440->ta.pps_c;
		sm5440->wq.pps_cl = 1;
	}

	return ret;
}

static void sm5440_pre_cc_work(struct work_struct *work)
{
	struct sm5440_charger *sm5440 = container_of(work, struct sm5440_charger,
			pre_cc_work.work);
	int ret, adc_ibus, adc_vbus, adc_vbat;
	int delay_time = DELAY_PPS_UPDATE;
	u8 loop_status;

	dev_info(sm5440->dev, "DC_WORK:%s (CI_GL=%dmA)\n", __func__, sm5440->chg.ci_gl);
	ret = sm5440_check_error_state(sm5440, SM5440_DC_PRE_CC);
	if (ret < 0) {
		return;
	}
	ret = update_work_state(sm5440, SM5440_DC_PRE_CC);
	if (ret < 0) {
		return;
	}

	sm5440_get_adc_values(sm5440, "DC_WORK:sm5440_pre_cc_work adc", &adc_vbus,
			&adc_ibus, NULL, &adc_vbat, NULL, NULL);

	loop_status = check_loop_status(sm5440);
	switch (loop_status) {
		case LOOP_VBATREG:
			cv_cnt = 0;
			dev_info(sm5440->dev, "DC_WORK:%s: request: pre-cc -> cv\n", __func__);
			sm5440_setup_adc(sm5440, SM5440_ADC_MODE_CONTINUOUS);
			sm5440_request_state_work(sm5440, SM5440_DC_CV, DELAY_ADC_UPDATE);
			return;
		case LOOP_IBUSLIM:
#if APPLY_SW_OCP
			if (sm5440->wq.pps_c_down == 1) {
				ret = sm5440_check_ocp(sm5440);
				if (ret < 0) {
					return;
				}
			}
#endif
			sm5440->ta.pps_c = sm5440->ta.pps_c - PPS_C_STEP;
			ret = sm5440_send_pd_message(sm5440);
			if (ret < 0) {
				return;
			}
			sm5440->wq.pps_c_down = 1;
			sm5440->wq.pps_c_up = 0;
			sm5440_request_state_work(sm5440, SM5440_DC_PRE_CC, DELAY_PPS_UPDATE);
			return;
	}

	if (sm5440->wq.pps_c_down) {
		dev_info(sm5440->dev, "DC_WORK:%s: decrease CI_GL(%dmA to %dmA)\n",
				__func__, sm5440->chg.ci_gl, sm5440->chg.ci_gl - PPS_C_STEP);
		sm5440->chg.ci_gl = sm5440->chg.ci_gl - PPS_C_STEP;
		sm5440->wq.pps_c_down = 0;
		sm5440_request_state_work(sm5440, SM5440_DC_PRE_CC, DELAY_PPS_UPDATE);
		return;
	}

	if (sm5440_pre_cc_check_limitation(sm5440, adc_ibus, adc_vbus)) {
		ret = sm5440_send_pd_message(sm5440);
		if (ret < 0) {
			return;
		}
	}

	if (adc_ibus > sm5440->chg.ci_gl - (PPS_C_STEP * 80 / 100)) {
		cc_cnt = 0;
		if (sm5440->chg.ci_gl > sm5440->ta.pps_c) {
			sm5440->wq.pps_c_offset = sm5440->chg.ci_gl - sm5440->ta.pps_c;
		} else {
			sm5440->wq.pps_c_offset = 0;
		}
		dev_info(sm5440->dev, "DC_WORK:%s: request: pre_cc -> cc (pps_c_offset=%d, pps_cl=%d)\n",
				__func__, sm5440->wq.pps_c_offset, sm5440->wq.pps_cl);
		sm5440_setup_adc(sm5440, SM5440_ADC_MODE_CONTINUOUS);
		sm5440_request_state_work(sm5440, SM5440_DC_CC, DELAY_ADC_UPDATE);
		return;
	}

	if ((sm5440->wq.pps_cl) &&
		(sm5440->ta.pps_v * (sm5440->ta.pps_c + PPS_C_STEP) >= sm5440->ta.pps_p_max)) {
		sm5440->wq.pps_c_offset = 0;
		cc_cnt = 0;
		dev_info(sm5440->dev, "DC_WORK:%s: request: pre_cc -> cc\n", __func__);
		sm5440_setup_adc(sm5440, SM5440_ADC_MODE_CONTINUOUS);
		sm5440_request_state_work(sm5440, SM5440_DC_CC, DELAY_ADC_UPDATE);
		return;
	}

	if (sm5440->wq.pps_cl) {
		if ((adc_ibus < sm5440->chg.ci_gl - (PPS_C_STEP * 6)) &&
				(sm5440->ta.pps_c < ((sm5440->chg.ci_gl * 90)/100))) {
			sm5440->ta.pps_c += (PPS_C_STEP * 3);
		} else {
			sm5440->ta.pps_c += PPS_C_STEP;
		}
		ret = sm5440_send_pd_message(sm5440);
		if (ret < 0) {
			return;
		}
		sm5440->wq.pps_c_up = 1;
		sm5440->wq.pps_c_down = 0;
	} else {
		sm5440->ta.pps_v = sm5440->ta.pps_v + PPS_V_STEP;
		ret = sm5440_send_pd_message(sm5440);
		if (ret < 0) {
			return;
		}
		sm5440->wq.pps_v_up = 1;
		sm5440->wq.pps_v_down = 0;
	}

	sm5440->wq.prev_adc_vbus = adc_vbus;
	sm5440->wq.prev_adc_ibus = adc_ibus;
	sm5440_request_state_work(sm5440, SM5440_DC_PRE_CC, delay_time);
}

static void sm5440_cc_work(struct work_struct *work)
{
	struct sm5440_charger *sm5440 = container_of(work, struct sm5440_charger,
			cc_work.work);
	int ret, adc_ibus, adc_vbus, adc_vbat;
	u8 loop_status;

	dev_info(sm5440->dev, "DC_WORK:%s\n", __func__);
	ret = sm5440_check_error_state(sm5440, SM5440_DC_CC);
	if (ret < 0) {
		return;
	}
	ret = update_work_state(sm5440, SM5440_DC_CC);
	if (ret < 0) {
		return;
	}

	sm5440_get_adc_values(sm5440, "DC_WORK:sm5440_cc_work adc", &adc_vbus,
			&adc_ibus, NULL, &adc_vbat, NULL, NULL);

	loop_status = check_loop_status(sm5440);
	if (loop_status & LOOP_VBATREG) {
		cv_cnt = 0;
		dev_info(sm5440->dev, "DC_WORK:%s: request: cc -> cv\n", __func__);
		sm5440_request_state_work(sm5440, SM5440_DC_CV, DELAY_ADC_UPDATE);
		return;
	}
#if APPLY_SW_OCP
	if (loop_status & LOOP_IBUSLIM) {
		ret = sm5440_check_ocp(sm5440);
		if (ret < 0) {
			return;
		}
	}
#endif

	if (adc_ibus >= sm5440->chg.ci_gl - (PPS_C_STEP * 2)) {
#if defined(SUPPORT_PPS_REMAIN)
		ret = sm5440_send_pd_message(sm5440);
		if (ret < 0) {
			return;
		}
#endif
		sm5440_request_state_work(sm5440, SM5440_DC_CC, DELAY_CHG_LOOP);
		return;
	}

	if ((sm5440->ta.pps_v + (PPS_V_STEP * 2)) * sm5440->ta.pps_c >= sm5440->ta.pps_p_max) {
		sm5440->ta.pps_c -= PPS_C_STEP;
		sm5440->ta.pps_v += PPS_V_STEP * 2;
	} else {
		cc_cnt++;
		if (cc_cnt % 2) {
			if (sm5440->ta.pps_v + (PPS_V_STEP * 2) <= SM5440_VBUS_OVP_TH - 500) {
				sm5440->ta.pps_v += PPS_V_STEP * 2;
			} else {
				goto CC_PPS_C_SHIFT;
			}
		} else {
CC_PPS_C_SHIFT:
			if (sm5440->ta.pps_c + PPS_C_STEP <= sm5440->target_ibus) {
				sm5440->ta.pps_c += PPS_C_STEP;
			}
		}
	}
	dev_info(sm5440->dev, "DC_WORK:%s: Changed PPS Msg\n", __func__);
	ret = sm5440_send_pd_message(sm5440);
	if (ret < 0) {
		return;
	}
	sm5440_request_state_work(sm5440, SM5440_DC_CC, DELAY_ADC_UPDATE);
}

static void sm5440_cv_work(struct work_struct *work)
{
	struct sm5440_charger *sm5440 = container_of(work, struct sm5440_charger,
			cv_work.work);
	int ret, adc_ibus, adc_vbus, adc_vbat, delay = DELAY_CHG_LOOP;
	u8 loop_status;

	dev_info(sm5440->dev, "DC_WORK:%s\n", __func__);
	ret = sm5440_check_error_state(sm5440, SM5440_DC_CV);
	if (ret < 0) {
		return;
	}
	ret = update_work_state(sm5440, SM5440_DC_CV);
	if (ret < 0) {
		return;
	}

	sm5440_get_adc_values(sm5440, "DC_WORK:sm5440_cv_work adc", &adc_vbus,
			&adc_ibus, NULL, &adc_vbat, NULL, NULL);

	loop_status = check_loop_status(sm5440);
	if ((cv_cnt == 0) && (loop_status & LOOP_VBATREG)) {
		cv_cnt = 1;
	} else if ((cv_cnt == 1) && (loop_status == LOOP_INACTIVE)) {
		cv_cnt = 2;
	}

	if (loop_status & LOOP_VBATREG) {
		if (cv_cnt == 1) {
			/* fast decrease PPS_V during on the first vbatreg loop */
			sm5440->ta.pps_v -= PPS_V_STEP * 2;
		} else {
			sm5440->ta.pps_v -= PPS_V_STEP;
		}
		ret = sm5440_send_pd_message(sm5440);
		if (ret < 0) {
			return;
		}
		delay = DELAY_PPS_UPDATE;
	}

	/* occurred abnormal CV status */
	if ((adc_vbat < sm5440->target_vbat - 100) ||
		(loop_status & LOOP_IBUSLIM)) {
		dev_info(sm5440->dev, "DC_WORK:%s: adnormal cv, request: cv -> pre_cc\n", __func__);
		sm5440_setup_adc(sm5440, SM5440_ADC_MODE_ONESHOT);
		sm5440_request_state_work(sm5440, SM5440_DC_PRE_CC, DELAY_ADC_UPDATE);
		return;
	}

#if defined(SUPPORT_PPS_REMAIN)
	if (loop_status == LOOP_INACTIVE) {
		ret = sm5440_send_pd_message(sm5440);
		if (ret < 0) {
			return;
		}
	}
#endif

	sm5440_request_state_work(sm5440, SM5440_DC_CV, delay);
}

static void sm5440_update_bat_work(struct work_struct *work)
{
	struct sm5440_charger *sm5440 = container_of(work, struct sm5440_charger,
			update_bat_work.work);
	int index, ret, cnt;
	bool need_to_preset = 1;

	dev_info(sm5440->dev, "DC_WORK:%s\n", __func__);

	ret = sm5440_check_error_state(sm5440, SM5440_DC_UPDAT_BAT);
	if (ret < 0) {
		return;
	}

	/* waiting for step change event */
	for (cnt = 0; cnt < 1; ++cnt) {
		if (sm5440->req_update_vbat && sm5440->req_update_ibus) {
			break;
		}
		dev_info(sm5440->dev, "%s: wait 1sec for step changed\n", __func__);
		msleep(1000);
	}

	mutex_lock(&sm5440->st_lock);
	index = (sm5440->req_update_vbat << 2) | (sm5440->req_update_ibus << 1) | sm5440->req_update_ibat;
	sm5440->req_update_vbat = 0;
	sm5440->req_update_ibus = 0;
	sm5440->req_update_ibat = 0;
	mutex_unlock(&sm5440->st_lock);

	ret = update_work_state(sm5440, SM5440_DC_UPDAT_BAT);
	if (ret < 0) {
		return;
	}

	if (index & (0x1 << 2)) {
		dev_info(sm5440->dev, "DC_WORK:%s: vbat changed (%dmV)\n",
				__func__, sm5440->target_vbat);
	}
	if (index & (0x1 << 1)) {
		dev_info(sm5440->dev, "DC_WORK:%s: ibus changed (%dmA)\n",
				__func__, sm5440->target_ibus);
	}
	if (index & 0x1) {
		dev_info(sm5440->dev, "DC_WORK:%s: ibat changed (%dmA)\n",
				__func__, sm5440->target_ibat);
	}

	/* check step change event */
	if ((index & (0x1 << 2)) && (index & (0x1 << 1))) {
		if ((sm5440->target_vbat > sm5440->chg.cv_gl) && (sm5440->target_ibus < sm5440->chg.ci_gl)) {
			need_to_preset = 0;
		}
	}

	sm5440_setup_adc(sm5440, SM5440_ADC_MODE_ONESHOT);

	if (need_to_preset) {
		dev_info(sm5440->dev, "DC_WORK:%s: request: update_bat -> preset\n", __func__);
		sm5440_request_state_work(sm5440, SM5440_DC_PRESET, DELAY_NONE);
	} else {
		sm5440_setup_pps_work_charging_config(sm5440);
		sm5440_print_regmap(sm5440);

		sm5440->ta.pps_c = sm5440->chg.ci_gl - 200 - sm5440->wq.pps_c_offset;
		ret = sm5440_send_pd_message(sm5440);
		if (ret < 0) {
			return;
		}
		dev_info(sm5440->dev, "DC_WORK:%s: request: update_bat -> pre_cc\n", __func__);
		sm5440_request_state_work(sm5440, SM5440_DC_PRE_CC, DELAY_ADC_UPDATE);
	}
}

static void sm5440_error_work(struct work_struct *work)
{
	struct sm5440_charger *sm5440 = container_of(work, struct sm5440_charger,
			error_work.work);
	int ret;

	dev_info(sm5440->dev, "DC_WORK: %s(err=0x%x)\n", __func__, sm5440->chg.err);

	ret = update_work_state(sm5440, SM5440_DC_ERR);
	if (ret < 0) {
		return;
	}
	sm5440_setup_adc(sm5440, SM5440_ADC_MODE_OFF);
	sm5440_enable_charging(sm5440, 0);
}

static void sm5440_adc_work(struct work_struct *work)
{
	struct sm5440_charger *sm5440 = container_of(work, struct sm5440_charger,
			adc_work.work);
	sm5440_enable_adc(sm5440, 1);

	schedule_delayed_work(&sm5440->adc_work, msecs_to_jiffies(200));
}

static int sm5440_irq_wdtmroff(struct sm5440_charger *sm5440)
{
	dev_info(sm5440->dev, "%s: WDT Expired!\n", __func__);

	return 0;
}

static int sm5440_irq_vbatreg(struct sm5440_charger *sm5440)
{
	if (sm5440->chg.state == SM5440_DC_CC) {
		if (delayed_work_pending(&sm5440->cc_work)) {
			cancel_delayed_work(&sm5440->cc_work);
			dev_info(sm5440->dev, "%s: cancel CC_work, direct request work\n", __func__);
			sm5440_request_state_work(sm5440, SM5440_DC_CC, DELAY_NONE);
		}
	}

	return 0;
}

static int sm5440_irq_vbus_uvlo(struct sm5440_charger *sm5440)
{
	if (sm5440->rev_boost) {
		/* for reverse-boost fault W/A. plz keep this code */
		sm5440_set_op_mode(sm5440, OP_MODE_CHG_OFF);
		msleep(10);
		sm5440_set_op_mode(sm5440, OP_MODE_REV_BOOST);
	}

	return 0;
}

static irqreturn_t sm5440_irq_thread(int irq, void *data)
{
	struct sm5440_charger *sm5440 = (struct sm5440_charger *)data;
	u8 reg_int[5], reg_st[5];
	u8 op_mode = sm5440_get_op_mode(sm5440);

	sm5440_bulk_read(sm5440, SM5440_REG_INT1, 4, reg_int);
	sm5440_bulk_read(sm5440, SM5440_REG_STATUS1, 4, reg_st);
	dev_info(sm5440->dev, "%s: INT[0x%x:0x%x:0x%x:0x%x] ST[0x%x:0x%x:0x%x:0x%x]\n",
			__func__, reg_int[0], reg_int[1], reg_int[2], reg_int[3],
			reg_st[0], reg_st[1], reg_st[2], reg_st[3]);

	/* check forced CUT-OFF status */
	if (sm5440->chg.state > SM5440_DC_PRESET && op_mode == 0x0) {
		sm5440->chg.err = (reg_int[0] & 0xF7) | (reg_int[2] & 0x87) << 8;
		dev_err(sm5440->dev, "%s: forced charge cut-off(err=0x%x)\n",
				__func__, sm5440->chg.err);

		sm5440_terminate_charging_work(sm5440);
		sm5440->chg.state = SM5440_DC_ERR;
		sm5440_report_dc_state(sm5440);
	}

	if (reg_int[1] & SM5440_INT2_VBATREG) {
		dev_info(sm5440->dev, "%s: VBATREG detected\n", __func__);
		sm5440_irq_vbatreg(sm5440);
	}
	if (reg_int[2] & SM5440_INT3_REVBLK) {
		dev_info(sm5440->dev, "%s: REVBLK detected\n", __func__);
	}
	if (reg_int[2] & SM5440_INT3_VBUSOVP) {
		dev_info(sm5440->dev, "%s: VBUS OVP detected\n", __func__);
	}
	if (reg_int[2] & SM5440_INT3_VBUSUVLO) {
		dev_info(sm5440->dev, "%s: VBUS UVLO detected\n", __func__);
		sm5440_irq_vbus_uvlo(sm5440);
	}
	if (reg_int[2] & SM5440_INT3_VBUSPOK) {
		dev_info(sm5440->dev, "%s: VBUS POK detected\n", __func__);
	}
	if (reg_int[3] & SM5440_INT4_WDTMROFF) {
		dev_info(sm5440->dev, "%s: WDTMROFF detected\n", __func__);
		sm5440_irq_wdtmroff(sm5440);
	}

	dev_info(sm5440->dev, "closed %s\n", __func__);

	return IRQ_HANDLED;
}

static int sm5440_irq_init(struct sm5440_charger *sm5440)
{
	int ret;

	sm5440->irq = gpio_to_irq(sm5440->pdata->irq_gpio);
	dev_info(sm5440->dev, "%s: irq_gpio=%d, irq=%d\n", __func__,
			sm5440->pdata->irq_gpio, sm5440->irq);

	ret = gpio_request(sm5440->pdata->irq_gpio, "sm5540_irq");
	if (ret) {
		dev_err(sm5440->dev, "%s: fail to request gpio(ret=%d)\n",
				__func__, ret);
		return ret;
	}
	gpio_direction_input(sm5440->pdata->irq_gpio);
	gpio_free(sm5440->pdata->irq_gpio);

	sm5440_write_reg(sm5440, SM5440_REG_MSK1, 0xC0);
	sm5440_write_reg(sm5440, SM5440_REG_MSK2, 0xF7);	/* used VBATREG */
	sm5440_write_reg(sm5440, SM5440_REG_MSK3, 0x18);
	sm5440_write_reg(sm5440, SM5440_REG_MSK4, 0xF8);

	ret = request_threaded_irq(sm5440->irq, NULL, sm5440_irq_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			"sm5440-irq", sm5440);
	if (ret) {
		dev_err(sm5440->dev, "%s: fail to request irq(ret=%d)\n",
				__func__, ret);
		return ret;
	}

	return 0;
}

static int sm5440_hw_init_config(struct sm5440_charger *sm5440)
{
	int ret;
	u8 reg;

	/* check to valid I2C transfer & register control */
	ret = sm5440_read_reg(sm5440, SM5440_REG_DEVICEID, &reg);
	if (ret < 0 || (reg & 0xF) != 0x1) {
		dev_err(sm5440->dev, "%s: device not found on this channel (reg=0x%x)\n",
				__func__, reg);
		return -ENODEV;
	}
	sm5440->pdata->rev_id = (reg >> 4) & 0xf;

	sm5440_set_wdt_timer(sm5440, WDT_TIMER_S_30);
	sm5440_set_freq(sm5440, sm5440->pdata->freq);

	sm5440_write_reg(sm5440, SM5440_REG_CNTL2, 0xF2);               /* disable IBUSOCP,IBATOCP,THEM */
	sm5440_write_reg(sm5440, SM5440_REG_CNTL3, 0xB8);			   /* disable CHGTMR */
	sm5440_write_reg(sm5440, SM5440_REG_CNTL4, 0xFF);			   /* used DEB:8ms */
	sm5440_write_reg(sm5440, SM5440_REG_CNTL6, 0x09);               /* forced PWM mode, disable ENHIZ */
	sm5440_update_reg(sm5440, SM5440_REG_VBUSCNTL, 0x7, 0x7, 0);	/* VBUS_OVP_TH=11V */
	sm5440_write_reg(sm5440, SM5440_REG_VOUTCNTL, 0x3F);			/* VOUT=max */
	sm5440_write_reg(sm5440, SM5440_REG_PRTNCNTL, 0xFE);			/* OCP,OVP setting */
	sm5440_update_reg(sm5440, SM5440_REG_ADCCNTL1, 1, 0x1, 3);	  /* ADC average sample = 32 */
	sm5440_write_reg(sm5440, SM5440_REG_ADCCNTL2, 0xDF);			/* ADC channel setting */

	sm5440->adc_mode = SM5440_ADC_MODE_OFF;

	return 0;
}

#if defined(CONFIG_OF)
static int sm5440_charger_parse_dt(struct device *dev,
		struct sm5440_platform_data *pdata)
{
	struct device_node *np_sm5440 = dev->of_node;
	struct device_node *np_battery;
	int ret;

	/* Parse: sm5440 node */
	if(!np_sm5440) {
		dev_err(dev, "%s: empty of_node for sm5440_dev\n", __func__);
		return -EINVAL;
	}
	pdata->irq_gpio = of_get_named_gpio(np_sm5440, "sm5440,irq-gpio", 0);

	ret = of_property_read_u32(np_sm5440, "sm5440,ta_min_curr",
			&pdata->ta_min_current);
	if (ret) {
		dev_info(dev, "%s: sm5440,ta_min_curr is Empty or lower\n", __func__);
		pdata->ta_min_current = 1000;
	}
	ret = of_property_read_u32(np_sm5440, "sm5440,ta_min_voltage",
			&pdata->ta_min_voltage);
	if (ret) {
		dev_info(dev, "%s: sm5440,ta_min_voltage is Empty\n", __func__);
		pdata->ta_min_voltage = 8000;
	}

	ret = of_property_read_u32(np_sm5440, "sm5440,dc_min_vbat",
			&pdata->dc_min_vbat);
	if (ret) {
		dev_info(dev, "%s: sm5440,dc_min_vbat is Empty\n", __func__);
		pdata->dc_min_vbat = 3500;
	}
	dev_info(dev, "parse_dt: irq_gpio=%d ta_min_c=%d, ta_min_v=%d, dc_min_vbat=%d\n",
			pdata->irq_gpio, pdata->ta_min_current,
			pdata->ta_min_voltage, pdata->dc_min_vbat);

	/* for calculate board registance */
	ret = of_property_read_u32(np_sm5440, "sm5440,PPS_LR",
			&pdata->pps_lr);
	if (ret) {
		dev_info(dev, "%s: sm5440,PPS_LR is Empty\n", __func__);
		pdata->pps_lr = 300000;
	}
	ret = of_property_read_u32(np_sm5440, "sm5440,RPARA",
			&pdata->rpara);
	if (ret) {
		dev_info(dev, "%s: sm5440,RPARA is Empty\n", __func__);
		pdata->rpara = 100000;
	}
	ret = of_property_read_u32(np_sm5440, "sm5440,RSNS",
			&pdata->rsns);
	if (ret) {
		dev_info(dev, "%s: sm5440,RSNS is Empty\n", __func__);
		pdata->rsns = 5000;
	}
	ret = of_property_read_u32(np_sm5440, "sm5440,RPCM",
			&pdata->rpcm);
	if (ret) {
		dev_info(dev, "%s: sm5440,RPCM is Empty\n", __func__);
		pdata->rpcm = 70000;
	}
	dev_info(dev, "parse_dt: PPS_LR=%d, RPARA=%d, RSNS=%d, RPCM=%d\n",
			pdata->pps_lr, pdata->rpara,
			pdata->rsns, pdata->rpcm);

	ret = of_property_read_u32(np_sm5440, "sm5440,cv_gl_offset",
			&pdata->cv_gl_offset);
	if (ret) {
		dev_info(dev, "%s: sm5440,cv_gl_offset is Empty\n", __func__);
		pdata->cv_gl_offset = 0;
	}
	ret = of_property_read_u32(np_sm5440, "sm5440,ci_gl_offset",
			&pdata->ci_gl_offset);
	if (ret) {
		dev_info(dev, "%s: sm5440,ci_gl_offset is Empty\n", __func__);
		pdata->ci_gl_offset = 150;
	}
	ret = of_property_read_u32(np_sm5440, "sm5440,cc_gl_offset",
			&pdata->cc_gl_offset);
	if (ret) {
		dev_info(dev, "%s: sm5440,cc_gl_offset is Empty\n", __func__);
		pdata->cc_gl_offset = 300;
	}
	dev_info(dev, "parse_dt: cv_gl_offset=%d, ci_gl_offset=%d, cc_gl_offset=%d\n",
			pdata->cv_gl_offset, pdata->ci_gl_offset, pdata->cc_gl_offset);

	ret = of_property_read_u32(np_sm5440, "sm5440,freq", &pdata->freq);
	if (ret) {
		dev_info(dev, "%s: sm5440,freq is Empty\n", __func__);
		pdata->freq = 1100;
	}
	dev_info(dev, "parse_dt: freq=%d\n", pdata->freq);

	/* Parse: battery node */
	np_battery = of_find_node_by_name(NULL, "battery");
	if(!np_battery) {
		dev_err(dev, "%s: empty of_node for battery\n", __func__);
		return -EINVAL;
	}
	ret = of_property_read_u32(np_battery, "battery,chg_float_voltage",
			&pdata->battery.chg_float_voltage);
	if (ret) {
		dev_info(dev, "%s: battery,chg_float_voltage is Empty\n", __func__);
		pdata->battery.chg_float_voltage = 4200;
	}
	ret = of_property_read_string(np_battery, "battery,charger_name",
			(char const **)&pdata->battery.sec_dc_name);
	if (ret) {
		dev_info(dev, "%s: battery,charger_name is Empty\n", __func__);
		pdata->battery.sec_dc_name = "sec-direct-charger";
	}

	dev_info(dev, "parse_dt: float_v=%d, sec_dc_name=%s\n",
			pdata->battery.chg_float_voltage,
			pdata->battery.sec_dc_name);

	return 0;
}
#endif  /* CONFIG_OF */

static int sm5440_dbg_read_reg(void *data, u64 *val)
{
	struct sm5440_charger *sm5440 = data;
	int ret;
	u8 reg;

	ret = sm5440_read_reg(sm5440, sm5440->debug_address, &reg);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: failed read 0x%02x\n",
				__func__, sm5440->debug_address);
		return ret;
	}
	*val = reg;

	return 0;
}

static int sm5440_dbg_write_reg(void *data, u64 val)
{
	struct sm5440_charger *sm5440 = data;
	int ret;

	ret = sm5440_write_reg(sm5440, sm5440->debug_address, (u8)val);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: failed write 0x%02x to 0x%02x\n",
				__func__, (u8)val, sm5440->debug_address);
		return ret;
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(register_debug_ops, sm5440_dbg_read_reg,
		sm5440_dbg_write_reg, "0x%02llx\n");

static int sm5440_create_debugfs_entries(struct sm5440_charger *sm5440)
{
	struct dentry *ent;

	sm5440->debug_root = debugfs_create_dir("charger-sm5440", NULL);
	if (!sm5440->debug_root) {
		dev_err(sm5440->dev, "%s: can't create dir\n", __func__);
		return -ENOENT;
	}

	ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
			sm5440->debug_root, &(sm5440->debug_address));
	if (!ent) {
		dev_err(sm5440->dev, "%s: can't create address\n", __func__);
		return -ENOENT;
	}

	ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
			sm5440->debug_root, sm5440, &register_debug_ops);
	if (!ent) {
		dev_err(sm5440->dev, "%s: can't create data\n", __func__);
		return -ENOENT;
	}

	return 0;
}

static int sm5440_charger_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct sm5440_charger *sm5440;
	struct sm5440_platform_data *pdata;
	struct power_supply_config psy_cfg = {};
	int ret;

	dev_info(&i2c->dev,"%s: probe start\n", __func__);

    sm5440 = devm_kzalloc(&i2c->dev, sizeof(struct sm5440_charger), GFP_KERNEL);
    if (!sm5440) {
        dev_err(&i2c->dev, "%s: can't allocate devmem for sm5540\n", __func__);
        return -ENOMEM;
    }


#if defined(CONFIG_OF)
    pdata = devm_kzalloc(&i2c->dev, sizeof(struct sm5440_platform_data), GFP_KERNEL);
    if (!pdata) {
        dev_err(&i2c->dev, "%s: can't allocate devmem for platform_data\n", __func__);
        kfree(sm5440);
        return -ENOMEM;
    }

	ret = sm5440_charger_parse_dt(&i2c->dev, pdata);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: fail to parse_dt\n", __func__);
		kfree(pdata);
		pdata = NULL;
	}
#else   /* CONFIG_OF */
	pdata = NULL;
#endif  /* CONFIG_OF */
	if (!pdata) {
		dev_err(&i2c->dev, "%s: we didn't support fixed platform_data yet\n", __func__);
		kfree(sm5440);
		return -EINVAL;
	}

	sm5440->dev = &i2c->dev;
	sm5440->i2c = i2c;
	sm5440->pdata = pdata;
	mutex_init(&sm5440->st_lock);
	mutex_init(&sm5440->pd_lock);
	mutex_init(&sm5440->i2c_lock);
	wakeup_source_init(&sm5440->wake_lock, "sm5440-charger");
	i2c_set_clientdata(i2c, sm5440);

	ret = sm5440_hw_init_config(sm5440);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: fail to init confg(ret=%d)\n", __func__, ret);
		goto err_devmem;
	}

	/* create work queue */
	sm5440->chg.state = SM5440_DC_CHG_OFF;
	sm5440->dc_wqueue = create_singlethread_workqueue(dev_name(sm5440->dev));
	if (!sm5440->dc_wqueue) {
		dev_err(sm5440->dev, "%s: fail to crearte workqueue\n", __func__);
		goto err_devmem;
	}
	INIT_DELAYED_WORK(&sm5440->check_vbat_work, sm5440_check_vbat_work);
#if 0
	INIT_DELAYED_WORK(&sm5440->preset_dc_work, sm5440_factory_preset_dc_work);
#else
	INIT_DELAYED_WORK(&sm5440->preset_dc_work, sm5440_preset_dc_work);
#endif
	INIT_DELAYED_WORK(&sm5440->pre_cc_work, sm5440_pre_cc_work);
	INIT_DELAYED_WORK(&sm5440->cc_work, sm5440_cc_work);
	INIT_DELAYED_WORK(&sm5440->cv_work, sm5440_cv_work);

	INIT_DELAYED_WORK(&sm5440->update_bat_work, sm5440_update_bat_work);
	INIT_DELAYED_WORK(&sm5440->error_work, sm5440_error_work);

	INIT_DELAYED_WORK(&sm5440->adc_work, sm5440_adc_work);

	psy_cfg.drv_data = sm5440;
	psy_cfg.supplied_to = sm5440_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sm5440_supplied_to);

	sm5440->psy_chg = power_supply_register(sm5440->dev,
			&sm5440_charger_power_supply_desc, &psy_cfg);
	if (IS_ERR(sm5440->psy_chg)) {
		dev_err(sm5440->dev, "%s: fail to register psy_chg\n", __func__);
		ret = PTR_ERR(sm5440->psy_chg);
		goto err_workqueue;
	}

	if (sm5440->pdata->irq_gpio) {
		ret = sm5440_irq_init(sm5440);
		if (ret < 0) {
			dev_err(sm5440->dev, "%s: fail to init irq(ret=%d)\n", __func__, ret);
			goto err_psy_chg;
		}
	} else {
		dev_warn(sm5440->dev, "%s: didn't assigned irq_gpio\n", __func__);
	}

	ret = sm5440_create_debugfs_entries(sm5440);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: fail to create debugfs(ret=%d)\n", __func__, ret);
		goto err_psy_chg;
	}

	dev_info(sm5440->dev, "%s: done. (rev_id=0x%x)\n", __func__, sm5440->pdata->rev_id);

	return 0;

err_psy_chg:
	power_supply_unregister(sm5440->psy_chg);

err_workqueue:
	destroy_workqueue(sm5440->dc_wqueue);

err_devmem:
	mutex_destroy(&sm5440->i2c_lock);
	mutex_destroy(&sm5440->st_lock);
	mutex_destroy(&sm5440->pd_lock);
	wakeup_source_trash(&sm5440->wake_lock);
	kfree(pdata);
	kfree(sm5440);

	return ret;
}

static int sm5440_charger_remove(struct i2c_client *i2c)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);

    sm5440_stop_charging(sm5440);

    power_supply_unregister(sm5440->psy_chg);
    destroy_workqueue(sm5440->dc_wqueue);

    mutex_destroy(&sm5440->i2c_lock);
    mutex_destroy(&sm5440->st_lock);
    mutex_destroy(&sm5440->pd_lock);
    wakeup_source_trash(&sm5440->wake_lock);
    kfree(sm5440->pdata);
    kfree(sm5440);

	return 0;
}

static void sm5440_charger_shutdown(struct i2c_client *i2c)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);

	sm5440_stop_charging(sm5440);
}

static const struct i2c_device_id sm5440_charger_id_table[] = {
	{ "sm5440-charger", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sm5440_charger_id_table);

#if defined(CONFIG_OF)
static struct of_device_id sm5440_of_match_table[] = {
	{ .compatible = "siliconmitus,sm5440" },
	{ },
};
MODULE_DEVICE_TABLE(of, sm5440_of_match_table);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int sm5440_charger_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		enable_irq_wake(sm5440->irq);

	disable_irq(sm5440->irq);

	return 0;
}

static int sm5440_charger_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		disable_irq_wake(sm5440->irq);

	enable_irq(sm5440->irq);

	return 0;
}
#else   /* CONFIG_PM */
#define sm5440_charger_suspend		NULL
#define sm5440_charger_resume		NULL
#endif  /* CONFIG_PM */

const struct dev_pm_ops sm5440_pm_ops = {
	.suspend = sm5440_charger_suspend,
	.resume = sm5440_charger_resume,
};

static struct i2c_driver sm5440_charger_driver = {
	.driver = {
		.name = "sm5440-charger",
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = sm5440_of_match_table,
#endif  /* CONFIG_OF */
#if defined(CONFIG_PM)
		.pm = &sm5440_pm_ops,
#endif  /* CONFIG_PM */
	},
	.probe		= sm5440_charger_probe,
	.remove	   = sm5440_charger_remove,
	.shutdown	 = sm5440_charger_shutdown,
	.id_table	 = sm5440_charger_id_table,
};

static int __init sm5440_i2c_init(void)
{
	pr_info("sm5440-charger: %s\n", __func__);
	return i2c_add_driver(&sm5440_charger_driver);
}
module_init(sm5440_i2c_init);

static void __exit sm5440_i2c_exit(void)
{
	i2c_del_driver(&sm5440_charger_driver);
}
module_exit(sm5440_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SiliconMitus <sungdae.choi@siliconmitus.com>");
MODULE_DESCRIPTION("Charger driver for SM5440");
