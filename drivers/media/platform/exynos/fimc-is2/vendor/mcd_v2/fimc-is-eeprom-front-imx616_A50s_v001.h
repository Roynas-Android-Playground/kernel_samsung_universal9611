#ifndef FIMC_IS_EEPROM_FRONT_IMX616_A50S_V001_H
#define FIMC_IS_EEPROM_FRONT_IMX616_A50S_V001_H

/* Reference File
 * Universal EEPROM map data Universal EEPROM map data_V001_20190415_for LSI AP_IMX616_Front(A50s).xlsx
 */

#define FIMC_IS_FRONT_MAX_CAL_SIZE (8 * 1024)
#define FRONT_HEADER_CHECKSUM_LEN (0xD7 - 0x00 + 0x1)
#define FRONT_OEM_CHECKSUM_LEN (0x01BF - 0x0190 + 0x1)
#define FRONT_AWB_CHECKSUM_LEN (0x021F - 0x0200 + 0x1)
#define FRONT_SHADING_CHECKSUM_LEN (0x169F - 0x0300 + 0x1)
#define FRONT_SENSOR_CAL_CHECKSUM_LEN (0x1E2B - 0x1700 + 0x1)

static int front_imx616_oem_checksum_base_addr = 0x0190;

const struct rom_extend_cal_addr front_imx616_extend_cal_addr = {
    .name = EXTEND_OEM_CHECKSUM,
    .data = &front_imx616_oem_checksum_base_addr,
    .next = NULL,
};

const struct fimc_is_vender_rom_addr front_imx616_cal_addr = {
	/* Set '-1' if not used */

	.camera_module_es_version                  = 'A',
	.cal_map_es_version                        = '1',
	.rom_max_cal_size                          = FIMC_IS_FRONT_MAX_CAL_SIZE,

	.rom_header_cal_data_start_addr            = 0x00,
	.rom_header_main_module_info_start_addr    = 0x28,
	.rom_header_cal_map_ver_start_addr         = 0x38,
	.rom_header_project_name_start_addr        = 0x40,
	.rom_header_module_id_addr                 = 0xA8+6,
	.rom_header_main_sensor_id_addr            = 0xB8,

	.rom_header_sub_module_info_start_addr     = -1,
	.rom_header_sub_sensor_id_addr             = -1,

	.rom_header_main_header_start_addr         = 0x00,
	.rom_header_main_header_end_addr           = 0xFF,
	.rom_header_main_oem_start_addr            = 0x00,
	.rom_header_main_oem_end_addr              = 0x04,
	.rom_header_main_awb_start_addr            = 0x08,
	.rom_header_main_awb_end_addr              = 0x0C,
	.rom_header_main_shading_start_addr        = 0x10,
	.rom_header_main_shading_end_addr          = 0x14,
	.rom_header_main_sensor_cal_start_addr     = 0x18,
	.rom_header_main_sensor_cal_end_addr       = 0x1C,
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

	.rom_header_main_mtf_data_addr             = 0x0156,
	.rom_header_sub_mtf_data_addr              = -1,

	.rom_header_checksum_addr                  = 0xFC,
	.rom_header_checksum_len                   = FRONT_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr              = 0x0100,
	.rom_oem_af_macro_position_addr            = 0x0108,
	.rom_oem_module_info_start_addr            = 0x01E0,
	.rom_oem_checksum_addr                     = 0x01FC,
	.rom_oem_checksum_len                      = FRONT_OEM_CHECKSUM_LEN,

	.rom_awb_module_info_start_addr            = 0x02E0,
	.rom_awb_checksum_addr                     = 0x02FC,
	.rom_awb_checksum_len                      = FRONT_AWB_CHECKSUM_LEN,

	.rom_shading_module_info_start_addr        = 0x16E0,
	.rom_shading_checksum_addr                 = 0x16FC,
	.rom_shading_checksum_len                  = FRONT_SHADING_CHECKSUM_LEN,

	.rom_sensor_cal_module_info_start_addr     = 0x1FE0,
	.rom_sensor_cal_checksum_addr              = 0x1FFC,
	.rom_sensor_cal_checksum_len               = FRONT_SENSOR_CAL_CHECKSUM_LEN,

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

	.extend_cal_addr                           = &front_imx616_extend_cal_addr,
};

#endif
