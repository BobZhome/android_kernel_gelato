/*
 *  apds9190.c - Linux kernel modules for ambient proximity sensor
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <mach/board_lge.h>

#include <mach/gpio.h>

#include <asm/uaccess.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <mach/vreg.h>
#include <linux/wakelock.h>
#include <mach/msm_i2ckbd.h>
#include <linux/spinlock.h>

#include <linux/string.h>

#ifdef CONFIG_LGE_PCB_VERSION
extern void lge_set_hw_version_string(char *pcb_version);
#endif

//#define APDS9190_TUNE

#define APDS9190_DRV_NAME	"proximity_apds9190"
#define DRIVER_VERSION		"1.0.0"

#define STRONG_LIGHT
/*
 * Defines
 */

#define APDS9190_ENABLE_REG		0x00
#define APDS9190_ATIME_REG		0x01 //ALS Non Use, Reserved
#define APDS9190_PTIME_REG		0x02
#define APDS9190_WTIME_REG		0x03
#define APDS9190_AILTL_REG		0x04 //ALS Non Use, Reserved
#define APDS9190_AILTH_REG		0x05 //ALS Non Use, Reserved
#define APDS9190_AIHTL_REG		0x06 //ALS Non Use, Reserved
#define APDS9190_AIHTH_REG		0x07 //ALS Non Use, Reserved
#define APDS9190_PILTL_REG		0x08
#define APDS9190_PILTH_REG		0x09
#define APDS9190_PIHTL_REG		0x0A
#define APDS9190_PIHTH_REG		0x0B
#define APDS9190_PERS_REG		0x0C
#define APDS9190_CONFIG_REG		0x0D
#define APDS9190_PPCOUNT_REG	0x0E
#define APDS9190_CONTROL_REG	0x0F
#define APDS9190_REV_REG		0x11
#define APDS9190_ID_REG			0x12
#define APDS9190_STATUS_REG		0x13
#define APDS9190_CDATAL_REG		0x14 //ALS Non Use, Reserved
#define APDS9190_CDATAH_REG		0x15 //ALS Non Use, Reserved
#define APDS9190_IRDATAL_REG	0x16 //ALS Non Use, Reserved
#define APDS9190_IRDATAH_REG	0x17 //ALS Non Use, Reserved
#define APDS9190_PDATAL_REG		0x18
#define APDS9190_PDATAH_REG		0x19

#define CMD_BYTE				0x80
#define CMD_WORD				0xA0
#define CMD_SPECIAL				0xE0

#define CMD_CLR_PS_INT			0xE5
#define CMD_CLR_ALS_INT			0xE6
#define CMD_CLR_PS_ALS_INT		0xE7

#define APDS9190_ENABLE_PIEN 	0x20 
#ifdef STRONG_LIGHT
#define APDS9190_ENABLE_AIEN 	0x10 
#else
#define APDS9190_ENABLE_AIEN 	0x00 //ALS Non Use, Reserved
#endif
#define APDS9190_ENABLE_WEN 	0x08 
#define APDS9190_ENABLE_PEN 	0x04 

//Proximity false Interrupt by strong ambient light
#ifdef STRONG_LIGHT
#define APDS9190_ENABLE_AEN 	0x02 
#else
//Non use Proximity false Interrupt by strong ambient light
#define APDS9190_ENABLE_AEN 	0x00 //ALS Non Use, Reserved 
#endif
#define APDS9190_ENABLE_PON 	0x01


//Proximity false Interrupt by strong ambient light
#ifdef STRONG_LIGHT
#define ATIME 	 				0xDB 	// 100.64ms . minimum ALS integration time //ALS Non Use
#else
#define ATIME 	 				0x00 	// ALS Non Use
#endif

#define WTIME 					0xFF 	// 2.72 ms . 
#define PTIME 	 				0xFF 	
#define INT_PERS 				0x33

#define PDIODE 	 				0x20 	// IR Diode
#define PGAIN 				 	0x00 	//1x Prox gain
#define AGAIN 					0x00 	//1x ALS gain, Reserved ALS Non Use

#ifdef CONFIG_LGE_PCB_VERSION
#ifdef CONFIG_MACH_MSM7X27_GELATO_DOP //DOP Request H/W Threshold
#define REV_B_APDS_PROXIMITY_HIGH_THRESHHOLD		140
#define REV_B_APDS_PROXIMITY_LOW_THRESHHOLD			60

#define REV_C_APDS_PROXIMITY_HIGH_THRESHHOLD		140
#define REV_C_APDS_PROXIMITY_LOW_THRESHHOLD			60

#define REV_D_APDS_PROXIMITY_HIGH_THRESHHOLD		400
#define REV_D_APDS_PROXIMITY_LOW_THRESHHOLD			200

#define REV_B_PPCOUNT 			10 		//prox pulse count
#define REV_B_PDRIVE 	 		0 		//100mA of LED Power

#define REV_C_PPCOUNT 			10 		//prox pulse count
#define REV_C_PDRIVE 	 		0 		//100mA of LED Power

#define REV_D_PPCOUNT 			15 		//prox pulse count
#define REV_D_PDRIVE 		 	0 		//100mA of LED Power
#else //Qwerty
#define REV_B_APDS_PROXIMITY_HIGH_THRESHHOLD		600
#define REV_B_APDS_PROXIMITY_LOW_THRESHHOLD			550

#define REV_C_APDS_PROXIMITY_HIGH_THRESHHOLD		400
#define REV_C_APDS_PROXIMITY_LOW_THRESHHOLD			250

#define REV_D_APDS_PROXIMITY_HIGH_THRESHHOLD		400
#define REV_D_APDS_PROXIMITY_LOW_THRESHHOLD			250

#define REV_B_PPCOUNT 			10 		//prox pulse count
#define REV_B_PDRIVE	 	 	0 		//100mA of LED Power

#define REV_C_PPCOUNT 			13 		//prox pulse count
#define REV_C_PDRIVE 		 	0 		//100mA of LED Power

#define REV_D_PPCOUNT 			13 		//prox pulse count
#define REV_D_PDRIVE 		 	0 		//50mA of LED Power
#endif
#endif

#define DEFAULT_APDS_PROXIMITY_HIGH_THRESHHOLD		600
#define DEFAULT_APDS_PROXIMITY_LOW_THRESHHOLD		550

