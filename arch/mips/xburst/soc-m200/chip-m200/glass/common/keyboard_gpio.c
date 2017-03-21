#include <linux/platform_device.h>

#include <linux/gpio_keys.h>
#include <linux/input.h>
#include "board_base.h"

struct gpio_keys_button __attribute__((weak)) board_buttons[] = {
#ifdef GPIO_HOME_KEY
	{
		.gpio		= GPIO_HOME_KEY,
		.code   	= KEY_HOME,
		.desc		= "home key",
		.active_low	= ACTIVE_LOW_HOME,
	},
#endif
#ifdef GPIO_MENU_KEY
	{
		.gpio		= GPIO_MENU_KEY,
		.code   	= KEY_MENU,
		.desc		= "menu key",
		.active_low	= ACTIVE_LOW_MENU,
	},
#endif
#ifdef GPIO_BACK_KEY
	{
		.gpio		= GPIO_BACK_KEY,
		.code   	= KEY_BACK,
		.desc		= "back key",
		.active_low	= ACTIVE_LOW_BACK,
	},
#endif
#ifdef GPIO_VOLUMEDOWN_KEY
	{
		.gpio		= GPIO_VOLUMEDOWN_KEY,
		.code   	= KEY_VOLUMEDOWN,
		.desc		= "volum down key",
		.active_low	= ACTIVE_LOW_VOLUMEDOWN,
	},
#endif
#ifdef GPIO_VOLUMEUP_KEY
	{
		.gpio		= GPIO_VOLUMEUP_KEY,
		.code   	= KEY_VOLUMEUP,
		.desc		= "volum up key",
		.active_low	= ACTIVE_LOW_VOLUMEUP,
	},
#endif

#ifdef GPIO_ENDCALL_KEY
	{
		.gpio           = GPIO_ENDCALL_KEY,
		.code           = KEY_POWER,
		.desc           = "end call key",
		.active_low     = ACTIVE_LOW_ENDCALL,
		.wakeup         = 1,
	},
#endif

#ifdef GPIO_CAMERA_KEY
	{
		.gpio		= GPIO_CAMERA_KEY,
		.code   	= KEY_CAMERA,
		.desc		= "camera key",
		.active_low	= ACTIVE_LOW_CAMERA,
		.wakeup         = 1,
		.debounce_interval = 10,
	},
#endif

#ifdef GPIO_KEY_A
	{
		.gpio		= GPIO_KEY_A,
		.code   	= KEY_A,
		.desc		= "key A",
		.active_low	= ACTIVE_LOW_A,
		.wakeup         = 1,
		.debounce_interval = 10,
	},
#endif

#ifdef GPIO_KEY_R
	{
		.gpio		= GPIO_KEY_R,
		.code   	= KEY_R,
		.desc		= "key R",
		.active_low	= ACTIVE_LOW_R,
		.wakeup         = 1,
		.debounce_interval = 10,
	},
#endif

#ifdef GPIO_KEY_L
#ifndef WAKE_EN_L
#define WAKE_EN_L 1
#endif
	{
		.gpio		= GPIO_KEY_L,
		.code   	= KEY_L,
		.desc		= "key L",
		.active_low	= ACTIVE_LOW_L,
		.wakeup         = WAKE_EN_L,
		.debounce_interval = 10,
	},
#endif

#ifdef GPIO_KEY_P
#ifndef WAKE_EN_P
#define WAKE_EN_P 1
#endif
	{
		.gpio		= GPIO_KEY_P,
		.code   	= KEY_P,
		.desc		= "key P",
		.active_low	= ACTIVE_LOW_P,
		.wakeup         = WAKE_EN_P,
		.debounce_interval = 10,
	},
#endif

#ifdef GPIO_KEY_D
	{
		.gpio		= GPIO_KEY_D,
		.code   	= KEY_D,
		.desc		= "key D",
		.active_low	= ACTIVE_LOW_D,
		.wakeup         = 1,
		.debounce_interval = 10,
	},
#endif

#ifdef GPIO_AR_KEY
	{
		.gpio		= GPIO_AR_KEY,
		.code   	= KEY_BACK,
		.desc		= "ar key",
		.active_low	= ACTIVE_LOW_AR,
		.debounce_interval = 10,
	},
#endif
};

static struct gpio_keys_platform_data board_button_data = {
	.buttons	= board_buttons,
	.nbuttons	= ARRAY_SIZE(board_buttons),
};

struct platform_device jz_button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
                .platform_data	= &board_button_data,
	}
};
