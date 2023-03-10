#ifndef __S2MU106_USBPD_H__
#define __S2MU106_USBPD_H__

#include <linux/usb/typec/pdic_core.h>
#include <linux/muic/muic.h>

#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/wakelock.h>

/* for header */
#define USBPD_REV_20	(1)
#define USBPD_REV_30	(2)
#define PD_SID		(0xFF00)
#define PD_SID_1	(0xFF01)

#define MAX_CHARGING_VOLT		12000 /* 12V */
#define USBPD_VOLT_UNIT			50 /* 50mV */
#define USBPD_CURRENT_UNIT		10 /* 10mA */
#define USBPD_PPS_VOLT_UNIT			100 /* 50mV */
#define USBPD_PPS_CURRENT_UNIT		50 /* 10mA */
#define USBPD_PPS_RQ_VOLT_UNIT			20 /* 50mV */
#define USBPD_PPS_RQ_CURRENT_UNIT		50 /* 10mA */

#define USBPD_MAX_COUNT_MSG_OBJECT	(8) /* 0..7 */

/* Counter */
#define USBPD_nMessageIDCount		(7)
#define USBPD_nRetryCount		(3)
#define USBPD_nHardResetCount		(4)
#define USBPD_nCapsCount		(16)
#define USBPD_nDiscoverIdentityCount	(20)

/* Timer */
#define tSrcTransition		(25)	/* 25~35 ms */
#define tPSSourceOn		(420)	/* 390~480 ms */
#define tPSSourceOff		(750)	/* 750~960 ms */
#if defined(CONFIG_SEC_FACTORY)
#define tSenderResponse		(1100)	/* for UCT300 */
#else
#define tSenderResponse		(25)	/* 24~30ms */
#endif
#define tSenderResponseSRC	(300)	/* 1000 ms */
#define tSendSourceCap		(10)	/* 1~2 s */
#define tPSHardReset		(22)	/* 25~35 ms */
#define tSinkWaitCap		(2500)	/* 2.1~2.5 s  */
#define tPSTransition		(550)	/* 450~550 ms */
#define tVCONNSourceOn		(100)	/* 24~30 ms */
#define tVDMSenderResponse	(35)	/* 24~30 ms */
#define tVDMWaitModeEntry	(50)	/* 40~50  ms */
#define tVDMWaitModeExit	(50)    /* 40~50  ms */
#define tDiscoverIdentity	(50)	/* 40~50  ms */
#define tSwapSourceStart        (20)	/* 20  ms */
#define tTypeCSinkWaitCap       (600)	/* 310~620 ms */
#define tTypeCSendSourceCap (100) /* 100~200ms */
#define tSrcRecover (880) /* 660~1000ms */
#define tNoResponse (5500) /* 660~1000ms */

/* Protocol States */
typedef enum {
	/* Rx */
	PRL_Rx_Layer_Reset_for_Receive	= 0x11,
	PRL_Rx_Wait_for_PHY_Message	= 0x12,
	PRL_Rx_Send_GoodCRC		= 0x13,
	PRL_Rx_Store_MessageID		= 0x14,
	PRL_Rx_Check_MessageID		= 0x15,

	/* Tx */
	PRL_Tx_PHY_Layer_Reset		= 0x21,
	PRL_Tx_Wait_for_Message_Request	= 0x22,
	PRL_Tx_Layer_Reset_for_Transmit	= 0x23,
	PRL_Tx_Construct_Message	= 0x24,
	PRL_Tx_Wait_for_PHY_Response	= 0x25,
	PRL_Tx_Match_MessageID		= 0x26,
	PRL_Tx_Message_Sent		= 0x27,
	PRL_Tx_Check_RetryCounter	= 0x28,
	PRL_Tx_Transmission_Error	= 0x29,
	PRL_Tx_Discard_Message		= 0x2A,
} protocol_state;

