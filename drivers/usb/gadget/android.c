/*
 * Gadget Driver for Android
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 * Author: Mike Lockwood <lockwood@android.com>
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

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>

#include <linux/usb/android_composite.h>
#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>
/* BEGIN:0009986 [yk.kim@lge.com] 2010-10-18 */
/* ADD:0009986 USB in full-speed mode configure VDDA33 voltage to 3.4 when cable is connected */
#include <mach/vreg.h>
/* END:0009986 [yk.kim@lge.com] 2010-10-18 */
#include "gadget_chips.h"
/* BEGIN: yk.kim@lge.com 2010-10-11 */
/* MOD: Set Product ID */
#include "u_lgeusb.h"
/* END: yk.kim@lge.com 2010-10-11 */

#ifdef CONFIG_LGE_USB_GADGET_MTP_DRIVER
#include <linux/miscdevice.h>
#include "f_mtp.h"
#endif
#ifdef CONFIG_LGE_USB_GADGET_DRIVER
#include <mach/rpc_hsusb.h>
#endif

#ifdef CONFIG_LGE_USB_SUPPORT_ANDROID_AUTORUN
#include <linux/time.h>
#endif

/*
 * Kbuild is not very cooperative with respect to linking separately
 * compiled library objects into one module.  So for now we won't use
 * separate compilation ... ensuring init/exit sections work to shrink
 * the runtime footprint, and giving us at least some parts of what
 * a "gcc --combine ... part1.c part2.c part3.c ... " build would.
 */
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"
#include "composite.c"

