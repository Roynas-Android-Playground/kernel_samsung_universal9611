/*
 * Copyright (C) 2010 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __USBPD_SM5713_H__
#define __USBPD_SM5713_H__

#if defined(CONFIG_CCIC_NOTIFIER)
#include <linux/ccic/ccic_notifier.h>
#endif
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
#include <linux/usb/class-dual-role.h>
#endif

#define USBPD_DEV_NAME	"usbpd-sm5713"

#define SM5713_MAX_NUM_MSG_OBJ (7)


#define SM5713_REG_INT_STATUS1_VBUSPOK			(1<<0)
#define SM5713_REG_INT_STATUS1_VBUSUVLO			(1<<1)
#define SM5713_REG_INT_STATUS1_ADC_DONE			(1<<2)
#define SM5713_REG_INT_STATUS1_ATTACH			(1<<3)
#define SM5713_REG_INT_STATUS1_DETACH			(1<<4)
#define SM5713_REG_INT_STATUS1_WAKEUP			(1<<5)
#define SM5713_REG_INT_STATUS1_DET_DETECT		(1<<6)
#define SM5713_REG_INT_STATUS1_DET_RELEASE		(1<<7)

#define SM5713_REG_INT_STATUS2_RID_INT			(1<<0)
#define SM5713_REG_INT_STATUS2_VCONN_DISCHG		(1<<2)
#define SM5713_REG_INT_STATUS2_AES_DONE			(1<<3)
#define SM5713_REG_INT_STATUS2_SRC_ADV_CHG		(1<<4)
#define SM5713_REG_INT_STATUS2_VBUS_0V			(1<<5)
#define SM5713_REG_INT_STATUS2_DEB_ORI_DETECT	(1<<6)

#define SM5713_REG_INT_STATUS3_WATER				(1<<0)
#define SM5713_REG_INT_STATUS3_CC1_OVP			(1<<1)
#define SM5713_REG_INT_STATUS3_CC2_OVP			(1<<2)
#define SM5713_REG_INT_STATUS3_VCONN_OCP		(1<<3)
#define SM5713_REG_INT_STATUS3_WATER_RLS			(1<<4)
#define SM5713_REG_INT_STATUS3_VBUS_ERR			(1<<5)
#define SM5713_REG_INT_STATUS3_SNKONLY_BAT		(1<<6)
#define SM5713_REG_INT_STATUS3_DEAD_BAT			(1<<7)

#define SM5713_REG_INT_STATUS4_RX_DONE			(1<<0)
#define SM5713_REG_INT_STATUS4_TX_DONE			(1<<1)
#define SM5713_REG_INT_STATUS4_TX_SOP_ERR		(1<<2)
#define SM5713_REG_INT_STATUS4_TX_NSOP_ERR		(1<<3)
#define SM5713_REG_INT_STATUS4_PRL_RST_DONE		(1<<4)
#define SM5713_REG_INT_STATUS4_HRST_RCVED		(1<<5)
#define SM5713_REG_INT_STATUS4_HCRST_DONE		(1<<6)
#define SM5713_REG_INT_STATUS4_TX_DISCARD		(1<<7)

#define SM5713_REG_INT_STATUS5_SBU1_OVP	(1<<0)
#define SM5713_REG_INT_STATUS5_SBU2_OVP	(1<<1)
#define SM5713_REG_INT_STATUS5_CC_ABNORMAL_ST	(1<<7)


/* interrupt for checking message */
#define ENABLED_INT_1	(SM5713_REG_INT_STATUS1_VBUSPOK |\
			SM5713_REG_INT_STATUS1_VBUSUVLO |\
			SM5713_REG_INT_STATUS1_ATTACH |\
			SM5713_REG_INT_STATUS1_DETACH |\
			SM5713_REG_INT_STATUS1_DET_DETECT |\
			SM5713_REG_INT_STATUS1_DET_RELEASE)
#define ENABLED_INT_2	(SM5713_REG_INT_STATUS2_RID_INT)
#define ENABLED_INT_3	(SM5713_REG_INT_STATUS3_WATER |\
			SM5713_REG_INT_STATUS3_CC1_OVP |\
			SM5713_REG_INT_STATUS3_CC2_OVP |\
			SM5713_REG_INT_STATUS3_VCONN_OCP |\
			SM5713_REG_INT_STATUS3_WATER_RLS)
