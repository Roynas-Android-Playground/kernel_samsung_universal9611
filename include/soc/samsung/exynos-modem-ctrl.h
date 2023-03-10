/*
 * Copyright (C) 2015 Samsung Electronics Co.Ltd
 * http://www.samsung.com
 *
 * EXYNOS MODEM CONTROL driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __EXYNOS_MODEM_CTRL_H
#define __EXYNOS_MODEM_CTRL_H

#if defined(CONFIG_SEC_SIPC_MODEM_IF) || defined(CONFIG_SEC_MODEM_IF)
extern int modem_force_crash_exit_ext(void);
extern int modem_send_panic_noti_ext(void);
#else /* CONFIG_SEC_MODEM_IF */
static inline int modem_force_crash_exit_ext(void) { return 0; }
static inline int modem_send_panic_noti_ext(void) { return 0; }
#endif /* CONFIG_SEC_MODEM_IF */

#endif
