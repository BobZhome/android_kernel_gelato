/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2010 LGE. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/bootmem.h>
#include <linux/power_supply.h>


#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/flash.h>
#include <asm/setup.h>
#ifdef CONFIG_CACHE_L2X0
#include <asm/hardware/cache-l2x0.h>
#endif

#include <asm/mach/mmc.h>
#include <mach/vreg.h>
#include <mach/mpp.h>
#include <mach/board.h>
#include <mach/pmic.h>
#include <mach/msm_iomap.h>
#include <mach/msm_rpcrouter.h>
#include <mach/msm_hsusb.h>
#include <mach/rpc_hsusb.h>
#ifdef CONFIG_USB_ANDROID
#include <linux/usb/android_composite.h>
#endif
#include <mach/rpc_pmapp.h>
#include <mach/msm_serial_hs.h>
#include <mach/memory.h>
#include <mach/msm_battery.h>
#include <mach/rpc_server_handset.h>
#include <mach/msm_tsif.h>

#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/i2c.h>
#include <linux/android_pmem.h>
#include <mach/camera.h>

#include "devices.h"
#include "socinfo.h"
#include "clock.h"
#include "msm-keypad-devices.h"
#include "pm.h"
#ifdef CONFIG_ARCH_MSM7X27
#include <linux/msm_kgsl.h>
#endif
#ifdef CONFIG_USB_ANDROID
#include <linux/usb/android_composite.h>
#endif
#include <mach/board_lge.h>
#include "board-gelato.h"
#include "lge_diag_communication.h"

#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
#define LG_UNKNOWN_CABLE			0
#define LG_WALL_CHARGER_CABLE		1
#define LG_NORMAL_USB_CABLE			2
#define LG_FACTORY_CABLE_56K_TYPE	3
#define LG_FACTORY_CABLE_130K_TYPE	4
#define LG_FACTORY_CABLE_910K_TYPE	5
#define LG_RESERVED1_CABLE			6
#define LG_RESERVED2_CABLE			7
#define LG_NONE_CABLE				8
#endif

/* board-specific pm tuning data definitions */

/* currently, below declaration code is blocked.
 * if power management tuning is required in any board,
 * below "msm7x27_pm_data" array can be redefined and can be unblocked.
 * qualocomm's default setting value is configured in devices_lge.c
 * but that variable is declared in weak attribute
 * so board specific configuration can be redefined like "over riding" in OOP
 */
extern struct msm_pm_platform_data msm7x25_pm_data[MSM_PM_SLEEP_MODE_NR];
extern struct msm_pm_platform_data msm7x27_pm_data[MSM_PM_SLEEP_MODE_NR];

/* board-specific usb data definitions */

/* For supporting LG Android gadget framework, move android gadget platform
 * datas to specific board file
 * [younsuk.song@lge.com] 2010-07-11
 */
/* BEGIN:0011986 [yk.kim@lge.com] 2010-12-07 */
/* ADD:0011986 Bryce USB composition redefine */
#ifdef CONFIG_USB_ANDROID
#ifdef CONFIG_LGE_USB_GADGET_DRIVER
#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
#ifdef CONFIG_LGE_USB_GADGET_NDIS_VZW_DRIVER
static char *usb_functions_all[] = {
	"acm",
	"diag",
	"cdc_ethernet",
	"usb_mass_storage",
	"adb",
#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
	"usb_autorun",
#endif
#ifdef CONFIG_LGE_USB_GADGET_MTP_DRIVER
	"mtp",
#endif
};
#elif defined(CONFIG_LGE_USB_GADGET_NDIS_UNITED_DRIVER)
static char *usb_functions_all[] = {
#if 0	// moses.son@lge.com PID change
	"diag",
	"cdc_ethernet",
	"acm",
	"nmea",
	"usb_mass_storage",
	"adb",
#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
	"usb_autorun",
#endif
#ifdef CONFIG_LGE_USB_GADGET_MTP_DRIVER
	"mtp",
#endif
#else
	"acm",
	"diag",
	"nmea",				// activate when PID 0x6310 USB driver released. moses.son@lge.com
	"cdc_ethernet",
	"usb_mass_storage",
	"adb"
#endif
};
#endif
#else
#ifdef CONFIG_LGE_USB_GADGET_PLATFORM_DRIVER
static char *usb_functions_all[] = {
	"acm",
	"diag",
	"nmea",
	"usb_mass_storage",
	"adb",
#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
	"usb_autorun",
#endif
#ifdef CONFIG_LGE_USB_GADGET_MTP_DRIVER
	"mtp",
#endif
};
#else
static char *usb_functions_all[] = {
	"acm",
	"diag",
	"nmea",
	"rmnet",
	"usb_mass_storage",
	"adb",
#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
	"usb_autorun",
#endif
#ifdef CONFIG_LGE_USB_GADGET_MTP_DRIVER
	"mtp",
#endif
};
#endif
#endif

