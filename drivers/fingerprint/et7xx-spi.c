/*
 * Copyright (C) 2016 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "fingerprint.h"
#include "et7xx.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/sysfs.h>
#include <linux/version.h>

static void et7xx_reset(struct et7xx_data *etspi)
{
	pr_debug("Entry\n");
	if (etspi->sleepPin) {
		gpio_set_value(etspi->sleepPin, 0);
		usleep_range(1050, 1100);
		gpio_set_value(etspi->sleepPin, 1);
	}
}

static void et7xx_reset_control(struct et7xx_data *etspi, int status)
{
	pr_debug("Entry\n");
	if (etspi->sleepPin)
		gpio_set_value(etspi->sleepPin, status);
}

void et7xx_pin_control(struct et7xx_data *etspi, bool pin_set)
{
	int retval = 0;

	if (IS_ERR(etspi->p))
		return;

	etspi->p->state = NULL;
	if (pin_set) {
		if (!IS_ERR(etspi->pins_poweron)) {
			retval = pinctrl_select_state(etspi->p, etspi->pins_poweron);
			if (retval)
				pr_err("can't set pin default state : %d\n", retval);
			pr_info("idle\n");
		}
	} else {
		if (!IS_ERR(etspi->pins_poweroff)) {
			retval = pinctrl_select_state(etspi->p, etspi->pins_poweroff);
			if (retval)
				pr_err("can't set pin sleep state : %d\n", retval);
			pr_info("sleep\n");
		}
	}
}

static void et7xx_power_control(struct et7xx_data *etspi, int status)
{
	int retval = 0;

	if (etspi->ldo_enabled == status)
	{
		pr_err("called duplicate\n");
		return;
	}
	pr_info("status = %d\n", status);
	if (status == 1) {
		if (etspi->regulator_3p3) {
			retval = regulator_enable(etspi->regulator_3p3);
			if (retval) {
				pr_err("regulator enable failed rc=%d\n", retval);
				goto regulator_failed;
			}
			etspi->ldo_enabled = 1;
			usleep_range(2100, 2100);
		} else if (etspi->ldo_pin) {
			gpio_set_value(etspi->ldo_pin, 1);
			etspi->ldo_enabled = 1;
			usleep_range(2100, 2100);
		}
		if (etspi->sleepPin) {
			gpio_set_value(etspi->sleepPin, 1);
			usleep_range(1100, 1150);
		}
		et7xx_pin_control(etspi, true);
	} else if (status == 0) {
		et7xx_pin_control(etspi, false);
		if (etspi->sleepPin)
			gpio_set_value(etspi->sleepPin, 0);
		if (etspi->regulator_3p3) {
			retval = regulator_disable(etspi->regulator_3p3);
			if (retval) {
				pr_err("regulator disable failed rc=%d\n", retval);
				goto regulator_failed;
			}
			etspi->ldo_enabled = 0;
		} else if (etspi->ldo_pin) {
			gpio_set_value(etspi->ldo_pin, 0);
			etspi->ldo_enabled = 0;
		}
	} else {
		pr_err("can't support this value. %d\n", status);
	}

regulator_failed:
	return;
}

static ssize_t et7xx_read(struct file *filp,
						char __user *buf,
						size_t count,
						loff_t *f_pos)
{
	return 0;
}

static ssize_t et7xx_write(struct file *filp,
						const char __user *buf,
						size_t count,
						loff_t *f_pos)
{
	return 0;
}

static long et7xx_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	struct et7xx_data *etspi;
	u32 tmp;
	struct egis_ioc_transfer *ioc = NULL;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	u8 *buf, *address, *result, *fr;
#endif

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != EGIS_IOC_MAGIC) {
		pr_err("_IOC_TYPE(cmd) != EGIS_IOC_MAGIC");
		return -ENOTTY;
	}

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		retval = !access_ok(VERIFY_WRITE,
						(void __user *)arg,
						_IOC_SIZE(cmd));
	if (retval == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		retval = !access_ok(VERIFY_READ,
						(void __user *)arg,
						_IOC_SIZE(cmd));
	if (retval) {
		pr_err("err");
		return -EFAULT;
	}

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	etspi = filp->private_data;
	mutex_lock(&etspi->buf_lock);

	/* segmented and/or full-duplex I/O request */
	if (_IOC_NR(cmd) != _IOC_NR(EGIS_IOC_MESSAGE(0))
					|| _IOC_DIR(cmd) != _IOC_WRITE) {
		retval = -ENOTTY;
		goto et7xx_ioctl_out;
	}

	tmp = _IOC_SIZE(cmd);
	if ((tmp == 0) || (tmp % sizeof(struct egis_ioc_transfer)) != 0) {
		pr_err("ioc size error\n");
		retval = -EINVAL;
		goto et7xx_ioctl_out;
	}
	/* copy into scratch area */
	ioc = kmalloc(tmp, GFP_KERNEL);
	if (!ioc) {
		retval = -ENOMEM;
		goto et7xx_ioctl_out;
	}
	if (__copy_from_user(ioc, (void __user *)arg, tmp)) {
		pr_err("__copy_from_user error\n");
		retval = -EFAULT;
		goto et7xx_ioctl_out;
	}

	switch (ioc->opcode) {
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	/*
	 * Read register
	 * tx_buf include register address will be read
	 */
	case FP_REGISTER_READ:
		address = ioc->tx_buf;
		result = ioc->rx_buf;
		pr_debug("etspi FP_REGISTER_READ\n");
		retval = et7xx_io_read_register(etspi, address, result);
		if (retval < 0)
			pr_err("FP_REGISTER_READ error retval = %d\n", retval);
		break;
	case FP_REGISTER_BREAD:
		pr_debug("FP_REGISTER_BREAD\n");
		retval = et7xx_io_burst_read_register(etspi, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_BREAD error retval = %d\n", retval);
		break;
	case FP_REGISTER_BREAD_BACKWARD:
		pr_debug("FP_REGISTER_BREAD_BACKWARD\n");
		retval = et7xx_io_burst_read_register_backward(etspi, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_BREAD_BACKWARD error retval = %d\n", retval);
		break;
	/*
	 * Write data to register
	 * tx_buf includes address and value will be wrote
	 */
	case FP_REGISTER_WRITE:
		buf = ioc->tx_buf;
		pr_debug("FP_REGISTER_WRITE\n");
		retval = et7xx_io_write_register(etspi, buf);
		if (retval < 0)
			pr_err("FP_REGISTER_WRITE error retval = %d\n", retval);
		break;
	case FP_REGISTER_BWRITE:
		pr_debug("FP_REGISTER_BWRITE\n");
		retval = et7xx_io_burst_write_register(etspi, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_BWRITE error retval = %d\n", retval);
		break;
	case FP_REGISTER_BWRITE_BACKWARD:
		pr_debug("FP_REGISTER_BWRITE_BACKWARD\n");
		retval = et7xx_io_burst_write_register_backward(etspi, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_BWRITE_BACKWARD error retval = %d\n", retval);
		break;

	case FP_EFUSE_READ:
		pr_debug("FP_EFUSE_READ\n");
		retval = et7xx_io_read_efuse(etspi, ioc);
		if (retval < 0)
			pr_err("FP_EFUSE_READ error retval = %d\n", retval);
		break;
	case FP_EFUSE_WRITE:
		pr_debug("FP_EFUSE_WRITE\n");
		retval = et7xx_io_write_efuse(etspi, ioc);
		if (retval < 0)
			pr_err("FP_EFUSE_WRITE error retval = %d\n", retval);
		break;
	case FP_GET_IMG:
		fr = ioc->rx_buf;
		pr_debug("FP_GET_IMG\n");
		retval = et7xx_io_get_frame(etspi, fr, ioc->len);
		if (retval < 0)
			pr_err("FP_GET_IMG error retval = %d\n", retval);
		break;
	case FP_WRITE_IMG:
		fr = ioc->tx_buf;
		pr_debug("FP_WRITE_IMG\n");
		retval = et7xx_io_write_frame(etspi, fr, ioc->len);
		if (retval < 0)
			pr_err("FP_WRITE_IMG error retval = %d\n", retval);
		break;
	case FP_GET_ZAVG:
		fr = ioc->rx_buf;
		pr_debug("FP_GET_ZAVG\n");
		retval = et7xx_io_get_zone_average(etspi, fr, ioc->len);
		if (retval < 0)
			pr_err("FP_GET_ZAVG error retval = %d\n", retval);
		break;
	case FP_GET_HSTG:
		fr = ioc->rx_buf;
		pr_debug("FP_GET_HSTG\n");
		retval = et7xx_io_get_histogram(etspi, fr, ioc->len);
		if (retval < 0)
			pr_err("FP_GET_HSTG error retval = %d\n", retval);
		break;
	case FP_CIS_REGISTER_READ:
		address = ioc->tx_buf;
		result = ioc->rx_buf;
		pr_debug("FP_CIS_REGISTER_READ\n");
		retval = et7xx_io_read_cis_register(etspi, address, result);
		if (retval < 0)
			pr_err("FP_CIS_REGISTER_READ error retval = %d\n", retval);
		break;
	case FP_CIS_REGISTER_WRITE:
		buf = ioc->tx_buf;
		pr_debug("FP_CIS_REGISTER_WRITE\n");
		retval = et7xx_io_write_cis_register(etspi, buf);
		if (retval < 0)
			pr_err("FP_CIS_REGISTER_WRITE error retval = %d\n", retval);
		break;
	case FP_CIS_PRE_CAPTURE:
		pr_debug("FP_CIS_PRE_CAPTURE\n");
		retval = et7xx_io_pre_capture(etspi);
		if (retval < 0)
			pr_err("FP_CIS_PRE_CAPTURE error retval = %d\n",retval);
		break;
	case FP_GET_CIS_FRAME:
		fr = ioc->rx_buf;
		pr_debug("FP_GET_FRAME\n");
		retval = et7xx_io_get_cis_frame(etspi, fr, ioc->len);
		if (retval < 0)
			pr_err("FP_GET_FRAME error retval = %d\n", retval);
		break;
	case FP_TRANSFER_COMMAND:
		pr_debug("FP_TRANSFER_COMMAND\n");
		retval = et7xx_io_transfer_command(etspi, ioc->tx_buf, ioc->rx_buf,
											ioc->len);
		if (retval < 0)
			pr_err("FP_TRANSFER_COMMAND error retval = %d\n", retval);
		break;
	case FP_EEPROM_READ:
		pr_info("FP_EEPROM_READ\n");
		et7xx_eeprom_read(etspi, ioc);
		break;
	case FP_EEPROM_HIGH_SPEED_READ:
		pr_info("FP_EEPROM_HIGH_SPEED_READ\n");
		et7xx_eeprom_high_speed_read(etspi, ioc);
		break;
	case FP_EEPROM_WRITE:
		pr_info("FP_EEPROM_WRITE\n");
		et7xx_eeprom_write(etspi, ioc);
		break;
	case FP_EEPROM_CHIP_ERASE:
		pr_info("FP_EEPROM_CHIP_ERASE\n");
		et7xx_eeprom_chip_erase(etspi);
		break;
	case FP_EEPROM_SECTOR_ERASE:
		pr_info("FP_EEPROM_SECTOR_ERASE\n");
		et7xx_eeprom_sector_erase(etspi, ioc);
		break;
	case FP_EEPROM_BLOCK_ERASE:
		pr_info("FP_EEPROM_BLOCK_ERASE\n");
		et7xx_eeprom_block_erase(etspi, ioc);
		break;
	case FP_EEPROM_WREN:
		pr_info("FP_EEPROM_WREN\n");
		et7xx_eeprom_write_controller(etspi, 1);
		break;
	case FP_EEPROM_WRDI:
		pr_info("FP_EEPROM_WRDI\n");
		et7xx_eeprom_write_controller(etspi, 0);
		break;
	case FP_EEPROM_RSDR:
		pr_info("FP_EEPROM_RSDR\n");
		et7xx_eeprom_rdsr(etspi, ioc->rx_buf);
		break;
	case FP_EEPROM_WRITE_IN_NON_TZ:
		pr_info("FP_EEPROM_WRITE_IN_NON_TZ\n");
		et7xx_eeprom_write_in_non_tz(etspi, ioc);
		break;
#endif
	case FP_SENSOR_RESET:
		pr_info("FP_SENSOR_RESET\n");
		et7xx_reset(etspi);
		break;

	case FP_RESET_CONTROL:
		pr_info("FP_RESET_CONTROL, status = %d\n", ioc->len);
		et7xx_reset_control(etspi, ioc->len);
		break;

	case FP_POWER_CONTROL:
		pr_info("FP_POWER_CONTROL, status = %d\n", ioc->len);
		et7xx_power_control(etspi, ioc->len);
		break;
	case FP_SET_SPI_CLOCK:
		pr_info("FP_SET_SPI_CLOCK, clock = %d\n", ioc->speed_hz);
		etspi->spi_speed = ioc->speed_hz;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
		etspi->spi->max_speed_hz = ioc->speed_hz;
#endif
		et7xx_spi_clk_enable(etspi);
		break;

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	case FP_DISABLE_SPI_CLOCK:
		pr_info("FP_DISABLE_SPI_CLOCK\n");
		et7xx_spi_clk_disable(etspi);
		break;
	case FP_CPU_SPEEDUP:
		pr_debug("FP_CPU_SPEEDUP\n");
		et7xx_set_cpu_speedup(etspi, ioc->len);
		break;
	case FP_SET_SENSOR_TYPE:
		if ((int)ioc->len >= SENSOR_OOO &&
				(int)ioc->len < SENSOR_MAXIMUM) {
			if ((int)ioc->len == SENSOR_OOO &&
					etspi->sensortype == SENSOR_FAILED) {
				pr_info("maintain type check from out of order :%s\n",
					sensor_status[etspi->sensortype + 2]);
			} else {
				etspi->sensortype = (int)ioc->len;
				pr_info("FP_SET_SENSOR_TYPE :%s\n",
					sensor_status[etspi->sensortype + 2]);
			}
		} else {
			pr_err("FP_SET_SENSOR_TYPE invalid value %d\n", (int)ioc->len);
			etspi->sensortype = SENSOR_UNKNOWN;
		}
		break;
#endif
	case FP_SPI_VALUE:
		etspi->spi_value = ioc->len;
		pr_info("spi_value: 0x%x\n", etspi->spi_value);
		break;

	case FP_MODEL_INFO:
		pr_info("modelinfo is %s\n", etspi->model_info);
		retval = copy_to_user((u8 __user *) (uintptr_t)ioc->rx_buf,
												etspi->model_info, 10);
		if (retval != 0)
			pr_err("FP_IOCTL_MODEL_INFO copy_to_user failed: %d\n", retval);
		break;

	case FP_IOCTL_RESERVED_01:
	case FP_IOCTL_RESERVED_02:
	case FP_IOCTL_RESERVED_03:
	case FP_IOCTL_RESERVED_04:
	case FP_IOCTL_RESERVED_05:
	case FP_IOCTL_RESERVED_06:
	case FP_IOCTL_RESERVED_07:
	case FP_IOCTL_RESERVED_08:
		break;

	default:
		retval = -EFAULT;
		break;

	}

et7xx_ioctl_out:
	if (ioc != NULL)
		kfree(ioc);

	mutex_unlock(&etspi->buf_lock);
	if (retval < 0)
		pr_err("retval = %d\n", retval);
	return retval;
}

#ifdef CONFIG_COMPAT
static long et7xx_compat_ioctl(struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	return et7xx_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define et7xx_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int et7xx_open(struct inode *inode, struct file *filp)
{
	struct et7xx_data *etspi;
	int	retval = -ENXIO;

	pr_info("Entry\n");
	mutex_lock(&device_list_lock);

	list_for_each_entry(etspi, &device_list, device_entry) {
		if (etspi->devt == inode->i_rdev) {
			retval = 0;
			break;
		}
	}
	if (retval == 0) {
		if (etspi->buf == NULL) {
			etspi->buf = kmalloc(bufsiz, GFP_KERNEL);
			if (etspi->buf == NULL) {
				dev_dbg(&etspi->spi->dev, "open/ENOMEM\n");
				retval = -ENOMEM;
			}
		}
		if (retval == 0) {
			etspi->users++;
			filp->private_data = etspi;
			nonseekable_open(inode, filp);
			etspi->bufsiz = bufsiz;
		}
	} else
		pr_debug("nothing for minor %d\n", iminor(inode));

	mutex_unlock(&device_list_lock);
	return retval;
}

static int et7xx_release(struct inode *inode, struct file *filp)
{
	struct et7xx_data *etspi;

	pr_info("Entry\n");
	mutex_lock(&device_list_lock);
	etspi = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	etspi->users--;
	if (etspi->users == 0) {
#ifndef ENABLE_SENSORS_FPRINT_SECURE
		int	dofree;
#endif
		kfree(etspi->buf);
		etspi->buf = NULL;

		/* ... after we unbound from the underlying device? */
#ifndef ENABLE_SENSORS_FPRINT_SECURE
		spin_lock_irq(&etspi->spi_lock);
		dofree = (etspi->spi == NULL);
		spin_unlock_irq(&etspi->spi_lock);

		if (dofree)
			kfree(etspi);
#endif
	}
	mutex_unlock(&device_list_lock);

	return 0;
}

int et7xx_platformInit(struct et7xx_data *etspi)
{
	int retval = 0;

	pr_info("Entry\n");
	/* gpio setting for ldo, sleep pin */
	if (etspi != NULL) {
		if (etspi->btp_vdd) {
			etspi->regulator_3p3 = regulator_get(NULL, etspi->btp_vdd);
			if (IS_ERR(etspi->regulator_3p3)) {
				pr_err("regulator get failed\n");
				etspi->regulator_3p3 = NULL;
				goto et7xx_platformInit_ldo_failed;
			} else {
				regulator_set_load(etspi->regulator_3p3, 100000);
				pr_info("btp_regulator ok\n");
				etspi->ldo_enabled = 0;
			}
		} else if (etspi->ldo_pin) {
			retval = gpio_request(etspi->ldo_pin, "et7xx_ldo_en");
			if (retval < 0) {
				pr_err("gpio_request et7xx_ldo_en failed\n");
				goto et7xx_platformInit_ldo_failed;
			}
			gpio_direction_output(etspi->ldo_pin, 0);
			etspi->ldo_enabled = 0;
		}

		retval = gpio_request(etspi->sleepPin, "et7xx_sleep");
		if (retval < 0) {
			pr_err("gpio_requset et7xx_sleep failed\n");
			goto et7xx_platformInit_sleep_failed;
		}
		gpio_direction_output(etspi->sleepPin, 0);
		if (retval < 0) {
			pr_err("gpio_direction_output SLEEP failed\n");
			retval = -EBUSY;
			goto et7xx_platformInit_sleep_failed;
		}

		if (etspi->sleepPin)
			pr_info("sleep value =%d\n", gpio_get_value(etspi->sleepPin));
		if (etspi->ldo_pin)
			pr_info("ldo en value =%d\n", gpio_get_value(etspi->ldo_pin));

	} else {
		retval = -EFAULT;
	}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	wake_lock_init(&etspi->fp_spi_lock,
		WAKE_LOCK_SUSPEND, "et7xx_wake_lock");
#endif

	pr_info("successful status=%d\n", retval);
	return retval;

et7xx_platformInit_sleep_failed:
	if (etspi->sleepPin)
		gpio_free(etspi->sleepPin);
	if (etspi->ldo_pin)
		gpio_free(etspi->ldo_pin);
et7xx_platformInit_ldo_failed:
	pr_err("is failed. %d\n", retval);
	return retval;
}

void et7xx_platformUninit(struct et7xx_data *etspi)
{
	pr_info("Entry\n");

	if (etspi != NULL) {
		et7xx_pin_control(etspi, false);
		if (etspi->regulator_3p3)
			regulator_put(etspi->regulator_3p3);
		else if (etspi->ldo_pin)
			gpio_free(etspi->ldo_pin);
		if (etspi->sleepPin)
			gpio_free(etspi->sleepPin);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
		wake_lock_destroy(&etspi->fp_spi_lock);
#endif
	}
}

static int et7xx_parse_dt(struct device *dev, struct et7xx_data *etspi)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int retval = 0;
	int gpio;

	gpio = of_get_named_gpio_flags(np, "etspi-sleepPin", 0, &flags);
	if (gpio < 0) {
		retval = gpio;
		pr_err("fail to get sleepPin\n");
		goto dt_exit;
	} else {
		etspi->sleepPin = gpio;
		pr_info("sleepPin=%d\n", etspi->sleepPin);
	}

	gpio = of_get_named_gpio_flags(np, "etspi-ldoPin", 0, &flags);
	if (gpio < 0) {
		etspi->ldo_pin = 0;
		pr_info("not use ldo_pin\n");
	} else {
		etspi->ldo_pin = gpio;
		pr_info("ldo_pin=%d\n", etspi->ldo_pin);
	}

	if (of_property_read_string(np, "etspi-regulator", &etspi->btp_vdd) < 0) {
		pr_info("not use btp_regulator\n");
		etspi->btp_vdd = NULL;
		if (gpio < 0) {
			retval = gpio;
			pr_err("fail to get power\n");
			goto dt_exit;
		}
	}
	pr_info("regulator: %s\n", etspi->btp_vdd);

	if (of_property_read_u32(np, "etspi-min_cpufreq_limit",
		&etspi->min_cpufreq_limit))
		etspi->min_cpufreq_limit = 0;

	if (of_property_read_string_index(np, "etspi-chipid", 0,
			(const char **)&etspi->chipid)) {
		etspi->chipid = NULL;
	}
	pr_info("chipid: %s\n", etspi->chipid);

	if (of_property_read_string_index(np, "etspi-modelinfo", 0,
			(const char **)&etspi->model_info)) {
		etspi->model_info = "NONE";
	}
	pr_info("modelinfo: %s\n", etspi->model_info);

	if (of_property_read_string_index(np, "etspi-position", 0,
			(const char **)&etspi->sensor_position)) {
		etspi->sensor_position = "0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00";
	}
	pr_info("position: %s\n", etspi->sensor_position);

	if (of_property_read_string_index(np, "etspi-rb", 0,
			(const char **)&etspi->rb)) {
		etspi->rb = "525,-1,-1";
	}
	pr_info("rb: %s\n", etspi->rb);

	etspi->p = pinctrl_get_select_default(dev);
#if !defined(ENABLE_SENSORS_FPRINT_SECURE) || defined(DISABLED_GPIO_PROTECTION)
	if (!IS_ERR(etspi->p)) {
		etspi->pins_poweroff = pinctrl_lookup_state(etspi->p, "pins_poweroff");
		if (IS_ERR(etspi->pins_poweroff)) {
			pr_err("could not get pins sleep_state (%li)\n",
				PTR_ERR(etspi->pins_poweroff));
		}

		etspi->pins_poweron = pinctrl_lookup_state(etspi->p, "pins_poweron");
		if (IS_ERR(etspi->pins_poweron)) {
			pr_err("could not get pins idle_state (%li)\n",
				PTR_ERR(etspi->pins_poweron));
		}
	} else {
		pr_err("failed pinctrl_get\n");
	}
#endif
	et7xx_pin_control(etspi, false);
	pr_info("is successful\n");
	return retval;

dt_exit:
	pr_err("is failed. %d\n", retval);
	return retval;
}

static const struct file_operations et7xx_fops = {
	.owner = THIS_MODULE,
	.write = et7xx_write,
	.read = et7xx_read,
	.unlocked_ioctl = et7xx_ioctl,
	.compat_ioctl = et7xx_compat_ioctl,
	.open = et7xx_open,
	.release = et7xx_release,
	.llseek = no_llseek,
};

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static int et7xx_type_check(struct et7xx_data *etspi)
{
	u8 buf1, buf2, buf3;

	et7xx_power_control(etspi, 1);

	msleep(20);

	et7xx_read_register(etspi, 0x00, &buf1);
	et7xx_read_register(etspi, 0x01, &buf2);
	et7xx_read_register(etspi, 0x02, &buf3);

	et7xx_power_control(etspi, 0);

	pr_info("buf1-3: %x, %x, %x\n", buf1, buf2, buf3);

	/*
	 * type check return value
	 * ET711A : 0x07 / 0x1D or 0x07 / 0x0B
	 * ET713A : 0x07 / 0x0D
	 * ET715  : 0x07 / 0x0F
	 */
	if ((buf1 == 0x07) && ((buf2 == 0x1D) || (buf2 == 0x0B))) {
		etspi->sensortype = SENSOR_EGISOPTICAL;
		pr_info("sensor type is EGIS ET711A sensor\n");
	} else if ((buf1 == 0x07) && (buf2 == 0x0D)) {
		etspi->sensortype = SENSOR_EGISOPTICAL;
		pr_info("sensor type is EGIS ET713A sensor\n");
	} else if ((buf1 == 0x07) && (buf2 == 0x0F)) {
		etspi->sensortype = SENSOR_EGISOPTICAL;
		pr_info("sensor type is EGIS ET715 sensor\n");
	} else {
		etspi->sensortype = SENSOR_FAILED;
		pr_info("sensor type is FAILED\n");
		return -ENODEV;
	}
	return 0;
}
#endif

static ssize_t et7xx_bfs_values_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct et7xx_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "\"FP_SPICLK\":\"%d\"\n",
			data->spi_speed);
}

static ssize_t et7xx_type_check_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct et7xx_data *data = dev_get_drvdata(dev);
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	int retry = 0;
	int retval = 0;

	do {
		retval = et7xx_type_check(data);
		pr_info("type (%u), retry (%d)\n",
			data->sensortype, retry);
	} while (!data->sensortype && ++retry < 3);

	if (retval == -ENODEV)
		pr_info("type check fail\n");
#endif
	return snprintf(buf, PAGE_SIZE, "%d\n", data->sensortype);
}

