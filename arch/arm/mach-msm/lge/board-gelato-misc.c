/* arch/arm/mach-msm/lge/board-gelato-misc.c
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

#include <linux/types.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/power_supply.h>
#include <asm/setup.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/pmic.h>
#include <mach/msm_battery.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <asm/io.h>
#include <mach/rpc_server_handset.h>
#include <mach/board_lge.h>
#include "board-gelato.h"

#include <linux/syscalls.h>
#include <linux/fcntl.h> 
#include <linux/fs.h>
#include <linux/uaccess.h>


#if defined(CONFIG_LGE_FUEL_GAUGE)
static u32 gelato_battery_capacity(u32 current_soc)
{
	if(current_soc > 100)
		current_soc = 100;
	return current_soc;
}
#endif

static struct msm_psy_batt_pdata msm_psy_batt_data = {
	.voltage_min_design     = 3200,
	.voltage_max_design     = 4200,
	.avail_chg_sources      = AC_CHG | USB_CHG ,
	.batt_technology        = POWER_SUPPLY_TECHNOLOGY_LION,
#if defined(CONFIG_LGE_FUEL_GAUGE)
	.calculate_capacity     = gelato_battery_capacity,
#endif
};

static struct platform_device msm_batt_device = {
	.name           = "msm-battery",
	.id         = -1,
	.dev.platform_data  = &msm_psy_batt_data,
};

/* Vibrator Functions for Android Vibrator Driver */
#define VIBE_IC_VOLTAGE			3300
#define GPIO_LIN_MOTOR_PWM		28

#define GP_MN_CLK_MDIV_REG		0x004C
#define GP_MN_CLK_NDIV_REG		0x0050
#define GP_MN_CLK_DUTY_REG		0x0054

/* about 22.93 kHz, should be checked */
#define GPMN_M_DEFAULT			21
#define GPMN_N_DEFAULT			4500
/* default duty cycle = disable motor ic */
#define GPMN_D_DEFAULT			(GPMN_N_DEFAULT >> 1) 
#define PWM_MAX_HALF_DUTY		((GPMN_N_DEFAULT >> 1) - 80) /* minimum operating spec. should be checked */

#define GPMN_M_MASK				0x01FF
#define GPMN_N_MASK				0x1FFF
#define GPMN_D_MASK				0x1FFF

#define REG_WRITEL(value, reg)	writel(value, (MSM_WEB_BASE+reg))

extern int aat2870bl_ldo_set_level(struct device * dev, unsigned num, unsigned vol);
extern int aat2870bl_ldo_enable(struct device * dev, unsigned num, unsigned enable);

static char *dock_state_string[] = {
	"0",
	"1",
	"2",
};

enum {
	DOCK_STATE_UNDOCKED = 0,
	DOCK_STATE_DESK = 1, /* multikit */
	DOCK_STATE_CAR = 2, /* carkit */
	DOCK_STATE_UNKNOWN,
};

enum {
	KIT_DOCKED = 0,
	KIT_UNDOCKED = 1,
};

static void gelato_desk_dock_detect_callback(int state)
{
	int ret;

	if (state)
		state = DOCK_STATE_DESK;

	ret = lge_gpio_switch_pass_event("dock", state);

	if (ret)
		printk(KERN_INFO "%s: desk dock event report fail\n", __func__);

	return;
}

static int gelato_register_callback(void)
{
	rpc_server_hs_register_callback(gelato_desk_dock_detect_callback);

	return 0;
}

static int gelato_gpio_carkit_work_func(void)
{
	return DOCK_STATE_UNDOCKED;
}

static char *gelato_gpio_carkit_print_state(int state)
{
	return dock_state_string[state];
}

static int gelato_gpio_carkit_sysfs_store(const char *buf, size_t size)
{
	int state;

	if (!strncmp(buf, "undock", size-1))
		state = DOCK_STATE_UNDOCKED;
	else if (!strncmp(buf, "desk", size-1))
		state = DOCK_STATE_DESK;
	else if (!strncmp(buf, "car", size-1))
		state = DOCK_STATE_CAR;
	else
		return -EINVAL;

	return state;
}

