#include "board_base.h"
#include <linux/leds.h>
#include <linux/platform_device.h>

#ifdef CONFIG_LEDS_GPIO_INGENIC
static struct gpio_led gpio_leds[] = {
#ifdef INGENIC_LED_GPIO_RED
	{
		.name			= "red",
		.gpio			= INGENIC_LED_GPIO_RED,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state		= LEDS_GPIO_DEFSTATE_OFF,
	},
#endif

#ifdef INGENIC_LED_GPIO_GREEN
	{
		.name			= "green",
		.gpio			= INGENIC_LED_GPIO_GREEN,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state		= LEDS_GPIO_DEFSTATE_OFF,
	},
#endif

#ifdef INGENIC_LED_GPIO_BLUE
	{
		.name			= "blue",
		.gpio			= INGENIC_LED_GPIO_BLUE,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state		= LEDS_GPIO_DEFSTATE_OFF,
	},
#endif

#ifdef INGENIC_LED_GPIO_ORANGE
	{
		.name			= "orange",
		.gpio			= INGENIC_LED_GPIO_ORANGE,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state		= LEDS_GPIO_DEFSTATE_OFF,
	},
#endif

#ifdef INGENIC_LED_GPIO_YELLOW
	{
		.name			= "yellow",
		.gpio			= INGENIC_LED_GPIO_YELLOW,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state		= LEDS_GPIO_DEFSTATE_OFF,
	},
#endif

#ifdef INGENIC_LED_GPIO_RED1
	{
		.name			= "red1",
		.gpio			= INGENIC_LED_GPIO_RED1,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state		= LEDS_GPIO_DEFSTATE_OFF,
	},
#endif

#ifdef INGENIC_LED_GPIO_RED2
	{
		.name			= "red2",
		.gpio			= INGENIC_LED_GPIO_RED2,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state		= LEDS_GPIO_DEFSTATE_OFF,
	},
#endif

#ifdef INGENIC_LED_GPIO_RED3
	{
		.name			= "red3",
		.gpio			= INGENIC_LED_GPIO_RED3,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state		= LEDS_GPIO_DEFSTATE_OFF,
	},
#endif
};

static struct gpio_led_platform_data ingenic_leds_info = {
	.num_leds	= ARRAY_SIZE(gpio_leds),
	.leds		= gpio_leds,
};

struct platform_device ingenic_leds = {
	.name	= "ingenic-leds",
	.id	= 0,
	.dev	= {
		.platform_data	= &ingenic_leds_info,
	}
};
#endif
