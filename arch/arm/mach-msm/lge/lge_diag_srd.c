#include <linux/module.h>
#include <mach/lge_diagcmd.h>
#include <linux/input.h>
#include <linux/syscalls.h>
#include <linux/slab.h>

#include "lge_diag_communication.h"
#include <mach/lge_diag_srd.h>
#include <linux/delay.h>

#ifndef SKW_TEST
#include <linux/fcntl.h> 
#include <linux/fs.h>
#include <linux/uaccess.h>
#endif
static struct diagcmd_dev *diagpdev;

extern PACK(void *) diagpkt_alloc (diagpkt_cmd_code_type code, unsigned int length);
extern PACK(void *) diagpkt_free (PACK(void *)pkt);
extern void srd_send_to_arm9( void*	pReq, void	*pRsp);


PACK (void *)LGF_SRD (
			PACK (void	*)req_pkt_ptr,	/* pointer to request packet  */
			uint16		pkt_len )		      /* length of request packet   */
{
	srd_req_type *req_ptr = (srd_req_type *) req_pkt_ptr;
	srd_rsp_type *rsp_ptr;
	unsigned int rsp_len;

	diagpdev = diagcmd_get_dev();

	rsp_len = sizeof(srd_rsp_type);
	rsp_ptr = (srd_rsp_type *)diagpkt_alloc(DIAG_SRD_F, rsp_len);

	if (!rsp_ptr)
		return 0;

	printk(KERN_INFO "[SRD] cmd_code : [0x%X], sub_cmd : [0x%X] --> goto MODEM through oem rapi\n",req_ptr->header.cmd_code, req_ptr->header.sub_cmd);
	printk(KERN_INFO "[SRD] backup : [0x%X], class : [0x%X] --> goto MODEM through oem rapi\n",req_ptr->req_data.do_dl_entry.backup_used, req_ptr->req_data.do_dl_entry.backup_used);
	srd_send_to_arm9((void*)req_ptr, (void*)rsp_ptr);

	return (rsp_ptr);
}
EXPORT_SYMBOL(LGF_SRD);