static unsigned gelato_carkit_gpios[] = {
};

static struct lge_gpio_switch_platform_data gelato_carkit_data = {
	.name = "dock",
	.gpios = gelato_carkit_gpios,
	.num_gpios = ARRAY_SIZE(gelato_carkit_gpios),
	.irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.wakeup_flag = 1,
	.work_func = gelato_gpio_carkit_work_func,
	.print_state = gelato_gpio_carkit_print_state,
	.sysfs_store = gelato_gpio_carkit_sysfs_store,
	.additional_init = gelato_register_callback,
};

static struct platform_device gelato_carkit_device = {
	.name = "lge-switch-gpio",
	.id = 0,
	.dev = {
		.platform_data = &gelato_carkit_data,
	},
};
int gelato_vibrator_power_set(int enable)
{
//#ifdef	LG_FW_AUDIO_GELATO_MOTOR
#if 1
	struct vreg *vreg_motor;
		
		printk("[Touch] %s() onoff:%d\n",__FUNCTION__, enable);
	
		vreg_motor = vreg_get(0, "gp1");
		
	
		if((IS_ERR(vreg_motor)) ){
			printk("[motor] vreg_get fail : motor\n");
			return -1;
		}
	
		if (enable) {	
			vreg_enable(vreg_motor);
	
		} else {
			vreg_disable(vreg_motor);
			
		}
		return 0;
#else
#if 0
	static int is_enabled = 0;
	struct device *dev = gelato_backlight_dev();

	if (dev==NULL) {
		printk(KERN_ERR "%s: backlight devive get failed\n", __FUNCTION__);
		return -1;
	}

	if (enable) {
		if (is_enabled) {
			//printk(KERN_INFO "vibrator power was enabled, already\n");
			return 0;
		}
		/* 3300 mV for Motor IC */				
		if (aat28xx_ldo_set_level(dev, 1, VIBE_IC_VOLTAGE) < 0) {
			printk(KERN_ERR "%s: vibrator LDO set failed\n", __FUNCTION__);
			return -EIO;
		}
		if (aat28xx_ldo_enable(dev, 1, 1) < 0) {
			printk(KERN_ERR "%s: vibrator LDO enable failed\n", __FUNCTION__);
			return -EIO;
		}
		is_enabled = 1;
	} else {
		if (!is_enabled) {
			//printk(KERN_INFO "vibrator power was disabled, already\n");
			return 0;
		}
		if (aat28xx_ldo_set_level(dev, 1, 0) < 0) {		
			printk(KERN_ERR "%s: vibrator LDO set failed\n", __FUNCTION__);
			return -EIO;
		}
		if (aat28xx_ldo_enable(dev, 1, 0) < 0) {
			printk(KERN_ERR "%s: vibrator LDO disable failed\n", __FUNCTION__);
			return -EIO;
		}
		is_enabled = 0;
	}
	return 0;
#endif 	
#endif
}

