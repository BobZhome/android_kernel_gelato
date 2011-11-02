/* arch/arm/mach-msm/include/mach/board_gelato.h
 * Copyright (C) 2009 LGE, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __ARCH_MSM_BOARD_GELATO_H
#define __ARCH_MSM_BOARD_GELATO_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <asm/setup.h>
#include "pm.h"

/* sdcard related macros */
#ifdef CONFIG_MMC_MSM_CARD_HW_DETECTION
#define GPIO_SD_DETECT_N    49
#define VREG_SD_LEVEL       3000

#define GPIO_SD_DATA_3      51
#define GPIO_SD_DATA_2      52
#define GPIO_SD_DATA_1      53
#define GPIO_SD_DATA_0      54
#define GPIO_SD_CMD         55
#define GPIO_SD_CLK         56
#endif

/* touch-screen macros */
#define TS_X_MIN			0
#define TS_X_MAX			320
#define TS_Y_MIN			0
#define TS_Y_MAX			480
#define TS_GPIO_I2C_SDA		91
#define TS_GPIO_I2C_SCL		90
#define TS_GPIO_IRQ			92
#define TS_I2C_SLAVE_ADDR	0x20

/* camera */

// LGE_CHANGE_S 2011.01.26 [jongkwon.chae@lge.com] [gelato] sensor porting
#define HYNIX_CAMERA_SENSOR 	1
#define SAMSUNG_CAMERA_SENSOR 	2

#ifdef CONFIG_MACH_MSM7X27_GELATO_DOP    //DOP
	#if   defined (CONFIG_LGE_PCB_REV_A) //REV_A
		#define CAMERA_SENSOR HYNIX_CAMERA_SENSOR
	#elif defined (CONFIG_LGE_PCB_REV_B) //REV_B
		#define CAMERA_SENSOR HYNIX_CAMERA_SENSOR
	#elif defined (CONFIG_LGE_PCB_REV_C) //REV_C
		#define CAMERA_SENSOR SAMSUNG_CAMERA_SENSOR
	#else
		#define CAMERA_SENSOR HYNIX_CAMERA_SENSOR
	#endif
#else                                    //QWERTY
	#if   defined (CONFIG_LGE_PCB_REV_A) //REV_A
		#define CAMERA_SENSOR HYNIX_CAMERA_SENSOR
	#elif defined (CONFIG_LGE_PCB_REV_B) //REV_B
		#define CAMERA_SENSOR HYNIX_CAMERA_SENSOR
	#elif defined (CONFIG_LGE_PCB_REV_C) //REV_C
		#define CAMERA_SENSOR SAMSUNG_CAMERA_SENSOR
	#else
		#define CAMERA_SENSOR HYNIX_CAMERA_SENSOR
	#endif
#endif

#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR) 
#define CAM_I2C_SLAVE_ADDR			(0x40 >> 1) //0x20
#else
#define CAM_I2C_SLAVE_ADDR			(0x78 >> 1) //0x3C
#endif
// LGE_CHANGE_E

#define GPIO_CAM_RESET		 		0		/* GPIO_0 */
#define GPIO_CAM_PWDN		 		1		/* GPIO_1 */
#define GPIO_CAM_MCLK				15		/* GPIO_15 */

#define CAM_POWER_OFF				0
#define CAM_POWER_ON				1

//int aat2870_camera_power_ctrl(int on_off);
#define LDO_CAM_AVDD_NO		2	/* 2.7V */
#define LDO_CAM_DVDD_NO		3	/* 1.2V */
#define LDO_CAM_IOVDD_NO	4	/* 2.6V */

/* proximity sensor */
#define PROXI_GPIO_I2C_SCL	80
#define PROXI_GPIO_I2C_SDA 	81
#define PROXI_GPIO_DOUT		109
//#if defined(CONFIG_LGE_PCB_REV_A)
#ifdef CONFIG_LGE_PCB_REV_A
#define PROXI_I2C_ADDRESS	0x44 /*slave address 7bit*/
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-02-22, [gelato] Rev B, Proximity Sensor change to apds9190 [START]
#else //Rev B ~
#define PROXI_I2C_ADDRESS	0x39 /*slave address 7bit*/
#endif
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-02-22, [gelato] Rev B, Proximity Sensor change to apds9190 [END]
#define PROXI_LDO_NO_VCC	1

/* accelerometer */
#define ACCEL_GPIO_INT	 		39
#define ACCEL_GPIO_I2C_SCL  	78
#define ACCEL_GPIO_I2C_SDA  	76
#define ACCEL_I2C_ADDRESS		0x19 /*kr3dh slave address 7bit*/

/*Ecompass*/
#define ECOM_GPIO_I2C_SCL		107
#define ECOM_GPIO_I2C_SDA		108
#define ECOM_GPIO_RST			31
#define ECOM_GPIO_INT		
#define ECOM_I2C_ADDRESS		0x0E /* slave address 7bit */

/* ear sense driver macros */
#define GPIO_EAR_SENSE		29
#define GPIO_HS_MIC_BIAS_EN	26

/* lcd & backlight */
#define GPIO_LCD_BL_EN		82
#define GPIO_BL_I2C_SCL		88
#define GPIO_BL_I2C_SDA		89
#define GPIO_LCD_VSYNC_O	97
#define GPIO_LCD_MAKER_LOW	93
#define GPIO_LCD_RESET_N	32

#define BL_POWER_SUSPEND	0
#define BL_POWER_RESUME		1

/* bluetooth gpio pin */
enum {
	BT_WAKE         = 42,
	BT_RFR          = 43,
	BT_CTS          = 44,
	BT_RX           = 45,
	BT_TX           = 46,
	BT_PCM_DOUT     = 68,
	BT_PCM_DIN      = 69,
	BT_PCM_SYNC     = 70,
	BT_PCM_CLK      = 71,
	BT_HOST_WAKE    = 83,
	BT_RESET_N			= 123,
};

// LGE_CHANGE_S 2010.12.27 [myeonggyu.son@lge.com] [gelato] kbd_pp2106 qwerty device [START]
#define PP2106_KEYPAD_ROW	8
#define PP2106_KEYPAD_COL	8

#define GPIO_PP2106_RESET	77
#define GPIO_PP2106_IRQ		40
#define GPIO_PP2106_SDA		48
#define GPIO_PP2106_SCL		47
// LGE_CHANGE_E 2010.12.27 [myeonggyu.son@lge.com] [gelato] kbd_pp2106 qwerty device [END]

// LGE_CHANGE_S 2010.12.27 [myeonggyu.son@lge.com] [gelato] hall ic device [START]
#define GPIO_BU52031_IRQ	18
// LGE_CHANGE_E 2010.12.27 [myeonggyu.son@lge.com] [gelato] hall ic device [END]

/* interface variable */
extern struct platform_device msm_device_snd;
extern struct platform_device msm_device_adspdec;
extern struct i2c_board_info i2c_devices[1];

extern int camera_power_state;
extern int lcd_bl_power_state;

/* interface functions */
void config_camera_on_gpios(void);
void config_camera_off_gpios(void);
void camera_power_mutex_lock(void);
void camera_power_mutex_unlock(void);

struct device* gelato_backlight_dev(void);
void gelato_pwrsink_resume(void);
#endif
