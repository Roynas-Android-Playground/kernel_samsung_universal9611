#ifndef FIMC_IS_OTPROM_REAR2_GC5035_V001_H
#define FIMC_IS_OTPROM_REAR2_GC5035_V001_H

/*
 * Reference File
 * 20200401_M31S_Depth_5M_GC5035_OTP_Rear_Cal map V001_for LSI.xlsx
 */

#define GC5035_OTP_PAGE_ADDR                         0xfe
#define GC5035_OTP_MODE_ADDR                         0xf3
#define GC5035_OTP_BUSY_ADDR                         0x6f
#define GC5035_OTP_PAGE                              0x02
#define GC5035_OTP_ACCESS_ADDR_HIGH                  0x69
#define GC5035_OTP_ACCESS_ADDR_LOW                   0x6a
#define GC5035_OTP_READ_ADDR                         0x6c

#define GC5035_BANK_SELECT_ADDR                      0x1000
#define GC5035_OTP_START_ADDR_BANK1                  0x1080
#define GC5035_OTP_START_ADDR_BANK2                  0x1480
#define GC5035_OTP_START_ADDR_BANK3                  0x1880
#define GC5035_OTP_START_ADDR_BANK4                  0x1C80
#define GC5035_OTP_USED_CAL_SIZE                     ((0x13FF - 0x1080 + 0x1) / 8)

#define FIMC_IS_REAR2_MAX_CAL_SIZE                   (8 * 1024)
#define REAR2_HEADER_CHECKSUM_LEN                    ((0x13DF - 0x1080 + 0x1) / 8)

const struct fimc_is_vender_rom_addr rear2_gc5035_otp_cal_addr = {
	/* Set '-1' if not used */

	.camera_module_es_version                  = 'A',
	.cal_map_es_version                        = '1',
	.rom_max_cal_size                          = FIMC_IS_REAR2_MAX_CAL_SIZE,

	.rom_header_cal_data_start_addr            = 0x00,
	.rom_header_main_module_info_start_addr    = 0x00,
	.rom_header_cal_map_ver_start_addr         = 0x1C,
	.rom_header_project_name_start_addr        = 0x24,
	.rom_header_module_id_addr                 = 0x46,
	.rom_header_main_sensor_id_addr            = 0x50,

	.rom_header_sub_module_info_start_addr     = -1,
	.rom_header_sub_sensor_id_addr             = -1,

	.rom_header_main_header_start_addr         = -1,
	.rom_header_main_header_end_addr           = -1,
	.rom_header_main_oem_start_addr            = -1,
	.rom_header_main_oem_end_addr              = -1,
	.rom_header_main_awb_start_addr            = -1,
	.rom_header_main_awb_end_addr              = -1,
	.rom_header_main_shading_start_addr        = -1,
	.rom_header_main_shading_end_addr          = -1,
	.rom_header_main_sensor_cal_start_addr     = -1,
	.rom_header_main_sensor_cal_end_addr       = -1,
	.rom_header_dual_cal_start_addr            = -1,
	.rom_header_dual_cal_end_addr              = -1,
	.rom_header_pdaf_cal_start_addr            = -1,
	.rom_header_pdaf_cal_end_addr              = -1,

	.rom_header_sub_oem_start_addr             = -1,
	.rom_header_sub_oem_end_addr               = -1,
	.rom_header_sub_awb_start_addr             = -1,
	.rom_header_sub_awb_end_addr               = -1,
	.rom_header_sub_shading_start_addr         = -1,
	.rom_header_sub_shading_end_addr           = -1,

	.rom_header_main_mtf_data_addr             = -1,
	.rom_header_sub_mtf_data_addr              = -1,

	.rom_header_checksum_addr                  = 0x6C,
	.rom_header_checksum_len                   = REAR2_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr              = -1,
	.rom_oem_af_macro_position_addr            = -1,
	.rom_oem_module_info_start_addr            = -1,
	.rom_oem_checksum_addr                     = -1,
	.rom_oem_checksum_len                      = -1,

	.rom_awb_module_info_start_addr            = -1,
	.rom_awb_checksum_addr                     = -1,
	.rom_awb_checksum_len                      = -1,

	.rom_shading_module_info_start_addr        = -1,
	.rom_shading_checksum_addr                 = -1,
	.rom_shading_checksum_len                  = -1,

	.rom_sensor_cal_module_info_start_addr     = -1,
	.rom_sensor_cal_checksum_addr              = -1,
	.rom_sensor_cal_checksum_len               = -1,

	.rom_dual_module_info_start_addr           = -1,
	.rom_dual_checksum_addr                    = -1,
	.rom_dual_checksum_len                     = -1,

	.rom_pdaf_module_info_start_addr           = -1,
	.rom_pdaf_checksum_addr                    = -1,
	.rom_pdaf_checksum_len                     = -1,

	.rom_sub_oem_af_inf_position_addr          = -1,
	.rom_sub_oem_af_macro_position_addr        = -1,
	.rom_sub_oem_module_info_start_addr        = -1,
	.rom_sub_oem_checksum_addr                 = -1,
	.rom_sub_oem_checksum_len                  = -1,

	.rom_sub_awb_module_info_start_addr        = -1,
	.rom_sub_awb_checksum_addr                 = -1,
	.rom_sub_awb_checksum_len                  = -1,

	.rom_sub_shading_module_info_start_addr    = -1,
	.rom_sub_shading_checksum_addr             = -1,
	.rom_sub_shading_checksum_len              = -1,

	.rom_dual_cal_data2_start_addr             = -1,
	.rom_dual_cal_data2_size                   = -1,
	.rom_dual_tilt_x_addr                      = -1,
	.rom_dual_tilt_y_addr                      = -1,
	.rom_dual_tilt_z_addr                      = -1,
	.rom_dual_tilt_sx_addr                     = -1,
	.rom_dual_tilt_sy_addr                     = -1,
	.rom_dual_tilt_range_addr                  = -1,
	.rom_dual_tilt_max_err_addr                = -1,
	.rom_dual_tilt_avg_err_addr                = -1,
	.rom_dual_tilt_dll_version_addr            = -1,
	.rom_dual_shift_x_addr                     = -1,
	.rom_dual_shift_y_addr                     = -1,

	.extend_cal_addr                           = NULL,
};