MODULE_AUTHOR("Mike Lockwood");
MODULE_DESCRIPTION("Android Composite USB Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

/* BEGIN:0011202 [yk.kim@lge.com] 2010-11-21 */
/* ADD:0011202 support autorun */
#ifdef CONFIG_LGE_USB_GADGET_DRIVER
/* product id */
u16 product_id;
int android_set_pid(const char *val, struct kernel_param *kp);
static int android_get_pid(char *buffer, struct kernel_param *kp);
module_param_call(product_id, android_set_pid, android_get_pid,
					&product_id, 0664);
MODULE_PARM_DESC(product_id, "USB device product id");
#endif

/* serial number */
#define MAX_SERIAL_LEN 256

#ifdef CONFIG_LGE_USB_GADGET_DRIVER
#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
#ifdef CONFIG_LGE_USB_GADGET_NDIS_VZW_DRIVER
const u16 lg_default_pid	= 0x6200;
#elif defined(CONFIG_LGE_USB_GADGET_NDIS_UNITED_DRIVER)
#if 0	// moses.son@lge.com	pid change
const u16 lg_default_pid	= 0x61A1;
#else
//const u16 lg_default_pid	= 0x61FC;
const u16 lg_default_pid	= 0x6310;		// activate when PID 0x6310 USB driver released.
#endif
#endif
#else
#ifdef CONFIG_LGE_USB_GADGET_PLATFORM_DRIVER
const u16 lg_default_pid	= 0x618E;
#else
const u16 lg_default_pid	= 0x61CF;
#endif
#endif
const u16 lg_ums_pid		= 0x6201;

/* LGE_CHANGES_S [moses.son@lge.com] 2011-04-17, charge only mode */
const u16 lg_charge_only_pid_sprint = 0xFFFF;
/* LGE_CHANGES_E [moses.son@lge.com] 2011-04-17 */


static int cable;
int switch_cable;
extern int get_msm_cable_type(void);
/* [yk.kim@lge.com] 2011-01-05, workaround flag, FIX ME */
static int switch_flag = 0;
int set_pid_flag = 0;
#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
#else
int hidden_pid = 0;	/* switching UMS pid */
#endif
#endif

#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN_CGO
const u16 lg_charge_only_pid = 0xFFFF;
#endif
static char autorun_serial_number[MAX_SERIAL_LEN] = "LGANDROIDVS910";
static u16 autorun_user_mode;
static int android_set_usermode(const char *val, struct kernel_param *kp);
module_param_call(user_mode, android_set_usermode, param_get_string,
					&autorun_user_mode, 0664);
MODULE_PARM_DESC(user_mode, "USB Autorun user mode");
#endif
/* END:0011202 [yk.kim@lge.com] 2010-11-21 */

static const char longname[] = "Gadget Android";

/* Default vendor and product IDs, overridden by platform data */
#define VENDOR_ID		0x18D1
#define PRODUCT_ID		0x0001

struct android_dev {
	struct usb_composite_dev *cdev;
	struct usb_configuration *config;
	int num_products;
	struct android_usb_product *products;
	int num_functions;
	char **functions;

	int product_id;
	int version;
#ifdef CONFIG_LGE_USB_GADGET_DRIVER
	struct mutex lock;
#endif
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
    unique_usb_function unique_function;
#endif
};

static struct android_dev *_android_dev;

/* BEGIN:0010739 [yk.kim@lge.com] 2010-11-11 */
/* ADD:0010739 Bryce USB switch composition */
#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
const u16 lg_autorun_pid	= 0x6203;
#endif
/* END:0010739 [yk.kim@lge.com] 2010-11-11 */

#define MAX_STR_LEN		16
/* string IDs are assigned dynamically */

#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1
#define STRING_SERIAL_IDX		2
#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
const u16 lg_factory_pid = 0x6000;
extern struct android_usb_platform_data android_usb_pdata_factory;
#endif

#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
char serial_number[MAX_SERIAL_LEN] = "\0";
char user_serial_number[MAX_SERIAL_LEN];
#else
char serial_number[MAX_STR_LEN];
#endif
#ifdef CONFIG_LGE_USB_GADGET_MTP_DRIVER
int mtp_enable_flag = 0;
const u16 lg_mtp_pid		= 0x6202;
#endif
#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
#ifdef CONFIG_LGE_USB_GADGET_NDIS_VZW_DRIVER
const u16 lg_ndis_pid = 0x6200;
#elif defined(CONFIG_LGE_USB_GADGET_NDIS_UNITED_DRIVER)
#if 0	// moses.son@lge.com pid change
const u16 lg_ndis_pid = 0x61A1;
#else
//const u16 lg_ndis_pid = 0x61FC;
const u16 lg_ndis_pid = 0x6310;		// activate when PID 0x6310 USB driver released.
#endif
#endif
#elif defined(CONFIG_LGE_USB_GADGET_ECM_DRIVER)
const u16 lg_ecm_pid = 0x6200;
#elif defined(CONFIG_LGE_USB_GADGET_RNDIS_DRIVER)
const u16 lg_rndis_pid = 0x6200;
#else
#ifdef CONFIG_LGE_USB_GADGET_PLATFORM_DRIVER
const u16 lg_android_pid = 0x618E;
#else
const u16 lg_rmnet_pid = 0x61CF;
#endif
#endif

/* String Table */
static struct usb_string strings_dev[] = {
	/* These dummy values should be overridden by platform data */
	[STRING_MANUFACTURER_IDX].s = "Android",
	[STRING_PRODUCT_IDX].s = "Android",
#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
	[STRING_SERIAL_IDX].s = "\0",
#else
	[STRING_SERIAL_IDX].s = "0123456789ABCDEF",
#endif
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};

static struct usb_device_descriptor device_desc = {
	.bLength              = sizeof(device_desc),
	.bDescriptorType      = USB_DT_DEVICE,
	.bcdUSB               = __constant_cpu_to_le16(0x0200),
	.bDeviceClass         = USB_CLASS_PER_INTERFACE,
	.idVendor             = __constant_cpu_to_le16(VENDOR_ID),
	.idProduct            = __constant_cpu_to_le16(PRODUCT_ID),
	.bcdDevice            = __constant_cpu_to_le16(0xffff),
	.bNumConfigurations   = 1,
};

static struct usb_otg_descriptor otg_descriptor = {
	.bLength =		sizeof otg_descriptor,
	.bDescriptorType =	USB_DT_OTG,
	.bmAttributes =		USB_OTG_SRP | USB_OTG_HNP,
	.bcdOTG               = __constant_cpu_to_le16(0x0200),
};

static const struct usb_descriptor_header *otg_desc[] = {
	(struct usb_descriptor_header *) &otg_descriptor,
	NULL,
};

static struct list_head _functions = LIST_HEAD_INIT(_functions);
static int _registered_function_count = 0;

#ifdef CONFIG_LGE_USB_GADGET_DRIVER
extern void diag_set_serial_number(char *serial_number);
#endif
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
void android_set_device_class(u16 pid);
#endif

static void android_set_default_product(int product_id);
/*NRB_CHANGES_S [myoungkim@nuribom.com] 2011-07-15 HiddenMenu : add PORT */
//adb enable after boot complete.
extern int get_adb_demon_started(void);

int get_pid_switch_flag(void)
{
	return switch_flag;
}
/*NRB_CHANGES_E [myoungkim@nuribom.com] 2011-07-15 HiddenMenu : add PORT */	
void android_usb_set_connected(int connected)
{
	if (_android_dev && _android_dev->cdev && _android_dev->cdev->gadget) {
		if (connected)
			usb_gadget_connect(_android_dev->cdev->gadget);
		else
			usb_gadget_disconnect(_android_dev->cdev->gadget);
	}
}

static struct android_usb_function *get_function(const char *name)
{
	struct android_usb_function	*f;
	list_for_each_entry(f, &_functions, list) {
		if (!strcmp(name, f->name))
			return f;
	}
	return 0;
}

static void bind_functions(struct android_dev *dev)
{
	struct android_usb_function	*f;
	char **functions = dev->functions;
	int i;

	for (i = 0; i < dev->num_functions; i++) {
		char *name = *functions++;
		f = get_function(name);
		if (f)
			f->bind_config(dev->config);
		else
			pr_err("%s: function %s not found\n", __func__, name);
	}

	/*
	 * set_alt(), or next config->bind(), sets up
	 * ep->driver_data as needed.
	 */
	usb_ep_autoconfig_reset(dev->cdev->gadget);
}

static int __ref android_bind_config(struct usb_configuration *c)
{
	struct android_dev *dev = _android_dev;

	pr_debug("android_bind_config\n");
	dev->config = c;

	/* bind our functions if they have all registered */
	if (_registered_function_count == dev->num_functions)
		bind_functions(dev);

	return 0;
}

static int android_setup_config(struct usb_configuration *c,
		const struct usb_ctrlrequest *ctrl);

static struct usb_configuration android_config_driver = {
	.label		= "android",
	.bind		= android_bind_config,
	.setup		= android_setup_config,
	.bConfigurationValue = 1,
	.bMaxPower	= 0xFA, /* 500ma */
};

static int android_setup_config(struct usb_configuration *c,
		const struct usb_ctrlrequest *ctrl)
{
	int i;
	int ret = -EOPNOTSUPP;

	for (i = 0; i < android_config_driver.next_interface_id; i++) {
		if (android_config_driver.interface[i]->setup) {
			ret = android_config_driver.interface[i]->setup(
				android_config_driver.interface[i], ctrl);
			if (ret >= 0)
				return ret;
		}
	}
	return ret;
}

static int product_has_function(struct android_usb_product *p,
		struct usb_function *f)
{
	char **functions = p->functions;
	int count = p->num_functions;
	const char *name = f->name;
	int i;

	for (i = 0; i < count; i++) {
		if (!strcmp(name, *functions++))
			return 1;
	}
	return 0;
}

static int product_matches_functions(struct android_usb_product *p)
{
	struct usb_function		*f;
	list_for_each_entry(f, &android_config_driver.functions, list) {
		if (product_has_function(p, f) == !!f->disabled)
			return 0;
	}
	return 1;
}

#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
static int product_matches_unique_functions(struct android_dev *dev, struct android_usb_product *p)
{
    if(p->unique_function == dev->unique_function)
		return 1;
	else
		return 0;
}

static int get_init_product_id(struct android_dev *dev)
{
	struct android_usb_product *p = dev->products;
	int count = dev->num_products;
	int i;

    if (p) {
		for (i = 0; i < count; i++, p++) {  
			if (product_matches_unique_functions(dev,p))
			{
				USB_DBG("dev->product_id 0x%x\n", p->product_id);
				return p->product_id;
			}
		}
    }

	USB_DBG("dev->product_id 0x%x\n", dev->product_id);
	/* use default product ID */
	return dev->product_id;
}

#endif
static int get_product_id(struct android_dev *dev)
{
	struct android_usb_product *p = dev->products;
	int count = dev->num_products;
	int i;

#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
    if(product_id == lg_factory_pid){
        return product_id;
    	}
/*NRB_CHANGES_S [myoungkim@nuribom.com] 2011-05-29 HiddenMenu : add PORT */	
	else if (product_id ==  0x61A6)
    	{
    		return product_id;
	}
/*NRB_CHANGES_E [myoungkim@nuribom.com] */	
	else if(p) {
#else
	if (p) {
#endif
		for (i = 0; i < count; i++, p++) {
			if (product_matches_functions(p))
			{
				USB_DBG("dev->product_id 0x%x\n", p->product_id);
				return p->product_id;
			}
		}
	}

	USB_DBG("dev->product_id 0x%x\n", dev->product_id);
	/* use default product ID */
	return dev->product_id;
}
	
#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
static int lge_bind_config(u16 pid)
{
	int ret = 0;

	if (pid == lg_factory_pid) 
	{
		serial_number[0] = '\0';
		msm_hsusb_is_serial_num_null(1); 
		device_desc.iSerialNumber = 0; 
	}
	else
	{
	    sprintf(serial_number, "%s", user_serial_number);
		ret = lge_get_usb_serial_number(serial_number);

		/* Send Serial number to A9 for software download */
		if (serial_number[0] != '\0') {
			msm_hsusb_is_serial_num_null(0);
			msm_hsusb_send_serial_number(serial_number);
		} else {
			/* If error to get serial number, we check the
			 * pdata's serial number. If pdata's serial number
			 * (e.g. default serial number) is set, we use the 
			 * serial number.
			 */
			/* TEMP : [yk.kim@lge.com] 2010-12-27, for user cable multi-download */
			//if (user_serial_number == NULL) {
				serial_number[0] = '\0';
				msm_hsusb_is_serial_num_null(1); 
				device_desc.iSerialNumber = 0; 
			//} else {
			//	msm_hsusb_is_serial_num_null(0);
			//	msm_hsusb_send_serial_number(serial_number);
			//}
		}
	}

	if (pid)
		msm_hsusb_send_productID(pid);

	android_config_driver.bmAttributes |= USB_CONFIG_ATT_SELFPOWER;
	android_config_driver.bMaxPower = 0xFA; /* 500 mA */

    return ret;
	
}
#endif

#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
static char random_serial_number[14];
static void __init set_random_serial_number()
{
	struct timeval tv;
	do_gettimeofday(&tv);
	sprintf(random_serial_number,"123456789%5d", tv.tv_usec % 100000);
	pr_info("random_serial_number[%s]\n", random_serial_number);
	return;
}
#endif

static int __devinit android_bind(struct usb_composite_dev *cdev)
{
	struct android_dev *dev = _android_dev;
	struct usb_gadget	*gadget = cdev->gadget;
	int			gcnum, id, product_id, ret;

	pr_debug("android_bind\n");

	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */
	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_MANUFACTURER_IDX].id = id;
	device_desc.iManufacturer = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_PRODUCT_IDX].id = id;
	device_desc.iProduct = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_SERIAL_IDX].id = id;
	device_desc.iSerialNumber = id;

	if (gadget_is_otg(cdev->gadget))
		android_config_driver.descriptors = otg_desc;

	if (!usb_gadget_set_selfpowered(gadget))
		android_config_driver.bmAttributes |= USB_CONFIG_ATT_SELFPOWER;

	if (gadget->ops->wakeup)
		android_config_driver.bmAttributes |= USB_CONFIG_ATT_WAKEUP;

	/* register our configuration */
	ret = usb_add_config(cdev, &android_config_driver);
	if (ret) {
		pr_err("%s: usb_add_config failed\n", __func__);
		return ret;
	}

	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0)
		device_desc.bcdDevice = cpu_to_le16(0x0200 + gcnum);
	else {
		/* gadget zero is so simple (for now, no altsettings) that
		 * it SHOULD NOT have problems with bulk-capable hardware.
		 * so just warn about unrcognized controllers -- don't panic.
		 *
		 * things like configuration and altsetting numbering
		 * can need hardware-specific attention though.
		 */
		pr_warning("%s: controller '%s' not recognized\n",
			longname, gadget->name);
		device_desc.bcdDevice = __constant_cpu_to_le16(0x9999);
	}

	usb_gadget_set_selfpowered(gadget);
	dev->cdev = cdev;
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
	product_id = get_init_product_id(dev);
