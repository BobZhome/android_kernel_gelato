/* arch/arm/mach-msm/lge/lg_fw_diag_icd.c
 *
 * Copyright (C) 2009,2010 LGE, Inc.
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

#include <linux/module.h>
#include <mach/lge_diagcmd.h>
#include <mach/lge_diag_icd.h>
#include <mach/lge_base64.h>
#include <mach/lge_pcb_version.h>
#include "lge_diag_communication.h"

#include <linux/unistd.h>	/*for open/close */
#include <linux/fcntl.h>	/*for O_RDWR */

#include <linux/fb.h>		/* to handle framebuffer ioctls */
#include <linux/ioctl.h>
#include <linux/uaccess.h>

#include <linux/syscalls.h>	//for sys operations

#include <linux/input.h>	// for input_event
#include <linux/fs.h>		// for file struct
#include <linux/types.h>	// for ssize_t
#include <linux/input.h>	// for event parameters
#include <linux/jiffies.h>
#include <linux/delay.h>

/*
 * EXTERNAL FUNCTION AND VARIABLE DEFINITIONS
 */
extern PACK(void *) diagpkt_alloc(diagpkt_cmd_code_type code,unsigned int length);
extern PACK(void *) diagpkt_free(PACK(void *)pkt);

extern void icd_send_to_arm9(void* pReq, void* pRsp, unsigned int output_length);

extern icd_user_table_entry_type icd_mstr_tbl[ICD_MSTR_TBL_SIZE];

extern int lge_bd_rev;

/*
 * LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE
 *
 * This section contains local definitions for constants, macros, types,
 * variables and other items needed by this module.
 */
static struct diagcmd_dev *diagpdev;

char process_status[10];
EXPORT_SYMBOL(process_status);

char process_value[100];
EXPORT_SYMBOL(process_value);

// LGE_CHANGE_S [myeonggyu.son@lge.com] [2011.02.25] [GELATO] enable or disable key logging status of slate [START]
#ifdef CONFIG_LGE_DIAG
int key_touch_logging_status = 0;
EXPORT_SYMBOL(key_touch_logging_status);
#endif
// LGE_CHANGE_E [myeonggyu.son@lge.com] [2011.02.25] [GELATO] enable or disable key logging status of slate [END]

/*
 * INTERNAL FUNCTION DEFINITIONS
 */
PACK(void *) LGE_ICDProcess(PACK(void *)req_pkt_ptr,	/* pointer to request packet  */
			    		unsigned short pkt_len			/* length of request packet   */)
{
	DIAG_ICD_F_req_type *req_ptr = (DIAG_ICD_F_req_type *) req_pkt_ptr;
	DIAG_ICD_F_rsp_type *rsp_ptr = NULL;
	icd_func_type func_ptr = NULL;
	int is_valid_arm9_command = 1;
	unsigned int rsp_ptr_len;

	int nIndex = 0;

	diagpdev = diagcmd_get_dev();

	for (nIndex = 0; nIndex < ICD_MSTR_TBL_SIZE; nIndex++) 
	{
		if(icd_mstr_tbl[nIndex].cmd_code == req_ptr->hdr.sub_cmd) 
		{
			if (icd_mstr_tbl[nIndex].which_procesor == ICD_ARM11_PROCESSOR)
			{
				func_ptr = icd_mstr_tbl[nIndex].func_ptr;
			}
			
			break;
		} 
		else if (icd_mstr_tbl[nIndex].cmd_code == ICD_MAX_REQ_CMD)
		{
			break;
		}
		else
		{
			continue;
		}
	}

	if (func_ptr != NULL) 
	{
		printk(KERN_INFO "[ICD] cmd_code : [0x%X], sub_cmd : [0x%X]\n",req_ptr->hdr.cmd_code, req_ptr->hdr.sub_cmd);
		rsp_ptr = func_ptr((DIAG_ICD_F_req_type *) req_ptr);
	} 
	else
	{
		switch(req_ptr->hdr.sub_cmd) {
			case ICD_GETDEVICEINFO_REQ_CMD:
				rsp_ptr_len = sizeof(icd_device_info_rsp_type);
				break;
			case ICD_GETGPSSTATUS_REQ_CMD:
				rsp_ptr_len = sizeof(icd_get_gps_status_rsp_type);
				break;
			case ICD_SETGPSSTATUS_REQ_CMD:
				rsp_ptr_len = sizeof(icd_set_gps_status_rsp_type);
				break;
			case ICD_GETROAMINGMODE_REQ_CMD:
				rsp_ptr_len = sizeof(icd_get_roamingmode_rsp_type);
				break;
			case ICD_GETSTATEANDCONNECTIONATTEMPTS_REQ_CMD:
				rsp_ptr_len = sizeof(icd_get_state_connect_rsp_type);
				break;
			case ICD_GETBATTERYCHARGINGSTATE_REQ_CMD:
				rsp_ptr_len = sizeof(icd_set_battery_charging_state_rsp_type);
				break;
			case ICD_GETBATTERYLEVEL_REQ_CMD:
				rsp_ptr_len = sizeof(icd_get_battery_level_rsp_type);
				break;
			case ICD_GETRSSI_REQ_CMD:
				rsp_ptr_len = sizeof(icd_get_rssi_rsp_type);
				break;
			case ICD_SETDISCHARGING_REQ_CMD:
				rsp_ptr_len = sizeof(icd_set_discharger_rsp_type);
				break;
			default:
				is_valid_arm9_command = 0;
				printk(KERN_INFO "[ICD] %s : invalid sub command : 0x%x\n",__func__,req_ptr->hdr.sub_cmd);
				break;
		}
		
		if(is_valid_arm9_command == 1)
		{
			rsp_ptr = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_ptr_len);
			if (rsp_ptr == NULL) {
				printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
				return rsp_ptr;
			}
				
			printk(KERN_INFO "[ICD] cmd_code : [0x%X], sub_cmd : [0x%X] --> goto MODEM through oem rapi\n",req_ptr->hdr.cmd_code, req_ptr->hdr.sub_cmd);
			icd_send_to_arm9((void *)req_ptr, (void *)rsp_ptr, rsp_ptr_len);
		}
	}

	return (rsp_ptr);
}
EXPORT_SYMBOL(LGE_ICDProcess);

