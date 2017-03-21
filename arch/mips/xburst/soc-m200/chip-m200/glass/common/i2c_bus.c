#include <linux/platform_device.h>
#include <linux/i2c-gpio.h>
#include <board.h>

#ifdef CONFIG_I2C_GPIO
#define DEF_GPIO_I2C(NO)						\
	static struct i2c_gpio_platform_data i2c##NO##_gpio_data = {	\
		.sda_pin	= GPIO_I2C##NO##_SDA,			\
		.scl_pin	= GPIO_I2C##NO##_SCK,			\
	};								\
	struct platform_device i2c##NO##_gpio_device = {		\
		.name	= "i2c-gpio",					\
		.id	= NO,						\
		.dev	= { .platform_data = &i2c##NO##_gpio_data,},	\
	};

#ifdef CONFIG_SOFT_I2C0_GPIO_V12_JZ
DEF_GPIO_I2C(0);
#endif
#ifdef CONFIG_SOFT_I2C1_GPIO_V12_JZ
DEF_GPIO_I2C(1);
#endif
#ifdef CONFIG_SOFT_I2C2_GPIO_V12_JZ
DEF_GPIO_I2C(2);
#endif
#ifdef CONFIG_SOFT_I2C3_GPIO_V12_JZ
DEF_GPIO_I2C(3);
#endif
#ifdef CONFIG_SOFT_I2C4_GPIO_V12_JZ
DEF_GPIO_I2C(4);
#endif
#ifdef CONFIG_SOFT_I2C5_GPIO_V12_JZ
DEF_GPIO_I2C(5);
#endif
#ifdef CONFIG_SOFT_I2C6_GPIO_V12_JZ
DEF_GPIO_I2C(6);
#endif
#endif /*CONFIG_I2C_GPIO*/