static const u32 sensor_mode_read_initial_setting_gc5035[] = {
	0xfa, 0x10, 0x0,
	0xf5, 0xe9, 0x0,
	0xfe, 0x02, 0x0,
	0x67, 0xc0, 0x0,
	0x59, 0x3f, 0x0,
	0x55, 0x80, 0x0,
	0x65, 0x80, 0x0,
	0x66, 0x03, 0x0,
};

static const u32 sensor_global_gc5035[] = {
	0xfc, 0x01, 0x0,
	0xf4, 0x40, 0x0,
	0xf5, 0xe9, 0x0,
	0xf6, 0x14, 0x0,
	0xf8, 0x44, 0x0,
	0xf9, 0x82, 0x0,
	0xfa, 0x00, 0x0,
	0xfc, 0x81, 0x0,
	0xfe, 0x00, 0x0,
	0x36, 0x01, 0x0,
	0xd3, 0x87, 0x0,
	0x36, 0x00, 0x0,
	0x33, 0x00, 0x0,
	0xfe, 0x03, 0x0,
	0x01, 0xe7, 0x0,
	0xf7, 0x01, 0x0,
	0xfc, 0x8f, 0x0,
	0xfc, 0x8f, 0x0,
	0xfc, 0x8e, 0x0,
	0xfe, 0x00, 0x0,
	0xee, 0x30, 0x0,
	0x87, 0x18, 0x0,
	0xfe, 0x01, 0x0,
	0x8c, 0x90, 0x0,
	0xfe, 0x00, 0x0,
};

static const u32 sensor_mode_read_initial_setting_gc5035_size =
	sizeof(sensor_mode_read_initial_setting_gc5035) / sizeof( sensor_mode_read_initial_setting_gc5035[0]);

static const u32 sensor_global_gc5035_size =
	sizeof(sensor_global_gc5035) / sizeof( sensor_global_gc5035[0]);

#endif /* FIMC_IS_OTPROM_REAR2_GC5035_V001_H */