/* Policy Engine States */
typedef enum {
	/* Source */
	PE_SRC_Startup			= 0x30,
	PE_SRC_Discovery		= 0x31,
	PE_SRC_Send_Capabilities	= 0x32,
	PE_SRC_Negotiate_Capability	= 0x33,
	PE_SRC_Transition_Supply	= 0x34,
	PE_SRC_Ready			= 0x35,
	PE_SRC_Disabled			= 0x36,
	PE_SRC_Capability_Response	= 0x37,
	PE_SRC_Hard_Reset		= 0x38,
	PE_SRC_Hard_Reset_Received	= 0x39,
	PE_SRC_Transition_to_default	= 0x3A,
	PE_SRC_Give_Source_Cap		= 0x3B,
	PE_SRC_Get_Sink_Cap		= 0x3C,
	PE_SRC_Wait_New_Capabilities	= 0x3D,

	/* Sink */
	PE_SNK_Startup			= 0x40,
	PE_SNK_Discovery		= 0x41,
	PE_SNK_Wait_for_Capabilities	= 0x42,
	PE_SNK_Evaluate_Capability	= 0x43,
	PE_SNK_Select_Capability	= 0x44,
	PE_SNK_Transition_Sink		= 0x45,
	PE_SNK_Ready			= 0x46,
	PE_SNK_Hard_Reset		= 0x47,
	PE_SNK_Transition_to_default	= 0x48,
	PE_SNK_Give_Sink_Cap		= 0x49,
	PE_SNK_Get_Source_Cap		= 0x4A,

	/* Source Soft Reset */
	PE_SRC_Send_Soft_Reset		= 0x50,
	PE_SRC_Soft_Reset		= 0x51,

	/* Sink Soft Reset */
	PE_SNK_Send_Soft_Reset		= 0x60,
	PE_SNK_Soft_Reset		= 0x61,

	/* UFP VDM */
	PE_UFP_VDM_Get_Identity		= 0x70,
	PE_UFP_VDM_Send_Identity	= 0x71,
	PE_UFP_VDM_Get_Identity_NAK	= 0x72,
	PE_UFP_VDM_Get_SVIDs		= 0x73,
	PE_UFP_VDM_Send_SVIDs		= 0x74,
	PE_UFP_VDM_Get_SVIDs_NAK	= 0x75,
	PE_UFP_VDM_Get_Modes		= 0x76,
	PE_UFP_VDM_Send_Modes		= 0x77,
	PE_UFP_VDM_Get_Modes_NAK	= 0x78,
	PE_UFP_VDM_Evaluate_Mode_Entry	= 0x79,
	PE_UFP_VDM_Mode_Entry_ACK	= 0x7A,
	PE_UFP_VDM_Mode_Entry_NAK	= 0x7B,
	PE_UFP_VDM_Mode_Exit		= 0x7C,
	PE_UFP_VDM_Mode_Exit_ACK	= 0x7D,
	PE_UFP_VDM_Mode_Exit_NAK	= 0x7E,
	PE_UFP_VDM_Attention_Request	= 0x7F,
	PE_UFP_VDM_Evaluate_Status	= 0x80,
	PE_UFP_VDM_Status_ACK		= 0x81,
	PE_UFP_VDM_Status_NAK		= 0x82,
	PE_UFP_VDM_Evaluate_Configure	= 0x83,
	PE_UFP_VDM_Configure_ACK	= 0x84,
	PE_UFP_VDM_Configure_NAK	= 0x85,

	/* DFP VDM */
	PE_DFP_VDM_Identity_Request		= 0x8A,
	PE_DFP_VDM_Identity_ACKed		= 0x8B,
	PE_DFP_VDM_Identity_NAKed		= 0x8C,
	PE_DFP_VDM_SVIDs_Request		= 0x8D,
	PE_DFP_VDM_SVIDs_ACKed			= 0x8E,
	PE_DFP_VDM_SVIDs_NAKed			= 0x8F,
	PE_DFP_VDM_Modes_Request		= 0x90,
	PE_DFP_VDM_Modes_ACKed			= 0x91,
	PE_DFP_VDM_Modes_NAKed			= 0x92,
	PE_DFP_VDM_Mode_Entry_Request		= 0x93,
	PE_DFP_VDM_Mode_Entry_ACKed		= 0x94,
	PE_DFP_VDM_Mode_Entry_NAKed		= 0x95,
	PE_DFP_VDM_Mode_Exit_Request		= 0x96,
	PE_DFP_VDM_Mode_Exit_ACKed		= 0x97,
	PE_DFP_VDM_Mode_Exit_NAKed		= 0x98,
	PE_DFP_VDM_Status_Update		= 0x99,
	PE_DFP_VDM_Status_Update_ACKed		= 0x9A,
	PE_DFP_VDM_Status_Update_NAKed		= 0x9B,
	PE_DFP_VDM_DisplayPort_Configure	= 0x9C,
	PE_DFP_VDM_DisplayPort_Configure_ACKed	= 0x9D,
	PE_DFP_VDM_DisplayPort_Configure_NAKed	= 0x9E,
	PE_DFP_VDM_Attention_Request		= 0x9F,

	/* Power Role Swap */
	PE_PRS_SRC_SNK_Reject_PR_Swap	= 0xA0,
	PE_PRS_SRC_SNK_Evaluate_Swap	= 0xA1,
	PE_PRS_SRC_SNK_Send_Swap	= 0xA2,
	PE_PRS_SRC_SNK_Accept_Swap	= 0xA3,
	PE_PRS_SRC_SNK_Transition_off	= 0xA4,
	PE_PRS_SRC_SNK_Assert_Rd	= 0xA5,
	PE_PRS_SRC_SNK_Wait_Source_on	= 0xA6,
	PE_PRS_SNK_SRC_Reject_Swap	= 0xA7,
	PE_PRS_SNK_SRC_Evaluate_Swap	= 0xA8,
	PE_PRS_SNK_SRC_Send_Swap	= 0xA9,
	PE_PRS_SNK_SRC_Accept_Swap	= 0xAA,
	PE_PRS_SNK_SRC_Transition_off	= 0xAB,
	PE_PRS_SNK_SRC_Assert_Rp	= 0xAC,
	PE_PRS_SNK_SRC_Source_on	= 0xAD,

	/* Data Role Swap */
	PE_DRS_DFP_UFP_Evaluate_DR_Swap	= 0xAE,
	PE_DRS_DFP_UFP_Accept_DR_Swap	= 0xAF,
	PE_DRS_DFP_UFP_Change_to_UFP	= 0xB0,
	PE_DRS_DFP_UFP_Send_DR_Swap	= 0xB1,
	PE_DRS_DFP_UFP_Reject_DR_Swap	= 0xB2,
	PE_DRS_UFP_DFP_Evaluate_DR_Swap	= 0xB3,
	PE_DRS_UFP_DFP_Accept_DR_Swap	= 0xB4,
	PE_DRS_UFP_DFP_Change_to_DFP	= 0xB5,
	PE_DRS_UFP_DFP_Send_DR_Swap	= 0xB6,
	PE_DRS_UFP_DFP_Reject_DR_Swap	= 0xB7,
	PE_DRS_Evaluate_Port		= 0xB8,
	PE_DRS_Evaluate_Send_Port	= 0xB9,

	/* Vconn Source Swap */
	PE_VCS_Evaluate_Swap		= 0xC0,
	PE_VCS_Accept_Swap		= 0xC1,
	PE_VCS_Wait_for_VCONN		= 0xC2,
	PE_VCS_Turn_Off_VCONN		= 0xC3,
	PE_VCS_Turn_On_VCONN		= 0xC4,
	PE_VCS_Send_PS_RDY		= 0xC5,
	PE_VCS_Send_Swap		= 0xC6,
	PE_VCS_Reject_VCONN_Swap	= 0xC7,

	/* UVDM Message */
	PE_DFP_UVDM_Send_Message	= 0xD0,
	PE_DFP_UVDM_Receive_Message	= 0xD1,

	/* Dual Role */
	PE_DR_SRC_Get_Source_Cap = 0xE0,
	PE_DR_SRC_Give_Sink_Cap = 0xE1,
	PE_DR_SNK_Get_Sink_Cap  = 0xE2,
	PE_DR_SNK_Give_Source_Cap = 0xE3,
	PE_DR_SRC_Get_Source_Cap_Ext	= 0xE4,
	PE_DR_SNK_Give_Source_Cap_Ext	= 0xE5,

	/* Bist Mode */
	PE_BIST_Carrier_Mode = 0xE6,

	/* for PD 3.0 */
	PE_SRC_Send_Not_Supported	= 0xE7,
	PE_SRC_Not_Supported_Received	= 0xE8,
	PE_SRC_Chunk_Received	= 0xE9,

	PE_SNK_Send_Not_Supported	= 0xEA,
	PE_SNK_Not_Supported_Received	= 0xEB,
	PE_SNK_Chunk_Received	= 0xEC,

	PE_SRC_Send_Source_Alert	= 0xED,
	PE_SNK_Source_Alert_Received	= 0xEE,
	PE_SNK_Send_Sink_Alert	= 0xEF,
	PE_SRC_Sink_Alert_Received	= 0xF0,

	PE_SNK_Get_Source_Cap_Ext	= 0xF1,
	PE_SRC_Give_Source_Cap_Ext	= 0xF2,
	PE_SNK_Get_Source_Status	= 0xF3,
	PE_SRC_Give_Source_Status	= 0xF4,
	PE_SRC_Get_Sink_Status		= 0xF5,
	PE_SNK_Give_Sink_Status		= 0xF6,
	PE_SNK_Get_PPS_Status		= 0xF7,
	PE_SRC_Give_PPS_Status		= 0xF8,

	PE_Get_Battery_Cap	= 0xF9,
	PE_Give_Battery_Cap	= 0xFA,
	PE_Get_Battery_Status	= 0xFB,
	PE_Give_Battery_Status	= 0xFC,
	PE_Get_Manufacturer_Info	= 0xFD,
	PE_Give_Manufacturer_Info	= 0xFE,

	PE_Get_Country_Codes	= 0x10,
	PE_Give_Country_Codes	= 0x11,
	PE_Get_Country_Info		= 0x12,
	PE_Give_Country_Info	= 0x13,
	PE_Send_Security_Request	= 0x14,
	PE_Send_Security_Response	= 0x15,
	PE_Security_Response_Received	= 0x16,
	PE_Send_Firmware_Update_Request	= 0x17,
	PE_Send_Firmware_Update_Response	= 0x18,
	PE_Firmware_Update_Response_Received	= 0x19,

	/* for fast role swap */
	PE_FRS_SRC_SNK_Evaluate_Swap	= 0x1A,
	PE_FRS_SRC_SNK_Accept_Swap		= 0x1B,
	PE_FRS_SRC_SNK_Transition_to_off = 0x1C,
	PE_FRS_SRC_SNK_Assert_Rd		= 0x1D,
	PE_FRS_SRC_SNK_Wait_Source_on	= 0x1E,

	PE_FRS_SNK_SRC_Start_AMS		= 0x1F,
	PE_FRS_SNK_SRC_Send_Swap		= 0x20,
	PE_FRS_SNK_SRC_Transition_to_off = 0x21,
	PE_FRS_SNK_SRC_Vbus_Applied		= 0x22,
	PE_FRS_SNK_SRC_Assert_Rp		= 0x23,
	PE_FRS_SNK_SRC_Source_on		= 0x24,

	Error_Recovery			= 0xFF
} policy_state;