#define DEFAULT_PPCOUNT 		10 		//prox pulse count
#define DEFAULT_PDRIVE 		 	0 		//100mA of LED Power




static int apds_proxi_high_threshold = 0;
static int apds_proxi_low_threshold = 0;

static int apds_ppcount = 0;
static int apds_pdrive = 0;


static char hw_pcb_version[10] = "Unknown";

#define APDS9190_STATUS_PINT_AINT	0x30
#define APDS9190_STATUS_PINT		0x20
#define APDS9190_STATUS_AINT		0x10 // ALS Interrupt STATUS ALS Non Use

#define PROX_SENSOR_DETECT_N	(0)

enum apds9190_input_event {
		PROX_INPUT_NEAR = 0,
		PROX_INPUT_FAR,
};


#define APDS900_SENSOR_DEBUG 0
#if APDS900_SENSOR_DEBUG
#define DEBUG_MSG(args...)  printk(args)
#else
#define DEBUG_MSG(args...)
#endif


/*
 * Structs
 */

struct apds9190_data {
		struct i2c_client *client;
		struct mutex update_lock;

		unsigned int enable;
		unsigned int atime;
		unsigned int ptime;
		unsigned int wtime;
		unsigned int ailt;
		unsigned int aiht;
		unsigned int pilt;
		unsigned int piht;
		unsigned int pers;
		unsigned int config;
		unsigned int ppcount;
		unsigned int control;

		unsigned int pDrive;

		unsigned int GA;
		unsigned int DF;
		unsigned int LPC;

		int irq;
		unsigned int isNear;
		unsigned int last_isNear;
		unsigned int sw_mode;
		spinlock_t lock;
		struct input_dev *input_dev;
		struct work_struct dwork;
		struct work_struct poll_dwork;
};

enum apds9190_dev_status {
		PROX_STAT_SHUTDOWN = 0,
		PROX_STAT_OPERATING,
};

static struct i2c_client *apds_9190_i2c_client = NULL;
static struct workqueue_struct *proximity_wq = NULL;
static int apds_9190_initlizatied = 0;
static int enable_status = 0;
static int methods = -1;
#ifdef APDS9190_TUNE
static int g_pilt;
static int g_piht;
#endif


static int apds9190_set_command(struct i2c_client *client, int command)
{
		struct apds9190_data *data = i2c_get_clientdata(client);
		int ret, i;
		int clearInt;

		if (command == 0)
		{
			clearInt = CMD_CLR_PS_INT;
			printk(KERN_INFO "%s, clear [CMD_CLR_PS_INT]\n",__func__);
		}
		else if (command == 1)
		{
			clearInt = CMD_CLR_ALS_INT;
		}
		else
		{
			clearInt = CMD_CLR_PS_ALS_INT;
			printk(KERN_INFO "%s, clear [CMD_CLR_PS_ALS_INT]\n",__func__);			
		}

		mutex_lock(&data->update_lock);
		for(i = 0;i<10;i++) {
			ret = i2c_smbus_write_byte(client, clearInt);
			if(ret < 0) {
				printk("%s, I2C Fail\n",__func__);
				continue;
			}
			else
				break;
				
		}
		mutex_unlock(&data->update_lock);

		return ret;
}

static int apds9190_set_enable(struct i2c_client *client, int enable)
{
		struct apds9190_data *data = i2c_get_clientdata(client);
		int ret = 0;

		mutex_lock(&data->update_lock);
		ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9190_ENABLE_REG, enable);
		mutex_unlock(&data->update_lock);

		DEBUG_MSG("apds9190_set_enable = [%x] \n",enable);

		data->enable = enable;

		return ret;
}
#ifdef STRONG_LIGHT
static int apds9190_set_atime(struct i2c_client *client, int atime)
{
		struct apds9190_data *data = i2c_get_clientdata(client);
		int ret = 0;

		mutex_lock(&data->update_lock);
		ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9190_ATIME_REG, ATIME);
		mutex_unlock(&data->update_lock);

		data->atime = atime;

		return ret;

}
#endif
static int apds9190_set_ptime(struct i2c_client *client, int ptime)
{
		struct apds9190_data *data = i2c_get_clientdata(client);
		int ret = 0;

		mutex_lock(&data->update_lock);
		ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9190_PTIME_REG, ptime);
		mutex_unlock(&data->update_lock);

		data->ptime = ptime;

		return ret;
}

static int apds9190_set_wtime(struct i2c_client *client, int wtime)
{
		struct apds9190_data *data = i2c_get_clientdata(client);
		int ret = 0;

		mutex_lock(&data->update_lock);
		ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9190_WTIME_REG, wtime);
		mutex_unlock(&data->update_lock);

		data->wtime = wtime;

		return ret;
}

static int apds9190_set_pilt(struct i2c_client *client, int threshold)
{
		struct apds9190_data *data = i2c_get_clientdata(client);
		int ret = 0;

		mutex_lock(&data->update_lock);
		ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS9190_PILTL_REG, threshold);
		mutex_unlock(&data->update_lock);

		data->pilt = threshold;

		return ret;
}

static int apds9190_set_piht(struct i2c_client *client, int threshold)
{
		struct apds9190_data *data = i2c_get_clientdata(client);
		int ret = 0;

		mutex_lock(&data->update_lock);
		ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS9190_PIHTL_REG, threshold);
		mutex_unlock(&data->update_lock);
		
		data->piht = threshold;

		return ret;
}
#ifdef STRONG_LIGHT
static int apds9190_set_aiht(struct i2c_client* client, int threshold)
{
	struct apds9190_data *data = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS9190_AIHTL_REG, threshold);
	mutex_unlock(&data->update_lock);
	data->aiht = threshold;

	return ret;
}


static int apds9190_set_ailt(struct i2c_client* client, int threshold)
{
	struct apds9190_data *data = i2c_get_clientdata(client);
	int ret = 0;
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS9190_AILTL_REG, threshold);
	mutex_unlock(&data->update_lock);	
	data->ailt = threshold;

	return ret;
}
#endif
static int apds9190_set_pers(struct i2c_client *client, int pers)
{
		struct apds9190_data *data = i2c_get_clientdata(client);
		int ret = 0;
		mutex_lock(&data->update_lock);
		ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9190_PERS_REG, pers);
		mutex_unlock(&data->update_lock);

		if(ret < 0) {
			printk(KERN_INFO "%s, pers Register Write Fail\n",__func__);
			return -1;
		}


		data->pers = pers;

		return ret;
}

