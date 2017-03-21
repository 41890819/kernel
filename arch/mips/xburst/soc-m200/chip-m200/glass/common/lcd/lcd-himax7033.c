#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pwm_backlight.h>
#include <linux/digital_pulse_backlight.h>
#include <mach/jzfb.h>

#include "../board_base.h"

#ifdef CONFIG_LCD_HIMAX7033
/* LCD device */
struct platform_device himax7033_device = {
	.name		= "himax7033",
	.dev		= {
		.platform_data = NULL,
	},
};

struct fb_videomode jzfb_himax7033_videomode = {
	.name = "320x240",
	.refresh = 120,
	.xres = 320,
	.yres = 240,
	.pixclock = KHZ2PICOS(14400),
	.left_margin = 40,
	.right_margin = 40,
	.upper_margin = 30,
	.lower_margin = 30,
	.hsync_len = 0,
	.vsync_len = 0,
	.sync = ~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

struct jzfb_platform_data jzfb_pdata = {
	.num_modes = 1,
	.modes = &jzfb_himax7033_videomode,
	.lcd_type = LCD_TYPE_GENERIC_24_BIT,
	/* 
	 * Origin bpp is RGB888, but we change it to RGB565 
	 * This will reduce memory access and save power
	 */
	.bpp = 16,
	.width = 45,
	.height = 75,
	/*this pixclk_edge is for lcd_controller sending data, which is reverse to lcd*/
	.pixclk_falling_edge = 1,
	.data_enable_active_low = 0,

};
#endif

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

/***********************************************************************************************/
#ifdef CONFIG_BACKLIGHT_DIGITAL_PULSE
struct platform_device digital_pulse_backlight_device = {
	.name		= "digital-pulse-backlight",
	.dev		= {
		.platform_data	= NULL,
	},
};
#endif
