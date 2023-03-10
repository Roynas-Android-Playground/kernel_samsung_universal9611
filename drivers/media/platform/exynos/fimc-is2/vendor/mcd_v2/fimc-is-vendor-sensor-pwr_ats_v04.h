#ifndef FIMC_IS_VENDOR_SENSOR_PWR_ATS_V04_H
#define FIMC_IS_VENDOR_SENSOR_PWR_ATS_V04_H

/***** This file is used to define sensor power pin name for only ATS_V04 *****/

#define USE_VENDOR_PWR_PIN_NAME


/***** REAR MAIN - S5K4HA *****/
#define S5K4HA_IOVDD    "gpio_camio_1p8_en"    /* CAM_VDDIO_1P8 */
#define S5K4HA_AVDD     "gpio_cam_2p8_en"    /* VDD_RCAM_A2P8 */
#define S5K4HA_DVDD     "gpio_cam_core_en"    /* RCAM_CORE_1P2 */
#define RCAM_AF_VDD     "gpio_camaf_2p8_en"    /* VDD_RCAM_AF_2P8 */

/***** FRONT - S5K5E9 *****/
#define S5K5E9_IOVDD    "gpio_camio_1p8_en"         /* CAM_VDDIO_1P8  */
#define S5K5E9_AVDD     "gpio_cam_2p8_en"         /* VDD_FCAM_A2P8  */
#define S5K5E9_DVDD     "gpio_cam_core_en"         /* FCAM_CORE_1P2  */

/***** ETC Define related to sensor power *****/
#define USE_COMMON_CAM_IO_PWR             /* CAM_VDDIO_1P8 Power is used commonly for all camera and EEPROM */
//#define DIVISION_EEP_IO_PWR             /* Use Rear IO power for Front EEPROM i2c pull-up power */


#endif /* FIMC_IS_VENDOR_SENSOR_PWR_ATS_V04_H */
