#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>
#include <linux/hall.h>
#include <linux/notifier.h>
#include <linux/sec_class.h>

#if defined(CONFIG_HALL_NEW_NODE)
struct device *hall_ic;
EXPORT_SYMBOL(hall_ic);
#endif
struct device *sec_device_create(void *drvdata, const char *fmt);

struct hall_drvdata {
	struct input_dev *input;
	struct hall_platform_data *pdata;
#if defined(CONFIG_EMULATE_HALL_IC)
	int emulated_hall_ic_status;
#endif
	struct work_struct work;
	struct delayed_work flip_cover_dwork;
	struct wake_lock flip_wake_lock;
	u8 event_val;
};

static bool flip_cover;

static ssize_t hall_detect_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (flip_cover)
		sprintf(buf, "OPEN\n");
	else
		sprintf(buf, "CLOSE\n");

	return strlen(buf);
}
static DEVICE_ATTR_RO(hall_detect);

static struct attribute *hall_attrs[] = {
	&dev_attr_hall_detect.attr,
	NULL,
};

static struct attribute_group hall_attr_group = {
	.attrs = hall_attrs,
};

#ifdef CONFIG_SEC_FACTORY
static void flip_cover_work(struct work_struct *work)
{
	bool first, second;
	struct hall_drvdata *ddata =
		container_of(work, struct hall_drvdata,
				flip_cover_dwork.work);

	first = gpio_get_value(ddata->pdata->gpio_flip_cover);

	pr_info("[keys] %s flip_status #1 : %d (%s)\n", __func__, first,
			first ? "open" : "close");

	msleep(50);

	second = gpio_get_value(ddata->pdata->gpio_flip_cover);

	pr_info("[keys] %s flip_status #2 : %d (%s)\n", __func__, second,
			second ? "open" : "close");

	if (first == second) {
		flip_cover = first;

		input_report_switch(ddata->input, ddata->event_val, !flip_cover);
		input_sync(ddata->input);
	}
}
#else
static void flip_cover_work(struct work_struct *work)
{
	bool first;

	struct hall_drvdata *ddata =
		container_of(work, struct hall_drvdata,
				flip_cover_dwork.work);
#if !defined(CONFIG_EMULATE_HALL_IC)
	first = gpio_get_value(ddata->pdata->gpio_flip_cover);

	pr_info("[keys] %s flip_status : %d (%s)\n", __func__, first,
			first ? "open" : "close");

	flip_cover = first;

	input_report_switch(ddata->input, ddata->event_val, !flip_cover);
	input_sync(ddata->input);
#else
	first = !gpio_get_value(ddata->pdata->gpio_flip_cover_key1)
		& !gpio_get_value(ddata->pdata->gpio_flip_cover_key2);
	pr_info("keys:%s #1 key1=%d,key2=%d : %d\n", __func__,
			gpio_get_value(ddata->pdata->gpio_flip_cover_key1),
			gpio_get_value(ddata->pdata->gpio_flip_cover_key2),
			first);
	if (flip_cover != first) {
		if (first) {
			ddata->emulated_hall_ic_status =
				!ddata->emulated_hall_ic_status;
			pr_info("keys:%s #1 : %d , %d\n", __func__, first,
					ddata->emulated_hall_ic_status);
			input_report_switch(ddata->input, ddata->event_val,
					ddata->emulated_hall_ic_status);
			input_sync(ddata->input);
		}
		flip_cover = first;
	}
	schedule_delayed_work(&ddata->flip_cover_dwork, msecs_to_jiffies(1000));
#endif
}
#endif

static void __flip_cover_detect(struct hall_drvdata *ddata, bool flip_status)
{
	cancel_delayed_work_sync(&ddata->flip_cover_dwork);
#ifdef CONFIG_SEC_FACTORY
	schedule_delayed_work(&ddata->flip_cover_dwork, HZ / 20);
#else
	if (flip_status) {
		/* 50ms */
		wake_lock_timeout(&ddata->flip_wake_lock, HZ * 5 / 100);
		/* 10ms */
		schedule_delayed_work(&ddata->flip_cover_dwork, HZ * 1 / 100);
	} else {
		wake_unlock(&ddata->flip_wake_lock);
		schedule_delayed_work(&ddata->flip_cover_dwork, 0);
	}
#endif
}

