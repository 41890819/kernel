/*
 * Copyright (c) 2016 Ingenic Semiconductor Co.,Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Author :   Kznan <Derrick.kznan@ingenic.com>
 * Description :  lcd driver for ili9342c
 *
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/pwm_backlight.h>
#include <linux/lcd.h>
#include <linux/regulator/consumer.h>
#include <mach/jzfb.h>

#include "../board_base.h"

#define DEBUG(x...) printk("ili9342c_slcd : " x)

struct ili9342c_power {
	struct regulator *vlcdio;
	struct regulator *vlcdvcc;
	int inited;
};

static struct ili9342c_power lcd_power = {
	NULL,
	NULL,
	0
};

int ili9342c_power_init(struct lcd_device *ld)
{
	if (lcd_power.inited == 1)
		return 0;

	// This is necessary
	if (GPIO_LCD_RST > 0) {
		int ret = gpio_request(GPIO_LCD_RST, "lcd rst");
		if (ret) {
			DEBUG("LCD reset gpio request fail\n");
			return -1;
		}
	}

	if (GPIO_LCD_CS > 0) {
		int ret = gpio_request(GPIO_LCD_CS, "lcd cs");
		if (ret) {
			DEBUG("LCD chip select gpio request fail\n");
			gpio_free(GPIO_LCD_RST);
			return -1;
		}
	}

	// do init action if we can
	if (GPIO_LCD_RD > 0) {
		int ret = gpio_request(GPIO_LCD_RD, "lcd rd");
		if (ret) {
			DEBUG("LCD read gpio request fail\n");
		} else {
			gpio_direction_output(GPIO_LCD_RD, 1);
		}
	}

	lcd_power.vlcdio = regulator_get(&ld->dev, "lcd_iovcc");
	if ( IS_ERR(lcd_power.vlcdio) ) {
		DEBUG("get lcd iovcc error\n");
		lcd_power.vlcdio = NULL;
	}

	lcd_power.vlcdvcc = regulator_get(&ld->dev, "lcd_vcc");
	if ( IS_ERR(lcd_power.vlcdvcc) ) {
		DEBUG("get lcd vcc error\n");
		lcd_power.vlcdvcc = NULL;
	}

	lcd_power.inited = 1;

	return 0;
}

int ili9342c_power_reset(struct lcd_device *ld)
{
	if (lcd_power.inited == 0)
		return -EFAULT;

	gpio_direction_output(GPIO_LCD_RST, 1);
	mdelay(5);
	gpio_direction_output(GPIO_LCD_RST, 0);
	mdelay(10);
	gpio_direction_output(GPIO_LCD_RST, 1);
	mdelay(120);

	return 0;
}

int ili9342c_power_on(struct lcd_device *ld, int enable)
{
	ili9342c_power_init(ld);

	if (lcd_power.inited == 0)
		return -EFAULT;

	if (enable) {
		if (lcd_power.vlcdio != NULL)
			regulator_enable(lcd_power.vlcdio);

		if (lcd_power.vlcdvcc != NULL)
			regulator_enable(lcd_power.vlcdvcc);

		gpio_direction_output(GPIO_LCD_CS, 0);
		ili9342c_power_reset(ld);
	} else {
		if (lcd_power.vlcdio != NULL)
			regulator_disable(lcd_power.vlcdio);

		if (lcd_power.vlcdvcc != NULL)
			regulator_disable(lcd_power.vlcdvcc);

		gpio_direction_output(GPIO_LCD_CS, 1);
	}

	return 0;
}

struct lcd_platform_data ili9342c_pdata = {
	.reset    = ili9342c_power_reset,
	.power_on = ili9342c_power_on,
};

/* LCD Panel Device */
struct platform_device ili9342c_device = {
	.name		= "ili9342c_slcd",
	.dev		= {
		.platform_data	= &ili9342c_pdata,
	},
};

static struct smart_lcd_data_table ili9342c_data_table[] = {
	// Show color bar
	/* {SMART_CONFIG_CMD, 0x11}, */
	/* {SMART_CONFIG_CMD, 0x29}, */

	{SMART_CONFIG_CMD, 0xC8},
	{SMART_CONFIG_DATA, 0xFF},
	{SMART_CONFIG_DATA, 0x93},
	{SMART_CONFIG_DATA, 0x42},

	// Set screen direction
	{SMART_CONFIG_CMD, 0x36},
	{SMART_CONFIG_DATA, 0x98},

	{SMART_CONFIG_CMD, 0xB4},
	{SMART_CONFIG_DATA, 0x02},

	// Blanking proch control---vfp/vbp
	{SMART_CONFIG_CMD, 0xB5},
	{SMART_CONFIG_DATA, 0x1A},
	{SMART_CONFIG_DATA, 0x1A},

	{SMART_CONFIG_CMD, 0x44},
	{SMART_CONFIG_DATA, 0x1A},

	// Display function control
	{SMART_CONFIG_CMD, 0xB6},
	{SMART_CONFIG_DATA, 0x0A},
	{SMART_CONFIG_DATA, 0xC0},