void* icd_app_handler(DIAG_ICD_F_req_type*pReq)
{
	return NULL;
}

/** SAR : Sprint Automation Requirement - START **/
DIAG_ICD_F_rsp_type *icd_info_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;

	rsp_len = sizeof(icd_device_info_rsp_type);

	printk(KERN_INFO "[ICD] icd_info_req_proc\n");
	printk(KERN_INFO "[ICD] icd_info_req_proc rsp_len :(%d)\n", rsp_len);

	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}

	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_GETDEVICEINFO_REQ_CMD;

	// get manufacture info
	
	// get model name info

	// get hw version info
	
	// get sw version info

	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_extended_info_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;

	rsp_len = sizeof(icd_extended_info_rsp_type);

	printk(KERN_INFO "[ICD] icd_extended_info_req_proc\n");
	printk(KERN_INFO "[ICD] icd_extended_info_req_proc rsp_len :(%d)\n", rsp_len);

	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}

	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_EXTENDEDVERSIONINFO_REQ_CMD;

	// get extended version info
	memset(pRsp->icd_rsp.extended_rsp_info.ver_string, 0x00, sizeof(pRsp->icd_rsp.extended_rsp_info.ver_string));
	if(diagpdev != NULL)
	{
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather\n",__func__);
		update_diagcmd_state(diagpdev, "ICD_GETEXTENDEDVERSION", 1);
		mdelay(50);
		if(strcmp(process_status,"COMPLETED") == 0)
		{
			strcpy(pRsp->icd_rsp.extended_rsp_info.ver_string,process_value);
			printk(KERN_INFO "[ICD] %s was successful\n",__func__);
		}
		else
		{
			strcpy(pRsp->icd_rsp.extended_rsp_info.ver_string,"UNKNOWN");
			printk(KERN_INFO "[ICD] %s was unsuccessful\n",__func__);
		}
	}
	else
	{
		strcpy(pRsp->icd_rsp.extended_rsp_info.ver_string,"UNKNOWN");
		printk(KERN_INFO "[ICD] %s was unsuccessful\n",__func__);
	}
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_handset_disp_text_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;

	rsp_len = sizeof(icd_handset_disp_text_rsp_type);

	printk(KERN_INFO "[ICD] icd_handset_disp_text_req_proc\n");
	printk(KERN_INFO "[ICD] icd_handset_disp_text_req_proc rsp_len :(%d)\n", rsp_len);

	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}

	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_HANDSETDISPLAYTEXT_REQ_CMD;

	// get handset display text
	
	return pRsp;
}

static ssize_t read_framebuffer(byte* pBuf)
{
  	struct file *phMscd_Filp = NULL;
  	ssize_t read_size = 0;

  	mm_segment_t old_fs=get_fs();

  	set_fs(get_ds());

  	phMscd_Filp = filp_open("/dev/graphics/fb0", O_RDONLY |O_LARGEFILE, 0);

  	if( !phMscd_Filp)
    	printk("open fail screen capture \n" );

  	read_size = phMscd_Filp->f_op->read(phMscd_Filp, pBuf, ICD_SCRN_BUF_SIZE_MAX, &phMscd_Filp->f_pos);
  	filp_close(phMscd_Filp,NULL);

  	set_fs(old_fs);

  	return read_size;
}

