/*
 * haptic motor driver for s2mu106 - s2mu106_haptic.c
 *
 * Copyright (C) 2018 Suji Lee <suji0908.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "[VIB] " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/samsung/s2mu106.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/vibrator/sec_vibrator.h>
#include <linux/vibrator/slsi/s2mu106/s2mu106_vibrator.h>

#define S2MU106_DIVIDER		128
#define S2MU106_FREQ_DIVIDER	NSEC_PER_SEC / S2MU106_DIVIDER * 10

struct s2mu106_haptic_data {
	struct s2mu106_dev *s2mu106;
	struct i2c_client *i2c;
	struct s2mu106_haptic_platform_data *pdata;
	struct sec_vibrator_drvdata sec_vib_ddata;
	int duty_ratio;
	int max_duty;
	int duty;
	int period;
};

static void s2mu106_set_boost_voltage(struct s2mu106_haptic_data *haptic, int voltage)
{
	u8 data;
	if (voltage <= 3150)
		data = 0x00;
	else if (voltage > 3150 && voltage <= 6300)
		data = (voltage - 3150) / 50;
	else
		data = 0xFF;
	pr_info("%s: boost voltage %d, 0x%02x\n", __func__, voltage, data);

	s2mu106_update_reg(haptic->i2c, S2MU106_REG_HBST_CTRL1,
				data, HAPTIC_BOOST_VOLTAGE_MASK);
}

static int s2mu106_haptic_set_freq(struct device *dev, int num)
{
	struct s2mu106_haptic_data *haptic = dev_get_drvdata(dev);

	if (num >= 0 && num < haptic->pdata->freq_nums) {
		haptic->period =
			S2MU106_FREQ_DIVIDER / haptic->pdata->freq_array[num];
		haptic->duty = haptic->max_duty =
			(haptic->period * haptic->duty_ratio) / 100;
	} else if (num >= HAPTIC_ENGINE_FREQ_MIN &&
			num <= HAPTIC_ENGINE_FREQ_MAX) {
		haptic->period = S2MU106_FREQ_DIVIDER / num;
		haptic->duty = haptic->max_duty =
			(haptic->period * haptic->duty_ratio) / 100;
	} else {
		pr_err("%s out of range %d\n", __func__, num);
		return -EINVAL;
	}

	return 0;
}

static int s2mu106_haptic_set_intensity(struct device *dev, int intensity)
{
	struct s2mu106_haptic_data *haptic = dev_get_drvdata(dev);
	int data = 0x3FFFF;
	int max = 0x7FFFF;
	u8 val1, val2, val3;

	if (intensity < 0 || MAX_INTENSITY < intensity) {
		pr_err("%s out of range %d\n", __func__, intensity);
		return -EINVAL;
	}

	if (intensity == MAX_INTENSITY)
		data = max;
	else if (intensity != 0)
		data = max * intensity / MAX_INTENSITY;
	else
		data = 0;

	data &= 0x7FFFF;
	val1 = data & 0x0000F;
	val2 = (data & 0x00FF0) >> 4;
	val3 = (data & 0x7F000) >> 12;

	s2mu106_update_reg(haptic->i2c, S2MU106_REG_AMPCOEF1, val3, 0x7F);
	s2mu106_write_reg(haptic->i2c, S2MU106_REG_AMPCOEF2, val2);
	s2mu106_update_reg(haptic->i2c, S2MU106_REG_AMPCOEF3, val1 << 4, 0xF0);

	return 0;
}

static void s2mu106_haptic_onoff(struct s2mu106_haptic_data *haptic, bool en)
{
	if (en) {
		pr_info("Motor Enable\n");

		switch (haptic->pdata->hap_mode) {
		case S2MU106_HAPTIC_LRA:
			s2mu106_write_reg(haptic->i2c, S2MU106_REG_HAPTIC_MODE,
					LRA_MODE_EN);
			if (haptic->pdata->hbst.en) {
				s2mu106_update_reg(haptic->i2c,
						S2MU106_REG_HBST_CTRL0,
		          			SEL_HBST_HAPTIC_MASK,
						SEL_HBST_HAPTIC_MASK);
				s2mu106_update_reg(haptic->i2c,
						S2MU106_REG_OV_BK_OPTION,
						0, LRA_BST_MODE_SET_MASK);
			}
			pwm_config(haptic->pdata->pwm, haptic->max_duty,
					haptic->period);
			pwm_enable(haptic->pdata->pwm);
			break;
		case S2MU106_HAPTIC_ERM_GPIO:
			if (gpio_is_valid(haptic->pdata->motor_en))
				gpio_direction_output(haptic->pdata->motor_en,
						1);
			break;
		case S2MU106_HAPTIC_ERM_I2C:
			s2mu106_write_reg(haptic->i2c, S2MU106_REG_HAPTIC_MODE,
					ERM_MODE_ON);
			break;
		default:
			break;
		}
	} else {
		pr_info("Motor Disable\n");

		switch (haptic->pdata->hap_mode) {
		case S2MU106_HAPTIC_LRA:
			pwm_disable(haptic->pdata->pwm);
			s2mu106_write_reg(haptic->i2c, S2MU106_REG_HAPTIC_MODE,
					HAPTIC_MODE_OFF);
			if (haptic->pdata->hbst.en) {
				s2mu106_update_reg(haptic->i2c,
						S2MU106_REG_HBST_CTRL0,
						0, SEL_HBST_HAPTIC_MASK);
				s2mu106_update_reg(haptic->i2c,
						S2MU106_REG_OV_BK_OPTION,
						LRA_BST_MODE_SET_MASK,
						LRA_BST_MODE_SET_MASK);
			}
			break;
		case S2MU106_HAPTIC_ERM_GPIO:
			if (gpio_is_valid(haptic->pdata->motor_en))
				gpio_direction_output(haptic->pdata->motor_en,
						0);
			break;
		case S2MU106_HAPTIC_ERM_I2C:
			s2mu106_write_reg(haptic->i2c, S2MU106_REG_HAPTIC_MODE,
					HAPTIC_MODE_OFF);
			break;
		default:
			break;
		}
	}
}

static int s2mu106_haptic_enable(struct device *dev, bool en)
{
	struct s2mu106_haptic_data *haptic = dev_get_drvdata(dev);

	s2mu106_haptic_onoff(haptic, en);

	return 0;
}

static int s2mu106_haptic_set_overdrive(struct device *dev, bool en)
{
	struct s2mu106_haptic_data *haptic = dev_get_drvdata(dev);

	if (haptic->pdata->overdrive_ratio)
		haptic->duty_ratio = en ? haptic->pdata->overdrive_ratio :
			haptic->pdata->normal_ratio;

	return 0;
}

static int s2mu106_haptic_get_motor_type(struct device *dev, char *buf)
{
	struct s2mu106_haptic_data *haptic = dev_get_drvdata(dev);
	int ret = snprintf(buf, VIB_BUFSIZE, "%s\n", haptic->pdata->motor_type);

	return ret;
}

#if defined(CONFIG_OF)
static int s2mu106_haptic_parse_dt_lra(struct device *dev,
		struct device_node *np,
		struct s2mu106_haptic_platform_data *pdata)
{
	u32 temp = 0;
	int ret = 0, i = 0;

	ret = of_property_read_u32(np, "haptic,multi_frequency", &temp);
	if (ret) {
		pr_info("%s: multi_frequency isn't used\n", __func__);
		pdata->freq_nums = 0;
	} else
		pdata->freq_nums = (int)temp;

	if (pdata->freq_nums) {
		pdata->freq_array = devm_kzalloc(dev,
				sizeof(u32)*pdata->freq_nums, GFP_KERNEL);
		if (!pdata->freq_array) {
			pr_err("%s: failed to allocate freq_array data\n",
					__func__);
			goto err_parsing_dt;
		}

		ret = of_property_read_u32_array(np, "haptic,frequency",
				pdata->freq_array, pdata->freq_nums);
		if (ret) {
			pr_err("%s: error to get dt node frequency\n",
					__func__);
			goto err_parsing_dt;
		}

		pdata->frequency = pdata->freq_array[0];
	} else {
		ret = of_property_read_u32(np,
				"haptic,frequency", &temp);
		if (ret) {
			pr_err("%s: error to get dt node frequency\n",
					__func__);
			goto err_parsing_dt;
		} else
			pdata->frequency = (int)temp;
	}

	ret = of_property_read_u32(np,
			"haptic,normal_ratio", &temp);
	if (ret) {
		pr_err("%s: error to get dt node normal_ratio\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->normal_ratio = (int)temp;

	ret = of_property_read_u32(np,
			"haptic,overdrive_ratio", &temp);
	if (ret)
		pr_info("%s: overdrive_ratio isn't used\n", __func__);
	else
		pdata->overdrive_ratio = (int)temp;

	pdata->pwm = devm_of_pwm_get(dev, np, NULL);
	if (IS_ERR(pdata->pwm)) {
		pr_err("%s: error to get pwms\n", __func__);
		goto err_parsing_dt;
	}

	if (pdata->freq_nums) {
		pr_info("multi frequency: %d\n", pdata->freq_nums);
		for (i = 0; i < pdata->freq_nums; i++)
			pr_info("frequency[%d]: %d.%dHz\n", i,
					pdata->freq_array[i]/10,
					pdata->freq_array[i]%10);
	} else {
		pr_info("frequency: %d.%dHz\n", pdata->frequency/10,
				pdata->frequency%10);
	}
	pr_info("normal_ratio: %d\n", pdata->normal_ratio);
	pr_info("overdrive_ratio: %d\n", pdata->overdrive_ratio);

err_parsing_dt:
	return ret;
}

static int s2mu106_haptic_parse_dt(struct device *dev,
			struct s2mu106_haptic_data *haptic)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mu106-haptic");
	struct s2mu106_haptic_platform_data *pdata = haptic->pdata;
	u32 temp;
	int ret = 0;

	pr_info("%s : start dt parsing\n", __func__);

	if (np == NULL) {
		pr_err("%s : error to get dt node\n", __func__);
		goto err_parsing_dt;
	}

	ret = of_property_read_string(np, "haptic,motor_type",
			&pdata->motor_type);
	if (ret)
		pr_err("%s: motor_type is undefined\n", __func__);

	pr_info("motor_type: %s\n", pdata->motor_type);

	/*	Haptic operation mode
		0 : S2MU106_HAPTIC_ERM_I2C
		1 : S2MU106_HAPTIC_ERM_GPIO
		2 : S2MU106_HAPTIC_LRA
		default : S2MU106_HAPTIC_ERM_GPIO
	*/
	pdata->hap_mode = 1;
	ret = of_property_read_u32(np, "haptic,operation_mode", &temp);
	if (ret < 0) {
		pr_err("%s : error to get operation mode\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->hap_mode = temp;

	if (pdata->hap_mode == S2MU106_HAPTIC_LRA) {
		ret = s2mu106_haptic_parse_dt_lra(dev, np, pdata);
		if (ret) {
			pr_err("%s: error to get lra data\n", __func__);
			goto err_parsing_dt;
		}
	}

	if (pdata->hap_mode == S2MU106_HAPTIC_ERM_GPIO) {
		ret = pdata->motor_en = of_get_named_gpio(np,
				"haptic,motor_en", 0);
		if (ret < 0) {
			pr_err("%s : can't get motor_en\n", __func__);
			goto err_parsing_dt;
		}
	}

	/* Haptic boost setting */
	pdata->hbst.en = (of_find_property(np, "haptic,hbst_en", NULL)) ?
		true : false;

	pdata->hbst.automode =
		(of_find_property(np, "haptic,hbst_automode", NULL)) ?
		true : false;

	ret = of_property_read_u32(np, "haptic,boost_level", &temp);
	if (ret < 0)
		pdata->hbst.level = 5500;
	else
		pdata->hbst.level = temp;

	/* parsing info */
	pr_info("%s :operation_mode = %d, HBST_EN %s, HBST_AUTO_MODE %s\n",
			__func__, pdata->hap_mode,
			pdata->hbst.en ? "enabled" : "disabled",
			pdata->hbst.automode ? "enabled" : "disabled");

	return 0;

err_parsing_dt:
	return -1;
}
#endif

static void s2mu106_haptic_initial(struct s2mu106_haptic_data *haptic)
{
	u8 data;

	/* Haptic Boost initial setting */
	if (haptic->pdata->hbst.en){
		pr_info("%s : Haptic Boost Enable - Auto mode(%s)\n", __func__,
				haptic->pdata->hbst.automode ? "enabled" : "disabled");
		/* Boost voltage level setting
			default : 5.5V */
		s2mu106_set_boost_voltage(haptic, haptic->pdata->hbst.level);

		if (haptic->pdata->hbst.automode) {
			s2mu106_update_reg(haptic->i2c, S2MU106_REG_HT_OTP0,
						HBST_OK_MASK_EN, HBST_OK_MASK_EN);
			s2mu106_update_reg(haptic->i2c,	S2MU106_REG_HBST_CTRL0,
						0, SEL_HBST_HAPTIC_MASK);
		} else {
			s2mu106_update_reg(haptic->i2c, S2MU106_REG_HT_OTP0,
						0, HBST_OK_MASK_EN);
			s2mu106_update_reg(haptic->i2c,	S2MU106_REG_HBST_CTRL0,
						SEL_HBST_HAPTIC_MASK, SEL_HBST_HAPTIC_MASK);
		}
	} else {
		pr_info("%s : HDVIN - Vsys HDVIN voltage : Min 3.5V\n", __func__);
#if IS_ENABLED(CONFIG_VIBRATOR_S2MU106_VOLTAGE_3P3)
		s2mu106_update_reg(haptic->i2c, S2MU106_REG_HT_OTP3, 0x03, VCENUP_TRIM_MASK);
#endif
		s2mu106_update_reg(haptic->i2c, S2MU106_REG_HT_OTP2, 0x00, VCEN_SEL_MASK);
	}

	/* mode setting */
	switch (haptic->pdata->hap_mode) {
	case S2MU106_HAPTIC_LRA:
		data = LRA_MODE_EN;
		pwm_config(haptic->pdata->pwm, haptic->max_duty,
				haptic->period);
		s2mu106_update_reg(haptic->i2c, S2MU106_REG_OV_BK_OPTION,
					LRA_MODE_SET_MASK, LRA_MODE_SET_MASK);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_FILTERCOEF1, 0x7F);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_FILTERCOEF2, 0x5A);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_FILTERCOEF3, 0x02);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_PWM_CNT_NUM, 0x40);
		s2mu106_update_reg(haptic->i2c, S2MU106_REG_OV_WAVE_NUM, 0xF0, 0xF0);
		break;
	case S2MU106_HAPTIC_ERM_GPIO:
		data = ERM_HDPWM_MODE_EN;
		if (gpio_is_valid(haptic->pdata->motor_en)) {
			pr_info("%s : MOTOR_EN enable\n", __func__);
			gpio_request_one(haptic->pdata->motor_en, GPIOF_OUT_INIT_LOW, "MOTOR_EN");
			gpio_free(haptic->pdata->motor_en);
		}
		break;
	case S2MU106_HAPTIC_ERM_I2C:
		data = HAPTIC_MODE_OFF;
		break;
	default:
		data = ERM_HDPWM_MODE_EN;
		break;
	}
	s2mu106_write_reg(haptic->i2c, S2MU106_REG_HAPTIC_MODE, data);

	if (haptic->pdata->hap_mode == S2MU106_HAPTIC_ERM_I2C ||
			haptic->pdata->hap_mode == S2MU106_HAPTIC_ERM_GPIO) {
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_PERI_TAR1, 0x00);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_PERI_TAR2, 0x00);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_DUTY_TAR1, 0x00);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_DUTY_TAR2, 0x00);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_AMPCOEF1, 0x7D);
	}

	pr_info("%s, haptic operation mode = %d\n", __func__, haptic->pdata->hap_mode);
}

