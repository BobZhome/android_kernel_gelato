/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Sony 3M ISX005 camera sensor driver
 * Auther: Lee Hyung Tae[hyungtae.lee@lge.com], 2010-04-09
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

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>

// LGE_CHANGE_S 2011.01.26 [jongkwon.chae@lge.com] [gelato] sensor porting
#include <linux/slab.h>
// LGE_CHANGE_E

#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <linux/kthread.h>

#include "hi351_reg.h"

#if (READ_INIT_DATA_FROM_FILE == 1) 
#include <linux/syscalls.h>

#include "hi351_tun.h"
#else
#include "hi351.h"
#endif


#define ISX005_INTERVAL_T2		8	/* 8ms */
#define ISX005_INTERVAL_T3		1	/* 0.5ms */
#define ISX005_INTERVAL_T4		2	/* 15ms */
#define ISX005_INTERVAL_T5		25	/* 200ms */

/*
* AF Total steps parameters
*/

//[2011.01.13][JKCHAE] Sensor Porting
#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	#define ISX005_TOTAL_STEPS_NEAR_TO_FAR	12 //30
#else //SAMSUNG CAMERA SENSOR
	#define ISX005_TOTAL_STEPS_NEAR_TO_FAR	10 //17 //30
#endif

/*  ISX005 Registers  */
#define REG_ISX005_INTSTS_ID			0x00F8	/* Interrupt status */
#define REG_ISX005_INTCLR_ID			0x00FC	/* Interrupt clear */

#define ISX005_OM_CHANGED				0x0001	/* Operating mode */
#define ISX005_CM_CHANGED				0x0002	/* Camera mode */

//LGE_CHANGE_S[byungsik.choi@lge.com]2010-07-23 6020 patch
DEFINE_MUTEX(isx005_tuning_mutex);
static int tuning_thread_run;

#define CFG_WQ_SIZE		64

struct config_work_queue {
	int cfgtype;
	int mode;
};

struct config_work_queue *cfg_wq;
static int cfg_wq_num;
//LGE_CHANGE_E[byungsik.choi@lge.com]


/* It is distinguish normal from macro focus */
static int prev_af_mode;
/* It is distinguish scene mode */
static int prev_scene_mode;

//LGE_CHANGE_S CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)
#if (USE_I2C_BURSTMODE == 1)
	static int init_burst_mode = 0;
	static unsigned char* sensor_burst_buffer = 0;
	#define sensor_burst_size 1000
#endif
//LGE_CHANGE_E CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)

//Hynix sensor related modification
static short current_sensor = 0x00; //Default: PV1
static int current_effect = CAMERA_EFFECT_OFF;
static int isx005_set_effect(int effect);
static void isx005_read_init_register_from_file();
static void isx005_read_preview_register_from_file();
static void isx005_read_snapshot_register_from_file();
//

struct isx005_work {
	struct work_struct work;
};
static struct isx005_work *isx005_sensorw;

static struct i2c_client *isx005_client;

struct isx005_ctrl {
	const struct msm_camera_sensor_info *sensordata;
};

static struct isx005_ctrl *isx005_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(isx005_wait_queue);
/* LGE_CHANGE_S [youngki.an@lge.com] 2010-05-18 */
/*=============================================================
	EXTERNAL DECLARATIONS
==============================================================*/
#if 1//def LG_CAMERA_HIDDEN_MENU
extern bool sensorAlwaysOnTest;
#endif
/* LGE_CHANGE_E [youngki.an@lge.com] 2010-05-18 */

DEFINE_MUTEX(isx005_mutex);

struct platform_device *isx005_pdev;

extern int mclk_rate;
static int always_on = 0;

static int32_t isx005_i2c_txdata(unsigned short saddr,
	unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
//#if ((CAMERA_SENSOR == SAMSUNG_CAMERA_SENSOR) && (USE_BURSTMODE == 1))
//			.flags = isx005_client->flags & I2C_M_TEN,
//#else
			.flags = 0,
//#endif
			.len = length,
			.buf = txdata,
		},
	};

	if (i2c_transfer(isx005_client->adapter, msg, 1) < 0) {
		printk(KERN_ERR "isx005_i2c_txdata failed\n");
		return -EIO;
	}

	return 0;
}

//[2011.01.13][JKCHAE] Sensor Porting
#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	static int32_t isx005_i2c_write(unsigned short saddr,
		unsigned short waddr, unsigned short wdata, enum isx005_width width)
	{
		int32_t rc = -EIO;
		unsigned char buf[4];

		memset(buf, 0, sizeof(buf));
		switch (width) {
			case BYTE_LEN:
				buf[0] = (waddr & 0x00FF);
				buf[1] = wdata;
				rc = isx005_i2c_txdata(saddr, buf, 2);				
				break;
				
			case WORD_LEN:
				buf[0] = (waddr & 0xFF00) >> 8;
				buf[1] = (waddr & 0x00FF);
				buf[2] = (wdata & 0xFF00) >> 8;
				buf[3] = (wdata & 0x00FF);
				rc = isx005_i2c_txdata(saddr, buf, 4);
				break;

			default:
				break;
		}

		if (rc < 0) {
			
			printk(KERN_ERR "[smiledice] %s FAILED (saddr:0x%x, waddr:0x%x, wdata:0x%x, width:%d) \n", 
				__func__, saddr, waddr, wdata, width);
			printk(KERN_ERR "i2c_write failed, addr = 0x%x, val = 0x%x!\n", waddr, wdata);
		}

		return rc;
	}
#else //SAMSUNG CAMERA SENSOR
	#if ((CAMERA_SENSOR == SAMSUNG_CAMERA_SENSOR) && (USE_BURSTMODE == 1))
	#define MAX_BURST_DATA_LEN  3000 //about 1500 lines
	static unsigned char burstBuf[MAX_BURST_DATA_LEN];
	static unsigned int bufIndex = 0;

	static int32_t isx005_i2c_write(unsigned short saddr,
	unsigned short waddr, unsigned short wdata, enum isx005_width width)
	{
		int32_t rc = -EIO;

		memset(burstBuf, 0, sizeof(burstBuf));
		switch (width) {
			case BYTE_LEN:	
				burstBuf[0] = (waddr & 0xFF00) >> 8;
				burstBuf[1] = (waddr & 0x00FF);
				burstBuf[2] = wdata;
				rc = isx005_i2c_txdata(saddr, burstBuf, 3);			
				break;
				
			case WORD_LEN:
				if (bufIndex > 0) {
					printk(KERN_ERR "[CHECK] Burst Write (burst data count: %d, include Addr)\n", bufIndex);

					rc = isx005_i2c_txdata(saddr, burstBuf, bufIndex);
					if (rc < 0) {
						printk(KERN_ERR "[CHECK] Burst Write FAILED!\n");
					}
					bufIndex = 0; //Make Init State

					//CHECK IF WRITTEN WELL

				}

				burstBuf[0] = (waddr & 0xFF00) >> 8;
				burstBuf[1] = (waddr & 0x00FF);
				burstBuf[2] = (wdata & 0xFF00) >> 8;
				burstBuf[3] = (wdata & 0x00FF);
				rc = isx005_i2c_txdata(saddr, burstBuf, 4);
				break;

			case BURST_LEN:
				//Address
				if (bufIndex == 0) {
					burstBuf[bufIndex++] = (waddr & 0xFF00) >> 8;
					burstBuf[bufIndex++] = (waddr & 0x00FF);
				}
				//Data
				burstBuf[bufIndex++] = (wdata & 0xFF00) >> 8;
				burstBuf[bufIndex++] = (wdata & 0x00FF);
				//rc = isx005_i2c_txdata(saddr, burstBuf, 3);
				rc = 0;
				break;

			default:
				break;
		}

		if (rc < 0) {
			
			printk(KERN_ERR "[smiledice] %s FAILED (saddr:0x%x, waddr:0x%x, wdata:0x%x, width:%d) \n", 
				__func__, saddr, waddr, wdata, width);
			printk(KERN_ERR "i2c_write failed, addr = 0x%x, val = 0x%x!\n", waddr, wdata);
		}

		return rc;
	}
	#else
	static int32_t isx005_i2c_write(unsigned short saddr,
	unsigned short waddr, unsigned short wdata, enum isx005_width width)
	{
		int32_t rc = 0; //-EIO;
		unsigned char buf[4];
#if (USE_I2C_BURSTMODE == 1)
		static int burst_num = 0;
		int32_t index_burst_buffer = 0;
#endif

		memset(buf, 0, sizeof(buf));
		switch (width) {
			case BYTE_LEN:	
				buf[0] = (waddr & 0xFF00) >> 8;
				buf[1] = (waddr & 0x00FF);
				buf[2] = wdata;
				rc = isx005_i2c_txdata(saddr, buf, 3);			
				break;
				
#if (USE_I2C_BURSTMODE == 1)
			case WORD_LEN:
				if(init_burst_mode)
				{
					if(waddr == 0x0F12)
					{
						if(burst_num == 0)
						{
							memset(sensor_burst_buffer, 0, sizeof(sensor_burst_buffer));
							sensor_burst_buffer[burst_num++] = 0x0F;
							sensor_burst_buffer[burst_num++] = 0x12;
						}
						sensor_burst_buffer[burst_num++] = (wdata & 0xFF00)>>8;
						sensor_burst_buffer[burst_num++] = (wdata & 0x00FF);
					}
					else
					{
						if(burst_num > 0)
						{
							rc = isx005_i2c_txdata(saddr, (unsigned char*)sensor_burst_buffer, burst_num);
							if (rc < 0) {
								printk(KERN_ERR "[smiledice] %s FAILED (saddr: %d, sensor_burst_buffer:%d, burst_num:%d) \n", 
										__func__, saddr, (int)sensor_burst_buffer, burst_num);
							
#if 0
								printk(KERN_ERR "[smiledice] ===== sensor_burst_buffer =====\n");
								while (index_burst_buffer < burst_num)
								{
									printk(KERN_ERR "[smiledice] 0x%02x 0x%02x \n", 
											sensor_burst_buffer[index_burst_buffer],
											sensor_burst_buffer[index_burst_buffer+1]);
									index_burst_buffer += 2;
								}
								printk(KERN_ERR "[smiledice] ===== sensor_burst_buffer =====\n");
#endif
							}
						}

						buf[0] = (waddr & 0xFF00)>>8;
						buf[1] = (waddr & 0x00FF);
						buf[2] = (wdata & 0xFF00)>>8;
						buf[3] = (wdata & 0x00FF);
						rc = isx005_i2c_txdata(saddr, buf, 4);
						burst_num = 0;
					}
				}
				else
				{
					buf[0] = (waddr & 0xFF00)>>8;
					buf[1] = (waddr & 0x00FF);
					buf[2] = (wdata & 0xFF00)>>8;
					buf[3] = (wdata & 0x00FF);
					rc = isx005_i2c_txdata(saddr, buf, 4);
					burst_num = 0;
				}
				break;
#else
			case WORD_LEN:
				buf[0] = (waddr & 0xFF00) >> 8;
				buf[1] = (waddr & 0x00FF);
				buf[2] = (wdata & 0xFF00) >> 8;
				buf[3] = (wdata & 0x00FF);
				rc = isx005_i2c_txdata(saddr, buf, 4);
				break;
#endif

			default:
				break;
		}

		if (rc < 0) {
			
			printk(KERN_ERR "[smiledice] %s FAILED (saddr:0x%x, waddr:0x%x, wdata:0x%x, width:%d) \n", 
				__func__, saddr, waddr, wdata, width);
			printk(KERN_ERR "i2c_write failed, addr = 0x%x, val = 0x%x!\n", waddr, wdata);
		}

		return rc;
	}
	#endif
#endif


static int32_t isx005_i2c_write_table(
	struct isx005_register_address_value_pair const *reg_conf_tbl,
	int num_of_items_in_table)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int i;
	int32_t rc = -EIO;

	for (i = 0; i < num_of_items_in_table; ++i) {
		rc = isx005_i2c_write(isx005_client->addr,
			reg_conf_tbl->register_address, 
			reg_conf_tbl->register_value,
			reg_conf_tbl->register_length);
		if (rc < 0)
			break;
		
#if 0
		printk(KERN_ERR "[smiledice] reg[%d] {addr:0x%x, data:0x%x, width:%d} \n", 
			i, 
			reg_conf_tbl->register_address, 
			reg_conf_tbl->register_value,
			reg_conf_tbl->register_length);
#endif

		reg_conf_tbl++;
	}
	

	printk(KERN_ERR "[smiledice] << %s END \n", __func__);

	return rc;
}

//[2010.12.30][JKCHAE] sensor porting (for the sensor-specific-delay)
struct DelayItem{
	int delayStepAfter;
	int delayTimeMS;
};

static int32_t isx005_i2c_write_table_with_delay(
	struct isx005_register_address_value_pair const *reg_conf_tbl,
	int num_of_items_in_table,
	struct DelayItem delayArray[],
	int delayArrayCount)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int i;
	int32_t rc = -EIO;

	int j = 0;

	for (i = 0; i < num_of_items_in_table; ++i) {
		rc = isx005_i2c_write(isx005_client->addr,
			reg_conf_tbl->register_address, 
			reg_conf_tbl->register_value,
			reg_conf_tbl->register_length);
		if (rc < 0)
			break;

#if 0
		printk(KERN_ERR "[smiledice] reg[%d] {addr:0x%x, data:0x%x, width:%d} \n", 
			i, 
			reg_conf_tbl->register_address, 
			reg_conf_tbl->register_value,
			reg_conf_tbl->register_length);
#endif

		if ((j < delayArrayCount) && (i == delayArray[j].delayStepAfter-1))
		{
			
			printk(KERN_ERR "[smiledice] %s delay: %d ms (after %d-th write) \n", 
			__func__, delayArray[j].delayTimeMS, i+1);
			msleep (delayArray[j].delayTimeMS);
			j++;
		}
			
		
		reg_conf_tbl++;
	}

	printk(KERN_ERR "[smiledice] << %s END \n", __func__);

	return rc;
}

//[2011.01.13][JKCHAE] Sensor Porting
#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	static int isx005_i2c_rxdata(unsigned short saddr,
		unsigned char *rxdata, int length)
	{
		struct i2c_msg msgs[] = {
			{
				.addr   = saddr,
				.flags = 0,
				.len   = 1,
	 			.buf   = rxdata,
			},
			{
				.addr   = saddr,
				.flags = I2C_M_RD,
				.len   = length,
				.buf   = rxdata,
			},
		};

		if (i2c_transfer(isx005_client->adapter, msgs, 2) < 0) {
			printk(KERN_ERR "isx005_i2c_rxdata failed!\n");
			return -EIO;
		}
		printk(KERN_ERR "[yt_test] isx005_i2c_rxdata sucess!\n");

		return 0;
	}
#else //SAMSUNG CAMERA SENSOR
	static int isx005_i2c_rxdata(unsigned short saddr,
		unsigned char *rxdata, int length)
	{
		struct i2c_msg msgs[] = {
			{
				.addr	= saddr,
				.flags = 0,
				.len   = 2,
				.buf   = rxdata,
			},
			{
				.addr	= saddr,
				.flags = I2C_M_RD,
				.len   = length,
				.buf   = rxdata,
			},
		};

		if (i2c_transfer(isx005_client->adapter, msgs, 2) < 0) {
			printk(KERN_ERR "isx005_i2c_rxdata failed!\n");
			return -EIO;
		}
		printk(KERN_ERR "[yt_test] isx005_i2c_rxdata sucess!\n");

		return 0;
	}

#endif


//[2011.01.13][JKCHAE] Sensor Porting
#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	static int32_t isx005_i2c_read(unsigned short   saddr,
		unsigned short raddr, unsigned short *rdata, enum isx005_width width)
	{
		int32_t rc = 0;
		unsigned char buf[4];

		if (!rdata)
			return -EIO;

		memset(buf, 0, sizeof(buf));

		switch (width) {
			case BYTE_LEN:
				buf[0] = (raddr & 0x00FF);
				rc = isx005_i2c_rxdata(saddr, buf, 1);
				if (rc < 0)
					return rc;
				*rdata = buf[0];
				break;
				
			case WORD_LEN:
				buf[0] = (raddr & 0xFF00) >> 8;
				buf[1] = (raddr & 0x00FF);			 
	 			rc = isx005_i2c_rxdata(saddr, buf, 2);
				if (rc < 0)
					return rc;
				*rdata = buf[0] << 8 | buf[1];
	 			break;

			default:
				break;
		}

		if (rc < 0)
			printk(KERN_ERR "isx005_i2c_read failed!\n");

		return rc;
	}