static char *usb_functions_acm_modem[] = {
	"acm",
	"diag",
	"nmea",
	"usb_mass_storage",
	"adb",
};

#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
#ifdef CONFIG_LGE_USB_GADGET_NDIS_VZW_DRIVER
static char *usb_functions_ums[] = {
	"acm",
	"diag",
	"cdc_ethernet",
	"usb_mass_storage",
	"adb",
};
#elif defined(CONFIG_LGE_USB_GADGET_NDIS_UNITED_DRIVER)
static char *usb_functions_ums[] = {
	"usb_mass_storage",
};
#endif
#else
static char *usb_functions_ums[] = {
	"usb_mass_storage",
};
#endif
#endif

#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
static char *usb_functions_factory[] = {
	"acm",
	"diag",
};
#endif

#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN 
static char *usb_functions_autorun[] = {
	"usb_autorun",
};
#endif

#ifdef CONFIG_LGE_USB_GADGET_MTP_DRIVER
static char *usb_functions_mtp[] = {
	"mtp",
};
#endif
#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
#ifdef CONFIG_LGE_USB_GADGET_NDIS_VZW_DRIVER
static char *usb_functions_ndis[] = {
	"acm",
	"diag",
	"cdc_ethernet",
	"adb",
};
#elif defined(CONFIG_LGE_USB_GADGET_NDIS_UNITED_DRIVER)
static char *usb_functions_ndis[] = {
#if 0	// moses.son@lge.com pid change
	"diag",
	"cdc_ethernet",
	"acm",
	"nmea",
	"usb_mass_storage",
	"adb",
#else
	"acm",
	"diag",
	"nmea",				// activate when PID 0x6310 USB driver released. moses.son@lge.com
	"cdc_ethernet",
	"usb_mass_storage",
	"adb"
#endif
};
#endif
#else
static char *usb_functions_rmnet[] = {
	"acm",
	"diag",
	"nmea",
	"rmnet",
	"usb_mass_storage",
	"adb",
};
#endif

/* NRB_CHANGES_S [myoungkim@nuribom.com] 2011-05-29 - ##port#*/
static char * usb_functions_portlock[] = {
	"usb_mass_storage",
	"adb",
};
/* NRB_CHANGES_E [myoungkim@nuribom.com] 2011-05-29 */

