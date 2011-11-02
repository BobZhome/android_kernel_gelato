/* drivers/video/msm/src/panel/mddi/mddi_hitachi_hvga.c
 *
 * Copyright (C) 2008 QUALCOMM Incorporated.
 * Copyright (c) 2008 QUALCOMM USA, INC.
 * 
 * All source code in this file is licensed under the following license
 * except where indicated.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include "msm_fb.h"
#include "mddihost.h"
#include "mddihosti.h"
#include <asm/gpio.h>
#include <mach/vreg.h>
#include <mach/board_lge.h>

#define PANEL_DEBUG 0

#define LCD_CONTROL_BLOCK_BASE	0x110000
#define INTFLG		LCD_CONTROL_BLOCK_BASE|(0x18)
#define INTMSK		LCD_CONTROL_BLOCK_BASE|(0x1c)
#define VPOS		LCD_CONTROL_BLOCK_BASE|(0xc0)

static boolean is_lcd_on = -1;

/* The comment from AMSS codes:
 * Dot clock (10MHz) / pixels per row (320) = rows_per_second
 * Rows Per second, this number arrived upon empirically 
 * after observing the timing of Vsync pulses
 * XXX: TODO: change this values for INNOTEK PANEL */
static uint32 mddi_auo_rows_per_second = 31250;
static uint32 mddi_auo_rows_per_refresh = 480;
extern boolean mddi_vsync_detect_enabled;

static msm_fb_vsync_handler_type mddi_auo_vsync_handler = NULL;
static void *mddi_auo_vsync_handler_arg;
static uint16 mddi_auo_vsync_attempts;

#if defined(CONFIG_FB_MSM_MDDI_NOVATEK_HITACHI_HVGA)
extern int g_mddi_lcd_probe;
#endif

#if defined(CONFIG_MACH_MSM7X27_GELATO)
static struct msm_panel_auo_pdata *mddi_auo_pdata;
#else
static struct msm_panel_common_pdata *mddi_auo_pdata;
#endif

static int mddi_auo_lcd_on(struct platform_device *pdev);
static int mddi_auo_lcd_off(struct platform_device *pdev);

static int mddi_auo_lcd_init(void);
static void mddi_auo_lcd_panel_poweron(void);
static void mddi_auo_lcd_panel_poweroff(void);


#define DEBUG 1
#if DEBUG
#define EPRINTK(fmt, args...) printk(fmt, ##args)
#else
#define EPRINTK(fmt, args...) do { } while (0)
#endif

struct display_table {
    unsigned reg;
    unsigned char count;
    unsigned char val_list[20];
};

#define REGFLAG_DELAY             0XFFFE
#define REGFLAG_END_OF_TABLE      0xFFFF   // END OF REGISTERS MARKER

