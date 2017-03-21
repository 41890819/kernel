#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pwm_backlight.h>
#include <linux/digital_pulse_backlight.h>
#include <mach/jzfb.h>

#include "board.h"

#ifdef CONFIG_LCD_HIMAX7097
/* LCD device */
struct platform_device himax7097_device = {
	.name		= "himax7097-lcd",
	.dev		= {
		.platform_data = NULL,
	},
};

struct platform_device himax7097_second_device = {
	.name		= "himax7097-second-lcd",
	.dev		= {
		.platform_data = NULL,
	},
};

struct fb_videomode jzfb_himax7097_videomode = {
	.name = "800*480",
	.refresh = 120,
	.xres = 800,
	.yres = 480,
	.pixclock     = KHZ2PICOS(56000),
	.left_margin  = 40,
	.right_margin = 40,
	.upper_margin = 24,
	.lower_margin = 24,
	.hsync_len = 0,
	.vsync_len = 0,
	.sync = 0 | 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

struct jzfb_platform_data jzfb_pdata = {
	.num_modes      = 1,
	.modes          = &jzfb_himax7097_videomode,
	.lcd_type       = LCD_TYPE_GENERIC_24_BIT,
	/* 
	 * Origin bpp is RGB888, but we change it to RGB565 
	 * This will reduce memory access and save power
	 */
	.bpp = 16,
	.pixclk_falling_edge = 1,
	//.date_enable_active_low = 0,
	.dither_enable = 0,
	//.alloc_vidmem = 1,
#ifdef CONFIG_BACKLIGHT_PWM_EN
	.backlight_enable = BACKLIGHT_PWM_EN,
#endif
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
static int init_backlight(struct device *dev)
{
	return 0;
}
static void exit_backlight(struct device *dev)
{

}

struct platform_digital_pulse_backlight_data bl_data = {
	.digital_pulse_gpio = GPIO_PE(1),
	.max_brightness = 255,
	.dft_brightness = 120,
	.max_brightness_step = 16,
	.high_level_delay_us = 5,
	.low_level_delay_us = 5,
	.init = init_backlight,
	.exit = exit_backlight,
};


struct platform_device digital_pulse_backlight_device = {
	.name		= "digital-pulse-backlight",
	.dev		= {
		.platform_data	= &bl_data,
	},
};
#endif