static int apds9190_set_config(struct i2c_client *client, int config)
{
		struct apds9190_data *data = i2c_get_clientdata(client);
		int ret = 0;

		mutex_lock(&data->update_lock);
		ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9190_CONFIG_REG, config);
		mutex_unlock(&data->update_lock);

		data->config = config;

		return ret;
}

static int apds9190_set_ppcount(struct i2c_client *client, int ppcount)
{
		struct apds9190_data *data = i2c_get_clientdata(client);
		int ret;

		mutex_lock(&data->update_lock);
		ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9190_PPCOUNT_REG, ppcount);
		mutex_unlock(&data->update_lock);

		data->ppcount = ppcount;

		return ret;
}

static int apds9190_set_control(struct i2c_client *client, int control)
{
		struct apds9190_data *data = i2c_get_clientdata(client);
		int ret;

		mutex_lock(&data->update_lock);
		ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9190_CONTROL_REG, control);
		mutex_unlock(&data->update_lock);

		data->control = control;
		return ret;
}

static int apds_9190_initialize(void)
{
		struct apds9190_data *data = i2c_get_clientdata(apds_9190_i2c_client);
		u8 enable;
		int err = 0;
		
		data->pDrive = apds_pdrive;

		enable = APDS9190_ENABLE_PIEN | APDS9190_ENABLE_AIEN | APDS9190_ENABLE_WEN | APDS9190_ENABLE_PEN | 
				APDS9190_ENABLE_AEN | APDS9190_ENABLE_PON;

		err = apds9190_set_enable(apds_9190_i2c_client,enable);
		
		if(err < 0){
			printk(KERN_INFO "%s, enable set Fail\n",__func__);
			goto EXIT;
		}

		err = apds9190_set_wtime(apds_9190_i2c_client, WTIME);
		if(err < 0){
			printk(KERN_INFO "%s, wtime set Faile\n",__func__);
			goto EXIT;
		}

		err = apds9190_set_ptime(apds_9190_i2c_client, PTIME);
		if(err < 0){
			printk(KERN_INFO "%s, ptime set Fail\n",__func__);
			goto EXIT;
		}
#ifdef STRONG_LIGHT
		err =  apds9190_set_atime(apds_9190_i2c_client, ATIME);
		if(err < 0){
			printk(KERN_INFO "%s, atime set Fail\n",__func__);
			goto EXIT;
		}	
#endif		
		err = apds9190_set_pers(apds_9190_i2c_client, 0x22); //Interrupt persistence
		if(err < 0){
			printk(KERN_INFO "%s, pers set Fail\n",__func__);
			goto EXIT;
		}

		err = apds9190_set_config(apds_9190_i2c_client, 0x00); // Wait long timer <- no needs so set 0
		if(err < 0){
			printk(KERN_INFO "%s, config set Fail\n",__func__);
			goto EXIT;
		}

		err = apds9190_set_ppcount(apds_9190_i2c_client, apds_ppcount); // Pulse count for proximity
		if(err < 0){
			printk(KERN_INFO "%s, ppcount set Fail\n",__func__);
			goto EXIT;
		}

		err = apds9190_set_control(apds_9190_i2c_client, data->pDrive| PDIODE | PGAIN | AGAIN);
		if(err < 0){
			printk(KERN_INFO "%s, control set Fail\n",__func__);
			goto EXIT;
		}

		err = apds9190_set_pilt(apds_9190_i2c_client, 0); // init threshold for proximity
		if(err < 0){
			printk(KERN_INFO "%s, pilt set Fail\n",__func__);
			goto EXIT;
		}
		
		err = apds9190_set_piht(apds_9190_i2c_client, apds_proxi_high_threshold);
		if(err < 0){
			printk(KERN_INFO "%s, piht set Fail\n",__func__);
			goto EXIT;
		}
#ifdef STRONG_LIGHT
		err = apds9190_set_aiht(apds_9190_i2c_client, (75*(1024*(256-data->atime)))/100);
		if(err < 0){
			printk(KERN_INFO "%s, aiht set Fail\n",__func__);
			goto EXIT;
		}
		
		err = apds9190_set_ailt(apds_9190_i2c_client, 0);
		if(err < 0){
			printk(KERN_INFO "%s, ailt set Fail\n",__func__);
			goto EXIT;
		}		
#endif		
		enable_status = enable;

		data->pilt = 0;
		data->piht = apds_proxi_high_threshold;

#ifdef APDS9190_TUNE		
		g_pilt = apds_proxi_low_threshold;
		g_piht = apds_proxi_high_threshold;
#endif		
		return 0;
EXIT:
	return err;
}


/*
 * SysFS support
 */

static ssize_t apds9190_status_show(struct device *dev, 
				struct device_attribute *attr, char *buf)
{
		struct i2c_client *client = to_i2c_client(dev);
		struct apds9190_data *data = i2c_get_clientdata(client);

		if(client != NULL)
				return sprintf(buf, "%d\n",data->last_isNear);
		else 
			return -1;
}
static DEVICE_ATTR(show, S_IRUGO | S_IWUSR, apds9190_status_show, NULL);	

static ssize_t
apds9190_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
		struct i2c_client *client = to_i2c_client(dev);
		struct apds9190_data *data = i2c_get_clientdata(client);

		return snprintf(buf, PAGE_SIZE, "%d\n", data->sw_mode);
}

static int apds9190_suspend(struct i2c_client *i2c_dev, pm_message_t state);
static int apds9190_resume(struct i2c_client *i2c_dev);


static ssize_t
apds9190_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
		struct i2c_client *client = to_i2c_client(dev);
		struct apds9190_data *pdev = i2c_get_clientdata(client);
		pm_message_t dummy_state;
		int mode;

		dummy_state.event = 0;

		sscanf(buf, "%d", &mode);

		if ((mode != PROX_STAT_SHUTDOWN) && (mode != PROX_STAT_OPERATING)) {
				printk(KERN_INFO "Usage: echo [0 | 1] > enable");
				printk(KERN_INFO " 0: disable\n");
				printk(KERN_INFO " 1: enable\n");
				return count;
		}

		if (mode == pdev->sw_mode) {
				printk(KERN_INFO "mode is already %d\n", pdev->sw_mode);
				return count;
		}
		else {
				if (mode) {
						apds9190_resume(client);
						printk(KERN_INFO "Power On Enable\n");
				}
				else {
						apds9190_suspend(client, dummy_state);
						printk(KERN_INFO "Power Off Disable\n");
				}
		}

		return count;
}
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, apds9190_enable_show, apds9190_enable_store);	