static irqreturn_t flip_cover_detect(int irq, void *dev_id)
{
	bool flip_status;
	struct hall_drvdata *ddata = dev_id;

#if !defined(CONFIG_EMULATE_HALL_IC)
	flip_status = gpio_get_value(ddata->pdata->gpio_flip_cover);
#else
	flip_status = gpio_get_value(ddata->pdata->gpio_flip_cover_key1)
		& gpio_get_value(ddata->pdata->gpio_flip_cover_key2);
#endif

	pr_info("keys:%s flip_status : %d\n",
		 __func__, flip_status);

	__flip_cover_detect(ddata, flip_status);

	return IRQ_HANDLED;
}

static int hall_open(struct input_dev *input)
{
	struct hall_drvdata *ddata = input_get_drvdata(input);

	/* update the current status */
	schedule_delayed_work(&ddata->flip_cover_dwork, HZ / 2);
	/* Report current state of buttons that are connected to GPIOs */
	input_sync(input);

	return 0;
}

static void hall_close(struct input_dev *input)
{
}


static void init_hall_ic_irq(struct input_dev *input)
{
	struct hall_drvdata *ddata = input_get_drvdata(input);
	int ret = 0;
	int irq = ddata->pdata->irq_flip_cover;

#if !defined(EMUALTE_HALL_IC)
	flip_cover = gpio_get_value(ddata->pdata->gpio_flip_cover);
#else
	flip_cover = 0;
#endif

	INIT_DELAYED_WORK(&ddata->flip_cover_dwork, flip_cover_work);

	ret = request_threaded_irq(irq, NULL, flip_cover_detect,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"flip_cover", ddata);
	if (ret < 0) {
		pr_err("keys: failed to request flip cover irq %d gpio %d\n",
			irq, ddata->pdata->gpio_flip_cover);
	} else {
		pr_info("%s : success\n", __func__);
	}
}

#if defined(CONFIG_OF)
static struct hall_platform_data *of_hall_data_parsing_dt(struct device *dev)
{
	struct device_node *np_hall;
	struct hall_platform_data *pdata;
	int gpio;
	enum of_gpio_flags flags;
	int ret = 0;

	np_hall = dev->of_node;
	if (np_hall == NULL) {
		pr_err("%s : error to get dt node\n", __func__);
		ret = -EINVAL;
		goto err_out;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto err_out;
	}

	gpio = of_get_named_gpio_flags(np_hall, "hall,gpio_flip_cover", 0,
			&flags);
	if (gpio < 0) {
		pr_err("%s: fail to get flip_cover\n", __func__);
		ret = -EINVAL;
		goto err_out;
	}
	pdata->gpio_flip_cover = gpio;
	pr_info("%s: hall gpio=%d\n", __func__, gpio);

	gpio = gpio_to_irq(gpio);
	if (gpio < 0) {
		pr_err("%s: fail to return irq corresponding gpio\n", __func__);
		ret = -EINVAL;
		goto err_out;
	}
	pdata->irq_flip_cover = gpio;
	pr_info("%s : gpio_flip_cover=%d , irq_flip_cover=%d\n", __func__,
			pdata->gpio_flip_cover, pdata->irq_flip_cover);

#if defined(CONFIG_EMULATE_HALL_IC)
	gpio = of_get_named_gpio_flags(np_hall, "hall,gpio_flip_cover_key1", 0,
			&flags);
	pdata->gpio_flip_cover_key1 = gpio;

	gpio = of_get_named_gpio_flags(np_hall, "hall,gpio_flip_cover_key2", 0,
			&flags);
	pdata->gpio_flip_cover_key2 = gpio;
	pr_info("%s : cover_key1=%d , cover_key2=%d\n", __func__,
			pdata->gpio_flip_cover_key1,
			pdata->gpio_flip_cover_key2);
#endif

	return pdata;
err_out:
	return ERR_PTR(ret);
}
#endif