typedef enum usbpd_manager_command {
	MANAGER_REQ_GET_SNKCAP			= 1,
	MANAGER_REQ_GOTOMIN			= 1 << 2,
	MANAGER_REQ_SRCCAP_CHANGE		= 1 << 3,
	MANAGER_REQ_PR_SWAP			= 1 << 4,
	MANAGER_REQ_DR_SWAP			= 1 << 5,
	MANAGER_REQ_VCONN_SWAP			= 1 << 6,
	MANAGER_REQ_VDM_DISCOVER_IDENTITY	= 1 << 7,
	MANAGER_REQ_VDM_DISCOVER_SVID		= 1 << 8,
	MANAGER_REQ_VDM_DISCOVER_MODE		= 1 << 9,
	MANAGER_REQ_VDM_ENTER_MODE		= 1 << 10,
	MANAGER_REQ_VDM_EXIT_MODE		= 1 << 11,
	MANAGER_REQ_VDM_ATTENTION		= 1 << 12,
	MANAGER_REQ_VDM_STATUS_UPDATE		= 1 << 13,
	MANAGER_REQ_VDM_DisplayPort_Configure	= 1 << 14,
	MANAGER_REQ_NEW_POWER_SRC		= 1 << 15,
	MANAGER_REQ_UVDM_SEND_MESSAGE		= 1 << 16,
	MANAGER_REQ_UVDM_RECEIVE_MESSAGE		= 1 << 17,
	MANAGER_REQ_GET_SRC_CAP		= 1 << 18,
} usbpd_manager_command_type;

typedef enum usbpd_manager_event {
	MANAGER_DISCOVER_IDENTITY_ACKED	= 0,
	MANAGER_DISCOVER_IDENTITY_NAKED	= 1,
	MANAGER_DISCOVER_SVID_ACKED	= 2,
	MANAGER_DISCOVER_SVID_NAKED	= 3,
	MANAGER_DISCOVER_MODE_ACKED	= 4,
	MANAGER_DISCOVER_MODE_NAKED	= 5,
	MANAGER_ENTER_MODE_ACKED	= 6,
	MANAGER_ENTER_MODE_NAKED	= 7,
	MANAGER_EXIT_MODE_ACKED		= 8,
	MANAGER_EXIT_MODE_NAKED		= 9,
	MANAGER_ATTENTION_REQUEST	= 10,
	MANAGER_STATUS_UPDATE_ACKED	= 11,
	MANAGER_STATUS_UPDATE_NAKED	= 12,
	MANAGER_DisplayPort_Configure_ACKED	= 13,
	MANAGER_DisplayPort_Configure_NACKED	= 14,
	MANAGER_NEW_POWER_SRC		= 15,
	MANAGER_UVDM_SEND_MESSAGE		= 16,
	MANAGER_UVDM_RECEIVE_MESSAGE		= 17,
	MANAGER_START_DISCOVER_IDENTITY	= 18,
	MANAGER_GET_SRC_CAP			= 19,
	MANAGER_SEND_PR_SWAP	= 20,
	MANAGER_SEND_DR_SWAP	= 21,
} usbpd_manager_event_type;