static ssize_t apds9190_show_ptime(struct device *dev,
				struct device_attribute *attr, char *buf)
{
		struct i2c_client *client = to_i2c_client(dev);
		struct apds9190_data *data = i2c_get_clientdata(client);
		if(client != NULL)
				return sprintf(buf, "%d\n",data->ptime);
		else
				return -1;
}

static ssize_t apds9190_store_ptime(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
		struct i2c_client *client = to_i2c_client(dev);
		unsigned long rdata = simple_strtoul(buf, NULL, 10);	

		if(client != NULL)
				apds9190_set_ptime(client,rdata);
		else
				return -1;

		return count;
}

static DEVICE_ATTR(ptime, S_IRUGO | S_IWUSR, apds9190_show_ptime, apds9190_store_ptime);	

static ssize_t apds9190_show_wtime(struct device *dev,
				struct device_attribute *attr, char *buf)
{
		struct i2c_client *client = to_i2c_client(dev);
		struct apds9190_data *data = i2c_get_clientdata(client);

		if(client != NULL)
				return sprintf(buf, "%d\n",data->wtime);
		else
				return -1;
}

static ssize_t apds9190_store_wtime(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
		struct i2c_client *client = to_i2c_client(dev);
		unsigned long rdata = simple_strtoul(buf, NULL, 10);	

		if(client != NULL)
				apds9190_set_wtime(client,rdata);
		else
				return -1;

		return count;
}

static DEVICE_ATTR(wtime, S_IRUGO | S_IWUSR, apds9190_show_wtime, apds9190_store_wtime);	

static ssize_t apds9190_show_ppcount(struct device *dev,
				struct device_attribute *attr, char *buf)
{
		struct i2c_client *client = to_i2c_client(dev);
		struct apds9190_data *data = i2c_get_clientdata(client);

		if(client != NULL)
				return sprintf(buf, "%d\n",data->ppcount);
		else
				return -1;
}

static ssize_t apds9190_store_ppcount(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
		struct i2c_client *client = to_i2c_client(dev);
		int rdata;
		sscanf(buf, "%d", &rdata);

		if(client != NULL)
				apds9190_set_ppcount(client,rdata);
		else
				return -1;

		return count;
}

static DEVICE_ATTR(ppcount, S_IRUGO | S_IWUSR, apds9190_show_ppcount, apds9190_store_ppcount);	

static ssize_t apds9190_show_pers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
		struct i2c_client *client = to_i2c_client(dev);
		struct apds9190_data *data = i2c_get_clientdata(client);

		if(client != NULL)
				return sprintf(buf, "%d\n",data->pers);
		else
				return -1;
}

static ssize_t apds9190_store_pers(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
		struct i2c_client *client = to_i2c_client(dev);
		unsigned long rdata = simple_strtoul(buf, NULL, 10);	

		if(client != NULL)
				apds9190_set_pers(client,rdata);
		else
				return -1;

		return count;
}

static DEVICE_ATTR(pers, S_IRUGO | S_IWUSR, apds9190_show_pers, apds9190_store_pers);

static ssize_t apds9190_show_pilt(struct device *dev,
				struct device_attribute *attr, char *buf)
{
		struct i2c_client *client = to_i2c_client(dev);
		struct apds9190_data *data = i2c_get_clientdata(client);

		if(client != NULL)
				return sprintf(buf, "%d\n",data->pilt);
		else
				return -1;
}

static ssize_t apds9190_store_pilt(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
		struct i2c_client *client = to_i2c_client(dev);
		unsigned int rdata;

		sscanf(buf, "%d", &rdata);
#ifdef APDS9190_TUNE		
		g_pilt = rdata;
#endif
		if(client != NULL)
			apds9190_set_pilt(client, rdata);
		else
				return -1;

		return count;
}

static DEVICE_ATTR(pilt, S_IRUGO | S_IWUSR, apds9190_show_pilt, apds9190_store_pilt);	


static ssize_t apds9190_show_piht(struct device *dev,
				struct device_attribute *attr, char *buf)
{
		struct i2c_client *client = to_i2c_client(dev);
		struct apds9190_data *data = i2c_get_clientdata(client);

		if(client != NULL)
				return sprintf(buf, "%d\n",data->piht);
		else
				return -1;
}

static ssize_t apds9190_store_piht(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
		struct i2c_client *client = to_i2c_client(dev);
		unsigned int rdata;

		sscanf(buf, "%d", &rdata);
#ifdef APDS9190_TUNE		
		g_piht = rdata;
#endif
		if(client != NULL)
			apds9190_set_piht(client, rdata);
		else
				return -1;

		return count;
}

static DEVICE_ATTR(piht, S_IRUGO | S_IWUSR, apds9190_show_piht, apds9190_store_piht);	


static ssize_t apds9190_show_pdata(struct device *dev,
				struct device_attribute *attr, char *buf)
{
		struct i2c_client *client = to_i2c_client(dev);

		if(client != NULL){
				int pdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS9190_PDATAL_REG);		

				return sprintf(buf, "%d\n",pdata);
		}
		else{
				return -1;
		}
}

static DEVICE_ATTR(pdata, S_IRUGO | S_IWUSR, apds9190_show_pdata, NULL);	


static ssize_t apds9190_show_interrupt(struct device *dev,
				struct device_attribute *attr, char *buf)
{
		return sprintf(buf, "%d\n",enable_status);
}

