#ifndef __BOARD_H__
#define __BOARD_H__

#include <gpio.h>
#include <soc/gpio.h>
#include "pmu.h"

/* ****************************GPIO SLEEP START******************************* */
#define GPIO_REGULATOR_SLP	GPIO_PE(22)
#define GPIO_OUTPUT_TYPE	GPIO_OUTPUT1
/* ****************************GPIO SLEEP END******************************** */

/* ****************************GPIO LCD START******************************** */
#ifdef CONFIG_BACKLIGHT_PWM
#define BACKLIGHT_PWM_ID	1
#endif

#ifdef CONFIG_BACKLIGHT_PWM_EN
#define BACKLIGHT_PWM_EN GPIO_PD(27)
#endif

#ifdef CONFIG_BACKLIGHT_DIGITAL_PULSE
#define GPIO_GIGITAL_PULSE      GPIO_PE(1)
#endif
/* ****************************GPIO LCD END********************************** */

/* ****************************GPIO I2C START******************************** */
#ifdef CONFIG_SOFT_I2C0_GPIO_V12_JZ
#define GPIO_I2C0_SDA  GPIO_PD(30)
#define GPIO_I2C0_SCK  GPIO_PD(31)
#endif

#ifdef CONFIG_SOFT_I2C1_GPIO_V12_JZ
#define GPIO_I2C1_SDA  GPIO_PE(30)
#define GPIO_I2C1_SCK  GPIO_PE(31)
#endif

#ifdef CONFIG_SOFT_I2C2_GPIO_V12_JZ
#define GPIO_I2C2_SDA  GPIO_PE(0)
#define GPIO_I2C2_SCK  GPIO_PE(3)
#endif

#ifdef CONFIG_SOFT_I2C3_GPIO_V12_JZ
#define GPIO_I2C3_SDA  GPIO_PB(7)
#define GPIO_I2C3_SCK  GPIO_PB(8)
#endif

#ifdef CONFIG_SOFT_I2C4_GPIO_V12_JZ
#define GPIO_I2C4_SDA  GPIO_PA(5)
#define GPIO_I2C4_SCK  GPIO_PA(8)
#endif

#ifdef CONFIG_SOFT_I2C5_GPIO_V12_JZ
#define GPIO_I2C5_SDA  -1
#define GPIO_I2C5_SCK  -1
#endif

#ifdef CONFIG_SOFT_I2C6_GPIO_V12_JZ
#define GPIO_I2C6_SDA  -1
#define GPIO_I2C6_SCK  -1
#endif
/* ****************************GPIO I2C END********************************** */

/* ****************************GPIO KEY START******************************** */
#define GPIO_CAMERA_KEY         GPIO_PA(11)
#define ACTIVE_LOW_CAMERA     	1

#define GPIO_KEY_A              GPIO_PD(18)
#define ACTIVE_LOW_A     	0

#define GPIO_ENDCALL_KEY        GPIO_PA(30)
#define ACTIVE_LOW_ENDCALL      1
/* ****************************GPIO KEY END********************************** */

/* ****************************GPIO PMU START******************************** */
#ifdef CONFIG_REGULATOR_RICOH619
#define PMU_IRQ_N		GPIO_PE(23)
#endif

#ifdef CONFIG_REGULATOR_DA9024
#define GPIO_PMU_IRQ		GPIO_PA(3)
#endif
/* ****************************GPIO PMU END********************************** */

/* ****************************GPIO EFUSE START****************************** */
#define GPIO_EFUSE_VDDQ      GPIO_PA(12)
/* ****************************GPIO EFUSE END******************************** */

/* ****************************GPIO LI ION START***************************** */
#define GPIO_LI_ION_CHARGE   GPIO_PB(1)
#define GPIO_LI_ION_AC       GPIO_PA(13)
#define GPIO_ACTIVE_LOW      1
/* ****************************GPIO LI ION END******************************* */

/* ****************************GPIO USB START******************************** */
#define GPIO_USB_ID			GPIO_PA(13)
#define GPIO_USB_ID_LEVEL		LOW_ENABLE
#define GPIO_USB_DETE			GPIO_PA(14)
#define GPIO_USB_HP_DETE	        GPIO_PA(14)
#define GPIO_USB_DETE_LEVEL		0
#define GPIO_USB_DRVVBUS		GPIO_PE(10)
#define GPIO_USB_DRVVBUS_LEVEL		HIGH_ENABLE
/* ****************************GPIO USB END********************************** */

