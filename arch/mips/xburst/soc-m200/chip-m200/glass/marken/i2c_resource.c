#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <board_base.h>

#if (defined(CONFIG_SOFT_I2C0_GPIO_V12_JZ) || defined(CONFIG_I2C0_V12_JZ))
struct i2c_board_info jz_i2c0_devs[] __initdata = {
};
#endif

#if (defined(CONFIG_SOFT_I2C1_GPIO_V12_JZ) || defined(CONFIG_I2C1_V12_JZ))
struct i2c_board_info jz_i2c1_devs[] __initdata = {
};
#endif  /*I2C1*/

#if (defined(CONFIG_SOFT_I2C2_GPIO_V12_JZ) || defined(CONFIG_I2C2_V12_JZ))
struct i2c_board_info jz_i2c2_devs[] __initdata = {
#if defined(CONFIG_TOUCHPANEL_IT7236) || defined(CONFIG_TOUCHPANEL_IT7236_KEY)
	{
		I2C_BOARD_INFO("ite7236_tp_i2c", 0x46),
		.irq = GPIO_7236_INT,
	},
#endif
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
};
#endif  /*I2C5*/

#if defined(CONFIG_SOFT_I2C6_GPIO_V12_JZ)
struct i2c_board_info jz_i2c6_devs[] __initdata = {
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
