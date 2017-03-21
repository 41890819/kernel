#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pwm_backlight.h>
#include <linux/digital_pulse_backlight.h>
#include <mach/jzfb.h>

#include "../board_base.h"

#ifdef CONFIG_OLED_SSD1306
/* LCD device */
struct platform_device ssd1306_device = {
	.name		= "SSD1306_oled",
	.dev		= {
		.platform_data = NULL,
	},
};

struct fb_videomode jzfb_ssd1306_videomode = {
	.name = "320x240",
	.refresh = 30,
	.xres = 320,
	.yres = 240,
	.pixclock = KHZ2PICOS(2304),
	.left_margin = 0,
	.right_margin = 0,
	.upper_margin = 0,
	.lower_margin = 0,
	.hsync_len = 0,
	.vsync_len = 0,
	.sync = ~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

struct jzfb_platform_data jzfb_pdata = {
	.num_modes = 1,
	.modes = &jzfb_ssd1306_videomode,
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

static struct platform_pwm_backlight_data backlight_data = {
	.pwm_id		= BACKLIGHT_PWM_ID,
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
	.digital_pulse_gpio = GPIO_GIGITAL_PULSE,
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