DIAG_ICD_F_rsp_type *icd_capture_img_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;

	struct fb_var_screeninfo fb_varinfo;
	int fbfd;
	ssize_t bmp_size;

	rsp_len = sizeof(icd_screen_capture_rsp_type);

	printk(KERN_INFO "[ICD] icd_capture_img_req_proc\n");
	printk(KERN_INFO "[ICD] icd_capture_img_req_proc rsp_len :(%d)\n", rsp_len);

	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}

	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_CAPTUREIMAGE_REQ_CMD;

	// get capture images

	return pRsp;
}
/** SAR : Sprint Automation Requirement - END **/

/** ICDR : ICD Implementation Recommendation  - START **/
DIAG_ICD_F_rsp_type *icd_get_airplanemode_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_get_airplane_mode_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_get_airplanemode_req_proc\n");
	printk(KERN_INFO "[ICD] icd_get_airplanemode_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_GETAIRPLANEMODE_REQ_CMD;
	
	// get airplane mode info
	if(diagpdev != NULL)
	{
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather\n",__func__);
		update_diagcmd_state(diagpdev, "ICD_GETAIRPLANEMODE", 1);
		mdelay(50);
		if(strcmp(process_status,"COMPLETED") == 0)
		{
			if(strcmp(process_value,"GETAIRPLANE_0") == 0)
			{
				pRsp->icd_rsp.get_airplane_mode_rsp_info.airplane_mode = 0;
				printk(KERN_INFO "[ICD] %s was successful : airplan mode is on\n",__func__);
			}
			else if(strcmp(process_value,"GETAIRPLANE_1") == 0)
			{
				pRsp->icd_rsp.get_airplane_mode_rsp_info.airplane_mode = 1;
				printk(KERN_INFO "[ICD] %s was successful : airplan mode is off\n",__func__);
			}
			else
			{
				pRsp->icd_rsp.get_airplane_mode_rsp_info.airplane_mode = 0xFF;
				printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
			}
		}
		else
		{
			pRsp->icd_rsp.get_airplane_mode_rsp_info.airplane_mode = 0xFF;
			printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
		}
	}
	else
	{
		pRsp->icd_rsp.get_airplane_mode_rsp_info.airplane_mode = 0xFF;
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather : Error cannot open diagpdev\n",__func__);
	}
		
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_set_airplanemode_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_set_airplane_mode_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_set_airplanemode_req_proc : req mode = %d\n",pReq->icd_req.set_aiplane_mode_req_info.airplane_mode);
	printk(KERN_INFO "[ICD] icd_set_airplanemode_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_SETAIRPLANEMODE_REQ_CMD;
	
	// set airplane mode info
	if(diagpdev != NULL)
	{
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather\n",__func__);
		update_diagcmd_state(diagpdev, "ICD_SETAIRPLANEMODE", pReq->icd_req.set_aiplane_mode_req_info.airplane_mode);
		mdelay(50);

		if(strcmp(process_status,"COMPLETED") == 0)
		{
			if(strcmp(process_value,"SETAIRPLANE_0") == 0)
			{
				pRsp->icd_rsp.set_airplane_mode_rsp_info.cmd_status = 0;
				printk(KERN_INFO "[ICD] %s was successful\n",__func__);
			}
			else if(strcmp(process_value,"SETAIRPLANE_1") == 0)
			{
				pRsp->icd_rsp.set_airplane_mode_rsp_info.cmd_status = 1;
				printk(KERN_INFO "[ICD] %s was unsuccessful\n",__func__);
			}
			else
			{
				pRsp->icd_rsp.set_airplane_mode_rsp_info.cmd_status = 0xFF;
				printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
			}
		}
		else
		{
			pRsp->icd_rsp.set_airplane_mode_rsp_info.cmd_status = 0xFF;
			printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
		}
	}
	else
	{
		pRsp->icd_rsp.set_airplane_mode_rsp_info.cmd_status = 0xFF;
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather : Error cannot open diagpdev\n",__func__);
	}
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_get_backlight_setting_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_get_backlight_setting_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_get_backlight_setting_req_proc\n");
	printk(KERN_INFO "[ICD] icd_get_backlight_setting_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_GETBACKLIGHTSETTING_REQ_CMD;
	
	// get backlight setting info
	if(diagpdev != NULL)
	{
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather\n",__func__);
		update_diagcmd_state(diagpdev, "ICD_GETBACKLIGHTSETTING", 1);
		mdelay(50);

		if(strcmp(process_status,"COMPLETED") == 0)
		{
			if(strcmp(process_value,"GETBACKLIGHT_15") == 0)
			{
				pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data = 15;
				printk(KERN_INFO "[ICD] %s was successful : %dsec\n",__func__,pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data);
			}
			else if(strcmp(process_value,"GETBACKLIGHT_30") == 0)
			{
				pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data = 30;
				printk(KERN_INFO "[ICD] %s was successful : %dsec\n",__func__,pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data);
			}
			else if(strcmp(process_value,"GETBACKLIGHT_60") == 0)
			{
				pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data = 60;
				printk(KERN_INFO "[ICD] %s was successful : %dsec\n",__func__,pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data);
			}
			else if(strcmp(process_value,"GETBACKLIGHT_120") == 0)
			{
				pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data = 120;
				printk(KERN_INFO "[ICD] %s was successful : %dsec\n",__func__,pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data);
			}
			else if(strcmp(process_value,"GETBACKLIGHT_600") == 0)
			{
				pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data = 600;
				printk(KERN_INFO "[ICD] %s was successful : %dsec\n",__func__,pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data);
			}
			else if(strcmp(process_value,"GETBACKLIGHT_1800") == 0)
			{
				pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data = 1800;
				printk(KERN_INFO "[ICD] %s was successful : %dsec\n",__func__,pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data);
			}
			else if(strcmp(process_value,"GETBACKLIGHT_ALWAY") == 0)
			{
				pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data = 100;
				printk(KERN_INFO "[ICD] %s was successful : %dsec\n",__func__,pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data);
			}
			else
			{
				pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data = 0x00;
				printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
			}
		}
		else
		{
			pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data = 0x00;
			printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
		}
	}
	else
	{
		pRsp->icd_rsp.get_backlight_setting_rsp_info.item_data = 0x00;
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather : Error cannot open diagpdev\n",__func__);
	}
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_set_backlight_setting_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_set_backlight_setting_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_set_backlight_setting_req_proc, req = %d\n",pReq->icd_req.set_backlight_setting_req_info.item_data);
	printk(KERN_INFO "[ICD] icd_set_backlight_setting_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_SETBACKLIGHTSETTING_REQ_CMD;
	
	// set backlight setting info
	if(diagpdev != NULL)
	{
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather\n",__func__);
		update_diagcmd_state(diagpdev, "ICD_SETBACKLIGHTSETTING", pReq->icd_req.set_backlight_setting_req_info.item_data);
		mdelay(50);

		if(strcmp(process_status,"COMPLETED") == 0)
		{
			if(strcmp(process_value,"SETBACKLIGHT_0") == 0)
			{
				pRsp->icd_rsp.set_backlight_setting_rsp_info.cmd_status = 0;
				printk(KERN_INFO "[ICD] %s was successful\n",__func__);
			}
			else if(strcmp(process_value,"SETBACKLIGHT_1") == 0)
			{
				pRsp->icd_rsp.set_backlight_setting_rsp_info.cmd_status = 1;
				printk(KERN_INFO "[ICD] %s was unsuccessful\n",__func__);
			}
			else
			{
				pRsp->icd_rsp.set_backlight_setting_rsp_info.cmd_status = 0xFF;
				printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
			}
		}
		else
		{
			pRsp->icd_rsp.set_backlight_setting_rsp_info.cmd_status = 0xFF;
			printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
		}
	}
	else
	{
		pRsp->icd_rsp.set_backlight_setting_rsp_info.cmd_status = 0xFF;
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather : Error cannot open diagpdev\n",__func__);
	}
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_get_batterycharging_state_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_get_battery_charging_state_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_get_batterycharging_state_req_proc\n");
	printk(KERN_INFO "[ICD] icd_get_batterycharging_state_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_GETBATTERYCHARGINGSTATE_REQ_CMD;
	
	// get battery charging info
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_set_batterycharging_state_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_set_battery_charging_state_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_set_batterycharging_state_req_proc\n");
	printk(KERN_INFO "[ICD] icd_set_batterycharging_state_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_SETBATTERYCHARGINGSTATE_REQ_CMD;
	
	// set battery charging info
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_get_battery_level_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_get_battery_level_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_get_battery_level_req_proc\n");
	printk(KERN_INFO "[ICD] icd_get_battery_level_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_GETBATTERYLEVEL_REQ_CMD;
	
	// get battery level info
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_get_bluetooth_status_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_get_bluetooth_status_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_get_bluetooth_status_req_proc\n");
	printk(KERN_INFO "[ICD] icd_get_bluetooth_status_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_GETBLUETOOTHSTATUS_REQ_CMD;
	
	// get bluetooth status info
	if(diagpdev != NULL)
	{
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather\n",__func__);
		update_diagcmd_state(diagpdev, "ICD_GETBLUETOOTH", 1);
		mdelay(50);

		if(strcmp(process_status,"COMPLETED") == 0)
		{		
			if(strcmp(process_value,"GETBLUETOOTH_0") == 0)
			{
				pRsp->icd_rsp.get_bluetooth_status_rsp_info.bluetooth_status = 0;
				printk(KERN_INFO "[ICD] %s was successful : status = %d\n",__func__,pRsp->icd_rsp.get_bluetooth_status_rsp_info.bluetooth_status);
			}
			else if(strcmp(process_value,"GETBLUETOOTH_1") == 0)
			{
				pRsp->icd_rsp.get_bluetooth_status_rsp_info.bluetooth_status = 1;
				printk(KERN_INFO "[ICD] %s was successful : status = %d\n",__func__,pRsp->icd_rsp.get_bluetooth_status_rsp_info.bluetooth_status);
			}
			else
			{
				pRsp->icd_rsp.get_bluetooth_status_rsp_info.bluetooth_status = 0xFF;
				printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
			}
		}
		else
		{
			pRsp->icd_rsp.get_bluetooth_status_rsp_info.bluetooth_status = 0xFF;
			printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
		}
	}
	else
	{
		pRsp->icd_rsp.get_bluetooth_status_rsp_info.bluetooth_status = 0xFF;
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather : Error cannot open diagpdev\n",__func__);
	}
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_set_bluetooth_status_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_set_bluetooth_status_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_set_bluetooth_status_req_proc\n");
	printk(KERN_INFO "[ICD] icd_set_bluetooth_status_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_SETBLUETOOTHSTATUS_REQ_CMD;
	
	// set bluetooth status info
	if(diagpdev != NULL)
	{
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather\n",__func__);
		update_diagcmd_state(diagpdev, "ICD_SETBLUETOOTH", pReq->icd_req.set_bluetooth_status_req_info.bluetooth_status);
		mdelay(50);

		if(strcmp(process_status,"COMPLETED") == 0)
		{		
			if(strcmp(process_value,"SETBLUETOOTH_0") == 0)
			{
				pRsp->icd_rsp.set_bluetooth_status_rsp_info.cmd_status = 0;
				printk(KERN_INFO "[ICD] %s was successful \n",__func__);
			}
			else if(strcmp(process_value,"SETBLUETOOTH_1") == 0)
			{
				pRsp->icd_rsp.set_bluetooth_status_rsp_info.cmd_status = 1;
				printk(KERN_INFO "[ICD] %s was unsuccessful\n",__func__);
			}
			else
			{
				pRsp->icd_rsp.set_bluetooth_status_rsp_info.cmd_status = 0xFF;
				printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
			}
		}
		else
		{
			pRsp->icd_rsp.set_bluetooth_status_rsp_info.cmd_status = 0xFF;
			printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
		}
	}
	else
	{
		pRsp->icd_rsp.set_bluetooth_status_rsp_info.cmd_status = 0xFF;
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather : Error cannot open diagpdev\n",__func__);
	}
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_get_gps_status_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_get_gps_status_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_get_gps_status_req_proc\n");
	printk(KERN_INFO "[ICD] icd_get_gps_status_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_GETGPSSTATUS_REQ_CMD;
	
	// get gps status info
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_set_gps_status_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_set_gps_status_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_set_gps_status_req_proc\n");
	printk(KERN_INFO "[ICD] icd_set_gps_status_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_SETGPSSTATUS_REQ_CMD;
	
	// set gps status info
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_get_keypadbacklight_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_get_keypadbacklight_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_get_keypadbacklight_req_proc\n");
	printk(KERN_INFO "[ICD] icd_get_keypadbacklight_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_GETKEYPADBACKLIGHT_REQ_CMD;
	
	// get keypadbacklight status info
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_set_keypadbacklight_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_set_keypadbacklight_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_set_keypadbacklight_req_proc\n");
	printk(KERN_INFO "[ICD] icd_set_keypadbacklight_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_SETKEYPADBACKLIGHT_REQ_CMD;
	
	// set keypadbacklight status info
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_get_roamingmode_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_get_roamingmode_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_get_roamingmode_req_proc\n");
	printk(KERN_INFO "[ICD] icd_get_roamingmode_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_GETROAMINGMODE_REQ_CMD;
	
	// get roaming mode info
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_get_rssi_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_get_rssi_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_get_rssi_req_proc\n");
	printk(KERN_INFO "[ICD] icd_get_rssi_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_GETRSSI_REQ_CMD;
	
	// get RSSI info
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_get_state_connect_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_get_state_connect_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_get_state_connect_req_proc\n");
	printk(KERN_INFO "[ICD] icd_get_state_connect_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_GETSTATEANDCONNECTIONATTEMPTS_REQ_CMD;
	
	// get state and connection attempts info
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_get_ui_screen_id_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_get_ui_screen_id_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_get_ui_screen_id_req_proc\n");
	printk(KERN_INFO "[ICD] icd_get_ui_screen_id_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_GETUISCREENID_REQ_CMD;
	
	// get ui screen id 
	if(diagpdev != NULL)
	{
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather\n",__func__);
		update_diagcmd_state(diagpdev, "ICD_GETUISCREENID", pReq->icd_req.get_ui_srceen_id_req_info.physical_screen);
		mdelay(50);

		if(strcmp(process_status,"COMPLETED") == 0)
		{
			if(strcmp(process_value,"GETUISCREENID_1") == 0)
			{
				pRsp->icd_rsp.get_ui_screen_id_rsp_info.ui_screen_id = 1;
				printk(KERN_INFO "[ICD] %s was successful\n",__func__);
			}
			else if(strcmp(process_value,"GETUISCREENID_2") == 0)
			{
				pRsp->icd_rsp.get_ui_screen_id_rsp_info.ui_screen_id = 2;
				printk(KERN_INFO "[ICD] %s was unsuccessful\n",__func__);
			}
			else if(strcmp(process_value,"GETUISCREENID_3") == 0)
			{
				pRsp->icd_rsp.get_ui_screen_id_rsp_info.ui_screen_id = 3;
				printk(KERN_INFO "[ICD] %s was unsuccessful\n",__func__);
			}
			else if(strcmp(process_value,"GETUISCREENID_4") == 0)
			{
				pRsp->icd_rsp.get_ui_screen_id_rsp_info.ui_screen_id = 4;
				printk(KERN_INFO "[ICD] %s was unsuccessful\n",__func__);
			}
			else if(strcmp(process_value,"GETUISCREENID_5") == 0)
			{
				pRsp->icd_rsp.get_ui_screen_id_rsp_info.ui_screen_id = 5;
				printk(KERN_INFO "[ICD] %s was unsuccessful\n",__func__);
			}
			else if(strcmp(process_value,"GETUISCREENID_6") == 0)
			{
				pRsp->icd_rsp.get_ui_screen_id_rsp_info.ui_screen_id = 6;
				printk(KERN_INFO "[ICD] %s was unsuccessful\n",__func__);
			}
			else if(strcmp(process_value,"GETUISCREENID_7") == 0)
			{
				pRsp->icd_rsp.get_ui_screen_id_rsp_info.ui_screen_id = 7;
				printk(KERN_INFO "[ICD] %s was unsuccessful\n",__func__);
			}
			else
			{
				pRsp->icd_rsp.get_ui_screen_id_rsp_info.ui_screen_id = 0xFF;
				printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
			}
		}
		else
		{
			pRsp->icd_rsp.get_ui_screen_id_rsp_info.ui_screen_id = 0xFF;
			printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
		}
	}
	else
	{
		pRsp->icd_rsp.get_ui_screen_id_rsp_info.ui_screen_id = 0xFF;
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather : Error cannot open diagpdev\n",__func__);
	}
	pRsp->icd_rsp.get_ui_screen_id_rsp_info.physical_screen = 0;
		
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_get_wifi_status_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_get_wifi_status_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_get_wifi_status_req_proc\n");
	printk(KERN_INFO "[ICD] icd_get_wifi_status_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_GETWIFISTATUS_REQ_CMD;

	// get wifi status
	if(diagpdev != NULL)
	{
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather\n",__func__);
		update_diagcmd_state(diagpdev, "ICD_GETWIFISTATUS", 1);
		mdelay(50);

		if(strcmp(process_status,"COMPLETED") == 0)
		{
			if(strcmp(process_value,"GETWIFISTATUS_0") == 0)
			{
				pRsp->icd_rsp.get_wifi_status_rsp_info.wifi_status = 0;
				printk(KERN_INFO "[ICD] %s was successful : wifi status = %d\n",__func__,pRsp->icd_rsp.get_wifi_status_rsp_info.wifi_status);
			}
			else if(strcmp(process_value,"GETWIFISTATUS_1") == 0)
			{
				pRsp->icd_rsp.get_wifi_status_rsp_info.wifi_status = 1;
				printk(KERN_INFO "[ICD] %s was successful : wifi status = %d\n",__func__,pRsp->icd_rsp.get_wifi_status_rsp_info.wifi_status);
			}
			else
			{
				pRsp->icd_rsp.get_wifi_status_rsp_info.wifi_status = 0xFF;
				printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
			}
		}
		else
		{
			pRsp->icd_rsp.get_wifi_status_rsp_info.wifi_status = 0xFF;
			printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
		}
	}
	else
	{
		pRsp->icd_rsp.get_wifi_status_rsp_info.wifi_status = 0xFF;
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather : Error cannot open diagpdev\n",__func__);
	}
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_set_wifi_status_req_proc(DIAG_ICD_F_req_type * pReq)
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;
	
	rsp_len = sizeof(icd_set_wifi_status_rsp_type);
	
	printk(KERN_INFO "[ICD] icd_set_wifi_status_req_proc, req = %d\n", pReq->icd_req.set_wifi_status_req_info.wifi_status);
	printk(KERN_INFO "[ICD] icd_set_wifi_status_req_proc rsp_len :(%d)\n", rsp_len);
	
	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}
	
	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_SETWIFISTATUS_REQ_CMD;
	
	// set wifi status
	if(diagpdev != NULL)
	{
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather\n",__func__);
		update_diagcmd_state(diagpdev, "ICD_SETWIFISTATUS", pReq->icd_req.set_wifi_status_req_info.wifi_status);
		mdelay(50);

		if(strcmp(process_status,"COMPLETED") == 0)
		{
			if(strcmp(process_value,"SETWIFISTATUS_0") == 0)
			{
				pRsp->icd_rsp.set_wifi_status_rsp_info.cmd_status = 0;
				printk(KERN_INFO "[ICD] %s was successful \n",__func__);
			}
			else if(strcmp(process_value,"SETWIFISTATUS_1") == 0)
			{
				pRsp->icd_rsp.set_wifi_status_rsp_info.cmd_status = 1;
				printk(KERN_INFO "[ICD] %s was unsuccessful\n",__func__);
			}
			else
			{
				pRsp->icd_rsp.set_wifi_status_rsp_info.cmd_status = 0xFF;
				printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
			}
		}
		else
		{
			pRsp->icd_rsp.set_wifi_status_rsp_info.cmd_status = 0xFF;
			printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
		}
	}
	else
	{
		pRsp->icd_rsp.set_wifi_status_rsp_info.cmd_status = 0xFF;
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather : Error cannot open diagpdev\n",__func__);
	}
	
	return pRsp;
}

DIAG_ICD_F_rsp_type *icd_set_screenorientationlock_req_proc(DIAG_ICD_F_req_type * pReq)	
{
	unsigned int rsp_len;
	DIAG_ICD_F_rsp_type *pRsp;

	rsp_len = sizeof(icd_set_screenorientationlock_rsp_type);
		
	printk(KERN_INFO "[ICD] %s req = %d\n", __func__,pReq->icd_req.set_screenorientationlock_req_info.orientation_mode);
	printk(KERN_INFO "[ICD] %s rsp_len :(%d)\n", __func__, rsp_len);

	pRsp = (DIAG_ICD_F_rsp_type *) diagpkt_alloc(DIAG_ICD_F, rsp_len);
	if (pRsp == NULL) {
		printk(KERN_ERR "[ICD] diagpkt_alloc failed\n");
		return pRsp;
	}

	pRsp->hdr.cmd_code = DIAG_ICD_F;
	pRsp->hdr.sub_cmd = ICD_SETSCREENORIENTATIONLOCK_REQ_CMD;
		
	// set screen orientation lock
	
	if(diagpdev != NULL)
	{
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather\n",__func__);
		update_diagcmd_state(diagpdev, "ICD_SETORIENTATIONLOCK", pReq->icd_req.set_screenorientationlock_req_info.orientation_mode);
		mdelay(50);
		if(strcmp(process_status,"COMPLETED") == 0)
		{
			if(strcmp(process_value,"SETORIENTATION_0") == 0)
			{
				pRsp->icd_rsp.set_screenorientation_rsp_info.cmd_status = 0;
				printk(KERN_INFO "[ICD] %s was successful\n",__func__);
			}
			else if(strcmp(process_value,"SETORIENTATION_1") == 0)
			{
				pRsp->icd_rsp.set_screenorientation_rsp_info.cmd_status = 1;
				printk(KERN_INFO "[ICD] %s was unsuccessful\n",__func__);
			}
			else
			{
				pRsp->icd_rsp.set_screenorientation_rsp_info.cmd_status = 0xFF;
				printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
			}
		}
		else
		{
			pRsp->icd_rsp.set_screenorientation_rsp_info.cmd_status = 0xFF;
			printk(KERN_INFO "[ICD] %s return value from DiagCommandDispather is invalid\n",__func__);
		}
	}
	else
	{
		pRsp->icd_rsp.set_screenorientation_rsp_info.cmd_status = 0xFF;
		printk(KERN_INFO "[ICD] %s goto DiagCommandDispather : Error cannot open diagpdev\n",__func__);
	}
	
	return pRsp;
}

/** ICDR : ICD Implementation Recommendation  - END **/

/*  USAGE (same as testmode
 *    1. If you want to handle at ARM9 side, 
 *       you have to insert fun_ptr as NULL and mark ARM9_PROCESSOR
 *    2. If you want to handle at ARM11 side , 
 *       you have to insert fun_ptr as you want and mark AMR11_PROCESSOR.
 */
icd_user_table_entry_type icd_mstr_tbl[ICD_MSTR_TBL_SIZE] = {
	/*sub_command								fun_ptr									which procesor*/
	/** SAR : Sprint Automation Requirement - START **/
	{ICD_GETDEVICEINFO_REQ_CMD, 				NULL, 									ICD_ARM9_PROCESSOR},
	{ICD_EXTENDEDVERSIONINFO_REQ_CMD,			icd_extended_info_req_proc, 			ICD_ARM11_PROCESSOR},
	{ICD_HANDSETDISPLAYTEXT_REQ_CMD,			icd_handset_disp_text_req_proc,		 	ICD_ARM11_PROCESSOR},
	{ICD_CAPTUREIMAGE_REQ_CMD,					icd_capture_img_req_proc,				ICD_ARM11_PROCESSOR},
	/** SAR : Sprint Automation Requirement - END **/

	/** ICDR : ICD Implementation Recommendation  - START **/
	{ICD_GETAIRPLANEMODE_REQ_CMD,				icd_get_airplanemode_req_proc,			ICD_ARM11_PROCESSOR},
	{ICD_SETAIRPLANEMODE_REQ_CMD,				icd_set_airplanemode_req_proc,			ICD_ARM11_PROCESSOR},
	{ICD_GETBACKLIGHTSETTING_REQ_CMD,			icd_get_backlight_setting_req_proc,		ICD_ARM11_PROCESSOR},
	{ICD_SETBACKLIGHTSETTING_REQ_CMD,			icd_set_backlight_setting_req_proc,		ICD_ARM11_PROCESSOR},
	{ICD_GETBATTERYCHARGINGSTATE_REQ_CMD,			NULL,						ICD_ARM9_PROCESSOR},
	{ICD_SETBATTERYCHARGINGSTATE_REQ_CMD,			NULL,						ICD_ARM9_PROCESSOR},
	{ICD_GETBATTERYLEVEL_REQ_CMD,				NULL,						ICD_ARM9_PROCESSOR},
	{ICD_GETBLUETOOTHSTATUS_REQ_CMD,			icd_get_bluetooth_status_req_proc,		ICD_ARM11_PROCESSOR},
	{ICD_SETBLUETOOTHSTATUS_REQ_CMD,			icd_set_bluetooth_status_req_proc,		ICD_ARM11_PROCESSOR},
	{ICD_GETGPSSTATUS_REQ_CMD,					NULL,									ICD_ARM9_PROCESSOR},
	{ICD_SETGPSSTATUS_REQ_CMD,					NULL,									ICD_ARM9_PROCESSOR},
	{ICD_GETKEYPADBACKLIGHT_REQ_CMD,			icd_app_handler,						ICD_ARM11_PROCESSOR},
	{ICD_SETKEYPADBACKLIGHT_REQ_CMD,			icd_app_handler,						ICD_ARM11_PROCESSOR},
	{ICD_GETROAMINGMODE_REQ_CMD,				NULL,									ICD_ARM9_PROCESSOR},
	{ICD_GETSTATEANDCONNECTIONATTEMPTS_REQ_CMD,	NULL,									ICD_ARM9_PROCESSOR},
	{ICD_GETUISCREENID_REQ_CMD,					icd_get_ui_screen_id_req_proc,			ICD_ARM11_PROCESSOR},
	{ICD_GETWIFISTATUS_REQ_CMD,					icd_get_wifi_status_req_proc,			ICD_ARM11_PROCESSOR},
	{ICD_SETWIFISTATUS_REQ_CMD,					icd_set_wifi_status_req_proc,			ICD_ARM11_PROCESSOR},
	{ICD_SETSCREENORIENTATIONLOCK_REQ_CMD,		icd_set_screenorientationlock_req_proc,	ICD_ARM11_PROCESSOR},
	{ICD_GETRSSI_REQ_CMD,						NULL,					ICD_ARM9_PROCESSOR},
	/** ICDR : ICD Implementation Recommendation  - END **/
};
