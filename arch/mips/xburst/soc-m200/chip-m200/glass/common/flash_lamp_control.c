#include <linux/gpio.h>
#include "board_base.h"
#include <linux/delay.h>
#include <linux/platform_device.h>

void flash_lamp_on(void){
  printk("flash_lamp_on\n");
#ifdef GPIO_FLASHLAMP
  gpio_request(GPIO_FLASHLAMP, "flash_lamp");
  gpio_direction_output(GPIO_FLASHLAMP, 1);
#endif
  msleep(1000);
}
EXPORT_SYMBOL(flash_lamp_on);

void flash_lamp_off(void){
  printk("flash_lamp_off\n");
#ifdef GPIO_FLASHLAMP
  gpio_request(GPIO_FLASHLAMP, "flash_lamp");
  gpio_direction_input(GPIO_FLASHLAMP);
#endif
}
EXPORT_SYMBOL(flash_lamp_off);

struct platform_device flashlamp_device = {
	.name = "flashlamp" ,
	.id = -1 ,
	.dev   = {
		.platform_data = NULL,
	},

};
