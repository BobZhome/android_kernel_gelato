/*
 * arch/arm/mach-msm/lge/lge_mtd_direct_access.c
 *
 * Copyright (C) 2010 LGE, Inc
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

#include <asm/div64.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/mtd/mtd.h>
#include <linux/sched.h>
#include <linux/slab.h>

// LGE_CHANGE [dojip.kim@lge.com] 2010-08-23, do something after cold boot
#include "lge_diag_communication.h"

#include <linux/delay.h>

#if defined(CONFIG_MACH_MSM7X27_GELATO)
#define MISC_PART_NUM 8
#define MISC_PART_SIZE 4
#define PERSIST_PART_NUM 9
#define PERSIST_PART_SIZE 12
#define PAGE_NUM_PER_BLK 64
#define PAGE_SIZE_BYTE 2048
#else
#define MISC_PART_NUM	4
#endif

#define LGE_RETRY_MTD_DIRECT_ACCESS

static struct mtd_info *mtd;

// kthread_lg_diag: page allocation failure. order:5, mode:0xd0, lge_init_mtd_access: error: cannot allocate memory
//static unsigned char *global_buf;
static unsigned char global_buf[PAGE_NUM_PER_BLK*PAGE_SIZE_BYTE];
static unsigned char *bbt;

static int pgsize;
static int bufsize;
static int ebcnt;
static int pgcnt;

int lge_erase_block(int ebnum);
int lge_write_block(int ebnum, unsigned char *buf, size_t size);
int lge_read_block(int ebnum, unsigned char *buf);
int lge_retry_erase_block(int ebnum);
int lge_retry_write_block(int ebnum, unsigned char *buf, size_t size);

static int scan_for_bad_eraseblocks(void);
static int is_block_bad(int ebnum);

int init_mtd_access(int partition, int block);

static int dev;
static int target_block;
static int dummy_arg;
int boot_info = 0;

module_param(dev, int, S_IRUGO);
module_param(target_block, int, S_IRUGO);

#ifdef CONFIG_LGE_UART
/*LGE_CHANGES yongman.kwon 2010-09-07[MS690] : firstboot check [START]*/
//module_param(boot_info, int, S_IWUSR | S_IRUGO);
extern void lg_set_boot_info(void);
static void set_boot_info(const char *val, struct kernel_param *kp)
{
	//change ASCII -> int
	//atoi(*val); 48 means '0' as ASCII.
	boot_info = *val - 48;

	printk("[INFORPC]boot info = %d\n", boot_info);

	lg_set_boot_info();	
}
module_param_call(boot_info, set_boot_info, param_get_int, &dummy_arg, S_IWUSR | S_IRUGO);
/*LGE_CHANGES yongman.kwon 2010-09-07[MS690] : firstboot check [START]*/
#endif

// LGE_CHANGE_S [myeonggyu.son@lge.com] [2011.03.30] [gelato] all auto test & testmode item [START]
int db_integrity_ready = 0;
module_param(db_integrity_ready, int, S_IWUSR | S_IRUGO);

int fpri_crc_ready = 0;
module_param(fpri_crc_ready, int, S_IWUSR | S_IRUGO);

int file_crc_ready = 0;
module_param(file_crc_ready, int, S_IWUSR | S_IRUGO);

int code_partition_crc_ready = 0;
module_param(code_partition_crc_ready, int, S_IWUSR | S_IRUGO);

int total_crc_ready = 0;
module_param(total_crc_ready, int, S_IWUSR | S_IRUGO);

typedef struct {
        char ret[32];
} testmode_rsp_from_diag_type;
testmode_rsp_from_diag_type integrity_ret;
static int integrity_ret_write(const char *val, struct kernel_param *kp)
{
        memcpy(integrity_ret.ret, val, 32);
        return 0;
}
static int integrity_ret_read(char *buf, struct kernel_param *kp)
{
        memcpy(buf, integrity_ret.ret, 32);
        return 0;
}
module_param_call(integrity_ret, integrity_ret_write, integrity_ret_read, &dummy_arg, S_IWUSR | S_IRUGO);
// LGE_CHANGE_E [myeonggyu.son@lge.com] [2011.03.30] [gelato] all auto test & testmode item [END]