#else
	product_id = get_product_id(dev);
#endif
#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
    ret = lge_bind_config(product_id);
#endif
	device_desc.idProduct = __constant_cpu_to_le16(product_id);
	cdev->desc.idProduct = device_desc.idProduct;

/* LGE_CHANGES_S [jaeho.cho@lge.com] 2010-08-16, Autorun Serial Number */
#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
    if(product_id == lg_autorun_pid)
    {      
	  strings_dev[STRING_SERIAL_IDX].s = autorun_serial_number;
    }
	else
    {
	  strings_dev[STRING_SERIAL_IDX].s = serial_number;
	}
#endif

#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
	if(product_id == lg_ndis_pid)
	{
		device_desc.bDeviceClass		 = USB_CLASS_MISC;
		device_desc.bDeviceSubClass 	 = 0x02;
		device_desc.bDeviceProtocol 	 = 0x01;
	}
	else
	{
		device_desc.bDeviceClass		 = USB_CLASS_COMM;
		device_desc.bDeviceSubClass 	 = 0x00;
		device_desc.bDeviceProtocol 	 = 0x00;
	}
#else
	/* BEGIN:0010739 [yk.kim@lge.com] 2010-11-11 */
	/* ADD:0010739 set device desc. */
	device_desc.bDeviceClass		 = USB_CLASS_COMM;
	device_desc.bDeviceSubClass 	 = 0x00;
	device_desc.bDeviceProtocol 	 = 0x00;
	/* END:0010739 [yk.kim@lge.com] 2010-11-11 */
