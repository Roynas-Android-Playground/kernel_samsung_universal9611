#ifndef FIMC_IS_VENDOR_SENSOR_PWR_MMS_V31S_H
#define FIMC_IS_VENDOR_SENSOR_PWR_MMS_V31S_H

/***** This file is used to define sensor power pin name for only MMS_V31S *****/

#define USE_VENDOR_PWR_PIN_NAME

/***** REAR MAIN - IMX682 *****/
#define RCAM_AF_VDD        "vdd_ldo37"    /* RCAM1_AFVDD_2P8 */
#define IMX682_IOVDD       "CAM_VLDO3"    /* CAM_VDDIO_1P8 : CAM_VLDO3 is used for all camera commonly */
#define IMX682_AVDD1       "CAM_VLDO6"    /* RCAM1_AVDD1_2P9 */
#define IMX682_AVDD2       "vdd_ldo33"    /* RCAM1_AVDD2_1P8 */
#define IMX682_AVDD2_2ND   "gpio_ldo_en"  /* RCAM1_AVDD2_1P8 : for old HW support */
#define IMX682_DVDD        "CAM_VLDO1"    /* RCAM1_DVDD_1P1 */

/***** FRONT - IMX616 *****/
#define IMX616_IOVDD       "CAM_VLDO3"    /* CAM_PMIC_VLDO3 */
#define IMX616_AVDD        "CAM_VLDO5"    /* CAM_PMIC_VLDO5 */
#define IMX616_DVDD        "CAM_VLDO2"    /* CAM_PMIC_VLDO2 */

/***** REAR2 SUB - GC5035 *****/
#define GC5035_IOVDD    "CAM_VLDO3"        /* CAM_VDDIO_1P8 */ 
#define GC5035_AVDD     "CAM_VLDO7"        /* RCAM2_AVDD_2P8 */ 
#define GC5035_DVDD     "CAM_VLDO4"        /* RCAM2_DVDD_1P2 */ 

/***** REAR3 WIDE - 3L6 *****/
#define S5K3L6_IOVDD    "CAM_VLDO3"        /* CAM_VDDIO_1P8 */ 
#define S5K3L6_AVDD     "gpio_ldo_en"      /* RCAM3_AVDD_2P8 */ 
#define S5K3L6_DVDD     "vdd_ldo44"        /* RCAM3_DVDD_1P05 */

/***** REAR4 MACRO - GC5035 *****/
#define GC5035_2ND_IOVDD   "CAM_VLDO3"               /* CAM_VDDIO_1P8 */
#define GC5035_2ND_AVDD    "gpio_cam_a2p8_1p2_en"    /* RCAM4_AVDD_2P8 */
#define GC5035_2ND_DVDD    "gpio_cam_a2p8_1p2_en"    /* RCAM4_DVDD_1P2 */

/***** ETC Define related to sensor power *****/
#define USE_COMMON_CAM_IO_PWR              /* CAM_VDDIO_1P8 Power is used commonly for all camera and EEPROM */
//#define DIVISION_EEP_IO_PWR              /* Use Rear IO power for Front EEPROM i2c pull-up power */

#endif /* FIMC_IS_VENDOR_SENSOR_PWR_MMS_V31S_H */