enum usbpd_msg_status {
	MSG_GOODCRC	= 0,
	MSG_ACCEPT	= 1,
	MSG_PSRDY	= 2,
	MSG_REQUEST	= 3,
	MSG_REJECT	= 4,
	MSG_WAIT	= 5,
	MSG_ERROR	= 6,
	MSG_PING	= 7,
	MSG_GET_SNK_CAP = 8,
	MSG_GET_SRC_CAP = 9,
	MSG_SNK_CAP     = 10,
	MSG_SRC_CAP     = 11,
	MSG_PR_SWAP	= 12,
	MSG_DR_SWAP	= 13,
	MSG_VCONN_SWAP	= 14,
	VDM_DISCOVER_IDENTITY	= 15,
	VDM_DISCOVER_SVID	= 16,
	VDM_DISCOVER_MODE	= 17,
	VDM_ENTER_MODE		= 18,
	VDM_EXIT_MODE		= 19,
	VDM_ATTENTION		= 20,
	VDM_DP_STATUS_UPDATE	= 21,
	VDM_DP_CONFIGURE	= 22,
	MSG_SOFTRESET		= 23,
	PLUG_DETACH		= 24,
	PLUG_ATTACH		= 25,
	MSG_HARDRESET		= 26,
	CC_DETECT		= 27,
	UVDM_MSG		= 28,
	MSG_PASS		= 29,
	MSG_RID			= 30,
	MSG_BIST		= 31,

	/* PD 3.0 : Control Message */
	MSG_NOT_SUPPORTED = 32,
	MSG_GET_SOURCE_CAP_EXTENDED = 33,
	MSG_GET_STATUS = 34,
	MSG_FR_SWAP = 35,
	MSG_GET_PPS_STATUS = 36,
	MSG_GET_COUNTRY_CODES = 37,
	MSG_GET_SINK_CAP_EXTENDED = 38,

	/* PD 3.0 : Data Message */
	MSG_BATTERY_STATUS = 39,
	MSG_ALERT = 40,
	MSG_GET_COUNTRY_INFO = 41,

	/* PD 3.0 : Extended Message */
	MSG_SOURCE_CAPABILITIES_EXTENDED = 42,
	MSG_STATUS = 43,
	MSG_GET_BATTERY_CAP = 44,
	MSG_GET_BATTERY_STATUS = 45,
	MSG_BATTERY_CAPABILITIES = 46,
	MSG_GET_MANUFACTURER_INFO = 47,
	MSG_MANUFACTURER_INFO = 48,
	MSG_SECURITY_REQUEST = 49,
	MSG_SECURITY_RESPONSE = 50,
	MSG_FIRMWARE_UPDATE_REQUEST = 51,
	MSG_FIRMWARE_UPDATE_RESPONSE = 52,
	MSG_PPS_STATUS = 53,
	MSG_COUNTRY_INFO = 54,
	MSG_COUNTRY_CODES = 55,
	MSG_SINK_CAPABILITIES_EXTENDED = 56,

	MSG_NONE = 61,
	/* Reserved */
	MSG_RESERVED = 62,
};

/* Timer */
enum usbpd_timer_id {
	DISCOVER_IDENTITY_TIMER   = 1,
	HARD_RESET_COMPLETE_TIMER = 2,
	NO_RESPONSE_TIMER         = 3,
	PS_HARD_RESET_TIMER       = 4,
	PS_SOURCE_OFF_TIMER       = 5,
	PS_SOURCE_ON_TIMER        = 6,
	PS_TRANSITION_TIMER       = 7,
	SENDER_RESPONSE_TIMER     = 8,
	SINK_ACTIVITY_TIMER       = 9,
	SINK_REQUEST_TIMER        = 10,
	SINK_WAIT_CAP_TIMER       = 11,
	SOURCE_ACTIVITY_TIMER     = 12,
	SOURCE_CAPABILITY_TIMER   = 13,
	SWAP_RECOVERY_TIMER       = 14,
	SWAP_SOURCE_START_TIMER   = 15,
	VCONN_ON_TIMER            = 16,
	VDM_MODE_ENTRY_TIMER      = 17,
	VDM_MODE_EXIT_TIMER       = 18,
	VDM_RESPONSE_TIMER        = 19,
	USBPD_TIMER_MAX_COUNT
};

enum usbpd_protocol_status {
	DEFAULT_PROTOCOL_NONE	= 0,
	MESSAGE_SENT		= 1,
	TRANSMISSION_ERROR	= 2
};

enum usbpd_policy_informed {
	DEFAULT_POLICY_NONE	= 0,
	HARDRESET_RECEIVED	= 1,
	SOFTRESET_RECEIVED	= 2,
	PLUG_EVENT		= 3,
	PLUG_ATTACHED		= 4,
	PLUG_DETACHED		= 5,
};

typedef enum {
	PLUG_CTRL_RP0 = 0,
	PLUG_CTRL_RP80 = 1,
	PLUG_CTRL_RP180 = 2,
	PLUG_CTRL_RP330 = 3
} CCIC_RP_SCR_SEL;

typedef enum {
	PPS_DISABLE = 0,
	PPS_ENABLE = 1,
} PPS_ENABLE_SEL;

enum  {
	S2MU106_USBPD_IP,
	S2MU107_USBPD_IP,
};

