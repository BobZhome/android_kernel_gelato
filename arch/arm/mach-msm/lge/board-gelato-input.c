/* arch/arm/mach-msm/board-gelato-input.c
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
#include <linux/gpio_event.h>
#include <linux/keyreset.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/board.h>
#include <mach/board_lge.h>
#include <mach/rpc_server_handset.h>

#include "board-gelato.h"
#include <linux/synaptics_i2c_rmi.h>	//20100705 myeonggyu.son@lge.com [MS690] synaptcis touch series


/* head set device */
static struct msm_handset_platform_data hs_platform_data = {
	.hs_name = "7k_handset",
	.pwr_key_delay_ms = 500, /* 0 will disable end key */
};

static struct platform_device hs_device = {
	.name   = "msm-handset",
	.id     = -1,
	.dev    = {
		.platform_data = &hs_platform_data,
	},
};

/* pp2106 qwerty keypad device */
static unsigned short pp2106_keycode[PP2106_KEYPAD_ROW][PP2106_KEYPAD_COL] =
{
	 /*0*/			  /*1*/ 		  /*2*/ 		  /*3*/ 		  /*4*/ 		  /*5*/ 		  /*6*/ 		  /*7*/
/*0*/{KEY_BACKSPACE,  KEY_RIGHT,  	  KEY_COMMA,   	  KEY_MINUS,   	  KEY_SPACE,	  KEY_T,		  KEY_V,		  KEY_G},
/*1*/{KEY_NEWLINE,	  KEY_EQUAL,	  KEY_SEMICOLON,  KEY_P,		  KEY_EMAIL,   	  KEY_R,		  KEY_C,		  KEY_F},
/*2*/{KEY_ENTER,   	  KEY_QUESTION,   KEY_L,		  KEY_O,		  KEY_SYM,    	  KEY_E,		  KEY_X,		  KEY_D},
/*3*/{KEY_DOWN,	  	  KEY_M,		  KEY_K,		  KEY_I,		  KEY_SMILE,      KEY_W,		  KEY_Z,		  KEY_S},
/*4*/{KEY_LEFT,	  	  KEY_N,		  KEY_J,		  KEY_U,		  KEY_RIGHTALT,	  KEY_Q,		  KEY_LEFTSHIFT,  KEY_A},
/*5*/{KEY_DOTCOM, 	  KEY_B,		  KEY_H,		  KEY_Y,		  KEY_HOME, 	  KEY_SEARCH,	  KEY_MENU, 	  KEY_BACK},
/*6*/{KEY_UP, 	  	  KEY_DOT,		  KEY_RESERVED,   KEY_RESERVED,   KEY_RESERVED,   KEY_RESERVED,   KEY_RESERVED,   KEY_RESERVED},
/*7*/{KEY_RESERVED,   KEY_RESERVED,   KEY_RESERVED,   KEY_RESERVED,   KEY_RESERVED,   KEY_RESERVED,   KEY_RESERVED,   KEY_RESERVED},
};

static struct pp2106_platform_data pp2106_pdata = {
	.keypad_row = PP2106_KEYPAD_ROW,
	.keypad_col = PP2106_KEYPAD_COL,
	.keycode = (unsigned char *)pp2106_keycode,
	.reset_pin = GPIO_PP2106_RESET,
	.irq_pin = GPIO_PP2106_IRQ,
	.sda_pin = GPIO_PP2106_SDA,
	.scl_pin = GPIO_PP2106_SCL,
};

static struct platform_device qwerty_device = {
	.name = "kbd_pp2106",
	.id = -1,
	.dev = {
		.platform_data = &pp2106_pdata,
	},
};

static struct bu52031_platform_data bu52031_pdata = {
	.irq_pin = GPIO_BU52031_IRQ,
	.prohibit_time = 100,
};

static struct platform_device hallic_device = {
	.name = "hall-ic",
	.id = -1,
	.dev = {
		.platform_data = &bu52031_pdata,
	},
};