static struct android_usb_product usb_products[] = {
#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
	{
		.product_id = 0x6000,
		.num_functions	= ARRAY_SIZE(usb_functions_factory),
		.functions	= usb_functions_factory,
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
		.unique_function = FACTORY,
#endif
	},
#endif
#ifdef CONFIG_LGE_USB_GADGET_PLATFORM_DRIVER
	{
		.product_id = 0x618E,
		.num_functions	= ARRAY_SIZE(usb_functions_acm_modem),
		.functions	= usb_functions_acm_modem,
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
		.unique_function = ACM_MODEM,
#endif
	},
#endif
#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
#ifdef CONFIG_LGE_USB_GADGET_NDIS_VZW_DRIVER
	{
		.product_id = 0x6200,
		.num_functions	= ARRAY_SIZE(usb_functions_ndis),
		.functions	= usb_functions_ndis,
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
		.unique_function = NDIS,
#endif	
	},
	{
		.product_id = 0x6201,
		.num_functions	= ARRAY_SIZE(usb_functions_ums),
		.functions	= usb_functions_ums,
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
		.unique_function = UMS,
#endif	
	},
#elif defined(CONFIG_LGE_USB_GADGET_NDIS_UNITED_DRIVER)
	{
#if 0	// moses.son@lge.com pid change
		.product_id = 0x61A1,
#else
//		.product_id = 0x61FC,
		.product_id = 0x6310,			// activate when PID 0x6310 USB driver released.
#endif
		.num_functions	= ARRAY_SIZE(usb_functions_ndis),
		.functions	= usb_functions_ndis,
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
		.unique_function = NDIS,
#endif	
	},
#endif
#else
	{
		.product_id = 0x61CF,
		.num_functions	= ARRAY_SIZE(usb_functions_rmnet),
		.functions	= usb_functions_rmnet,
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
		.unique_function = RMNET,
#endif
	},
#endif
#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
	{
		.product_id = 0x6203,
		.num_functions	= ARRAY_SIZE(usb_functions_autorun),
		.functions	= usb_functions_autorun,
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
		.unique_function = CD_ROM,
#endif	
	},
#endif
#ifdef CONFIG_LGE_USB_GADGET_MTP_DRIVER
	{
		.product_id = 0x6202,
		.num_functions	= ARRAY_SIZE(usb_functions_mtp),
		.functions	= usb_functions_mtp,
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
		.unique_function = MTP,
#endif	
	},
#endif
/* NRB_CHANGES_S [myoungkim@nuribom.com] 2011-05-29 */
	{
		.product_id = 0x61A6,
		.num_functions = ARRAY_SIZE(usb_functions_portlock),
		.functions = usb_functions_portlock,
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
		.unique_function = UMS,
#endif			
	},
/* NRB_CHANGES_E [myoungkim@nuribom.com] 2011-05-29 */	
};

/* [yk.kim@lge.com] 2010-12-28, ums sysfs enable */
#if 1
struct usb_mass_storage_platform_data mass_storage_pdata = {
	.nluns		= 1,
#if 0		
	.vendor 	= "LG Electronics Inc.",
	.product		= "Mass storage",
#else	
	.vendor 	= "LGE",
	.product	= "Android",
#endif
	.release	= 0x0100,
	.can_stall	= 1,
};

struct platform_device usb_mass_storage_device = {
	.name	= "usb_mass_storage",
	.id = -1,
	.dev	= {
		.platform_data = &mass_storage_pdata,
	},
};
#endif

#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
struct android_usb_platform_data android_usb_pdata_factory = {
	.vendor_id	= 0x1004,
	.product_id = 0x6000,
	.version	= 0x0100,
	.product_name		= "LGE CDMA Composite USB Device",
	.manufacturer_name	= "LG Electronics Inc.",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_factory),
	.functions = usb_functions_factory,
	.serial_number = "\0",
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
	.unique_function = FACTORY,
#endif	
};
#endif
#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
#ifdef CONFIG_LGE_USB_GADGET_NDIS_VZW_DRIVER
struct android_usb_platform_data android_usb_pdata_ndis = {
	.vendor_id	= 0x1004,
	.product_id = 0x6200,
	.version	= 0x0100,
	.product_name		= "LG Android USB Device",
	.manufacturer_name	= "LG Electronics Inc.",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_all),
	.functions = usb_functions_all,
	.serial_number = "LGANDROIDVS910",
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
	.unique_function = NDIS,
#endif	
};
#elif defined(CONFIG_LGE_USB_GADGET_NDIS_UNITED_DRIVER)
struct android_usb_platform_data android_usb_pdata_ndis = {
	.vendor_id	= 0x1004,
#if 0	// moses.son@lge.com		
	.product_id = 0x61A1,
#else
//	.product_id = 0x61FC,
	.product_id = 0x6310,			// activate when PID 0x6310 USB driver released.
#endif
	.version	= 0x0100,
	.product_name		= "LG Android USB Device",
	.manufacturer_name	= "LG Electronics Inc.",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_all),
	.functions = usb_functions_all,
	.serial_number = "LGANDROIDLS685",
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
	.unique_function = NDIS,