#else //SAMSUNG CAMERA SENSOR
	static int32_t isx005_i2c_read(unsigned short   saddr,
		unsigned short raddr, unsigned short *rdata, enum isx005_width width)
	{
		int32_t rc = 0;
		unsigned char buf[4];

		if (!rdata)
			return -EIO;

		memset(buf, 0, sizeof(buf));

		switch (width) {
			case BYTE_LEN:
				buf[0] = (raddr & 0xFF00) >> 8;
				buf[1] = (raddr & 0x00FF);
				rc = isx005_i2c_rxdata(saddr, buf, 2);
				if (rc < 0)
					return rc;
				*rdata = buf[0];
				break;
				
			case WORD_LEN:
				buf[0] = (raddr & 0xFF00) >> 8;
				buf[1] = (raddr & 0x00FF);			 
	 			rc = isx005_i2c_rxdata(saddr, buf, 2);
				if (rc < 0)
					return rc;
				*rdata = buf[0] << 8 | buf[1];
	 			break;

			default:
				break;
		}

		if (rc < 0)
			printk(KERN_ERR "isx005_i2c_read failed!\n");

		return rc;
	}

#endif

//[2011.01.13][JKCHAE] Sensor Porting (AF)
#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	static int32_t isx005_i2c_page_read(
									unsigned short saddr,    // [IN]  Slave Addr.
									unsigned short rpage,    // [IN]  Page#
									unsigned short raddr,    // [IN]  Addr
									unsigned short *rdata,   // [OUT] Read Data
									enum isx005_width width) // [IN]  Data Width
	{
		//printk(KERN_ERR "[smiledice] >> %s START (saddr: 0x%02x, page: 0x%02x, addr: 0x%02x, width: %d)\n", 
		//	__func__, saddr, rpage, raddr, width);
		
		//1. Move to the Requested Page
		int rc = isx005_i2c_write(saddr, 0x03, rpage, width);

		if (rc < 0) {
			printk(KERN_ERR "[smiledice] %s: Change 0x%02x Page FAIL (rc: %d)\n", __func__, rpage, rc);
			//printk(KERN_ERR "[smiledice] << %s END : Change 0x%02x Page FAIL (rc: %d)\n", 
			//__func__, rpage, rc);
			return rc;
		}
		else {
			//printk(KERN_ERR "[smiledice] Change Page(0x%02x) Succeed\n", rpage);
		}

		
		//2. Read Register
		rc = isx005_i2c_read(saddr,	raddr, rdata, BYTE_LEN);
		if (rc < 0) {
			printk(KERN_ERR "[smiledice] %s : [Page 0x%02x] 0x%02x Read FAIL (rc: %d)\n", 
				__func__, rpage, raddr, rc);
			//printk(KERN_ERR "[smiledice] << %s END : [Page 0x%02x] 0x%02x Read FAIL (rc: %d)\n", 
			//	__func__, rpage, raddr, rc);
		}
		else {
			printk(KERN_ERR "[smiledice] [Page 0x%02x / Addr 0x%02x] Read SUCCEED (Read Value: 0x%02x)\n", 
				rpage, raddr, *rdata);
			//printk(KERN_ERR "[smiledice] << %s END : [Page 0x%02x] 0x%02x Read SUCCEED (Read Value: 0x%02x) (rc: %d)\n", 
			//	__func__, rpage, raddr, *rdata, rc);
		}
		
		return rc;
	}
#else //SAMSUNG CAMERA SENSOR
	  //(N/A, Currently)
	  //I2C_SLAVE_ADDR_FOR_READ_SAMSUNG_SENSOR
static int32_t isx005_i2c_page_read(
								unsigned short saddr,	 // [IN]  Slave Addr.
								unsigned short rpage,	 // [IN]  Page#
								unsigned short raddr,	 // [IN]  Addr
								unsigned short *rdata,	 // [OUT] Read Data
								enum isx005_width width) // [IN]  Data Width
{
	//printk(KERN_ERR "[smiledice] >> %s START (saddr: 0x%02x, page: 0x%02x, addr: 0x%02x, width: %d)\n", 
	//	__func__, saddr, rpage, raddr, width);
	
	//1.1 Move to the Requested Page (Set Page)
	int rc = isx005_i2c_write(saddr, 0x002C, 0x7000, width);

	if (rc < 0) {
		printk(KERN_ERR "[smiledice] %s: Change 0x%04x Page FAIL (rc: %d)\n", __func__, rpage, rc);
		//printk(KERN_ERR "[smiledice] << %s END : Change 0x%04x Page FAIL (rc: %d)\n", 
		//__func__, rpage, rc);
		return rc;
	}
	else {
		//printk(KERN_ERR "[smiledice] Change Page(0x%04x) Succeed\n", rpage);
	}

	//1.2 Move to the Requested Page (Add Page)
	rc = isx005_i2c_write(saddr, 0x002E, rpage, width);

	if (rc < 0) {
		printk(KERN_ERR "[smiledice] %s: Change 0x%04x Page FAIL (rc: %d)\n", __func__, rpage, rc);
		//printk(KERN_ERR "[smiledice] << %s END : Change 0x%04x Page FAIL (rc: %d)\n", 
		//__func__, rpage, rc);
		return rc;
	}
	else {
		//printk(KERN_ERR "[smiledice] Change Page(0x%04x) Succeed\n", rpage);
	}
	
	//2. Read Register
	rc = isx005_i2c_read(I2C_SLAVE_ADDR_FOR_READ_SAMSUNG_SENSOR, 
						raddr, rdata, WORD_LEN);
	if (rc < 0) {
		printk(KERN_ERR "[smiledice] %s : [Page 0x%04x] 0x%04x Read FAIL (rc: %d)\n", 
			__func__, rpage, raddr, rc);
		//printk(KERN_ERR "[smiledice] << %s END : [Page 0x%04x] 0x%04x Read FAIL (rc: %d)\n", 
		//	__func__, rpage, raddr, rc);
	}
	else {
		printk(KERN_ERR "[smiledice] [Page 0x%02x / Addr 0x%04x] Read SUCCEED (Read Value: 0x%02x)\n", 
			rpage, raddr, *rdata);
		//printk(KERN_ERR "[smiledice] << %s END : [Page 0x%04x] 0x%04x Read SUCCEED (Read Value: 0x%04x) (rc: %d)\n", 
		//	__func__, rpage, raddr, *rdata, rc);
	}
	
	return rc;
}

#endif
//


//[2011.01.13][JKCHAE] Sensor Porting (AF)
#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	static int isx005_reg_init(void)
	{
		printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
		printk(KERN_ERR "[smiledice]    (SLAVE_ADDR) isx005_client->addr: 0x%x\n", isx005_client->addr);

		int rc = 0;
		int i;

		/* Configure sensor for Initial setting (PLL, Clock, etc) */
		for (i = 0; i < isx005_regs.init_reg_settings_size; ++i) {
			rc = isx005_i2c_write(isx005_client->addr,
				isx005_regs.init_reg_settings[i].register_address,
				isx005_regs.init_reg_settings[i].register_value,
				isx005_regs.init_reg_settings[i].register_length);

			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
				return rc;
			}


		}

		printk(KERN_ERR "[smiledice] << [CHECK] Total Reg Count: %d\n", i);
		printk(KERN_ERR "[smiledice] << %s END : OK\n", __func__);
		return rc;
	}
#else //SAMSUNG CAMERA SENSOR	
	static int isx005_reg_init(void)
	{
		printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
		printk(KERN_ERR "[smiledice]    (SLAVE_ADDR) isx005_client->addr: 0x%x\n", isx005_client->addr);

		int rc = 0;
		int i;

		/* Configure sensor for Initial setting (PLL, Clock, etc) */
		for (i = 0; i < isx005_regs.init_reg_settings_size; ++i) {
			rc = isx005_i2c_write(isx005_client->addr,
				isx005_regs.init_reg_settings[i].register_address,
				isx005_regs.init_reg_settings[i].register_value,
				isx005_regs.init_reg_settings[i].register_length);

			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
				return rc;
			}

			#if 1 //[2010.12.27][JKCHAE] sensor porting (samsung sensor specific.)
			if (i == 3) {   // after 4th i2c write
				mdelay(12); // p10 //Min.10ms delay is required
			}
			#endif

#if 0
			printk(KERN_ERR "[smiledice] reg[%d] {addr:0x%x, data:0x%x, width:%d} \n", 
							i, 
							isx005_regs.init_reg_settings[i].register_address, 
							isx005_regs.init_reg_settings[i].register_value,
							isx005_regs.init_reg_settings[i].register_length);
#endif
		}
		
		printk(KERN_ERR "[smiledice] << [CHECK] Total Reg Count: %d\n", i);
		printk(KERN_ERR "[smiledice] << %s END : OK\n", __func__);
		return rc;
	}
#endif
	
//LGE_CHANGE_S[byungsik.choi@lge.com]2010-07-23 6020 patch
static int dequeue_sensor_config(int cfgtype, int mode);

static void dequeue_cfg_wq(struct config_work_queue *cfg_wq)
{
	int rc;
	int i;

	for (i = 0; i < cfg_wq_num; ++i) {
		rc = dequeue_sensor_config(cfg_wq[i].cfgtype, cfg_wq[i].mode);
		if (rc < 0) {
			printk(KERN_ERR "[ERROR]%s: dequeue sensor config error!\n",
				__func__);
			return;
		}
	}

	cfg_wq_num = 0;
}

static void enqueue_cfg_wq(int cfgtype, int mode)
{
	if(cfg_wq_num == CFG_WQ_SIZE)
		return;

	cfg_wq[cfg_wq_num].cfgtype = cfgtype;
	cfg_wq[cfg_wq_num].mode= mode;

	++cfg_wq_num;
}
//LGE_CHANGE_E[byungsik.choi@lge.com]

int isx005_reg_tuning(void *data)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int rc = 0;
	int i;

	mutex_lock(&isx005_tuning_mutex);
	cfg_wq = kmalloc(sizeof(struct config_work_queue) * CFG_WQ_SIZE,
		GFP_KERNEL);
	cfg_wq_num = 0;
	tuning_thread_run = 1;
	mutex_unlock(&isx005_tuning_mutex);

	#if (TUNING_THREAD_ENABLE == 1)
	/* Configure sensor for various tuning */
	for (i = 0; i < isx005_regs.tuning_reg_settings_size; ++i) {
		rc = isx005_i2c_write(isx005_client->addr,
			isx005_regs.tuning_reg_settings[i].register_address,
			isx005_regs.tuning_reg_settings[i].register_value,
			isx005_regs.tuning_reg_settings[i].register_length);

		if (rc < 0) {			
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}
	}
	#endif

	mutex_lock(&isx005_tuning_mutex);
	dequeue_cfg_wq(cfg_wq);
	kfree(cfg_wq);
	tuning_thread_run = 0;
	mutex_unlock(&isx005_tuning_mutex);
//LGE_CHANGE_E[byungsik.choi@lge.com]

	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
	return rc;
}

static int isx005_reg_preview(void)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int rc = 0;
	int i;

	#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
		#if 0
		//#if (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B)
			if (current_sensor == 0x00) { //PV1
				printk(KERN_ERR "[smiledice] [CHECK] PV1 preview routine\n");
				/* Configure sensor for Preview mode */
				for (i = 0; i < isx005_regs.prev_reg_settings_size; ++i) {
					rc = isx005_i2c_write(isx005_client->addr,
							isx005_regs.prev_reg_settings[i].register_address,
							isx005_regs.prev_reg_settings[i].register_value,
							isx005_regs.prev_reg_settings[i].register_length);

					if (rc < 0){			
						printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
						return rc;
					}
				}
			}
			else if (current_sensor == 0x01) { //PV2
				printk(KERN_ERR "[smiledice] [CHECK] PV2 preview routine\n");
				/* Configure sensor for Preview mode */
				for (i = 0; i < preview_mode_reg_pv2_size; ++i) {
					rc = isx005_i2c_write(isx005_client->addr,
							preview_mode_reg_pv2[i].register_address,
							preview_mode_reg_pv2[i].register_value,
							preview_mode_reg_pv2[i].register_length);

					if (rc < 0){			
						printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
						return rc;
					}
				}
			}
			else {
				printk(KERN_ERR "[smiledice] [CHECK] NOT SUPPORTED SENSOR INDEX return -1\n");
				printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
				return -1;
			}
		#else //HYNIX (REV.A or ELSE)
			/* Configure sensor for Preview mode */
			for (i = 0; i < isx005_regs.prev_reg_settings_size; ++i) {
				rc = isx005_i2c_write(isx005_client->addr,
				  isx005_regs.prev_reg_settings[i].register_address,
				  isx005_regs.prev_reg_settings[i].register_value,
				  isx005_regs.prev_reg_settings[i].register_length);

				if (rc < 0){			
					printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
					return rc;
				}
			}
		#endif
	#else //SAMSUNG or Other SENSOR
		/* Configure sensor for Preview mode */
		for (i = 0; i < isx005_regs.prev_reg_settings_size; ++i) {
			rc = isx005_i2c_write(isx005_client->addr,
			  isx005_regs.prev_reg_settings[i].register_address,
			  isx005_regs.prev_reg_settings[i].register_value,
			  isx005_regs.prev_reg_settings[i].register_length);

			if (rc < 0){			
				printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
				return rc;
			}
		}
	#endif
	
	msleep(150); //[2010.12.24][JKCHAE] mode change confirm delay
		

	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
	return rc;
}

static int isx005_reg_snapshot(void)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int rc = 0;
	int i;
    unsigned short cm_changed_status= 0;

	#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
		#if 0
		//#if (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B)
		if (current_sensor == 0x00) { //PV1
			printk(KERN_ERR "[smiledice] [CHECK] PV1 snapshot routine\n");
			/* Configure sensor for Snapshot mode */
			for (i = 0; i < isx005_regs.snap_reg_settings_size; ++i) {
				rc = isx005_i2c_write(isx005_client->addr,
					isx005_regs.snap_reg_settings[i].register_address,
					isx005_regs.snap_reg_settings[i].register_value,
					isx005_regs.snap_reg_settings[i].register_length);

				if (rc < 0){			
					printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
					return rc;
				}
			}

			rc = isx005_set_effect(current_effect);
			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2\n", __func__);
				return rc;
			}
		}
		else if (current_sensor == 0x01) { //PV2
			printk(KERN_ERR "[smiledice] [CHECK] PV2 snapshot routine\n");
			/* Configure sensor for Snapshot mode */
			for (i = 0; i < snapshot_mode_reg_pv2_size; ++i) {
				rc = isx005_i2c_write(isx005_client->addr,
					snapshot_mode_reg_pv2[i].register_address,
					snapshot_mode_reg_pv2[i].register_value,
					snapshot_mode_reg_pv2[i].register_length);

				if (rc < 0){			
					printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
					return rc;
				}
			}

			rc = isx005_set_effect(current_effect);
			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2\n", __func__);
				return rc;
			}
		}
		else {
			printk(KERN_ERR "[smiledice] [CHECK] NOT SUPPORTED SENSOR INDEX return -1\n");
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return -1;
		}

		#else //HYNIX (REV.A or ELSE)
			/* Configure sensor for Snapshot mode */
			for (i = 0; i < isx005_regs.snap_reg_settings_size; ++i) {
				rc = isx005_i2c_write(isx005_client->addr,
					isx005_regs.snap_reg_settings[i].register_address,
					isx005_regs.snap_reg_settings[i].register_value,
					isx005_regs.snap_reg_settings[i].register_length);

				if (rc < 0){			
					printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
					return rc;
				}
			}

			//[TODO] REMOVE THIS CODE (its temporary)
			rc = isx005_set_effect(current_effect);
			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2\n", __func__);
				return rc;
			}
		#endif
	#else //SAMSUNG or Other Sensor
		/* Configure sensor for Snapshot mode */
		for (i = 0; i < isx005_regs.snap_reg_settings_size; ++i) {
			rc = isx005_i2c_write(isx005_client->addr,
				isx005_regs.snap_reg_settings[i].register_address,
				isx005_regs.snap_reg_settings[i].register_value,
				isx005_regs.snap_reg_settings[i].register_length);

			if (rc < 0){			
				printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
				return rc;
			}
		}
	#endif

	
	#if 0
	//LGE_CHANGE_S[byungsik.choi@lge.com]2010-07-23 6020 patch
	/* Checking the mode change status */
	/* eunyoung.shin@lge.com 2010.07.13*/	
	for(i = 0; i < 300; i++)
	{
		printk(KERN_ERR "[%s]:Sensor Snapshot Mode Start\n", __func__);
		cm_changed_status = 0;
		rc = isx005_i2c_read(isx005_client->addr, 0x00F8, &cm_changed_status, BYTE_LEN);

		if(cm_changed_status & 0x0002)
		{
			printk(KERN_ERR "[%s]:Sensor Snapshot Mode check : %d-> success \n", __func__, cm_changed_status);
			break;
		}
		else
			msleep(10);

		printk(KERN_ERR "[%s]:Sensor Snapshot Mode checking : %d \n", __func__, cm_changed_status);
		}
	//LGE_CHANGE_E[byungsik.choi@lge.com]	
	#else
	
	msleep(150); //[2010.12.24][JKCHAE] mode change confirm delay
	
	#endif
	
	
	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
	return rc;
}

