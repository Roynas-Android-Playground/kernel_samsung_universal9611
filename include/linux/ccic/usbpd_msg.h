#ifndef __USBPD_MSG_H__
#define __USBPD_MSG_H__

/* for header */
#define USBPD_REV_20	(1)
#define PD_SID		(0xFF00)
#define PD_SID_1	(0xFF01)

typedef union {
	u16 word;
	u8  byte[2];

	struct {
		unsigned msg_type:4;
		unsigned:1;
		unsigned port_data_role:1;
		unsigned spec_revision:2;
		unsigned port_power_role:1;
		unsigned msg_id:3;
		unsigned num_data_objs:3;
		unsigned:1;
	};
} msg_header_type;

typedef union {
	u32 object;
	u16 word[2];
	u8  byte[4];

	struct {
		unsigned:30;
		unsigned supply_type:2;
	} power_data_obj_supply_type;

	struct {
		unsigned max_current:10;        /* 10mA units */
		unsigned voltage:10;            /* 50mV units */
		unsigned peak_current:2;
		unsigned:3;
		unsigned data_role_swap:1;
		unsigned usb_comm_capable:1;
		unsigned externally_powered:1;
		unsigned usb_suspend_support:1;
		unsigned dual_role_power:1;
		unsigned supply:2;
	} power_data_obj;

	struct {
		unsigned op_current:10;	/* 10mA units */
		unsigned voltage:10;	/* 50mV units */
		unsigned:5;
		unsigned data_role_swap:1;
		unsigned usb_comm_capable:1;
		unsigned externally_powered:1;
		unsigned higher_capability:1;
		unsigned dual_role_power:1;
		unsigned supply_type:2;
	} power_data_obj_sink;

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
		unsigned min_current:10;	/* 10mA units */
		unsigned op_current:10;		/* 10mA units */
		unsigned:4;
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
		unsigned:4;
		unsigned no_usb_suspend:1;
		unsigned usb_comm_capable:1;
		unsigned capability_mismatch:1;
		unsigned give_back:1;
		unsigned object_position:3;
		unsigned:1;
	} request_data_object_battery;

	struct {
		unsigned vendor_defined:15;
		unsigned vdm_type:1;
		unsigned vendor_id:16;
	} unstructured_vdm;

	struct {
		unsigned command:5;
		unsigned:1;
		unsigned command_type:2;
		unsigned obj_pos:3;
		unsigned:2;
		unsigned version:2;
		unsigned vdm_type:1;
		unsigned svid:16;
	} structured_vdm;

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

	struct{
		unsigned select_configuration:2;
		unsigned displayport_protocol:4;
		unsigned rsvd1:2;
		unsigned ufp_u_pin_assignment:8;
		unsigned rsvd2:16;
	} displayport_configurations;

	struct{
		unsigned svid_1:16;
		unsigned svid_0:16;
	} vdm_svid;
} data_obj_type;

typedef enum {
	POWER_TYPE_FIXED = 0,
	POWER_TYPE_BATTERY,
	POWER_TYPE_VARIABLE,
} power_supply_type;

typedef enum {
	SOP_TYPE_SOP,
	SOP_TYPE_SOP1,
	SOP_TYPE_SOP2,
	SOP_TYPE_SOP1_DEBUG,
	SOP_TYPE_SOP2_DEBUG
} sop_type;

enum usbpd_control_msg_type {
	USBPD_GoodCRC        = 0x1,
	USBPD_GotoMin        = 0x2,
	USBPD_Accept         = 0x3,
	USBPD_Reject         = 0x4,
	USBPD_Ping           = 0x5,
	USBPD_PS_RDY         = 0x6,
	USBPD_Get_Source_Cap = 0x7,
	USBPD_Get_Sink_Cap   = 0x8,
	USBPD_DR_Swap        = 0x9,
	USBPD_PR_Swap        = 0xA,
	USBPD_VCONN_Swap     = 0xB,
	USBPD_Wait           = 0xC,
	USBPD_Soft_Reset     = 0xD
};

enum usbpd_check_msg_pass {
	NONE_CHECK_MSG_PASS,
	CHECK_MSG_PASS,
};

enum usbpd_port_data_role {
	USBPD_UFP,
	USBPD_DFP,
};

enum usbpd_port_power_role {
	USBPD_SINK,
	USBPD_SOURCE,
};

enum usbpd_port_vconn_role {
	USBPD_VCONN_OFF,
	USBPD_VCONN_ON,
};

enum usbpd_port_role {
	USBPD_Rp	= 0x01,
	USBPD_Rd	= 0x01 << 1,
	USBPD_Ra	= 0x01 << 2,
};

enum vdm_command_type{
	Initiator       = 0,
	Responder_ACK   = 1,
	Responder_NAK   = 2,
	Responder_BUSY  = 3
};

enum vdm_type{
	Unstructured_VDM = 0,
	Structured_VDM = 1
};

enum vdm_configure_type{
	USB		= 0,
	USB_U_AS_DFP_D	= 1,
	USB_U_AS_UFP_D	= 2
};

enum vdm_displayport_protocol{
	UNSPECIFIED	= 0,
	DP_V_1_3	= 1,
	GEN_2		= 1 << 1
};

enum vdm_pin_assignment{
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

enum usbpd_data_msg_type {
	USBPD_Source_Capabilities	= 0x1,
	USBPD_Request		        = 0x2,
	USBPD_BIST			= 0x3,
	USBPD_Sink_Capabilities		= 0x4,
	USBPD_Vendor_Defined		= 0xF,
};

#endif

