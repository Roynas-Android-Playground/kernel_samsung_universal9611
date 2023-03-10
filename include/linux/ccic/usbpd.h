#ifndef __USBPD_H__
#define __USBPD_H__

#include <linux/ccic/usbpd_msg.h>
#include <linux/muic/muic.h>

#define MAX_CHARGING_VOLT		12000 /* 12V */
#define USBPD_VOLT_UNIT			50 /* 50mV */
#define USBPD_CURRENT_UNIT		10 /* 10mA */

#define USBPD_MAX_COUNT_MSG_OBJECT	(8) /* 0..7 */

/* Counter */
#define USBPD_nMessageIDCount		(7)
#define USBPD_nRetryCount		(3)
#define USBPD_nHardResetCount		(2)
#define USBPD_nCapsCount		(30)
#define USBPD_nDiscoverIdentityCount	(20)

/* Timer */
#define tSrcTransition		(35)	/* 25~35 ms */
#define tPSSourceOn		(480)	/* 390~480 ms */
#define tPSSourceOff		(750)	/* 750~960 ms */
#define tSenderResponse		(1000)	/* 1000 ms */
#define tSenderResponseSRC	(150)	/* 1000 ms */
#define tSendSourceCap		(10)	/* 1~2 s */
#define tPSHardReset		(25)	/* 25~35 ms */
#define tSinkWaitCap		(2500)	/* 2.1~2.5 s  */
#define tPSTransition		(5500)	/* 450~550 ms */
#define tVCONNSourceOn		(100)	/* 24~30 ms */
#define tVDMSenderResponse	(30)	/* 24~30 ms */
#define tVDMWaitModeEntry	(50)	/* 40~50  ms */
#define tVDMWaitModeExit	(50)    /* 40~50  ms */
#define tDiscoverIdentity	(50)	/* 40~50  ms */
#define tSwapSourceStart        (20)	/* 20  ms */

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

	Error_Recovery			= 0xFF
} policy_state;

typedef enum usbpd_manager_command {
	MANAGER_REQ_GET_SNKCAP			= 1,
	MANAGER_REQ_GOTOMIN			= 2,
	MANAGER_REQ_SRCCAP_CHANGE		= 3,
	MANAGER_REQ_PR_SWAP			= 4,
	MANAGER_REQ_DR_SWAP			= 5,
	MANAGER_REQ_VCONN_SWAP			= 6,
	MANAGER_REQ_VDM_DISCOVER_IDENTITY	= 7,
	MANAGER_REQ_VDM_DISCOVER_SVID		= 8,
	MANAGER_REQ_VDM_DISCOVER_MODE		= 9,
	MANAGER_REQ_VDM_ENTER_MODE		= 10,
	MANAGER_REQ_VDM_EXIT_MODE		= 11,
	MANAGER_REQ_VDM_ATTENTION		= 12,
	MANAGER_REQ_VDM_STATUS_UPDATE		= 13,
	MANAGER_REQ_VDM_DisplayPort_Configure	= 14,
	MANAGER_REQ_NEW_POWER_SRC		= 15,
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
} usbpd_manager_event_type;

enum usbpd_msg_status {
	MSG_GOODCRC	= 1<<0,
	MSG_ACCEPT	= 1<<1,
	MSG_PSRDY	= 1<<2,
	MSG_REQUEST	= 1<<3,
	MSG_REJECT	= 1<<4,
	MSG_WAIT	= 1<<5,
	MSG_ERROR	= 1<<6,
	MSG_PING	= 1<<7,
	MSG_GET_SNK_CAP = 1<<8,
	MSG_GET_SRC_CAP = 1<<9,
	MSG_SNK_CAP     = 1<<10,
	MSG_SRC_CAP     = 1<<11,
	MSG_PR_SWAP	= 1<<12,
	MSG_DR_SWAP	= 1<<13,
	MSG_VCONN_SWAP	= 1<<14,
	VDM_DISCOVER_IDENTITY	= 1<<15,
	VDM_DISCOVER_SVID	= 1<<16,
	VDM_DISCOVER_MODE	= 1<<17,
	VDM_ENTER_MODE		= 1<<18,
	VDM_EXIT_MODE		= 1<<19,
	VDM_ATTENTION		= 1<<20,
	VDM_DP_STATUS_UPDATE	= 1<<21,
	VDM_DP_CONFIGURE	= 1<<22,
	MSG_SOFTRESET		= 1<<23,
	PLUG_DETACH		= 1<<24,
	PLUG_ATTACH		= 1<<25,
	MSG_HARDRESET		= 1<<26,
	CC_DETECT		= 1<<27,
	PLUG_WAKEUP		= 1<<28,
	MSG_PASS		= 1<<29,
	MSG_RID			= 1<<30,
	MSG_NONE		= 1<<31,
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

typedef struct usbpd_phy_ops {
	/*    1st param should be 'usbpd_data *'    */
	int    (*tx_msg)(void *, msg_header_type *, data_obj_type *);
	int    (*rx_msg)(void *, msg_header_type *, data_obj_type *);
	int    (*hard_reset)(void *);
	int    (*set_power_role)(void *, int);
	int    (*get_power_role)(void *, int *);
	int    (*set_data_role)(void *, int);
	int    (*get_data_role)(void *, int *);
	int    (*set_vconn_source)(void *, int);
	int    (*get_vconn_source)(void *, int *);
	int    (*set_check_msg_pass)(void *, int);
	unsigned   (*get_status)(void *, unsigned);
	bool   (*poll_status)(void *);
	void   (*driver_reset)(void *);
} usbpd_phy_ops_type;

struct policy_data {
	policy_state		state;
    policy_state        last_state;
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
    u8              origin_message;
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

	int test_cnt; /* for test */
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

	/* power role swap*/
	bool power_role_swap;
	/* data role swap*/
	bool data_role_swap;
	bool vconn_source_swap;

	struct usbpd_data *pd_data;
	muic_attached_dev_t	attached_dev;
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
	data_obj_type		source_request_obj;
	struct usbpd_manager_data	manager;
	struct work_struct	worker;
	struct completion	msg_arrived;
	unsigned                wait_for_msg_arrived;
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
extern void usbpd_manager_plug_attach(struct device *, muic_attached_dev_t);
extern void usbpd_manager_plug_detach(struct device *dev, bool notify);
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
extern int usbpd_manager_enter_mode(struct usbpd_data *, unsigned mode, u32);
extern int usbpd_manager_exit_mode(struct usbpd_data *, unsigned mode);
extern void usbpd_manager_inform_event(struct usbpd_data *,
		usbpd_manager_event_type);
extern int usbpd_manager_evaluate_capability(struct usbpd_data *);
extern data_obj_type usbpd_manager_select_capability(struct usbpd_data *);
extern bool usbpd_manager_vdm_request_enabled(struct usbpd_data *);

extern void usbpd_policy_work(struct work_struct *);
extern void usbpd_protocol_tx(struct usbpd_data *);
extern void usbpd_protocol_rx(struct usbpd_data *);
extern void usbpd_kick_policy_work(struct device *);
extern void usbpd_rx_hard_reset(struct device *);
extern void usbpd_rx_soft_reset(struct usbpd_data *);
extern void usbpd_policy_reset(struct usbpd_data *, unsigned flag);
extern void usbpd_tx_request_discard(struct usbpd_data *pd_data);
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

#endif
