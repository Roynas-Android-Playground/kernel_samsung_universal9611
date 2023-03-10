#ifndef _HALL_H
#define _HALL_H
struct hall_platform_data {
	unsigned int rep:1;		/* enable input subsystem auto repeat */
	int gpio_flip_cover;
	int gpio_certify_cover;
#if defined(CONFIG_WACOM_HALL)
	int gpio_wacom_cover;
#endif
	int irq_flip_cover;
	int irq_certify_cover;
#if defined(CONFIG_WACOM_HALL)
	int irq_wacom_cover;
#endif
#if defined(EMULATE_HALL_IC)
	int gpio_flip_cover_key1;
	int gpio_flip_cover_key2;
#endif
};
extern struct device *sec_key;

#endif /* _HALL_H */