#endif

#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
#else
	/* workaround pid flag */
	hidden_pid = product_id;
#endif

#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
	if (serial_number[0] == '\0')
		set_random_serial_number();
#endif

	return 0;
}

static struct usb_composite_driver android_usb_driver = {
	.name		= "android_usb",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.bind		= android_bind,
	.enable_function = android_enable_function,
};

void android_register_function(struct android_usb_function *f)
{
	struct android_dev *dev = _android_dev;
 
	pr_debug("%s: %s\n", __func__, f->name);
	list_add_tail(&f->list, &_functions);
	_registered_function_count++;

	/* bind our functions if they have all registered
	 * and the main driver has bound.
	 */


	printk(KERN_ERR "%s func:%s bind", __func__, f->name);
	
	if (dev->config && _registered_function_count == dev->num_functions) {
		bind_functions(dev);
		android_set_default_product(dev->product_id);
	}
}

/**
 * android_set_function_mask() - enables functions based on selected pid.
 * @up: selected product id pointer
 *
 * This function enables functions related with selected product id.extern int get_deb_demon_started(void);
 */


static void android_set_function_mask(struct android_usb_product *up)
{
	int index, found = 0;
	struct usb_function *func;

	list_for_each_entry(func, &android_config_driver.functions, list) {
		/* BEGIN:0012045 [yk.kim@lge.com] 2010-12-09, fix adb connect fail when CTS test */
		/*NRB_CHANGES_S [myoungkim@nuribom.com] 2011-07-15 HiddenMenu : add PORT */
		//adb enable after boot complete.
		//if (!switch_flag)  
		if(!get_adb_demon_started() || !switch_flag)
		{
		/* adb function enable/disable handled separetely */
		if (!strcmp(func->name, "adb"))
			continue;
		}
		/*NRB_CHANGES_E [myoungkim@nuribom.com]  */
		/* END:0011785 [yk.kim@lge.com] 2010-12-09 */
		
		for (index = 0; index < up->num_functions; index++) {
			if (!strcmp(up->functions[index], func->name)) {
				found = 1;
				break;
			}
		}

		if (found) { /* func is part of product. */
			/* if func is disabled, enable the same. */
			if (func->disabled)
				usb_function_set_enabled(func, 1);
			found = 0;
		} else { /* func is not part if product. */
			/* if func is enabled, disable the same. */
			if (!func->disabled)
				usb_function_set_enabled(func, 0);
		}
	}
}

/**
 * android_set_defaut_product() - selects default product id and enables
 * required functions
 * @product_id: default product id
 *
 * This function selects default product id using pdata information and
 * enables functions for same.
*/
static void android_set_default_product(int pid)
{
	struct android_dev *dev = _android_dev;
	struct android_usb_product *up = dev->products;
	int index;

	for (index = 0; index < dev->num_products; index++, up++) {
		if (pid == up->product_id)
			break;
	}
	USB_DBG("up->product_id 0x%x\n", up->product_id);
	android_set_function_mask(up);
}

/**
 * android_config_functions() - selects product id based on function need
 * to be enabled / disabled.
 * @f: usb function
 * @enable : function needs to be enable or disable
 *
 * This function selects product id having required function at first index.
 * TODO : Search of function in product id can be extended for all index.
 * RNDIS function enable/disable uses this.
*/
#if defined (CONFIG_USB_ANDROID_RNDIS) || defined (CONFIG_LGE_USB_GADGET_NDIS_DRIVER)
static void android_config_functions(struct usb_function *f, int enable)
{
	struct android_dev *dev = _android_dev;
	struct android_usb_product *up = dev->products;
	int index;
	char **functions;

	/* Searches for product id having function at first index */
	if (enable) {
		for (index = 0; index < dev->num_products; index++, up++) {
			functions = up->functions;
			if (!strcmp(*functions, f->name))
				break;
		}
		android_set_function_mask(up);
	} else
		android_set_default_product(dev->product_id);
}
#endif