static ssize_t apds9190_store_interrupt(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
		// this value should be same with the value in sensors.cpp
#define STORE_INTERUPT_SELECT_PROXIMITY		0x02


		struct i2c_client *client = to_i2c_client(dev);
		struct apds9190_data *data = i2c_get_clientdata(client);
		unsigned long rdata = simple_strtoul(buf, NULL, 10);	
		int enable = (int)rdata;
		int ret;

		DEBUG_MSG("apds9190_store_interrupt = [%d] apds_9190_initlizatied [%d] \n",rdata, apds_9190_initlizatied);

		if(!apds_9190_initlizatied){
				apds_9190_initialize();
				apds_9190_initlizatied = 1;		
		}

		if(enable & STORE_INTERUPT_SELECT_PROXIMITY)
		{	
				if(enable & 0x01) // enable
				{
						data->enable |= (APDS9190_ENABLE_PIEN|APDS9190_ENABLE_PEN|APDS9190_ENABLE_PON); 	
				}
				else		//disable
				{
						data->enable &= ~(APDS9190_ENABLE_PIEN|APDS9190_ENABLE_PEN); 	
				}
		}

		if(data->enable == 1)
		{
				data->enable = 0;
		}

		ret = apds9190_set_enable(client, data->enable);

		enable_status = data->enable;


		DEBUG_MSG("apds9190_store_interrupt enable_status = [%x] data->enable [%x] \n",enable_status, data->enable);

		if (ret < 0)
				return ret;

		return count;
}

static DEVICE_ATTR(interrupt, 0664,
				apds9190_show_interrupt, apds9190_store_interrupt);		   


static ssize_t apds9190_show_pdrive(struct device *dev,
				struct device_attribute *attr, char *buf)
{
		struct i2c_client *client = to_i2c_client(dev);
		struct apds9190_data *data = i2c_get_clientdata(client);

		if(client != NULL)
				return sprintf(buf, "%d\n",data->pDrive);

		else
				return -1;
}

static ssize_t apds9190_store_pdrive(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
		struct i2c_client *client = to_i2c_client(dev);
		struct apds9190_data *data = i2c_get_clientdata(client);
		unsigned int rdata;

		sscanf(buf, "%d", &rdata);

		if(client != NULL){
				data->pDrive= rdata;
				apds9190_set_control(client,(data->pDrive | PDIODE | PGAIN | AGAIN));
		}
		else
				return -1;

		return count;
}

static DEVICE_ATTR(pdrive, S_IRUGO | S_IWUSR, apds9190_show_pdrive, apds9190_store_pdrive);	



static struct attribute *apds9190_attributes[] = {
		&dev_attr_show.attr,
		&dev_attr_enable.attr,
		&dev_attr_ptime.attr,
		&dev_attr_wtime.attr,
		&dev_attr_ppcount.attr,
		&dev_attr_pers.attr,
		&dev_attr_pilt.attr,
		&dev_attr_piht.attr,
		&dev_attr_pdata.attr,
		&dev_attr_interrupt.attr,
		&dev_attr_pdrive.attr,
		NULL
};

static const struct attribute_group apds9190_attr_group = {
		.attrs = apds9190_attributes,
};

/*
 * Initialization function
 */

static int apds9190_init_client(struct i2c_client *client)
{
		struct apds9190_data *data = i2c_get_clientdata(apds_9190_i2c_client);
		int err;

		apds9190_set_enable(apds_9190_i2c_client, 0);

		mutex_lock(&data->update_lock);
		err = i2c_smbus_read_byte_data(apds_9190_i2c_client, APDS9190_ENABLE_REG);
		mutex_unlock(&data->update_lock);

		if (err != 0)
				return -ENODEV;

		DEBUG_MSG("apds9190_init_client\n");

		data->enable = 0;

		return 0;
}

static void apds9190_event_report(int state)
{
		struct apds9190_data *data = i2c_get_clientdata(apds_9190_i2c_client);	 

		if(state == 1) {
			printk(KERN_INFO "%s, Far report\n",__func__);
			input_report_abs(data->input_dev, ABS_DISTANCE, state);
			input_sync(data->input_dev);
		} else {
			printk(KERN_INFO "%s, Near report\n",__func__);

			input_report_abs(data->input_dev, ABS_DISTANCE, state);
			input_sync(data->input_dev);				
		}
		data->last_isNear = data->isNear;
}

void apds_9190_proximity_handler(struct apds9190_data *data) 	
{
		int pdata = i2c_smbus_read_word_data(apds_9190_i2c_client, CMD_WORD|APDS9190_PDATAL_REG);		
				
		if((pdata > data->pilt) && (pdata >= data->piht)){
				apds9190_set_enable(apds_9190_i2c_client,0);
				data->isNear = 0;
				apds9190_set_piht(apds_9190_i2c_client, 1023);
#ifdef APDS9190_TUNE
				apds9190_set_pilt(apds_9190_i2c_client, g_pilt);
#else
				apds9190_set_pilt(apds_9190_i2c_client, apds_proxi_low_threshold);
#endif
				printk(KERN_INFO "%s, piht : %d\n",__func__, data->piht);
				printk(KERN_INFO "%s, pilt : %d\n",__func__, data->pilt);
				printk(KERN_INFO "%s, reg pdata : %d \n",__func__, pdata);
				
		}
		else if((pdata < data->piht) && (pdata <= data->pilt)) {
				apds9190_set_enable(apds_9190_i2c_client,0);		
				data->isNear = 1;

				apds9190_set_pilt(apds_9190_i2c_client, 0);
#ifdef APDS9190_TUNE
				apds9190_set_piht(apds_9190_i2c_client, g_piht);
#else	
				apds9190_set_piht(apds_9190_i2c_client, apds_proxi_high_threshold);
#endif
				printk(KERN_INFO "%s, piht : %d\n",__func__, data->piht);
				printk(KERN_INFO "%s, pilt : %d\n",__func__, data->pilt);				
				printk(KERN_INFO "%s, reg pdata : %d \n",__func__, pdata);

		}

		if(data->isNear != data->last_isNear){
				apds9190_event_report(data->isNear);
				data->last_isNear = data->isNear;
		}

}


#define PROX_LOW_TH		(0x6F) // far threshold
#define PROX_HIGH_TH	(0x70) // near threshold

