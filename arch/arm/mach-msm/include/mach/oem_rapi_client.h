/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __ASM__ARCH_OEM_RAPI_CLIENT_H
#define __ASM__ARCH_OEM_RAPI_CLIENT_H

/*
 * OEM RAPI CLIENT Driver header file
 */

#include <linux/types.h>
#include <mach/msm_rpcrouter.h>

enum {
	OEM_RAPI_CLIENT_EVENT_NONE = 0,

	/*
	 * list of oem rapi client events
	 */

#if defined (CONFIG_LGE_SUPPORT_RAPI)
	/* LGE_CHANGES_S [khlee@lge.com] 2009-12-04, [VS740] use OEMRAPI */
	LG_FW_RAPI_START = 100,
	LG_FW_RAPI_CLIENT_EVENT_GET_LINE_TYPE = LG_FW_RAPI_START,
	LG_FW_TESTMODE_EVENT_FROM_ARM11 = LG_FW_RAPI_START + 1,
	LG_FW_A2M_BATT_INFO_GET = LG_FW_RAPI_START + 2,
	LG_FW_A2M_PSEUDO_BATT_INFO_SET = LG_FW_RAPI_START + 3,
	LG_FW_MEID_GET = LG_FW_RAPI_START + 4,
	/* LGE_CHANGE_S 
	 * SUPPORT TESTMODE FOR AIRPLAN MODE
	 * 2010-07-12 taehung.kim@lge.com
	 */
	LG_FW_SET_OPERATIN_MODE = LG_FW_RAPI_START + 5,
	LG_FW_SET_CHARGING_STAT_REALTIME_UPDATE = LG_FW_RAPI_START + 7,
	LG_FW_GET_CHARGING_STAT_REALTIME_UPDATE = LG_FW_RAPI_START + 8,
	LG_FW_RAPI_CLIENT_EVENT_SET_TEMPERATURE_BLOCK = LG_FW_RAPI_START + 10,
	LG_FW_A2M_BLOCK_CHARGING_SET = LG_FW_RAPI_START + 11,
#ifdef CONFIG_USB_SUPPORT_LGE_ANDROID_AUTORUN
	/* LGE_CHANGE_S 
	 * SUPPORT SW version for AUTORUN
	 * 2011-04-07 moses.son@lge.com
	 */
	LG_FW_SW_VERSION_GET = 	LG_FW_RAPI_START + 12,
#endif
#ifdef CONFIG_LGE_DIAG_ICD
	// LGE_CHANGE [2011.02.08] [myeonggyu.son@lge.com] [gelato] add icd oem rapi function
	LG_FW_RAPI_ICD_DIAG_EVENT = LG_FW_RAPI_START + 20,
#endif
#ifdef CONFIG_LGE_PCB_VERSION
	/* LGE_CHANGE [dojip.kim@lge.com] 2010-05-29, [LS670] PCB Version */
	LG_FW_GET_PCB_VERSION = LG_FW_RAPI_START + 21,
#endif

#ifdef CONFIG_LGE_UART
	/*LGE_CHANGES yongman.kwon 2010-09-07[MS690] : firstboot check */
	LG_FW_SET_BOOT_INFO =	  LG_FW_RAPI_START + 22,
	/*LGE_CHANGES yongman.kwon 2010-09-07[MS690] : check power mode [START]*/
	LG_FW_GET_POWER_MODE =		 LG_FW_RAPI_START + 23,	
	LG_FW_GET_FLIGHT_MODE = 	 LG_FW_RAPI_START + 24, 
	/*LGE_CHANGES yongman.kwon 2010-09-07[MS690] : check power mode [END]*/ 
#endif

/* [yk.kim@lge.com] 2011-01-25, get manual test mode NV */
	LG_FW_MANUAL_TEST_MODE = LG_FW_RAPI_START + 26,

// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-06-01, Add LED On/Off oem rapi function [START]	
	LG_FW_LED_ON = LG_FW_RAPI_START + 35,
	LG_FW_LED_OFF = LG_FW_RAPI_START + 36,
// LGE_CHANGE [jaekyung83.lee@lge.com] 2011-06-01, Add LED On/Off oem rapi function [END]	

	LG_FW_SRD_EVENT_FROM_ARM11 = LG_FW_RAPI_START + 50,

#endif
	OEM_RAPI_CLIENT_EVENT_MAX

};

struct oem_rapi_client_streaming_func_cb_arg {
	uint32_t  event;
	void      *handle;
	uint32_t  in_len;
	char      *input;
	uint32_t out_len_valid;
	uint32_t output_valid;
	uint32_t output_size;
};

struct oem_rapi_client_streaming_func_cb_ret {
	uint32_t *out_len;
	char *output;
};

struct oem_rapi_client_streaming_func_arg {
	uint32_t event;
	int (*cb_func)(struct oem_rapi_client_streaming_func_cb_arg *,
		       struct oem_rapi_client_streaming_func_cb_ret *);
	void *handle;
	uint32_t in_len;
	char *input;
	uint32_t out_len_valid;
	uint32_t output_valid;
	uint32_t output_size;
};

struct oem_rapi_client_streaming_func_ret {
	uint32_t *out_len;
	char *output;
};

int oem_rapi_client_streaming_function(
	struct msm_rpc_client *client,
	struct oem_rapi_client_streaming_func_arg *arg,
	struct oem_rapi_client_streaming_func_ret *ret);

int oem_rapi_client_close(void);

struct msm_rpc_client *oem_rapi_client_init(void);

#endif