typedef union {
	u32 object;
	u16 word[2];
	u8  byte[4];

	struct {
		unsigned data_size:9;
		unsigned :1;
		unsigned request_chunk:1;
		unsigned chunk_number:4;
		unsigned chunked:1;
		unsigned data : 16;
	} extended_msg_header_type;

	struct {
		unsigned:30;
		unsigned supply_type:2;
	} power_data_obj_supply_type;

	struct {
		unsigned max_current:10;        /* 10mA units */
		unsigned voltage:10;            /* 50mV units */
		unsigned peak_current:2;
		unsigned rsvd:2;
		unsigned unchunked_extended_message_supported:1;
		unsigned data_role_swap:1;
		unsigned usb_comm_capable:1;
		unsigned externally_powered:1;
		unsigned usb_suspend_support:1;
		unsigned dual_role_power:1;
		unsigned supply:2;
	} power_data_obj;

	struct {
		unsigned max_current:10;	/* 10mA units */
		unsigned min_voltage:10;	/* 50mV units */
		unsigned max_voltage:10;	/* 50mV units */
		unsigned supply_type:2;
	} power_data_obj_variable;

	struct {
		unsigned max_power:10;		/* 250mW units */
		unsigned min_voltage:10;	/* 50mV units  */
		unsigned max_voltage:10;	/* 50mV units  */
		unsigned supply_type:2;
	} power_data_obj_battery;

	struct {
		unsigned max_current:7;		/* 50mA units */
		unsigned :1;
		unsigned min_voltage:8;		/* 100mV units  */
		unsigned :1;
		unsigned max_voltage:8;		/* 100mV units  */
		unsigned :2;
		unsigned pps_power_limited:1;
		unsigned supply_type:2;
		unsigned augmented_power_data:2;
	} power_data_obj_pps;

	struct {
		unsigned op_current:10;	/* 10mA units */
		unsigned voltage:10;	/* 50mV units */
		unsigned rsvd:3;
		unsigned fast_role_swap:2;
		unsigned data_role_swap:1;
		unsigned usb_comm_capable:1;
		unsigned externally_powered:1;
		unsigned higher_capability:1;
		unsigned dual_role_power:1;
		unsigned supply_type:2;
	} power_data_obj_sink;

	struct {
		unsigned min_current:10;	/* 10mA units */
		unsigned op_current:10;		/* 10mA units */
		unsigned rsvd:3;
		unsigned unchunked_supported:1;
		unsigned no_usb_suspend:1;
		unsigned usb_comm_capable:1;
		unsigned capability_mismatch:1;
		unsigned give_back:1;
		unsigned object_position:3;
		unsigned:1;
	} request_data_object;

	struct {
		unsigned max_power:10;		/* 250mW units */
		unsigned op_power:10;		/* 250mW units */
		unsigned rsvd:3;
		unsigned unchunked_supported:1;
		unsigned no_usb_suspend:1;
		unsigned usb_comm_capable:1;
		unsigned capability_mismatch:1;
		unsigned give_back:1;
		unsigned object_position:3;
		unsigned:1;
	} request_data_object_battery;

	struct {
		unsigned op_current:7;		/* 50mA units */
		unsigned :2;
		unsigned output_voltage:11; /* 20mV units */
		unsigned :3;
		unsigned unchunked_supported:1;
		unsigned no_usb_suspend:1;
		unsigned usb_comm_capable:1;
		unsigned capability_mismatch:1;
		unsigned :1;
		unsigned object_position:3;
		unsigned:1;
	} request_data_object_pps;

	struct {
		unsigned :28;
		unsigned mode:4;
	} bist_type;

	struct {
		unsigned vendor_defined:15;
		unsigned vdm_type:1;
		unsigned vendor_id:16;
	} unstructured_vdm;

	struct {
		unsigned data:8;
		unsigned total_number_of_uvdm_set:4;
		unsigned rsvd:1;
		unsigned cmd_type:2;
		unsigned data_type:1;
		unsigned pid:16;
	} sec_uvdm_header;

	struct {
		unsigned command:5;
		unsigned reserved:1;
		unsigned command_type:2;
		unsigned obj_pos:3;
		unsigned rsvd:2;
		unsigned version:2;
		unsigned vdm_type:1;
		unsigned svid:16;
	} structured_vdm;

	struct {
		unsigned USB_Vendor_ID:16;
		unsigned rsvd:7;
		unsigned product_type_dfp:3;
		unsigned modal_op_supported:1;
		unsigned Product_Type:3;
		unsigned Data_Capable_USB_Device:1;
		unsigned Data_Capable_USB_Host:1;
	} id_header_vdo;

	struct{
		unsigned xid:32;
	} cert_stat_vdo;

	struct {
		unsigned Device_Version:16;
		unsigned USB_Product_ID:16;
	} product_vdo;

	struct {
		unsigned port_capability:2;
		unsigned displayport_protocol:4;
		unsigned receptacle_indication:1;
		unsigned usb_r2_signaling:1;
		unsigned dfp_d_pin_assignments:8;
		unsigned ufp_d_pin_assignments:8;
		unsigned rsvd:8;
	} displayport_capabilities;

	struct {
		unsigned port_connected:2;
		unsigned power_low:1;
		unsigned enabled:1;
		unsigned multi_function_preferred:1;
		unsigned usb_configuration_request:1;
		unsigned exit_displayport_mode_request:1;
		unsigned hpd_state:1;
		unsigned irq_hpd:1;
		unsigned rsvd:23;
	} displayport_status;

	struct {
		unsigned select_configuration:2;
		unsigned displayport_protocol:4;
		unsigned rsvd1:2;
		unsigned ufp_u_pin_assignment:8;
		unsigned rsvd2:16;
	} displayport_configurations;

	struct {
		unsigned svid_1:16;
		unsigned svid_0:16;
	} vdm_svid;

	struct {
		unsigned :8;
		unsigned battery_info:8;
		unsigned battery_present_capacity:16;
	} battery_status;

	struct {
		unsigned :16;
		unsigned hot_swappable_batteries:4;
		unsigned fixed_batteries:4;
		unsigned type_of_alert:8;
	} alert_type;

	struct {
		unsigned :16;
		unsigned second_character:8;
		unsigned first_character:8;
	} get_country_info;

	struct {
		unsigned present_batt_input:8;
		unsigned :1;
		unsigned ocp_event:1;
		unsigned otp_event:1;
		unsigned ovp_event:1;
		unsigned cf_cv_mode:1;
		unsigned :3;
		unsigned temp_status:8;
		unsigned :8;
	} status_type;

	struct{
		unsigned PID:16;
		unsigned Manufacture:16;
	} Manufacturer_Info;
} data_obj_type;