void apds_9190_irq_work_func(struct work_struct *work) 	
{
		struct apds9190_data *data = 
				container_of(work, struct apds9190_data, dwork);
		struct proximity_platform_data		*pdev = NULL;	
#ifdef STRONG_LIGHT		
		int status, pdata, cdata;
#else
		int status, pdata;
#endif
		int org_enable = data->enable;
			

		pdev = data->client->dev.platform_data;

		if(pdev->methods){
				
				status = i2c_smbus_read_byte_data(apds_9190_i2c_client, CMD_BYTE|APDS9190_STATUS_REG);
#ifdef STRONG_LIGHT

				if((status & APDS9190_STATUS_PINT_AINT) == 0x30) 
				{
					disable_irq(data->irq);

					cdata = i2c_smbus_read_word_data(apds_9190_i2c_client, CMD_WORD|APDS9190_CDATAL_REG);
				
					printk(KERN_INFO "%s, [APDS9190_STATUS_PINT_AINT] status : %d,   cdata : %d, isNear : %d\n",__func__, status, cdata, data->isNear);		

					if((data->isNear == 0) && (cdata >= (75*(1024*(256-data->atime)))/100) )
					{
						pdata = i2c_smbus_read_word_data(apds_9190_i2c_client, CMD_WORD|APDS9190_PDATAL_REG);													
						printk(KERN_INFO "%s, [APDS9190_STATUS_PINT_AINT] cdata : %d, pdata : %d\n", __func__, cdata, pdata);

						printk(KERN_INFO "%s, [APDS9190_STATUS_PINT_AINT] status change Near to Far while Near status but couldn't recognize Far\n", __func__);
						printk(KERN_INFO "%s, [APDS9190_STATUS_PINT_AINT] Force status to change Far\n",__func__);
						data->isNear = 1;
						data->last_isNear = 0;
						apds9190_set_pilt(apds_9190_i2c_client, 0);
#ifdef APDS9190_TUNE
						apds9190_set_piht(apds_9190_i2c_client,g_piht);
#else
						apds9190_set_piht(apds_9190_i2c_client,apds_proxi_high_threshold);
#endif
						if(data->isNear != data->last_isNear)
						{
							apds9190_event_report(data->isNear);
							data->last_isNear = data->isNear;
						}
					}
					else if((data->isNear == 1) && (cdata >= (75*(1024*(256-data->atime)))/100) )
					{
						pdata = i2c_smbus_read_word_data(apds_9190_i2c_client, CMD_WORD|APDS9190_PDATAL_REG);													
						printk(KERN_INFO "%s, [APDS9190_STATUS_PINT_AINT] cdata : %d, pdata : %d\n", __func__, cdata, pdata);												
					}
					else if(cdata < (75*(1024*(256-data->atime)))/100) 
					{
						apds_9190_proximity_handler(data);
					}		
					else // Far state
					{
						pdata = i2c_smbus_read_word_data(apds_9190_i2c_client, CMD_WORD|APDS9190_PDATAL_REG);							
						printk(KERN_ERR "%s, [APDS9190_STATUS_PINT_AINT] Triggered by background ambient noise pdata : %d isNear : %d\n", __func__, pdata, data->isNear);		
					}


					apds9190_set_command(apds_9190_i2c_client, 2);

					data->enable = org_enable;

					apds9190_set_control(apds_9190_i2c_client, (data->pDrive | PDIODE | PGAIN | AGAIN));
					apds9190_set_enable(apds_9190_i2c_client,org_enable);

					enable_irq(data->irq);
				}
				else if((status & APDS9190_STATUS_PINT)  == 0x20) 
				{	
					disable_irq(data->irq);

					cdata = i2c_smbus_read_word_data(apds_9190_i2c_client, CMD_WORD|APDS9190_CDATAL_REG);
					printk(KERN_INFO "%s, [APDS9190_STATUS_PINT] status : %d,   cdata : %d, isNear : %d\n",__func__, status, cdata, data->isNear);		

					if((data->isNear == 0) && (cdata >= (75*(1024*(256-data->atime)))/100) ) // Near state light 75% 
					{
						pdata = i2c_smbus_read_word_data(apds_9190_i2c_client, CMD_WORD|APDS9190_PDATAL_REG);
						printk(KERN_INFO "%s, [APDS9190_STATUS_PINT] cdata : %d, pdata : %d\n", __func__, cdata, pdata);

						printk(KERN_INFO "%s, [APDS9190_STATUS_PINT] status change Near to Far while Near status but couldn't recognize Far\n", __func__);
						printk(KERN_INFO "%s, [APDS9190_STATUS_PINT] Force status to change Far\n",__func__);
						data->isNear = 1;
						data->last_isNear = 0;
						apds9190_set_pilt(apds_9190_i2c_client, 0);
#ifdef APDS9190_TUNE
						apds9190_set_piht(apds_9190_i2c_client,g_piht);
#else
						apds9190_set_piht(apds_9190_i2c_client,apds_proxi_high_threshold);
#endif
						if(data->isNear != data->last_isNear)
						{
							apds9190_event_report(data->isNear);
							data->last_isNear = data->isNear;
						}
					}
					else if((data->isNear == 1) && (cdata >= (75*(1024*(256-data->atime)))/100) )
					{
						pdata = i2c_smbus_read_word_data(apds_9190_i2c_client, CMD_WORD|APDS9190_PDATAL_REG);													
						printk(KERN_INFO "%s, [APDS9190_STATUS_PINT_AINT] cdata : %d, pdata : %d\n", __func__, cdata, pdata);												
					}
					else if(cdata < (75 * (1024 * (256 - data->atime))) / 100)
					{
						apds_9190_proximity_handler(data);
					}
					else // Far state
					{
						printk(KERN_ERR "%s, [APDS9190_STATUS_PINT] Triggered by background ambient noise pdata : %d isNear : %d\n", __func__, pdata, data->isNear);		
					}
					
					apds9190_set_command(apds_9190_i2c_client, 0);

					data->enable = org_enable;

					apds9190_set_control(apds_9190_i2c_client, (data->pDrive | PDIODE | PGAIN | AGAIN));
					apds9190_set_enable(apds_9190_i2c_client,org_enable);
					

					enable_irq(data->irq);
				}
				else if((status & APDS9190_STATUS_AINT) == 0x10) 
				{
					apds9190_set_command(apds_9190_i2c_client, 1);
				}
#else
				if((status & APDS9190_STATUS_PINT)  == 0x20){

					disable_irq(data->irq);
					
					apds_9190_proximity_handler(data);
					
					apds9190_set_command(apds_9190_i2c_client, 0);

					data->enable = org_enable;

					apds9190_set_control(apds_9190_i2c_client, (data->pDrive | PDIODE | PGAIN | AGAIN));
					apds9190_set_enable(apds_9190_i2c_client,org_enable);

					printk(KERN_INFO "%s, irq num : %d\n",__func__,data->irq);
					printk(KERN_INFO "%s, enable irq\n",__func__);

					enable_irq(data->irq);
				}
#endif
		}	
		else{
				mutex_lock(&data->update_lock);

				pdata = i2c_smbus_read_word_data(data->client, CMD_WORD|APDS9190_PDATAL_REG);

				i2c_smbus_write_byte(data->client, CMD_CLR_PS_INT);
				mutex_unlock(&data->update_lock);


				if(pdata > PROX_HIGH_TH) // near intr
						data->isNear = 0; 
				else
						data->isNear = 1; // far intr,  pdata<= PROX_INT_LOW_TH


				if(data->isNear != data->last_isNear)
						apds9190_event_report(data->isNear);

				data->enable = org_enable;

				apds9190_set_control(apds_9190_i2c_client, (data->pDrive | PDIODE | PGAIN | AGAIN));
				apds9190_set_enable(apds_9190_i2c_client,org_enable);

		}

}