static unsigned int keypad_row_gpios[] = {38, 37, 36};	// {KEY_SENSE[0], KEY_SENSE[1], KEY_SENSE[2]}
static unsigned int keypad_col_gpios[] = {35, 34, 33};	// {KEY_DRV[0], KEY_DRV[1], KEY_DRV[2], KEY_DRV[3]}

#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios) + (col))

static const unsigned short keypad_keymap_gelato[ARRAY_SIZE(keypad_col_gpios) * ARRAY_SIZE(keypad_row_gpios)] = {
#ifdef CONFIG_MACH_MSM7X27_GELATO_DOP
	[KEYMAP_INDEX(0, 0)] = KEY_VOLUMEDOWN,
	[KEYMAP_INDEX(0, 1)] = KEY_VOLUMEUP,
#else
#ifdef CONFIG_LGE_PCB_REV_B
	[KEYMAP_INDEX(0, 0)] = KEY_VOLUMEUP,
	[KEYMAP_INDEX(0, 1)] = KEY_VOLUMEDOWN,
#else
	[KEYMAP_INDEX(0, 0)] = KEY_VOLUMEDOWN,
	[KEYMAP_INDEX(0, 1)] = KEY_VOLUMEUP,
#endif
#endif
	[KEYMAP_INDEX(0, 2)] = KEY_FOCUS,
	[KEYMAP_INDEX(1, 0)] = KEY_MENU,
	[KEYMAP_INDEX(1, 1)] = KEY_HOME,
	[KEYMAP_INDEX(1, 2)] = KEY_CAMERA,
	[KEYMAP_INDEX(2, 0)] = KEY_BACK,
	[KEYMAP_INDEX(2, 1)] = KEY_SEARCH,
	[KEYMAP_INDEX(2, 2)] = KEY_RESERVED,
};

static struct gpio_event_matrix_info gelato_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_gelato,
	.output_gpios	= keypad_col_gpios,
	.input_gpios	= keypad_row_gpios,
	.noutputs	= ARRAY_SIZE(keypad_col_gpios),
	.ninputs	= ARRAY_SIZE(keypad_row_gpios),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS
	//.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS | GPIOKPF_DRIVE_INACTIVE
};

static struct gpio_event_info *gelato_keypad_info[] = {
	&gelato_keypad_matrix_info.info
};

static struct gpio_event_platform_data gelato_keypad_data = {
	.name		= "gelato_keypad",
	.info		= gelato_keypad_info,
	.info_count	= ARRAY_SIZE(gelato_keypad_info)
};

struct platform_device keypad_device_gelato= {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &gelato_keypad_data,
	},
};

/* keyreset platform device */
static int gelato_reset_keys_up[] = {
	KEY_HOME,
	0
};

static struct keyreset_platform_data gelato_reset_keys_pdata = {
	.keys_up = gelato_reset_keys_up,
	.keys_down = {
		KEY_BACK,
		KEY_VOLUMEDOWN,
		KEY_MENU,
		0
	},
};

struct platform_device gelato_reset_keys_device = {
	.name = KEYRESET_NAME,
	.dev.platform_data = &gelato_reset_keys_pdata,
};

/* input platform device */
static struct platform_device *gelato_input_devices[] __initdata = {
	&hs_device,
#ifndef CONFIG_MACH_MSM7X27_GELATO_DOP
	&qwerty_device,
	&hallic_device,
#endif
	&keypad_device_gelato,
	&gelato_reset_keys_device,
};

static struct gpio_i2c_pin ts_i2c_pin[] = {
	[0] = {
		.sda_pin	= TS_GPIO_I2C_SDA,
		.scl_pin	= TS_GPIO_I2C_SCL,
		.reset_pin	= 0,
		.irq_pin	= TS_GPIO_IRQ,
	},
};

static struct i2c_gpio_platform_data ts_i2c_pdata = {
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.udelay				= 2,
};

static struct platform_device ts_i2c_device = {
	.name	= "i2c-gpio",
	.dev.platform_data = &ts_i2c_pdata,
};
//20100705 myeonggyu.son@lge.com [MS690] synatics touch series [START]

static int ts_config_gpio(int config)
{
	if (config)
	{		/* for wake state */
		gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	}
	else
	{		/* for sleep state */
		gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

		gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SDA, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SCL, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	}

	return 0;
}