static struct of_device_id s2mu106_haptic_match_table[] = {
	{ .compatible = "sec,s2mu106-haptic",},
	{},
};

static const struct sec_vibrator_ops s2mu106_multi_freq_vib_ops = {
	.enable = s2mu106_haptic_enable,
	.set_intensity = s2mu106_haptic_set_intensity,
	.set_frequency = s2mu106_haptic_set_freq,
	.set_overdrive = s2mu106_haptic_set_overdrive,
	.get_motor_type = s2mu106_haptic_get_motor_type,
};

static const struct sec_vibrator_ops s2mu106_single_freq_vib_ops = {
	.enable = s2mu106_haptic_enable,
	.set_intensity = s2mu106_haptic_set_intensity,
	.get_motor_type = s2mu106_haptic_get_motor_type,
};

static const struct sec_vibrator_ops s2mu106_dc_vib_ops = {
	.enable = s2mu106_haptic_enable,
	.get_motor_type = s2mu106_haptic_get_motor_type,
};

static int s2mu106_haptic_probe(struct platform_device *pdev)
{
	struct s2mu106_dev *s2mu106 = dev_get_drvdata(pdev->dev.parent);
	struct s2mu106_haptic_data *haptic;
	int ret = 0;

	pr_info("%s Start\n", __func__);
	haptic = devm_kzalloc(&pdev->dev,
			sizeof(struct s2mu106_haptic_data), GFP_KERNEL);

	if (!haptic) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	haptic->i2c = s2mu106->haptic;

	haptic->pdata = devm_kzalloc(&pdev->dev, sizeof(*(haptic->pdata)), GFP_KERNEL);
	if (!haptic->pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	ret = s2mu106_haptic_parse_dt(&pdev->dev, haptic);
	if (ret < 0)
		return ret;

	platform_set_drvdata(pdev, haptic);

	if (haptic->pdata->hap_mode == S2MU106_HAPTIC_LRA) {
		haptic->period =
			S2MU106_FREQ_DIVIDER / haptic->pdata->frequency;
		haptic->duty_ratio = haptic->pdata->normal_ratio;
		haptic->max_duty = haptic->duty =
			haptic->period * haptic->duty_ratio / 100;
		pr_info("%s : duty: %d period: %d\n", __func__,
				haptic->max_duty, haptic->period);
	}

	s2mu106_haptic_initial(haptic);

	haptic->sec_vib_ddata.dev = &pdev->dev;
	if (haptic->pdata->hap_mode == S2MU106_HAPTIC_LRA) {
		if (haptic->pdata->freq_nums)
			haptic->sec_vib_ddata.vib_ops =
				&s2mu106_multi_freq_vib_ops;
		else
			haptic->sec_vib_ddata.vib_ops =
				&s2mu106_single_freq_vib_ops;
	} else
		haptic->sec_vib_ddata.vib_ops = &s2mu106_dc_vib_ops;
	sec_vibrator_register(&haptic->sec_vib_ddata);

	return 0;
}

static int s2mu106_haptic_remove(struct platform_device *pdev)
{
	struct s2mu106_haptic_data *haptic = platform_get_drvdata(pdev);

	sec_vibrator_unregister(&haptic->sec_vib_ddata);
	s2mu106_haptic_onoff(haptic, false);
	return 0;
}

static int s2mu106_haptic_suspend(struct device *dev)
{
	struct platform_device *pdev =
		container_of(dev, struct platform_device, dev);
	struct s2mu106_haptic_data *haptic = platform_get_drvdata(pdev);

	s2mu106_haptic_onoff(haptic, false);
	return 0;
}

static int s2mu106_haptic_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(s2mu106_haptic_pm_ops, s2mu106_haptic_suspend,
		s2mu106_haptic_resume);
static struct platform_driver s2mu106_haptic_driver = {
	.driver = {
		.name	= "s2mu106-haptic",
		.owner	= THIS_MODULE,
		.pm	= &s2mu106_haptic_pm_ops,
		.of_match_table = s2mu106_haptic_match_table,
	},
	.probe		= s2mu106_haptic_probe,
	.remove		= s2mu106_haptic_remove,
};

static int __init s2mu106_haptic_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&s2mu106_haptic_driver);
}
late_initcall(s2mu106_haptic_init);

static void __exit s2mu106_haptic_exit(void)
{
	platform_driver_unregister(&s2mu106_haptic_driver);
}
module_exit(s2mu106_haptic_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("s2mu106 haptic driver");