int gelato_vibrator_pwm_set(int enable, int amp)
{
	int gain = ((PWM_MAX_HALF_DUTY*amp) >> 7)+ GPMN_D_DEFAULT;

	REG_WRITEL((GPMN_M_DEFAULT & GPMN_M_MASK), GP_MN_CLK_MDIV_REG);
	REG_WRITEL((~( GPMN_N_DEFAULT - GPMN_M_DEFAULT )&GPMN_N_MASK), GP_MN_CLK_NDIV_REG);
	
	if (enable) {
		REG_WRITEL((gain & GPMN_D_MASK), GP_MN_CLK_DUTY_REG);
		gpio_tlmm_config(GPIO_CFG(GPIO_LIN_MOTOR_PWM, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
//		gpio_tlmm_config(GPIO_CFG(GPIO_LIN_MOTOR_PWM, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_direction_output(GPIO_LIN_MOTOR_PWM, 1);
	} else {
		REG_WRITEL(GPMN_D_DEFAULT, GP_MN_CLK_DUTY_REG);
		gpio_tlmm_config(GPIO_CFG(GPIO_LIN_MOTOR_PWM, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);		
//		gpio_tlmm_config(GPIO_CFG(GPIO_LIN_MOTOR_PWM, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_direction_output(GPIO_LIN_MOTOR_PWM, 0);
	}

	return 0;
}

int gelato_vibrator_ic_enable_set(int enable)
{
	/* nothing to do, thunder does not using Motor Enable pin */
	return 0;
}

static struct android_vibrator_platform_data gelato_vibrator_data = {
	.enable_status = 0,
	.power_set = gelato_vibrator_power_set,
	.pwm_set = gelato_vibrator_pwm_set,
	.ic_enable_set = gelato_vibrator_ic_enable_set,
	.amp_value = 109,
	.pwm_gpio = GPIO_LIN_MOTOR_PWM,
};

static struct platform_device android_vibrator_device = {
	.name   = "android-vibrator",
	.id = -1,
	.dev = {
		.platform_data = &gelato_vibrator_data,
	},
};


static void maxim_pmic_subkey_backlight_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	struct vreg *vreg;
	static int mValue = -1;
	vreg = vreg_get(0, "gp17");
	
	if(value != mValue)
	{
		if (value != LED_OFF) {	
			printk("[SUBKEY BACKLIGHT] %s() on:%d, mValue:%d\n",__FUNCTION__, value, mValue);			
			vreg_enable(vreg);
		} else {
			printk("[SUBKEY BACKLIGHT] %s() off:%d, mValue:%d\n",__FUNCTION__, value, mValue);					
			vreg_disable(vreg);
		}
		mValue = value;
	}
}

#define RED_LED_ON		1
#define RED_LED_OFF		0

#define GREEN_LED_ON		1
#define GREEN_LED_OFF		0

static int red_led_status = 0;
static int green_led_status = 0;

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-06-01, Add LED On/Off oem rapi function [START]	
extern void lge_led_control(int val);
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-06-01, Add LED On/Off oem rapi function [END]	

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-06-01, Charging Info sysfs define [START]	
#define BATTERY_CAPACITIY_SYSFS 	"/sys/class/power_supply/battery/capacity"
#define BATTERY_STATUS_SYSFS 	"/sys/class/power_supply/battery/status"
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-06-01, Charging Info sysfs define [END]	

typedef enum { 
	ALL_OFF = 0,
	ONLY_RED_ON = 1,
	ONLY_GREEN_ON = 2,
	RED_ON_GREEN_BLINKING =3,
	CHARGING_FULL = 4,
	GREEN_OFF_RED_BLINKING = 5,
	GREEN_ON_RED_BLINKING = 6,
}type_led ;

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-06-01, get battery status informatino  [START]	
static void battery_status_read(char* status)
{
	int read;
	size_t count;
	int read_size;
	char buf[14];
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	
	memset(buf,0,sizeof(char) * 14);
	
	read = sys_open((const char __user *)BATTERY_STATUS_SYSFS, O_RDONLY , 0);

	if(read < 0) {
		printk(KERN_ERR "%s, STATUS File Open Fail\n",__func__);
		return -1;
	}

	read_size = 0;

	while(sys_read(read, &buf[read_size++], 1) == 1){}	

	sscanf(buf, "%s",status);

	printk(KERN_INFO "%s, battery status : %s\n", __func__, status);

	set_fs(oldfs);
	sys_close(read);
	
}
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-06-01, get battery status informatino  [END]	

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-06-01, get battery capacity informatino  [START]	
static int battery_capacity_read()
{
	int result = 0;

	int read;
	size_t count;
	int read_size;
	char buf[5];
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	
	memset(buf,0,sizeof(char) * 5);
	
	read = sys_open((const char __user *)BATTERY_CAPACITIY_SYSFS, O_RDONLY , 0);

	if(read < 0) {
		printk(KERN_ERR "%s, CAPACITY File Open Fail\n",__func__);
		return -1;
	}

	read_size = 0;

	while(sys_read(read, &buf[read_size++], 1) == 1){}	

	sscanf(buf, "%d",&result);

	printk(KERN_INFO "%s, battery capacity : %d\n", __func__, result);

	set_fs(oldfs);
	sys_close(read);
	
	return result;
}
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-06-01, get battery capacity informatino  [START]	


static void red_led_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	printk("[RED LED] %s() onoff:%d\n",__FUNCTION__, value);
	
	if (value != LED_OFF) {	
		lge_led_control(ONLY_RED_ON);
		red_led_status = RED_LED_ON;
	} else {
		lge_led_control(ALL_OFF);
		red_led_status = RED_LED_OFF;
	}

	printk(KERN_INFO "%s, red_led_status : %d\n",__func__,red_led_status);
}

static void green_led_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	int battery_capacity = 0;
	char status[14];
	
	printk("[GREEN LED] %s() onoff:%d\n",__FUNCTION__, value);

	memset(status, 0, sizeof(char) * 14);
	battery_capacity = battery_capacity_read();
	battery_status_read(status);

	printk(KERN_INFO "%s, status : %s\n", __func__, status);
	printk("%s, battery capacity : %d\n",__func__, battery_capacity);

	if (value != LED_OFF) {	
		if(red_led_status) 
		{
			if((battery_capacity == 100) && ((!strcmp(status, "Charging")) || (!strcmp(status, "Full"))))	// chg_complete moses.son@lge.com 
			{
				printk(KERN_INFO "%s(), green led on blinking red\n",__func__);
				printk(KERN_INFO "%s(), send 6\n",__func__);
				lge_led_control(GREEN_ON_RED_BLINKING);	
			}
			else
			{
				printk(KERN_INFO "%s(), red led on blinking green\n",__func__);
				printk(KERN_INFO "%s(), send 3\n",__func__);				
				lge_led_control(RED_ON_GREEN_BLINKING);				
			}
		}
		else
		{
			if((battery_capacity == 100) && ((!strcmp(status, "Charging")) || (!strcmp(status, "Full"))))	// chg_complete moses.son@lge.com 
			{
				printk(KERN_INFO "%s(), green on led On red Off during charging, battery full\n",__func__);
				printk(KERN_INFO "%s(), send 4\n",__func__);
				lge_led_control(CHARGING_FULL);		
			}
			else
			{	
				printk(KERN_INFO "%s(), green on led On red Off nonblinking\n",__func__);
				printk(KERN_INFO "%s(), send 2\n",__func__);
				lge_led_control(ONLY_GREEN_ON);		
			}
		}
		green_led_status = GREEN_LED_ON;
	} 
	else 
	{
		if(red_led_status)
		{
			lge_led_control(ONLY_RED_ON);
			printk("[GREEN LED] %s() green off red led On\n",__FUNCTION__);

		}
		else
		{
			lge_led_control(ALL_OFF);
			printk("[GREEN LED] %s() green, red led Off\n",__FUNCTION__);			
		}
		green_led_status = GREEN_LED_OFF;
	}
}