#define TS_POWER_ON 1
#define TS_POWER_OFF 0

static int ts_power_on = TS_POWER_OFF;

static int ts_set_vreg(int onoff)
{
	struct vreg *vreg_touch;
	struct vreg *vreg_pullup;
	int rc;

	printk("[Touch] ts_set_vreg() onoff:%d, ts_power_on=%d \n", onoff, ts_power_on);

	vreg_touch = vreg_get(0, "synt");
// LGE_CHANGE [yt.kim@lge.com] 2011-01-13, touch_VDDIO [START] 	
#ifdef CONFIG_MACH_MSM7X27_GELATO_DOP // yt_test
	vreg_pullup = vreg_get(0, "gp6");
#endif
// LGE_CHANGE [yt.kim@lge.com] 2011-01-13, touch_VDDIO [END] 

	if((IS_ERR(vreg_touch)) || (IS_ERR(vreg_pullup))) {
		printk("[Touch] vreg_get fail : touch\n");
		return -1;
	}
	
	if (onoff) {
		if (ts_power_on == TS_POWER_OFF)
		{
		ts_config_gpio(1);

		vreg_set_level(vreg_touch, 3000);
		vreg_enable(vreg_touch);
// LGE_CHANGE [yt.kim@lge.com] 2011-01-13, touch_VDDIO [START] 		
#ifdef CONFIG_MACH_MSM7X27_GELATO_DOP
		msleep(15);	// wait 15ms
		
		vreg_set_level(vreg_pullup, 2600);
		vreg_enable(vreg_pullup);
#endif
// LGE_CHANGE [yt.kim@lge.com] 2011-01-13, touch_VDDIO [START] 
			ts_power_on = TS_POWER_ON;
		}
	} 
	else
	{
		if (ts_power_on == TS_POWER_ON)
		{
		ts_config_gpio(0);
// LGE_CHANGE [yt.kim@lge.com] 2011-01-13, touch_VDDIO [START] 		
#ifdef CONFIG_MACH_MSM7X27_GELATO_DOP
		vreg_disable(vreg_pullup);
#endif
// LGE_CHANGE [yt.kim@lge.com] 2011-01-13, touch_VDDIO [END] 
		vreg_disable(vreg_touch);
			ts_power_on = TS_POWER_OFF;
		}

	}
	return 0;
}

//20100705 myeonggyu.son@lge.com [MS690] synatics touch series [END]

//20100705 myeonggyu.son@lge.com [MS690] synatics touch series [START]
#if 1
static struct synaptics_i2c_rmi_platform_data ts_pdata = {
	.version = 0x0,
	.irqflags = IRQF_TRIGGER_FALLING,
	.use_irq = true,
	.power = ts_set_vreg
};
#else
static struct touch_platform_data ts_pdata = {
	.ts_x_min = TS_X_MIN,
	.ts_x_max = TS_X_MAX,
	.ts_y_min = TS_Y_MIN,
	.ts_y_max = TS_Y_MAX,
	.power = ts_set_vreg,
	.irq 	  = TS_GPIO_IRQ,
	.scl      = TS_GPIO_I2C_SCL,
	.sda      = TS_GPIO_I2C_SDA,
};
#endif
//20100705 myeonggyu.son@lge.com [MS690] synatics touch series [END]

//20100705 myeonggyu.son@lge.com [MS690] synatics touch series [START]
#if 1
static struct i2c_board_info ts_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("synaptics-rmi-ts", TS_I2C_SLAVE_ADDR),
		.type = "synaptics-rmi-ts",
		.platform_data = &ts_pdata,
	},
};
#else
static struct i2c_board_info ts_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("touch_mcs6000", TS_I2C_SLAVE_ADDR),
		.type = "touch_mcs6000",
		.platform_data = &ts_pdata,
	},
};
#endif
//20100705 myeonggyu.son@lge.com [MS690] synatics touch series [END]

