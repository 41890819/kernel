/*
 * Copyright (c) 2012 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * M200 dorado board lcd setup routines.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/pwm_backlight.h>
#include <linux/lcd.h>
#include <linux/interrupt.h>

#include <mach/jzfb.h>
#include "../board_base.h"

int himax7069_init(struct lcd_device *lcd)
{
    int ret = 0;

    if(GPIO_LCD_RST > 0){
	    gpio_free(GPIO_LCD_RST);
	    ret = gpio_request(GPIO_LCD_RST, "lcd rst");
	    if (ret) {
		    printk(KERN_ERR "can's request lcd rst\n");
		    return ret;
	    }
    }
    
    if(GPIO_MIPI_VDDA > 0){
	    gpio_free(GPIO_MIPI_VDDA);
	    ret = gpio_request(GPIO_MIPI_VDDA, "lcd mipi panel avcc");
	    if (ret) {
		    printk(KERN_ERR "can's request lcd panel avcc\n");
		    return ret;
	    }
    }
    
    return 0;
}

int himax7069_reset(struct lcd_device *lcd)
{
	if(himax7069_init(lcd))
		return -EFAULT;
	gpio_direction_output(GPIO_LCD_RST, 1);
	mdelay(300);
	gpio_direction_output(GPIO_LCD_RST, 0);  //reset active low
	mdelay(10);
	gpio_direction_output(GPIO_LCD_RST, 1);
	mdelay(5);
	return 0;
}

int himax7069_power_on(struct lcd_device *lcd, int enable)
{
	if(himax7069_init(lcd))
		return -EFAULT;
	gpio_direction_output(GPIO_MIPI_VDDA, enable); 
	mdelay(2);
	return 0;
}

struct lcd_platform_data himax7069_data = {
	.reset = himax7069_reset,
	.power_on= himax7069_power_on,
	.power_on_delay = 25,
	.power_off_delay = 200,
};

struct mipi_dsim_lcd_device himax7069_device={
	.name		= "himax7069",
	.id = 0,
	.platform_data = &himax7069_data,
}; 


unsigned long himax7069_cmd_buf[]= {
	0x2C2C2C2C,
};

struct fb_videomode jzfb_videomode = {
	.name = "himax7069",
	.refresh = 60,
	.xres = 640,
	.yres = 360,
	.pixclock = KHZ2PICOS(15504),
	.left_margin = 20,
	.right_margin = 19,
	.upper_margin = 9,
	.lower_margin = 10,
	.hsync_len = 1,
	.vsync_len = 1,
	.sync = ~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

struct jzdsi_data jzdsi_pdata = {
	.modes = &jzfb_videomode,
	.video_config.no_of_lanes = 2,
	.video_config.virtual_channel = 0,
	.video_config.color_coding = COLOR_CODE_24BIT,
	.video_config.byte_clock = DEFAULT_DATALANE_BPS / 8,
	.video_config.video_mode = VIDEO_BURST_WITH_SYNC_PULSES,
	.video_config.receive_ack_packets = 0,	/* enable receiving of ack packets */
	.video_config.is_18_loosely = 0, /*loosely: R0R1R2R3R4R5__G0G1G2G3G4G5G6__B0B1B2B3B4B5B6, not loosely: R0R1R2R3R4R5G0G1G2G3G4G5B0B1B2B3B4B5*/
	.video_config.data_en_polarity = 1,

	.dsi_config.max_lanes = 2,
	.dsi_config.max_hs_to_lp_cycles = 100,
	.dsi_config.max_lp_to_hs_cycles = 40,
	.dsi_config.max_bta_cycles = 4095,
	.dsi_config.color_mode_polarity = 1,
	.dsi_config.shut_down_polarity = 1,
};

struct jzfb_platform_data jzfb_pdata = {
	.num_modes = 1,
	.modes = &jzfb_videomode,
	.dsi_pdata = &jzdsi_pdata,

	.lcd_type = LCD_TYPE_SLCD,
	.bpp = 24,
	.width = 31,
	.height = 31,

	.smart_config.clkply_active_rising = 0,
	.smart_config.rsply_cmd_high = 0,
	.smart_config.csply_active_high = 0,
	.smart_config.write_gram_cmd = himax7069_cmd_buf,
	.smart_config.length_cmd = ARRAY_SIZE(himax7069_cmd_buf),
	.smart_config.bus_width = 8,

	.gpio_vdda = GPIO_MIPI_VDDA,
#ifdef CONFIG_BACKLIGHT_PWM_EN
	.backlight_enable = BACKLIGHT_PWM_EN,
#endif

};

#ifdef CONFIG_BACKLIGHT_PWM
static int backlight_init(struct device *dev)
{
	int ret;
	ret = gpio_request(GPIO_LCD_PWM, "Backlight");
	if (ret) {
		printk(KERN_ERR "failed to request GPF for PWM-OUT1\n");
		return ret;
	}

	return 0;
}

static void backlight_exit(struct device *dev)
{
	gpio_free(GPIO_LCD_PWM);
}

static struct platform_pwm_backlight_data backlight_data = {
	.pwm_id		= 1,
	.max_brightness	= 255,
	.dft_brightness	= 120,
	.pwm_period_ns	= 30000,
	.init		= backlight_init,
	.exit		= backlight_exit,
};

struct platform_device backlight_device = {
	.name		= "pwm-backlight",
	.dev		= {
		.platform_data	= &backlight_data,
	},
};

#endif