static ssize_t et7xx_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t et7xx_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct et7xx_data *etspi = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", etspi->chipid);
}

static ssize_t et7xx_adm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", DETECT_ADM);
}

static ssize_t et7xx_position_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct et7xx_data *etspi = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", etspi->sensor_position);
}

static ssize_t et7xx_rb_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct et7xx_data *etspi = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", etspi->rb);
}

static DEVICE_ATTR(bfs_values, 0444, et7xx_bfs_values_show, NULL);
static DEVICE_ATTR(type_check, 0444, et7xx_type_check_show, NULL);
static DEVICE_ATTR(vendor, 0444, et7xx_vendor_show, NULL);
static DEVICE_ATTR(name, 0444, et7xx_name_show, NULL);
static DEVICE_ATTR(adm, 0444, et7xx_adm_show, NULL);
static DEVICE_ATTR(position, 0444, et7xx_position_show, NULL);
static DEVICE_ATTR(rb, 0444, et7xx_rb_show, NULL);

static struct device_attribute *fp_attrs[] = {
	&dev_attr_bfs_values,
	&dev_attr_type_check,
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_adm,
	&dev_attr_position,
	&dev_attr_rb,
	NULL,
};

static void et7xx_work_func_debug(struct work_struct *work)
{
	pr_info("ldo: %d, sleep: %d, tz: %d, spi_value: 0x%x, type: %s\n",
		g_data->ldo_enabled, gpio_get_value(g_data->sleepPin),
		g_data->tz_mode, g_data->spi_value,
		sensor_status[g_data->sensortype + 2]);
}