static void __init gelato_init_i2c_touch(int bus_num)
{
	ts_i2c_device.id = bus_num;

	init_gpio_i2c_pin(&ts_i2c_pdata, ts_i2c_pin[0],	&ts_i2c_bdinfo[0]);
	i2c_register_board_info(bus_num, &ts_i2c_bdinfo[0], 1);
	platform_device_register(&ts_i2c_device);
}

/* accelerometer */
//#if defined(CONFIG_LGE_PCB_REV_A)
#ifdef CONFIG_LGE_PCB_REV_A
#ifdef CONFIG_MACH_MSM7X27_GELATO_DOP
//K3DH Rev A
static int k3dh_config_gpio(int config)
{
	if (config) { /* for wake state */
	}
	else { /* for sleep state */
		gpio_tlmm_config(GPIO_CFG(ACCEL_GPIO_INT, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	}
	return 0;
}
#else 
//KR3DH Rev A
static int kr3dh_config_gpio(int config)
{
	if (config) { /* for wake state */
	}
	else { /* for sleep state */
		gpio_tlmm_config(GPIO_CFG(ACCEL_GPIO_INT, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	}

	return 0;
}
#endif
//Rev B
#else 
static int k3dh_config_gpio(int config)
{
	if (config) { /* for wake state */
	}
	else { /* for sleep state */
		gpio_tlmm_config(GPIO_CFG(ACCEL_GPIO_INT, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	}
	return 0;
}
#endif
static int kr_init(void)
{
	return 0;
}

static void kr_exit(void)
{
	return 0; 
}

#define ECOM_POWER_OFF		0
#define ECOM_POWER_ON		1
static int ecom_is_power_on = ECOM_POWER_OFF;
static int power_on(void)
{
	int ret = 0;
	struct vreg *gp3_vreg = vreg_get(0, "gp3");


	if (ecom_is_power_on == ECOM_POWER_OFF) {
		vreg_set_level(gp3_vreg, 3000);
		vreg_enable(gp3_vreg);
		ecom_is_power_on = ECOM_POWER_ON;
	}
	
	return 0;
}

static int power_off(void)
{
	int ret = 0;
	struct vreg *gp3_vreg = vreg_get(0, "gp3");

	printk("[Accelrometer] %s() Poweroff\n",__FUNCTION__);
	if (ecom_is_power_on == ECOM_POWER_ON) {
		vreg_disable(gp3_vreg);
		ecom_is_power_on = ECOM_POWER_OFF;
	}
	
	return 0;
}
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-12, KR3DH, K3DH Platform Data[START]
//#if defined(CONFIG_LGE_PCB_REV_A)
#ifdef CONFIG_LGE_PCB_REV_A
#ifdef CONFIG_MACH_MSM7X27_GELATO_DOP
//K3DH
struct k3dh_platform_data k3dh_data = {
	.poll_interval = 100,
	.min_interval = 0,
	.g_range = 0x00,
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,

	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 0,

	.power_on = power_on,
	.power_off = power_off,
	.kr_init = kr_init,
	.kr_exit = kr_exit,
	.gpio_config = k3dh_config_gpio,
};

#else 
//KR3DH
struct kr3dh_platform_data kr3dh_data = {
	.poll_interval = 100,
	.min_interval = 0,
	.g_range = 0x00,
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,

	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 0,

	.power_on = power_on,
	.power_off = power_off,
	.kr_init = kr_init,
	.kr_exit = kr_exit,
	.gpio_config = kr3dh_config_gpio,
};
#endif
//Rev B ~
#else 
struct k3dh_platform_data k3dh_data = {
	.poll_interval = 25,
	.min_interval = 0,
	.g_range = 0x00,
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,

	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 0,

	.power_on = power_on,
	.power_off = power_off,
	.kr_init = kr_init,
	.kr_exit = kr_exit,
	.gpio_config = k3dh_config_gpio,
};
#endif

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-12, KR3DH, K3DH Platform Data[END]
static struct gpio_i2c_pin accel_i2c_pin[] = {
	[0] = {
		.sda_pin	= ACCEL_GPIO_I2C_SDA,
		.scl_pin	= ACCEL_GPIO_I2C_SCL,
		.reset_pin	= 0,
		.irq_pin	= ACCEL_GPIO_INT,
	},
};

static struct i2c_gpio_platform_data accel_i2c_pdata = {
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.udelay = 2,
};

static struct platform_device accel_i2c_device = {
	.name = "i2c-gpio",
	.dev.platform_data = &accel_i2c_pdata,
};

//#if defined(CONFIG_LGE_PCB_REV_A) 
#ifdef CONFIG_LGE_PCB_REV_A
#ifdef CONFIG_MACH_MSM7X27_GELATO_DOP
//K3DH
static struct i2c_board_info accel_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("K3DH", ACCEL_I2C_ADDRESS),
		.type = "K3DH",
		.platform_data = &k3dh_data,
	},
};

#else
//KR3DH
static struct i2c_board_info accel_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("KR3DH", ACCEL_I2C_ADDRESS),
		.type = "KR3DH",
		.platform_data = &kr3dh_data,
	},
};
#endif
#else //Rev B
//K3DH
static struct i2c_board_info accel_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("K3DH", ACCEL_I2C_ADDRESS),
		.type = "K3DH",
		.platform_data = &k3dh_data,
	},
};
#endif