static irqreturn_t apds_9190_irq_handler(int irq, void *dev_id)						   
{
		struct apds9190_data *data = dev_id;
		struct proximity_platform_data		*pdata;	
		unsigned long delay;
		int ret;

		pdata = data->client->dev.platform_data;
		spin_lock(&data->lock);

		delay = msecs_to_jiffies(0);
		ret = queue_work(proximity_wq, &data->dwork);
		if(ret < 0){
			printk(KERN_ERR "%s, queue_work Erro\n",__func__);
		}
		spin_unlock(&data->lock);
		return IRQ_HANDLED;
}	


/*
 * I2C init/probing/exit functions
 */

static struct i2c_driver apds9190_driver;
static int __devinit apds9190_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
		struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
		struct apds9190_data *data;
		struct proximity_platform_data		*pdata;
		pm_message_t dummy_state;
		int err = 0;

		if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
				err = -EIO;
				goto exit;
		}

		data = kzalloc(sizeof(struct apds9190_data), GFP_KERNEL);
		apds_9190_i2c_client = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
		if (!data) {
				err = -ENOMEM;
				goto exit;
		}

		memset(data, 0x00, sizeof(struct apds9190_data));

		INIT_WORK(&data->dwork, apds_9190_irq_work_func);		

		data->client = client;
		apds_9190_i2c_client = client;
		i2c_set_clientdata(data->client, data);

		data->input_dev = input_allocate_device();		

		data->input_dev->name = "proximity";
		data->input_dev->phys = "proximity/input2";	
		set_bit(EV_SYN, data->input_dev->evbit);
		set_bit(EV_ABS, data->input_dev->evbit);
		input_set_abs_params(data->input_dev, ABS_DISTANCE, 0, 1, 0, 0);

		err = input_register_device(data->input_dev);

		if (err) {
				DEBUG_MSG("Unable to register input device: %s\n",
								data->input_dev->name);
				goto exit_input_register_device_failed;
		}

		pdata = data->client->dev.platform_data;
		if(NULL == pdata){
				printk(KERN_INFO "platform data is NULL");
				return -1;
		}

		methods = pdata->methods;

		data->irq = gpio_to_irq(pdata->irq_num);

		spin_lock_init(&data->lock);
		mutex_init(&data->update_lock);

		data->enable = 0;	/* default mode is standard */
		dev_info(&client->dev, "enable = %s\n", data->enable ? "1" : "0");

		err = pdata->power(1);
		if(err < 0) {
			printk(KERN_INFO "%s,Proximity Power On Fail in Probe\n",__func__);
			goto exit_kfree;
		}
		
		mdelay(50);

		/* Initialize the APDS9190 chip */
		err = apds9190_init_client(apds_9190_i2c_client);
		if(err < 0) {
			printk(KERN_INFO "%s,Proximity apds9190_init_client Fail in Probe\n",__func__);
			goto exit_kfree;
		}
		
		err = apds_9190_initialize();	
		if(err < 0) {
			printk(KERN_INFO "%s,Proximity apds_9190_initialize Fail in Probe\n",__func__);
			goto exit_kfree;
		}
		
		if(request_irq(data->irq,apds_9190_irq_handler,IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,"proximity_irq", data) < 0){
				err = -EIO;
				goto exit_request_irq_failed;
		}
		err = set_irq_wake(data->irq, 1);
		if (err)
			set_irq_wake(data->irq, 0);

			
		data->sw_mode = PROX_STAT_OPERATING;

		/* Register sysfs hooks */
		err = sysfs_create_group(&client->dev.kobj, &apds9190_attr_group);
		if (err)
				goto exit_kfree;

		dummy_state.event = 0;
		apds9190_suspend(data->client, dummy_state);


		return 0;
exit_input_register_device_failed:	
exit_request_irq_failed:
exit_kfree:
		dev_info(&client->dev, "probe error\n");
		kfree(data);
exit:
		return err;
}

static int __devexit apds9190_remove(struct i2c_client *client)
{
		struct apds9190_data *data = i2c_get_clientdata(client);

		DEBUG_MSG("apds9190_remove\n");

		apds9190_set_enable(client, 0);

		set_irq_wake(data->irq, 0);
		free_irq(data->irq, NULL);
		input_unregister_device(data->input_dev);
		input_free_device(data->input_dev);

		kfree(data);		
		/* Power down the device */

		sysfs_remove_group(&client->dev.kobj, &apds9190_attr_group);

		return 0;
}

static int apds9190_suspend(struct i2c_client *client, pm_message_t mesg)
{
		struct apds9190_data *data = i2c_get_clientdata(apds_9190_i2c_client);	
		struct proximity_platform_data* pdata = NULL;
		int enable;
		int err;

		printk("apds9190_suspend [%d], proximity_wq=%d\n", data->enable, proximity_wq);

		if(!data->sw_mode)
				return 0;

		pdata = data->client->dev.platform_data;

		if(NULL == pdata){
				printk(KERN_INFO "Platform data is NULL\n");
				return -1;
		}

		apds9190_set_enable(client, 0);
		apds9190_set_command(apds_9190_i2c_client, 2);
		
		cancel_work_sync(&data->dwork);
		flush_work(&data->dwork);
		flush_workqueue(proximity_wq);

		enable_status = enable;
		data->sw_mode = PROX_STAT_SHUTDOWN;
        disable_irq(data->irq);
		err = pdata->power(0);
		if(err < 0) {
			printk(KERN_INFO "%s, Proximity Power Off Fail in susped\n",__func__);
			return err;
		}
			
		set_irq_wake(data->irq, 0);
		if(NULL != proximity_wq){
			destroy_workqueue(proximity_wq);
			printk(KERN_INFO "%s, Destroy workqueue\n",__func__);
			proximity_wq = NULL;
		}
		return 0;
}