typedef struct usbpd_phy_ops {
	/*    1st param should be 'usbpd_data *'    */
	int    (*tx_msg)(void *, msg_header_type *, data_obj_type *);
	int    (*rx_msg)(void *, msg_header_type *, data_obj_type *);
	int    (*hard_reset)(void *);
	void    (*soft_reset)(void *);
	int    (*set_power_role)(void *, int);
	int    (*get_power_role)(void *, int *);
	int    (*set_data_role)(void *, int);
	int    (*get_data_role)(void *, int *);
	int    (*set_vconn_source)(void *, int);
	int    (*get_vconn_source)(void *, int *);
	int    (*set_check_msg_pass)(void *, int);
	unsigned   (*get_status)(void *, u64);
	bool   (*poll_status)(void *);
	void   (*driver_reset)(void *);
	int    (*set_otg_control)(void *, int);
	void    (*get_vbus_short_check)(void *, bool *);
	void    (*pd_vbus_short_check)(void *);
	int    (*set_cc_control)(void *, int);
	void    (*pr_swap)(void *, int);
	int    (*vbus_on_check)(void *);
	int		(*get_side_check)(void *_data);
	int    (*set_rp_control)(void *, int);
	int    (*set_chg_lv_mode)(void *, int);
#if defined(CONFIG_TYPEC)
	void	(*set_pwr_opmode)(void *, int);
#endif
	int    (*cc_instead_of_vbus)(void *, int);
	int    (*op_mode_clear)(void *);
#if defined(CONFIG_PDIC_PD30)
#if defined(CONFIG_CCIC_AUTO_PPS)
	int    (*get_pps_voltage)(void *);
	void    (*send_hard_reset_dc)(void *);
	void    (*force_pps_disable)(void *);
	void    (*send_psrdy)(void *);
	int    (*pps_enable)(void *, int);
	int    (*get_pps_enable)(void *, int *);
#endif
	void    (*send_ocp_info)(void *);
#endif
	void    (*send_pd_info)(void *, int);
} usbpd_phy_ops_type;

struct policy_data {
	policy_state		state;
	msg_header_type         tx_msg_header;
	msg_header_type		rx_msg_header;
	data_obj_type           tx_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
	data_obj_type		rx_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
	bool			rx_hardreset;
	bool			rx_softreset;
	bool			plug;
	bool			plug_valid;
	bool			modal_operation;
	bool			abnormal_state;
	bool			sink_cap_received;
	bool			send_sink_cap;
	bool			txhardresetflag;
	bool			pd_support;
	bool			pps_enable;
	bool			check_ps_ready_retry;
	bool			send_ocp;
};

struct protocol_data {
	protocol_state		state;
	unsigned		stored_message_id;
	msg_header_type		msg_header;
	data_obj_type		data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
	unsigned		status;
};

struct usbpd_counter {
	unsigned	retry_counter;
	unsigned	message_id_counter;
	unsigned	caps_counter;
	unsigned	hard_reset_counter;
	unsigned	discover_identity_counter;
	unsigned	swap_hard_reset_counter;
};

struct usbpd_manager_data {
	usbpd_manager_command_type cmd;  /* request to policy engine */
	usbpd_manager_event_type   event;    /* policy engine infromed */

	msg_header_type		uvdm_msg_header;
	data_obj_type		uvdm_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];

	int alt_sended;
	int vdm_en;
	/* request */
	int	max_power;
	int	op_power;
	int	max_current;
	int	op_current;
	int	min_current;
	bool	giveback;
	bool	usb_com_capable;
	bool	no_usb_suspend;

	/* source */
	int source_max_volt;
	int source_min_volt;
	int source_max_power;

	/* sink */
	int sink_max_volt;
	int sink_min_volt;
	int sink_max_power;

	/* sink cap */
	int sink_cap_max_volt;

	/* power role swap*/
	bool power_role_swap;
	/* data role swap*/
	bool data_role_swap;
	bool vconn_source_swap;
	bool vbus_short;

	bool flash_mode;
	int prev_available_pdo;
	int prev_current_pdo;
	bool ps_rdy;

	bool is_samsung_accessory_enter_mode;
	bool uvdm_first_req;
	bool uvdm_dir;
	struct completion uvdm_out_wait;
	struct completion uvdm_in_wait;

	uint16_t Vendor_ID;
	uint16_t Product_ID;
	uint16_t Device_Version;
	int acc_type;
	uint16_t SVID_0;
	uint16_t SVID_1;
	uint16_t Standard_Vendor_ID;

	struct mutex vdm_mutex;
	struct mutex pdo_mutex;

	struct usbpd_data *pd_data;
	struct delayed_work	acc_detach_handler;
	struct delayed_work select_pdo_handler;
	struct delayed_work start_discover_msg_handler;
	muic_attached_dev_t	attached_dev;

	int pd_attached;
};

struct usbpd_data {
	struct device		*dev;
	void			*phy_driver_data;
	struct usbpd_counter	counter;
	struct hrtimer		timers[USBPD_TIMER_MAX_COUNT];
	unsigned                expired_timers;
	usbpd_phy_ops_type	phy_ops;
	struct protocol_data	protocol_tx;
	struct protocol_data	protocol_rx;
	struct policy_data	policy;
	msg_header_type		source_msg_header;
	data_obj_type           source_data_obj;
	msg_header_type		sink_msg_header;
	data_obj_type           sink_data_obj[2];
	data_obj_type		source_request_obj;
	struct usbpd_manager_data	manager;
	struct workqueue_struct *policy_wqueue;
	struct work_struct	worker;
	struct completion	msg_arrived;
	unsigned                wait_for_msg_arrived;
	int					id_matched;

	int					msg_id;
	int					alert_msg_id;
	int					specification_revision;
	struct mutex		accept_mutex;

	struct timeval		time1;
	struct timeval		time2;
	struct timeval		check_time;

	struct wake_lock	policy_wake;
	int					ip_num;
};

