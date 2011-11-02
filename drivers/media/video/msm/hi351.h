/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef ISX005_H
#define ISX005_H

#include <linux/types.h>
#include <mach/camera.h>


//[2010.12.28][JKCHAE] sensor porting
#define READ_INIT_DATA_FROM_FILE    0
#define MDP_REMOVAL                 0

//[2011.01.13][JKCHAE] Sensor Porting
#ifndef CAMERA_SENSOR
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
#endif

#if (CAMERA_SENSOR == SAMSUNG_CAMERA_SENSOR)
	#define USE_BURSTMODE				0
	#define USE_I2C_BURSTMODE			1
	#define CAPTURE_WITH_15FPS          0

	#if (READ_INIT_DATA_FROM_FILE == 1)
		#define TUNING_THREAD_ENABLE		0 
	#else
		#define TUNING_THREAD_ENABLE		0 
	#endif


#endif

//[2011.01.21][JKCHAE] Hynix Rev.A , Rev.B
#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	#define HYNIX_REV_A			1
	#define HYNIX_REV_B			2

	#ifdef CONFIG_LGE_PCB_REV_A 
		#define HYNIX_CAMERA_SENSOR_REV HYNIX_REV_A
	#else
		#define HYNIX_CAMERA_SENSOR_REV HYNIX_REV_B
	#endif
#endif

#if (CAMERA_SENSOR == SAMSUNG_CAMERA_SENSOR)
	#define I2C_SLAVE_ADDR_FOR_READ_SAMSUNG_SENSOR (0x79 >> 1) //0x79

	#define AF_CHECK_RET_IDLE	   0x0000
	#define AF_CHECK_RET_PROGRESS  0x0001
	#define AF_CHECK_RET_SUCCESS   0x0002
	#define AF_CHECK_RET_LOWCONF   0x0003
	#define AF_CHECK_RET_CANCELED  0x0004

	#define CAMSENSOR_FIXED_FPS    0x0002
	#define	CAMSENSOR_VARIABLE_FPS 0x0000

	//refered from init register
	#define AF_FOCUS_STEP_0 	   0x001E
	#define AF_FOCUS_STEP_1 	   0x0021
	#define AF_FOCUS_STEP_2 	   0x0024
	#define AF_FOCUS_STEP_3 	   0x0027
	#define AF_FOCUS_STEP_4 	   0x002A
	#define AF_FOCUS_STEP_5 	   0x002D
	#define AF_FOCUS_STEP_6 	   0x0030
	#define AF_FOCUS_STEP_7 	   0x0033
	#define AF_FOCUS_STEP_8 	   0x0036
	#define AF_FOCUS_STEP_9 	   0x0039
	#define AF_FOCUS_STEP_10	   0x003C
	#define AF_FOCUS_STEP_11	   0x0040
	#define AF_FOCUS_STEP_12	   0x0043
	#define AF_FOCUS_STEP_13	   0x0046
	#define AF_FOCUS_STEP_14	   0x004A
	#define AF_FOCUS_STEP_15	   0x004E
	#define AF_FOCUS_STEP_16	   0x005A

#endif

extern struct isx005_reg isx005_regs;

enum isx005_width {
	BYTE_LEN,
	WORD_LEN,
#if ((CAMERA_SENSOR == SAMSUNG_CAMERA_SENSOR) && (USE_BURSTMODE == 1))
	BURST_LEN,
#endif
};

struct isx005_register_address_value_pair {
	uint16_t register_address;
	uint16_t register_value;
	enum isx005_width register_length;
};

struct isx005_reg {
	const struct isx005_register_address_value_pair *init_reg_settings;
	uint16_t init_reg_settings_size;
	const struct isx005_register_address_value_pair *tuning_reg_settings;
	uint16_t tuning_reg_settings_size;
	
	const struct isx005_register_address_value_pair *prev_reg_settings;
	uint16_t prev_reg_settings_size;
	const struct isx005_register_address_value_pair *snap_reg_settings;
	uint16_t snap_reg_settings_size;
	