#if 0
static void red_led_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	struct vreg* vreg_red = NULL;
	vreg_red = vreg_get(NULL, "gp14");
	
	printk("[RED LED] %s() onoff:%d\n",__FUNCTION__, value);
	
	if (value != LED_OFF) {	
		vreg_enable(vreg_red);
	} else {
		vreg_disable(vreg_red);
	}

	printk(KERN_INFO "%s, red_led_status : %d\n",__func__,red_led_status);
}

static void green_led_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	struct vreg* vreg_green = NULL;
	vreg_green = vreg_get(NULL, "gp15");
	
	printk("[GREEN LED] %s() onoff:%d\n",__FUNCTION__, value);
	
	if (value != LED_OFF) {	
		vreg_enable(vreg_green);		
	} else {
		vreg_disable(vreg_green);				
	}
	printk(KERN_INFO "%s, green_led_status : %d\n",__func__,green_led_status);
}
#endif


static void blue_led_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
}

static int check = LED_OFF;

static void flag_led_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	check = value;
}

struct led_classdev gelato_custom_leds[] = {
	{
		.name = "button-backlight",
		.brightness_set = maxim_pmic_subkey_backlight_set,
		.brightness = LED_OFF,
	},
	{
		.name = "red",
		.brightness_set = red_led_set,
		.brightness = LED_OFF,
	},
	{
		.name = "green",
		.brightness_set = green_led_set,
		.brightness = LED_OFF,
	},	
	{
		.name = "blue",
		.brightness_set = blue_led_set,
		.brightness = LED_OFF,
	},
};


