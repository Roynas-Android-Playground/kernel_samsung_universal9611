#ifndef FIMC_IS_VENDOR_SENSOR_PWR_XXS_VPRO_H
#define FIMC_IS_VENDOR_SENSOR_PWR_XXS_VPRO_H

/***** This file is used to define sensor power pin name for only AAS_V51 *****/

#define USE_VENDOR_PWR_PIN_NAME


/***** REAR MAIN - IMX576 *****/
#define RCAM_AF_VDD        "vdd_ldo37"    /* RCAM1_AFVDD_2P8 */
#define IMX576_IOVDD       "CAM_VLDO3"    /* CAM_VDDIO_1P8 : CAM_VLDO3 is used for all camera commonly */
#define IMX576_AVDD        "CAM_VLDO6"    /* RCAM1_AVDD_2P8 */
#define IMX576_DVDD        "CAM_VLDO1"    /* RCAM1_DVDD_1P05 */

/***** REAR3 WIDE - S54HA *****/
#define S5K4HA_IOVDD    "CAM_VLDO3"         /* CAM_VDDIO_1P8  */
#define S5K4HA_AVDD     "CAM_VLDO5"         /* RCAM3_AVDD_2P8 */
#define S5K4HA_DVDD     "CAM_VLDO4"         /* RCAM3_DVDD_1P1 */

/***** FRONT - HI1336 *****/
#define HI1336_IOVDD    "CAM_VLDO3"         /* CAM_VDDIO_1P8  */
#define HI1336_AVDD     "CAM_VLDO7"         /* FCAM_AVDD_2P8  */
#define HI1336_DVDD     "CAM_VLDO2"         /* FCAM_DVDD_1P1  */

/***** ETC Define related to sensor power *****/
#define USE_COMMON_CAM_IO_PWR             /* CAM_VDDIO_1P8 Power is used commonly for all camera and EEPROM */
//#define DIVISION_EEP_IO_PWR             /* Use Rear IO power for Front EEPROM i2c pull-up power */


#endif /* FIMC_IS_VENDOR_SENSOR_PWR_XXS_VPRO_H */