static void __init gelato_init_i2c_acceleration(int bus_num)
{
	accel_i2c_device.id = bus_num;

	init_gpio_i2c_pin(&accel_i2c_pdata, accel_i2c_pin[0], &accel_i2c_bdinfo[0]);

	i2c_register_board_info(bus_num, &accel_i2c_bdinfo[0], 1);	

	platform_device_register(&accel_i2c_device);

	printk(KERN_INFO "init i2c acceleration\n");
}

static int ecom_power_set(unsigned char onoff)
{
	int ret = 0;
	struct vreg *gp3_vreg = vreg_get(0, "gp3");

	printk("[Ecompass] %s() onoff %d, prev_status %d\n",__FUNCTION__, 
			onoff, ecom_is_power_on);

	if (onoff) {
		if (ecom_is_power_on == ECOM_POWER_OFF) {
			vreg_set_level(gp3_vreg, 3000);
			vreg_enable(gp3_vreg);
			ecom_is_power_on = ECOM_POWER_ON;
		}
	} else {
		if (ecom_is_power_on == ECOM_POWER_ON) {

			vreg_disable(gp3_vreg);
			ecom_is_power_on = ECOM_POWER_OFF;
		}
	}
	return ret;
}

static struct ecom_platform_data ecom_pdata = {
	.pin_int        	= 0,
	.pin_rst			= ECOM_GPIO_RST,
	.power          	= ecom_power_set,
};
// LGE_CHANGE [dojip.kim@lge.com] 2010-07-21, proxi power control (from MS690)
#define PROX_POWER_OFF		0
#define PROX_POWER_ON		1
int lcd_led_en = PROX_POWER_OFF;
static int prox_is_power_status = PROX_POWER_OFF;