static int isx005_set_sensor_mode(int mode)
{

	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);

	int rc = 0;
	int retry = 0;

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		printk(KERN_ERR "[smiledice] case SENSOR_PREVIEW_MODE \n");
		for (retry = 0; retry < 3; ++retry) {
			rc = isx005_reg_preview();
			if (rc < 0)
				printk(KERN_ERR "[ERROR]%s:Sensor Preview Mode Fail\n", __func__);
			else
				break;

			mdelay(1);
		}
		break;

	case SENSOR_SNAPSHOT_MODE:
		printk(KERN_ERR "[smiledice] case SENSOR_SNAPSHOT_MODE \n");
	case SENSOR_RAW_SNAPSHOT_MODE:	/* Do not support */
		printk(KERN_ERR "[smiledice] case SENSOR_RAW_SNAPSHOT_MODE \n");
		for (retry = 0; retry < 3; ++retry) {
			rc = isx005_reg_snapshot();
			if (rc < 0)
				printk(KERN_ERR "[ERROR]%s:Sensor Snapshot Mode Fail\n", __func__);
			else
				break;			
		}		
		break;

	default:
		printk(KERN_ERR "[smiledice] << %s END : FAIL return -EINVAL\n", __func__);
		return -EINVAL;
	}

	printk(KERN_ERR "Sensor Mode : %d, rc = %d\n", mode, rc);
	printk(KERN_ERR "yt_test%s: isx005_set_sensor_mode! %d\n",__func__,mode);

	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

	return rc;
}


//[2011.01.13][JKCHAE] Sensor Porting (AF)
#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	static int isx005_cancel_focus(int mode)
	{
		printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
		int rc;

		#if (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_A)
			//[2010.12.29][JKCHAE] sensor porting (AF) 
			struct DelayItem delayArray[1] = {{5, 200}};
			rc = isx005_i2c_write_table_with_delay(
				isx005_regs.af_cancel_reg_settings,
				isx005_regs.af_cancel_reg_settings_size,
				delayArray, 1);

		#elif (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B)
			//[2010.12.29][JKCHAE] sensor porting (AF) 
			rc = isx005_i2c_write_table(
				isx005_regs.af_cancel_reg_settings,
				isx005_regs.af_cancel_reg_settings_size);
		#else
			rc = 0;
		#endif

		printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

		return rc;
	}
#else //SAMSUNG CAMERA SENSOR
	static int isx005_cancel_focus(int mode)
	{
		printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
		int rc;

		rc = isx005_i2c_write_table(
			isx005_regs.af_cancel_reg_settings,
			isx005_regs.af_cancel_reg_settings_size);

		printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

		return rc;
	}
#endif

static int isx005_check_af_lock(void)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int rc;
	int i;
	unsigned short af_lock;

	#if 0
	
	/* check AF lock status */
	for (i = 0; i < 10; ++i) {
		/*INT state read -*/
		rc = isx005_i2c_read(isx005_client->addr,
			0x00F8, &af_lock, BYTE_LEN);
		
		if (rc < 0) {
			printk(KERN_ERR "isx005: reading af_lock fail\n");
			printk(KERN_ERR "[smiledice] << %s END : FAIL#1\n", __func__);
			return rc;
		}

		/* af interruption lock state read compelete */
		if((af_lock & 0x10) == 0x10)
			break;

		msleep(10);
	}

	/* INT clear */
	rc = isx005_i2c_write(isx005_client->addr,
		0x00FC, 0x10, BYTE_LEN);
	if (rc < 0) {
		printk(KERN_ERR "[smiledice] << %s END : FAIL#2\n", __func__);
		return rc;
	}

	/* check AF lock status */
	for (i = 0; i < 10; ++i) {
		/*INT state read to confirm INT release state*/
		rc = isx005_i2c_read(isx005_client->addr,
				0x00F8, &af_lock, BYTE_LEN);
		
		if (rc < 0) {
			printk(KERN_ERR "isx005: reading af_lock fail\n");
			printk(KERN_ERR "[smiledice] << %s END : FAIL#3\n", __func__);
			return rc;
		}

		if ((af_lock & 0x10) == 0x00) {
			printk(KERN_ERR "af_lock is released\n");
			break;
		}
		msleep(10);
	}
	#endif

	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

	return rc;
}

//[2011.01.13][JKCHAE] Sensor Porting (AF)
#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	static int isx005_check_focus(int *lock)
	{
		printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
		
		int rc;

		//[2010.12.29][JKCHAE] sensor porting
		#if (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_A)
			/////////////////////////////////////////////////////////////////
			//[Step #3] AF Done Check
			//          - if ([Page60] 0x39 == 0x0A) { AF Done }
			//          - else { AF Fail }

			printk(KERN_ERR "[smiledice] [Step #3] AF Done Check\n");

			//3.1 Read AF Done Register
			unsigned short rdata = 0;
			int i = 0;
			for (i = 0; i < 30; i++) 
			{
				rc = isx005_i2c_page_read(isx005_client->addr, 0x60, 0x39, &rdata, BYTE_LEN);
				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END #1 : I2C Read FAIL (rc: %d)\n", __func__, rc);
					return rc;
				}

				
				//3.2 AF Done Register Check
				if (rdata == 0x0a) {
					printk(KERN_ERR "[smiledice] [Step #3] Succeed: AF Done Check \n");
					//[JKCHAE] continue to next check step #3
					break;
				}
				else {
					printk(KERN_ERR "[smiledice] [Step #3] AF Done Check: Not Yet Done ==> delay 50ms !\n");
					mdelay(50);
				}
			}

			if (rdata != 0x0a)
			{
				printk(KERN_ERR "[smiledice] [Step #3] Succeed: AF Done Check ==> AF Initialize !\n");
				rc = isx005_cancel_focus(prev_af_mode);
				if (rc < 0) 
				{
					printk(KERN_ERR "[smiledice] << %s END #2 : FAIL: AF Initialize Fail\n", __func__);
					return rc;
				}
				else {
					printk(KERN_ERR "[smiledice] [Step #3] AF Initialize OK\n", __func__);
				}

				*lock = CFG_AF_UNLOCKED; //0: focus fail or 2: during focus
				printk(KERN_ERR "[smiledice] << %s END #3 : FAIL: AF Done Check (rdata (0x%x) != 0x0a)(rc: %d)\n", __func__, rdata, rc);
				return rc;
			}


			/////////////////////////////////////////////////////////////////
			//[Step #4] Focus Value check
			//          - if ([Page60] 0x35[MSB], 0x36[LSB] >= 0x45) { AF Pass }
			//          - else { AF Fail }
			printk(KERN_ERR "[smiledice] [Step #4] Focus Value Check\n");


			unsigned short focus_value = 0;
			
			//4.1 Read FocusValue: 0x35 (MSB)
			rdata = 0;
			rc = isx005_i2c_page_read(isx005_client->addr, 0x60, 0x35, &rdata, BYTE_LEN);
			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END #3 : I2C Read FAIL (rc: %d)\n", __func__, rc);
				return rc;
			}
			
			focus_value = (rdata & 0x00FF) << 8;

			//4.2 Read FocusValue: 0x36 (LSB)
			rdata = 0;
			rc = isx005_i2c_page_read(isx005_client->addr, 0x60, 0x36, &rdata, BYTE_LEN);
			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END #4: I2C Read FAIL (rc: %d)\n", __func__, rc);
				return rc;
			}
			
			focus_value |= (rdata & 0x00FF);
			
			printk(KERN_ERR "[smiledice] [Step #4] CHECK FocusValue: %x\n", focus_value);
			
			//4.3 Focus Value Check
			if (focus_value >= 0x45) 
			{
				*lock = CFG_AF_LOCKED;  // success
				printk(KERN_ERR "[smiledice] [Step #4] AF Succeed (focus_value: %x >= 0x45)\n", focus_value);
			}
			else {
				*lock = CFG_AF_UNLOCKED; //0: focus fail or 2: during focus
				printk(KERN_ERR "[smiledice] [Step #4] AF Failed !!!\n");

				
				printk(KERN_ERR "[smiledice] [Step #4] AF Failed ==> AF Initialize !\n");
				rc = isx005_cancel_focus(prev_af_mode);
				if (rc < 0) 
				{
					printk(KERN_ERR "[smiledice] << %s END #5 : FAIL: AF Initialize Fail\n", __func__);
					return rc;
				}
				else {
					printk(KERN_ERR "[smiledice] [Step #4] AF Initialize OK\n", __func__);
				}
			}
			
			printk(KERN_ERR "[smiledice] << %s END #7: rc = %d\n", __func__, rc);
			return rc;
		
		#elif (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B)
			/////////////////////////////////////////////////////////////////
			//[Step #3] AF Done Check
			//          - if ([Page60] 0x39 == 0x0A) { AF Done }
			//          - else { AF Fail }

			printk(KERN_ERR "[smiledice] [Step #3] AF Done Check\n");

			//3.1 Read AF Done Register
			unsigned short rdata = 0;
			int i = 0;
			for (i = 0; i < 30; i++) 
			{
				rc = isx005_i2c_page_read(isx005_client->addr, 0xce, 0x39, &rdata, BYTE_LEN);
				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END #1 : I2C Read FAIL (rc: %d)\n", __func__, rc);
					return rc;
				}

				
				//3.2 AF Done Register Check
				if (rdata == 0x0c) {
					printk(KERN_ERR "[smiledice] [Step #3] Succeed: AF Done Check \n");
					//[JKCHAE] continue to next check step #3
					break;
				}
				else {
					printk(KERN_ERR "[smiledice] [Step #3] AF Done Check: Not Yet Done ==> delay 50ms !\n");
					mdelay(50);
				}
			}

			if (rdata != 0x0c)
			{
				printk(KERN_ERR "[smiledice] [Step #3] AF Done Check: AF Fail ==> AF Initialize !\n");
				rc = isx005_cancel_focus(prev_af_mode);
				if (rc < 0) 
				{
					printk(KERN_ERR "[smiledice] << %s END #2 : FAIL: AF Initialize Fail\n", __func__);
					return rc;
				}
				else {
					printk(KERN_ERR "[smiledice] [Step #3] AF Initialize OK\n", __func__);
				}

				*lock = CFG_AF_UNLOCKED; //0: focus fail or 2: during focus
				printk(KERN_ERR "[smiledice] << %s END #3 : FAIL: AF Done Check (rdata (0x%x) != 0x0c)(rc: %d)\n", __func__, rdata, rc);
				return rc;
			}

			*lock = CFG_AF_LOCKED; //0: focus fail or 2: during focus
			printk(KERN_ERR "[smiledice] << %s END #3: rc = %d\n", __func__, rc);
			return rc;
		#else
			rc = 0;

			printk(KERN_ERR "[smiledice] << %s END #3: rc = %d\n", __func__, rc);
			return rc;
		#endif
	}
#else //SAMSUNG CAMERA SENSOR
	static int isx005_check_focus(int *lock)
	{
		printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
		
		int rc;		
		/////////////////////////////////////////////////////////////////
		//[Step #3] AF Done Check
		//          - switch (read [Page 0x26FE]) 
		//            { 
		//               * case 0x0000: IDLE
		//               * case 0x0001: PROGRESS
		//               * case 0x0002: SUCCESS
		//               * case 0x0003: LOWCONF (AF_FAIL)
		//               * case 0x0004: CANCELED
		//            }

		printk(KERN_ERR "[smiledice] [Step #3] AF Done Check\n");

		//3.1 Read AF State Register
		unsigned short rdata = 0;
		int i = 0;
		bool loop_continue = true;
		const int MAX_AF_WAIT_COUNT = 50;
		for (i = 0; (i < MAX_AF_WAIT_COUNT) && loop_continue; i++) 
		{
			rc = isx005_i2c_page_read(isx005_client->addr, 0x26FE, 0x0F12, &rdata, WORD_LEN);
			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END #1 : I2C Read FAIL (rc: %d)\n", __func__, rc);
				return rc;
			}

			switch (rdata)
			{
			case AF_CHECK_RET_IDLE:
				printk(KERN_ERR "[smiledice] rdata = AF_CHECK_RET_IDLE \n");
				loop_continue = false;
				break;

			case AF_CHECK_RET_PROGRESS:
				printk(KERN_ERR "[smiledice] rdata = AF_CHECK_RET_PROGRESS \n");
				printk(KERN_ERR "[smiledice] [Step #3] Wait 50ms !\n");
				mdelay(50);
				break;

			case AF_CHECK_RET_SUCCESS:
				printk(KERN_ERR "[smiledice] rdata = AF_CHECK_RET_SUCCESS \n");
				loop_continue = false;
				break;

			case AF_CHECK_RET_LOWCONF:
				printk(KERN_ERR "[smiledice] rdata = AF_CHECK_RET_LOWCONF \n");
				loop_continue = false;
				break;

			case AF_CHECK_RET_CANCELED:
				printk(KERN_ERR "[smiledice] rdata = AF_CHECK_RET_CANCELED \n");
				loop_continue = false;
				break;
				
			default:
				printk(KERN_ERR "[smiledice] rdata = (INVALID) \n");
				printk(KERN_ERR "[smiledice] [Step #3] Wait 50ms !\n");
				mdelay(50);
				break;					
			}
		} //end of for loop

		if ((rdata == AF_CHECK_RET_LOWCONF) ||
			(rdata != AF_CHECK_RET_SUCCESS && i == MAX_AF_WAIT_COUNT))
		{
			printk(KERN_ERR "[smiledice] [Step #3] AF FAIL ==> CANCEL FOCUS !\n");
			rc = isx005_cancel_focus(prev_af_mode);
			if (rc < 0) 
			{
				printk(KERN_ERR "[smiledice] [Step #3] FAIL: AF Initialize Fail\n", __func__);
			}
			
			*lock = CFG_AF_UNLOCKED; //0: focus fail or 2: during focus

			printk(KERN_ERR "[smiledice] << %s END #3 : FAIL: AF Done Check (rdata (0x%x) != 0x0a)(rc: %d)\n", __func__, rdata, rc);
			return rc;
		}
		else {
			/////////////////////////////////////////////////////////////////
			//[Step #4] Focus Value check
			//          - (read [Page 0x282E])
			
			#if 0 //Guide from Cowell
				printk(KERN_ERR "[smiledice] [Step #4] Focus Value Check\n");
				//4. Read FocusValue
				rdata = 0;
				rc = isx005_i2c_page_read(isx005_client->addr, 0x282E, 0x0F12, &rdata, WORD_LEN);
				if (rc < 0) {
					*lock = CFG_AF_UNLOCKED; //0: focus fail or 2: during focus
					printk(KERN_ERR "[smiledice] << %s END #3 : I2C Read FAIL (rc: %d)\n", __func__, rc);
					return rc;
				}
				printk(KERN_ERR "[smiledice] [Step #4] CHECK FocusValue: %x\n", rdata);
			#endif

			mdelay(166); //avoid unfocused capture preview (1frame delay + margin)

			#if 1 //Lens Position Check
				printk(KERN_ERR "[smiledice] [Step #4] Lens Position Check\n");
				//4. Read LensPosition
				rdata = 0;
				rc = isx005_i2c_page_read(isx005_client->addr, 0x26f8, 0x0F12, &rdata, WORD_LEN);
				if (rc < 0) {
					*lock = CFG_AF_UNLOCKED; //0: focus fail or 2: during focus
					printk(KERN_ERR "[smiledice] << %s END #3 : I2C Read FAIL (rc: %d)\n", __func__, rc);
					return rc;
				}
				printk(KERN_ERR "[smiledice] [Step #4] CHECK Lens Position: 0x%04x\n", rdata);

				uint16_t threshold = 0;
				switch (prev_af_mode) {
					case FOCUS_NORMAL:
					case FOCUS_MANUAL:
					case FOCUS_AUTO:
						threshold = 0x0078;
						break;
					case FOCUS_MACRO:
						threshold = 0x0097; //MAX BOUNDARY
						break;

					default:
						threshold = 0x0097; //MAX BOUNDARY
						break;
				}

				printk(KERN_ERR "[smiledice] [Step #5] Filter AF Threshold: 0x%04x)\n", threshold);
				if (rdata >= threshold) {
					//redefine lock state from succeed to fail (DV1 NG addressing)
					printk(KERN_ERR "[smiledice] [Step #5] Filter AF false-positive Succeed @ <7Cm (rdata: 0x%04x)\n", rdata);

					printk(KERN_ERR "[smiledice] [Step #5] AF FAIL ==> CANCEL FOCUS !\n");
					rc = isx005_cancel_focus(prev_af_mode);
					if (rc < 0) {
						printk(KERN_ERR "[smiledice] [Step #5] FAIL: AF Initialize Fail\n", __func__);
					}
			
					*lock = CFG_AF_UNLOCKED;
				}
				else {
					*lock = CFG_AF_LOCKED;  // success
				}
			#else
					*lock = CFG_AF_LOCKED;  // success
			#endif


			printk(KERN_ERR "[smiledice] << %s END #7: rc = %d\n", __func__, rc);
			return rc;
		}
	}