static int test_init(void)
{
	int partition = PERSIST_PART_NUM;
	int block = 0;

	return init_mtd_access(partition, block);
}

static int test_erase_block(void)
{
	int i;
	int err;

	/* Erase eraseblock */
	printk(KERN_INFO"%s: erasing block\n", __func__);

	for (i = 0; i < ebcnt; i++) {
		if (bbt[i])
			continue;
		else
			break;
	}

#ifdef LGE_RETRY_MTD_DIRECT_ACCESS
	err = lge_retry_erase_block(i);
#else
	err = lge_erase_block(i);
#endif
	if (err) {
		printk(KERN_INFO"%s: erased %u block fail\n", __func__, i);
		return err;
	}

	printk(KERN_INFO"%s: erased %u block\n", __func__, i);

	return 0;
}
static int test_write_block(const char *val, struct kernel_param *kp)
{
	int i;
	int err;
/* LGE_CHANGE_S [sm.shim@lge.com] 2010-08-22, merge First Boot Complete Test from VS660 */
	unsigned char *test_string;
	unsigned long flag=0;
	// LGE_CHANGE [dojip.kim@lge.com] 2010-08-23, do something after cold boot
	struct diagcmd_dev *diagpdev;

	flag = simple_strtoul(val,NULL,10);
	if(5 == flag)
		test_string="FACT_RESET_5";
	else if(6 == flag)
		test_string="FACT_RESET_6";
	// LGE_CHANGE [dojip.kim@lge.com] 2010-09-04, for RTN and Factory reset
	else if (3 == flag)
		test_string="FACT_RESET_3";
	else
		return -1;
	
	test_init();
	test_erase_block();
	printk(KERN_INFO"%s: writing block: flag = %lu\n", __func__, flag);
/* LGE_CHANGE_E [sm.shim@lge.com] 2010-08-22, merge First Boot Complete Test from VS660 */
	
	for (i = 0; i < ebcnt; i++) {
		if (bbt[i])
			continue;
		else
			break;
	}
#ifdef LGE_RETRY_MTD_DIRECT_ACCESS
	err = lge_retry_write_block(i, test_string, strlen(test_string));
#else
	err = lge_write_block(i, test_string, strlen(test_string));
#endif
	if (err) {
		printk(KERN_INFO"%s: write %u block fail\n", __func__, i);
		return err;
	}
	
	printk(KERN_INFO"%s: write %u block\n", __func__, i);
	
	// LGE_CHANGE [dojip.kim@lge.com] 2010-08-23, do something after cold boot
	if (flag == 5) {
		diagpdev = diagcmd_get_dev();	
		if (diagpdev != NULL) {
			update_diagcmd_state(diagpdev, "ADBSET", 0);
		}
	}
	
	return 0;
}
module_param_call(write_block, test_write_block, param_get_bool, &dummy_arg,S_IWUSR | S_IRUGO);
module_param_call(write_block_2, test_write_block, param_get_bool, &dummy_arg,S_IWUSR | S_IRUGO);

#define FACTORY_RESET_STR_SIZE 11
#define FACTORY_RESET_STR "FACT_RESET_"
static int test_read_block(char *buf, struct kernel_param *kp)
{
	int i;
	int err;
	unsigned char status=0;
	char* temp = "1";

	printk(KERN_INFO"%s: read block\n", __func__);
	test_init();

	for (i = 0; i < ebcnt; i++) {
		if (bbt[i])
			continue;
		else
			break;
	}

	err = lge_read_block(i, global_buf);
	if (err) {
		printk(KERN_INFO"%s: read %u block fail\n", __func__, i);
		goto error;
	}

	printk(KERN_INFO"%s: read %u block\n", __func__, i);
	printk(KERN_INFO"%s: %s\n", __func__, global_buf);
	
	if(memcmp(global_buf, FACTORY_RESET_STR, FACTORY_RESET_STR_SIZE)==0){
		status = global_buf[FACTORY_RESET_STR_SIZE];
		err = sprintf(buf,"%s",global_buf+FACTORY_RESET_STR_SIZE);
		return err;
	}
	else{
error:
		err = sprintf(buf,"%s",temp);
		return err;
	}
	
}
module_param_call(read_block, param_get_bool, test_read_block, &dummy_arg, S_IWUSR | S_IRUGO);

