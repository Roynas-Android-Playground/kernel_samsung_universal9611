#ifndef FIMC_IS_VENDOR_SENSOR_PWR_MMS_V30S_H
#define FIMC_IS_VENDOR_SENSOR_PWR_MMS_V30S_H

/***** This file is used to define sensor power pin name for only MMS_V30S *****/

#define USE_VENDOR_PWR_PIN_NAME

/***** REAR MAIN - GM2 *****/
#define RCAM_AF_VDD      "vdd_ldo37"       /* RCAM1_AFVDD_2P8 */
#define S5KGM2_IOVDD     "CAM_VLDO3"       /* CAM_VDDIO_1P8 : CAM_VLDO3 is used for all camera commonly */
#define S5KGM2_AVDD      "CAM_VLDO6"       /* RCAM1_AVDD1_2P9 */
#define S5KGM2_DVDD      "CAM_VLDO1"       /* RCAM1_DVDD_1P05 */

/***** FRONT - HI2021Q *****/
#define HI2021Q_IOVDD    "CAM_VLDO3"      /* CAM_VDDIO_1P8  */
#define HI2021Q_AVDD     "CAM_VLDO5"      /* RCAM3_AVDD_2P8 */
#define HI2021Q_DVDD     "CAM_VLDO2"      /* RCAM3_DVDD_1P1 */

/***** REAR2 SUB - GC5035 *****/
#define GC5035_IOVDD    "CAM_VLDO3"        /* CAM_VDDIO_1P8 */ 
#define GC5035_AVDD     "CAM_VLDO7"        /* RCAM2_AVDD_2P8 */ 
#define GC5035_DVDD     "vdd_ldo44"        /* RCAM2_DVDD_1P2 */ 

/***** REAR3 WIDE - 4HA *****/
#define S5K4HA_IOVDD    "CAM_VLDO3"        /* CAM_VDDIO_1P8 */ 
#define S5K4HA_AVDD     "gpio_cam_a2p8_en"  /* RCAM3_AVDD_2P8 : XGPIO14(GPG1[6]) */ 
#define S5K4HA_DVDD     "CAM_VLDO4"        /* RCAM2_DVDD_1P2 */ 

/***** REAR4 MACRO - GC5035 *****/
#define GC5035_2ND_IOVDD   "CAM_VLDO3"               /* CAM_VDDIO_1P8 */
#define GC5035_2ND_AVDD    "gpio_cam_a2p8_1p2_en"    /* RCAM4_AVDD_2P8 */
#define GC5035_2ND_DVDD    "gpio_cam_a2p8_1p2_en"    /* RCAM4_DVDD_1P2 */

/***** ETC Define related to sensor power *****/
#define USE_COMMON_CAM_IO_PWR              /* CAM_VDDIO_1P8 Power is used commonly for all camera and EEPROM */
//#define DIVISION_EEP_IO_PWR              /* Use Rear IO power for Front EEPROM i2c pull-up power */


#endif /* FIMC_IS_VENDOR_SENSOR_PWR_MMS_V30S_H */