static int hall_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hall_drvdata *ddata;
	struct hall_platform_data *pdata = pdev->dev.platform_data;
	struct input_dev *input;
	int error;
	int wakeup = 0;

	ddata = devm_kzalloc(&pdev->dev, sizeof(struct hall_drvdata),
			GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;
#ifdef CONFIG_OF
	if (!pdata) {
		pdata = of_hall_data_parsing_dt(dev);
		if (IS_ERR(pdata)) {
			pr_err("%s : fail to get the dt (HALL)\n", __func__);
			error = -ENODEV;
			goto fail1;
		}
	}
#endif
	input = input_allocate_device();
	if (!input) {
		dev_err(dev, "failed to allocate state\n");
		error = -ENOMEM;
		goto fail1;
	}

	ddata->input = input;

	wake_lock_init(&ddata->flip_wake_lock, WAKE_LOCK_SUSPEND,
		"flip wake lock");

	ddata->pdata = pdata;
	platform_set_drvdata(pdev, ddata);
	input_set_drvdata(input, ddata);
	input->name = "hall";
	input->phys = "hall";
	input->dev.parent = &pdev->dev;

	input->evbit[0] |= BIT_MASK(EV_SW);

	ddata->event_val = SW_LID;

	input_set_capability(input, EV_SW, ddata->event_val);

	input->open = hall_open;
	input->close = hall_close;

	/* Enable auto repeat feature of Linux input subsystem */
	__set_bit(EV_REP, input->evbit);

	init_hall_ic_irq(input);

#if defined(CONFIG_HALL_NEW_NODE)
	hall_ic = sec_device_create(ddata, "hall_ic");
	error = sysfs_create_group(&hall_ic->kobj, &hall_attr_group);
#else
	error = sysfs_create_group(&sec_key->kobj, &hall_attr_group);
#endif
	if (error) {
		dev_err(dev, "Unable to export keys/switches, error: %d\n",
			error);
		goto fail2;
	}
	error = input_register_device(input);
	if (error) {
		dev_err(dev, "Unable to register input device, error: %d\n",
			error);
		goto fail3;
	}
	device_init_wakeup(&pdev->dev, wakeup);

	return 0;

 fail3:
	sysfs_remove_group(&pdev->dev.kobj, &hall_attr_group);
 fail2:
	platform_set_drvdata(pdev, NULL);
	wake_lock_destroy(&ddata->flip_wake_lock);
	input_free_device(input);
 fail1:
	kfree(ddata);

	return error;
}

static int hall_remove(struct platform_device *pdev)
{
	struct hall_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;

	pr_info("%s start\n", __func__);
	sysfs_remove_group(&pdev->dev.kobj, &hall_attr_group);

	device_init_wakeup(&pdev->dev, 0);

	input_unregister_device(input);

	wake_lock_destroy(&ddata->flip_wake_lock);

	kfree(ddata);

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id hall_dt_ids[] = {
	{ .compatible = "hall" },
	{ },
};
MODULE_DEVICE_TABLE(of, hall_dt_ids);
#endif /* CONFIG_OF */

#ifdef CONFIG_PM_SLEEP
static int hall_suspend(struct device *dev)
{
	struct hall_drvdata *ddata = dev_get_drvdata(dev);
	struct input_dev *input = ddata->input;
	bool status;

	pr_info("%s start\n", __func__);
	status = gpio_get_value(ddata->pdata->gpio_flip_cover);
	pr_info("[keys] %s flip_status : %d (%s)\n", __func__, status,
			status ? "open" : "close");

	/*
	 * need to be change
	 * Without below one line, it is not able to get the irq
	 * during freezing
	 */
	enable_irq_wake(ddata->pdata->irq_flip_cover);

	if (device_may_wakeup(dev)) {
		enable_irq_wake(ddata->pdata->irq_flip_cover);
	} else {
		mutex_lock(&input->mutex);
		if (input->users)
			hall_close(input);
		mutex_unlock(&input->mutex);
	}

	return 0;
}

static int hall_resume(struct device *dev)
{
	struct hall_drvdata *ddata = dev_get_drvdata(dev);
	struct input_dev *input = ddata->input;
	bool status;

	pr_info("%s start\n", __func__);
	status = gpio_get_value(ddata->pdata->gpio_flip_cover);
	pr_info("[keys] %s flip_status : %d (%s)\n", __func__, status,
			status ? "open" : "close");
	input_sync(input);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(hall_pm_ops, hall_suspend, hall_resume);

static struct platform_driver hall_device_driver = {
	.probe		= hall_probe,
	.remove		= hall_remove,
	.driver		= {
		.name	= "hall",
		.owner	= THIS_MODULE,
		.pm	= &hall_pm_ops,
#if defined(CONFIG_OF)
		.of_match_table	= hall_dt_ids,
#endif /* CONFIG_OF */
	}
};

static int __init hall_init(void)
{
	pr_info("%s start\n", __func__);
	return platform_driver_register(&hall_device_driver);
}

static void __exit hall_exit(void)
{
	pr_info("%s start\n", __func__);
	platform_driver_unregister(&hall_device_driver);
}

late_initcall(hall_init);
module_exit(hall_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Insun Choi <insun77.choi@samsung.com>");
MODULE_DESCRIPTION("Hall IC driver for GPIOs");
MODULE_ALIAS("platform:gpio-keys");