/* ****************************GPIO CAMERA START***************************** */
#ifdef CONFIG_VIDEO_OV8858
#define CAMERA_AF_EN		GPIO_PD(28)
#define CAMERA_I2C_ADDR         0x36
#elif defined (CONFIG_VIDEO_OV8865)
#define CAMERA_AF_EN		GPIO_PD(28)
#define CAMERA_I2C_ADDR         0x36
#elif defined (CONFIG_VIDEO_OV5648)
/* #define CAMERA_AF_EN		GPIO_PD(28) */
#define CAMERA_I2C_ADDR         0x36
#define CAMERA_RST		GPIO_PA(10)
#elif defined (CONFIG_VIDEO_OV6211)
#define CAMERA_I2C_ADDR         0x60
#define CAMERA_RST              GPIO_PE(20)
#define CAMERA_OCCUPY_SDL       GPIO_PE(30)
#define CAMERA_OCCUPY_SCK       GPIO_PE(31)
#endif

/* #define CAMERA_PWDN_N           GPIO_PA(13) /\* pin conflict with USB_ID *\/ */
#define CAMERA_MCLK		GPIO_PE(2) /* no use */
#ifdef CONFIG_DVP_OV9712
#define OV9712_POWER	 	GPIO_PC(2) //the power of camera board
#define OV9712_RST		GPIO_PA(11)
#define OV9712_PWDN_EN		GPIO_PD(28)
#endif
/* ****************************GPIO CAMERA END******************************* */

/* ****************************GPIO AUDIO START****************************** */
#define GPIO_HP_MUTE		-1	/*hp mute gpio*/
#define GPIO_HP_MUTE_LEVEL	-1	/*vaild level*/

#define GPIO_SPEAKER_EN	        -1      /*speaker enable gpio*/
#define GPIO_SPEAKER_EN_LEVEL	-1
#define SPK_REGULATOR_EN       "amp_vdd"

#define GPIO_HANDSET_EN		-1	/*handset enable gpio*/
#define GPIO_HANDSET_EN_LEVEL   -1

#define	GPIO_HP_DETECT	        -1  /*hp detect gpio*/
#define GPIO_HP_INSERT_LEVEL    -1
#define GPIO_MIC_SELECT		-1	/*mic select gpio*/
#define GPIO_BUILDIN_MIC_LEVEL	-1	/*builin mic select level*/
#define GPIO_MIC_DETECT		-1
#define GPIO_MIC_INSERT_LEVEL   -1
#define GPIO_MIC_DETECT_EN	-1  /*mic detect enable gpio*/
#define GPIO_MIC_DETECT_EN_LEVEL -1 /*mic detect enable gpio*/

// Select USB or AUDIO_OUTPUT
#define GPIO_AUDIO_USB_SEL  	GPIO_PB(0)
#define GPIO_AUDIO_USB_SEL_LEVEL       	 0

#define HP_SENSE_ACTIVE_LEVEL	1
#define HOOK_ACTIVE_LEVEL	-1
/* ****************************GPIO AUDIO END******************************** */

/* ****************************GPIO WIFI START******************************* */
#define WL_WAKE_HOST	-1
#define WL_REG_EN	-1
#if 0
#define GPIO_WLAN_REG_ON	GPIO_PG(7)
#define GPIO_WLAN_INT	        GPIO_PG(8)
#define GPIO_WLAN_WAKE	        GPIO_PB(28)
#define GPIO_WIFI_RST_N         GPIO_PB(20)
#endif

#define WLAN_PWR_EN	(-1)
/* ****************************GPIO WIFI END********************************* */

/* ****************************GPIO BLUETOOTH START************************** */
/* BT gpio */
#define HOST_WAKE_BT	GPIO_PA(1)
#define BT_WAKE_HOST	GPIO_PA(0)
#define BT_REG_EN	GPIO_PA(2)
#define BT_UART_RTS	GPIO_PF(2)
#define BLUETOOTH_UPORT_NAME  "ttyS0"
#define BLUETOOTH_UART_GPIO_PORT    GPIO_PORT_F   
#define BLUETOOTH_UART_GPIO_FUNC    GPIO_FUNC_2  
#define BLUETOOTH_UART_FUNC_SHIFT   0x4

#if defined(CONFIG_BCM4330_RFKILL)
#define GPIO_BT_REG_ON      GPIO_PB(30)
#define GPIO_BT_WAKE        GPIO_PB(20)
#define GPIO_BT_INT    	    GPIO_PB(31)
//#define GPIO_BT_RST_N       GPIO_PB(28)
#define GPIO_BT_UART_RTS    GPIO_PF(2)
#define GPIO_PB_FLGREG      (0x10010158)
#define GPIO_BT_INT_BIT	    (1 << (GPIO_BT_INT % 32))
#endif
/* ****************************GPIO BLUETOOTH END**************************** */

#endif /* __BOARD_H__ */