#endif

//[2011.01.13][JKCHAE] Sensor Porting (AF)
#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	static int isx005_set_af_start(int mode)
	{
		printk(KERN_ERR "[smiledice] >> %s START (mode: %d) \n", __func__, mode);
		
		int rc = 0;

		#if (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_A)
			/////////////////////////////////////////////////////////////////
			//[Step #2] AF run
			if(prev_af_mode == mode) {
				rc = isx005_i2c_write_table(isx005_regs.af_start_reg_settings,
					isx005_regs.af_start_reg_settings_size);
			} else {
				switch (mode) {
					case FOCUS_NORMAL:
					{
						printk(KERN_ERR "[smiledice] case FOCUS_NORMAL \n");
						struct DelayItem delayArray[1] = {{4, 200}};
						rc = isx005_i2c_write_table_with_delay(
							isx005_regs.af_normal_reg_settings,
							isx005_regs.af_normal_reg_settings_size,
							delayArray, 1);
						break;
					}
					case FOCUS_MACRO:
					{
						printk(KERN_ERR "[smiledice] case FOCUS_MACRO \n");
						rc = isx005_i2c_write_table(
							isx005_regs.af_macro_reg_settings,
							isx005_regs.af_macro_reg_settings_size);
						break;
					}
					case FOCUS_AUTO:	
					{
						printk(KERN_ERR "[smiledice] case FOCUS_AUTO \n");
						struct DelayItem delayArray[1] = {{4, 200}};
						rc = isx005_i2c_write_table_with_delay(
							isx005_regs.af_auto_reg_settings,
							isx005_regs.af_auto_reg_settings_size,
							delayArray, 1);
						break;
					}
					case FOCUS_MANUAL:
					{
						printk(KERN_ERR "[smiledice] case FOCUS_MANUAL \n");
						struct DelayItem delayArray[1] = {{4, 200}};
						rc = isx005_i2c_write_table_with_delay(
							isx005_regs.af_manual_reg_settings,
							isx005_regs.af_manual_reg_settings_size,
							delayArray, 1);
						break;
					}
					default:
					{
						printk(KERN_ERR "[smiledice] invalid af mode \n");
						printk(KERN_ERR "[ERROR]%s: invalid af mode\n", __func__);
						break;
					}
				}

				printk(KERN_ERR "[smiledice] af start \n");
				/*af start*/
				rc = isx005_i2c_write_table(
					isx005_regs.af_start_reg_settings,
					isx005_regs.af_start_reg_settings_size);
			}	
		#elif (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B)
			/////////////////////////////////////////////////////////////////
			//[Step #2] AF run
			if(prev_af_mode == mode) {
				rc = isx005_i2c_write_table(isx005_regs.af_start_reg_settings,
					isx005_regs.af_start_reg_settings_size);
			} else {
				switch (mode) {
					case FOCUS_NORMAL:
					{
						printk(KERN_ERR "[smiledice] case FOCUS_NORMAL \n");
						rc = isx005_i2c_write_table(
							isx005_regs.af_normal_reg_settings,
							isx005_regs.af_normal_reg_settings_size);
						break;
					}
					case FOCUS_MACRO:
					{
						printk(KERN_ERR "[smiledice] case FOCUS_MACRO \n");
						rc = isx005_i2c_write_table(
							isx005_regs.af_macro_reg_settings,
							isx005_regs.af_macro_reg_settings_size);
						break;
					}
					case FOCUS_AUTO:	
					{
						printk(KERN_ERR "[smiledice] case FOCUS_AUTO \n");
						rc = isx005_i2c_write_table(
							isx005_regs.af_auto_reg_settings,
							isx005_regs.af_auto_reg_settings_size);
						break;
					}
					case FOCUS_MANUAL:
					{
						printk(KERN_ERR "[smiledice] case FOCUS_MANUAL \n");
						rc = isx005_i2c_write_table(
							isx005_regs.af_manual_reg_settings,
							isx005_regs.af_manual_reg_settings_size);
						break;
					}
					default:
					{
						printk(KERN_ERR "[smiledice] invalid af mode \n");
						printk(KERN_ERR "[ERROR]%s: invalid af mode\n", __func__);
						break;
					}
				}

				printk(KERN_ERR "[smiledice] af start \n");
				/*af start*/
				rc = isx005_i2c_write_table(
					isx005_regs.af_start_reg_settings,
					isx005_regs.af_start_reg_settings_size);
			}	
		#else

		#endif

		
		prev_af_mode = mode;

		printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
		return rc;
	}
#else
	static int isx005_set_af_start(int mode)
	{
		printk(KERN_ERR "[smiledice] >> %s START (mode: %d) \n", __func__, mode);
		
		int rc = 0;
		
		switch (mode) {
			case FOCUS_NORMAL:
			{
				printk(KERN_ERR "[smiledice] case FOCUS_NORMAL \n");
				#if 0
				struct DelayItem delayArray[2] = {
					{3, 133},
					{5, 200}};
				rc = isx005_i2c_write_table_with_delay(
					isx005_regs.af_normal_reg_settings,
					isx005_regs.af_normal_reg_settings_size,
					delayArray, 2);
				#else
				rc = isx005_i2c_write_table(
					isx005_regs.af_normal_reg_settings,
					isx005_regs.af_normal_reg_settings_size);
				#endif
				break;
			}
			case FOCUS_MACRO:
			{
				printk(KERN_ERR "[smiledice] case FOCUS_MACRO \n");
				#if 0
				struct DelayItem delayArray[2] = {
					{3, 133},
					{5, 200}};
				rc = isx005_i2c_write_table_with_delay(
					isx005_regs.af_macro_reg_settings,
					isx005_regs.af_macro_reg_settings_size,
					delayArray, 2);
				#else
				rc = isx005_i2c_write_table(
					isx005_regs.af_macro_reg_settings,
					isx005_regs.af_macro_reg_settings_size);
				#endif
				break;
			}
			case FOCUS_AUTO:	
			{
				#if 1
				printk(KERN_ERR "[smiledice] case FOCUS_AUTO \n");
				rc = isx005_i2c_write_table(
					isx005_regs.af_auto_reg_settings,
					isx005_regs.af_auto_reg_settings_size);
				#endif
				
				break;
			}
			case FOCUS_MANUAL:	
			{
				printk(KERN_ERR "[smiledice] case FOCUS_MANUAL (Not Yet Implemented!)\n");
				#if 1
				rc = isx005_i2c_write_table(
					isx005_regs.af_manual_reg_settings,
					isx005_regs.af_manual_reg_settings_size);
				#endif
				break;
			}
			default:
			{
				printk(KERN_ERR "[smiledice] invalid af mode \n");
				printk(KERN_ERR "[ERROR]%s: invalid af mode\n", __func__);
				break;
			}
		}
		
		prev_af_mode = mode;
		
		//Samsung sensor requires 2 frame time for AF operation
		mdelay(333); //calculate delay time (= 2 frame time): 15~30fsp = 66~33ms/frame * 2

#if 0
		printk(KERN_ERR "[smiledice] af start \n");
		/*af start*/
		rc = isx005_i2c_write_table(
			isx005_regs.af_start_reg_settings,
			isx005_regs.af_start_reg_settings_size);
#endif

		printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
		return rc;
	}
	
#endif

static int isx005_move_focus(int32_t steps)
{
	printk(KERN_ERR "[smiledice] >> %s(steps:%d) START \n", __func__, steps);
	
	int32_t rc;
	unsigned short cm_changed_sts, cm_changed_clr, af_pos, manual_pos;
	int i;

#if 0
	rc = isx005_i2c_write_table(isx005_regs.af_manual_reg_settings,
			isx005_regs.af_manual_reg_settings_size);

	prev_af_mode = FOCUS_MANUAL;

	if (rc < 0) {
		printk(KERN_ERR "[ERROR]%s:fail in writing for move focus\n",
			__func__);
		printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
		return rc;
	}

	/* check cm_changed_sts */
	for(i = 0; i < 24; ++i) {
		rc = isx005_i2c_read(isx005_client->addr,
				0x00F8, &cm_changed_sts, BYTE_LEN);
		if (rc < 0){
			printk(KERN_ERR "[ERROR]%s; fail in reading cm_changed_sts\n",
				__func__);
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}

		if((cm_changed_sts & 0x02) == 0x02)
			break;

		msleep(10);
	}

	/* clear the interrupt register */
	rc = isx005_i2c_write(isx005_client->addr, 0x00FC, 0x02, BYTE_LEN);
	if (rc < 0) {
		printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
		return rc;
	}

	/* check cm_changed_clr */
	for(i = 0; i < 24; ++i) {
		rc = isx005_i2c_read(isx005_client->addr,
			0x00FC, &cm_changed_clr, BYTE_LEN);
		if (rc < 0) {
			printk(KERN_ERR "[ERROR]%s:fail in reading cm_changed_clr\n",
				__func__);
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}

		if((cm_changed_clr & 0x00) == 0x00)
			break;

		msleep(10);
	}

	if (steps <= 10)
		manual_pos = cpu_to_be16(50 + (50 * steps));
	else
		manual_pos = 50;

	rc = isx005_i2c_write(isx005_client->addr, 0x4852, manual_pos, WORD_LEN);
	if (rc < 0) {		
		printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
		return rc;
	}

	rc = isx005_i2c_write(isx005_client->addr, 0x4850, 0x01, BYTE_LEN);
	if (rc < 0){		
		printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
		return rc;
	}
	
	rc = isx005_i2c_write(isx005_client->addr, 0x0015, 0x01, BYTE_LEN);
	if (rc < 0){		
		printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
		return rc;
	}

	isx005_check_af_lock();
	
	/* check lens position */
	for(i = 0; i < 24; ++i) {
		rc = isx005_i2c_read(isx005_client->addr, 0x6D7A, &af_pos, WORD_LEN);
		if (rc < 0) {
			printk(KERN_ERR "[ERROR]%s:fail in reading af_lenspos\n",
				__func__);
			
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);

		}
	
		if(af_pos == manual_pos)
			break;
		
		msleep(10);
	}
#endif

	#if (CAMERA_SENSOR == SAMSUNG_CAMERA_SENSOR)

		#if 0
		unsigned short MIN_STEP_LENS_POS = 0x003c;
		unsigned short MAX_STEP_LENS_POS = 0x0078;
		unsigned short STEP_SIZE = (MAX_STEP_LENS_POS - MIN_STEP_LENS_POS) / ISX005_TOTAL_STEPS_NEAR_TO_FAR;
		unsigned short lens_pos = MIN_STEP_LENS_POS; //We set min step to default.

		if (steps <= ISX005_TOTAL_STEPS_NEAR_TO_FAR && steps >= 0) {
			lens_pos = MIN_STEP_LENS_POS + (steps * STEP_SIZE);

		}
		else if (steps < 0) {
			lens_pos = MIN_STEP_LENS_POS;
		}
		else { //steps > ISX005_TOTAL_STEPS_NEAR_TO_FAR
			lens_pos = MAX_STEP_LENS_POS;
		}

		printk(KERN_ERR "[smiledice] [CHECK] lens_pos: %d\n", lens_pos);

		//////////////////////////////////////////////////////////////////////
		// Set Manual Focus Registers
		rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
		if (rc < 0) {		
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}

		rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x0254, WORD_LEN);
		if (rc < 0) {		
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}

		rc = isx005_i2c_write(isx005_client->addr, 0x0f12, lens_pos, WORD_LEN);
		if (rc < 0) {		
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}

		msleep(133); //P133

		rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x0252, WORD_LEN);
		if (rc < 0) {		
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}

		rc = isx005_i2c_write(isx005_client->addr, 0x0f12, 0x0004, WORD_LEN);
		if (rc < 0) {		
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}
		//
		//////////////////////////////////////////////////////////////////////

		prev_af_mode = FOCUS_MANUAL;

		#if 0
		isx005_check_af_lock();
		#endif
		
		/* check lens position */
		for(i = 0; i < 24; ++i) {
			unsigned short af_pos = 0;
			rc = isx005_i2c_page_read(isx005_client->addr, 0x0254, 0x0F12, &af_pos, WORD_LEN);
			if (rc < 0) {
				printk(KERN_ERR "[ERROR]%s:fail in reading af_lenspos\n", __func__);
			}
		
			if(af_pos == lens_pos) {
				break;
			}
			
			msleep(10);
		}
		#else

		unsigned short MIN_STEP_LENS_POS = 0x0028;
		unsigned short MAX_STEP_LENS_POS = 0x0078;
		unsigned short lens_pos = MIN_STEP_LENS_POS; //We set min step to default.



		//////////////////////////////////////////////////////////////////////
		// Set Manual Focus Registers
		rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
		if (rc < 0) {		
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}

		rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x0254, WORD_LEN);
		if (rc < 0) {		
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}

		switch (steps)
		{
			case 0:  lens_pos = 0x0028; break;
			case 1:  lens_pos = 0x0030; break;
			case 2:  lens_pos = 0x0038; break;
			case 3:  lens_pos = 0x0040; break;
			case 4:  lens_pos = 0x0048; break;
			case 5:  lens_pos = 0x0050; break;
			case 6:  lens_pos = 0x0058; break;
			case 7:  lens_pos = 0x0060; break;
			case 8:  lens_pos = 0x0068; break;
			case 9:  lens_pos = 0x0070; break;
			case 10: lens_pos = 0x0078; break;
			default: break;	
		};

		printk(KERN_ERR "[smiledice] [CHECK] lens_pos: %d\n", lens_pos);

		rc = isx005_i2c_write(isx005_client->addr, 0x0f12, lens_pos, WORD_LEN);
		if (rc < 0) {		
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}

		msleep(133); //P133

		rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x0252, WORD_LEN);
		if (rc < 0) {		
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}

		rc = isx005_i2c_write(isx005_client->addr, 0x0f12, 0x0004, WORD_LEN);
		if (rc < 0) {		
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}
		//
		//////////////////////////////////////////////////////////////////////

		prev_af_mode = FOCUS_MANUAL;

		#if 0
		isx005_check_af_lock();
		#endif
		
		/* check lens position */
		for(i = 0; i < 24; ++i) {
			unsigned short af_pos = 0;
			rc = isx005_i2c_page_read(isx005_client->addr, 0x0254, 0x0F12, &af_pos, WORD_LEN);
			if (rc < 0) {
				printk(KERN_ERR "[ERROR]%s:fail in reading af_lenspos\n", __func__);
			}
		
			if(af_pos == lens_pos) {
				break;
			}
			
			msleep(10);
		}
		#endif
	#else

	rc = 0;
	
	#endif

	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

	return rc;
}


//[2011.01.13][JKCHAE] Sensor Porting (AF)
#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	static int isx005_set_default_focus(void)
	{
		printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
		
		int rc;

		#if (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_A) 
			//[2010.12.29][JKCHAE] sensor porting (AF)
			rc = isx005_cancel_focus(prev_af_mode);
			if (rc < 0) {
				printk(KERN_ERR "[ERROR]%s:fail in cancel_focus\n", __func__);
				printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
				return rc;
			}

			#if 0
			rc = isx005_i2c_write_table(isx005_regs.af_normal_reg_settings,
				isx005_regs.af_normal_reg_settings_size);
			#else
			struct DelayItem delayArray[1] = {{4, 200}};
			rc = isx005_i2c_write_table_with_delay(
				isx005_regs.af_normal_reg_settings,
				isx005_regs.af_normal_reg_settings_size,
				delayArray, 1);
			#endif

			prev_af_mode = FOCUS_AUTO;

			if (rc < 0) {
				printk(KERN_ERR "[ERROR]%s:fail in writing for focus\n", __func__);
				printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
				return rc;
			}

			#if 0
			msleep(60);
			#endif
			
			isx005_check_focus(&rc);
		#elif (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B) 
			//[2010.12.29][JKCHAE] sensor porting (AF)
			rc = isx005_cancel_focus(prev_af_mode);
			if (rc < 0) {
				printk(KERN_ERR "[ERROR]%s:fail in cancel_focus\n", __func__);
				printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
				return rc;
			}

			rc = isx005_i2c_write_table(
					isx005_regs.af_normal_reg_settings,
					isx005_regs.af_normal_reg_settings_size);

			prev_af_mode = FOCUS_AUTO;

			if (rc < 0) {
				printk(KERN_ERR "[ERROR]%s:fail in writing for focus\n", __func__);
				printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
				return rc;
			}

			#if 0
			msleep(60);
			#endif
			
			isx005_check_focus(&rc);
		#else
			rc = 0;
		#endif

		printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

		return rc;
	}
