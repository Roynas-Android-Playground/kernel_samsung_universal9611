#ifndef FIMC_IS_VENDOR_CONFIG_MMS_V31S_H
#define FIMC_IS_VENDOR_CONFIG_MMS_V31S_H

#define VENDER_PATH

/***** CAL ROM DEFINE *****/
#define VENDER_CAL_STRUCT_VER2                                /* For USE the extend cal data ver2 */
#define ROM_DEBUG
//#define ROM_CRC32_DEBUG
//#define SKIP_CHECK_CRC                                      /* Skip the CRC CHECK of cal data */
//#define USE_SHARED_ROM_REAR3                                /* Rear3 Camera share the ROM with Rear(Wide)*/

#ifdef VENDER_CAL_STRUCT_VER2
#define USE_AE_CAL                                            /* USE Ae cal data in rear camera, Only VENDER_CAL_STRUCT_VER2 */
#endif

/***** SUPPORT CAMERA DEFINE *****/
#define REAR_SUB_CAMERA            (SENSOR_POSITION_REAR2)    /* Supported Camera Type for rear bokeh */
#define REAR_ULTRA_WIDE_CAMERA     (SENSOR_POSITION_REAR3)    /* Supported Camera Type for rear ultra wide */
#define REAR_MACRO_CAMERA          (SENSOR_POSITION_REAR4)    /* Supported Camera Type for rear macro camera */
//#define FRONT_SUB_CAMERA         (SENSOR_POSITION_FRONT2)   /* Supported Camera Type for front bokeh */

#define CAMERA_REAR2               (REAR_ULTRA_WIDE_CAMERA)   /* For Rear2 of SYSFS */
#define CAMERA_REAR3               (REAR_SUB_CAMERA)          /* For Rear3 of SYSFS */
#define CAMERA_REAR4               (REAR_MACRO_CAMERA)        /* For Rear4 of SYSFS */
//#define CAMERA_FRONT2            (FRONT_SUB_CAMERA)         /* For Front2 of SYSFS */

#define USE_3L6B_SETFILE                                      /* For using 3L6 as 12MP (4000x3000) */
#define SENSOR_OTP_GC5035
#define USE_DIFFERENT_ISP_MODULE
#ifdef USE_DIFFERENT_ISP_MODULE
#define USE_QUALCOMM_MODULE                                   /* Allow common module load, meant for Qualcomm */
#endif /* USE_DIFFERENT_ISP_MODULE */
#define OTPROM_SHARED_MCLK_SENSOR_POSITION       (SENSOR_POSITION_REAR)

/***** SUPPORT FUCNTION DEFINE *****/
#define SAMSUNG_LIVE_OUTFOCUS                                 /* Allocate memory For Dual Camera */
#define ENABLE_REMOSAIC_CAPTURE                               /* Base Remosaic */
#define ENABLE_REMOSAIC_CAPTURE_WITH_ROTATION                 /* M2M and Rotation is used during Remosaic */
#define CHAIN_USE_STRIPE_PROCESSING  1                        /* Support STRIPE_PROCESSING during Remosaic */
#define STRIPE_MARGIN_WIDTH_1024                              /* Stripe margin width size set as 1024 when this define enable. Default is 512 */
#define USE_AP_PDAF                                           /* Support sensor PDAF SW Solution */
//#define USE_SENSOR_WDR                                      /* Support sensor WDR */
#define SUPPORT_SENSOR_SEAMLESS_3HDR                          /* whether capable of 3DHDR or not */
#undef OVERFLOW_PANIC_ENABLE_CSIS                             /* Not Support Kernel Panic when CSIS OVERFLOW */
#define ENABLE_CSIS_ERR_SENSOR_DUMP                           /* Enable Sensor register dump when CSIS ERROR occurs */
#define SUPPORT_FRONT_SM_DVFS                                 /* Support Front SlowMotion at L0 DVFS */
//#define DISABLE_DUAL_SYNC                                     /* Disable dual sync Between rear main and rear bokeh */
/* Use set calibration for processing raw calibaration data in Samsung side instead of outsourcing */
#define USE_DUALCAM_SET_CAL
#define USE_PDAF_MED_LINE_RESET                               /* reset med_line for pdaf */

/***** DDK - DRIVER INTERFACE *****/
#define USE_AI_CAMERA_INTERFACE     (1)                       /* This feature since A7 2018 */
#define USE_MFHDR_CAMERA_INTERFACE  (1)                       /* This feature since A7 2018 */
#define USE_3HDR_CAMERA_INTERFACE
/**
 * This feature since A50s(ramen) and A30s(lassen).
 * In previous projects, only A30 / A20 / A20p is applied for sensor duplication
 */
#define USE_DEBUG_LIBRARY_VER
//#define USE_FACE_UNLOCK_AE_AWB_INIT                         /* for Face Unlock */

/***** HW DEFENDANT DEFINE *****/
#define USE_CAMERA_ACT_DRIVER_SOFT_LANDING

/***** SUPPORT EXTERNEL FUNCTION DEFINE *****/
#define USE_SSRM_CAMERA_INFO                                  /* Match with SAMSUNG_SSRM define of Camera Hal side */

#define USE_CAMERA_HW_BIG_DATA
#ifdef USE_CAMERA_HW_BIG_DATA
//#define USE_CAMERA_HW_BIG_DATA_FOR_PANIC
#define CSI_SCENARIO_SEN_REAR	(0)                           /* This value follows dtsi */
#define CSI_SCENARIO_SEN_FRONT	(1)
#endif

#define USE_MONO_SENSOR

#define ENABLE_FULLCHAIN_OVERFLOW_RECOVERY

#endif /* FIMC_IS_VENDOR_CONFIG_MMS_V31S_H */