#endif	
};
#endif
#else
#ifdef CONFIG_LGE_USB_GADGET_PLATFORM_DRIVER
struct android_usb_platform_data android_usb_pdata_platform = {
	.vendor_id	= 0x1004,
	.product_id = 0x618E,
	.version	= 0x0100,
	.product_name		= "LG Android USB Device",
	.manufacturer_name	= "LG Electronics Inc.",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_all),
	.functions = usb_functions_all,
	.serial_number = "LGANDROIDVS910",
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
	.unique_function = ACM_MODEM,
#endif	
};
#else
struct android_usb_platform_data android_usb_pdata_rmnet = {
	.vendor_id	= 0x1004,
	.product_id = 0x61CF,
	.version	= 0x0100,
	.product_name		= "LG Android USB Device",
	.manufacturer_name	= "LG Electronics Inc.",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_all),
	.functions = usb_functions_all,
	.serial_number = "LGANDROIDVS910",
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
	.unique_function = RMNET,
#endif	
};
#endif
#endif
static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id 	= -1,
	.dev		= {
#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
	.platform_data = &android_usb_pdata_ndis,
#else
#ifdef CONFIG_LGE_USB_GADGET_PLATFORM_DRIVER
	.platform_data = &android_usb_pdata_platform,
#else
	.platform_data = &android_usb_pdata_rmnet,
#endif
#endif
	},
};

#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
static struct usb_ether_platform_data ecm_pdata = {
	/* ethaddr is filled by board_serialno_setup */
	.vendorID	= 0x1004,
	.vendorDescr	= "LG Electronics Inc.",
};

static struct platform_device ecm_device = {
	.name	= "cdc_ethernet",
	.id = -1,
	.dev	= {
		.platform_data = &ecm_pdata,
	},
};
#endif

#endif /* CONFIG_USB_ANDROID */
	/* END:0011986 [yk.kim@lge.com] 2010-12-07 */

static struct diagcmd_platform_data lg_fw_diagcmd_pdata = {
	.name = "lg_fw_diagcmd",
};

static struct platform_device lg_fw_diagcmd_device = {
	.name = "lg_fw_diagcmd",
	.id = -1,
	.dev    = {
		.platform_data = &lg_fw_diagcmd_pdata
	},
};

static struct platform_device lg_diag_cmd_device = {
	.name = "lg_diag_cmd",
	.id = -1,
	.dev    = {
		.platform_data = 0, //&lg_diag_cmd_pdata
	},
};

static struct platform_device *devices[] __initdata = {
	&msm_device_smd,
	&msm_device_dmov,
	&msm_device_nand,
	&msm_device_i2c,
	&msm_device_uart_dm1,
	&msm_device_snd,
	&msm_device_adspdec,
	&lg_fw_diagcmd_device,
	&lg_diag_cmd_device,
	&android_usb_device,
#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
	&ecm_device,
#endif
	
};

extern struct sys_timer msm_timer;

/* LGE_CHANGES_S [moses.son@lge.com] 2011-02-09, need to check LT cable type */
int get_msm_cable_type(void)
{ 
	unsigned int cable_type;
#if 0
	int fn_type = CUSTOMER_CMD1_GET_CABLE_TYPE;

	msm_proc_comm(PCOM_CUSTOMER_CMD1,  &cable_type,&fn_type);
	printk("[LGE_PWR] cable type detection from muic at modem side Cable=%d \n",cable_type);
#else
	unsigned int modem_cable_type;

	extern int msm_chg_LG_cable_type(void);

	modem_cable_type = msm_chg_LG_cable_type();

	switch(modem_cable_type)
	{
		case 0: //NOINIT_CABLE
		case 9: //NO_CABLE
			cable_type = LG_NONE_CABLE;
			break;
		case 1: //UNKNOWN_CABLE
			cable_type = LG_UNKNOWN_CABLE;
			break;
		case 2: //TA_CABLE
		case 5: // FORGED_TA_CABLE
		case 8: // C1A_TA_CABLE
			cable_type = LG_WALL_CHARGER_CABLE;
			break;
		case 3: // LT_CABLE
			cable_type = LG_FACTORY_CABLE_56K_TYPE;
			break;
		case 4: // USB_CABLE
		case 6: //ABNORMAL_USB_CABLE
		case 7 : //ABNORMAL_USB_400c_CABLE
			cable_type = LG_NORMAL_USB_CABLE;
			break;
		case 10: // LT_CABLE_130K
			cable_type = LG_FACTORY_CABLE_130K_TYPE;
			break;
		case 11: // LT_CABLE_910K
			cable_type = LG_FACTORY_CABLE_910K_TYPE;
			break;
		default:
			cable_type = LG_NONE_CABLE;
	}
#endif

	return cable_type;
}
/* LGE_CHANGES_E [moses.son@lge.com] 2011-02-09 */