typedef struct {
	uint16_t	VID;
	uint16_t	PID;
	uint32_t	XID;
	uint8_t		FW_Version;
	uint8_t		HW_Version;
	uint8_t		Voltage_Regulation;
	uint8_t		Holdup_Time;
	uint8_t		Compliance;
	uint8_t		Touch_Current;
	uint16_t	Peak_Current1;
	uint16_t	Peak_Current2;
	uint16_t	Peak_Current3;
	uint8_t		Touch_Temp;
	uint8_t		Source_Inputs;
	uint8_t		Number_of_Batteries;
	uint8_t		Source_PDP;
} Source_Capabilities_Extended_Data_Block_Type;

typedef struct {
	uint16_t	VID;
	uint16_t	PID;
	uint16_t	Battery_Design_Capacity;
	uint16_t	Last_Full_Charge;
	uint8_t		Battery_Type;
} Battery_Capabilities;

typedef union {
	u8  byte;

	struct {
		unsigned invalid_battery_ref:1;
		unsigned :7;
	};
} battery_type;

typedef struct {
	uint16_t	output_voltage;
	uint16_t	output_current;
	uint8_t		real_time_flags;
} pps_status;

typedef union {
	u8  byte;
	struct {
		unsigned :1;
		unsigned ptf:2;
		unsigned omf:1;
		unsigned :4;
	};
} real_time_flags_type;

enum uvdm_res_type {
	RES_INIT = 0,
	RES_ACK,
	RES_NAK,
	RES_BUSY,
};

enum uvdm_rx_type {
	RX_ACK = 0,
	RX_NAK,
	RX_BUSY,
};

typedef enum {
	POWER_TYPE_FIXED = 0,
	POWER_TYPE_BATTERY,
	POWER_TYPE_VARIABLE,
	POWER_TYPE_PPS,
} power_supply_type;

typedef enum {
	SOP_TYPE_SOP,
	SOP_TYPE_SOP1,
	SOP_TYPE_SOP2,
	SOP_TYPE_SOP1_DEBUG,
	SOP_TYPE_SOP2_DEBUG
} sop_type;

enum usbpd_control_msg_type {
	USBPD_GoodCRC        = 1,
	USBPD_GotoMin        = 2,
	USBPD_Accept         = 3,
	USBPD_Reject         = 4,
	USBPD_Ping           = 5,
	USBPD_PS_RDY         = 6,
	USBPD_Get_Source_Cap = 7,
	USBPD_Get_Sink_Cap   = 8,
	USBPD_DR_Swap        = 9,
	USBPD_PR_Swap        = 10,
	USBPD_VCONN_Swap     = 11,
	USBPD_Wait           = 12,
	USBPD_Soft_Reset     = 13,
	USBPD_Not_Supported  = 16,
	USBPD_Get_Source_Cap_Extended  = 17,
	USBPD_Get_Status	 = 18,
	USBPD_FR_Swap		 = 19,
	USBPD_Get_PPS_Status = 20,
	USBPD_Get_Country_Codes		   = 21,
	USBPD_Get_Sink_Cap_Extended	   = 22,
	USBPD_UVDM_MSG       = 23,
};

enum usbpd_check_msg_pass {
	NONE_CHECK_MSG_PASS,
	CHECK_MSG_PASS,
};

enum usbpd_spec_revision {
	USBPD_PD2_0 = 1,
	USBPD_PD3_0 = 2,
};

enum usbpd_power_role_swap {
	USBPD_SINK_OFF,
	USBPD_SINK_ON,
	USBPD_SOURCE_OFF,
	USBPD_SOURCE_ON,
};

enum usbpd_port_role {
	USBPD_Rp	= 0x01,
	USBPD_Rd	= 0x01 << 1,
	USBPD_Ra	= 0x01 << 2,
};

enum usbpd_port_rp_level {
    USBPD_56k   = 1,
    USBPD_22k   = 3,
    USBPD_10k   = 7,
};

enum {
	USBPD_CC_OFF,
	USBPD_CC_ON,
};

enum vdm_command_type {
	Initiator       = 0,
	Responder_ACK   = 1,
	Responder_NAK   = 2,
	Responder_BUSY  = 3
};

enum vdm_type {
	Unstructured_VDM = 0,
	Structured_VDM = 1
};

enum vdm_version_type{
	VDM_Version1	= 0,
	VDM_Version2	= 1
};

enum product_type_dfp_type{
	DFP_Undifined					= 0,
	DFP_PDUSB_Hub					= 1,
	DFP_PDUSB_Host					= 2,
	DFP_Power_Brick					= 3,
	DFP_Alternate_Mode_Controller	= 4
};

enum product_type_ufp_type{
	UFP_Undifined					= 0,
	UFP_PDUSB_Hub					= 1,
	UFP_PDUSB_Peripheral			= 2,
	UFP_PSD							= 3,
	UFP_Alternate_Mode_Adapter		= 5,
	UFP_VCONN_Powered_USB_Device	= 6
};

enum vdm_configure_type {
	USB		= 0,
	USB_U_AS_DFP_D	= 1,
	USB_U_AS_UFP_D	= 2
};

enum vdm_displayport_protocol {
	UNSPECIFIED	= 0,
	DP_V_1_3	= 1,
	GEN_2		= 1 << 1
};

enum dp_support {
	USBPD_NOT_DP = 0,
	USBPD_DP_SUPPORT = 1,
};

enum vdm_pin_assignment {
	DE_SELECT_PIN		= 0,
	PIN_ASSIGNMENT_A	= 1,
	PIN_ASSIGNMENT_B	= 1 << 1,
	PIN_ASSIGNMENT_C	= 1 << 2,
	PIN_ASSIGNMENT_D	= 1 << 3,
	PIN_ASSIGNMENT_E	= 1 << 4,
	PIN_ASSIGNMENT_F	= 1 << 5,
};

enum vdm_command_msg {
	Discover_Identity		= 1,
	Discover_SVIDs			= 2,
	Discover_Modes			= 3,
	Enter_Mode			= 4,
	Exit_Mode			= 5,
	Attention			= 6,
	DisplayPort_Status_Update	= 0x10,
	DisplayPort_Configure		= 0x11,
};