static struct display_table mddi_auo_position_table[] = {
	// PASET (set page address)
	{0x2B, 4, {0x00, 0x00, 0x01, 0xDF}},
	// CASET (set column address)
	{0x2A, 4, {0x00, 0x00, 0x01, 0x3F}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

#if 0
static struct display_table2 mddi_hitachi_img[] = {
	{0x2c, 16384, {}},
};
static struct display_table mddi_hitachi_img_end[] = {
	{0x00, 0, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

static struct display_table mddi_auo_display_on[] = {
	// Display on sequence
	{0x11, 4, {0x00, 0x00, 0x00, 0x00}},
	{REGFLAG_DELAY, 80, {}},
	{0x29, 4, {0x00, 0x00, 0x00, 0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct display_table mddi_auo_sleep_mode_on_data[] = {
	// Display off / sleep sequence
	{0x28, 4, {0x00, 0x00, 0x00, 0x00}},
	{REGFLAG_DELAY, 20, {}},
	{0x10, 4, {0x00, 0x00, 0x00, 0x00}},
	{REGFLAG_DELAY, 40, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct display_table mddi_auo_initialize[] = {

	// PASSWD1 (Release the protection of L2 CMD)
	{0xF0, 4, {0x5A, 0x5A, 0x00, 0x00}},

	// PASSWD2 (Release the protection of L2 CMD)
	{0xF1, 4, {0x5A, 0x5A, 0x00, 0x00}},

	// DISCTL 
	{0xF2, 20, {0x3B, 0x48, 0x03, 0x08, 0x08, 0x08, 0x08, 0x00, 
			 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x54, 0x08, 
			 0x08, 0x08, 0x08, 0x00}},

	// PWRCTL 
	{0xF4, 16, {0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
			 0x3F, 0x79, 0x03, 0x3F, 0x79, 0x03, 0x00, 0x00}},

	// VCMCTL 
	{0xF5, 12, {0x00, 0x5D, 0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 
			 0x04, 0x00, 0x5D, 0x75}},

	// SRCCTL
	{0xF6, 8, {0x04, 0x00, 0x08, 0x03, 0x01, 0x00, 0x01, 0x00}},

	// IFCTL (Interface Control)
	{0xF7, 8, {0x48, 0x80, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00}},

	// PANELCTL
	{0xF8, 4, {0x11, 0x00, 0x00, 0x00}},

	// GAMMASEL
	{0xF9, 4, {0x24, 0x00, 0x00, 0x00}}, // Red

	// PGAMMACTL
	{0xFA, 16, {0x0B, 0x0B, 0x0C, 0x1F, 0x1F, 0x27, 0x2F, 0x14, 
			0x21, 0x26, 0x32, 0x31, 0x24, 0x00, 0x00, 0x01}},

	// GAMMASEL
	{0xF9, 4, {0x22, 0x00, 0x00, 0x00}}, // Green

	// PGAMMACTL
	{0xFA, 16, {0x0B, 0x0B, 0x0E, 0x27, 0x29, 0x30, 0x33, 0x12, 
			0x1F, 0x25, 0x31, 0x30, 0x24, 0x00, 0x00, 0x01}},

	// GAMMASEL
	{0xF9, 4, {0x21, 0x00, 0x00, 0x00}}, // Blue

	// PGAMMACTL
	{0xFA, 16, {0x0B, 0x0B, 0x1A, 0x3A, 0x3F, 0x3F, 0x3F, 0x08, 
			0x19, 0x21, 0x2C, 0x2A, 0x1A, 0x00, 0x00, 0x01}},

	// COLMOD
	{0x3A, 4, {0x55, 0x00, 0x00, 0x00}}, // 55H=16bits/pixel

	// MADCTL
	{0x36, 4, {0x00, 0x00, 0x00, 0x00}},

	// TEON
	{0x35, 4, {0x00, 0x00, 0x00, 0x00}},

	// CASET
	{0x2A, 4, {0x00, 0x00, 0x01, 0x3F}},

	// PASET
	{0x2B, 4, {0x00, 0x00, 0x01, 0xDF}},

	{REGFLAG_END_OF_TABLE, 0x00, {0}}
};

static struct display_table mddi_auo_initialize_20110324[] = {

	// PASSWD1 (Release the protection of L2 CMD)
	{0xF0, 4, {0x5A, 0x5A, 0x00, 0x00}},

	// PASSWD2 (Release the protection of L2 CMD)
	{0xF1, 4, {0x5A, 0x5A, 0x00, 0x00}},

	// DISCTL 
	{0xF2, 20, {0x3B, 0x48, 0x03, 0x08, 0x08, 0x08, 0x08, 0x00, 
			 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x54, 0x08, 
			 0x08, 0x08, 0x08, 0x00}},

	// PWRCTL
	{0xF4, 16, {0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
			 0x3F, 0x79, 0x03, 0x3F, 0x79, 0x03, 0x00, 0x00}},

	// VCMCTL 
	{0xF5, 12, {0x00, 0x5D, 0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 
			 0x04, 0x00, 0x5D, 0x75}},

	// SRCCTL
	{0xF6, 8, {0x04, 0x00, 0x08, 0x03, 0x01, 0x00, 0x01, 0x00}},

	// IFCTL (Interface Control)
	{0xF7, 8, {0x48, 0x80, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00}},

	// PANELCTL
	{0xF8, 4, {0x11, 0x00, 0x00, 0x00}},

	// GAMMASEL
	{0xF9, 4, {0x24, 0x00, 0x00, 0x00}}, // Red

	// PGAMMACTL 
	{0xFA, 16, {0x0B, 0x0B, 0x10, 0x34, 0x35, 0x2E, 0x2E, 0x13, 
			0x22, 0x26, 0x37, 0x2F, 0x24, 0x00, 0x00, 0x01}},

	// GAMMASEL
	{0xF9, 4, {0x22, 0x00, 0x00, 0x00}}, // Green

	// PGAMMACTL
	{0xFA, 16, {0x0B, 0x0B, 0x10, 0x35, 0x35, 0x3C, 0x38, 0x11, 
			0x20, 0x25, 0x37, 0x2F, 0x24, 0x00, 0x00, 0x01}},

	// GAMMASEL
	{0xF9, 4, {0x21, 0x00, 0x00, 0x00}}, // Blue

	// PGAMMACTL
	{0xFA, 16, {0x0B, 0x0B, 0x1A, 0x3A, 0x3F, 0x3F, 0x3F, 0x08, 
			0x1B, 0x21, 0x35, 0x2C, 0x24, 0x00, 0x00, 0x01}},

	// COLMOD
	{0x3A, 4, {0x55, 0x00, 0x00, 0x00}}, // 55H=16bits/pixel

	// MADCTL
	{0x36, 4, {0x00, 0x00, 0x00, 0x00}},

	// TEON
	{0x35, 4, {0x00, 0x00, 0x00, 0x00}},

	// PASET
	{0x2B, 4, {0x00, 0x00, 0x01, 0xDF}},

	// CASET
	{0x2A, 4, {0x00, 0x00, 0x01, 0x3F}},

	{REGFLAG_END_OF_TABLE, 0x00, {0}}
};

static struct display_table mddi_auo_initialize_20110330[] = {

	// PASSWD1 (Release the protection of L2 CMD)
	{0xF0, 4, {0x5A, 0x5A, 0x00, 0x00}},

	// PASSWD2 (Release the protection of L2 CMD)
	{0xF1, 4, {0x5A, 0x5A, 0x00, 0x00}},

	// DISCTL : 0x48 -> frame rate = 70Hz
	{0xF2, 20, {0x3B, 0x48, 0x03, 0x08, 0x08, 0x08, 0x08, 0x00,
			0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x54, 0x08,
			0x08, 0x08, 0x08, 0x00}},

	// PWRCTL
	{0xF4, 16, {0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x3F, 0x79, 0x03, 0x3F, 0x79, 0x03, 0x00, 0x00}},

	// VCMCTL 
	{0xF5, 12, {0x00, 0x5D, 0x75, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x04, 0x00, 0x5D, 0x75}},

	// SRCCTL
	{0xF6, 8, {0x04, 0x00, 0x08, 0x03, 0x01, 0x00, 0x01, 0x00}},

	// IFCTL (Interface Control)
	{0xF7, 8, {0x48, 0x80, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00}},

	// PANELCTL
	{0xF8, 4, {0x11, 0x00, 0x00, 0x00}},

	// GAMMASEL
	{0xF9, 4, {0x24, 0x00, 0x00, 0x00}},

	// PGAMMACTL 
	{0xFA, 16, {0x0B, 0x0B, 0x05, 0x01, 0x0B, 0x20, 0x2C, 0x13,
			0x1C, 0x21, 0x23, 0x2F, 0x24, 0x00, 0x00, 0x01}},

	// GAMMASEL
	{0xF9, 4, {0x22, 0x00, 0x00, 0x00}},

	// PGAMMACTL
	{0xFA, 16, {0x0B, 0x0B, 0x10, 0x2C, 0x27, 0x2A, 0x30, 0x11,
			0x1A, 0x20, 0x26, 0x1F, 0x24, 0x00, 0x00, 0x01}},

	// GAMMASEL
	{0xF9, 4, {0x21, 0x00, 0x00, 0x00}},

	// PGAMMACTL
	{0xFA, 16, {0x0B, 0x0B, 0x1A, 0x3A, 0x3F, 0x3F, 0x3F, 0x07,
			0x14, 0x1D, 0x17, 0x18, 0x19, 0x00, 0x00, 0x01}},

	// COLMOD
	{0x3A, 4, {0x55, 0x00, 0x00, 0x00}}, // 55H=16bits/pixel

	// MADCTL
	{0x36, 4, {0x00, 0x00, 0x00, 0x00}},

	// TEON
	{0x35, 4, {0x00, 0x00, 0x00, 0x00}},

	// PASET
	{0x2B, 4, {0x00, 0x00, 0x01, 0xDF}},

	// CASET
	{0x2A, 4, {0x00, 0x00, 0x01, 0x3F}},
	
	{REGFLAG_END_OF_TABLE, 0x00, {0}}
};

#if defined(CONFIG_FB_MSM_MDDI_24BIT)
static struct display_table mddi_auo_initialize_24bit_full[] = {

	// PASSWD1 (Release the protection of L2 CMD)
	{0xF0, 4, {0x5A, 0x5A, 0x00, 0x00}},

	// PASSWD2 (Release the protection of L2 CMD)
	{0xF1, 4, {0x5A, 0x5A, 0x00, 0x00}},

	// DISCTL : 0x48 -> frame rate = 70Hz
	{0xF2, 20, {0x3B, 0x48, 0x03, 0x08, 0x08, 0x08, 0x08, 0x00,
			0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x54, 0x08,
			0x08, 0x08, 0x08, 0x00}},

	// PWRCTL
	{0xF4, 16, {0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x3F, 0x79, 0x03, 0x3F, 0x79, 0x03, 0x00, 0x00}},

	// VCMCTL 
	{0xF5, 12, {0x00, 0x5D, 0x75, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x04, 0x00, 0x5D, 0x75}},

	// SRCCTL
	{0xF6, 8, {0x04, 0x00, 0x08, 0x03, 0x01, 0x00, 0x01, 0x00}},

	// IFCTL (Interface Control)
	{0xF7, 8, {0x48, 0x80, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00}},

	// PANELCTL
	{0xF8, 4, {0x11, 0x00, 0x00, 0x00}},

	// GAMMASEL
	{0xF9, 4, {0x24, 0x00, 0x00, 0x00}},

	// PGAMMACTL 
	{0xFA, 16, {0x0B, 0x0B, 0x05, 0x01, 0x0B, 0x20, 0x2C, 0x13,
			0x1C, 0x21, 0x23, 0x2F, 0x24, 0x00, 0x00, 0x01}},

	// GAMMASEL
	{0xF9, 4, {0x22, 0x00, 0x00, 0x00}},

	// PGAMMACTL
	{0xFA, 16, {0x0B, 0x0B, 0x10, 0x2C, 0x27, 0x2A, 0x30, 0x11,
			0x1A, 0x20, 0x26, 0x1F, 0x24, 0x00, 0x00, 0x01}},

	// GAMMASEL
	{0xF9, 4, {0x21, 0x00, 0x00, 0x00}},

	// PGAMMACTL
	{0xFA, 16, {0x0B, 0x0B, 0x1A, 0x3A, 0x3F, 0x3F, 0x3F, 0x07,
			0x14, 0x1D, 0x17, 0x18, 0x19, 0x00, 0x00, 0x01}},

	// COLMOD
	{0x3A, 4, {0x77, 0x00, 0x00, 0x00}}, // 77H=24bits/pixel

	// MADCTL
	{0x36, 4, {0x00, 0x00, 0x00, 0x00}},

	// TEON
	{0x35, 4, {0x00, 0x00, 0x00, 0x00}},

	// PASET
	{0x2B, 4, {0x00, 0x00, 0x01, 0xDF}},

	// CASET
	{0x2A, 4, {0x00, 0x00, 0x01, 0x3F}},
	
	{REGFLAG_END_OF_TABLE, 0x00, {0}}
};
#endif

#if defined(CONFIG_FB_MSM_MDDI_24BIT)
static struct display_table mddi_auo_initialize_24bit[] = {
	// COLMOD : 0x77(24 bits/pixel)
	{0x3A, 4, {0x77, 0x00, 0x00, 0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

void auo_display_table(struct display_table *table, unsigned int count)
{
	unsigned int i;

    for(i = 0; i < count; i++) {
		
        unsigned reg;
        reg = table[i].reg;
		
        switch (reg) {
			
            case REGFLAG_DELAY :
                msleep(table[i].count);
				EPRINTK("%s() : delay %d msec\n", __func__, table[i].count);
                break;
				
            case REGFLAG_END_OF_TABLE :
                break;
				
            default:
                mddi_host_register_cmds_write8(reg, table[i].count, table[i].val_list, 1, 0, 0);
				//EPRINTK("%s: reg : %x, val : %x.\n", __func__, reg, table[i].val_list[0]);
       	}
    }
	
}

static void mddi_auo_vsync_set_handler(msm_fb_vsync_handler_type handler,	/* ISR to be executed */
					 void *arg)
{
	boolean error = FALSE;
	unsigned long flags;

	/* LGE_CHANGE [neo.kang@lge.com] 2009-11-26, change debugging api */
	printk("%s : handler = %x\n", 
			__func__, (unsigned int)handler);

	/* Disable interrupts */
	spin_lock_irqsave(&mddi_host_spin_lock, flags);
	/* INTLOCK(); */

	if (mddi_auo_vsync_handler != NULL) {
		error = TRUE;
	} else {
		/* Register the handler for this particular GROUP interrupt source */
		mddi_auo_vsync_handler = handler;
		mddi_auo_vsync_handler_arg = arg;
	}
	
	/* Restore interrupts */
	spin_unlock_irqrestore(&mddi_host_spin_lock, flags);
	/* MDDI_INTFREE(); */
	if (error) {
		printk("MDDI: Previous Vsync handler never called\n");
	} else {
		/* Enable the vsync wakeup */
		/* mddi_queue_register_write(INTMSK, 0x0000, FALSE, 0); */
		mddi_auo_vsync_attempts = 1;
		mddi_vsync_detect_enabled = TRUE;
	}
}

static void mddi_auo_lcd_vsync_detected(boolean detected)
{
	mddi_vsync_detect_enabled = TRUE;;
}

static int mddi_auo_lcd_on(struct platform_device *pdev)
{
	EPRINTK("%s: started.\n", __func__);

#if defined(CONFIG_MACH_MSM7X27_GELATO)
	if (system_state == SYSTEM_BOOTING && mddi_auo_pdata->initialized) {
		is_lcd_on = TRUE;
#if defined(CONFIG_FB_MSM_MDDI_24BIT)
		EPRINTK("%s: mddi_auo_initialize_24bit.\n", __func__);
		auo_display_table(mddi_auo_initialize_24bit, sizeof(mddi_auo_initialize_24bit)/sizeof(struct display_table));
#endif
		return 0;
	}
#endif

	// LCD HW Reset
	mddi_auo_lcd_panel_poweron();

#if defined(CONFIG_MACH_MSM7X27_THUNDERC) || defined(CONFIG_MACH_MSM7X27_GELATO)
#if defined(CONFIG_FB_MSM_MDDI_24BIT)
	auo_display_table(mddi_auo_initialize_24bit_full, sizeof(mddi_auo_initialize_24bit_full)/sizeof(struct display_table));
#else
	auo_display_table(mddi_auo_initialize_20110330, sizeof(mddi_auo_initialize_20110330)/sizeof(struct display_table));
#endif
	auo_display_table(mddi_auo_display_on, sizeof(mddi_auo_display_on) / sizeof(struct display_table));
#endif

	is_lcd_on = TRUE;
	return 0;
}

static int mddi_auo_lcd_off(struct platform_device *pdev)
{
	auo_display_table(mddi_auo_sleep_mode_on_data, sizeof(mddi_auo_sleep_mode_on_data)/sizeof(struct display_table));
	mddi_auo_lcd_panel_poweroff();
	is_lcd_on = FALSE;
	return 0;
}

ssize_t mddi_auo_lcd_show_onoff(struct device *dev, struct device_attribute *attr, char *buf)
{
	EPRINTK("%s : strat\n", __func__);
	return 0;
}

ssize_t mddi_auo_lcd_store_onoff(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev); 
	int onoff;

	sscanf(buf, "%d", &onoff);

	EPRINTK("%s: onoff : %d\n", __func__, onoff);
	
	if(onoff) {
		mddi_auo_lcd_on(pdev);
	}
	else {
		mddi_auo_lcd_off(pdev);
	}

	return count;
}

int mddi_auo_position(void)
{
	auo_display_table(mddi_auo_position_table, ARRAY_SIZE(mddi_auo_position_table));
	return 0;
}
EXPORT_SYMBOL(mddi_auo_position);

/* LGE_CHANGE [james.jang@lge.com] 2010-08-28, probe LCD */
DEVICE_ATTR(lcd_onoff, 0665, mddi_auo_lcd_show_onoff, mddi_auo_lcd_store_onoff);

struct msm_fb_panel_data auo_panel_data0 = {
	.on = mddi_auo_lcd_on,
	.off = mddi_auo_lcd_off,
	.set_backlight = NULL,
	.set_vsync_notifier = mddi_auo_vsync_set_handler,
};

static struct platform_device this_device_0 = {
	.name   = "mddi_auo_hvga",
	.id	= MDDI_LCD_AUO,
	.dev	= {
		.platform_data = &auo_panel_data0,
	}
};

static int mddi_auo_lcd_probe(struct platform_device *pdev)
{
	int ret;
	EPRINTK("%s: started.\n", __func__);

	if (pdev->id == 0) {
		mddi_auo_pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_add_device(pdev);

/* LGE_CHANGE [james.jang@lge.com] 2010-08-28, probe LCD */
	ret = device_create_file(&pdev->dev, &dev_attr_lcd_onoff);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mddi_auo_lcd_probe,
	.driver = {
		.name   = "mddi_auo_hvga",
	},
};

static int mddi_auo_lcd_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

#ifdef CONFIG_FB_MSM_MDDI_AUTO_DETECT
	u32 id;
	id = mddi_get_client_id();

	/* TODO: Check client id */

#endif

/* LGE_CHANGE [james.jang@lge.com] 2010-08-28, probe LCD */
#if defined(CONFIG_LGE_PCB_REV_A)
#if defined(CONFIG_FB_MSM_MDDI_NOVATEK_HITACHI_HVGA)
  gpio_tlmm_config(GPIO_CFG(101, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
	gpio_configure(101, GPIOF_INPUT);
  if (gpio_get_value(101) != 0)
		return -ENODEV;
	g_mddi_lcd_probe = 0;
#endif
#else
#if defined(CONFIG_FB_MSM_MDDI_NOVATEK_HITACHI_HVGA)
  gpio_tlmm_config(GPIO_CFG(93, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
	gpio_configure(93, GPIOF_INPUT);
  if (gpio_get_value(93) != 0)
		return -ENODEV;
	g_mddi_lcd_probe = 0;
#endif

#endif

	ret = platform_driver_register(&this_driver);
	if (!ret) {
		pinfo = &auo_panel_data0.panel_info;
		EPRINTK("%s: setting up panel info.\n", __func__);
		pinfo->xres = 320;
		pinfo->yres = 480;
		pinfo->type = MDDI_PANEL;
		pinfo->pdest = DISPLAY_1;
		pinfo->mddi.vdopkt = 0x23;//MDDI_DEFAULT_PRIM_PIX_ATTR;
		pinfo->wait_cycle = 0;
#if defined(CONFIG_FB_MSM_MDDI_24BIT)
		pinfo->bpp = 24;
#else
		pinfo->bpp = 16;
#endif
		// vsync config
		pinfo->lcd.vsync_enable = TRUE;
		pinfo->lcd.refx100 = (mddi_auo_rows_per_second * 100) /
                        		mddi_auo_rows_per_refresh;

/* LGE_CHANGE.
  * Change proch values to resolve LCD Tearing. Before BP:14, FP:6. After BP=FP=6.
  * The set values on LCD are both 8, but we use 6 for MDDI in order to secure timing margin.
  * 2010-08-21, minjong.gong@lge.com
  */
		pinfo->lcd.v_back_porch = 6 /*200*/;
		pinfo->lcd.v_front_porch = 6 /*200*/;
		pinfo->lcd.v_pulse_width = 4 /*30*/;

		pinfo->lcd.hw_vsync_mode = TRUE;
		pinfo->lcd.vsync_notifier_period = (1 * HZ);

		pinfo->bl_max = 4;
		pinfo->bl_min = 1;

		pinfo->clk_rate =10000000 /* 122880000*/;
		pinfo->clk_min = 9000000/*120000000*/;
		pinfo->clk_max =11000000 /* 130000000*/;
		pinfo->fb_num = 2;

		ret = platform_device_register(&this_device_0);
		if (ret) {
			EPRINTK("%s: this_device_0 register success\n", __func__);
			platform_driver_unregister(&this_driver);
		}
	}

	if(!ret) {
		mddi_lcd.vsync_detected = mddi_auo_lcd_vsync_detected;
	}

	return ret;
}

extern unsigned fb_width;
extern unsigned fb_height;

static void mddi_auo_lcd_panel_poweron(void)
{
	struct msm_panel_auo_pdata *pdata = mddi_auo_pdata;

	EPRINTK("%s: started.\n", __func__);

	fb_width = 320;
	fb_height = 480;

	if(pdata && pdata->gpio) {
		gpio_set_value(pdata->gpio, 1);
		mdelay(10);
		gpio_set_value(pdata->gpio, 0);
		mdelay(50);
		gpio_set_value(pdata->gpio, 1);
		mdelay(50);
	}
}

#if 0
static void mddi_auo_lcd_panel_store_poweron(void)
{
//	struct msm_panel_common_pdata *pdata = mddi_auo_pdata;
	struct msm_panel_auo_pdata *pdata = mddi_auo_pdata;

	EPRINTK("%s: started.\n", __func__);

	fb_width = 320;
	fb_height = 480;

	if(pdata && pdata->gpio) {
		gpio_set_value(pdata->gpio, 1);
		mdelay(10);
		gpio_set_value(pdata->gpio, 0);
		mdelay(20);
		gpio_set_value(pdata->gpio, 1);
		mdelay(50);
	}
}
#endif

/* LGE_CHANGE
  * Add new function to reduce current comsumption in sleep mode.
  * In sleep mode disable LCD by assertion low on reset pin.
  * 2010-06-07, minjong.gong@lge.com
  */
static void mddi_auo_lcd_panel_poweroff(void)
{
	struct msm_panel_auo_pdata *pdata = mddi_auo_pdata;

	EPRINTK("%s: started.\n", __func__);

	fb_width = 320;
	fb_height = 480;

	if(pdata && pdata->gpio) {
		gpio_set_value(pdata->gpio, 0);
		mdelay(5);
	}
}
module_init(mddi_auo_lcd_init);