static int register_leds(struct platform_device *pdev)
{
	int rc;
	int i;

	for(i = 0 ; i < ARRAY_SIZE(gelato_custom_leds) ; i++) {
		rc = led_classdev_register(&pdev->dev, &gelato_custom_leds[i]);
		if (rc) {
			dev_err(&pdev->dev, "unable to register led class driver : gelato_custom_leds \n");
			return rc;
		}
	}

	return rc;
}

static void unregister_leds (void)
{
	int i;
	for (i = 0; i< ARRAY_SIZE(gelato_custom_leds); ++i)
		led_classdev_unregister(&gelato_custom_leds[i]);
}

static void suspend_leds (void)
{
	led_classdev_suspend(&gelato_custom_leds[0]);
}

static void resume_leds (void)
{
	led_classdev_resume(&gelato_custom_leds[0]);
}

int keypad_led_set(unsigned char value)
{
#ifndef CONFIG_MACH_MSM7X27_GELATO_DOP
	struct vreg *vreg;
	static int mValue = -1;
	vreg = vreg_get(0, "gp16");

	if(value != mValue)
	{
		if (value != LED_OFF) { 
			printk("[MAINKEY BACKLIGHT] %s() on:%d, mValue:%d\n",__FUNCTION__, value, mValue);			
			vreg_enable(vreg);
		} else {
			printk("[MAINKEY BACKLIGHT] %s() off:%d, mValue:%d\n",__FUNCTION__, value, mValue);					
			vreg_disable(vreg);
		}
		mValue = value;
	}
#endif
	return 0;
}

static struct msm_pmic_leds_pdata leds_pdata = {
	.custom_leds		= gelato_custom_leds,
	.register_custom_leds	= register_leds,
	.unregister_custom_leds	= unregister_leds,
	.suspend_custom_leds	= suspend_leds,
	.resume_custom_leds	= resume_leds,
	.msm_keypad_led_set	= keypad_led_set,
};

static struct platform_device msm_device_pmic_leds = {
	.name = "pmic-leds",
	.id = -1,
	.dev.platform_data = &leds_pdata,
};


/* ear sense driver */
static char *ear_state_string[] = {
	"0",
	"1",
};

enum {
	EAR_STATE_EJECT = 0,
	EAR_STATE_INJECT = 1, 
};

enum {
	EAR_EJECT = 0,
	EAR_INJECT = 1,
};

#if 0
static int gelato_hs_mic_bias_power(int enable)
{
	struct vreg *hs_bias_vreg;
	static int is_enabled = 0;

	hs_bias_vreg = vreg_get(NULL, "ruim");

	if (IS_ERR(hs_bias_vreg)) {
		printk(KERN_ERR "%s: vreg_get failed\n", __FUNCTION__);
		return PTR_ERR(hs_bias_vreg);
	}

	if (enable) {
		if (is_enabled) {
			//printk(KERN_INFO "HS Mic. Bias power was enabled, already\n");
			return 0;
		}

		if (vreg_set_level(hs_bias_vreg, 2600) <0) {
			printk(KERN_ERR "%s: vreg_set_level failed\n", __FUNCTION__);
			return -EIO;
		}

		if (vreg_enable(hs_bias_vreg) < 0 ) {
			printk(KERN_ERR "%s: vreg_enable failed\n", __FUNCTION__);
			return -EIO;
		}
		is_enabled = 1;
	} else {
		if (!is_enabled) {
			//printk(KERN_INFO "HS Mic. Bias power was disabled, already\n");
			return 0;
		}

		if (vreg_set_level(hs_bias_vreg, 0) <0) {
			printk(KERN_ERR "%s: vreg_set_level failed\n", __FUNCTION__);
			return -EIO;
		}

		if (vreg_disable(hs_bias_vreg) < 0) {
			printk(KERN_ERR "%s: vreg_disable failed\n", __FUNCTION__);
			return -EIO;
		}
		is_enabled = 0;
	}
	return 0;
}
#endif 