int lge_erase_block(int ebnum)
{
	int err;
	struct erase_info ei;
	loff_t addr = ebnum * mtd->erasesize;

	memset(&ei, 0, sizeof(struct erase_info));
	ei.mtd  = mtd;
	ei.addr = addr;
	ei.len  = mtd->erasesize;

	err = mtd->erase(mtd, &ei);
	if (err) {
		printk(KERN_INFO"%s: error %d while erasing EB %d\n", __func__,  err, ebnum);
		return err;
	}

	if (ei.state == MTD_ERASE_FAILED) {
		printk(KERN_INFO"%s: some erase error occurred at EB %d\n", __func__,
		       ebnum);
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(lge_erase_block);

int lge_retry_erase_block(int ebnum)
{
	int retry_cnt = 0;
	int err = 0;

	for(retry_cnt = 0; retry_cnt < 5; retry_cnt++)
	{
		err = lge_erase_block(ebnum);
		if(err != 0)
		{
			printk(KERN_INFO"%s: error %d while erasing EB %d, retry count %d\n", __func__, err, ebnum, retry_cnt);
			msleep(100);
			continue;
		}
		else
		{
			printk(KERN_INFO"%s: success while erasing EB %d, retry count %d\n", __func__, ebnum, retry_cnt);
			break;
		}
	}

	return err;
}
EXPORT_SYMBOL(lge_retry_erase_block);


int lge_write_block(int ebnum, unsigned char *buf, size_t size)
{
	int err = 0;
	size_t written = 0;
	loff_t addr = ebnum * mtd->erasesize;

	memset(global_buf, 0, mtd->erasesize);
	memcpy(global_buf, buf, size);
	err = mtd->write(mtd, addr, pgsize, &written, global_buf);
	if (err || written != pgsize)
		printk(KERN_INFO"%s: error: write failed at %#llx\n",
		      __func__, (long long)addr);

	return err;
}
EXPORT_SYMBOL(lge_write_block);

int lge_retry_write_block(int ebnum, unsigned char *buf, size_t size)
{
	int retry_cnt = 0;
	int err = 0;
	unsigned char preadback_buf[2048];

	err = lge_write_block(ebnum, buf, size);

	if(err != 0)
	{
		printk(KERN_ERR "%s: lge_write_block() failed, error num = %d \n", __func__, err);

		for(retry_cnt=0; retry_cnt<5; retry_cnt++)
		{
			err = lge_retry_erase_block(ebnum);
			if(err!=0)
			{
				printk(KERN_ERR "%s: lge_retry_erase_block() failed, error num = %d \n", __func__, err);
			}
			else
			{
				err = lge_write_block(ebnum, buf, size);						
				if(err!=0)
				{
					printk(KERN_ERR "%s: lge_write_block() failed, error num = %d, retry count = %d \n", 
																					__func__, err, retry_cnt);
					continue;
				}
				else
				{
					memset((void*)preadback_buf, 0, sizeof(preadback_buf));
					err = lge_read_block(ebnum, preadback_buf);
					if(err!=0)
					{
						printk(KERN_ERR "%s: lge_read_block() failed, error num = %d, retry count = %d \n", 
																						__func__, err, retry_cnt);
						continue;
					}
					else
					{
						if(memcmp(preadback_buf, FACTORY_RESET_STR, FACTORY_RESET_STR_SIZE) == 0) // tag read sucess
						{
							printk(KERN_ERR "%s: lge_read_block() succeeded read back, error num = %d, retry count = %d \n",
																						__func__, err, retry_cnt);
							break;
						}
						else
						{
							printk(KERN_ERR "%s: lge_read_block() failed read back, error num = %d, retry count = %d \n",
																						__func__, err, retry_cnt);
							continue;
						}
					}
				}
			}
		}
	}
	else
	{
		printk(KERN_ERR "%s: lge_write_block() succeeded, error num = %d \n", __func__, err);
		
		memset((void*)preadback_buf, 0, sizeof(preadback_buf));
		err = lge_read_block(ebnum, preadback_buf);
		if(err!=0)
		{
			printk(KERN_ERR "%s: lge_read_block() failed, error num = %d \n", __func__, err);

			for(retry_cnt=0; retry_cnt<5; retry_cnt++)
			{
				err = lge_retry_erase_block(ebnum);
				if(err!=0)
				{
					printk(KERN_ERR "%s: lge_retry_erase_block() failed, error num = %d \n", __func__, err);
				}
				else
				{
					err = lge_write_block(ebnum, buf, size);						
					if(err!=0)
					{
						printk(KERN_ERR "%s: lge_write_block() failed, error num = %d, retry count = %d \n", 
																						__func__, err, retry_cnt);
						continue;					
					}
					else
					{
						memset((void*)preadback_buf, 0, sizeof(preadback_buf));
						err = lge_read_block(ebnum, preadback_buf);
						if(err!=0)
						{
							printk(KERN_ERR "%s: lge_read_block() failed, error num = %d, retry count = %d \n", 
																							__func__, err, retry_cnt);
							continue;
						}
						else
						{
							if(memcmp(preadback_buf, FACTORY_RESET_STR, FACTORY_RESET_STR_SIZE) == 0) // tag read sucess
							{
								printk(KERN_ERR "%s: lge_read_block() succeeded read back, error num = %d, retry count = %d \n",
																							__func__, err, retry_cnt);
								break;
							}
							else
							{
								printk(KERN_ERR "%s: lge_read_block() failed read back, error num = %d, retry count = %d \n",
																							__func__, err, retry_cnt);
								continue;
							}
						}
					}
				}
			}
		}
		else
		{
			printk(KERN_ERR "%s: lge_read_block() succeeded, error num = %d \n", __func__, err);
			if(memcmp(preadback_buf, FACTORY_RESET_STR, FACTORY_RESET_STR_SIZE) == 0) // tag read sucess
			{
				printk(KERN_ERR "%s: lge_read_block() succeeded read back, error num = %d \n", __func__, err);
			}
			else
			{
				printk(KERN_ERR "%s: lge_read_block() failed, error num = %d \n", __func__, err);
				
				for(retry_cnt=0; retry_cnt<5; retry_cnt++)
				{
					err = lge_retry_erase_block(ebnum);
					if(err!=0)
					{
						printk(KERN_ERR "%s: lge_retry_erase_block() failed, error num = %d \n", __func__, err);
					}
					else
					{
						err = lge_write_block(ebnum, buf, size);						
						if(err!=0)
						{
							printk(KERN_ERR "%s: lge_write_block() failed, error num = %d, retry count = %d \n", 
																							__func__, err, retry_cnt);
							continue;					
						}
						else
						{
							memset((void*)preadback_buf, 0, sizeof(preadback_buf));
							err = lge_read_block(ebnum, preadback_buf);
							if(err!=0)
							{
								printk(KERN_ERR "%s: lge_read_block() failed, error num = %d, retry count = %d \n", 
																								__func__, err, retry_cnt);
								continue;
							}
							else
							{
								if(memcmp(preadback_buf, FACTORY_RESET_STR, FACTORY_RESET_STR_SIZE) == 0) // tag read sucess
								{
									printk(KERN_ERR "%s: lge_read_block() succeeded read back, error num = %d, retry count = %d \n",
																								__func__, err, retry_cnt);
									break;
								}
								else
								{
									printk(KERN_ERR "%s: lge_read_block() failed read back, error num = %d, retry count = %d \n",
																								__func__, err, retry_cnt);
									continue;
								}
							}
						}
					}
				}
			}
		}
	}

	return err;
}
EXPORT_SYMBOL(lge_retry_write_block);

int lge_read_block(int ebnum, unsigned char *buf)
{
	int err = 0;
	size_t read = 0;
	loff_t addr = ebnum * mtd->erasesize;

	//memset(buf, 0, mtd->erasesize); 
	memset(buf, 0, pgsize);
	err = mtd->read(mtd, addr, pgsize, &read, buf);
	if (err || read != pgsize)
		printk(KERN_INFO"%s: error: read failed at %#llx\n",
		      __func__, (long long)addr);

	return err;
}
EXPORT_SYMBOL(lge_read_block);

static int is_block_bad(int ebnum)
{
	loff_t addr = ebnum * mtd->erasesize;
	int ret;

	ret = mtd->block_isbad(mtd, addr);
	if (ret)
		printk(KERN_INFO"%s: block %d is bad\n", __func__, ebnum);
	return ret;
}

static int scan_for_bad_eraseblocks(void)
{
	int i, bad = 0;

	bbt = kmalloc(ebcnt, GFP_KERNEL);
	if (!bbt) {
		printk(KERN_INFO"%s: error: cannot allocate memory\n", __func__);
		return -ENOMEM;
	}
	memset(bbt, 0 , ebcnt);

	printk(KERN_INFO"%s: scanning for bad eraseblocks\n", __func__);
	for (i = 0; i < ebcnt; ++i) {
		bbt[i] = is_block_bad(i) ? 1 : 0;
		if (bbt[i])
			bad += 1;
		cond_resched();
	}
	printk(KERN_INFO"%s: scanned %d eraseblocks, %d are bad\n", __func__, i, bad);
	return 0;
}

int init_mtd_access(int partition, int block)
{
	int err = 0;
	uint64_t tmp;

	dev = partition;
	target_block = block;
	
	mtd = get_mtd_device(NULL, dev);
	if (IS_ERR(mtd)) {
		err = PTR_ERR(mtd);
		printk(KERN_INFO"%s: cannot get MTD device\n", __func__);
		return err;
	}

	if (mtd->type != MTD_NANDFLASH) {
		printk(KERN_INFO"%s: this test requires NAND flash\n", __func__);
		goto out;
	}

	if (mtd->writesize == 1) {
		printk(KERN_INFO"%s: not NAND flash, assume page size is 512 "
		       "bytes.\n", __func__);
		pgsize = 512;
	} else
		pgsize = mtd->writesize;

	tmp = mtd->size;
	do_div(tmp, mtd->erasesize);
	ebcnt = tmp;
	pgcnt = mtd->erasesize / mtd->writesize;

	printk(KERN_INFO"%s: MTD device size %llu, eraseblock size %u, "
	       "page size %u, count of eraseblocks %u, pages per "
	       "eraseblock %u, OOB size %u\n", __func__,
	       (unsigned long long)mtd->size, mtd->erasesize,
	       pgsize, ebcnt, pgcnt, mtd->oobsize);

	err = -ENOMEM;
	bufsize = pgsize * 2;
	
	#if 0 
	global_buf = kmalloc(mtd->erasesize, GFP_KERNEL);
	if (!global_buf) {
		printk(KERN_INFO"%s: error: cannot allocate memory\n", __func__);
		goto out;
	}
	#endif
	err = scan_for_bad_eraseblocks();
	if (err)
		goto out;

	return 0;

out:
	return err;
}

//FACTORY_RESET
unsigned int lge_get_mtd_part_info(void)
{
	unsigned int result = 0;

	result = (PERSIST_PART_NUM<<16) | (PERSIST_PART_SIZE&0x0000FFFF);
	printk(KERN_INFO"%s: part_num : %d, count : %d\n", __func__, PERSIST_PART_NUM, PERSIST_PART_SIZE);
	return result;
}

int lge_get_mtd_factory_mode_blk(int target_blk)
{
	int i = 0;
	int factory_mode_blk = 0;

	for (i = 0; i < ebcnt; i++) {
		bbt[i] = is_block_bad(i) ? 1 : 0;
		if(bbt[i])
			continue;
		else
		{
			factory_mode_blk += 1;
			if(factory_mode_blk == target_blk)
				return i;
		}
	}
	return -1;
}

static int __init lge_mtd_direct_access_init(void)
{
	printk(KERN_INFO"%s: finished\n", __func__);

	return 0;
}

static void __exit lge_mtd_direct_access_exit(void)
{
	return;
}

module_init(lge_mtd_direct_access_init);
module_exit(lge_mtd_direct_access_exit);

MODULE_DESCRIPTION("LGE mtd direct access apis");
MODULE_AUTHOR("SungEun Kim <cleaneye.kim@lge.com>");
MODULE_LICENSE("GPL");
