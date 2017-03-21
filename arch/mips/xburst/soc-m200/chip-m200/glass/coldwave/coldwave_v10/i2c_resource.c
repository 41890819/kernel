#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <board_base.h>

#if (defined(CONFIG_SOFT_I2C0_GPIO_V12_JZ) || defined(CONFIG_I2C0_V12_JZ))
struct i2c_board_info jz_i2c0_devs[] __initdata = {
#ifdef CONFIG_INV_MPU_IIO
	{
		I2C_BOARD_INFO("mpu6500", 0x68),
		.irq = (IRQ_GPIO_BASE + GPIO_GSENSOR_INT),
		.platform_data = &mpu9250_platform_data,
	},
#endif /*CONFIG_INV_MPU_IIO*/
#if defined(CONFIG_BCM2079X_NFC)
	{
		I2C_BOARD_INFO("bcm2079x-i2c", 0x77),
		.platform_data = &bcm2079x_pdata,
	},
#endif /*CONFIG_BCM2079X_NFC*/
};
#endif

#if (defined(CONFIG_SOFT_I2C1_GPIO_V12_JZ) || defined(CONFIG_I2C1_V12_JZ))
struct i2c_board_info jz_i2c1_devs[] __initdata = {
};
#endif  /*I2C1*/

#if (defined(CONFIG_SOFT_I2C2_GPIO_V12_JZ) || defined(CONFIG_I2C2_V12_JZ))
struct i2c_board_info jz_i2c2_devs[] __initdata = {
};
#endif  /*I2C2*/

#if (defined(CONFIG_SOFT_I2C3_GPIO_V12_JZ) || defined(CONFIG_I2C3_V12_JZ))
struct i2c_board_info jz_i2c3_devs[] __initdata = {
};
#endif  /*I2C3*/

#if defined(CONFIG_SOFT_I2C4_GPIO_V12_JZ)
struct i2c_board_info jz_i2c4_devs[] __initdata = {
};
#endif
 
#if defined(CONFIG_SOFT_I2C5_GPIO_V12_JZ)
struct i2c_board_info jz_i2c5_devs[] __initdata = {
#ifdef CONFIG_TOUCHPANEL_ZET6231
	{
		I2C_BOARD_INFO("zet6231", 0x76),
		.irq = GPIO_TP_INT,
	},
#endif
#if (defined(CONFIG_TOUCHPANEL_CP2615) || defined(CONFIG_TOUCHPANEL_CP2615_PDMA))
	{
		I2C_BOARD_INFO("cp2615", 0x2c),
		.irq = GPIO_2615_INT,
	},
#endif
};
#endif  /*I2C5*/

#if defined(CONFIG_SOFT_I2C6_GPIO_V12_JZ)
struct i2c_board_info jz_i2c6_devs[] __initdata = {

#ifdef CONFIG_LCD_HIMAX7097
	{
		I2C_BOARD_INFO("himax7097", 0x48),
	},
#endif

#ifdef CONFIG_SENSOR_ISL29035
	{
		I2C_BOARD_INFO("isl29035",0x44),
		.irq = GPIO_ISL29035_INT,          
		.platform_data = NULL,
	},
#endif
};
#endif

#if	(defined(CONFIG_SOFT_I2C0_GPIO_V12_JZ) || defined(CONFIG_I2C0_V12_JZ))
int jz_i2c0_devs_size = ARRAY_SIZE(jz_i2c0_devs);
#endif

#if	(defined(CONFIG_SOFT_I2C1_GPIO_V12_JZ) || defined(CONFIG_I2C1_V12_JZ))
int jz_i2c1_devs_size = ARRAY_SIZE(jz_i2c1_devs);
#endif

#if	(defined(CONFIG_SOFT_I2C2_GPIO_V12_JZ) || defined(CONFIG_I2C2_V12_JZ))
int jz_i2c2_devs_size = ARRAY_SIZE(jz_i2c2_devs);
#endif

#if	(defined(CONFIG_SOFT_I2C3_GPIO_V12_JZ) || defined(CONFIG_I2C3_V12_JZ))
int jz_i2c3_devs_size = ARRAY_SIZE(jz_i2c3_devs);
#endif

#if     defined(CONFIG_SOFT_I2C4_GPIO_V12_JZ)
int jz_i2c4_devs_size = ARRAY_SIZE(jz_i2c4_devs);
#endif 

#if	defined(CONFIG_SOFT_I2C5_GPIO_V12_JZ)
int jz_i2c5_devs_size = ARRAY_SIZE(jz_i2c5_devs);
#endif

#if     defined(CONFIG_SOFT_I2C6_GPIO_V12_JZ)
int jz_i2c6_devs_size = ARRAY_SIZE(jz_i2c6_devs);
#endif