	{SMART_CONFIG_CMD, 0x3A},
	{SMART_CONFIG_DATA, 0x66},

	{SMART_CONFIG_CMD, 0x35},
	{SMART_CONFIG_DATA, 0x00},

	{SMART_CONFIG_CMD, 0xC0},
	{SMART_CONFIG_DATA, 0x0F},
	{SMART_CONFIG_DATA, 0x0F},

	{SMART_CONFIG_CMD, 0xC1},
	{SMART_CONFIG_DATA, 0x06},

	{SMART_CONFIG_CMD, 0xC5},
	{SMART_CONFIG_DATA, 0xDF},

	// Set Gmma
	{SMART_CONFIG_CMD, 0xE0},
	{SMART_CONFIG_DATA, 0x00},
	{SMART_CONFIG_DATA, 0x08},
	{SMART_CONFIG_DATA, 0x13},
	{SMART_CONFIG_DATA, 0x08},
	{SMART_CONFIG_DATA, 0x16},
	{SMART_CONFIG_DATA, 0x09},
	{SMART_CONFIG_DATA, 0x44},
	{SMART_CONFIG_DATA, 0x67},
	{SMART_CONFIG_DATA, 0x54},
	{SMART_CONFIG_DATA, 0x06},
	{SMART_CONFIG_DATA, 0x0C},
	{SMART_CONFIG_DATA, 0x08},
	{SMART_CONFIG_DATA, 0x24},
	{SMART_CONFIG_DATA, 0x26},
	{SMART_CONFIG_DATA, 0x0F},

	// Set Gmma
	{SMART_CONFIG_CMD, 0xE1},
	{SMART_CONFIG_DATA, 0x00},
	{SMART_CONFIG_DATA, 0x2A},
	{SMART_CONFIG_DATA, 0x2F},
	{SMART_CONFIG_DATA, 0x00},
	{SMART_CONFIG_DATA, 0x0C},
	{SMART_CONFIG_DATA, 0x01},
	{SMART_CONFIG_DATA, 0x42},
	{SMART_CONFIG_DATA, 0x22},
	{SMART_CONFIG_DATA, 0x52},
	{SMART_CONFIG_DATA, 0x05},
	{SMART_CONFIG_DATA, 0x0E},
	{SMART_CONFIG_DATA, 0x0D},
	{SMART_CONFIG_DATA, 0x36},
	{SMART_CONFIG_DATA, 0x38},
	{SMART_CONFIG_DATA, 0x0F},

	{SMART_CONFIG_CMD, 0x11}, // Exit sleep
	{SMART_CONFIG_UDELAY, 120000}, // Delay 120 ms
	{SMART_CONFIG_CMD, 0x29}, // Display on
};

unsigned long truly_cmd_buf[]= {
	0x2C2C2C2C,
};

struct fb_videomode jzfb0_videomode = {
	.name = "320x240",
	.refresh = 70,
	.xres = 320,
	.yres = 240,
	// For 8-bit bus SLCD, send a 24-bit data need 3 times
	.pixclock = KHZ2PICOS(18749),
	.left_margin = 20,
	.right_margin = 10,
	.upper_margin = 2,
	.lower_margin = 4,
	.hsync_len = 10,
	.vsync_len = 2,
	.sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};


struct jzfb_platform_data jzfb_pdata = {
	.num_modes = 1,
	.modes = &jzfb0_videomode,
	.lcd_type = LCD_TYPE_SLCD,
	.bpp    = 24,
	.width  = 31,
	.height = 31,
	.pinmd  = 0,

	.smart_config.rsply_cmd_high       = 0,
	.smart_config.csply_active_high    = 0,
	.smart_config.newcfg_fmt_conv =  1,
	// write graphic ram command, in word, for example 8-bit bus, write_gram_cmd=C3C2C1C0.
	.smart_config.write_gram_cmd = truly_cmd_buf,
	.smart_config.length_cmd = ARRAY_SIZE(truly_cmd_buf),
	.smart_config.bus_width = 8,
	.smart_config.length_data_table =  ARRAY_SIZE(ili9342c_data_table),
	.smart_config.data_table = ili9342c_data_table,
	.dither_enable = 0,
};

/**************************************************************************************************/
#ifdef CONFIG_BACKLIGHT_PWM
static int backlight_init(struct device *dev)
{
	return 0;
}

static void backlight_exit(struct device *dev)
{
}

static int backlight_notify(struct device *dev, int brightness)
{
#if defined(CONFIG_CONVERT_PWM_OUTPUT)
	struct platform_pwm_backlight_data *data = dev->platform_data;
	return (data->max_brightness - brightness);
#else
	return brightness;
#endif
}

static struct platform_pwm_backlight_data backlight_data = {
	.pwm_id		= BACKLIGHT_PWM_ID,
	.max_brightness	= 255,
	.dft_brightness	= 120,
	.pwm_period_ns	= 30000,
	.init		= backlight_init,
	.exit		= backlight_exit,
	.notify         = backlight_notify,
};

struct platform_device backlight_device = {
	.name		= "pwm-backlight",
	.dev		= {
		.platform_data	= &backlight_data,
	},
};
#endif