#else //SAMSUNG CAMERA SENSOR
	static int isx005_set_default_focus(void)
	{
		printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
		
		int rc;

		#if 0
		rc = isx005_cancel_focus(prev_af_mode);
		if (rc < 0) {
			printk(KERN_ERR "[ERROR]%s:fail in cancel_focus\n", __func__);
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}

		rc = isx005_i2c_write_table(isx005_regs.af_normal_reg_settings,
			isx005_regs.af_normal_reg_settings_size);

		prev_af_mode = FOCUS_AUTO;

		if (rc < 0) {
			printk(KERN_ERR "[ERROR]%s:fail in writing for focus\n", __func__);
			printk(KERN_ERR "[smiledice] << %s END : FAIL\n", __func__);
			return rc;
		}

		#if 0
		msleep(60);
		#endif
		
		isx005_check_focus(&rc);
		#else
		
		rc = 0;
		
		#endif

		printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

		return rc;
	}
#endif

static int isx005_set_effect(int effect)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	int rc = 0;

	#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	#if (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B)
	current_effect = effect;

	switch (effect) {
	case CAMERA_EFFECT_OFF:
		printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_OFF:");
		rc = isx005_i2c_write_table(
				effect_off_reg_settings,
				effect_off_reg_settings_size);

		if (rc < 0) {
			printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
			return rc;
		}
		break;

	case CAMERA_EFFECT_MONO:
		printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_MONO:");
		rc = isx005_i2c_write_table(
				effect_mono_reg_settings,
				effect_mono_reg_settings_size);

		if (rc < 0) {
			printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
			return rc;
		}
		break;

	case CAMERA_EFFECT_NEGATIVE:
		printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_NEGATIVE:");
		rc = isx005_i2c_write_table(
				effect_negative_reg_settings,
				effect_negative_reg_settings_size);

		if (rc < 0) {
			printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
			return rc;
		}
		break;

	case CAMERA_EFFECT_SOLARIZE:
		printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_SOLARIZE:");
		printk(KERN_ERR "[smiledice] [CHECK] (CURRENTLY, WE HAVE NO VALUE TO SET SOLARIZE)");
		break;

	case CAMERA_EFFECT_SEPIA:
		printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_SEPIA:");
		rc = isx005_i2c_write_table(
				effect_sepia_reg_settings,
				effect_sepia_reg_settings_size);

		if (rc < 0) {
			printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
			return rc;
		}
		break;

	case CAMERA_EFFECT_AQUA:
		printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_AQUA:");
		rc = isx005_i2c_write_table(
				effect_aqua_reg_settings,
				effect_aqua_reg_settings_size);

		if (rc < 0) {
			printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
			return rc;
		}
		break;

	case CAMERA_EFFECT_MONO_NEGATIVE:
		printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_MONO_NEGATIVE:");
		printk(KERN_ERR "[smiledice] [CHECK] (CURRENTLY, WE HAVE NO VALUE TO SET MONO_NEGATIVE)");
		break;

	case CAMERA_EFFECT_SKETCH:
		printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_SKETCH:");
		printk(KERN_ERR "[smiledice] [CHECK] (CURRENTLY, WE HAVE NO VALUE TO SET SKETCH)");
		break;

	default:
		printk(KERN_ERR "[smiledice] << %s END : FAIL#17 (-EINVAL)\n", __func__);
		return -EINVAL;
	}
	#endif 
	#else // SAMSUNG SENSOR
		current_effect = effect;

		switch (effect) {
		case CAMERA_EFFECT_OFF:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_OFF:");
			rc = isx005_i2c_write_table(
					effect_off_reg_settings,
					effect_off_reg_settings_size);

			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;
			}
			break;

		case CAMERA_EFFECT_MONO:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_MONO:");
			rc = isx005_i2c_write_table(
					effect_mono_reg_settings,
					effect_mono_reg_settings_size);

			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;
			}
			break;

		case CAMERA_EFFECT_NEGATIVE:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_NEGATIVE:");
			rc = isx005_i2c_write_table(
					effect_negative_reg_settings,
					effect_negative_reg_settings_size);


			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END : FAIL#3 (rc = %d)\n", __func__, rc);
				return rc;
			}
			break;

		case CAMERA_EFFECT_SOLARIZE:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_SOLARIZE:");
			printk(KERN_ERR "[smiledice] [CHECK] (CURRENTLY, WE HAVE NO VALUE TO SET SOLARIZE)");
			break;

		case CAMERA_EFFECT_SEPIA:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_SEPIA:");
			rc = isx005_i2c_write_table(
					effect_sepia_reg_settings,
					effect_sepia_reg_settings_size);

			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END : FAIL#5 (rc = %d)\n", __func__, rc);
				return rc;
			}
			break;


		case CAMERA_EFFECT_AQUA:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_AQUA:");
			rc = isx005_i2c_write_table(
					effect_aqua_reg_settings,
					effect_aqua_reg_settings_size);

			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END : FAIL#6 (rc = %d)\n", __func__, rc);
				return rc;
			}
			break;

		case CAMERA_EFFECT_MONO_NEGATIVE:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_MONO_NEGATIVE:");
			rc = isx005_i2c_write_table(
					effect_mono_negative_reg_settings,
					effect_mono_negative_reg_settings_size);
			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END : FAIL#8 (rc = %d)\n", __func__, rc);
				return rc;
			}
			break;

		case CAMERA_EFFECT_SKETCH:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_EFFECT_SKETCH:");
			rc = isx005_i2c_write_table(
					effect_sketch_reg_settings,
					effect_sketch_reg_settings_size);
			if (rc < 0) {
				printk(KERN_ERR "[smiledice] << %s END : FAIL#8 (rc = %d)\n", __func__, rc);
				return rc;
			}
			break;

		default:
			printk(KERN_ERR "[smiledice] << %s END : FAIL#9 (-EINVAL)\n", __func__);
			return -EINVAL;
		}
	#endif


	printk(KERN_ERR "Effect : %d, rc = %d\n", effect, rc);

	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

	return rc;
}


static int isx005_set_wb(int mode)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	int rc = 0;

#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	#if (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B)
	if (current_sensor == 0x00) { //PV1 
		switch (mode) {
			case CAMERA_WB_AUTO:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_AUTO:");
				rc = isx005_i2c_write_table(
						wb_auto_reg_settings,
						wb_auto_reg_size);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				break;
				
			case CAMERA_WB_CUSTOM:	/* Do not support */
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_CUSTOM:");
				break;

			case CAMERA_WB_INCANDESCENT:
				rc = isx005_i2c_write_table(
						wb_incandscent_reg_settings,
						wb_incandscent_reg_size);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_INCANDESCENT:");
				break;

			case CAMERA_WB_FLUORESCENT:
				rc = isx005_i2c_write_table(
						wb_fluorescent_reg_settings,
						wb_fluorescent_reg_size);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_FLUORESCENT:");
				break;
				
			case CAMERA_WB_DAYLIGHT:
				rc = isx005_i2c_write_table(
						wb_sunny_reg_settings,
						wb_sunny_reg_size);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_DAYLIGHT:");
				break;

			case CAMERA_WB_CLOUDY_DAYLIGHT:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_CLOUDY_DAYLIGHT:");
				rc = isx005_i2c_write_table(
						wb_cloudy_reg_settings,
						wb_cloudy_reg_size);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				break;

			case CAMERA_WB_TWILIGHT:	/* Do not support */
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_TWILIGHT:");
				break;

			case CAMERA_WB_SHADE:		/* Do not support */
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_SHADE:");
				break;

			default:
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (-EINVAL)\n", __func__, rc);
				return -EINVAL;
		}
	}
	else if (current_sensor == 0x01) { //PV2
		switch (mode) {
			case CAMERA_WB_AUTO:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_AUTO:");
				rc = isx005_i2c_write_table(
						wb_auto_reg_settings_pv2,
						wb_auto_reg_size_pv2);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				break;
				
			case CAMERA_WB_CUSTOM:	/* Do not support */
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_CUSTOM:");
				break;

			case CAMERA_WB_INCANDESCENT:
				rc = isx005_i2c_write_table(
						wb_incandscent_reg_settings_pv2,
						wb_incandscent_reg_size_pv2);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_INCANDESCENT:");
				break;

			case CAMERA_WB_FLUORESCENT:
				rc = isx005_i2c_write_table(
						wb_fluorescent_reg_settings_pv2,
						wb_fluorescent_reg_size_pv2);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_FLUORESCENT:");
				break;
				
			case CAMERA_WB_DAYLIGHT:
				rc = isx005_i2c_write_table(
						wb_sunny_reg_settings_pv2,
						wb_sunny_reg_size_pv2);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_DAYLIGHT:");
				break;

			case CAMERA_WB_CLOUDY_DAYLIGHT:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_CLOUDY_DAYLIGHT:");
				rc = isx005_i2c_write_table(
						wb_cloudy_reg_settings_pv2,
						wb_cloudy_reg_size_pv2);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				break;

			case CAMERA_WB_TWILIGHT:	/* Do not support */
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_TWILIGHT:");
				break;

			case CAMERA_WB_SHADE:		/* Do not support */
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_SHADE:");
				break;

			default:
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (-EINVAL)\n", __func__, rc);
				return -EINVAL;
		}
	}
	else { //Not supported.
			printk(KERN_ERR "[smiledice] [CHECK] NOT SUPPORTED SENSOR INDEX return -1\n");
			return -1;
	}
	

	#else //REV_A

	#endif

#else //SAMSUNG CAMERA SENSOR

		switch (mode) {
			case CAMERA_WB_AUTO:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_AUTO:");
				rc = isx005_i2c_write_table(
						wb_auto_reg_settings,
						wb_auto_reg_size);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				break;
				
			case CAMERA_WB_CUSTOM:	/* Do not support */
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_CUSTOM:");
				break;

			case CAMERA_WB_INCANDESCENT:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_INCANDESCENT:");
				rc = isx005_i2c_write_table(
						wb_incandscent_reg_settings,
						wb_incandscent_reg_size);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				break;

			case CAMERA_WB_FLUORESCENT:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_FLUORESCENT:");
				rc = isx005_i2c_write_table(
						wb_fluorescent_reg_settings,
						wb_fluorescent_reg_size);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				break;
				
			case CAMERA_WB_DAYLIGHT:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_DAYLIGHT:");
				rc = isx005_i2c_write_table(
						wb_sunny_reg_settings,
						wb_sunny_reg_size);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				break;

			case CAMERA_WB_CLOUDY_DAYLIGHT:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_CLOUDY_DAYLIGHT:");
				rc = isx005_i2c_write_table(
						wb_cloudy_reg_settings,
						wb_cloudy_reg_size);

				if (rc < 0) {
					printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
					return rc;
				}
				break;

			case CAMERA_WB_TWILIGHT:	/* Do not support */
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_TWILIGHT:");
				break;

			case CAMERA_WB_SHADE:		/* Do not support */
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_WB_SHADE:");
				break;

			default:
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (-EINVAL)\n", __func__, rc);
				return -EINVAL;
		}
#endif

	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
	return rc;
}


static int isx005_set_antibanding(int mode)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	int rc;

#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	#if (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B)
	switch (mode) {
		case CAMERA_ANTIBANDING_OFF:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_ANTIBANDING_OFF:");
			break;

		case CAMERA_ANTIBANDING_60HZ:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_ANTIBANDING_60HZ:");
			break;

		case CAMERA_ANTIBANDING_50HZ:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_ANTIBANDING_50HZ:");
			break;

		case CAMERA_ANTIBANDING_AUTO:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_ANTIBANDING_AUTO:");
			break;

		case CAMERA_MAX_ANTIBANDING:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_MAX_ANTIBANDING:");
			break;

		default:
			printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (-EINVAL)\n", __func__, rc);
			return -EINVAL;
	}
	#endif
#endif

	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

	return rc;	
}

static int isx005_get_iso(int* isoValue) //void* argp, struct sensor_cfg_data* cfg_data)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	int32_t rc = 0;

	*isoValue = 200; //We set Default Value as 200.
	unsigned short rdata = 0x00;

	//1. ISO Speed Read (7000Page / 23EC)
	rc = isx005_i2c_page_read(isx005_client->addr, 0x23EC, 0x0F12, &rdata, WORD_LEN);
	if (rc < 0) {
		printk(KERN_ERR "[smiledice] << %s END #1 : I2C Read FAIL (rc: %d)\n", __func__, rc);
		return rc;
	}

	//2. Calculate ISO Value
	int gain = (rdata * 1000) / 256;
	printk(KERN_ERR "[smiledice] [CHECK] gain value: %d (scaled by 1000)\n", gain);

	
	if (gain >= 1000 && gain <= 1899) {
		*isoValue = 50;
	}
	else if (gain >= 1900 && gain <= 2299) {
		*isoValue = 100;
	}
	else if (gain >= 2300 && gain <= 2799) {
		*isoValue = 200;
	}
	else if (gain >= 2800) {
		*isoValue = 400;
	}
	else {
		printk(KERN_ERR "[smiledice] [CHECK] INVALID gain value: %d (scaled by 1000)\n", gain);
	}

	printk(KERN_ERR "[smiledice] [CHECK] ISO Value: %d\n", *isoValue);

	return rc;
}

static int isx005_get_focus_distance(int* focusDistances) 
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	int32_t rc = 0;

	uint8_t near    = 5;   //5cm
	uint8_t optimal = near;
	uint8_t far     = 120; //120cm

	unsigned short rdata = 0x00;

	//1. Lens Position Read (7000Page / 26F8)
	rc = isx005_i2c_page_read(isx005_client->addr, 0x26f8, 0x0F12, &rdata, WORD_LEN);
	if (rc < 0) {
		printk(KERN_ERR "[smiledice] << %s END #1 : I2C Read FAIL (rc: %d)\n", __func__, rc);
		return rc;
	}

	//2. Calculate Distances
	if (rdata <= 0x0041) {
			optimal = 110;
	}
	else if (rdata <= 0x4D) {
			optimal = 100;
	}
	else if (rdata <= 0x50) {
			optimal = 40;
	}
	else if (rdata <= 0x53) {
			optimal = 30;
	}
	else if (rdata <= 0x59) {
			optimal = 25;
	}
	else if (rdata <= 0x5C) {
			optimal = 20;
	}
	else if (rdata <= 0x5F) {
			optimal = 10;
	}
	else if (rdata <= 0x62) {
			optimal = 7;
	}
	else {
		printk(KERN_ERR "[smiledice] [CHECK] rdata (0x%04x) means < 7cm\n", rdata);
	}

	//Set FocusDistances (bit combination)
	*focusDistances = ((int)near << 16) | ((int)optimal << 8)  | far;
	
	printk(KERN_ERR "[smiledice] [CHECK] bitwised focusDistances Value: %d\n", *focusDistances);

	return rc;
}

static int isx005_set_iso(int iso)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	int32_t rc = 0;

#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
	#if (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B)
	switch (iso) {
		case CAMERA_ISO_AUTO:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_ISO_AUTO:");
			break;

		case CAMERA_ISO_DEBLUR:	/* Do not support */
		case CAMERA_ISO_100:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_ISO_100:");
			break;

		case CAMERA_ISO_200:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_ISO_200:");
			break;

		case CAMERA_ISO_400:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_ISO_400:");
			break;
			
		case CAMERA_ISO_800:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_ISO_800:");
			break;

		default:
			printk(KERN_ERR "[smiledice] << %s END : FAIL (-EINVAL)\n", __func__, rc);
			rc = -EINVAL;
	}
	#else //REV_A

	#endif
#else //SAMSUNG CAMERA SENSOR

	switch (iso) {
		case CAMERA_ISO_AUTO:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_ISO_AUTO:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x1680, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0xCFD0, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0000, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x1688, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0xCFD0, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0000, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x168E, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0680, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0544, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0100, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0288, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0535, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x014D, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x037a, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0535, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0535, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x023C, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0000, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0240, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0230, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x023e, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			break;

		case CAMERA_ISO_DEBLUR:	/* Do not support */
		case CAMERA_ISO_100:
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x1680, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x1340, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0002, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x1688, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x1340, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0002, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x168E, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0300, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0544, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0100, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0288, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x03E8, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x014D, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x037a, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x03E8, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x03E8, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x023C, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0000, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0240, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0230, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x023e, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			break;

		case CAMERA_ISO_200:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_ISO_200:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x1680, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x1340, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0002, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x1688, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x1340, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0002, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x168E, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0600, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0544, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0100, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0288, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0457, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x014D, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x037a, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0457, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0457, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x023C, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0000, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0240, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0230, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x023e, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			break;

		case CAMERA_ISO_400:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_ISO_400:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x1680, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x1340, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0002, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x1688, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x1340, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0002, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x168E, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0600, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0544, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0100, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0288, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0594, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x014D, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x037a, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0594, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0594, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x023C, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0000, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0240, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0230, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x023e, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			break;
			
		case CAMERA_ISO_800:
			printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_ISO_800:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x1680, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x1340, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0002, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x1688, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x1340, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0002, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x168E, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0800, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0544, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0100, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0288, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0594, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x014D, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x037a, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0594, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0594, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x023C, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0000, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0240, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0230, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x023e, WORD_LEN);
			rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
			break;

		default:
			printk(KERN_ERR "[smiledice] << %s END : FAIL (-EINVAL)\n", __func__, rc);
			rc = -EINVAL;
	}