/* BEGIN:0011202 [yk.kim@lge.com] 2010-11-21 */
/* ADD:0011202 usb switch composition by pid */
#ifdef CONFIG_LGE_USB_GADGET_DRIVER
int android_switch_composition(u16 pid)
{
	
	struct android_dev *dev = _android_dev;
	int ret = -EINVAL;
	switch_flag = 1;

	printk(KERN_ERR "%s : pid = %lx\n", __func__,pid);
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
    android_set_device_class(pid);
#endif

#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN_CGO
	if (pid == lg_charge_only_pid) {
		product_id = pid;		
		device_desc.idProduct = __constant_cpu_to_le16(pid);
		/* If we are in charge only pid, disconnect android gadget */
		usb_gadget_disconnect(dev->cdev->gadget);
		return 0;
	}
#endif

/* LGE_CHANGES_S [moses.son@lge.com] 2011-04-17, charge only mode */
	if (pid == lg_charge_only_pid_sprint){
		product_id = pid;		
		device_desc.idProduct = __constant_cpu_to_le16(pid);
		/* If we are in charge only pid, disconnect android gadget */
		usb_gadget_disconnect(dev->cdev->gadget);
		return 0;
	}
/* LGE_CHANGES_E [moses.son@lge.com] 2011-04-17 */

/* [yk.kim@lge.com] 2011-01-18, two UMS mode (eMMC/SDcard), need meid */
#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN 
#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
	if (pid == lg_ums_pid) {
		if (serial_number[0] == '\0') {
			strings_dev[STRING_SERIAL_IDX].s = random_serial_number;
			device_desc.iSerialNumber = 3;

			if (dev->cdev)
				dev->cdev->desc.iSerialNumber = device_desc.iSerialNumber;		
		}
	}
#else
	if (pid == lg_ums_pid) {

		if (serial_number[0] == '\0') {
			strings_dev[STRING_SERIAL_IDX].s = random_serial_number;
			device_desc.iSerialNumber = 3;

			if (dev->cdev)
				dev->cdev->desc.iSerialNumber = device_desc.iSerialNumber;		
		}

		hidden_pid = lg_ums_pid;
		goto OUT;
	}
	else {
		hidden_pid = pid;
	}
#endif
#endif

	device_desc.idProduct = __constant_cpu_to_le16(pid);
	android_set_default_product(pid);
#ifdef CONFIG_LGE_USB_GADGET_DRIVER
	product_id = pid;
#endif

	if (dev->cdev)
		dev->cdev->desc.idProduct = device_desc.idProduct;

#ifdef CONFIG_LGE_USB_GADGET_MTP_DRIVER
	if(device_desc.idProduct == lg_mtp_pid)
	{
		mtp_enable_flag = 1;
	}
	else
	{
		mtp_enable_flag = 0;
	}
#if 0	
	if(dev->mtp_enabled)
	{
		/* set mtp product name */
		strings_dev[STRING_PRODUCT_IDX].s = mtp_product_name;
	}
	else
	{
		strings_dev[STRING_PRODUCT_IDX].s = lg_product_name;
	}
#endif
#endif

#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
	ret = lge_bind_config(product_id);
#endif
#if 0
#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
    if(pid == lg_factory_pid || pid == lg_ndis_pid)
#else
#ifdef CONFIG_LGE_USB_GADGET_PLATFORM_DRIVER
    if(pid == lg_factory_pid || pid == lg_android_pid)
#else
    if(pid == lg_factory_pid || pid == lg_rmnet_pid)
#endif
#endif

//	diag_set_serial_number(serial_number);		/* LGE_CHANGES_S [moses.son@lge.com] 2011-02-09, need to check */
#endif
	/* LGE_CHANGES_S [jaeho.cho@lge.com] 2010-08-16, Autorun Serial Number */
#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
	if(product_id == lg_autorun_pid) {	   
		strings_dev[STRING_SERIAL_IDX].s = autorun_serial_number;
		device_desc.iSerialNumber = 3;
	}
	else {
		strings_dev[STRING_SERIAL_IDX].s = serial_number;
	}

	/* [yk.kim@lge.com] 2010-12-31, update serial number id */
	if (dev->cdev)
		dev->cdev->desc.iSerialNumber = device_desc.iSerialNumber;
#endif

OUT:
	/* force reenumeration */
	if (dev->cdev && dev->cdev->gadget &&
            dev->cdev->gadget->speed != USB_SPEED_UNKNOWN) {
		usb_gadget_disconnect(dev->cdev->gadget);
		msleep(10);
		ret = usb_gadget_connect(dev->cdev->gadget);
	}

    return ret;
}
#endif
/* END:0011202 [yk.kim@lge.com] 2010-11-21 */

