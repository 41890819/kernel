#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pwm_backlight.h>
#include <linux/digital_pulse_backlight.h>
#include <mach/jzfb.h>

#include "../board_base.h"

#ifdef CONFIG_LCD_ECX336AF
#include <linux/ecx336af.h>
static struct platform_ecx336af_data ecx336af_pdata= {
#ifdef ECX336AF_GPIO_RESET
        .gpio_reset = ECX336AF_GPIO_RESET,
#else
        .gpio_reset = -1,
#endif

#ifdef ECX336AF_SPI_CS
	.gpio_spi_cs  = ECX336AF_SPI_CS,
#else
	.gpio_spi_cs = -1,
#endif

#ifdef ECX336AF_SPI_CLK
	.gpio_spi_clk = ECX336AF_SPI_CLK,
#else
	.gpio_spi_clk = -1,
#endif

#ifdef ECX336AF_SPI_SDI
	.gpio_spi_sdi = ECX336AF_SPI_SDI,
#else
	.gpio_spi_sdi = -1,
#endif

#ifdef ECX336AF_SPI_SDO
	.gpio_spi_sdo = ECX336AF_SPI_SDO,
#else
	.gpio_spi_sdo = -1,
#endif

	.power_1v8 = NULL,
};

/* LCD device */
struct platform_device ecx336af_device = {
	.name		= "ecx336af-lcd",
	.dev		= {
		.platform_data = &ecx336af_pdata,
	},
};

struct fb_videomode jzfb_ecx336af_videomode = {
	.name = "640x400",
	.refresh = 60,
	.xres = 640,
	.yres = 400,
	.pixclock = KHZ2PICOS(27027),
	.left_margin = 58,
	.right_margin = 96,
	.upper_margin = 32,
	.lower_margin = 87,
	.hsync_len = 64,
	.vsync_len = 6,
	.sync = ~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

struct jzfb_platform_data jzfb_pdata = {
	.num_modes = 1,
	.modes = &jzfb_ecx336af_videomode,
	.lcd_type = LCD_TYPE_GENERIC_24_BIT,
	/* 
	 * Origin bpp is RGB888, but we change it to RGB565 
	 * This will reduce memory access and save power
	 */
	.bpp = 24,
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

extern void ecx336af_backlight(unsigned char brightness);
static int backlight_notify(struct device *dev, int brightness)
{
	if (brightness > 0 && brightness < 27){
	  brightness = 140;
	}else if (brightness >= 27 && brightness < 94){
	  brightness = 165;
	}else if (brightness >= 94 && brightness < 158){
	  brightness = 200;
	}else if (brightness >= 158 && brightness < 222){
	  brightness = 225;
	}else if (brightness >= 222 && brightness < 256){
	  brightness = 255;
	}else{
	  return 0;
	}

	ecx336af_backlight(brightness);

	return 0;
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