#endif

	
	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
	return rc;
}


static int32_t isx005_set_scene_mode(int8_t mode)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	int32_t rc = 0;

	#if ( ((CAMERA_SENSOR == HYNIX_CAMERA_SENSOR) && (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B)) || \
			(CAMERA_SENSOR == SAMSUNG_CAMERA_SENSOR) )
		#if 0 //[TODO] Restore. (Temporarily Removed.)
		if (prev_scene_mode == mode) {
			
			printk(KERN_ERR "[smiledice] << %s END : rc = %d (mode is same)\n", __func__, rc);
			return rc;
		}
		#endif

		switch (mode) {
			case CAMERA_SCENE_AUTO:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_SCENE_AUTO:");
				rc = isx005_i2c_write_table(isx005_regs.scene_auto_reg_settings,
					isx005_regs.scene_auto_reg_settings_size);
				break;

			case CAMERA_SCENE_PORTRAIT:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_SCENE_PORTRAIT:");
				rc = isx005_i2c_write_table(isx005_regs.scene_portrait_reg_settings,
					isx005_regs.scene_portrait_reg_settings_size);
				break;

			case CAMERA_SCENE_LANDSCAPE:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_SCENE_LANDSCAPE:");
				rc = isx005_i2c_write_table(isx005_regs.scene_landscape_reg_settings,
					isx005_regs.scene_landscape_reg_settings_size);
				break;

			case CAMERA_SCENE_SPORTS:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_SCENE_SPORTS:");
				rc = isx005_i2c_write_table(isx005_regs.scene_sports_reg_settings,
					isx005_regs.scene_sports_reg_settings_size);
				break;

			case CAMERA_SCENE_SUNSET:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_SCENE_SUNSET:");
				rc = isx005_i2c_write_table(isx005_regs.scene_sunset_reg_settings,
					isx005_regs.scene_sunset_reg_settings_size);
				break;

			case CAMERA_SCENE_NIGHT:
				printk(KERN_ERR "[smiledice] [CHECK] case CAMERA_SCENE_NIGHT:");
				rc = isx005_i2c_write_table(isx005_regs.scene_night_reg_settings,
					isx005_regs.scene_night_reg_settings_size);
				break;

			default:
				printk(KERN_ERR "[ERROR]%s:Incorrect scene mode value\n", __func__);
				break;
		}
	#endif
	
	prev_scene_mode = mode;

	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

	return rc;
}