enum usbpd_connect_type {
	USBPD_UP_SIDE	= 1,
	USBPD_DOWN_SIDE	= 2,
	USBPD_UNDEFFINED_SIDE	= 3,
};


enum usbpd_data_msg_type {
	USBPD_Source_Capabilities	= 0x1,
	USBPD_Request		        = 0x2,
	USBPD_BIST					= 0x3,
	USBPD_Sink_Capabilities		= 0x4,
	USBPD_Battery_Status		= 0x5,
	USBPD_Alert					= 0x6,
	USBPD_Get_Country_Info		= 0x7,
	USBPD_Vendor_Defined		= 0xF,
};

enum usbpd_extended_msg_type{
	USBPD_Source_Capabilities_Extended	= 1,
	USBPD_Status						= 2,
	USBPD_Get_Battery_Cap				= 3,
	USBPD_Get_Battery_Status			= 4,
	USBPD_Battery_Capabilities			= 5,
	USBPD_Get_Manufacturer_Info			= 6,
	USBPD_Manufacturer_Info				= 7,
	USBPD_Security_Request				= 8,
	USBPD_Security_Response				= 9,
	USBPD_Firmware_Update_Request		= 10,
	USBPD_Firmware_Update_Response		= 11,
	USBPD_PPS_Status					= 12,
	USBPD_Country_Info					= 13,
	USBPD_Country_Codes					= 14,
	USBPD_Sink_Capabilities_Extended	= 15
};

enum usbpd_msg_type {
	USBPD_CTRL_MSG		= 0,
	USBPD_DATA_MSG		= 1,
	USBPD_EXTENDED_MSG		= 2,
};

static inline struct usbpd_data *protocol_rx_to_usbpd(struct protocol_data *rx)
{
	return container_of(rx, struct usbpd_data, protocol_rx);
}

static inline struct usbpd_data *protocol_tx_to_usbpd(struct protocol_data *tx)
{
	return container_of(tx, struct usbpd_data, protocol_tx);
}

static inline struct usbpd_data *policy_to_usbpd(struct policy_data *policy)
{
	return container_of(policy, struct usbpd_data, policy);
}

static inline struct usbpd_data *manager_to_usbpd(struct usbpd_manager_data *manager)
{
	return container_of(manager, struct usbpd_data, manager);
}

extern int usbpd_init(struct device *dev, void *phy_driver_data);
extern void usbpd_init_policy(struct usbpd_data *);

extern void  usbpd_init_manager_val(struct usbpd_data *);
extern int  usbpd_init_manager(struct usbpd_data *);
extern int usbpd_manager_get_selected_voltage(struct usbpd_data *pd_data, int selected_pdo);
extern void usbpd_manager_plug_attach(struct device *, muic_attached_dev_t);
extern void usbpd_manager_plug_detach(struct device *dev, bool notify);
extern void usbpd_manager_acc_detach(struct device *dev);
extern int  usbpd_manager_match_request(struct usbpd_data *);
extern bool usbpd_manager_power_role_swap(struct usbpd_data *);
extern bool usbpd_manager_vconn_source_swap(struct usbpd_data *);
extern void usbpd_manager_turn_on_source(struct usbpd_data *);
extern void usbpd_manager_turn_off_power_supply(struct usbpd_data *);
extern void usbpd_manager_turn_off_power_sink(struct usbpd_data *);
extern void usbpd_manager_turn_off_vconn(struct usbpd_data *);
extern bool usbpd_manager_data_role_swap(struct usbpd_data *);
extern int usbpd_manager_get_identity(struct usbpd_data *);
extern int usbpd_manager_get_svids(struct usbpd_data *);
extern int usbpd_manager_get_modes(struct usbpd_data *);
extern int usbpd_manager_enter_mode(struct usbpd_data *);
extern int usbpd_manager_exit_mode(struct usbpd_data *, unsigned mode);
extern void usbpd_manager_inform_event(struct usbpd_data *,
		usbpd_manager_event_type);
extern int usbpd_manager_evaluate_capability(struct usbpd_data *);
extern data_obj_type usbpd_manager_select_capability(struct usbpd_data *);
extern bool usbpd_manager_vdm_request_enabled(struct usbpd_data *);
extern void usbpd_manager_acc_handler_cancel(struct device *);
extern int usbpd_manager_check_accessory(struct usbpd_manager_data *);
extern void usbpd_manager_acc_detach_handler(struct work_struct *);
extern void usbpd_manager_send_pr_swap(struct device *);
extern void usbpd_manager_send_dr_swap(struct device *);
extern void usbpd_policy_work(struct work_struct *);
extern void usbpd_protocol_tx(struct usbpd_data *);
extern void usbpd_protocol_rx(struct usbpd_data *);
extern void usbpd_kick_policy_work(struct device *);
extern void usbpd_cancel_policy_work(struct device *);
extern void usbpd_rx_hard_reset(struct device *);
extern void usbpd_rx_soft_reset(struct usbpd_data *);
extern void usbpd_policy_reset(struct usbpd_data *, unsigned flag);

extern void usbpd_set_ops(struct device *, usbpd_phy_ops_type *);
extern void usbpd_read_msg(struct usbpd_data *);
extern bool usbpd_send_msg(struct usbpd_data *, msg_header_type *,
		data_obj_type *);
extern bool usbpd_send_ctrl_msg(struct usbpd_data *d, msg_header_type *h,
		unsigned msg, unsigned dr, unsigned pr);
extern unsigned usbpd_wait_msg(struct usbpd_data *pd_data, unsigned msg_status,
		unsigned ms);
extern void usbpd_reinit(struct device *);
extern void usbpd_init_protocol(struct usbpd_data *);
extern void usbpd_init_counters(struct usbpd_data *);


/* for usbpd certification polling */
/* for usbpd certification polling */
void usbpd_timer1_start(struct usbpd_data *pd_data);
long long usbpd_check_time1(struct usbpd_data *pd_data);
void usbpd_timer2_start(struct usbpd_data *pd_data);
long long usbpd_check_time2(struct usbpd_data *pd_data);

#endif
