/* LGE_CHANGES LGE_RAPI_COMMANDS  */
/* Created by khlee@lge.com  
 * arch/arm/mach-msm/lge/LG_rapi_client.c
 *
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
 *
 */
#include <linux/kernel.h>
#include <linux/err.h>
#include <mach/oem_rapi_client.h>
#include <mach/lge_diag_testmode.h>
#ifdef CONFIG_LGE_DIAG_ICD
#include <mach/lge_diag_icd.h>
#endif
#include <mach/lge_diag_srd.h>
#if defined(CONFIG_MACH_MSM7X27_GELATO)
/* ADD THUNERC feature to use VS740 BATT DRIVER
 * 2010--5-13, taehung.kim@lge.com
 */
#include <mach/msm_battery_gelato.h>
#else
#include <mach/msm_battery.h>
#endif

#include <linux/slab.h>

#define GET_INT32(buf)       (int32_t)be32_to_cpu(*((uint32_t*)(buf)))
#define PUT_INT32(buf, v)        (*((uint32_t*)buf) = (int32_t)be32_to_cpu((uint32_t)(v)))
#define GET_U_INT32(buf)         ((uint32_t)GET_INT32(buf))
#define PUT_U_INT32(buf, v)      PUT_INT32(buf, (int32_t)(v))

#define GET_LONG(buf)            ((long)GET_INT32(buf))
#define PUT_LONG(buf, v) \
	(*((u_long*)buf) = (long)be32_to_cpu((u_long)(v)))

#define GET_U_LONG(buf)	      ((u_long)GET_LONG(buf))
#define PUT_U_LONG(buf, v)	      PUT_LONG(buf, (long)(v))


#define GET_BOOL(buf)            ((bool_t)GET_LONG(buf))
#define GET_ENUM(buf, t)         ((t)GET_LONG(buf))
#define GET_SHORT(buf)           ((short)GET_LONG(buf))
#define GET_U_SHORT(buf)         ((u_short)GET_LONG(buf))

#define PUT_ENUM(buf, v)         PUT_LONG(buf, (long)(v))
#define PUT_SHORT(buf, v)        PUT_LONG(buf, (long)(v))
#define PUT_U_SHORT(buf, v)      PUT_LONG(buf, (long)(v))

#define LG_RAPI_CLIENT_MAX_OUT_BUFF_SIZE 128
#define LG_RAPI_CLIENT_MAX_IN_BUFF_SIZE 128

#ifdef CONFIG_LGE_UART
/*LGE_CHANGES yongman.kwon 2010-09-07[MS690] : firstboot check */
extern int boot_info;
#endif

static uint32_t open_count;
struct msm_rpc_client *client;

int LG_rapi_init(void)
{
	client = oem_rapi_client_init();
	if (IS_ERR(client)) {
		pr_err("%s: couldn't open oem rapi client\n", __func__);
		return PTR_ERR(client);
	}
	open_count++;

	return 0;
}

void Open_check(void)
{
	/* to double check re-open; */
	if(open_count > 0) return;
	LG_rapi_init();
}

/* [yk.kim@lge.com] 2011-01-25, get manual test mode NV */
int msm_get_manual_test_mode(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	char output[4];
	int rc= -1;

	Open_check();

	arg.event = LG_FW_MANUAL_TEST_MODE;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = (char*) NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 4;

	ret.output = NULL;
	ret.out_len = NULL;

	rc = oem_rapi_client_streaming_function(client, &arg, &ret);
	
/* BEGIN: 0015327 jihoon.lee@lge.com 20110204 */
/* MOD 0015327: [KERNEL] LG RAPI validity check */
	if (rc < 0)
	{
		pr_err("%s error \r\n", __func__);
		memset(output,0,4);

		printk(KERN_INFO "MANUAL_TEST_MODE read FAIL!!!!!\n");
	}
	else
	{
		printk(KERN_INFO "MANUAL_TEST_MODE nv : %d out_len=%d\n", GET_INT32(ret.output), *ret.out_len);
	}
/* END: 0015327 jihoon.lee@lge.com 20110204 */


	if (ret.output&& ret.out_len)
		memcpy(output,ret.output,*ret.out_len);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

	
	return (GET_INT32(output));
}

EXPORT_SYMBOL(msm_get_manual_test_mode);



int msm_chg_LG_cable_type(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	char output[LG_RAPI_CLIENT_MAX_OUT_BUFF_SIZE];

	Open_check();

/* LGE_CHANGES_S [younsuk.song@lge.com] 2010-09-06, Add error control code. Repeat 3 times if error occurs*/
	
	int rc= -1;
	int errCount= 0;

	do 
	{
		arg.event = LG_FW_RAPI_CLIENT_EVENT_GET_LINE_TYPE;
		arg.cb_func = NULL;
		arg.handle = (void*) 0;
		arg.in_len = 0;
		arg.input = NULL;
		arg.out_len_valid = 1;
		arg.output_valid = 1;
		arg.output_size = 4;

		ret.output = NULL;
		ret.out_len = NULL;

		rc= oem_rapi_client_streaming_function(client, &arg, &ret);
	
		if (rc < 0 || ret.output == NULL)
			pr_err("get LG_cable_type error \r\n");
		else
			pr_info("msm_chg_LG_cable_type: %d \r\n", GET_INT32(ret.output));

	} while (rc < 0 && errCount++ < 3);

/* LGE_CHANGES_E [younsuk.song@lge.com] */

	if (ret.output != NULL)	
	{
		memcpy(output,ret.output,*ret.out_len);

		kfree(ret.output);
	}
	else
		printk(KERN_ERR "%s RPC FAIL!!!!\n", __func__);

	if (ret.out_len != NULL)
		kfree(ret.out_len);

	if (ret.output)
		return (GET_INT32(output));  
	else
		return -1;
	
}