#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
void android_set_device_class(u16 pid)
{
	struct android_dev *dev = _android_dev;
    int deviceclass = -1;
#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
#else
#ifdef CONFIG_LGE_USB_GADGET_PLATFORM_DRIVER
	if(pid == lg_android_pid) 
#else
    if(pid == lg_rmnet_pid) 
#endif
	{
	  deviceclass = USB_CLASS_COMM;
	  goto SetClass;
	}
#endif
#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
    if(pid == lg_factory_pid) 
    {
      deviceclass = USB_CLASS_COMM;
	  goto SetClass;
    }
#endif
#if defined(CONFIG_LGE_USB_GADGET_NDIS_DRIVER)
    if(pid == lg_ndis_pid) 
    {
  	  deviceclass = USB_CLASS_MISC;
  	  goto SetClass;
    }
#ifdef CONFIG_LGE_USB_GADGET_NDIS_VZW_DRIVER
    if(pid == lg_ums_pid) 
    {
  	  deviceclass = USB_CLASS_MISC;
  	  goto SetClass;
    }
#endif
#elif defined(CONFIG_LGE_USB_GADGET_ECM_DRIVER)
    if(pid == lg_ecm_pid) 
    {
  	  deviceclass = USB_CLASS_MISC;
  	  goto SetClass;
    }
#elif defined(CONFIG_LGE_USB_GADGET_RNDIS_DRIVER)
    if(pid == lg_rndis_pid) 
    {
  	  deviceclass = USB_CLASS_MISC;
  	  goto SetClass;
    }
#endif
#ifdef CONFIG_LGE_USB_GADGET_DRIVER
#ifdef CONFIG_LGE_USB_GADGET_NDIS_VZW_DRIVER
#else
    if(pid == lg_ums_pid) 
    {
  	  deviceclass = USB_CLASS_PER_INTERFACE;
  	  goto SetClass;
    }
#endif
#endif
#ifdef CONFIG_LGE_USB_GADGET_MTP_DRIVER
    if(pid == lg_mtp_pid) 
    {
  	  deviceclass = USB_CLASS_PER_INTERFACE;
  	  goto SetClass;
    }
#endif
#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
    if(pid == lg_autorun_pid) 
    {
  	  deviceclass = USB_CLASS_PER_INTERFACE;
  	  goto SetClass;
    }
#endif
#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN_CGO
    if(pid == lg_charge_only_pid) 
    {
  	  deviceclass = USB_CLASS_PER_INTERFACE;
  	  goto SetClass;
    }
#endif

SetClass:
	if(deviceclass == USB_CLASS_COMM)
	{
  		dev->cdev->desc.bDeviceClass = USB_CLASS_COMM;
		dev->cdev->desc.bDeviceSubClass      = 0x00;
		dev->cdev->desc.bDeviceProtocol      = 0x00;
	}
	else if(deviceclass == USB_CLASS_MISC)
	{
	  	dev->cdev->desc.bDeviceClass = USB_CLASS_MISC;
		dev->cdev->desc.bDeviceSubClass      = 0x02;
		dev->cdev->desc.bDeviceProtocol      = 0x01;
	}
	else if(deviceclass == USB_CLASS_PER_INTERFACE)
	{
		dev->cdev->desc.bDeviceClass = USB_CLASS_PER_INTERFACE;
		dev->cdev->desc.bDeviceSubClass      = 0x00;
		dev->cdev->desc.bDeviceProtocol      = 0x00;
	}
	else
	{
		dev->cdev->desc.bDeviceClass = USB_CLASS_PER_INTERFACE;
		dev->cdev->desc.bDeviceSubClass      = 0x00;
		dev->cdev->desc.bDeviceProtocol      = 0x00;
	}
}
#endif

void android_enable_function(struct usb_function *f, int enable)
{
	struct android_dev *dev = _android_dev;
	int disable = !enable;
	int product_id;

	if (!!f->disabled != disable) {
		usb_function_set_enabled(f, !disable);

#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
	if (!strcmp(f->name, "cdc_ethernet")) {

		/* We need to specify the COMM class in the device descriptor
		 * if we are using RNDIS.
		 */
		if (enable) {
			dev->cdev->desc.bDeviceClass = USB_CLASS_MISC;
			dev->cdev->desc.bDeviceSubClass      = 0x02;
			dev->cdev->desc.bDeviceProtocol      = 0x01;
		} else {
			dev->cdev->desc.bDeviceClass = USB_CLASS_PER_INTERFACE;
			dev->cdev->desc.bDeviceSubClass      = 0;
			dev->cdev->desc.bDeviceProtocol      = 0;
		}

		android_config_functions(f, enable);
	}
#else
#ifdef CONFIG_USB_ANDROID_RNDIS
		if (!strcmp(f->name, "rndis")) {

			/* We need to specify the COMM class in the device descriptor
			 * if we are using RNDIS.
			 */
			if (enable) {
#ifdef CONFIG_USB_ANDROID_RNDIS_WCEIS
				dev->cdev->desc.bDeviceClass = USB_CLASS_MISC;
				dev->cdev->desc.bDeviceSubClass      = 0x02;
				dev->cdev->desc.bDeviceProtocol      = 0x01;
#else
				dev->cdev->desc.bDeviceClass = USB_CLASS_COMM;
#endif
			} else {
				dev->cdev->desc.bDeviceClass = USB_CLASS_PER_INTERFACE;
				dev->cdev->desc.bDeviceSubClass      = 0;
				dev->cdev->desc.bDeviceProtocol      = 0;
			}

			android_config_functions(f, enable);
		}
#endif
#endif

		product_id = get_product_id(dev);
		device_desc.idProduct = __constant_cpu_to_le16(product_id);
		if (dev->cdev)
			dev->cdev->desc.idProduct = device_desc.idProduct;
		usb_composite_force_reset(dev->cdev);
	}
}

#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
static int android_set_usermode(const char *val, struct kernel_param *kp)
{
	int ret = 0;
	unsigned long tmp;

	ret = strict_strtoul(val, 16, &tmp);
	if (ret)
		return ret;

	autorun_user_mode = (unsigned int)tmp;
	pr_info("autorun user mode : %d\n", autorun_user_mode);

	return ret;
}

int get_autorun_user_mode(void)
{
	return autorun_user_mode;
}
EXPORT_SYMBOL(get_autorun_user_mode);
#endif