static int32_t isx005_set_brightness(int8_t brightness)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int32_t rc=0;
#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
#if (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B)
	switch (brightness) {
		case 0:
			printk(KERN_ERR "[smiledice] [CHECK] case 0:");
			rc = isx005_i2c_write(isx005_client->addr,
					0x03, 0x10, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr,
					0x13, 0x0a, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr,
					0x4a, 0x30, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 1:
			printk(KERN_ERR "[smiledice] [CHECK] case 1:");
			rc = isx005_i2c_write(isx005_client->addr,
					0x03, 0x10, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr,
					0x13, 0x0a, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr,
					0x4a, 0x40, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 2:
			printk(KERN_ERR "[smiledice] [CHECK] case 2:");
			rc = isx005_i2c_write(isx005_client->addr,
					0x03, 0x10, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr,
					0x13, 0x0a, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr,
					0x4a, 0x50, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 3:
			printk(KERN_ERR "[smiledice] [CHECK] case 3:");
			rc = isx005_i2c_write(isx005_client->addr,
					0x03, 0x10, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr,
					0x13, 0x0a, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr,
					0x4a, 0x60, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 4:
			printk(KERN_ERR "[smiledice] [CHECK] case 4:");
			rc = isx005_i2c_write(isx005_client->addr,
					0x03, 0x10, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr,
					0x13, 0x0a, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr,
					0x4a, 0x70, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 5:
			printk(KERN_ERR "[smiledice] [CHECK] case 5:");
			rc = isx005_i2c_write(isx005_client->addr,
					0x03, 0x10, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr,
					0x13, 0x0a, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr,
					0x4a, 0x80, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 6:
			printk(KERN_ERR "[smiledice] [CHECK] case 6:");
			rc = isx005_i2c_write(isx005_client->addr,
					0x03, 0x10, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr,
					0x13, 0x0a, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr,
					0x4a, 0x90, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 7:
			printk(KERN_ERR "[smiledice] [CHECK] case 7:");
			rc = isx005_i2c_write(isx005_client->addr,
					0x03, 0x10, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr,
					0x13, 0x0a, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr,
					0x4a, 0xa0, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 8:
			printk(KERN_ERR "[smiledice] [CHECK] case 8:");
			rc = isx005_i2c_write(isx005_client->addr,
					0x03, 0x10, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr,
					0x13, 0x0a, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr,
					0x4a, 0xb0, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 9:
			printk(KERN_ERR "[smiledice] [CHECK] case 9:");
			rc = isx005_i2c_write(isx005_client->addr,
					0x03, 0x10, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr,
					0x13, 0x0a, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr,
					0x4a, 0xc0, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 10:
			printk(KERN_ERR "[smiledice] [CHECK] case 10:");
			rc = isx005_i2c_write(isx005_client->addr,
					0x03, 0x10, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr,
					0x13, 0x0a, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr,
					0x4a, 0xd0, BYTE_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		default:
			printk(KERN_ERR "[ERROR]%s:incoreect brightness value: %d\n",
				__func__, brightness);
	}

#endif
#else //SAMSUNG SENSOR

	switch (brightness) {
		case -5:
			printk(KERN_ERR "[smiledice] [CHECK] case -5:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x020C, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr, 0x0f12, 0xff9c, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case -4:
			printk(KERN_ERR "[smiledice] [CHECK] case -4:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x020C, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr, 0x0f12, 0xffb0, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case -3:
			printk(KERN_ERR "[smiledice] [CHECK] case -3:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x020C, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr, 0x0f12, 0xffc4, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case -2:
			printk(KERN_ERR "[smiledice] [CHECK] case -2:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x020C, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr, 0x0f12, 0xffd8, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case -1:
			printk(KERN_ERR "[smiledice] [CHECK] case -1:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x020C, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr, 0x0f12, 0xffec, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 0:
			printk(KERN_ERR "[smiledice] [CHECK] case 0:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x020C, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr, 0x0f12, 0x0000, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 1:
			printk(KERN_ERR "[smiledice] [CHECK] case 1:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x020C, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr, 0x0f12, 0x0014, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 2:
			printk(KERN_ERR "[smiledice] [CHECK] case 2:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x020C, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr, 0x0f12, 0x0028, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 3:
			printk(KERN_ERR "[smiledice] [CHECK] case 3:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x020C, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr, 0x0f12, 0x003c, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 4:
			printk(KERN_ERR "[smiledice] [CHECK] case 4:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x020C, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr, 0x0f12, 0x0050, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		case 5:
			printk(KERN_ERR "[smiledice] [CHECK] case 5:");
			rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
				return rc;	
			}		

			rc = isx005_i2c_write(isx005_client->addr, 0x002a, 0x020C, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	

			rc = isx005_i2c_write(isx005_client->addr, 0x0f12, 0x0064, WORD_LEN);
			if(rc<0){
				printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
				return rc;	
			}	
			break;

		default:
			printk(KERN_ERR "[ERROR]%s:incoreect brightness value: %d\n",
				__func__, brightness);
	}

#endif
	
	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
	return rc;
}


//LGE_CHANGE_S CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)
#if (CAMERA_SENSOR == SAMSUNG_CAMERA_SENSOR)
static int isx005_set_fps_range(int fps)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	int rc = 0;

	const int MAX_FPS_BOUNDARY = 30000; //30fps
	const int MIN_FPS_BOUNDARY = 7500;  //7.5fps

	uint16_t min_fps = (((uint32_t) fps) & 0xFFFF0000) >> 16;
	uint16_t max_fps = (fps & 0x0000FFFF);

	printk(KERN_ERR "[smiledice] [CHECK] min_fps: %d, max_fps: %d\n",  min_fps, max_fps);

	//VALIDATE REQUESTED FPS VALUES
	if (min_fps == 0 || min_fps < MIN_FPS_BOUNDARY) {
		min_fps = MIN_FPS_BOUNDARY;
		printk(KERN_ERR "[smiledice] [CHECK] modify min_fps: %d\n",  min_fps);
	}

	if (max_fps > MAX_FPS_BOUNDARY) {
		max_fps = MAX_FPS_BOUNDARY;
		printk(KERN_ERR "[smiledice] [CHECK] modify max_fps: %d\n",  max_fps);
	}


	//DETERMINE FPS MODE (VARIABLE / FIXED)
	int fpsType = CAMSENSOR_VARIABLE_FPS;
	if (min_fps == max_fps) { //means fixed fps
		fpsType = CAMSENSOR_FIXED_FPS;
		min_fps = max_fps;
	}

	//CHECK INVALID FPS RANGE
	if (min_fps > max_fps) {
		rc = -1;
		printk(KERN_ERR "[smiledice] [CHECK] fps range (%d ~ %d) is not valid!\n", min_fps, max_fps);
		printk(KERN_ERR "[smiledice] << %s END (rc = %d)\n", __func__, rc);
		return rc;
	}

	//SET REGISTER
	do {
		rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
		if (rc < 0) break;
		rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0284, WORD_LEN);
		if (rc < 0) break;

		//Set Variable / Fixed Fps
 		//0: Variable fps, 2: Fixed fps
		printk(KERN_ERR "[smiledice] [CHECK] fps type: 0x%04x \
				(cf. 0: Variable, 2: Fixed)\n", fpsType);
		rc = isx005_i2c_write(isx005_client->addr, 0x0F12, fpsType, WORD_LEN);
		if (rc < 0) break;

		rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
		if (rc < 0) break;

		//0. Calc FPS
		// [Equation] calc_value = 10000 / actual_fps.
		// Cf. min_fps, max_fps value is scaled by 1000.
		uint16_t calc_min_fps = (10000 * 1000 / min_fps);
		uint16_t calc_max_fps = (10000 * 1000 / max_fps);
		printk(KERN_ERR "[smiledice] [CHECK] calculated min fps value: 0x%04x\n", calc_min_fps);
		printk(KERN_ERR "[smiledice] [CHECK] calculated max fps value: 0x%04x\n", calc_max_fps);

		//1. Set Min FPS
		rc = isx005_i2c_write(isx005_client->addr, 0x0F12, calc_min_fps, WORD_LEN);
		if (rc < 0) break;

		//2. Set Max FPS
		rc = isx005_i2c_write(isx005_client->addr, 0x0F12, calc_max_fps, WORD_LEN);
		if (rc < 0) break;

		//Update Preview Configuration
		rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0230, WORD_LEN);
		if (rc < 0) break;
		rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
		if (rc < 0) break;

		rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x023E, WORD_LEN);
		if (rc < 0) break;
		rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
		if (rc < 0) break;
	} while (0);

	printk(KERN_ERR "[smiledice] << %s END (rc = %d)\n", __func__, rc);
	return rc;
}
#else
static int isx005_set_fps_range(int fps)
{
	return 0;
}
#endif
//LGE_CHANGE_E CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)

//LGE_CHANGE_S CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)
#if (CAMERA_SENSOR == SAMSUNG_CAMERA_SENSOR)
static int isx005_set_frame_rate(int fps)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	int rc = 0;

	const int MAX_FPS_SUPPORTED = 30;
	const int MIN_FPS_SUPPORTED = 8;

	int8_t min_fps = ((fps & 0x0000FF00) >> 8);
	int8_t max_fps = (fps & 0x000000FF);

	printk(KERN_ERR "[smiledice] [CHECK] min_fps: %d, max_fps: %d\n",  min_fps, max_fps);

	//VALIDATE REQUESTED FPS VALUES
	if (min_fps == 0 || min_fps < MIN_FPS_SUPPORTED) {
		min_fps = MIN_FPS_SUPPORTED;
		printk(KERN_ERR "[smiledice] [CHECK] modify min_fps: %d\n",  min_fps);
	}

	if (max_fps > MAX_FPS_SUPPORTED) {
		max_fps = MAX_FPS_SUPPORTED;
		printk(KERN_ERR "[smiledice] [CHECK] modify max_fps: %d\n",  max_fps);
	}


	//DETERMINE FPS MODE (VARIABLE / FIXED)
	int fpsType = CAMSENSOR_VARIABLE_FPS;
	if (min_fps == max_fps) { //means fixed fps
		fpsType = CAMSENSOR_FIXED_FPS;
		min_fps = max_fps;
	}

	//CHECK INVALID FPS RANGE
	if (min_fps > max_fps) {
		rc = -1;
		printk(KERN_ERR "[smiledice] [CHECK] fps range (%d ~ %d) is not valid!\n", min_fps, max_fps);
		printk(KERN_ERR "[smiledice] << %s END (rc = %d)\n", __func__, rc);
		return rc;
	}

	//SET REGISTER
	do {
		rc = isx005_i2c_write(isx005_client->addr, 0x0028, 0x7000, WORD_LEN);
		if (rc < 0) break;
		rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0284, WORD_LEN);
		if (rc < 0) break;

		//Set Variable / Fixed Fps
 		//0: Variable fps, 2: Fixed fps
		printk(KERN_ERR "[smiledice] [CHECK] fps type: 0x%04x \
				(cf. 0: Variable, 2: Fixed)\n", fpsType);
		rc = isx005_i2c_write(isx005_client->addr, 0x0F12, fpsType, WORD_LEN);
		if (rc < 0) break;

		rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
		if (rc < 0) break;

		//0. Calc FPS
		uint16_t calc_min_fps = (10000 / min_fps);
		uint16_t calc_max_fps = (10000 / max_fps);
		printk(KERN_ERR "[smiledice] [CHECK] calculated min fps value: 0x%04x\n", calc_min_fps);
		printk(KERN_ERR "[smiledice] [CHECK] calculated max fps value: 0x%04x\n", calc_max_fps);

		//1. Set Min FPS
		rc = isx005_i2c_write(isx005_client->addr, 0x0F12, calc_min_fps, WORD_LEN);
		if (rc < 0) break;

		//2. Set Max FPS
		rc = isx005_i2c_write(isx005_client->addr, 0x0F12, calc_max_fps, WORD_LEN);
		if (rc < 0) break;

		//Update Preview Configuration
		rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x0230, WORD_LEN);
		if (rc < 0) break;
		rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
		if (rc < 0) break;

		rc = isx005_i2c_write(isx005_client->addr, 0x002A, 0x023E, WORD_LEN);
		if (rc < 0) break;
		rc = isx005_i2c_write(isx005_client->addr, 0x0F12, 0x0001, WORD_LEN);
		if (rc < 0) break;
	} while (0);

	printk(KERN_ERR "[smiledice] << %s END (rc = %d)\n", __func__, rc);
	return rc;
}
#else
static int isx005_set_frame_rate(int fps)
{
	return 0;
}
#endif
//LGE_CHANGE_E CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)

#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
#if (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B)
static int isx005_set_registers_by_revision()
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);

	//1. READ Sensor Revision Info.
	int rc = isx005_i2c_page_read(isx005_client->addr, 
			0x00, 0x05, 
			&current_sensor, BYTE_LEN);

	if (rc < 0) {
		printk(KERN_ERR "[smiledice] << %s END #1 : I2C Read FAIL (rc: %d)\n", __func__, rc);
		return rc;
	}

	//2. ASSIGN registers (init/preview/snapshot)
	switch(current_sensor) {
		case 0x00:  //PV1 
		{
			printk(KERN_ERR "[smiledice] [CHECK] PV1 Hynix Sensor\n");
			//Default Set  (NO NEED TO SET)
			break;
	   	}
		case 0x01:  //PV2
		{
			printk(KERN_ERR "[smiledice] [CHECK] PV2 Hynix Sensor\n");

			isx005_regs.init_reg_settings      = init_reg_settings_pv2;
			isx005_regs.init_reg_settings_size = ARRAY_SIZE(init_reg_settings_pv2);

			isx005_regs.prev_reg_settings	   = preview_mode_reg_pv2;
			isx005_regs.prev_reg_settings_size = ARRAY_SIZE(preview_mode_reg_pv2);

			isx005_regs.snap_reg_settings      = snapshot_mode_reg_pv2;
			isx005_regs.snap_reg_settings_size = ARRAY_SIZE(snapshot_mode_reg_pv2);

			break;
	   	}
		default:
		{
			printk(KERN_ERR "[smiledice] [CHECK] (Undefined) Hynix Sensor\n");
			break;
	   	}
	}

	printk(KERN_ERR "[smiledice] << %s END (rc = %d)\n", __func__, rc);
	return rc;
}
#endif
#endif

static int isx005_init_sensor(const struct msm_camera_sensor_info *data)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int rc;
	int nNum = 0;
	//LGE_CHANGE[byungsik.choi@lge.com]2010-07-23 6020 patch
	struct task_struct *p;

	rc = data->pdata->camera_power_on();
	if (rc < 0) {
		printk(KERN_ERR "[ERROR]%s:failed to power on!\n", __func__);
		printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d)\n", __func__, rc);
		return rc;
	}

#if (CAMERA_SENSOR == HYNIX_CAMERA_SENSOR)
#if (HYNIX_CAMERA_SENSOR_REV == HYNIX_REV_B)
	isx005_set_registers_by_revision();
#endif
#endif

#if (READ_INIT_DATA_FROM_FILE == 1) 
	isx005_read_init_register_from_file();
	isx005_read_preview_register_from_file();
	isx005_read_snapshot_register_from_file();
#endif

#if (USE_I2C_BURSTMODE == 1)
	init_burst_mode = 1;
#endif

	/*pll register write*/
	rc = isx005_reg_init();
	if (rc < 0) {
		for(nNum = 0; nNum<5; nNum++)
		{
		 msleep(2);
			printk(KERN_ERR "[ERROR]%s:Set initial register error! retry~! \n", __func__);
			rc = isx005_reg_init();
			if(rc < 0)
			{
				nNum++;
				printk(KERN_ERR "[ERROR]%s:Set initial register error!- loop no:%d \n", __func__, nNum);
			}
			else
			{
				printk(KERN_DEBUG"[%s]:Set initial register Success!\n", __func__);
				break;
			}
		}
	}
#if (USE_I2C_BURSTMODE == 1)
	init_burst_mode = 0;
#endif

	//mdelay(16);  // T3+T4
	mdelay(150);
	printk(KERN_ERR "[yt_test]%s:isx005_init_sensor %d\n", __func__, nNum);

	/*tuning register write
	rc = isx005_reg_tuning();
	if (rc < 0) {
		for(nNum = 0; nNum<5 ;nNum++)
		{
		  msleep(2);
			printk(KERN_ERR "[ERROR]%s:Set initial register error! retry~! \n", __func__);
			rc = isx005_reg_tuning();
			if(rc < 0)
			{
				nNum++;
				printk(KERN_ERR "[ERROR]%s:Set tuning register error! loop no:%d\n", __func__, nNum);
			}
			else
			{
				printk(KERN_DEBUG"[%s]:Set initial tuning Success!\n", __func__);
				break;
			}
		}
	
	}
	*/
	//LGE_CAHNGE_S[byungsik.choi@lge.com]2010-07-23 6020 patch
	p = kthread_run(isx005_reg_tuning, 0, "reg_tuning");

	if (IS_ERR(p)) {
		printk(KERN_ERR "[smiledice] << %s END : FAIL (PTR_ERR(p))\n", __func__);
		return PTR_ERR(p);

	}
	//LGE_CAHNGE_S[byungsik.choi@lge.com]
	
	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
	return rc;
}

//[2010.12.28][JKCHAE] Sensor Porting
#if (READ_INIT_DATA_FROM_FILE == 1) 

#define INIT_REG_SIZE		8000
#define PREVIEW_REG_SIZE	1000
#define SNAPSHOT_REG_SIZE	1000
#define TUNING_REG_SIZE		8000

static struct isx005_register_address_value_pair
	init_register[INIT_REG_SIZE];
static struct isx005_register_address_value_pair
	preview_register[PREVIEW_REG_SIZE];
static struct isx005_register_address_value_pair
	snapshot_register[SNAPSHOT_REG_SIZE];
static struct isx005_register_address_value_pair
	tuning_register[TUNING_REG_SIZE];

#define BUF_SIZE	(256 * 1024)

static void parsing_init_register(char* buf, int buf_size)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int i = 0;
	int reg_count = 0;
	
	int addr, value, length;
	int rc;

	char scan_buf[32];
	int scan_buf_len;

	while (i < buf_size && reg_count < INIT_REG_SIZE) {
		if (buf[i] == '{') {
			scan_buf_len = 0;
			while(buf[i] != '}' && scan_buf_len < 30) {
				if (buf[i] < 33 || 126 < buf[i]) {
					++i;
					continue;
				} else
					scan_buf[scan_buf_len++] = buf[i++];
			}
			scan_buf[scan_buf_len++] = buf[i];
			scan_buf[scan_buf_len] = 0;

			rc = sscanf(scan_buf, "{%x,%x,%d}", &addr, &value, &length);
			if (rc != 3) {
				printk(KERN_ERR "file format error. rc = %d\n", rc);
				printk(KERN_ERR "[smiledice] [CHECK] scan_buf: (%s)\n", scan_buf);
				printk(KERN_ERR "[smiledice] << %s END (file format error. rc: %d)\n", __func__, rc);
				return;
			}

			#if 0 // only for debug
			printk("init reg[%d] addr = 0x%x, value = 0x%x, length = %d\n", 
				reg_count, addr, value, length);
			#endif

			init_register[reg_count].register_address = addr;
			init_register[reg_count].register_value = value;
			init_register[reg_count].register_length= length;
			++reg_count;
		}
		++i;
	}

	if (reg_count > 0) {
		isx005_regs.init_reg_settings = init_register;
		isx005_regs.init_reg_settings_size = reg_count;
	}
	printk(KERN_ERR "[smiledice] << [CHECK] Total Reg Count: %d\n", reg_count);
	printk(KERN_ERR "[smiledice] << %s END\n", __func__);
}

static void parsing_preview_register(char* buf, int buf_size)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int i = 0;
	int reg_count = 0;
	
	int addr, value, length;
	int rc;

	char scan_buf[32];
	int scan_buf_len;

	while (i < buf_size && reg_count < PREVIEW_REG_SIZE) {
		if (buf[i] == '{') {
			scan_buf_len = 0;
			while(buf[i] != '}' && scan_buf_len < 30) {
				if (buf[i] < 33 || 126 < buf[i]) {
					++i;
					continue;
				} else
					scan_buf[scan_buf_len++] = buf[i++];
			}
			scan_buf[scan_buf_len++] = buf[i];
			scan_buf[scan_buf_len] = 0;

			rc = sscanf(scan_buf, "{%x,%x,%d}", &addr, &value, &length);
			if (rc != 3) {
				printk(KERN_ERR "file format error. rc = %d\n", rc);
				printk(KERN_ERR "[smiledice] [CHECK] scan_buf: (%s)\n", scan_buf);
				printk(KERN_ERR "[smiledice] << %s END (file format error. rc: %d)\n", __func__, rc);
				return;
			}

			#if 0 // only for debug
			printk("init reg[%d] addr = 0x%x, value = 0x%x, length = %d\n", 
				reg_count, addr, value, length);
			#endif

			preview_register[reg_count].register_address = addr;
			preview_register[reg_count].register_value = value;
			preview_register[reg_count].register_length= length;
			++reg_count;
		}
		++i;
	}

	if (reg_count > 0) {
		isx005_regs.prev_reg_settings = preview_register;
		isx005_regs.prev_reg_settings_size = reg_count;
	}
	printk(KERN_ERR "[smiledice] << [CHECK] Total Reg Count: %d\n", reg_count);
	printk(KERN_ERR "[smiledice] << %s END\n", __func__);
}

static void parsing_snapshot_register(char* buf, int buf_size)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int i = 0;
	int reg_count = 0;
	
	int addr, value, length;
	int rc;

	char scan_buf[32];
	int scan_buf_len;

	while (i < buf_size && reg_count < SNAPSHOT_REG_SIZE) {
		if (buf[i] == '{') {
			scan_buf_len = 0;
			while(buf[i] != '}' && scan_buf_len < 30) {
				if (buf[i] < 33 || 126 < buf[i]) {
					++i;
					continue;
				} else
					scan_buf[scan_buf_len++] = buf[i++];
			}
			scan_buf[scan_buf_len++] = buf[i];
			scan_buf[scan_buf_len] = 0;

			rc = sscanf(scan_buf, "{%x,%x,%d}", &addr, &value, &length);
			if (rc != 3) {
				printk(KERN_ERR "file format error. rc = %d\n", rc);
				printk(KERN_ERR "[smiledice] [CHECK] scan_buf: (%s)\n", scan_buf);
				printk(KERN_ERR "[smiledice] << %s END (file format error. rc: %d)\n", __func__, rc);
				return;
			}

			#if 0 // only for debug
			printk("init reg[%d] addr = 0x%x, value = 0x%x, length = %d\n", 
				reg_count, addr, value, length);
			#endif

			snapshot_register[reg_count].register_address = addr;
			snapshot_register[reg_count].register_value = value;
			snapshot_register[reg_count].register_length= length;
			++reg_count;
		}
		++i;
	}

	if (reg_count > 0) {
		isx005_regs.snap_reg_settings = snapshot_register;
		isx005_regs.snap_reg_settings_size = reg_count;
	}
	printk(KERN_ERR "[smiledice] << [CHECK] Total Reg Count: %d\n", reg_count);
	printk(KERN_ERR "[smiledice] << %s END\n", __func__);
}

static void parsing_tuning_register(char* buf, int buf_size)
{
	printk(KERN_ERR "[smiledice] >> %s start \n", __func__);
	
	int i = 0;
	int reg_count = 0;
	
	int addr, value, length;
	int rc;

	char scan_buf[32];
	int scan_buf_len;

	while (i < buf_size && reg_count < TUNING_REG_SIZE) {
		if (buf[i] == '{') {
			scan_buf_len = 0;
			while(buf[i] != '}' && scan_buf_len < 30) {
				if (buf[i] < 33 || 126 < buf[i]) {
					++i;
					continue;
				} else
					scan_buf[scan_buf_len++] = buf[i++];
			}
			scan_buf[scan_buf_len++] = buf[i];
			scan_buf[scan_buf_len] = 0;
			
			rc = sscanf(scan_buf, "{%x,%x,%d}", &addr, &value, &length);
			if (rc != 3) {
				printk(KERN_ERR "file format error. rc = %d\n", rc);
				printk(KERN_ERR "[smiledice] << %s end (file format error. rc = %d)\n", __func__, rc);
				return;
			}
			printk("tuning reg[%d] addr = 0x%x, value = 0x%x, length = %d\n",
				reg_count, addr, value, length);

			tuning_register[reg_count].register_address = addr;
			tuning_register[reg_count].register_value = value;
			tuning_register[reg_count].register_length= length;
			++reg_count;
		}
		++i;
	}

	if (reg_count > 0) {
		isx005_regs.tuning_reg_settings = tuning_register;
		isx005_regs.tuning_reg_settings_size = reg_count;
	}
	printk(KERN_ERR "[smiledice] << %s end\n", __func__);
}

static void isx005_read_init_register_from_file()
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int fd;
	mm_segment_t oldfs;
	char* buf;
	int read_size;

	oldfs = get_fs();
	set_fs(get_ds());

	fd = sys_open("/sdcard/init_register", O_RDONLY, 0644);
	if (fd < 0) {
		printk(KERN_ERR "File open fail\n");
		printk(KERN_ERR "[smiledice] << %s END (File open fail)\n", __func__);
		return;
	}

	buf = kmalloc(BUF_SIZE, GFP_KERNEL);
	if (!buf) {
		printk(KERN_ERR "Memory alloc fail\n");
		printk(KERN_ERR "[smiledice] << %s END (Memory alloc fail)\n", __func__);
		return;
	}

	read_size = sys_read(fd, buf, BUF_SIZE);
	if (read_size < 0) {
		printk(KERN_ERR "File read fail: read_size = %d\n", read_size);
		printk(KERN_ERR "[smiledice] << %s END (File read fail: read_size = %d)\n", __func__, read_size);
		return;
	}
	
	parsing_init_register(buf, read_size);

	kfree(buf);

	sys_close(fd);

	set_fs(oldfs);

	printk(KERN_ERR "[smiledice] << %s END\n", __func__);
}

static void isx005_read_preview_register_from_file()
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int fd;
	mm_segment_t oldfs;
	char* buf;
	int read_size;

	oldfs = get_fs();
	set_fs(get_ds());

	fd = sys_open("/sdcard/preview_register", O_RDONLY, 0644);
	if (fd < 0) {
		printk(KERN_ERR "File open fail\n");
		printk(KERN_ERR "[smiledice] << %s END (File open fail)\n", __func__);
		return;
	}

	buf = kmalloc(BUF_SIZE, GFP_KERNEL);
	if (!buf) {
		printk(KERN_ERR "Memory alloc fail\n");
		printk(KERN_ERR "[smiledice] << %s END (Memory alloc fail)\n", __func__);
		return;
	}

	read_size = sys_read(fd, buf, BUF_SIZE);
	if (read_size < 0) {
		printk(KERN_ERR "File read fail: read_size = %d\n", read_size);
		printk(KERN_ERR "[smiledice] << %s END (File read fail: read_size = %d)\n", __func__, read_size);
		return;
	}
	
	parsing_preview_register(buf, read_size);

	kfree(buf);

	sys_close(fd);

	set_fs(oldfs);

	printk(KERN_ERR "[smiledice] << %s END\n", __func__);
}

static void isx005_read_snapshot_register_from_file()
{
	printk(KERN_ERR "[smiledice] >> %s start \n", __func__);
	
	int fd;
	mm_segment_t oldfs;
	char* buf;
	int read_size;

	oldfs = get_fs();
	set_fs(get_ds());

	fd = sys_open("/sdcard/snapshot_register", O_RDONLY, 0644);
	if (fd < 0) {
		printk(KERN_ERR "file open fail\n");
		printk(KERN_ERR "[smiledice] << %s end (file open fail)\n", __func__);
		return;
	}

	buf = kmalloc(BUF_SIZE, GFP_KERNEL);
	if (!buf) {
		printk(KERN_ERR "memory alloc fail\n");
		printk(KERN_ERR "[smiledice] << %s end (memory alloc fail)\n", __func__);
		return;
	}

	read_size = sys_read(fd, buf, BUF_SIZE);
	if (read_size < 0) {
		printk(KERN_ERR "file read fail: read_size = %d\n", read_size);
		printk(KERN_ERR "[smiledice] << %s end (file read fail: read_size = %d)\n", __func__, read_size);
		return;
	}
	
	parsing_snapshot_register(buf, read_size);

	kfree(buf);

	sys_close(fd);

	set_fs(oldfs);

	printk(KERN_ERR "[smiledice] << %s end\n", __func__);
}

static void isx005_read_tuning_register_from_file()
{
	printk(KERN_ERR "[smiledice] >> %s start \n", __func__);
	int fd;
	mm_segment_t oldfs;
	char* buf;
	int read_size;

	oldfs = get_fs();
	set_fs(get_ds());

	fd = sys_open("/sdcard/tuning_register", O_RDONLY, 0644);
	if (fd < 0) {
		printk(KERN_ERR "file open fail\n");
		printk(KERN_ERR "[smiledice] << %s end (file open fail)\n", __func__);
		return;
	}

	buf = kmalloc(BUF_SIZE, GFP_KERNEL);
	if (!buf) {
		printk(KERN_ERR "memory alloc fail\n");
		printk(KERN_ERR "[smiledice] << %s end (memory alloc fail)\n", __func__);
		return;
	}

	read_size = sys_read(fd, buf, BUF_SIZE);
	if (read_size < 0) {
		printk(KERN_ERR "file read fail: read_size = %d\n", read_size);
		printk(KERN_ERR "[smiledice] << %s end (file read fail: read_size = %d)\n", __func__, read_size);
		return;
	}
	
	parsing_tuning_register(buf, read_size);

	kfree(buf);

	sys_close(fd);

	set_fs(oldfs);
	printk(KERN_ERR "[smiledice] << %s end\n", __func__);
}
#endif


static int isx005_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	printk(KERN_ERR "[smiledice] >> %s start \n", __func__);
	
	int rc = 0;

	printk(KERN_ERR "init entry \n");

	if (data == 0) {
		printk(KERN_ERR "[error]%s: data is null!\n", __func__);
		printk(KERN_ERR "[smiledice] << %s end : fail#1 (-1)\n", __func__);
		return -1;
	}

	//[2010.12.28][jkchae] sensor porting
#if 0
#if (READ_INIT_DATA_FROM_FILE == 1) 
	isx005_read_init_register_from_file();
	#if 0
	isx005_read_tuning_register_from_file();
	#endif
#endif
#endif

#if defined(CONFIG_MACH_MSM7X27_THUNDERG) || \
	defined(CONFIG_MACH_MSM7X27_THUNDERC) || \
	defined(CONFIG_MACH_MSM7X27_GELATO)
	/* LGE_CHANGE_S. Change code to apply new LUT for display quality.
	 * 2010-08-13. minjong.gong@lge.com */

	#if (MDP_REMOVAL == 1)
	#else
		mdp_load_thunder_lut(2);	/* Camera LUT */
	#endif
#endif
	rc = isx005_init_sensor(data);
	if (rc < 0) {
		printk(KERN_ERR "[ERROR]%s:failed to initialize sensor!\n", __func__);
		goto init_probe_fail;
	}
	//LGE_CHANGE[byungsik.choi@lge.com]2010-07-23 6020 patch
	tuning_thread_run = 0;
	cfg_wq = 0;

	prev_af_mode = -1;
	prev_scene_mode = -1;
    printk(KERN_ERR "[yt_test]%s:isx005_sensor_init_probe \n", __func__);
	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
	return rc;

init_probe_fail:
	printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
	return rc;
}

int isx005_sensor_init(const struct msm_camera_sensor_info *data)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int rc = 0;

	isx005_ctrl = kzalloc(sizeof(struct isx005_ctrl), GFP_KERNEL);
	if (!isx005_ctrl) {
		printk(KERN_ERR "[ERROR]%s:isx005_init failed!\n", __func__);
		rc = -ENOMEM;
		goto init_done;
	}


#if (USE_I2C_BURSTMODE == 1)
	sensor_burst_buffer = kzalloc(sensor_burst_size*2, GFP_KERNEL);
	if (!sensor_burst_buffer) {
		printk(KERN_ERR "[ERROR]%s:sensor_burst_buffer alloc failed!\n", __func__);
		rc = -ENOMEM;
		goto init_done;
	}
#endif

	if (data)
		isx005_ctrl->sensordata = data;

	rc = isx005_sensor_init_probe(data);
	if (rc < 0) {
		printk(KERN_ERR "[ERROR]%s:isx005_sensor_init failed!\n", __func__);
		goto init_fail;
	}

init_done:
	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
	return rc;

init_fail:
	kfree(isx005_ctrl);
#if (USE_I2C_BURSTMODE == 1)
	kfree(sensor_burst_buffer);
#endif
	printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d)\n", __func__, rc);
	return rc;
}

int isx005_sensor_release(void)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	int rc = 0;
/* LGE_CHANGE_S [youngki.an@lge.com] 2010-05-18 */
#if 1//def LG_CAMERA_HIDDEN_MENU
	if(sensorAlwaysOnTest ==true)
	{
		printk(KERN_ERR "==========================isx005_sensor_not_release sensorAlwaysOnTest is true ==========================");
		printk("==========================isx005_sensor_not_release sensorAlwaysOnTest is true ==========================");
		
		printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

		return 0;
	}
	else{
		printk(KERN_ERR "==========================isx005_sensor_release sensorAlwaysOnTest is false ==========================");
		printk("==========================isx005_sensor_release sensorAlwaysOnTest is false ==========================");
	}
#endif
/* LGE_CHANGE_E [youngki.an@lge.com] 2010-05-18 */
		
	mutex_lock(&isx005_mutex);

	rc = isx005_ctrl->sensordata->pdata->camera_power_off();

	kfree(isx005_ctrl);

#if (USE_I2C_BURSTMODE == 1)
	kfree(sensor_burst_buffer);
#endif

	mutex_unlock(&isx005_mutex);

#if defined(CONFIG_MACH_MSM7X27_THUNDERG) || \
	defined(CONFIG_MACH_MSM7X27_THUNDERC) || \
	defined(CONFIG_MACH_MSM7X27_GELATO)
	/* LGE_CHANGE_S. Change code to apply new LUT for display quality.
	 * 2010-08-13. minjong.gong@lge.com */
	 
	#if (MDP_REMOVAL == 1)
	#else
		mdp_load_thunder_lut(1);	/* Normal LUT */
	#endif
	
#endif

	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

	return rc;
}
	//LGE_CHANGE_S[byungsik.choi@lge.com]2010-07-23 6020 patch

	