static void __init msm7x2x_init_irq(void)
{
	msm_init_irq();
}

static struct msm_acpu_clock_platform_data msm7x2x_clock_data = {
	.acpu_switch_time_us = 50,
	.max_speed_delta_khz = 400000,
	.vdd_switch_time_us = 62,
	.max_axi_khz = 160000,
};


void msm_serial_debug_init(unsigned int base, int irq,
			   struct device *clk_device, int signal_irq);

static void msm7x27_wlan_init(void)
{
	int rc = 0;
	/* TBD: if (machine_is_msm7x27_ffa_with_wcn1312()) */
	if (machine_is_msm7x27_ffa()) {
		rc = mpp_config_digital_out(3, MPP_CFG(MPP_DLOGIC_LVL_MSMP,
				MPP_DLOGIC_OUT_CTRL_LOW));
		if (rc)
			printk(KERN_ERR "%s: return val: %d \n",
				__func__, rc);
	}
}

unsigned pmem_fb_size = 	0x96000;
unsigned pmem_adsp_size = 	0xAE4000;

static void __init msm7x2x_init(void)
{
	if (socinfo_init() < 0)
		BUG();

	msm_clock_init(msm_clocks_7x27, msm_num_clocks_7x27);

#if defined(CONFIG_MSM_SERIAL_DEBUGGER)
	msm_serial_debug_init(MSM_UART1_PHYS, INT_UART1,
			&msm_device_uart1.dev, 1);
#endif

	if (cpu_is_msm7x27())
		msm7x2x_clock_data.max_axi_khz = 200000;

	msm_acpu_clock_init(&msm7x2x_clock_data);

	msm_add_pmem_devices();
	msm_add_fb_device();

	platform_add_devices(devices, ARRAY_SIZE(devices));
#ifdef CONFIG_ARCH_MSM7X27
	msm_add_kgsl_device();
#endif
	msm_add_usb_devices();

#ifdef CONFIG_MSM_CAMERA
	config_camera_off_gpios(); /* might not be necessary */
#endif
	msm_device_i2c_init();
	i2c_register_board_info(0, i2c_devices, ARRAY_SIZE(i2c_devices));

	if (cpu_is_msm7x27())
		msm_pm_set_platform_data(msm7x27_pm_data,
					ARRAY_SIZE(msm7x27_pm_data));
	else
		msm_pm_set_platform_data(msm7x25_pm_data,
					ARRAY_SIZE(msm7x25_pm_data));
	msm7x27_wlan_init();

#ifdef CONFIG_ANDROID_RAM_CONSOLE
	lge_add_ramconsole_devices();
	lge_add_ers_devices();
	lge_add_panic_handler_devices();
#endif
	lge_add_camera_devices();
	lge_add_lcd_devices();
	lge_add_btpower_devices();
	lge_add_mmc_devices();
	lge_add_input_devices();
	lge_add_misc_devices();
	lge_add_pm_devices();
	
	/* gpio i2c devices should be registered at latest point */
	lge_add_gpio_i2c_devices();
}

static void __init msm7x2x_map_io(void)
{
	msm_map_common_io();

	msm_msm7x2x_allocate_memory_regions();

#ifdef CONFIG_CACHE_L2X0
	/* 7x27 has 256KB L2 cache:
		64Kb/Way and 4-Way Associativity;
		R/W latency: 3 cycles;
		evmon/parity/share disabled. */
	l2x0_init(MSM_L2CC_BASE, 0x00068012, 0xfe000000);
#endif
}

MACHINE_START(MSM7X27_GELATO, "GELATO board")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= PHYS_OFFSET + 0x100,
	.map_io			= msm7x2x_map_io,
	.init_irq		= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer			= &msm_timer,
MACHINE_END