/* BEGIN:0011202 [yk.kim@lge.com] 2010-11-21 */
/* ADD:0011202 usb switch composition by pid */
#ifdef CONFIG_LGE_USB_GADGET_DRIVER
int android_set_pid(const char *val, struct kernel_param *kp)
{
	int ret = 0;
	unsigned long tmp;

	ret = strict_strtoul(val, 16, &tmp);
	if (ret)
		goto out;

	/* We come here even before android_probe, when product id
	 * is passed via kernel command line.
	 */
	if (!_android_dev) {
		device_desc.idProduct = tmp;
		goto out;
	}

#ifdef CONFIG_LGE_USB_GADGET_NDIS_DRIVER
	if (device_desc.idProduct == tmp) {
		pr_info("[%s] Requested product id is same(%lx), ignore it\n", __func__, tmp);
		goto out;
	}
#else
	/* [yk.kim@lge.com] 2010-01-03, prevents from mode switching init time */
	if ((device_desc.idProduct == tmp) && (hidden_pid == tmp)) {
		pr_info("[%s] Requested product id is same(%lx), ignore it\n", __func__, tmp);
		goto out;
	}
#endif
#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
	/* If cable is factory cable, we ignore request from user space */
	if (device_desc.idProduct == LGE_FACTORY_USB_PID) {
		pr_info("[%s] Factory USB cable is connected, ignore it\n", __func__);
		goto out;
	}
#endif
	set_pid_flag = 1;

	mutex_lock(&_android_dev->lock);
	pr_info("[%s] user set product id - %lx begin\n", __func__, tmp);
	ret = android_switch_composition(tmp);
	pr_info("[%s] user set product id - %lx complete\n", __func__, tmp);
	mutex_unlock(&_android_dev->lock);
out:
	return ret;
}

static int android_get_pid(char *buffer, struct kernel_param *kp)
{
	int ret;
    pr_debug("[%s] get product id - %lx\n", __func__, device_desc.idProduct);
	mutex_lock(&_android_dev->lock);
	ret = sprintf(buffer, "%x", device_desc.idProduct);
	mutex_unlock(&_android_dev->lock);
	return ret;
}

u16 android_get_product_id(void)
{
    if(device_desc.idProduct != 0x0000 && device_desc.idProduct != 0x0001)
    {
      return device_desc.idProduct;
    }
	else
	{
	  USB_DBG("LG_FW : product_id is not initialized : device_desc.idProduct = 0x%x\n", device_desc.idProduct);
	  return lg_default_pid;
	}
}
	  
#endif
/* END:0011202 [yk.kim@lge.com] 2010-11-21 */

#ifdef CONFIG_LGE_USB_GADGET_MTP_DRIVER
static int mtp_enable_open(struct inode *ip, struct file *fp)
{
	struct android_dev *dev = _android_dev;
	int ret = 0;
	
#if defined(CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB)
    if(product_id == lg_factory_pid)
    {
      return 0;
    }
#endif
	mutex_lock(&dev->lock);

	if (mtp_enable_flag)
		goto out;

	mtp_enable_flag = 1;
#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
	product_id = lg_mtp_pid;
#endif
    
	printk(KERN_INFO "mtp_enable_open mtp\n");
	
	ret = android_switch_composition(lg_mtp_pid);
out:
	mutex_unlock(&dev->lock);

	return ret;
}

static int mtp_enable_release(struct inode *ip, struct file *fp)
{
	struct android_dev *dev = _android_dev;
	int ret = 0;

#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN_CGO
	if (product_id == lg_charge_only_pid) {
		pr_info("%s: mtp disable on Charge only mode, not back to modem mode\n", __func__);
		mutex_lock(&dev->lock);
		mtp_enable_flag = 0;
		mutex_unlock(&dev->lock);
		return 0;
	}
#endif

#if defined(CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB)
    if(product_id == lg_factory_pid)
    {
      return 0;
    }
#endif
	mutex_lock(&dev->lock);

	if (!mtp_enable_flag)
		goto out;

	mtp_enable_flag = 0;

#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
	if (product_id == lg_mtp_pid) {
		product_id = lg_default_pid;
	}
#endif

	printk(KERN_INFO "mtp_enable_release mtp\n");

	ret = android_switch_composition(lg_default_pid);
out:
	mutex_unlock(&dev->lock);

	return ret;
}

static struct file_operations mtp_enable_fops = {
	.owner =   THIS_MODULE,
	.open =    mtp_enable_open,
	.release = mtp_enable_release,
};

static struct miscdevice mtp_enable_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "android_mtp_enable",
	.fops = &mtp_enable_fops,
};

#endif /*CONFIG_LGE_USB_GADGET_MTP_DRIVER*/

#ifdef CONFIG_DEBUG_FS
static int android_debugfs_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t android_debugfs_serialno_write(struct file *file, const char
				__user *buf,	size_t count, loff_t *ppos)
{
	char str_buf[MAX_STR_LEN];

	if (count > MAX_STR_LEN)
		return -EFAULT;

	if (copy_from_user(str_buf, buf, count))
		return -EFAULT;

	memcpy(serial_number, str_buf, count);

	if (serial_number[count - 1] == '\n')
		serial_number[count - 1] = '\0';

	strings_dev[STRING_SERIAL_IDX].s = serial_number;

	return count;
}
const struct file_operations android_fops = {
	.open	= android_debugfs_open,
	.write	= android_debugfs_serialno_write,
};

struct dentry *android_debug_root;
struct dentry *android_debug_serialno;

static int android_debugfs_init(struct android_dev *dev)
{
	android_debug_root = debugfs_create_dir("android", NULL);
	if (!android_debug_root)
		return -ENOENT;

	android_debug_serialno = debugfs_create_file("serial_number", 0222,
						android_debug_root, dev,
						&android_fops);
	if (!android_debug_serialno) {
		debugfs_remove(android_debug_root);
		android_debug_root = NULL;
		return -ENOENT;
	}
	return 0;
}

