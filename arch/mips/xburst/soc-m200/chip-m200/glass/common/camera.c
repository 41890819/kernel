#include <mach/camera.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "board_base.h"

int temp = 1;
#ifdef CONFIG_VCM_DW9714A
static int dw9714a_power(int onoff)
{
	return 0;
}

static struct i2c_board_info dw9714a_board_info = {
	.type = "dw9714a",
	.addr = 0x0c,
};

#if defined(CONFIG_M200_BACK_CAMERA)
static struct ovisp_vcm_client back_vcm_client = {
	.board_info = &dw9714a_board_info,
	.flags = 0,
	.power = dw9714a_power,
};
#endif
#endif /* CONFIG_VCM_DW9714A */

#ifdef CONFIG_VCM_BU64243
static int bu64243_power(int onoff)
{
	return 0;
}

static struct i2c_board_info bu64243_board_info = {
	.type = "bu64243",
	.addr = 0x0c,
};

#if defined(CONFIG_M200_BACK_CAMERA)
static struct ovisp_vcm_client back_vcm_client = {
	.board_info = &bu64243_board_info,
	.flags = 0,
	.power = bu64243_power,
};
#endif
#endif /* CONFIG_VCM_BU64243 */

#ifndef CAMERA_PWDN_VALUE
#define CAMERA_PWDN_VALUE 0
#endif
#if defined(CONFIG_M200_BACK_CAMERA)
static int back_camera_power(int onoff)
{
	printk("function = %s, on = %d\n",__func__, onoff);
	if (temp) {
#ifdef CONFIG_VIDEO_OV6211
		gpio_request(CAMERA_OCCUPY_SDL, NULL);
		gpio_request(CAMERA_OCCUPY_SCK, NULL);
		gpio_direction_input(CAMERA_OCCUPY_SDL);
		gpio_direction_input(CAMERA_OCCUPY_SCK);
#endif
#ifdef CONFIG_BOARD_PARAMOUNT
		gpio_request(GPIO_PA(11), "dvdd_en");
		gpio_direction_output(GPIO_PA(11), 1);   /*PWM0 */
#endif
#ifdef CAMERA_PWDN_N
		gpio_request(CAMERA_PWDN_N, "CAMERA_PWDN_N");
#endif	    
#ifdef CAMERA_AF_EN
		gpio_request(CAMERA_AF_EN, "CAMERA_AF_EN");
#endif	    
#ifdef CAMERA_RST
		gpio_request(CAMERA_RST, "CAMERA_RST");
#endif
		temp = 0;
	}
	if (onoff) {
#ifdef CAMERA_RST
		gpio_direction_output(CAMERA_RST, 1);
#endif
#ifdef CAMERA_PWDN_N
		gpio_direction_output(CAMERA_PWDN_N, 1);
#endif	    
#ifdef CAMERA_AF_EN
#ifdef CONFIG_BOARD_DISATCH
		gpio_direction_output(CAMERA_AF_EN, 1);
#else
		gpio_direction_output(CAMERA_AF_EN, 0);
#endif
#endif	    
		mdelay(2);
	} else {
#ifdef CONFIG_VIDEO_OV6211
#ifdef CAMERA_RST
		gpio_direction_output(CAMERA_RST, 0);
#endif
#endif
#ifdef CAMERA_AF_EN
		gpio_direction_output(CAMERA_AF_EN, 1);   /*PWM0 */
#endif	    
#ifdef CAMERA_PWDN_N
/* #ifndef CONFIG_BOARD_VERSION_V23 */
/* 		gpio_direction_output(CAMERA_PWDN_N, 0); */
/* #endif */
		if (CAMERA_PWDN_VALUE == 0)
			gpio_direction_output(CAMERA_PWDN_N, CAMERA_PWDN_VALUE);
#endif
	}
	return 0;
}

static int back_camera_reset(void)
{
	/* 	printk("&&& function = %s, line = %d\n",__func__, __LINE__); */
	/* 	/\*reset*\/ */
	/* #ifdef CAMERA_RST */
	/* 	gpio_direction_output(CAMERA_RST, 1);   /\*PWM0 *\/ */
	/* 	mdelay(2); */
	/* 	gpio_direction_output(CAMERA_RST, 0);   /\*PWM0 *\/ */
	/* 	mdelay(2); */
	/* 	gpio_direction_output(CAMERA_RST, 1);   /\*PWM0 *\/ */
	/* 	mdelay(10); */
	/* #endif	     */
	return 0;
}