static int dequeue_sensor_config(int cfgtype, int mode)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	int rc;
	
	switch (cfgtype) {
		case CFG_SET_MODE:
			printk(KERN_ERR "[smiledice] case CFG_SET_MODE \n");
			rc = isx005_set_sensor_mode(mode);
			break;

		case CFG_SET_EFFECT:
			printk(KERN_ERR "[smiledice] case CFG_SET_EFFECT \n");
			rc = isx005_set_effect(mode);
			break;

		case CFG_MOVE_FOCUS:
			printk(KERN_ERR "[smiledice] case CFG_MOVE_FOCUS \n");
			rc = isx005_move_focus(mode);
			break;

		case CFG_SET_DEFAULT_FOCUS:
			printk(KERN_ERR "[smiledice] case CFG_SET_DEFAULT_FOCUS \n");
			rc = isx005_set_default_focus();
			break;

		case CFG_SET_WB:
			printk(KERN_ERR "[smiledice] case CFG_SET_WB \n");
			rc = isx005_set_wb(mode);
			break;

		case CFG_SET_ANTIBANDING:
			printk(KERN_ERR "[smiledice] case CFG_SET_ANTIBANDING \n");
			rc= isx005_set_antibanding(mode);
			break;

		case CFG_SET_ISO:
			printk(KERN_ERR "[smiledice] case CFG_SET_ISO \n");
			rc = isx005_set_iso(mode);
			break;

		case CFG_SET_SCENE:
			printk(KERN_ERR "[smiledice] case CFG_SET_SCENE \n");
			rc = isx005_set_scene_mode(mode);
			break;

		case CFG_SET_BRIGHTNESS:
			printk(KERN_ERR "[smiledice] case CFG_SET_BRIGHTNESS \n");
			rc = isx005_set_brightness(mode);
			break;

		//LGE_CHANGE_S CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)
		case CFG_SET_FRAMERATE:
			printk(KERN_ERR "[smiledice] case CFG_SET_FRAMERATE\n");
			rc = isx005_set_frame_rate(mode);
			break;

		case CFG_SET_FPS_RANGE:
			printk(KERN_ERR "[smiledice] case CFG_SET_FPS_RANGE\n");
			rc = isx005_set_fps_range(mode);
			break;
		//LGE_CHANGE_E CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)
			
		default:
			printk(KERN_ERR "[smiledice] [CHECK] unhandled enqued config: %d\n", cfgtype );
			rc = 0;
			break;
	}
	
	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
	return rc;
}
	//LGE_CHANGE_E[byungsik.choi@lge.com]2010-07-23 6020 patch

int isx005_sensor_config(void __user *argp)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	
	struct sensor_cfg_data cfg_data;
	int rc;

	rc = copy_from_user(&cfg_data, (void *)argp,
		sizeof(struct sensor_cfg_data));

	if (rc < 0) {
		
		printk(KERN_ERR "[smiledice] << %s END : FAIL (-EFAULT)\n", __func__);
		return -EFAULT;
	}

	printk(KERN_ERR "isx005_ioctl, cfgtype = %d, mode = %d\n",
		cfg_data.cfgtype, cfg_data.mode);
	
	//LGE_CHANGE_S[byungsik.choi@lge.com]2010-07-23 6020 patch
	mutex_lock(&isx005_tuning_mutex);
	if (tuning_thread_run) {

#if 1  //LGE_CHANGE_S CAMERA_FIRMWARE_UPDATE
		if (cfg_data.cfgtype == CFG_GET_AF_MAX_STEPS) {
			printk(KERN_ERR "[smiledice] case CFG_GET_AF_MAX_STEPS \n");
			cfg_data.max_steps = ISX005_TOTAL_STEPS_NEAR_TO_FAR;
			if (copy_to_user((void *)argp,
					&cfg_data,
					sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;

			mutex_unlock(&isx005_tuning_mutex);

			printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
			return rc;
		}
#endif //LGE_CHANGE_E CAMERA_FIRMWARE_UPDATE

		if (cfg_data.cfgtype == CFG_MOVE_FOCUS)
			cfg_data.mode = cfg_data.cfg.focus.steps;

		//LGE_CHANGE_S CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)
		else if (cfg_data.cfgtype == CFG_SET_FRAMERATE)
			cfg_data.mode = cfg_data.cfg.p_fps;
		//LGE_CHANGE_E CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)

		enqueue_cfg_wq(cfg_data.cfgtype, cfg_data.mode);
		mutex_unlock(&isx005_tuning_mutex);

		printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
		return rc;
	}
	mutex_unlock(&isx005_tuning_mutex);
	//LGE_CHANGE_E[byungsik.choi@lge.com]2010-07-23 
	
	mutex_lock(&isx005_mutex);

	switch (cfg_data.cfgtype) {
		case CFG_SET_MODE:
			printk(KERN_ERR "[smiledice] case CFG_SET_MODE \n");
			rc = isx005_set_sensor_mode(cfg_data.mode);
			break;

		case CFG_SET_EFFECT:
			printk(KERN_ERR "[smiledice] case CFG_SET_EFFECT \n");
			rc = isx005_set_effect(cfg_data.mode);
			break;

		case CFG_MOVE_FOCUS:
			printk(KERN_ERR "[smiledice] case CFG_MOVE_FOCUS \n");
			rc = isx005_move_focus(cfg_data.cfg.focus.steps);
			break;

		case CFG_SET_DEFAULT_FOCUS:
			printk(KERN_ERR "[smiledice] case CFG_SET_DEFAULT_FOCUS \n");
			rc = isx005_set_default_focus();
			break;

		//LGE_CHANGE_S CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)
		case CFG_GET_ISO:
			{
				int isoValue = 0;	
				printk(KERN_ERR "[smiledice] case CFG_GET_ISO \n");
				rc = isx005_get_iso(&isoValue);
				if (rc < 0) {
					printk(KERN_ERR "[smiledice] isx005_get_iso_value() failed!\n");
				}

				cfg_data.rs = isoValue;
				if (copy_to_user((void*)argp, &cfg_data, sizeof(struct sensor_cfg_data))) {
					rc = -EFAULT;
				}
			}
			break;
		case CFG_GET_FOCUS_DISTANCE:
			{
				int focusDistances = 0;	
				printk(KERN_ERR "[smiledice] case CFG_GET_FOCUS_DISTANCE \n");
				rc = isx005_get_focus_distance(&focusDistances);
				if (rc < 0) {
					printk(KERN_ERR "[smiledice] isx005_get_iso_value() failed!\n");
				}

				cfg_data.rs = focusDistances;
				if (copy_to_user((void*)argp, &cfg_data, sizeof(struct sensor_cfg_data))) {
					rc = -EFAULT;
				}
			}
			break;
		//LGE_CHANGE_E CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)

		case CFG_GET_AF_MAX_STEPS:
			printk(KERN_ERR "[smiledice] case CFG_GET_AF_MAX_STEPS \n");
			cfg_data.max_steps = ISX005_TOTAL_STEPS_NEAR_TO_FAR;
			if (copy_to_user((void *)argp,
					&cfg_data,
					sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_START_AF_FOCUS:
			printk(KERN_ERR "[smiledice] case CFG_START_AF_FOCUS \n");
			rc = isx005_set_af_start(cfg_data.mode);
			break;

		case CFG_CHECK_AF_DONE:
			printk(KERN_ERR "[smiledice] case CFG_CHECK_AF_DONE \n");
			rc = isx005_check_focus(&cfg_data.mode);
			if (copy_to_user((void *)argp,
					&cfg_data,
					sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_CHECK_AF_CANCEL:
			printk(KERN_ERR "[smiledice] case CFG_CHECK_AF_CANCEL \n");
			rc = isx005_cancel_focus(cfg_data.mode);
			break;

		case CFG_SET_WB:
			printk(KERN_ERR "[smiledice] case CFG_SET_WB \n");
			rc = isx005_set_wb(cfg_data.mode);
			break;

		case CFG_SET_ANTIBANDING:
			printk(KERN_ERR "[smiledice] case CFG_SET_ANTIBANDING \n");
			rc= isx005_set_antibanding(cfg_data.mode);
			break;

		case CFG_SET_ISO:
			printk(KERN_ERR "[smiledice] case CFG_SET_ISO \n");
			rc = isx005_set_iso(cfg_data.mode);
			break;

		case CFG_SET_SCENE:
			printk(KERN_ERR "[smiledice] case CFG_SET_SCENE \n");
			rc = isx005_set_scene_mode(cfg_data.mode);
			break;

		case CFG_SET_BRIGHTNESS:
			printk(KERN_ERR "[smiledice] case CFG_SET_BRIGHTNESS \n");
			rc = isx005_set_brightness(cfg_data.mode);
			break;

		//LGE_CHANGE_S CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)
		case CFG_SET_FRAMERATE:
			printk(KERN_ERR "[smiledice] case CFG_SET_FRAMERATE\n");
			rc = isx005_set_frame_rate(cfg_data.cfg.p_fps);
			break;

		case CFG_SET_FPS_RANGE:
			printk(KERN_ERR "[smiledice] case CFG_SET_FPS_RANGE\n");
			rc = isx005_set_fps_range(cfg_data.mode);
			break;
		//LGE_CHANGE_E CAMERA FIRMWARE UPDATE (jongkwon.chae@lge.com)

		default:
			rc = -EINVAL;
			break;
	}

	mutex_unlock(&isx005_mutex);

	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

	return rc;
}

static const struct i2c_device_id isx005_i2c_id[] = {
	{ "isx005", 0},
	{ },
};

static int isx005_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&isx005_wait_queue);
	return 0;
}

static int isx005_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	isx005_sensorw = kzalloc(sizeof(struct isx005_work), GFP_KERNEL);
	if (!isx005_sensorw) {
		printk(KERN_ERR "kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, isx005_sensorw);
	isx005_init_client(client);
	isx005_client = client;

	printk(KERN_ERR "isx005_probe succeeded!\n");

	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);

	return rc;

probe_failure:
	printk(KERN_ERR "[ERROR]%s:isx005_probe failed!\n", __func__);
	printk(KERN_ERR "[smiledice] << %s END : FAIL (rc = %d\n", __func__, rc);
	return rc;
}

static struct i2c_driver isx005_i2c_driver = {
	.id_table = isx005_i2c_id,
	.probe  = isx005_i2c_probe,
	.remove = __exit_p(isx005_i2c_remove),
	.driver = {
		.name = "isx005",
	},
};

static ssize_t mclk_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("mclk_rate = %d\n", mclk_rate);
	return 0;
}

static ssize_t mclk_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int value;

	sscanf(buf, "%d", &value);
	mclk_rate = value;

	printk("mclk_rate = %d\n", mclk_rate);
	return size;
}

//static DEVICE_ATTR(mclk,  S_IRWXUGO, mclk_show, mclk_store);
static DEVICE_ATTR(mclk, (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH), mclk_show, mclk_store);

static ssize_t always_on_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("always_on = %d\n", always_on);
	return 0;
}

static ssize_t always_on_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int value;

	sscanf(buf, "%d", &value);
	always_on = value;

	printk("always_on = %d\n", always_on);
	return size;
}

//static DEVICE_ATTR(always_on, S_IRWXUGO, always_on_show, always_on_store);
static DEVICE_ATTR(always_on, (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH), always_on_show, always_on_store);

static int isx005_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	printk(KERN_ERR "[smiledice] >> %s START \n", __func__);
	printk(KERN_ERR "[smiledice] [CHECK] i2c_add_driver (isx005_i2c_driver.probe: %d)\n", isx005_i2c_driver.probe);
	int rc = i2c_add_driver(&isx005_i2c_driver);
	if (rc < 0) {
		printk(KERN_ERR "[smiledice] [CHECK] i2c_add_driver failed! (rc: %d) \n", rc);
		rc = -ENOTSUPP;
		goto probe_done;
	}

	if (isx005_client == NULL) {
		printk(KERN_ERR "[smiledice] [CHECK] isx005_client is null !!\n");
		rc = -ENOTSUPP;
		goto probe_done;
	}

	s->s_init = isx005_sensor_init;
	s->s_release = isx005_sensor_release;
	s->s_config  = isx005_sensor_config;
	// LGE_CHANGE_S 2011.01.26 [jongkwon.chae@lge.com] [gelato] sensor porting
	s->s_mount_angle = 0;
	// LGE_CHANGE_E

	rc = device_create_file(&isx005_pdev->dev, &dev_attr_mclk);
	if (rc < 0) {
		printk("device_create_file error!\n");
		printk(KERN_ERR "[smiledice] << %s END : FAIL#1 (rc = %d\n", __func__, rc);
		return rc;
	}

	rc = device_create_file(&isx005_pdev->dev, &dev_attr_always_on);
	if (rc < 0) {
		printk("device_create_file error!\n");
		printk(KERN_ERR "[smiledice] << %s END : FAIL#2 (rc = %d\n", __func__, rc);
		return rc;
	}

probe_done:
	printk(KERN_ERR "%s %s:%d\n", __FILE__, __func__, __LINE__);

	printk(KERN_ERR "[smiledice] << %s END : rc = %d\n", __func__, rc);
	return rc;
}

static int __isx005_probe(struct platform_device *pdev)
{
	isx005_pdev = pdev;
	return msm_camera_drv_start(pdev, isx005_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __isx005_probe,
	.driver = {
		.name = "msm_camera_isx005",
		.owner = THIS_MODULE,
	},
};

static int __init isx005_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

late_initcall(isx005_init);
