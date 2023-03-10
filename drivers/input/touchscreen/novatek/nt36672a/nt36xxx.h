/*
 * Copyright (C) 2010 - 2017 Novatek, Inc.
 *
 * $Revision: 22429 $
 * $Date: 2018-01-30 19:42:59 +0800 (週二, 30 一月 2018) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#ifndef _LINUX_NVT_TOUCH_H
#define _LINUX_NVT_TOUCH_H

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>

#ifdef CONFIG_SAMSUNG_TUI
#include "stui_inf.h"
#endif

#if defined(CONFIG_DRM_MSM)
#include <linux/notifier.h>
#endif
#if defined(CONFIG_FB)
#include <linux/fb.h>
#endif
#include <linux/mutex.h>
#include <linux/input/sec_cmd.h>
#include <linux/firmware.h>

#include "nt36xxx_mem_map.h"

#define TSP_PATH_EXTERNAL_FW		"/sdcard/firmware/tsp/tsp.bin"
#define TSP_PATH_EXTERNAL_FW_SIGNED		"/sdcard/firmware/tsp/tsp_signed.bin"
#define TSP_PATH_SPU_FW_SIGNED		"/spu/TSP/ffu_tsp.bin"

//---I2C driver info.---
#define NVT_I2C_NAME "nvt-ts"
#define I2C_BLDR_Address 0x01
#define I2C_FW_Address 0x01
#define I2C_HW_Address 0x62

#define TOUCH_MAX_FINGER_NUM	10

#define FW_BIN_SIZE_116KB	118784
#define FW_BIN_SIZE		FW_BIN_SIZE_116KB
#define BLOCK_64KB_NUM		4
#define SIZE_64KB		65536


#define DEFAULT_MAX_HEIGHT		2340
#define DEFAULT_MAX_WIDTH		1080

struct nvt_ts_event_coord {
	u8 status:2;
	u8 reserved_1:1;
	u8 id:5;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
	u8 area;
	u8 pressure_7_0;
} __attribute__ ((packed));

struct nvt_ts_coord {
	u16 x;
	u16 y;
	u16 p;
	u16 p_x;
	u16 p_y;
	u8 area;
	u8 status;
	u8 p_status;
	bool press;
	bool p_press;
	int move_count;
};

struct nvt_ts_platdata {
	int irq_gpio;
	u32 irq_flags;
	u8 x_num;
	u8 y_num;
	u16 abs_x_max;
	u16 abs_y_max;
	u32 area_indicator;
	u32 area_navigation;
	u32 area_edge;
	u8 max_touch_num;
	u32 bringup;
	bool check_fw_project_id;
	bool support_dual_fw;
	const char *firmware_name;
	u32 open_test_spec[2];
	u32 short_test_spec[2];
	int diff_test_frame;
};

struct nvt_ts_data {
	struct i2c_client *client;
	struct nvt_ts_platdata *platdata;
	struct input_dev *input_dev;
	struct nvt_ts_coord coords[TOUCH_MAX_FINGER_NUM];
	u8 touch_count;
#if defined(CONFIG_DRM_MSM)
	struct notifier_block drm_notif;
#endif
#if defined(CONFIG_FB)
	struct notifier_block fb_nb;
#endif

	u32 ic_idx;
	u8 fw_ver_ic[4];
	u8 fw_ver_ic_bar;
	u8 fw_ver_bin[4];
	u8 fw_ver_bin_bar;
	struct mutex lock;
	struct mutex i2c_mutex;
	const struct nvt_ts_mem_map *mmap;
	u8 carrier_system;

	struct kthread_work kwork;
	struct kthread_worker kworker;
	struct delayed_work work_print_info;
	u32 print_info_cnt_open;
	u32 print_info_cnt_release;

#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	int ss_touch_num;
	atomic_t secure_enabled;
	atomic_t secure_pending_irqs;
	struct completion secure_powerdown;
	struct completion secure_interrupt;
#endif
	struct sec_cmd_data sec;
	const struct firmware *fw_entry;
	volatile int power_status;
	u16 sec_function;
	bool touchable_area;
	bool untouchable_area;

	bool check_multi;
	unsigned int multi_count;	/* multi touch count */
	unsigned int comm_err_count;	/* i2c comm error count */
	unsigned int checksum_result;	/* checksum result */
	unsigned int all_finger_count;
};

typedef enum {
	RESET_STATE_INIT	= 0xA0,// IC reset
	RESET_STATE_REK,	// ReK baseline
	RESET_STATE_REK_FINISH,	// baseline is ready
	RESET_STATE_NORMAL_RUN,	// normal run
	RESET_STATE_MAX		= 0xAF
} RST_COMPLETE_STATE;

typedef enum {
	EVENT_MAP_HOST_CMD			= 0x50,
	EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE	= 0x51,
	EVENT_MAP_FUNCT_STATE			= 0x5C,
	EVENT_MAP_RESET_COMPLETE		= 0x60,
	EVENT_MAP_BLOCK_AREA			= 0x62,
	EVENT_MAP_FWINFO			= 0x78,
	EVENT_MAP_PANEL				= 0x8F,
	EVENT_MAP_PROJECTID			= 0x9A,
	EVENT_MAP_SENSITIVITY_DIFF		= 0x9D,
} I2C_EVENT_MAP;


enum {
	POWER_OFF_STATUS = 0,
	POWER_ON_STATUS
};

void nvt_ts_bootloader_reset(struct nvt_ts_data *ts);
int nvt_ts_get_fw_info(struct nvt_ts_data *ts);
int nvt_ts_i2c_write(struct nvt_ts_data *ts, u16 address, u8 *buf, u16 len);
int nvt_ts_i2c_read(struct nvt_ts_data *ts, u16 address, u8 *buf, u16 len);

int nvt_ts_check_fw_reset_state(struct nvt_ts_data *ts, RST_COMPLETE_STATE check_reset_state);
void nvt_ts_sw_reset_idle(struct nvt_ts_data *ts);

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
void nvt_ts_flash_proc_init(struct nvt_ts_data *ts);
void nvt_ts_flash_proc_remove(void);
#endif

#if defined(CONFIG_EXYNOS_DPU20)
extern unsigned int lcdtype;
#endif
#endif /* _LINUX_NVT_TOUCH_H */