static void et7xx_enable_debug_timer(void)
{
	mod_timer(&g_data->dbg_timer,
		round_jiffies_up(jiffies + FPSENSOR_DEBUG_TIMER_SEC));
}

static void et7xx_disable_debug_timer(void)
{
	del_timer_sync(&g_data->dbg_timer);
	cancel_work_sync(&g_data->work_debug);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static void et7xx_timer_func(unsigned long ptr)
#else
static void et7xx_timer_func(struct timer_list *t)
#endif
{
	queue_work(g_data->wq_dbg, &g_data->work_debug);
	mod_timer(&g_data->dbg_timer,
		round_jiffies_up(jiffies + FPSENSOR_DEBUG_TIMER_SEC));
}

static int et7xx_set_timer(struct et7xx_data *etspi)
{
	int retval = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	setup_timer(&etspi->dbg_timer, et7xx_timer_func, (unsigned long)etspi);
#else
	timer_setup(&etspi->dbg_timer, et7xx_timer_func, 0);
#endif
	etspi->wq_dbg = create_singlethread_workqueue("et7xx_debug_wq");
	if (!etspi->wq_dbg) {
		pr_err("could not create workqueue\n");
		return -ENOMEM;
	}
	INIT_WORK(&etspi->work_debug, et7xx_work_func_debug);
	return retval;
}

static struct class *et7xx_class;

static int et7xx_probe_common(struct device *dev, struct et7xx_data *etspi)
{
	int retval;
	unsigned long minor;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	int retry = 0;
#endif

	pr_info("Entry\n");

	/* Initialize the driver data */
	g_data = etspi;
	etspi->dev = dev;
	spin_lock_init(&etspi->spi_lock);
	mutex_init(&etspi->buf_lock);
	mutex_init(&device_list_lock);
	INIT_LIST_HEAD(&etspi->device_entry);

	/* device tree call */
	if (dev->of_node) {
		retval = et7xx_parse_dt(dev, etspi);
		if (retval) {
			pr_err("Failed to parse DT\n");
			goto et7xx_probe_parse_dt_failed;
		}
	}
	/* platform init */
	retval = et7xx_platformInit(etspi);
	if (retval != 0) {
		pr_err("platforminit failed\n");
		goto et7xx_probe_platformInit_failed;
	}

	retval = et7xx_register_platform_variable(etspi);
	if (retval < 0) {
		pr_err("platform_variable failed\n");
		goto et7xx_probe_platform_variable_failed;
	}

	etspi->enabled_clk = false;
	etspi->spi_value = 0;
	etspi->spi_speed = SLOW_BAUD_RATE;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	do {
		retval = et7xx_type_check(etspi);
		pr_info("type (%u), retry (%d)\n", etspi->sensortype, retry);
	} while (!etspi->sensortype && ++retry < 3);

	if (retval == -ENODEV)
		pr_info("type check fail\n");
#endif

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev_t;

		etspi->devt = MKDEV(ET7XX_MAJOR, minor);
		dev_t = device_create(et7xx_class, dev,
				etspi->devt, etspi, "esfp0");
		retval = IS_ERR(dev_t) ? PTR_ERR(dev_t) : 0;
	} else {
		pr_err("no minor number available!\n");
		retval = -ENODEV;
		mutex_unlock(&device_list_lock);
		goto et7xx_device_create_failed;
	}

	if (retval == 0) {
		set_bit(minor, minors);
		list_add(&etspi->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);

	retval = fingerprint_register(etspi->fp_device,
		etspi, fp_attrs, "fingerprint");
	if (retval) {
		pr_err("sysfs register failed\n");
		goto et7xx_register_failed;
	}

	retval = et7xx_set_timer(etspi);
	if (retval)
		goto et7xx_sysfs_failed;
	et7xx_enable_debug_timer();
	pr_info("is successful\n");
	return retval;

et7xx_sysfs_failed:
	fingerprint_unregister(etspi->fp_device, fp_attrs);
et7xx_register_failed:
	device_destroy(et7xx_class, etspi->devt);
	class_destroy(et7xx_class);
	et7xx_unregister_platform_variable(etspi);
et7xx_device_create_failed:
et7xx_probe_platform_variable_failed:
	et7xx_platformUninit(etspi);
et7xx_probe_platformInit_failed:
et7xx_probe_parse_dt_failed:
	pr_err("is failed : %d\n", retval);

	return retval;
}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
static int et7xx_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct et7xx_data *etspi;

	pr_info("Platform_device Entry\n");
	etspi = kzalloc(sizeof(*etspi), GFP_KERNEL);
	if (!etspi)
		return -ENOMEM;

	etspi->sensortype = SENSOR_UNKNOWN;
	etspi->tz_mode = true;

	retval = et7xx_probe_common(&pdev->dev, etspi);
	if (retval)
		goto et7xx_platform_probe_failed;

	pr_info("is successful\n");
	return retval;

et7xx_platform_probe_failed:
	kfree(etspi);
	pr_err("is failed : %d\n", retval);
	return retval;
}
#else
static int et7xx_probe(struct spi_device *spi)
{
	int retval = 0;
	struct et7xx_data *etspi;

	pr_info("spi_device Entry\n");
	etspi = kzalloc(sizeof(*etspi), GFP_KERNEL);
	if (!etspi)
		return -ENOMEM;

	spi->bits_per_word = 8;
	spi->max_speed_hz = SLOW_BAUD_RATE;
	spi->mode = SPI_MODE_0;
	spi->chip_select = 0;
	etspi->spi = spi;
	etspi->tz_mode = false;

	retval = spi_setup(spi);
	if (retval != 0) {
		pr_err("spi_setup() is failed. status : %d\n", retval);
		goto et7xx_spi_probe_set_setup_failed;
	}
	spi_set_drvdata(spi, etspi);

	retval = et7xx_probe_common(&spi->dev, etspi);
	if (retval)
		goto et7xx_spi_probe_failed;

	pr_info("is successful\n");
	return retval;

et7xx_spi_probe_failed:
	spi_set_drvdata(etspi->spi, NULL);
et7xx_spi_probe_set_setup_failed:
	kfree(etspi);
	pr_err("is failed : %d\n", retval);
	return retval;
}
#endif