static int apds9190_setting()
{
#ifdef CONFIG_LGE_PCB_VERSION
	if (!strncmp(hw_pcb_version, "Unknown", 7)) 
	{
		printk(KERN_INFO "%s, Get PCB Version using rpc\n", __func__);
		lge_set_hw_version_string((char *)hw_pcb_version);
		printk(KERN_INFO "%s, PCB Ver. : %s\n",__func__, hw_pcb_version);
	}

	if(!strncmp(hw_pcb_version, "B", 1)) 
	{
		apds_proxi_high_threshold = REV_B_APDS_PROXIMITY_HIGH_THRESHHOLD;
		apds_proxi_low_threshold = REV_B_APDS_PROXIMITY_LOW_THRESHHOLD;			

		apds_ppcount = REV_B_PPCOUNT;
		apds_pdrive = REV_B_PDRIVE;
	}
	else if(!strncmp(hw_pcb_version, "C", 1))
	{
		apds_proxi_high_threshold = REV_C_APDS_PROXIMITY_HIGH_THRESHHOLD;
		apds_proxi_low_threshold = REV_C_APDS_PROXIMITY_LOW_THRESHHOLD;			

		apds_ppcount = REV_C_PPCOUNT;
		apds_pdrive = REV_C_PDRIVE;
	}
	else if(!strncmp(hw_pcb_version, "D", 1) || !strncmp(hw_pcb_version, "E", 1)|| (!strncmp(hw_pcb_version, "1.0", 3)))
	{
		apds_proxi_high_threshold = REV_D_APDS_PROXIMITY_HIGH_THRESHHOLD;
		apds_proxi_low_threshold = REV_D_APDS_PROXIMITY_LOW_THRESHHOLD;						

		apds_ppcount = REV_D_PPCOUNT;
		apds_pdrive = REV_D_PDRIVE;
	}
	else
	{
		printk(KERN_INFO "%s, Init Threshold, Pulse Count, LED Power Fail\n",__func__);
		apds_proxi_high_threshold = DEFAULT_APDS_PROXIMITY_HIGH_THRESHHOLD;
		apds_proxi_low_threshold = DEFAULT_APDS_PROXIMITY_LOW_THRESHHOLD;

		apds_ppcount = DEFAULT_PPCOUNT;
		apds_pdrive = DEFAULT_PDRIVE;
		return 0;
	}
	return 1;
#else	
	printk(KERN_INFO "%s, Non PCB Rev Read\n",__func__);
	apds_proxi_high_threshold = DEFAULT_APDS_PROXIMITY_HIGH_THRESHHOLD;
	apds_proxi_low_threshold = DEFAULT_APDS_PROXIMITY_LOW_THRESHHOLD;

	apds_ppcount = DEFAULT_PPCOUNT;
	apds_pdrive = DEFAULT_PDRIVE;
	
	return -1;
#endif
}
static int apds9190_resume(struct i2c_client *client)
{
		struct apds9190_data *data = i2c_get_clientdata(apds_9190_i2c_client);	
		struct proximity_platform_data* pdata = NULL;
		int ret;

		if(proximity_wq == NULL) {
			proximity_wq = create_singlethread_workqueue("proximity_wq");
			if(NULL == proximity_wq)
				return -ENOMEM;
		}
		
		pdata = data->client->dev.platform_data;

		if(NULL == pdata){
				printk(KERN_INFO "Platform data is NULL");
				return -1;
		}

		printk("apds9190_resume [%d]\n",enable_status);
		if(data->sw_mode)
				return 0;

		enable_irq(data->irq);
		ret = pdata->power(1);
		if(ret < 0) {
			printk(KERN_INFO "%s,Proximity Power On Fail in Resume\n",__func__);
			return ret;
		}
			
		mdelay(50);

		ret = apds9190_setting();
		
		ret = apds_9190_initialize();
		if(ret < 0) {
			printk(KERN_INFO "%s,Proximity apds_9190_initialize Fail in Resume\n",__func__);
			return ret;
		}

		data->last_isNear = -1;

		data->sw_mode = PROX_STAT_OPERATING;

		ret = set_irq_wake(data->irq, 1);
		if(ret)
			set_irq_wake(data->irq, 0);
	
		apds9190_set_command(apds_9190_i2c_client, 2);

		return 0;
}


static const struct i2c_device_id apds9190_id[] = {
		{ "proximity_apds9190", 0 },
		{ }
};
MODULE_DEVICE_TABLE(i2c, apds9190_id);

static struct i2c_driver apds9190_driver = {
		.driver = {
				.name	= APDS9190_DRV_NAME,
				.owner	= THIS_MODULE,
		},
		.probe	= apds9190_probe,
		.remove	= __devexit_p(apds9190_remove),
		.id_table = apds9190_id,
};

static int __init apds9190_init(void)
{
		int err;
		if(proximity_wq == NULL) {
			proximity_wq = create_singlethread_workqueue("proximity_wq");
				if(NULL == proximity_wq)
					return -ENOMEM;
			err = i2c_add_driver(&apds9190_driver);
			if(err < 0){
				printk(KERN_INFO "Failed to i2c_add_driver \n");
				return err;
			}
		}
		return 0;
}

static void __exit apds9190_exit(void)
{
		i2c_del_driver(&apds9190_driver);
		if(proximity_wq != NULL) {
			if(proximity_wq)
				destroy_workqueue(proximity_wq);		
		}
}

MODULE_AUTHOR("Lee Kai Koon <kai-koon.lee@avagotech.com>");
MODULE_DESCRIPTION("APDS9190 ambient proximity sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(apds9190_init);
module_exit(apds9190_exit);