static int prox_power_set(unsigned char onoff)
{
	int ret = 0;
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-04, proxi power control [START]
	struct device *dev = gelato_backlight_dev();

	if(onoff){
		if(PROX_POWER_OFF == prox_is_power_status){

#if CONFIG_MACH_MSM7X27_GELATO_DOP
#ifdef CONFIG_LGE_PCB_REV_A
			//power on 2.8V
			ret = aat28xx_ldo_set_level(dev,PROXI_LDO_NO_VCC, 2800);
#else  
			// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-02-22, [gelato] Rev B, Proximity Sensor change to apds9190 Voltage change 2.8V[START]
			//power on 2.8V
			ret = aat28xx_ldo_set_level(dev,PROXI_LDO_NO_VCC, 2800);
			// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-02-22, [gelato] Rev B, Proximity Sensor change to apds9190 Voltage change 2.8V[END]
#endif			
#else
#ifdef CONFIG_LGE_PCB_REV_A
			//power on 2.8V
			ret = aat28xx_ldo_set_level(dev,PROXI_LDO_NO_VCC, 2800);
#elif  CONFIG_LGE_PCB_REV_B
			// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-02-22, [gelato] Rev B, Proximity Sensor change to apds9190 Voltage change 2.8V[START]
			//power on 2.8V
			ret = aat28xx_ldo_set_level(dev,PROXI_LDO_NO_VCC, 2800);
			// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-02-22, [gelato] Rev B, Proximity Sensor change to apds9190 Voltage change 2.8V[END]			
#else
			// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-05-02, [gelato] Rev C, 1.0, Proximity Sensor change to apds9190 Voltage change 3.0V[START]
			//power on 3.0V
			ret = aat28xx_ldo_set_level(dev,PROXI_LDO_NO_VCC, 3000);
#endif
#endif
			// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-05-02, [gelato] Rev C, 1.0, Proximity Sensor change to apds9190 Voltage change 3.0V[END]
			
			if(ret < 0){
				printk("[Proxi] %s() Power On %d ",__FUNCTION__,  PROXI_LDO_NO_VCC);
				return ret;
			}
			ret = aat28xx_ldo_enable(dev,PROXI_LDO_NO_VCC, 1);
			if(ret < 0){
				printk("[Proxi] %s() Power On Control Error %d ",__FUNCTION__,  PROXI_LDO_NO_VCC);
				return ret;
			}		
			prox_is_power_status = PROX_POWER_ON;		
			// LGE_CHANGE [jaekyung83.lee@lge.com] [GELATO] 2011-04-05, Charge Pump LCD_LED_EN Control, Noncontrol during call, control during sleep [START]
			lcd_led_en++;
			// LGE_CHANGE [jaekyung83.lee@lge.com] [GELATO] 2011-04-05, Charge Pump LCD_LED_EN Control, Noncontrol during call, control during sleep [END]
		}
	}else{
		if(PROX_POWER_ON == prox_is_power_status){
			//power off 
			ret = aat28xx_ldo_set_level(dev,PROXI_LDO_NO_VCC, 0);
			if(ret < 0){
				printk("[Proxi] %s() Power Off %d ",__FUNCTION__,  PROXI_LDO_NO_VCC);
				return ret;
			}
			ret = aat28xx_ldo_enable(dev,PROXI_LDO_NO_VCC, 0);
			if(ret < 0){
				printk("[Proxi] %s() Power Off Control Error %d ",__FUNCTION__,  PROXI_LDO_NO_VCC);
				return ret;
			}
			prox_is_power_status = PROX_POWER_OFF;							
			gpio_tlmm_config(GPIO_CFG(PROXI_GPIO_I2C_SCL, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
			gpio_tlmm_config(GPIO_CFG(PROXI_GPIO_I2C_SDA, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
			// LGE_CHANGE [jaekyung83.lee@lge.com] [GELATO]  2011-04-05, Charge Pump LCD_LED_EN Control, Noncontrol during call, control during sleep [START]
			lcd_led_en--;			
			// LGE_CHANGE [jaekyung83.lee@lge.com] [GELATO] 2011-04-05, Charge Pump LCD_LED_EN Control, Noncontrol during call, control during sleep [END]
		}
	}	
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-04, proxi power control [END]

	return ret;
}

//#if defined(CONFIG_LGE_PCB_REV_A)
#ifdef CONFIG_LGE_PCB_REV_A
static struct proximity_platform_data proxi_pdata = {
	.irq_num	= PROXI_GPIO_DOUT,
	.power		= prox_power_set,
	.methods		= 0,
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-04, proxi operation mode A [START]
	.operation_mode		= 0,
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-04, proxi operation mode A [END]
	.debounce	 = 0,
	.cycle = 2,
};
#else 
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-02-22, [gelato] Rev B, Proximity Sensor change to apds9190 [START]
static struct proximity_platform_data proxi_apds_data = {
	.irq_num	= PROXI_GPIO_DOUT,
	.power		= prox_power_set,
	.methods		= 1,
	.operation_mode		= 0,
};
#endif 
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-02-22, [gelato] Rev B, Proximity Sensor change to apds9190 [END]

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, ecom_i2c_bdinfo [START]
static struct i2c_board_info ecom_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("ami306_sensor", ECOM_I2C_ADDRESS),
		.type = "ami306_sensor",
		.platform_data = &ecom_pdata,
	},
};
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, ecom_i2c_bdinfo [END]

//#if defined(CONFIG_LGE_PCB_REV_A)
#ifdef CONFIG_LGE_PCB_REV_A
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, proxi_i2c_bdinfo [START]
static struct i2c_board_info proxi_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("proximity_gp2ap", PROXI_I2C_ADDRESS),
		.type = "proximity_gp2ap",
		.platform_data = &proxi_pdata,
	},
};
#else //Rev B
static struct i2c_board_info proxi_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("proximity_apds9190", PROXI_I2C_ADDRESS),
		.type = "proximity_apds9190",
		.platform_data = &proxi_apds_data,
	},
};
#endif
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, proxi_i2c_bdinfo [END]

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, ecom_i2c_pin [START]
static struct gpio_i2c_pin ecom_i2c_pin[] = {
	[0] = {
		.sda_pin	= ECOM_GPIO_I2C_SDA,
		.scl_pin	= ECOM_GPIO_I2C_SCL,
		.reset_pin	= ECOM_GPIO_RST,
		.irq_pin	= 0,
	},
};
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, ecom_i2c_pin [END]

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, proxi_i2c_pin [START]
static struct gpio_i2c_pin proxi_i2c_pin[] = {
	[0] = {
		.sda_pin	= PROXI_GPIO_I2C_SDA,
		.scl_pin	= PROXI_GPIO_I2C_SCL,
		.reset_pin	= 0,
		.irq_pin	= PROXI_GPIO_DOUT,
	},
};
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, proxi_i2c_pin [END]

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, proxi_i2c_pdata [START]
static struct i2c_gpio_platform_data proxi_i2c_pdata = {
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.udelay = 2,
};
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, proxi_i2c_pdata [END]

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, ecom_i2c_pdata [START]
static struct i2c_gpio_platform_data ecom_i2c_pdata = {
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.udelay = 2,
};
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, ecom_i2c_pdata [END]

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, proxi_i2c_device [START]
static struct platform_device proxi_i2c_device = {
        .name = "i2c-gpio",
        .dev.platform_data = &proxi_i2c_pdata,
};
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, proxi_i2c_device [END]

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, ecom_i2c_device [START]
static struct platform_device ecom_i2c_device = {
        .name = "i2c-gpio",
        .dev.platform_data = &ecom_i2c_pdata,
};
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, ecom_i2c_device [END]


// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, init_i2c_proxi [START]
static void  __init gelato_init_i2c_proxi(int bus_num)
{
	proxi_i2c_device.id = bus_num;
	init_gpio_i2c_pin(&proxi_i2c_pdata, proxi_i2c_pin[0],&proxi_i2c_bdinfo[0]);
	i2c_register_board_info(bus_num, &proxi_i2c_bdinfo[0],1);
	platform_device_register(&proxi_i2c_device);

	printk(KERN_INFO "init i2c proximity\n");
}
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, init_i2c_proxi [END]

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, init_i2c_ecom [START]
static void  __init gelato_init_i2c_ecom(int bus_num)
{
	ecom_i2c_device.id = bus_num;
	init_gpio_i2c_pin(&ecom_i2c_pdata, ecom_i2c_pin[0],&ecom_i2c_bdinfo[0]);
	i2c_register_board_info(bus_num, &ecom_i2c_bdinfo[0],1);
	platform_device_register(&ecom_i2c_device);

	printk(KERN_INFO "init i2c ecom\n");
}
/* common function */
void __init lge_add_input_devices(void)
{
	platform_add_devices(gelato_input_devices, ARRAY_SIZE(gelato_input_devices));
	lge_add_gpio_i2c_device(gelato_init_i2c_touch);
	lge_add_gpio_i2c_device(gelato_init_i2c_acceleration);
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, add device proxi, ecom [START]	
	lge_add_gpio_i2c_device(gelato_init_i2c_proxi);
	lge_add_gpio_i2c_device(gelato_init_i2c_ecom);
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-01-06, add device proxi, ecom [END]		
}