static int et7xx_remove_common(struct device *dev)
{
	struct et7xx_data *etspi = dev_get_drvdata(dev);

	pr_info("Entry\n");
	if (etspi != NULL) {
		et7xx_disable_debug_timer();
		et7xx_platformUninit(etspi);
		et7xx_unregister_platform_variable(etspi);
		/* make sure ops on existing fds can abort cleanly */
		spin_lock_irq(&etspi->spi_lock);
#ifndef ENABLE_SENSORS_FPRINT_SECURE
		spi_set_drvdata(etspi->spi, NULL);
#endif
		etspi->spi = NULL;
		spin_unlock_irq(&etspi->spi_lock);

		/* prevent new opens */
		mutex_lock(&device_list_lock);
		fingerprint_unregister(etspi->fp_device, fp_attrs);
		list_del(&etspi->device_entry);
		device_destroy(et7xx_class, etspi->devt);
		clear_bit(MINOR(etspi->devt), minors);
		if (etspi->users == 0)
			kfree(etspi);
		mutex_unlock(&device_list_lock);
	}
	return 0;
}

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static int et7xx_remove(struct spi_device *spi) {
	et7xx_remove_common(&spi->dev);
#else
static int et7xx_remove(struct platform_device *pdev) {
	et7xx_remove_common(&pdev->dev);
#endif
	return 0;
}

static int et7xx_pm_suspend(struct device *dev)
{
	struct et7xx_data *etspi = dev_get_drvdata(dev);

	pr_info("Entry\n");
	if (etspi != NULL) {
		et7xx_disable_debug_timer();
		fps_suspend_set(etspi);
	}
	return 0;
}

static int et7xx_pm_resume(struct device *dev)
{
	struct et7xx_data *etspi = dev_get_drvdata(dev);

	pr_info("Entry\n");
	if (etspi != NULL) {
		et7xx_enable_debug_timer();
		fps_resume_set();
	}
	return 0;
}

static const struct dev_pm_ops et7xx_pm_ops = {
	.suspend = et7xx_pm_suspend,
	.resume = et7xx_pm_resume
};

static const struct of_device_id et7xx_match_table[] = {
	{ .compatible = "etspi,et7xx",},
	{},
};

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static struct spi_driver et7xx_spi_driver = {
#else
static struct platform_driver et7xx_spi_driver = {
#endif
	.driver = {
		.name =	"egis_fingerprint",
		.owner = THIS_MODULE,
		.pm = &et7xx_pm_ops,
		.of_match_table = et7xx_match_table
	},
	.probe = et7xx_probe,
	.remove = et7xx_remove,
};

/*-------------------------------------------------------------------------*/

static int __init et7xx_init(void)
{
	int retval = 0;

	pr_info("Entry\n");

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	retval = register_chrdev(ET7XX_MAJOR, "egis_fingerprint", &et7xx_fops);
	if (retval < 0) {
		pr_err("register_chrdev error.status:%d\n", retval);
		return retval;
	}

	et7xx_class = class_create(THIS_MODULE, "egis_fingerprint");
	if (IS_ERR(et7xx_class)) {
		pr_err("class_create error.\n");
		unregister_chrdev(ET7XX_MAJOR, et7xx_spi_driver.driver.name);
		return PTR_ERR(et7xx_class);
	}

#ifndef ENABLE_SENSORS_FPRINT_SECURE
	retval = spi_register_driver(&et7xx_spi_driver);
#else
	retval = platform_driver_register(&et7xx_spi_driver);
#endif
	if (retval < 0) {
		pr_err("spi_register_driver error.\n");
		class_destroy(et7xx_class);
		unregister_chrdev(ET7XX_MAJOR, et7xx_spi_driver.driver.name);
		return retval;
	}

	pr_info("is successful\n");
	return retval;
}

static void __exit et7xx_exit(void)
{
	pr_info("Entry\n");

#ifndef ENABLE_SENSORS_FPRINT_SECURE
	spi_unregister_driver(&et7xx_spi_driver);
#else
	platform_driver_unregister(&et7xx_spi_driver);
#endif
	class_destroy(et7xx_class);
	unregister_chrdev(ET7XX_MAJOR, et7xx_spi_driver.driver.name);
}

module_init(et7xx_init);
module_exit(et7xx_exit);

MODULE_AUTHOR("fp.sec@samsung.com");
MODULE_DESCRIPTION("Samsung Electronics Inc. ET7XX driver");
MODULE_LICENSE("GPL");