#define ENABLED_INT_4	(SM5713_REG_INT_STATUS4_RX_DONE |\
			SM5713_REG_INT_STATUS4_TX_DONE |\
			SM5713_REG_INT_STATUS4_TX_SOP_ERR |\
			SM5713_REG_INT_STATUS4_TX_NSOP_ERR |\
			SM5713_REG_INT_STATUS4_PRL_RST_DONE |\
			SM5713_REG_INT_STATUS4_HRST_RCVED |\
			SM5713_REG_INT_STATUS4_HCRST_DONE |\
			SM5713_REG_INT_STATUS4_TX_DISCARD)
#define ENABLED_INT_5	(SM5713_REG_INT_STATUS5_SBU1_OVP |\
			SM5713_REG_INT_STATUS5_SBU2_OVP |\
			SM5713_REG_INT_STATUS5_CC_ABNORMAL_ST)

#define ATTACHED_DEV_TYPE3_CHARGER_MUIC 55

#define SM5713_ATTACH_SOURCE		0x01
#define SM5713_ATTACH_SINK			(0x01 << SM5713_ATTACH_SOURCE)
#define SM5713_ATTACH_TYPE		0x07
#define SM5713_REG_CNTL_NOTIFY_RESET_DONE		SM5713_ATTACH_SOURCE
#define SM5713_REG_CNTL_CABLE_RESET_MESSAGE		(SM5713_REG_CNTL_NOTIFY_RESET_DONE << 1)
#define SM5713_REG_CNTL_HARD_RESET_MESSAGE		(SM5713_REG_CNTL_NOTIFY_RESET_DONE << 2)
#define SM5713_REG_CNTL_PROTOCOL_RESET_MESSAGE	(SM5713_REG_CNTL_NOTIFY_RESET_DONE << 3)

#define SM5713_SBU_CORR_CHECK				(1<<6)
#define SM5713_ADC_PATH_SEL_CC1			0x05
#define SM5713_ADC_PATH_SEL_CC2			0x07
#define SM5713_ADC_PATH_SEL_SBU1			0x09
#define SM5713_ADC_PATH_SEL_SBU2			0x0B
#define SM5713_DEAD_RD_ENABLE				(1<<7)

/* For SM5713_REG_TX_REQ_MESSAGE */
#define SM5713_REG_MSG_SEND_TX_SOP_REQ		0x01
#define SM5713_REG_MSG_SEND_TX_SOPP_REQ		0x11
#define SM5713_REG_MSG_SEND_TX_SOPPP_REQ	0x21


enum sm5713_power_role {
	PDIC_SINK,
	PDIC_SOURCE
};

enum sm5713_pdic_rid {
	REG_RID_UNDF = 0x00,
	REG_RID_255K = 0x03,
	REG_RID_301K = 0x04,
	REG_RID_523K = 0x05,
	REG_RID_619K = 0x06,
	REG_RID_OPEN = 0x07,
	REG_RID_MAX  = 0x08,
};


/* SM5713 I2C registers */
enum sm5713_usbpd_reg {
	SM5713_REG_INT1				= 0x01,
	SM5713_REG_INT3				= 0x03,
	SM5713_REG_INT_MASK1			= 0x06,
	SM5713_REG_INT_MASK4			= 0x09,
	SM5713_REG_INT_MASK5			= 0x0A,
	SM5713_REG_STATUS1			= 0x0B,
	SM5713_REG_STATUS3			= 0x0D,
	SM5713_REG_FACTORY			= 0x18,
	SM5713_REG_ADC_CTRL1			= 0x19,
	SM5713_REG_ADC_CTRL2			= 0x1A,
	SM5713_REG_SYS_CNTL			= 0x1B,
	SM5713_REG_CORR_CNTL4		= 0x23,
	SM5713_REG_CC_STATUS			= 0x28,
	SM5713_REG_CC_CNTL1			= 0x29,
	SM5713_REG_CC_CNTL2			= 0x2A,
	SM5713_REG_CC_CNTL3			= 0x2B,
	SM5713_REG_CC_CNTL4			= 0x2C,
	SM5713_REG_CC_CNTL5			= 0x2D,
	SM5713_REG_CC_CNTL6			= 0x2E,
	SM5713_REG_CC_CNTL7			= 0x2F,
	SM5713_REG_PD_CNTL1			= 0x38,
	SM5713_REG_PD_CNTL2			= 0x39,
	SM5713_REG_PD_CNTL4			= 0x3B,
	SM5713_REG_RX_HEADER_00		= 0x40,
	SM5713_REG_RX_HEADER_01		= 0x41,
	SM5713_REG_RX_PAYLOAD_00	= 0x42,
	SM5713_REG_RX_SRC				= 0x5E,
	SM5713_REG_TX_HEADER_00		= 0x60,
	SM5713_REG_TX_HEADER_01		= 0x61,
	SM5713_REG_TX_PAYLOAD_00		= 0x62,
	SM5713_REG_TX_REQ				= 0x7E,
	SM5713_REG_AES_D_SEL			= 0xAF,
	SM5713_REG_AES_RW00			= 0xB0,
	SM5713_REG_AES_CTRL			= 0xC0,
	SM5713_REG_AES_RNCTRL		= 0xC1,
	SM5713_REG_PROBE0				= 0xD0
};
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
typedef enum {
	TYPE_C_DETACH = 0,
	TYPE_C_ATTACH_DFP = 1, /* Host */
	TYPE_C_ATTACH_UFP = 2, /* Device */
	TYPE_C_ATTACH_DRP = 3, /* Dual role */
} CCIC_OTP_MODE;