#ifdef CONFIG_LGE_HEADSET_2GPIO
static struct gpio_h2w_platform_data gelato_h2w_data = {
	.gpio_detect = 29,
	.gpio_button_detect = 41,
	.gpio_jpole = 30,
	.gpio_mic_bias_en = 26,

};

static struct platform_device gelato_h2w_device = {
	.name = "gpio-h2w",
	.id = -1,
	.dev = {
		.platform_data = &gelato_h2w_data,
	},
};
#endif /*CONFIG_LGE_AUDIO_HEADSET*/

static int gelato_gpio_earsense_work_func(void)
{
	int state;
	int gpio_value;

	gpio_value = gpio_get_value(GPIO_EAR_SENSE);
	printk(KERN_INFO"%s: ear sense detected : %s\n", __func__, 
			gpio_value?"injected":"ejected");
	if (gpio_value == EAR_EJECT) {
		state = EAR_STATE_EJECT;
		/* LGE_CHANGE_S, [junyoub.an] , 2010-05-28, comment out to control at ARM9 part*/
		gpio_set_value(GPIO_HS_MIC_BIAS_EN, 0);
		/* LGE_CHANGE_E, [junyoub.an] , 2010-05-28, comment out to control at ARM9 part*/
	} else {
		state = EAR_STATE_INJECT;
		/* LGE_CHANGE_S, [junyoub.an] , 2010-05-28, comment out to control at ARM9 part*/
		gpio_set_value(GPIO_HS_MIC_BIAS_EN, 1);
		/* LGE_CHANGE_E, [junyoub.an] , 2010-05-28, comment out to control at ARM9 part*/
	}

	return state;
}

static char *gelato_gpio_earsense_print_state(int state)
{
	return ear_state_string[state];
}

static int gelato_gpio_earsense_sysfs_store(const char *buf, size_t size)
{
	int state;

	if (!strncmp(buf, "eject", size - 1))
		state = EAR_STATE_EJECT;
	else if (!strncmp(buf, "inject", size - 1))
		state = EAR_STATE_INJECT;
	else
		return -EINVAL;

	return state;
}

static unsigned gelato_earsense_gpios[] = {
	GPIO_EAR_SENSE,
};

static struct lge_gpio_switch_platform_data gelato_earsense_data = {
	.name = "h2w",
	.gpios = gelato_earsense_gpios,
	.num_gpios = ARRAY_SIZE(gelato_earsense_gpios),
	.irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.wakeup_flag = 1,
	.work_func = gelato_gpio_earsense_work_func,
	.print_state = gelato_gpio_earsense_print_state,
	.sysfs_store = gelato_gpio_earsense_sysfs_store,
};

static struct platform_device gelato_earsense_device = {
	.name   = "lge-switch-gpio",
	.id = 1,
	.dev = {
		.platform_data = &gelato_earsense_data,
	},
};

static struct platform_device *gelato_misc_devices[] __initdata = {
	/* LGE_CHANGE
	 * ADD VS740 BATT DRIVER IN GELATO
	 * 2010-05-13, taehung.kim@lge.com
	 */
	&msm_batt_device, 
	&msm_device_pmic_leds,
	&android_vibrator_device,
	//&gelato_carkit_device,
#ifdef CONFIG_LGE_HEADSET_2GPIO
	&gelato_h2w_device
#endif
};

void __init lge_add_misc_devices(void)
{
	platform_add_devices(gelato_misc_devices, ARRAY_SIZE(gelato_misc_devices));
}