	const struct isx005_register_address_value_pair *af_normal_reg_settings;
	uint16_t af_normal_reg_settings_size;
	//LGE_CHANGE[byungsik.choi@lge.com]2010-09-09 add auto focus mode
	const struct isx005_register_address_value_pair *af_auto_reg_settings;
	uint16_t af_auto_reg_settings_size;
	const struct isx005_register_address_value_pair *af_macro_reg_settings;
	uint16_t af_macro_reg_settings_size;
	const struct isx005_register_address_value_pair *af_manual_reg_settings;
	uint16_t af_manual_reg_settings_size;
	
	const struct isx005_register_address_value_pair *af_start_reg_settings;
	uint16_t af_start_reg_settings_size;

	//[2011.01.13][JKCHAE] Sensor Porting (AF)
	const struct isx005_register_address_value_pair *af_cancel_reg_settings;
	uint16_t af_cancel_reg_settings_size;

	const struct isx005_register_address_value_pair 
		*scene_auto_reg_settings;
	uint16_t scene_auto_reg_settings_size;	
	const struct isx005_register_address_value_pair 
		*scene_portrait_reg_settings;
	uint16_t scene_portrait_reg_settings_size;
	const struct isx005_register_address_value_pair 
		*scene_landscape_reg_settings;
	uint16_t scene_landscape_reg_settings_size;
	const struct isx005_register_address_value_pair 
		*scene_sports_reg_settings;
	uint16_t scene_sports_reg_settings_size;
	const struct isx005_register_address_value_pair 
		*scene_sunset_reg_settings;
	uint16_t scene_sunset_reg_settings_size;
	const struct isx005_register_address_value_pair 
		*scene_night_reg_settings;
	uint16_t scene_night_reg_settings_size;
};

			
/* this value is defined in Android native camera */
enum isx005_focus_mode {
	FOCUS_NORMAL,
	FOCUS_MACRO,
	FOCUS_AUTO,
	FOCUS_MANUAL,
};

/* this value is defined in Android native camera */
enum isx005_wb_type {
	CAMERA_WB_MIN_MINUS_1,
	CAMERA_WB_AUTO = 1,  /* This list must match aeecamera.h */
	CAMERA_WB_CUSTOM,
	CAMERA_WB_INCANDESCENT,
	CAMERA_WB_FLUORESCENT,
	CAMERA_WB_DAYLIGHT,
	CAMERA_WB_CLOUDY_DAYLIGHT,
	CAMERA_WB_TWILIGHT,
	CAMERA_WB_SHADE,
	CAMERA_WB_MAX_PLUS_1
};

enum isx005_antibanding_type {
	CAMERA_ANTIBANDING_OFF,
	CAMERA_ANTIBANDING_60HZ,
	CAMERA_ANTIBANDING_50HZ,
	CAMERA_ANTIBANDING_AUTO,
	CAMERA_MAX_ANTIBANDING,
};

/* Enum Type for different ISO Mode supported */
enum isx005_iso_value {
	CAMERA_ISO_AUTO = 0,
	CAMERA_ISO_DEBLUR,
	CAMERA_ISO_100,
	CAMERA_ISO_200,
	CAMERA_ISO_400,
	CAMERA_ISO_800,
	CAMERA_ISO_MAX
};

/* Enum type for scene mode */
enum {
	CAMERA_SCENE_AUTO = 1,
	CAMERA_SCENE_PORTRAIT,
	CAMERA_SCENE_LANDSCAPE,
	CAMERA_SCENE_SPORTS,
	CAMERA_SCENE_NIGHT,
	CAMERA_SCENE_SUNSET,
};

#if defined(CONFIG_MACH_MSM7X27_THUNDERG) || \
	defined(CONFIG_MACH_MSM7X27_THUNDERC) || \
	defined(CONFIG_MACH_MSM7X27_GELATO)
/* LGE_CHANGE_S. Change code to apply new LUT for display quality.
 * 2010-08-13. minjong.gong@lge.com */
extern void mdp_load_thunder_lut(int lut_type);
#endif

#endif /* ISX005_H */