static void android_debugfs_cleanup(void)
{
       debugfs_remove(android_debug_serialno);
       debugfs_remove(android_debug_root);
}
#endif
static int __init android_probe(struct platform_device *pdev)
{
	struct android_usb_platform_data *pdata = pdev->dev.platform_data;
	struct android_dev *dev = _android_dev;
	int result;

#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
	/* BEGIN:0010739 [yk.kim@lge.com] 2010-11-11 */
	/* ADD:0010739 init fatory usb switch composition */
	extern int get_msm_cable_type(void);
	int andorid_manual_test_mode;
	extern int msm_get_manual_test_mode(void);

	andorid_manual_test_mode = msm_get_manual_test_mode();

	cable = get_msm_cable_type();

	
	if (cable == LG_UNKNOWN_CABLE && andorid_manual_test_mode)
		cable = LG_FACTORY_CABLE_56K_TYPE;
	
	if (cable==LG_FACTORY_CABLE_56K_TYPE || cable==LG_FACTORY_CABLE_130K_TYPE || cable==LG_FACTORY_CABLE_910K_TYPE)
	{
		pdev->dev.platform_data = &android_usb_pdata_factory;
		pdata = pdev->dev.platform_data;
		switch_cable = 1;
	}
	/* END:0010739 [yk.kim@lge.com] 2010-11-11 */
#endif
	dev_dbg(&pdev->dev, "%s: pdata: %p\n", __func__, pdata);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	result = pm_runtime_get(&pdev->dev);
	if (result < 0) {
		dev_err(&pdev->dev,
			"Runtime PM: Unable to wake up the device, rc = %d\n",
			result);
		return result;
	}

	if (pdata) {
		dev->products = pdata->products;
		dev->num_products = pdata->num_products;
		dev->functions = pdata->functions;
		dev->num_functions = pdata->num_functions;
#ifdef CONFIG_LGE_USB_GADGET_FUNC_BIND_ONLY_INIT
        dev->unique_function = pdata->unique_function;
#endif
		if (pdata->vendor_id)
			device_desc.idVendor =
				__constant_cpu_to_le16(pdata->vendor_id);
		if (pdata->product_id) {
			dev->product_id = pdata->product_id;
#ifdef CONFIG_LGE_USB_GADGET_DRIVER
            product_id = pdata->product_id;
#endif
			device_desc.idProduct =
				__constant_cpu_to_le16(pdata->product_id);
		}
		if (pdata->version)
			dev->version = pdata->version;

		if (pdata->product_name)
			strings_dev[STRING_PRODUCT_IDX].s = pdata->product_name;
		if (pdata->manufacturer_name)
			strings_dev[STRING_MANUFACTURER_IDX].s =
					pdata->manufacturer_name;

#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
		strings_dev[STRING_SERIAL_IDX].s = serial_number;
		
		if (pdata->serial_number)
		    sprintf(user_serial_number, "%s", pdata->serial_number);
#else
		if (pdata->serial_number)
			strings_dev[STRING_SERIAL_IDX].s = pdata->serial_number;
#endif
	}
#ifdef CONFIG_DEBUG_FS
	result = android_debugfs_init(dev);
	if (result)
		pr_debug("%s: android_debugfs_init failed\n", __func__);
#endif
	return usb_composite_register(&android_usb_driver);
}

static int andr_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int andr_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static struct dev_pm_ops andr_dev_pm_ops = {
	.runtime_suspend = andr_runtime_suspend,
	.runtime_resume = andr_runtime_resume,
};

static struct platform_driver android_platform_driver = {
	.driver = { .name = "android_usb", .pm = &andr_dev_pm_ops},
};

static int __init init(void)
{
	struct android_dev *dev;
	int ret = 0;
	
#if 0	/* LGE_CHANGES_S [moses.son@lge.com] 2011-02-09, don't need to control usb vreg */	
#ifdef CONFIG_LGE_USB_GADGET_SUPPORT_FACTORY_USB
	struct vreg *vreg_usb3_3 = 0;

	/* BEGIN:0010739 [yk.kim@lge.com] 2010-11-11 */
	/* ADD:0010739 USB in full-speed mode configure VDDA33 voltage to 3.4 when cable is connected */
	vreg_usb3_3 = vreg_get(NULL, "usb");

	ret = vreg_set_level(vreg_usb3_3, 3400);
	if (ret)
		USB_DBG("%s: vreg_set_level failed \n", __func__);

	ret = vreg_enable(vreg_usb3_3);
	if (ret)
		USB_DBG("%s: vreg_enable failed \n", __func__);
	/* END:0010739 [yk.kim@lge.com] 2010-11-11 */
#endif
#endif

	pr_debug("android init\n");
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	/* set default values, which should be overridden by platform data */
	dev->product_id = PRODUCT_ID;
	_android_dev = dev;

#ifdef CONFIG_LGE_USB_GADGET_DRIVER
    mutex_init(&dev->lock);
#endif

#if defined(CONFIG_LGE_USB_GADGET_MTP_DRIVER)
	ret = mtp_function_init();
	if (ret)
		goto out;

#if defined(CONFIG_LGE_USB_GADGET_MTP_DRIVER)
	ret = misc_register(&mtp_enable_device);
	if (ret)
		goto out;
#endif
#endif

#if defined(CONFIG_LGE_USB_GADGET_MTP_DRIVER)
    ret = platform_driver_probe(&android_platform_driver, android_probe);
    if(ret)
		goto mtp_exit;
#else
	return platform_driver_probe(&android_platform_driver, android_probe);
#endif

#ifdef CONFIG_LGE_USB_GADGET_MTP_DRIVER
    return 0;

mtp_exit:
	misc_deregister(&mtp_enable_device);
    mtp_function_exit();
out:
	return ret;
#endif
}
module_init(init);

static void __exit cleanup(void)
{
#ifdef CONFIG_DEBUG_FS
	android_debugfs_cleanup();
#endif
	usb_composite_unregister(&android_usb_driver);
	platform_driver_unregister(&android_platform_driver);
	kfree(_android_dev);
	_android_dev = NULL;
}
module_exit(cleanup);