static struct i2c_board_info back_camera_board_info = {
	.type = "m200-back-camera",
	.addr = CAMERA_I2C_ADDR,
};
#endif /* CONFIG_M200_BACK_CAMERA */

#if defined(CONFIG_M200_FRONT_CAMERA)
static int front_camera_power(int onoff)
{
	printk("function = %s, on = %d\n",__func__, onoff);
	if(temp) {
#ifdef CONFIG_BOARD_PARAMOUNT
		gpio_request(GPIO_PA(11), "dvdd_en");
		gpio_direction_output(GPIO_PA(11), 1);   /*PWM0 */
#endif
#ifdef CAMERA_PWDN_N
		gpio_request(CAMERA_PWDN_N, "CAMERA_PWDN_N");
#endif	    
#ifdef CAMERA_AF_EN
		gpio_request(CAMERA_AF_EN, "CAMERA_AF_EN");
#endif	    
#ifdef CAMERA_RST
		gpio_request(CAMERA_RST, "CAMERA_RST");
#endif	    
		temp = 0;
	}
	if (onoff) {
#ifdef CAMERA_RST
		gpio_direction_output(CAMERA_RST, 1);
#endif
#ifdef CAMERA_PWDN_N
		gpio_direction_output(CAMERA_PWDN_N, 1);
#endif
#ifdef CAMERA_AF_EN
		gpio_direction_output(CAMERA_AF_EN, 1);
#endif
		mdelay(2);
	} else {
#ifdef CAMERA_AF_EN
		gpio_direction_output(CAMERA_AF_EN, 0);   /*PWM0 */
#endif	    
#ifdef CAMERA_PWDN_N
		gpio_direction_output(CAMERA_PWDN_N, 0);
#endif
	}
	return 0;
}

static int front_camera_reset(void)
{
	printk("&&& function = %s, line = %d\n",__func__, __LINE__);
	/*reset*/
#ifdef CAMERA_RST
	gpio_direction_output(CAMERA_RST, 1);   /*PWM0 */
	mdelay(2);
	gpio_direction_output(CAMERA_RST, 0);   /*PWM0 */
	mdelay(2);
	gpio_direction_output(CAMERA_RST, 1);   /*PWM0 */
	mdelay(10);
#endif	    
	return 0;
}

static struct i2c_board_info front_camera_board_info = {
	.type = "m200_front_camera",
	.addr = FRONT_CAMERA_I2C_ADDR,
};
#endif /* CONFIG_M200_FRONT_CAMERA */

static struct ovisp_camera_client ovisp_camera_clients[] = {

#ifdef CONFIG_M200_BACK_CAMERA
	{
		.board_info = &back_camera_board_info,
#ifdef CONFIG_VCM_DW9714A
		.vcm_client = &back_vcm_client,
		.flags = CAMERA_CLIENT_IF_MIPI
		| CAMERA_CLIENT_VCM
		| CAMERA_CLIENT_POWER_SUPPLY_ALWAYS,
#else
		.flags = CAMERA_CLIENT_IF_MIPI
		| CAMERA_CLIENT_POWER_SUPPLY_ALWAYS,
#endif
		.mclk_rate = 26000000,
		.power = back_camera_power,
		.reset = back_camera_reset,
	},
#endif /* CONFIG_M200_BACK_CAMERA */

#ifdef CONFIG_M200_FRONT_CAMERA
	{
		.board_info = &front_camera_board_info,

		.flags = CAMERA_CLIENT_IF_MIPI
		| CAMERA_CLIENT_CLK_EXT
		| CAMERA_CLIENT_ISP_BYPASS,

		.mclk_rate = 26000000,
		.power = front_camera_power,
		.reset = front_camera_reset,
	},
#endif /* CONFIG_M200_FRONT_CAMERA */
};

struct ovisp_camera_platform_data ovisp_camera_info = {
#ifdef CONFIG_OVISP_I2C
	.i2c_adapter_id = 8, /* larger than host i2c nums */
	.flags = CAMERA_USE_ISP_I2C | CAMERA_USE_HIGH_BYTE
	| CAMERA_I2C_PIO_MODE | CAMERA_I2C_STANDARD_SPEED,
#else
	.i2c_adapter_id = 3, /* use cpu's i2c adapter */
	.flags = CAMERA_USE_HIGH_BYTE
	| CAMERA_I2C_PIO_MODE | CAMERA_I2C_STANDARD_SPEED,
#endif
	.client = ovisp_camera_clients,
	.client_num = ARRAY_SIZE(ovisp_camera_clients),
};