void send_to_arm9(void*	pReq, void* pRsp)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_TESTMODE_EVENT_FROM_ARM11;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(DIAG_TEST_MODE_F_req_type);
	arg.input = (char*)pReq;
	arg.out_len_valid = 1;
	arg.output_valid = 1;

	if( ((DIAG_TEST_MODE_F_req_type*)pReq)->sub_cmd_code == TEST_MODE_FACTORY_RESET_CHECK_TEST)
		arg.output_size = sizeof(DIAG_TEST_MODE_F_rsp_type) - sizeof(test_mode_rsp_type);
	else
		arg.output_size = sizeof(DIAG_TEST_MODE_F_rsp_type);

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);
	memcpy(pRsp,ret.output,*ret.out_len);

	return;
}

// LGE_CHANGE_S [2011.02.08] [myeonggyu.son@lge.com] [gelato] add icd oem rapi function [START]
#ifdef CONFIG_LGE_DIAG_ICD
void icd_send_to_arm9(void*	pReq, void* pRsp, unsigned int output_length)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_RAPI_ICD_DIAG_EVENT;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(DIAG_ICD_F_req_type);
	arg.input = (char*)pReq;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = output_length;

	ret.output = NULL;
	ret.out_len = NULL;
	
	oem_rapi_client_streaming_function(client, &arg, &ret);

	memcpy(pRsp,ret.output,*ret.out_len);

	return;
}
EXPORT_SYMBOL(icd_send_to_arm9);
#endif
// LGE_CHANGE_E [2011.02.08] [myeonggyu.son@lge.com] [gelato] add icd oem rapi function [END]

#ifdef CONFIG_LGE_DIAG_SRD
void srd_send_to_arm9(void*	pReq, void* pRsp)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_SRD_EVENT_FROM_ARM11;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(srd_req_type);
	arg.input = (char*)pReq;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = sizeof(srd_rsp_type);
	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);
	memcpy(pRsp,ret.output,*ret.out_len);

	return;
}
#endif

void set_operation_mode(boolean info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_SET_OPERATIN_MODE;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(boolean);
	arg.input = (char*) &info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;

	ret.output = (char*) NULL;
	ret.out_len = 0;

	oem_rapi_client_streaming_function(client,&arg, &ret);
}
EXPORT_SYMBOL(set_operation_mode);


#ifdef CONFIG_MACH_MSM7X27_GELATO
void battery_info_get(struct batt_info* resp_buf)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	uint32_t out_len;
	int ret_val;
	struct batt_info rsp_buf;

	Open_check();

	arg.event = LG_FW_A2M_BATT_INFO_GET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = sizeof(rsp_buf);

	ret.output = (char*)&rsp_buf;
	ret.out_len = &out_len;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);
	if(ret_val == 0) {
		resp_buf->valid_batt_id = GET_U_INT32(&rsp_buf.valid_batt_id);
		resp_buf->batt_therm = GET_U_INT32(&rsp_buf.batt_therm);
		resp_buf->batt_temp = GET_INT32(&rsp_buf.batt_temp);
		resp_buf->vbatt_adc = GET_INT32(&rsp_buf.vbatt_adc);
	} else { /* In case error */
		resp_buf->valid_batt_id = 1; /* authenticated battery id */
		resp_buf->batt_therm = 100;  /* 100 battery therm adc */
		resp_buf->batt_temp = 30;    /* 30 degree celcius */
		resp_buf->vbatt_adc = 238;    /* 4.2V */
	}
	return;
}
EXPORT_SYMBOL(battery_info_get);

void pseudo_batt_info_set(struct pseudo_batt_info_type* info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_A2M_PSEUDO_BATT_INFO_SET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(struct pseudo_batt_info_type);
	arg.input = (char*)info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;  /* alloc memory for response */

	ret.output = (char*)NULL;
	ret.out_len = 0;

	oem_rapi_client_streaming_function(client, &arg, &ret);
	return;
}
EXPORT_SYMBOL(pseudo_batt_info_set);
void block_charging_set(int bypass)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();
	arg.event = LG_FW_A2M_BLOCK_CHARGING_SET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(int);
	arg.input = (char*) &bypass;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;

	ret.output = (char*)NULL;
	ret.out_len = 0;

	oem_rapi_client_streaming_function(client,&arg,&ret);
	return;
}
EXPORT_SYMBOL(block_charging_set);