typedef enum {
	PLUG_CTRL_RP0 = 0,
	PLUG_CTRL_RP80 = 1,
	PLUG_CTRL_RP180 = 2,
	PLUG_CTRL_RP330 = 3
} CCIC_RP_SCR_SEL;

#define DUAL_ROLE_SET_MODE_WAIT_MS 2000
#endif

typedef enum {
	WATER_MODE_OFF = 0,
	WATER_MODE_ON = 1,
} CCIC_WATER_MODE;

typedef enum {
	CLIENT_OFF = 0,
	CLIENT_ON = 1,
} CCIC_DEVICE_REASON;

typedef enum {
	HOST_OFF = 0,
	HOST_ON = 1,
} CCIC_HOST_REASON;

#if defined(CONFIG_CCIC_NOTIFIER)
struct ccic_state_work {
	struct work_struct ccic_work;
	int dest;
	int id;
	int attach;
	int event;
};
#endif

struct sm5713_usbpd_data {
	struct device *dev;
	struct i2c_client *i2c;
#if defined(CONFIG_CCIC_NOTIFIER)
	struct workqueue_struct *ccic_wq;
#endif
	struct mutex _mutex;
	struct mutex poll_mutex;
	struct mutex lpm_mutex;
	int vconn_en;
	int irq_gpio;
	int irq;
	int power_role;
	int vbus_state;
	int vbus_tran_st;
	int data_role;
	int vconn_source;
	msg_header_type header;
	data_obj_type obj[SM5713_MAX_NUM_MSG_OBJ];
	u64 status_reg;
	bool lpm_mode;
	bool detach_valid;
	bool is_cc_abnormal_state;
	bool is_sbu_abnormal_state;
	bool is_factory_mode;
	bool is_water_detect;
	bool is_otg_vboost;
	bool is_otg_reboost;
	int check_msg_pass;
	int rid;
	int is_host;
	int is_client;
	int data_role_dual; /* data_role for dual role swap */
	int power_role_dual; /* power_role for dual role swap */
	int is_attached;
	int reset_done;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	struct dual_role_phy_instance *dual_role;
	struct dual_role_phy_desc *desc;
	struct completion reverse_completion;
	int try_state_change;
	struct delayed_work role_swap_work;
#endif
	struct notifier_block type3_nb;
	struct workqueue_struct *pdic_queue;
	struct delayed_work plug_work;
/*	struct pdic_notifier_struct pdic_notifier; */
};


#if defined(CONFIG_CCIC_NOTIFIER)
extern void sm5713_control_option_command(struct sm5713_usbpd_data *usbpd_data, int cmd);
extern void sm5713_protocol_layer_reset(void *_data);
extern void sm5713_cc_state_hold_on_off(void *_data, int onoff);
extern bool sm5713_check_vbus_state(void *_data);
void select_pdo(int num);
void ccic_event_work(void *data, int dest, int id, int attach, int event);
#endif
void vbus_turn_on_ctrl(struct sm5713_usbpd_data *usbpd_data, bool enable);
void sm5713_src_transition_to_default(void *_data);
void sm5713_src_transition_to_pwr_on(void *_data);
void sm5713_error_recovery(void *_data);
void sm5713_snk_transition_to_default(void *_data);
int sm5713_set_lpm_mode(struct sm5713_usbpd_data *pdic_data);
int sm5713_set_normal_mode(struct sm5713_usbpd_data *pdic_data);

#endif /* __USBPD_SM5713_H__ */