void set_temperature_block(int info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;

	Open_check();

	arg.event = LG_FW_RAPI_CLIENT_EVENT_SET_TEMPERATURE_BLOCK;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(int);
	arg.input = (char *)&info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;	//alloc memory for response

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

}
EXPORT_SYMBOL(set_temperature_block);


#ifdef CONFIG_LGE_UART
//20100914 yongman.kwon@lge.com [MS690] : firstboot check [START]*/
void lg_set_boot_info(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();
	arg.event = LG_FW_SET_BOOT_INFO;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(int);
	arg.input = (char*) &boot_info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;

	ret.output = (char*)NULL;
	ret.out_len = 0;

	oem_rapi_client_streaming_function(client,&arg,&ret);
	return;

}
EXPORT_SYMBOL(lg_set_boot_info);
//20100914 yongman.kwon@lge.com [MS690] : firstboot check [START]*/

//20100914 yongman.kwon@lge.com [MS690] : power check mode [START]
int lg_get_power_check_mode(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	byte power_mode = 0xFF;
	unsigned int out_len = 0xFFFFFFFF;

	Open_check();

	arg.event = LG_FW_GET_POWER_MODE;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 1;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	power_mode = *ret.output;
	out_len = *ret.out_len;

	return power_mode;
}
EXPORT_SYMBOL(lg_get_power_check_mode);

int lg_get_flight_mode(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	byte flight_mode = 0xFF;
	unsigned int out_len = 0xFFFFFFFF;

	Open_check();

	arg.event = LG_FW_GET_FLIGHT_MODE;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 1;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	flight_mode = *ret.output;
	out_len = *ret.out_len;

	return flight_mode;
}
EXPORT_SYMBOL(lg_get_flight_mode);
//20100914 yongman.kwon@lge.com [MS690] power check mode [END]
#endif

#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
void msm_get_SW_VER_type(char* sw_ver)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	byte flight_mode = 0xFF;
	unsigned int out_len = 0xFFFFFFFF;

	Open_check();

	arg.event = LG_FW_SW_VERSION_GET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 8;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	sw_ver = *ret.output;
	out_len = *ret.out_len;

	return sw_ver;
}
EXPORT_SYMBOL(msm_get_SW_VER_type);
#endif



#endif	/* CONFIG_MACH_MSM7X27_GELATO */

void msm_get_MEID_type(char* sMeid)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
#if 1	/* LGE_CHANGES_S [moses.son@lge.com] 2011-02-09, to avoid compie error, need to check  */
	Open_check();

	arg.event = LG_FW_MEID_GET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 15;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	memcpy(sMeid,ret.output,14); 

	kfree(ret.output);
	kfree(ret.out_len);
#else
	memcpy(sMeid, "12345678912345", 14);
#endif
	return;  
}
void remote_set_charging_stat_realtime_update(int info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int ret_val;

	Open_check();

	arg.event = LG_FW_SET_CHARGING_STAT_REALTIME_UPDATE;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(int);
	arg.input = (char *)&info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;	//alloc memory for response

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

	return;
}

EXPORT_SYMBOL(remote_set_charging_stat_realtime_update);

void remote_get_charging_stat_realtime_update(int *info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	uint32_t out_len;
	int ret_val;
	int resp_buf;

	Open_check();

	arg.event = LG_FW_GET_CHARGING_STAT_REALTIME_UPDATE;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = sizeof(int);

	ret.output = NULL;
	ret.out_len = NULL;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);
	if (ret_val == 0) {
		memcpy(&resp_buf, ret.output, *ret.out_len);
		*info = GET_INT32(&resp_buf);
	} else {
		*info = 0;	//default value
	}

	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

	return;
}
EXPORT_SYMBOL(remote_get_charging_stat_realtime_update);


/* LGE_CHANGE [dojip.kim@lge.com] 2010-05-29, pcb version from LS680*/
#ifdef CONFIG_LGE_PCB_VERSION
int lge_get_hw_version(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int pcb_ver;

	Open_check();

	arg.event = LG_FW_GET_PCB_VERSION;
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 4;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);
	memcpy(&pcb_ver, ret.output, *ret.out_len);

	/* LGE_CHAGNE [dojip.kim@lge.com] 2010-06-21, free the allocated mem */
	if (ret.output)
		kfree(ret.output);
	if (ret.out_len)
		kfree(ret.out_len);

	return GET_INT32(&pcb_ver);
}

EXPORT_SYMBOL(lge_get_hw_version);
#endif /* CONFIG_LGE_PCB_VERSION */


// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-06-01, Add LED On/Off oem rapi function [START]	
void lge_led_control(int val)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	int rc= -1;
	int errCount= 0;
	char fs_err_buf[20];

	Open_check();

	if(val)
	{
		arg.event = LG_FW_LED_ON;
	}
	else
	{
		arg.event = LG_FW_LED_OFF;
	}
	arg.cb_func = NULL;
	arg.handle = (void *)0;
	arg.in_len = sizeof(int);
	arg.input = (char*)&val;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;

	ret.output = (char*)NULL;
	ret.out_len = 0;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	return;
}
EXPORT_SYMBOL(lge_led_control);
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-06-01, Add LED On/Off oem rapi function [END]
