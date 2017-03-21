/*
 * Linux/sound/oss/xb47XX/xb4780/jz4780_codec.c
 *
 * CODEC CODEC driver for Ingenic Jz4780 MIPS processor
 *
 * 2010-11-xx   jbbi <jbbi@ingenic.cn>
 *
 * Copyright (c) Ingenic Semiconductor Co., Ltd.
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/sound.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <linux/proc_fs.h>
#include <linux/soundcard.h>
#include <linux/dma-mapping.h>
#include <linux/earlysuspend.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/gpio.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>

#include "../xb47xx_i2s.h"
#include "jz4780_codec.h"
#include "jz4780_route_conf.h"

/*###############################################*/
#define CODEC_DUMP_IOC_CMD			0
#define CODEC_DUMP_ROUTE_REGS		0
#define CODEC_DUMP_ROUTE_PART_REGS	0
#define CODEC_DUMP_GAIN_PART_REGS	0
#define CODEC_DUMP_ROUTE_NAME		1
#define CODEC_DUMP_GPIO_STATE		0
/*##############################################*/

/***************************************************************************************\                                                                                *
 *global variable and structure interface                                              *
\***************************************************************************************/

static struct snd_board_route *cur_route = NULL;
static struct snd_board_route *keep_old_route = NULL;
static struct snd_codec_data *codec_platform_data = NULL;
static int default_replay_volume_base = 0;
static int default_record_volume_base = 0;
static int default_record_digital_volume_base = 0;
static int default_replay_digital_volume_base = 0;
static int default_bypass_l_volume_base = 0;					/*call uplink or other use*/
static int default_bypass_r_volume_base = 0;					/*call downlink or other use*/
static int user_replay_volume = 100;
static int user_record_volume = 100;

unsigned int cur_out_device = -1;


#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend early_suspend;
#endif

extern int i2s_register_codec(char *name, void *codec_ctl,unsigned long codec_clk,enum codec_mode mode);

/*==============================================================*/
/**
 * codec_sleep
 *
 *if use in suspend and resume, should use delay
 */

#define MIXER_RECORD  0x1
#define	MIXER_REPLAY  0x2

static int g_short_circut_state = 0;
static int g_codec_sleep_mode = 1;
static int g_mixer_is_used = 0;

void codec_sleep(int ms)
{
	if(!g_codec_sleep_mode)
		mdelay(ms);
	else
		msleep(ms);
}

static inline void codec_sleep_wait_bitset(int reg, unsigned bit,
					   int line, int mode)
{
	int i = 0;

        for (i=0; i<3000; i++) {
                codec_sleep(1);
                if (read_inter_codec_reg(reg) & (1 << bit))
                        return;
        }
	pr_err("CODEC:line%d Timed out waiting for event 0x%2x\n", line, mode);
}

static inline void codec_wait_event_complete(int event , int mode)
{
	if (event == IFR_ADC_MUTE_EVENT) {
		if (__codec_test_jadc_mute_state() != mode) {
			codec_sleep_wait_bitset(CODEC_REG_IFR, event , __LINE__,mode);
			__codec_set_irq_flag(1 << event);
			if (__codec_test_jadc_mute_state() != mode) {
				codec_sleep_wait_bitset(CODEC_REG_IFR, event ,  __LINE__,mode);
			}
		}
	} else if (event == IFR_DAC_MUTE_EVENT) {
		if (__codec_test_dac_mute_state() != mode) {
			codec_sleep_wait_bitset(CODEC_REG_IFR, event , __LINE__,mode);
			__codec_set_irq_flag(1 << event);
			if (__codec_test_dac_mute_state() != mode) {
				codec_sleep_wait_bitset(CODEC_REG_IFR, event ,  __LINE__,mode);
			}
		}
	} else if (event == IFR_DAC_MODE_EVENT) {
		if (__codec_test_dac_udp() != mode) {
			codec_sleep_wait_bitset(CODEC_REG_IFR, event , __LINE__,mode);
			__codec_set_irq_flag(1 << event);
			if (__codec_test_dac_udp() != mode) {
				codec_sleep_wait_bitset(CODEC_REG_IFR, event ,  __LINE__,mode);
			}
		}
	}
}

/***************************************************************************************\
 *debug part                                                                           *
\***************************************************************************************/

#if CODEC_DUMP_IOC_CMD
static void codec_print_ioc_cmd(int cmd)
{
	int i;

	int cmd_arr[] = {
		CODEC_INIT,					CODEC_TURN_OFF,
		CODEC_SHUTDOWN,				CODEC_RESET,
		CODEC_SUSPEND,				CODEC_RESUME,
		CODEC_ANTI_POP,				CODEC_SET_DEFROUTE,
		CODEC_SET_DEVICE,			CODEC_SET_RECORD_RATE,
		CODEC_SET_RECORD_DATA_WIDTH,	CODEC_SET_MIC_VOLUME,
		CODEC_SET_RECORD_CHANNEL,		CODEC_SET_REPLAY_RATE,
		CODEC_SET_REPLAY_DATA_WIDTH,	CODEC_SET_REPLAY_VOLUME,
		CODEC_SET_REPLAY_CHANNEL,	CODEC_DAC_MUTE,
		CODEC_DEBUG_ROUTINE,		CODEC_SET_STANDBY,
		CODEC_GET_RECORD_FMT_CAP,		CODEC_GET_RECORD_FMT,
		CODEC_GET_REPLAY_FMT_CAP,	CODEC_GET_REPLAY_FMT,
		CODEC_IRQ_HANDLE,			CODEC_DUMP_REG,
		CODEC_DUMP_GPIO
	};

	char *cmd_str[] = {
		"CODEC_INIT", 			"CODEC_TURN_OFF",
		"CODEC_SHUTDOWN", 		"CODEC_RESET",
		"CODEC_SUSPEND",		"CODEC_RESUME",
		"CODEC_ANTI_POP",		"CODEC_SET_DEFROUTE",
		"CODEC_SET_DEVICE",		"CODEC_SET_RECORD_RATE",
		"CODEC_SET_RECORD_DATA_WIDTH", 	"CODEC_SET_MIC_VOLUME",
		"CODEC_SET_RECORD_CHANNEL", 	"CODEC_SET_REPLAY_RATE",
		"CODEC_SET_REPLAY_DATA_WIDTH", 	"CODEC_SET_REPLAY_VOLUME",
		"CODEC_SET_REPLAY_CHANNEL", 	"CODEC_DAC_MUTE",
		"CODEC_DEBUG_ROUTINE",		"CODEC_SET_STANDBY",
		"CODEC_GET_RECORD_FMT_CAP",		"CODEC_GET_RECORD_FMT",
		"CODEC_GET_REPLAY_FMT_CAP",	"CODEC_GET_REPLAY_FMT",
		"CODEC_IRQ_HANDLE",		"CODEC_DUMP_REG",
		"CODEC_DUMP_GPIO"
	};

	for ( i = 0; i < sizeof(cmd_arr) / sizeof(int); i++) {
		if (cmd_arr[i] == cmd) {
			printk("CODEC IOC: Command name : %s\n", cmd_str[i]);
			return;
		}
	}

	if (i == sizeof(cmd_arr) / sizeof(int)) {
		printk("CODEC IOC: command is not under control\n");
	}
}
#endif //CODEC_DUMP_IOC_CMD

#if CODEC_DUMP_ROUTE_NAME
static void codec_print_route_name(int route)
{
	int i;

	int route_arr[] = {
		SND_ROUTE_NONE,
		SND_ROUTE_ALL_CLEAR,
		SND_ROUTE_REPLAY_CLEAR,
		SND_ROUTE_RECORD_CLEAR,
		SND_ROUTE_REPLAY_LINEIN2_BYPASS_TO_LINEOUT,
		SND_ROUTE_REPLAY_LINEIN2_BYPASS_TO_HPRL,
		SND_ROUTE_CALL_MIC_BYPASS_TO_HPRL,
		SND_ROUTE_CALL_RECORD,
		SND_ROUTE_REPLAY_DACRL_TO_LO,
		SND_ROUTE_REPLAY_DACRL_TO_HPRL,
		SND_ROUTE_REPLAY_DACRL_TO_ALL,
		SND_ROUTE_RECORD_MIC1_AN1,
		SND_ROUTE_RECORD_MIC1_SIN_AN2,
		SND_ROUTE_RECORD_MIC2_SIN_AN3,
		SND_ROUTE_RECORD_LINEIN1_DIFF_AN2,
		SND_ROUTE_RECORD_LINEIN2_SIN_AN3,
		SND_ROUTE_LINE1IN_BYPASS_TO_HP,
		SND_ROUTE_LOOP_MIC1_AN1_LOOP_TO_HP,
		SND_ROUTE_RECORD_LINEIN1_AN2_SIN_TO_ADCL_AND_LINEIN2_AN3_SIN_TO_ADCR,
	};

	char *route_str[] = {
		"SND_ROUTE_NONE",
		"SND_ROUTE_ALL_CLEAR",
		"SND_ROUTE_REPLAY_CLEAR",
		"SND_ROUTE_RECORD_CLEAR",
		"SND_ROUTE_REPLAY_LINEIN2_BYPASS_TO_LINEOUT",
		"SND_ROUTE_REPLAY_LINEIN2_BYPASS_TO_HPRL",
		"SND_ROUTE_CALL_MIC_BYPASS_TO_HPRL",
		"SND_ROUTE_CALL_RECORD",
		"SND_ROUTE_REPLAY_DACRL_TO_LO",
		"SND_ROUTE_REPLAY_DACRL_TO_HPRL",
		"SND_ROUTE_REPLAY_DACRL_TO_ALL",
		"SND_ROUTE_RECORD_MIC1_AN1",
		"SND_ROUTE_RECORD_MIC1_SIN_AN2",
		"SND_ROUTE_RECORD_MIC2_SIN_AN3",
		"SND_ROUTE_RECORD_LINEIN1_DIFF_AN2",
		"SND_ROUTE_RECORD_LINEIN2_SIN_AN3",
		"SND_ROUTE_LINE1IN_BYPASS_TO_HP",
		"SND_ROUTE_LOOP_MIC1_AN1_LOOP_TO_HP",
		"SND_ROUTE_RECORD_LINEIN1_AN2_SIN_TO_ADCL_AND_LINEIN2_AN3_SIN_TO_ADCR"
	};

	for ( i = 0; i < sizeof(route_arr) / sizeof(unsigned int); i++) {
		if (route_arr[i] == route) {
			printk("\nCODEC SET ROUTE: Route name : %s,%d\n", route_str[i],i);
			return;
		}
	}

	if (i == sizeof(route_arr) / sizeof(unsigned int)) {
		printk("\nCODEC SET ROUTE: Route %d is not configed yet! \n",route);
	}
}
#endif //CODEC_DUMP_ROUTE_NAME
static void dump_gpio_state(void)
{
	int val = -1;

	if(codec_platform_data &&
	   (codec_platform_data->gpio_hp_mute.gpio != -1)) {
		val = __gpio_get_value(codec_platform_data->gpio_hp_mute.gpio);
		printk("gpio hp mute %d statue is %d.\n",codec_platform_data->gpio_hp_mute.gpio, val);
	}

	if(codec_platform_data &&
	   (codec_platform_data->gpio_spk_en.gpio != -1)) {
		val = __gpio_get_value(codec_platform_data->gpio_spk_en.gpio);
		printk("gpio speaker enable %d statue is %d.\n",codec_platform_data->gpio_spk_en.gpio, val);
	}

	if(codec_platform_data &&
	   (codec_platform_data->gpio_handset_en.gpio != -1)) {
		val = __gpio_get_value(codec_platform_data->gpio_handset_en.gpio);
		printk("gpio handset enable %d statue is %d.\n",codec_platform_data->gpio_handset_en.gpio, val);
	}

	if(codec_platform_data &&
	   (codec_platform_data->gpio_hp_detect.gpio != -1)) {
		val = __gpio_get_value(codec_platform_data->gpio_hp_detect.gpio);
		printk("gpio hp detect %d statue is %d.\n",codec_platform_data->gpio_hp_detect.gpio, val);
	}

	if(codec_platform_data &&
	   (codec_platform_data->gpio_mic_detect.gpio != -1)) {
		val = __gpio_get_value(codec_platform_data->gpio_mic_detect.gpio);
		printk("gpio mic detect %d statue is %d.\n",codec_platform_data->gpio_mic_detect.gpio, val);
	}

	if(codec_platform_data &&
	   (codec_platform_data->gpio_mic_detect_en.gpio != -1)) {
		val = __gpio_get_value(codec_platform_data->gpio_mic_detect_en.gpio);
		printk("gpio mic detect enable %d statue is %d.\n",codec_platform_data->gpio_mic_detect_en.gpio, val);
	}

	if(codec_platform_data &&
	   (codec_platform_data->gpio_buildin_mic_select.gpio != -1)) {
		val = __gpio_get_value(codec_platform_data->gpio_buildin_mic_select.gpio);
		printk("gpio_mic_switch %d statue is %d.\n",codec_platform_data->gpio_buildin_mic_select.gpio, val);
	}


}

static void dump_codec_regs(void)
{
	unsigned int i;
	unsigned char data;
	printk("codec register list:\n");
	for (i = 0; i <= 0x3B; i++) {
		data = read_inter_codec_reg(i);
		printk("address = 0x%02x, data = 0x%02x\n", i, data);
	}
	printk("codec mixer register list:\n");
	for (i = CR_MIX0 ; i <= CR_MIX3; i++) {
		data = __codec_mix_read_reg(i);
		printk("mix%d val = 0x%02x\n",i,data);
	}
	if (!g_mixer_is_used)
		__codec_mix_disable();
}

#if CODEC_DUMP_ROUTE_PART_REGS
static void dump_codec_route_regs(void)
{
	unsigned int i;
	unsigned char data;
	for (i = 0xb; i < 0x1d; i++) {
		data = read_inter_codec_reg(i);
		printk("address = 0x%02x, data = 0x%02x\n", i, data);
	}
}
#endif

#if CODEC_DUMP_GAIN_PART_REGS
static void dump_codec_gain_regs(void)
{
	unsigned int i;
	unsigned char data;
	for (i = 0x28; i < 0x37; i++) {
		data = read_inter_codec_reg(i);
		printk("address = 0x%02x, data = 0x%02x\n", i, data);
	}
}
#endif
/*=========================================================*/

#if CODEC_DUMP_ROUTE_NAME
#define DUMP_ROUTE_NAME(route) codec_print_route_name(route)
#else //CODEC_DUMP_ROUTE_NAME
#define DUMP_ROUTE_NAME(route)
#endif //CODEC_DUMP_ROUTE_NAME

/*-------------------*/
#if CODEC_DUMP_IOC_CMD
#define DUMP_IOC_CMD(value)						\
	do {								\
		printk("[codec IOCTL]++++++++++++++++++++++++++++\n");	\
		printk("%s  cmd = %d, arg = %lu-----%s\n", __func__, cmd, arg, value); \
		codec_print_ioc_cmd(cmd);				\
		printk("[codec IOCTL]----------------------------\n");	\
	} while (0)
#else //CODEC_DUMP_IOC_CMD
#define DUMP_IOC_CMD(value)
#endif //CODEC_DUMP_IOC_CMD

#if CODEC_DUMP_GPIO_STATE
#define DUMP_GPIO_STATE()			\
	do {					\
		dump_gpio_state();		\
	} while (0)
#else
#define DUMP_GPIO_STATE()
#endif

#if CODEC_DUMP_ROUTE_REGS
#define DUMP_ROUTE_REGS(value)						\
	do {								\
		printk("codec register dump,%s\tline:%d-----%s:\n",	\
		       __func__, __LINE__, value);			\
		dump_codec_regs();					\
	} while (0)
#else //CODEC_DUMP_ROUTE_REGS
#define DUMP_ROUTE_REGS(value)				\
	do {						\
		if (!strcmp("enter",value))		\
			ENTER_FUNC();			\
		else if (!strcmp("leave",value))	\
			LEAVE_FUNC();			\
	} while (0)
#endif //CODEC_DUMP_ROUTE_REGS

#if CODEC_DUMP_ROUTE_PART_REGS
#define DUMP_ROUTE_PART_REGS(value)					\
	do {								\
		if (mode != DISABLE) {					\
			printk("codec register dump,%s\tline:%d-----%s:\n", \
			       __func__, __LINE__, value);		\
			dump_codec_route_regs();			\
		}							\
	} while (0)
#else //CODEC_DUMP_ROUTE_PART_REGS
#define DUMP_ROUTE_PART_REGS(value)			\
	do {						\
		if (!strcmp("enter",value))		\
			ENTER_FUNC();			\
		else if (!strcmp("leave",value))	\
			LEAVE_FUNC();			\
	} while (0)
#endif //CODEC_DUMP_ROUTE_PART_REGS

#if CODEC_DUMP_GAIN_PART_REGS
#define DUMP_GAIN_PART_REGS(value)					\
	do {								\
		printk("codec register dump,%s\tline:%d-----%s:\n",	\
		       __func__, __LINE__, value);			\
		dump_codec_gain_regs();					\
	} while (0)
#else //CODEC_DUMP_GAIN_PART_REGS
#define DUMP_GAIN_PART_REGS(value)			\
	do {						\
		if (!strcmp("enter",value))		\
			ENTER_FUNC();			\
		else if (!strcmp("leave",value))	\
			LEAVE_FUNC();			\
	} while (0)
#endif //CODEC_DUMP_GAIN_PART_REGS

/***************************************************************************************\
 *route part and attibute                                                              *
\***************************************************************************************/

#ifdef CONFIG_HAS_EARLYSUSPEND
static void codec_early_suspend(struct early_suspend *handler)
{
	//  __codec_switch_sb_micbias1(POWER_OFF);
	//  __codec_switch_sb_micbias2(POWER_OFF);
}

static void codec_late_resume(struct early_suspend *handler)
{
        //__codec_switch_sb_micbias1(POWER_ON);
	// __codec_switch_sb_micbias2(POWER_ON);
}
#endif
/***************************************************************************************\
 *route part and attibute                                                              *
\***************************************************************************************/
/*=========================power on==========================*/
static void codec_prepare_ready(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");
	if(__codec_get_sb() == POWER_OFF &&
	   g_short_circut_state == 0)
	{
		__codec_switch_sb(POWER_ON);
		codec_sleep(250);
	}
	/*wait a typical time 200ms for adc (400ms for dac) to get into normal mode*/
	if(__codec_get_sb_sleep() == POWER_OFF)
	{
		__codec_switch_sb_sleep(POWER_ON);
		if(mode == CODEC_RMODE)
			codec_sleep(200);
		else
			codec_sleep(400);
	}
	if (mode & CODEC_WMODE) {
		if (__codec_get_dac_interface_state()) {
			__codec_select_dac_digital_interface(CODEC_I2S_INTERFACE);
			__codec_enable_dac_interface();
		}
	}
	if (mode & CODEC_RMODE) {
		if (__codec_get_adc_interface_state()) {
			__codec_select_adc_digital_interface(CODEC_I2S_INTERFACE);
			__codec_enable_adc_interface();
		}
	}

	DUMP_ROUTE_PART_REGS("leave");
}

/*=================route part functions======================*/
/* select mic1 mode */
static void codec_set_mic1(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");
	switch(mode){
	case MIC1_DIFF_WITH_MICBIAS:
		if(__codec_get_sb_micbias1() == POWER_OFF)
		{
			__codec_switch_sb_micbias1(POWER_ON);
		}
		__codec_enable_mic1_diff();
		break;

	case MIC1_DIFF_WITHOUT_MICBIAS:
		if(__codec_get_sb_micbias1() == POWER_ON)
		{
        		__codec_switch_sb_micbias1(POWER_OFF);
		}
		__codec_enable_mic1_diff();
		break;

	case MIC1_SING_WITH_MICBIAS:
		if(__codec_get_sb_micbias1() == POWER_OFF)
		{
			__codec_switch_sb_micbias1(POWER_ON);
		}
		__codec_disable_mic1_diff();
		break;

	case MIC1_SING_WITHOUT_MICBIAS:
		if(__codec_get_sb_micbias1() == POWER_ON)
		{
			__codec_switch_sb_micbias1(POWER_OFF);
		}
		__codec_disable_mic1_diff();
		break;

	case MIC1_DISABLE:
		/*if(__codec_get_sb_micbias1() == POWER_ON)
		  {
		  __codec_switch_sb_micbias1(POWER_OFF);
		  }*/
		break;

	default:
		printk("JZ_CODEC: line: %d, mic1 mode error!\n", __LINE__);
	}
	DUMP_ROUTE_PART_REGS("leave");
}

/* select mic2  mode */
static void codec_set_mic2(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");
	switch(mode){
	case MIC2_SING_WITH_MICBIAS:
		if(__codec_get_sb_micbias2() == POWER_OFF)
		{
			__codec_switch_sb_micbias2(POWER_ON);
		}
		__codec_disable_mic2_diff();
		break;

	case MIC2_SING_WITHOUT_MICBIAS:
		if(__codec_get_sb_micbias2() == POWER_ON)
		{
			__codec_switch_sb_micbias2(POWER_OFF);
		}
		__codec_disable_mic2_diff();
		break;

	case MIC2_DISABLE:
		/*if(__codec_get_sb_micbias2() == POWER_ON)
		  {
		  __codec_switch_sb_micbias2(POWER_OFF);
		  }*/
		break;

	default:
		printk("JZ_CODEC: line: %d, mic2 mode error!\n", __LINE__);
	}

	DUMP_ROUTE_PART_REGS("leave");
}

static void codec_set_line1(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");
	switch (mode) {
	case LINE1_DIFF:
		__codec_enable_linein1_diff();
		break;
	case LINE1_SING:
		__codec_disable_linein1_diff();
		break;
	default:
		printk("JZ_CODEC: line: %d, line1 mode error!\n", __LINE__);
	}
	DUMP_ROUTE_PART_REGS("leave");
}


static void codec_set_line2(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");
	switch (mode) {
	case LINE2_SING:
		__codec_disable_linein2_diff();
		break;
	default:
		printk("JZ_CODEC: line: %d, line1 mode error!\n", __LINE__);
	}
	DUMP_ROUTE_PART_REGS("leave");
}

static void codec_set_inputl(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");
	switch (mode) {
	case INPUTL_TO_ADC_ENABLE:
		if(__codec_get_sb_mic1() == POWER_OFF)
		{
			__codec_switch_sb_mic1(POWER_ON);
		}
		break;
	case INPUTL_TO_ADC_DISABLE:
		if(__codec_get_sb_mic1() == POWER_ON)
		{
			__codec_switch_sb_mic1(POWER_OFF);
		}
		break;
	default :
		printk("JZ_CODEC: line: %d, inputl mode error!\n", __LINE__);
	}
	DUMP_ROUTE_PART_REGS("leave");
}
static void codec_set_inputr(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");
	switch (mode) {
	case INPUTR_TO_ADC_ENABLE:
		if(__codec_get_sb_mic2() == POWER_OFF)
		{
			__codec_switch_sb_mic2(POWER_ON);
		}
		break;
	case INPUTR_TO_ADC_DISABLE:
		if(__codec_get_sb_mic2() == POWER_ON)
		{
			__codec_switch_sb_mic2(POWER_OFF);
		}
		break;
	default :
		printk("JZ_CODEC: line: %d, inputl mode error!\n", __LINE__);
	}
	DUMP_ROUTE_PART_REGS("leave");
}
/*select input port for mic1 or linein1*/
static void codec_set_inputl_mux(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");
	switch (mode & INPUT_MASK_INPUT_MUX) {
	case INPUTL_MUX_MIC1_TO_AN1:
		__codec_select_mic1_input(CODEC_MIC1_AN1);
		break;
	case INPUTL_MUX_MIC1_TO_AN2:
		__codec_select_mic1_input(CODEC_MIC1_AN2);
		break;
	}
	switch (mode & INPUT_MASK_BYPASS_MUX) {
	case INPUTL_MUX_LINEIN1_TO_AN1:
		__codec_select_linein1_input(CODEC_LINEIN1_AN1);
		break;
	case INPUTL_MUX_LINEIN1_TO_AN2:
		__codec_select_linein1_input(CODEC_LINEIN1_AN2);
		break;
	}
	DUMP_ROUTE_PART_REGS("leave");
}

/*select input port for mic2 or linein2*/
static void codec_set_inputr_mux(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");
	switch (mode & INPUT_MASK_INPUT_MUX) {
	case INPUTR_MUX_MIC2_TO_AN3:
		__codec_select_mic2_input(CODEC_MIC2_AN3);
		break;
	case INPUTR_MUX_MIC2_TO_AN4:
		__codec_select_mic2_input(CODEC_MIC2_AN4);
		break;
	}
	switch (mode & INPUT_MASK_BYPASS_MUX) {
	case INPUTR_MUX_LINEIN2_TO_AN3:
		__codec_select_linein2_input(CODEC_LINEIN2_AN3);
		break;
	case INPUTR_MUX_LINEIN2_TO_AN4:
		__codec_select_linein2_input(CODEC_LINEIN2_AN4);
		break;
	}
	DUMP_ROUTE_PART_REGS("leave");
}

/*left input bypass*/
static void codec_set_inputl_to_bypass(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");

	switch(mode) {

	case INPUTL_TO_BYPASS_ENABLE:
		if(__codec_get_sb_linein1_bypass() == POWER_OFF)
		{
			__codec_switch_sb_linein1_bypass(POWER_ON);
		}
		break;

	case INPUTL_TO_BYPASS_DISABLE:
		if(__codec_get_sb_linein1_bypass() == POWER_ON)
		{
			__codec_switch_sb_linein1_bypass(POWER_OFF);
		}
		break;

	default:
		printk("JZ_CODEC: line: %d, bypass error!\n", __LINE__);
	}

	DUMP_ROUTE_PART_REGS("leave");
}

/*right input bypass*/
static void codec_set_inputr_to_bypass(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");

	switch(mode){

	case INPUTR_TO_BYPASS_ENABLE:
		if(__codec_get_sb_linein2_bypass() == POWER_OFF)
		{
			__codec_switch_sb_linein2_bypass(POWER_ON);
		}
		break;

	case INPUTR_TO_BYPASS_DISABLE:
		if(__codec_get_sb_linein2_bypass() == POWER_ON)
		{
			__codec_switch_sb_linein2_bypass(POWER_OFF);
		}
		break;

	default:
		printk("JZ_CODEC: line: %d, bypass error!\n", __LINE__);
	}

	DUMP_ROUTE_PART_REGS("leave");
}

/*adc mux*/
static void codec_set_record_mux(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");

	switch(mode){

	case RECORD_MUX_INPUTL_TO_LR:
		/* if digital micphone is not select, */
		/* 4780 codec auto set ADC_LEFT_ONLY to 1 */
		__codec_set_dmic_mux(CODEC_DMIC_SEL_ADC);
		__codec_disable_dmic_clk();
		__codec_set_mic_mono();
		__codec_set_adc_mux(CODEC_INPUTL_TO_LR);
		break;

	case RECORD_MUX_INPUTR_TO_LR:
		/* if digital micphone is not select, */
		/* 4780 codec auto set ADC_LEFT_ONLY to 1 */
		__codec_set_dmic_mux(CODEC_DMIC_SEL_ADC);
		__codec_disable_dmic_clk();
		__codec_set_mic_mono();
		__codec_set_adc_mux(CODEC_INPUTR_TO_LR);
		break;

	case RECORD_MUX_INPUTL_TO_L_INPUTR_TO_R:
		__codec_set_mic_stereo();
		__codec_set_adc_mux(CODEC_INPUTLR_NORMAL);
		__codec_set_dmic_mux(CODEC_DMIC_SEL_ADC);
		__codec_disable_dmic_clk();
		break;

	case RECORD_MUX_INPUTR_TO_L_INPUTL_TO_R:
		__codec_set_mic_stereo();
		__codec_set_adc_mux(CODEC_INPUTLR_NORMAL);
		__codec_set_dmic_mux(CODEC_DMIC_SEL_ADC);
		__codec_disable_dmic_clk();
		break;

	case RECORD_MUX_DIGITAL_MIC:
		__codec_set_dmic_mux(CODEC_DMIC_SEL_DIGITAL_MIC);
		__codec_enable_dmic_clk();
		break;

	default:
		printk("JZ_CODEC: line: %d, record mux mode error!\n", __LINE__);
	}

	DUMP_ROUTE_PART_REGS("leave");
}

/*adc mode*/
static void codec_set_adc(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");

	__codec_set_dmic_mux(CODEC_DMIC_SEL_ADC);

	switch(mode){
	case ADC_STEREO:
		if(__codec_get_sb_adc() == POWER_OFF)
		{
			__codec_switch_sb_adc(POWER_ON);
		}
		__codec_set_adc_mode(CODEC_ADC_STEREO);
		break;

	case ADC_STEREO_WITH_LEFT_ONLY:
		if(__codec_get_sb_adc() == POWER_OFF)
		{
			__codec_switch_sb_adc(POWER_ON);
		}
		__codec_set_adc_mode(CODEC_ADC_LEFT_ONLY);
		break;

	case ADC_DMIC_ENABLE:
		__codec_set_dmic_mux(CODEC_DMIC_SEL_DIGITAL_MIC);
		mdelay(1);
	case ADC_DISABLE:
		if(__codec_get_sb_adc() == POWER_ON)
		{
			__codec_switch_sb_adc(POWER_OFF);
		}
		break;

	default:
		printk("JZ_CODEC: line: %d, adc mode error!\n", __LINE__);
	}

	DUMP_ROUTE_PART_REGS("leave");
}

static void codec_set_mixer(unsigned long mode)
{
	uint32_t mixer_inputr = CODEC_MIX_FUNC_NOINPUT;
	uint32_t mixer_inputl = CODEC_MIX_FUNC_NOINPUT;
	uint32_t mixer_dacr = CODEC_MIX_FUNC_NOINPUT;
	uint32_t mixer_dacl = CODEC_MIX_FUNC_NOINPUT;

	switch (mode & MIXADC_MIXER_L_MASK) {
	case MIXADC_MIXER_INPUTL_TO_L:
		mixer_inputl = CODEC_MIX_FUNC_NORMAL;
		break;
	case MIXADC_MIXER_INPUTR_TO_L:
		mixer_inputl = CODEC_MIX_FUNC_CROSS;
		break;
	case MIXADC_MIXER_INPUTLR_TO_L:
		mixer_inputl = CODEC_MIX_FUNC_MIXED;
		break;
	case MIXADC_MIXER_NOINPUT_TO_L:
		mixer_inputl = CODEC_MIX_FUNC_NOINPUT;
		break;
	}

	switch (mode & MIXADC_MIXER_R_MASK) {
	case MIXADC_MIXER_INPUTL_TO_R:
		mixer_inputr = CODEC_MIX_FUNC_CROSS;
		break;
	case MIXADC_MIXER_INPUTR_TO_R:
		mixer_inputr = CODEC_MIX_FUNC_NORMAL;
		break;
	case MIXADC_MIXER_INPUTLR_TO_R:
		mixer_inputr = CODEC_MIX_FUNC_MIXED;
		break;
	case MIXADC_MIXER_NOINPUT_TO_R:
		mixer_inputr = CODEC_MIX_FUNC_NOINPUT;
		break;
	}
	__codec_set_mix(CR_MIX3,mixer_inputl,mixer_inputr);

	/* Note :there is a ic buge so we must exchange the left and right channel,
	 * do not modify it, the buge is follow:
	 * dir and dil is cross when out the digital audio interface
	 * */
	switch (mode & MIXDAC_MIXER_L_MASK) {
	case MIXDAC_MIXER_L_TO_DACL:
		mixer_dacl = CODEC_MIX_FUNC_CROSS;
		break;
	case MIXDAC_MIXER_R_TO_DACL:
		mixer_dacl = CODEC_MIX_FUNC_NORMAL;
		break;
	case MIXDAC_MIXER_LR_TO_DACL:
		mixer_dacl = CODEC_MIX_FUNC_MIXED;
		break;
	case MIXDAC_MIXER_NO_TO_DACL:
		mixer_dacl = CODEC_MIX_FUNC_NOINPUT;
		break;
	}

	switch (mode & MIXDAC_MIXER_R_MASK) {
	case MIXDAC_MIXER_L_TO_DACR:
		mixer_dacr = CODEC_MIX_FUNC_NORMAL;
		break;
	case MIXDAC_MIXER_R_TO_DACR:
		mixer_dacr = CODEC_MIX_FUNC_CROSS;
		break;
	case MIXDAC_MIXER_LR_TO_DACR:
		mixer_dacr = CODEC_MIX_FUNC_MIXED;
		break;
	case MIXDAC_MIXER_NO_TO_DACR:
		mixer_dacr = CODEC_MIX_FUNC_NOINPUT;
		break;
	}

	__codec_set_mix(CR_MIX1,mixer_dacl,mixer_dacr);
}

/*record mixer*/
static void codec_set_record_mixer(unsigned long mode)
{
	int mixer_inputr = CODEC_MIX_FUNC_NORMAL;
	int mixer_inputl = CODEC_MIX_FUNC_NORMAL;

	DUMP_ROUTE_PART_REGS("enter");
	switch(mode & RECORD_MIXER_MASK) {
	case RECORD_MIXER_NO_USE:
	case RECORD_MIXER_INPUT_ONLY:
		g_mixer_is_used &= ~MIXER_RECORD;
		__codec_set_rec_mix_mode(CODEC_RECORD_MIX_INPUT_ONLY);
		break;
	case RECORD_MIXER_INPUT_AND_DAC:
		g_mixer_is_used |= MIXER_RECORD;
		__codec_mix_enable();
		__codec_set_rec_mix_mode(CODEC_RECORD_MIX_INPUT_AND_DAC);
		break;
	}

	switch (mode & AIADC_MIXER_L_MASK) {
	case AIADC_MIXER_INPUTL_TO_L:
		mixer_inputl = CODEC_MIX_FUNC_NORMAL;
		break;
	case AIADC_MIXER_INPUTR_TO_L:
		mixer_inputl = CODEC_MIX_FUNC_CROSS;
		break;
	case AIADC_MIXER_INPUTLR_TO_L:
		mixer_inputl = CODEC_MIX_FUNC_MIXED;
		break;
	case AIADC_MIXER_NOINPUT_TO_L:
		mixer_inputl = CODEC_MIX_FUNC_NOINPUT;
		break;
	}

	switch (mode & AIADC_MIXER_R_MASK) {
	case AIADC_MIXER_INPUTL_TO_R:
		mixer_inputr = CODEC_MIX_FUNC_CROSS;
		break;
	case AIADC_MIXER_INPUTR_TO_R:
		mixer_inputr = CODEC_MIX_FUNC_NORMAL;
		break;
	case AIADC_MIXER_INPUTLR_TO_R:
		mixer_inputr = CODEC_MIX_FUNC_MIXED;
		break;
	case AIADC_MIXER_NOINPUT_TO_R:
		mixer_inputr = CODEC_MIX_FUNC_NOINPUT;
		break;
	}

	__codec_set_mix(CR_MIX2,mixer_inputl,mixer_inputr);

	if (mode & RECORD_MIXER_INPUT_ONLY || mode & RECORD_MIXER_NO_USE) {
		if (!g_mixer_is_used)
			__codec_mix_disable();
		return;
	} else
		codec_set_mixer(mode);

	DUMP_ROUTE_PART_REGS("leave");
}

/*replay mixer*/
static void codec_set_replay_mixer(unsigned long mode)
{
	/* Note :there is a ic buge so we must exchange the left and right channel,
	 * do not modify it, the buge is follow:
	 * dir and dil is cross when out the digital audio interface
	 */
	uint32_t mixer_l = CODEC_MIX_FUNC_NORMAL;
	uint32_t mixer_r = CODEC_MIX_FUNC_NORMAL;

	DUMP_ROUTE_PART_REGS("enter");
	switch (mode & REPLAY_MIXER_MASK) {
	case REPLAY_MIXER_NO_USE:
	case REPLAY_MIXER_DAC_ONLY:
		g_mixer_is_used &= ~MIXER_REPLAY;
		__codec_set_rep_mix_mode(CODEC_PLAYBACK_MIX_DAC_ONLY);
		break;
	case REPLAY_MIXER_DAC_AND_ADC:
		g_mixer_is_used |= MIXER_REPLAY;
		__codec_mix_enable();
		__codec_set_rep_mix_mode(CODEC_PLAYBACK_MIX_DAC_AND_ADC);
		break;
	}

	switch (mode & AIDAC_MIXER_L_MASK) {
	case AIDAC_MIXER_DACL_TO_L:
		if (!(g_mixer_is_used & MIXER_REPLAY))
			mixer_l = CODEC_MIX_FUNC_CROSS;
		else
			mixer_l = CODEC_MIX_FUNC_NORMAL;
		break;
	case AIDAC_MIXER_DACR_TO_L:
		if (!(g_mixer_is_used & MIXER_REPLAY))
			mixer_l = CODEC_MIX_FUNC_NORMAL;
		else
			mixer_l = CODEC_MIX_FUNC_CROSS;
		break;
	case AIDAC_MIXER_DACLR_TO_L:
		mixer_l = CODEC_MIX_FUNC_MIXED;
		break;
	case AIDAC_MIXER_NODAC_TO_L:
		mixer_l = CODEC_MIX_FUNC_NOINPUT;
		break;
	}

	switch (mode & AIDAC_MIXER_R_MASK) {
	case AIDAC_MIXER_DACL_TO_R:
		if (!(g_mixer_is_used & MIXER_REPLAY))
			mixer_r = CODEC_MIX_FUNC_NORMAL;
		else
			mixer_r = CODEC_MIX_FUNC_CROSS;
		break;
	case AIDAC_MIXER_DACR_TO_R:
		if (!(g_mixer_is_used & MIXER_REPLAY))
			mixer_r = CODEC_MIX_FUNC_CROSS;
		else
			mixer_r = CODEC_MIX_FUNC_NORMAL;
		break;
	case AIDAC_MIXER_DACLR_TO_R:
		mixer_r = CODEC_MIX_FUNC_MIXED;
		break;
	case AIDAC_MIXER_NODAC_TO_R:
		mixer_r = CODEC_MIX_FUNC_NOINPUT;
		break;
	}

	__codec_set_mix(CR_MIX0,mixer_l,mixer_r);

	if (mode & REPLAY_MIXER_DAC_ONLY || mode & REPLAY_MIXER_NO_USE) {
		if (!g_mixer_is_used)
			__codec_mix_disable();
		return;
	} else
		codec_set_mixer(mode);

	DUMP_ROUTE_PART_REGS("leave");
}

static void codec_set_dac(int mode)
{
	int dac_mute_not_enable = 0;
	DUMP_ROUTE_PART_REGS("enter");

	switch(mode){
	case DAC_STEREO:
		if(__codec_get_sb_dac() == POWER_OFF){
			__codec_switch_sb_dac(POWER_ON);
			udelay(500);
		}
		if (__codec_get_dac_mode() != CODEC_DAC_STEREO) {
			if (!__codec_get_dac_mute()) {
				__codec_set_irq_flag(1 << IFR_DAC_MUTE_EVENT);
				__codec_enable_dac_mute();
				codec_wait_event_complete(IFR_DAC_MUTE_EVENT,CODEC_IN_MUTE);
				dac_mute_not_enable = 1;
			}
			__codec_set_dac_mode(CODEC_DAC_STEREO);
			if (dac_mute_not_enable) {
				__codec_set_irq_flag(1 << IFR_DAC_MUTE_EVENT);
				__codec_disable_dac_mute();
				codec_wait_event_complete(IFR_DAC_MUTE_EVENT,CODEC_NOT_MUTE);
			}
		}

		break;

	case DAC_STEREO_WITH_LEFT_ONLY:
		if(__codec_get_sb_dac() == POWER_OFF){
			__codec_switch_sb_dac(POWER_ON);
			udelay(500);
		}
		if (__codec_get_dac_mode() != CODEC_DAC_LEFT_ONLY) {
			if(!__codec_get_dac_mute()){
				__codec_set_irq_flag(1 << IFR_DAC_MUTE_EVENT);
				__codec_disable_dac_mute();
				codec_wait_event_complete(IFR_DAC_MUTE_EVENT,CODEC_IN_MUTE);
				dac_mute_not_enable = 1;
			}
			__codec_set_dac_mode(CODEC_DAC_LEFT_ONLY);
			if (dac_mute_not_enable) {
				__codec_set_irq_flag(1 << IFR_DAC_MUTE_EVENT);
				__codec_disable_dac_mute();
				codec_wait_event_complete(IFR_DAC_MUTE_EVENT,CODEC_NOT_MUTE);
			}
		}
		break;

	case DAC_DISABLE:
		if(!__codec_get_dac_mute()){
			/* clear IFR_DAC_MUTE_EVENT */
			__codec_set_irq_flag(1 << IFR_DAC_MUTE_EVENT);
			/* turn off dac */
			__codec_enable_dac_mute();
			/* wait IFR_DAC_MUTE_EVENT set */
			codec_wait_event_complete(IFR_DAC_MUTE_EVENT,CODEC_IN_MUTE);
		}

		if(__codec_get_sb_dac() == POWER_ON)
			__codec_switch_sb_dac(POWER_OFF);
		break;

	default:
		printk("JZ_CODEC: line: %d, dac mode error!\n", __LINE__);
	}

	DUMP_ROUTE_PART_REGS("leave");
}

static void codec_set_hp_mux(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");

	switch(mode){

	case HP_MUX_DACL_TO_L_DACR_TO_R:
		__codec_set_hp_mux(CODEC_DACRL_TO_HP);
		break;

	case HP_MUX_DACL_TO_LR:
		__codec_set_hp_mux(CODEC_DACL_TO_HP);
		break;

	case HP_MUX_INPUTL_TO_L_INPUTR_TO_R:
		__codec_set_hp_mux(CODEC_INPUTRL_TO_HP);
		break;

	case HP_MUX_INPUTL_TO_LR:
		__codec_set_hp_mux(CODEC_INPUTL_TO_HP);
		break;

	case HP_MUX_INPUTR_TO_LR:
		__codec_set_hp_mux(CODEC_INPUTR_TO_HP);
		break;

	default:
		printk("JZ_CODEC: line: %d, replay mux mode error!\n", __LINE__);
	}

	DUMP_ROUTE_PART_REGS("leave");
}

static int codec_get_gain_hp_left(void);
static int codec_get_gain_hp_right(void);
static void codec_set_gain_hp_left(int);
static void codec_set_gain_hp_right(int);

static void codec_set_hp(int mode)
{
	int dac_mute_not_enable = 0;
	int linein1_to_bypass_power_on = 0;
	int linein2_to_bypass_power_on = 0;
	int	curr_hp_left_vol;
	int	curr_hp_right_vol;

	DUMP_ROUTE_PART_REGS("enter");

	switch(mode){

	case HP_ENABLE:
		if (__codec_get_sb_hp() == POWER_OFF) {
			__codec_enable_hp_mute();
			mdelay(1);
			if (__codec_get_sb_linein1_bypass() == POWER_ON) {
				__codec_switch_sb_linein1_bypass(POWER_OFF);
				linein1_to_bypass_power_on = 1;
			}
			if (__codec_get_sb_linein2_bypass() == POWER_ON) {
				__codec_switch_sb_linein2_bypass(POWER_OFF);
				linein2_to_bypass_power_on = 1;
			}
			if ((!__codec_get_dac_mute()) && (__codec_get_sb_dac() == POWER_ON)) {
				/* enable dac mute */
				__codec_set_irq_flag(1 << IFR_DAC_MUTE_EVENT);
				__codec_enable_dac_mute();
				codec_wait_event_complete(IFR_DAC_MUTE_EVENT,CODEC_IN_MUTE);
				dac_mute_not_enable = 1;
			}

			__codec_disable_hp_mute();

			curr_hp_left_vol = codec_get_gain_hp_left();
			curr_hp_right_vol = codec_get_gain_hp_right();
			if (curr_hp_left_vol != 6)
				codec_set_gain_hp_left(6);
			if (curr_hp_right_vol != 6)
				codec_set_gain_hp_right(6);
			mdelay(1);

			/* turn on sb_hp */
			__codec_set_irq_flag(1 << IFR_DAC_MODE_EVENT);
			__codec_switch_sb_hp(POWER_ON);
			codec_wait_event_complete(IFR_DAC_MODE_EVENT,CODEC_PROGRAME_MODE);

			if (curr_hp_left_vol != 6)
				codec_set_gain_hp_left(curr_hp_left_vol);
			if (curr_hp_right_vol != 6)
				codec_set_gain_hp_right(curr_hp_right_vol);

			if (linein1_to_bypass_power_on == 1)
				__codec_switch_sb_linein1_bypass(POWER_ON);
			if (linein2_to_bypass_power_on == 1)
				__codec_switch_sb_linein2_bypass(POWER_ON);

			if (dac_mute_not_enable) {
				/*disable dac mute*/
				__codec_set_irq_flag(1 << IFR_DAC_MUTE_EVENT);
				__codec_disable_dac_mute();
				codec_wait_event_complete(IFR_DAC_MUTE_EVENT,CODEC_NOT_MUTE);
			}
		}
		break;

	case HP_DISABLE:
		__codec_disable_hp_mute();
		mdelay(1);
		if(__codec_get_sb_hp() == POWER_ON)
		{
			if(__codec_get_sb_linein1_bypass() == POWER_ON) {
				__codec_switch_sb_linein1_bypass(POWER_OFF);
				linein1_to_bypass_power_on = 1;
			}
			if(__codec_get_sb_linein2_bypass() == POWER_ON) {
				__codec_switch_sb_linein2_bypass(POWER_OFF);
				linein2_to_bypass_power_on = 1;
			}

			if((!__codec_get_dac_mute()) && (__codec_get_sb_dac() == POWER_ON)){
				/* enable dac mute */
				__codec_set_irq_flag(1 << IFR_DAC_MUTE_EVENT);
				__codec_enable_dac_mute();
				codec_wait_event_complete(IFR_DAC_MUTE_EVENT,CODEC_IN_MUTE);
				dac_mute_not_enable = 1;

			}

			curr_hp_left_vol = codec_get_gain_hp_left();
			curr_hp_right_vol = codec_get_gain_hp_right();
			if (curr_hp_left_vol != 6)
				codec_set_gain_hp_left(6);
			if (curr_hp_right_vol != 6)
				codec_set_gain_hp_right(6);
			mdelay(1);

			/* turn off sb_hp */
			__codec_set_irq_flag(1 << IFR_DAC_MODE_EVENT);
			__codec_switch_sb_hp(POWER_OFF);
			codec_wait_event_complete(IFR_DAC_MODE_EVENT,CODEC_PROGRAME_MODE);

			__codec_enable_hp_mute();

			if (linein1_to_bypass_power_on == 1)
				__codec_switch_sb_linein1_bypass(POWER_ON);
			if (linein2_to_bypass_power_on == 1)
				__codec_switch_sb_linein2_bypass(POWER_ON);

			if(dac_mute_not_enable){
				/*disable dac mute*/
				__codec_set_irq_flag(1 << IFR_DAC_MUTE_EVENT);
				__codec_disable_dac_mute();
				codec_wait_event_complete(IFR_DAC_MUTE_EVENT,CODEC_NOT_MUTE);
			}

			if (curr_hp_left_vol != 6)
				codec_set_gain_hp_left(curr_hp_left_vol);
			if (curr_hp_right_vol != 6)
				codec_set_gain_hp_right(curr_hp_right_vol);
		}
		break;

	default:
		printk("JZ_CODEC: line: %d, hp mode error!\n", __LINE__);
	}

	DUMP_ROUTE_PART_REGS("leave");
}

static void codec_set_lineout_mux(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");

	switch(mode){

	case LO_MUX_INPUTL_TO_LO:
		__codec_set_lineout_mux(CODEC_INPUTL_TO_LO);
		break;

	case LO_MUX_INPUTR_TO_LO:
		__codec_set_lineout_mux(CODEC_INPUTR_TO_LO);
		break;

	case LO_MUX_INPUTLR_TO_LO:
		__codec_set_lineout_mux(CODEC_INPUTLR_TO_LO);
		break;

	case LO_MUX_DACL_TO_LO:
		__codec_set_lineout_mux(CODEC_DACL_TO_LO);
		break;

	case LO_MUX_DACR_TO_LO:
		__codec_set_lineout_mux(CODEC_DACR_TO_LO);
		break;

	case LO_MUX_DACLR_TO_LO:
		__codec_set_lineout_mux(CODEC_DACLR_TO_LO);
		break;

	default:
		printk("JZ_CODEC: line: %d, replay mux mode error!\n", __LINE__);
	}

	DUMP_ROUTE_PART_REGS("leave");
}

static void codec_set_lineout(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");

	switch(mode) {

	case LINEOUT_ENABLE:
		if (__codec_get_sb_line_out() == POWER_OFF) {
			__codec_switch_sb_line_out(POWER_ON);
		}
		__codec_disable_lineout_mute();
		break;

	case LINEOUT_DISABLE:
		if(__codec_get_sb_line_out() == POWER_ON) {
			__codec_switch_sb_line_out(POWER_OFF);
		}
		__codec_enable_lineout_mute();
		break;

	default:
		printk("JZ_CODEC: line: %d, lineout mode error!\n", __LINE__);
	}

	DUMP_ROUTE_PART_REGS("leave");
}
void codec_set_agc(int mode)
{
	DUMP_ROUTE_PART_REGS("enter");

	switch(mode){

	case AGC_ENABLE:
		__codec_enable_agc();
		break;

	case AGC_DISABLE:
		__codec_disable_agc();
		break;

	default:
		printk("JZ_CODEC: line: %d, agc mode error!\n", __LINE__);
	}

	DUMP_ROUTE_PART_REGS("leave");
}
/*=================route attibute(gain) functions======================*/

//--------------------- input left (linein1 or mic1)
static int codec_get_gain_input_left(void)
{
	int val,gain;
	DUMP_GAIN_PART_REGS("enter");

	val =  __codec_get_gm1();
	gain = val * 4;

	DUMP_GAIN_PART_REGS("leave");
	return gain;
}

static int codec_set_gain_input_left(int gain)
{
	int val;

	DUMP_GAIN_PART_REGS("enter");

	if (gain < 0)
		gain = 0;
	else if (gain > 20)
		gain = 20;

	val = gain / 4;

	__codec_set_gm1(val);

	if ((val = codec_get_gain_input_left()) != gain)
		printk("JZ_CODEC: codec_set_gain_input_left error!\n");

	DUMP_GAIN_PART_REGS("leave");

	return val;
}

//--------------------- input right (linein2 or mic2)
static int codec_get_gain_input_right(void)
{
	int val,gain;
	DUMP_GAIN_PART_REGS("enter");

	val =  __codec_get_gm2();
	gain = val * 4;

	DUMP_GAIN_PART_REGS("leave");
	return gain;
}

static int codec_set_gain_input_right(int gain)
{
	int val;

	DUMP_GAIN_PART_REGS("enter");

	if (gain < 0)
		gain = 0;
	else if (gain > 20)
		gain = 20;

	val = gain / 4;

	__codec_set_gm2(val);

	if ((val = codec_get_gain_input_right()) != gain)
		printk("JZ_CODEC: codec_set_gain_input_right error!\n");

	DUMP_GAIN_PART_REGS("leave");

	return val;
}


//--------------------- input left bypass (linein1 or mic1 bypass gain)

static int codec_get_gain_input_bypass_left(void)
{
	int val,gain;
	DUMP_GAIN_PART_REGS("enter");

	val = __codec_get_gil();

	gain = (6 - val);

	DUMP_GAIN_PART_REGS("leave");
	return gain;
}

static void codec_set_gain_input_bypass_left(int gain)
{
	int val;

	DUMP_GAIN_PART_REGS("enter");

	if (gain > 6)
		gain = 6;
	else if (gain < -25)
		gain = -25;

	val = (6 - gain);

	__codec_set_gil(val);

	if (codec_get_gain_input_bypass_left() != gain)
		printk("JZ_CODEC:codec_set_gain_input_bypass_left error!\n");

	DUMP_GAIN_PART_REGS("leave");
}

//--------------------- input bypass (linein2 or mic2 bypass gain)
static int codec_get_gain_input_bypass_right(void)
{
	int val,gain;
	DUMP_GAIN_PART_REGS("enter");

	val = __codec_get_gir();

	gain = (6 - val);

	DUMP_GAIN_PART_REGS("leave");
	return gain;
}

static void codec_set_gain_input_bypass_right(int gain)
{
	int val;

	DUMP_GAIN_PART_REGS("enter");

	if (gain > 6)
		gain = 6;
	else if (gain < -25)
		gain = -25;

	val = (6 - gain);

	__codec_set_gir(val);

	if (codec_get_gain_input_bypass_right() != gain)
		printk("JZ_CODEC: codec_set_gain_input_bypass_right error!\n");

	DUMP_GAIN_PART_REGS("leave");
}

//--------------------- adc left
static int codec_get_gain_adc_left(void)
{
	int val,gain;
	DUMP_GAIN_PART_REGS("enter");

	val = __codec_get_gidl();

	gain = val;

	DUMP_GAIN_PART_REGS("leave");

	return gain;
}

static int codec_set_gain_adc_left(int gain)
{
	int val;

	DUMP_GAIN_PART_REGS("enter");

	if (gain < 0)
		gain = 0;
	else if (gain > 23)
		gain = 23;

	val = gain;

	__codec_set_gidl(val);

	if ((val = codec_get_gain_adc_left()) != gain)
		printk("JZ_CODEC: codec_set_gain_adc_left error!\n");

	DUMP_GAIN_PART_REGS("leave");
	return val;
}

//--------------------- adc right
static int codec_get_gain_adc_right(void)
{
	int val,gain;
	DUMP_GAIN_PART_REGS("enter");

	val = __codec_get_gidr();

	gain = val;

	DUMP_GAIN_PART_REGS("leave");

	return gain;
}

static int codec_set_gain_adc_right(int gain)
{
	int val;

	DUMP_GAIN_PART_REGS("enter");

	if (gain < 0)
		gain = 0;
	else if (gain > 23)
		gain = 23;

	val = gain;

	__codec_set_gidr(val);

	if ((val = codec_get_gain_adc_right()) != gain)
		printk("JZ_CODEC: codec_set_gain_adc_right error!\n");

	DUMP_GAIN_PART_REGS("leave");
	return val;
}

//--------------------- record mixer

int codec_get_gain_record_mixer (void)
{
	int val,gain;
	DUMP_GAIN_PART_REGS("enter");

	val = __codec_get_gimixl();

	gain = -val;

	DUMP_GAIN_PART_REGS("leave");
	return gain;
}

void codec_set_gain_record_mixer(int gain)
{
	int val;

	DUMP_GAIN_PART_REGS("enter");

	if (gain > 0)
		gain = 0;
	else if (gain < -31)
		gain = -31;

	val = -gain;

	if (!__test_mixin_is_sync_gain())
		__codec_enable_mixin_sync_gain();
	__codec_set_gimixl(val);

	if (codec_get_gain_record_mixer() != gain)
		printk("JZ_CODEC: codec_set_gain_record_mixer error!\n");

	DUMP_GAIN_PART_REGS("leave");
}

//--------------------- replay mixer
static int codec_get_gain_replay_mixer(void)
{
	int val,gain;
	DUMP_GAIN_PART_REGS("enter");

	val = __codec_get_gomixl();

	gain = -val;

	DUMP_GAIN_PART_REGS("leave");
	return gain;
}

static void codec_set_gain_replay_mixer(int gain)
{
	int val;

	DUMP_GAIN_PART_REGS("enter");

	if (gain > 0)
		gain = 0;
	else if (gain < -31)
		gain = -31;

	val = -gain;
	if (!__test_mixout_is_sync_gain())
		__codec_enable_mixout_sync_gain();
	__codec_set_gomixl(val);

	if (codec_get_gain_replay_mixer() != gain)
		printk("JZ_CODEC: codec_set_gain_replay_mixer error!\n");

	DUMP_GAIN_PART_REGS("leave");
}

//--------------------- dac left
static int codec_get_gain_dac_left(void)
{
	int val,gain;
	DUMP_GAIN_PART_REGS("enter");

	val = __codec_get_godl();

	gain = -val;

	DUMP_GAIN_PART_REGS("leave");

	return gain;
}

void codec_set_gain_dac_left(int gain)
{
	int val;

	DUMP_GAIN_PART_REGS("enter");
	if (gain > 0)
		gain = 0;
	else if (gain < -31)
		gain = -31;

	val = -gain;

	__codec_set_godl(val);

	if (codec_get_gain_dac_left() != gain)
		printk("JZ_CODEC: codec_set_gain_dac_left error!\n");

	DUMP_GAIN_PART_REGS("leave");
}

//--------------------- dac right
int codec_get_gain_dac_right(void)
{
	int val,gain;
	DUMP_GAIN_PART_REGS("enter");

	val = __codec_get_godr();

	gain = -val;

	DUMP_GAIN_PART_REGS("leave");

	return gain;
}

void codec_set_gain_dac_right(int gain)
{
	int val;

	DUMP_GAIN_PART_REGS("enter");

	if (gain > 0)
		gain = 0;
	else if (gain < -31)
		gain = -31;

	val = -gain;

	__codec_set_godr(val);

	if (codec_get_gain_dac_right() != gain)
		printk("JZ_CODEC: codec_set_gain_dac_right error!\n");

	DUMP_GAIN_PART_REGS("leave");
}

//--------------------- hp left
static int codec_get_gain_hp_left(void)
{
	int val,gain;
	DUMP_GAIN_PART_REGS("enter");

	val = __codec_get_gol();

	gain = (6 - val);

	DUMP_GAIN_PART_REGS("leave");
	return gain;
}

void codec_set_gain_hp_left(int gain)
{
	int val;

	DUMP_GAIN_PART_REGS("enter");

	if (gain > 6)
		gain = 6;
	else if (gain < -25)
		gain = -25;

	val = (6 - gain);

	__codec_set_gol(val);

	if (codec_get_gain_hp_left() != gain)
		printk("JZ_CODEC: codec_set_gain_hp_left error!\n");

	DUMP_GAIN_PART_REGS("leave");
}

//--------------------- hp right
static int codec_get_gain_hp_right(void)
{
	int val,gain;
	DUMP_GAIN_PART_REGS("enter");

	val = __codec_get_gor();

	gain = (6 - val);

	DUMP_GAIN_PART_REGS("leave");
	return gain;
}

void codec_set_gain_hp_right(int gain)
{
	int val;

	DUMP_GAIN_PART_REGS("enter");

	if (gain > 6)
		gain = 6;
	else if (gain < -25)
		gain = -25;

	val = (6 - gain);

	__codec_set_gor(val);

	if (codec_get_gain_hp_right() != gain)
		printk("JZ_CODEC: codec_set_gain_hp_right error!\n");

	DUMP_GAIN_PART_REGS("leave");
}

/***************************************************************************************\
 *codec route                                                                            *
 \***************************************************************************************/
static void codec_set_route_base(const void *arg)
{
	route_conf_base *conf = (route_conf_base *)arg;

	/*codec turn on sb and sb_sleep*/
	if (conf->route_ready_mode)
		codec_prepare_ready(conf->route_ready_mode);

	/*--------------route---------------*/
	/* record path */
	if (conf->route_adc_mode)
		codec_set_adc(conf->route_adc_mode);
	if (conf->route_line1_mode)
		codec_set_line1(conf->route_line1_mode);
	if (conf->route_line2_mode)
		codec_set_line2(conf->route_line2_mode);
	if (conf->route_inputl_mux_mode)
		codec_set_inputl_mux(conf->route_inputl_mux_mode);
	if (conf->route_inputr_mux_mode)
		codec_set_inputr_mux(conf->route_inputr_mux_mode);
	if (conf->route_inputl_to_bypass_mode)
		codec_set_inputl_to_bypass(conf->route_inputl_to_bypass_mode);
	if (conf->route_inputr_to_bypass_mode)
		codec_set_inputr_to_bypass(conf->route_inputr_to_bypass_mode);
	if (conf->route_record_mux_mode)
		codec_set_record_mux(conf->route_record_mux_mode);
	if (conf->route_record_mixer_mode)
		codec_set_record_mixer(conf->route_record_mixer_mode);

	if (conf->route_inputl_mode)
		codec_set_inputl(conf->route_inputl_mode);
	if (conf->route_inputr_mode)
		codec_set_inputr(conf->route_inputr_mode);
	if (conf->route_mic1_mode)
		codec_set_mic1(conf->route_mic1_mode);
	if (conf->route_mic2_mode)
		codec_set_mic2(conf->route_mic2_mode);


	/* replay path */
	if (conf->route_replay_mixer_mode)
		codec_set_replay_mixer(conf->route_replay_mixer_mode);

	if (conf->route_hp_mux_mode)
		codec_set_hp_mux(conf->route_hp_mux_mode);
	if (conf->route_lineout_mux_mode)
		codec_set_lineout_mux(conf->route_lineout_mux_mode);
	if (conf->route_lineout_mode)
		codec_set_lineout(conf->route_lineout_mode);
	if (conf->route_dac_mode)
		codec_set_dac((conf->route_dac_mode));
	if (conf->route_hp_mode)
		codec_set_hp(conf->route_hp_mode);

	/*----------------attibute-------------*/
	/* auto gain */
	if (conf->attibute_agc_mode)
		codec_set_agc(conf->attibute_agc_mode);

	/* gain , use 32 instead of 0 */
	if (conf->attibute_record_mixer_gain) {
		if (conf->attibute_record_mixer_gain == 32)
			codec_set_gain_record_mixer(0);
		else
			codec_set_gain_record_mixer(conf->attibute_record_mixer_gain);
	}
	if (conf->attibute_replay_mixer_gain) {
		if (conf->attibute_replay_mixer_gain == 32)
			codec_set_gain_replay_mixer(0);
		else
			codec_set_gain_replay_mixer(conf->attibute_replay_mixer_gain);
	}
	/*Note it should not be set in andriod ,below */
	if (conf->attibute_input_l_gain) {
		if (conf->attibute_input_l_gain == 32)
			codec_set_gain_input_left(0);
		else
			codec_set_gain_input_left(conf->attibute_input_l_gain);
	}
	if (conf->attibute_input_r_gain){
		if (conf->attibute_input_r_gain == 32)
			codec_set_gain_input_right(0);
		else
			codec_set_gain_input_right(conf->attibute_input_r_gain);
	}
	if (conf->attibute_input_bypass_l_gain) {
		if (conf->attibute_input_bypass_l_gain == 32)
			codec_set_gain_input_bypass_left(0);
		else
			codec_set_gain_input_bypass_left(conf->attibute_input_bypass_l_gain);
	}
	if (conf->attibute_input_bypass_r_gain) {
		if (conf->attibute_input_bypass_r_gain == 32)
			codec_set_gain_input_bypass_right(0);
		else
			codec_set_gain_input_bypass_right(conf->attibute_input_bypass_r_gain);
	}
	if (conf->attibute_adc_l_gain) {
		if (conf->attibute_adc_l_gain == 32)
			codec_set_gain_adc_left(0);
		else
			codec_set_gain_adc_left(conf->attibute_adc_l_gain);
	}
	if (conf->attibute_adc_r_gain) {
		if (conf->attibute_adc_r_gain == 32)
			codec_set_gain_adc_right(0);
		else
			codec_set_gain_adc_right(conf->attibute_adc_r_gain);
	}
	if (conf->attibute_dac_l_gain) {
		if (conf->attibute_dac_l_gain == 32)
			codec_set_gain_dac_left(0);
		else
			codec_set_gain_dac_left(conf->attibute_dac_l_gain);
	}
	if (conf->attibute_dac_r_gain) {
		if (conf->attibute_dac_r_gain == 32)
			codec_set_gain_dac_right(0);
		else
			codec_set_gain_dac_right(conf->attibute_dac_r_gain);
	}
	if (conf->attibute_hp_l_gain) {
		if (conf->attibute_hp_l_gain == 32)
			codec_set_gain_hp_left(0);
		else
			codec_set_gain_hp_left(conf->attibute_hp_l_gain);
	}
	if (conf->attibute_hp_r_gain) {
		if (conf->attibute_hp_r_gain == 32)
			codec_set_gain_hp_right(0);
		else
			codec_set_gain_hp_right(conf->attibute_hp_r_gain);
	}
}

static int codec_set_mic_volume(int* val);
static int codec_set_replay_volume(int *val);

static void codec_set_gain_base(struct snd_board_route *route)
{

	int old_volume_base = 0;
	int tmp_volume = 0;

	if (codec_platform_data) {
		tmp_volume = user_record_volume;
		old_volume_base = codec_platform_data->record_volume_base;
		if (route->record_volume_base) {
			if (route->record_volume_base == 32)
				codec_platform_data->record_volume_base = 0;
			else
				codec_platform_data->record_volume_base = route->record_volume_base;
		} else
			codec_platform_data->record_volume_base = default_record_volume_base;

		if (old_volume_base != codec_platform_data->record_volume_base)
			codec_set_mic_volume(&tmp_volume);
	}

	/* set replay hp output gain base */
	if (codec_platform_data) {
		tmp_volume = user_replay_volume;
		old_volume_base = codec_platform_data->replay_volume_base;
		if (route->replay_volume_base) {
			if (route->replay_volume_base == 32)
				codec_platform_data->replay_volume_base = 0;
			else
				codec_platform_data->replay_volume_base = route->replay_volume_base;
		} else
			codec_platform_data->replay_volume_base = default_replay_volume_base;

		if (old_volume_base !=  codec_platform_data->replay_volume_base)
			codec_set_replay_volume(&tmp_volume);
	}

	/* set record digtal volume base */
	if (codec_platform_data) {
		if (route->record_digital_volume_base) {
			if (route->record_digital_volume_base == 32)
				codec_platform_data->record_digital_volume_base = 0;
			else
				codec_platform_data->record_digital_volume_base = route->record_digital_volume_base;
			codec_set_gain_adc_left(codec_platform_data->record_digital_volume_base);
			codec_set_gain_adc_right(codec_platform_data->record_digital_volume_base);
		} else if (codec_platform_data->record_digital_volume_base !=
			   default_record_digital_volume_base) {
			codec_platform_data->record_digital_volume_base = default_record_digital_volume_base;
			codec_set_gain_adc_left(codec_platform_data->record_digital_volume_base);
			codec_set_gain_adc_right(codec_platform_data->record_digital_volume_base);
		}
	}

	/* set replay digital volume base */
	if (codec_platform_data) {
		if (route->replay_digital_volume_base) {
			if (route->replay_digital_volume_base == 32)
				codec_platform_data->replay_digital_volume_base = 0;
			else
				codec_platform_data->replay_digital_volume_base = route->replay_digital_volume_base;
			codec_set_gain_dac_left(codec_platform_data->replay_digital_volume_base);
			codec_set_gain_dac_right(codec_platform_data->replay_digital_volume_base);
		} else if (codec_platform_data->replay_digital_volume_base !=
			   default_replay_digital_volume_base) {
			codec_platform_data->replay_digital_volume_base = default_replay_digital_volume_base;
			codec_set_gain_dac_left(codec_platform_data->replay_digital_volume_base);
			codec_set_gain_dac_right(codec_platform_data->replay_digital_volume_base);
		}
	}

	/*set bypass left volume base*/
	if (codec_platform_data) {
		if (route->bypass_l_volume_base) {
			if (route->bypass_l_volume_base == 32)
				codec_platform_data->bypass_l_volume_base = 0;
			else
				codec_platform_data->bypass_l_volume_base = route->bypass_l_volume_base;
			codec_set_gain_input_bypass_left(codec_platform_data->bypass_l_volume_base);
		}
	}

	/*set bypass right volume base*/
	if (codec_platform_data) {
		if (route->bypass_r_volume_base) {
			if (route->bypass_r_volume_base == 32)
				codec_platform_data->bypass_r_volume_base = 0;
			else
				codec_platform_data->bypass_r_volume_base = route->bypass_r_volume_base;
			codec_set_gain_input_bypass_right(codec_platform_data->bypass_r_volume_base);
		}
	}
}

/***************************************************************************************\
 *ioctl support function                                                               *
\***************************************************************************************/
/*------------------sub fun-------------------*/
static int gpio_enable_hp_mute(void)
{
	int val = -1;
	if(codec_platform_data && (codec_platform_data->gpio_hp_mute.gpio != -1)) {
		val = gpio_get_value(codec_platform_data->gpio_hp_mute.gpio);
		if (codec_platform_data->gpio_hp_mute.active_level) {
			gpio_direction_output(codec_platform_data->gpio_hp_mute.gpio , 1);
		} else {
			gpio_direction_output(codec_platform_data->gpio_hp_mute.gpio , 0);
			val = val == 1 ? 0 : val == 0 ? 1 : val ;
		}
	}
	DUMP_GPIO_STATE();
	return val;
}

static void gpio_disable_hp_mute(void)
{
	if(codec_platform_data && (codec_platform_data->gpio_hp_mute.gpio != -1)) {
		if (codec_platform_data->gpio_hp_mute.active_level) {
			gpio_direction_output(codec_platform_data->gpio_hp_mute.gpio , 0);
		} else {
			gpio_direction_output(codec_platform_data->gpio_hp_mute.gpio , 1);
		}
	}
	DUMP_GPIO_STATE();
}

static void gpio_enable_spk_en(void)
{
	int hp_unmute_state = 0;
	if(codec_platform_data && (codec_platform_data->gpio_spk_en.gpio != -1)) {
		if (!__codec_get_hp_mute()) {
			__codec_enable_hp_mute();
			hp_unmute_state = 1;
			mdelay(50);		/*FIXME :Avoid hardware pop maybe the speaker amp have stable time*/
		}
		if (codec_platform_data->gpio_spk_en.active_level) {
			gpio_direction_output(codec_platform_data->gpio_spk_en.gpio , 1);
		} else {
			gpio_direction_output(codec_platform_data->gpio_spk_en.gpio , 0);
		}
		if (hp_unmute_state)
			__codec_disable_hp_mute();
	}
}

static int gpio_disable_spk_en(void)
{
	int val = -1;
	if(codec_platform_data && (codec_platform_data->gpio_spk_en.gpio != -1)) {
		val = gpio_get_value(codec_platform_data->gpio_spk_en.gpio);
		if (codec_platform_data->gpio_spk_en.active_level) {
			gpio_direction_output(codec_platform_data->gpio_spk_en.gpio , 0);
		} else {
			val = val == 1 ? 0 : val == 0 ? 1 : val;
			gpio_direction_output(codec_platform_data->gpio_spk_en.gpio , 1);
		}
	}
	return val;
}

static void gpio_enable_handset_en(void)
{
	if(codec_platform_data && (codec_platform_data->gpio_handset_en.gpio != -1)) {
		if (codec_platform_data->gpio_handset_en.active_level) {
			gpio_direction_output(codec_platform_data->gpio_handset_en.gpio , 1);
		} else {
			gpio_direction_output(codec_platform_data->gpio_handset_en.gpio , 0);
		}
	}
}

static int gpio_disable_handset_en(void)
{
	int val = -1;
	if(codec_platform_data && (codec_platform_data->gpio_handset_en.gpio != -1)) {
		val = gpio_get_value(codec_platform_data->gpio_handset_en.gpio);
		if (codec_platform_data->gpio_handset_en.active_level) {
			gpio_direction_output(codec_platform_data->gpio_handset_en.gpio , 0);
		} else {
			gpio_direction_output(codec_platform_data->gpio_handset_en.gpio , 1);
			val = val == 1 ? 0 : val == 0 ? 1 : val ;
		}
	}
	return val;
}

static void gpio_select_headset_mic(void)
{
	if (codec_platform_data && codec_platform_data->gpio_buildin_mic_select.gpio != -1) {
		if (codec_platform_data->gpio_buildin_mic_select.active_level)
			gpio_direction_output(codec_platform_data->gpio_buildin_mic_select.gpio,0);
		else
			gpio_direction_output(codec_platform_data->gpio_buildin_mic_select.gpio,1);
	}
}

static void gpio_select_buildin_mic(void)
{
	if (codec_platform_data && codec_platform_data->gpio_buildin_mic_select.gpio != -1) {
		if (codec_platform_data->gpio_buildin_mic_select.active_level)
			gpio_direction_output(codec_platform_data->gpio_buildin_mic_select.gpio,1);
		else
			gpio_direction_output(codec_platform_data->gpio_buildin_mic_select.gpio,0);
	}
}

/*-----------------main fun-------------------*/
static int codec_set_board_route(struct snd_board_route *broute)
{
	int i = 0;
	int resave_hp_mute = -1;
	int resave_spk_en = -1;
	int resave_handset_en = -1;

	if (broute == NULL)
		return 0;

	/* set route */
	DUMP_ROUTE_NAME(broute->route);

	if (broute && ((cur_route == NULL) || (cur_route->route != broute->route))) {
		for (i = 0; codec_route_info[i].route_name != SND_ROUTE_NONE ; i ++) {
			if (broute->route == codec_route_info[i].route_name) {
				/* set hp mute and disable speaker by gpio */
				if (broute->gpio_hp_mute_stat != KEEP_OR_IGNORE)
					resave_hp_mute = gpio_enable_hp_mute();
				if (broute->gpio_spk_en_stat != KEEP_OR_IGNORE)
					resave_spk_en = gpio_disable_spk_en();
				if (broute->gpio_handset_en_stat != KEEP_OR_IGNORE)
					resave_handset_en = gpio_disable_handset_en();
				/* set route */
				codec_set_route_base(codec_route_info[i].route_conf);
				break;
			}
		}
		if (codec_route_info[i].route_name == SND_ROUTE_NONE) {
			printk("SET_ROUTE: codec set route error!, undecleard route, route = %d\n", broute->route);
			goto err_unclear_route;
		}
	} else
		printk("SET_ROUTE: waring: route not be setted!\n");

	if (broute->route != SND_ROUTE_RECORD_CLEAR) {
		/* keep_old_route is used in resume part and record release */
		if (cur_route == NULL || cur_route->route == SND_ROUTE_ALL_CLEAR)
			keep_old_route = broute;
		else if (cur_route->route >= SND_ROUTE_RECORD_ROUTE_START &&
			 cur_route->route <= SND_ROUTE_RECORD_ROUTE_END && keep_old_route != NULL) {
			/*DO NOTHING IN THIS CASE*/
		} else
			keep_old_route = cur_route;
		/* change cur_route */
		cur_route = broute;
	} else {
		if (cur_route != NULL) {
			if (cur_route->route >= SND_ROUTE_RECORD_ROUTE_START &&
			    cur_route->route <= SND_ROUTE_RECORD_ROUTE_END) {
				cur_route = keep_old_route;
			}
		}
	}

	/*set board gain*/
	codec_set_gain_base(broute);

	/* set gpio after set route */
	if (broute->gpio_hp_mute_stat == STATE_DISABLE ||
	    (resave_hp_mute == 0 && broute->gpio_hp_mute_stat == KEEP_OR_IGNORE))
		gpio_disable_hp_mute();
	else if (broute->gpio_hp_mute_stat == STATE_ENABLE ||
		 (resave_hp_mute == 1 && broute->gpio_hp_mute_stat == KEEP_OR_IGNORE))
		gpio_enable_hp_mute();

	if (broute->gpio_handset_en_stat == STATE_ENABLE ||
	    (resave_handset_en == 1 && broute->gpio_handset_en_stat == KEEP_OR_IGNORE))
		gpio_enable_handset_en();
	else if (broute->gpio_handset_en_stat == STATE_DISABLE ||
		 (resave_handset_en == 0 && broute->gpio_handset_en_stat == KEEP_OR_IGNORE))
		gpio_disable_handset_en();

	if (broute->gpio_spk_en_stat == STATE_ENABLE ||
	    (resave_spk_en == 1 && broute->gpio_spk_en_stat == KEEP_OR_IGNORE))
		gpio_enable_spk_en();
	else if (broute->gpio_spk_en_stat == STATE_DISABLE ||
		 (resave_spk_en == 0 && broute->gpio_spk_en_stat == KEEP_OR_IGNORE))
		gpio_disable_spk_en();

	if (broute->gpio_buildin_mic_en_stat == STATE_DISABLE)
		gpio_select_headset_mic();
	else if (broute->gpio_buildin_mic_en_stat == STATE_ENABLE)
		gpio_select_buildin_mic();

	DUMP_ROUTE_REGS("leave");

	return broute ? broute->route : (cur_route ? cur_route->route : SND_ROUTE_NONE);
err_unclear_route:
	return SND_ROUTE_NONE;
}

static int codec_set_default_route(int mode)
{
	int ret = 0;
	if (codec_platform_data) {
		if (codec_platform_data->replay_def_route.route == SND_ROUTE_NONE){
			codec_platform_data->replay_def_route.route = SND_ROUTE_REPLAY_DACRL_TO_LO;
		}

		if (codec_platform_data->record_def_route.route == SND_ROUTE_NONE) {
			codec_platform_data->record_def_route.route = SND_ROUTE_RECORD_MIC1_AN1;
		}
	}
	if (mode == CODEC_RWMODE) {
		ret =  codec_set_board_route(&codec_platform_data->replay_def_route);
		ret =  codec_set_board_route(&codec_platform_data->record_def_route);
	} else if (mode == CODEC_WMODE) {
		ret =  codec_set_board_route(&codec_platform_data->replay_def_route);
	} else if (mode == CODEC_RMODE){
		ret =  codec_set_board_route(&codec_platform_data->record_def_route);
	}

	return 0;
}

static struct snd_board_route tmp_broute;

static int codec_set_route(enum snd_codec_route_t route)
{
	tmp_broute.route = route;
	tmp_broute.gpio_handset_en_stat = KEEP_OR_IGNORE;
	if (route == SND_ROUTE_ALL_CLEAR || route == SND_ROUTE_REPLAY_CLEAR) {
		tmp_broute.gpio_spk_en_stat = STATE_DISABLE;
		tmp_broute.gpio_hp_mute_stat = STATE_ENABLE;
	} else {
		tmp_broute.gpio_spk_en_stat = KEEP_OR_IGNORE;
		tmp_broute.gpio_hp_mute_stat = KEEP_OR_IGNORE;
	}
	tmp_broute.replay_volume_base = 0;
	tmp_broute.record_volume_base = 0;
	tmp_broute.record_digital_volume_base = 0;
	tmp_broute.replay_digital_volume_base = 0;
	tmp_broute.bypass_l_volume_base = 0;
	tmp_broute.bypass_r_volume_base = 0;
	tmp_broute.gpio_buildin_mic_en_stat = KEEP_OR_IGNORE;
	return codec_set_board_route(&tmp_broute);
}

/*----------------------------------------*/
/****** codec_init ********/
static int codec_init(void)
{

	/* disable speaker and enable hp mute */
	gpio_enable_hp_mute();
	gpio_disable_spk_en();
	gpio_disable_handset_en();

	codec_prepare_ready(CODEC_WMODE);

	__codec_set_int_form(ICR_INT_HIGH);
	/* set IRQ mask and clear IRQ flags*/
	__codec_set_irq_mask(ICR_COMMON_MASK);
	__codec_set_irq_mask2(ICR_COMMON_MASK2);
	__codec_set_irq_flag(ICR_ALL_FLAG);
	__codec_set_irq_flag2(ICR_ALL_FLAG2);

	__codec_select_master_mode();
	__codec_select_adc_digital_interface(CODEC_I2S_INTERFACE);
	__codec_select_dac_digital_interface(CODEC_I2S_INTERFACE);
	/* set SYS_CLK to 12MHZ */
	__codec_set_crystal(codec_platform_data->codec_sys_clk);


	//__codec_set_irq_flag(1 << IFR_DAC_MUTE_EVENT);
	//__codec_enable_dac_mute();
	//codec_wait_event_complete(IFR_DAC_MUTE_EVENT,CODEC_IN_MUTE);

	if (codec_platform_data) {
		codec_set_gain_input_left(codec_platform_data->record_volume_base);
		codec_set_gain_input_right(codec_platform_data->record_volume_base);
		default_record_volume_base = codec_platform_data->record_volume_base;
	}
	/* set replay hp output gain base */
	if (codec_platform_data) {
		codec_set_gain_hp_right(codec_platform_data->replay_volume_base);
		codec_set_gain_hp_left(codec_platform_data->replay_volume_base);
		default_replay_volume_base = codec_platform_data->replay_volume_base;
	}
	/* set record digtal volume base */
	if (codec_platform_data) {
		codec_set_gain_adc_left(codec_platform_data->record_digital_volume_base);
		codec_set_gain_adc_right(codec_platform_data->record_digital_volume_base);
		default_record_digital_volume_base = codec_platform_data->record_digital_volume_base;
	}
	/* set replay digital volume base */
	if (codec_platform_data) {
		codec_set_gain_dac_left(codec_platform_data->replay_digital_volume_base);
		codec_set_gain_dac_right(codec_platform_data->replay_digital_volume_base);
		default_replay_digital_volume_base = codec_platform_data->replay_digital_volume_base;
	}

	if (codec_platform_data) {
		codec_set_gain_input_bypass_left(codec_platform_data->bypass_l_volume_base);
		default_bypass_l_volume_base = codec_platform_data->bypass_l_volume_base;
	}

	if (codec_platform_data) {
		codec_set_gain_input_bypass_right(codec_platform_data->bypass_r_volume_base);
		default_bypass_r_volume_base = codec_platform_data->bypass_r_volume_base;
	}

	/*micbias open all time, because it maybe bring cacophony*/
	__codec_switch_sb_micbias1(POWER_ON);
	__codec_switch_sb_micbias2(POWER_ON);

	return 0;
}

/****** codec_turn_off ********/
static int codec_turn_off(int mode)
{
	int ret = 0;

	if (mode & CODEC_RMODE) {
		ret = codec_set_route(SND_ROUTE_RECORD_CLEAR);
		if(ret != SND_ROUTE_RECORD_CLEAR)
		{
			printk("JZ CODEC: codec_turn_off_part record mode error!\n");
			return -1;
		}
	}
	if (mode & CODEC_WMODE) {
		ret = codec_set_route(SND_ROUTE_REPLAY_CLEAR);
		if(ret != SND_ROUTE_REPLAY_CLEAR)
		{
			printk("JZ CODEC: codec_turn_off_part replay mode error!\n");
			return -1;
		}
	}

	return ret;
}

/****** codec_shutdown *******/
static int codec_shutdown(void)
{
	/* disbale speaker and enbale hp mute */
	gpio_enable_hp_mute();
	gpio_disable_spk_en();
	gpio_disable_handset_en();

	/* shutdown sequence */
	__codec_set_irq_flag(1 << IFR_DAC_MUTE_EVENT);
	__codec_enable_dac_mute();
	codec_wait_event_complete(IFR_DAC_MUTE_EVENT,CODEC_IN_MUTE);

	__codec_set_irq_flag(1 << IFR_DAC_MODE_EVENT);
	__codec_switch_sb_hp(POWER_OFF);
	codec_wait_event_complete(IFR_DAC_MODE_EVENT,CODEC_PROGRAME_MODE);

	__codec_enable_hp_mute();
	udelay(500);

	__codec_switch_sb_dac(POWER_OFF);
	udelay(500);

	__codec_disable_dac_interface();
	__codec_disable_adc_interface();

	codec_sleep(10);
	__codec_switch_sb(POWER_OFF);
	codec_sleep(10);

	return 0;
}

/****** codec_reset **********/
static int codec_reset(void)
{
	/* select serial interface and work mode of adc and dac */
	__codec_select_adc_digital_interface(CODEC_I2S_INTERFACE);
	__codec_select_dac_digital_interface(CODEC_I2S_INTERFACE);

	/* reset codec ready for set route */
	codec_prepare_ready(CODEC_WMODE);
	return 0;
}

/******** codec_anti_pop ********/
static int codec_anti_pop_start(int mode)
{
	codec_prepare_ready(mode);
	switch(mode) {
	case CODEC_RWMODE:
	case CODEC_RMODE:
		break;
	case CODEC_WMODE:
		__codec_switch_sb_dac(POWER_ON);
		udelay(500);
		i2s_replay_zero_for_flush_codec();
		break;
	}
	return 0;
}

/******** codec_suspend ************/
static int codec_suspend(void)
{
	int ret = 10;

	g_codec_sleep_mode = 0;

	ret = codec_set_route(SND_ROUTE_ALL_CLEAR);
	if(ret != SND_ROUTE_ALL_CLEAR)
	{
		printk("JZ CODEC: codec_suspend_part error!\n");
		return 0;
	}

	__codec_switch_sb_sleep(POWER_OFF);
	codec_sleep(10);
	__codec_switch_sb(POWER_OFF);

	return 0;
}

static int codec_resume(void)
{
	int ret,tmp_route = 0;

	if (keep_old_route) {
		tmp_route = keep_old_route->route;
		ret = codec_set_board_route(keep_old_route);
		if(ret != tmp_route) {
			printk("JZ CODEC: codec_resume_part error!\n");
			return 0;
		}
	} else {
		__codec_switch_sb(POWER_ON);
		codec_sleep(250);
		__codec_switch_sb_sleep(POWER_ON);
		/*
		 * codec_sleep(400);
		 * this time we ignored becase other device resume waste time
		 */
	}

	g_codec_sleep_mode = 1;
	return 0;
}

/*---------------------------------------*/

/**
 * CODEC set device
 *
 * this is just a demo function, and it will be use as default
 * if it is not realized depend on difficent boards
 *
 */
static int codec_set_device(enum snd_device_t device)
{
	int ret = 0;
	int iserror = 0;

	printk("codec_set_device %d \n",device);
	switch (device) {
	case SND_DEVICE_HEADSET:
	case SND_DEVICE_HEADPHONE:
		if (codec_platform_data && codec_platform_data->replay_headset_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->replay_headset_route));
			if(ret != codec_platform_data->replay_headset_route.route) {
				return -1;
			}
		}
		break;

	case SND_DEVICE_SPEAKER:
		if (codec_platform_data && codec_platform_data->replay_speaker_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->replay_speaker_route));
			if(ret != codec_platform_data->replay_speaker_route.route) {
				return -1;
			}
		}
		break;

	case SND_DEVICE_HEADSET_AND_SPEAKER:
		if (codec_platform_data && codec_platform_data->replay_headset_and_speaker_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->replay_headset_and_speaker_route));
			if(ret != codec_platform_data->replay_headset_and_speaker_route.route) {
				return -1;
			}
		}
		break;

	case SND_DEVICE_CALL_HEADPHONE:
	case SND_DEVICE_CALL_HEADSET:
		if (codec_platform_data && codec_platform_data->call_headset_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->call_headset_route));
			if(ret != codec_platform_data->call_headset_route.route) {
				return -1;
			}
		}
		break;

	case SND_DEVICE_CALL_HANDSET:
		if (codec_platform_data && codec_platform_data->call_handset_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->call_handset_route));
			if(ret != codec_platform_data->call_handset_route.route) {
				return -1;
			}
		}
		break;

	case SND_DEVICE_CALL_SPEAKER:
		if (codec_platform_data && codec_platform_data->call_speaker_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->call_speaker_route));
			if(ret != codec_platform_data->call_speaker_route.route) {
				return -1;
			}
		}
		break;

	case SND_DEVICE_BUILDIN_MIC:
		if (codec_platform_data && codec_platform_data->record_buildin_mic_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->record_buildin_mic_route));
			if (ret != codec_platform_data->record_buildin_mic_route.route) {
				return -1;
			}
		}
		break;

	case SND_DEVICE_HEADSET_MIC:
		if (codec_platform_data && codec_platform_data->record_headset_mic_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->record_headset_mic_route));
			if (ret != codec_platform_data->record_headset_mic_route.route) {
				return -1;
			}
		}
		break;

	case SND_DEVICE_BUILDIN_RECORD_INCALL:
		if (codec_platform_data && codec_platform_data->record_buildin_incall_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->record_buildin_incall_route));
			if(ret != codec_platform_data->record_buildin_incall_route.route) {
				return -1;
			}
		}
		break;
	case SND_DEVICE_HEADSET_RECORD_INCALL:
		if (codec_platform_data && codec_platform_data->record_headset_incall_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->record_headset_incall_route));
			if(ret != codec_platform_data->record_headset_incall_route.route) {
				return -1;
			}
		}
		break;

	case SND_DEVICE_BT:
		if (codec_platform_data && codec_platform_data->bt_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->bt_route));
			if(ret != codec_platform_data->bt_route.route) {
				return -1;
			}
		}
		break;

	case SND_DEVICE_LOOP_TEST:
		if (codec_platform_data && codec_platform_data->replay_loop_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->replay_loop_route));
			if(ret != codec_platform_data->replay_loop_route.route) {
				return -1;
			}
		}
		break;

	case SND_DEVICE_LINEIN_RECORD:
		if (codec_platform_data && codec_platform_data->record_linein_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->record_linein_route));
			if(ret != codec_platform_data->record_linein_route.route) {
				return -1;
			}
		}
		break;

	case SND_DEVICE_LINEIN1_RECORD:
		if (codec_platform_data && codec_platform_data->record_linein1_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->record_linein1_route));
			if(ret != codec_platform_data->record_linein1_route.route) {
				return -1;
			}
		}
		break;
	case SND_DEVICE_LINEIN2_RECORD:
		if (codec_platform_data && codec_platform_data->record_linein2_route.route) {
			ret = codec_set_board_route(&(codec_platform_data->record_linein2_route));
			if(ret != codec_platform_data->record_linein2_route.route) {
				return -1;
			}
		}
		break;
	case SND_DEVICE_LINEIN3_RECORD:
		ret = -1;
		printk("JZ CODEC: our internal codec not have linein3!\n");
		break;

	default:
		iserror = 1;
		printk("JZ CODEC: Unkown ioctl argument %d in SND_SET_DEVICE\n",device);
	};

	if (!iserror)
		cur_out_device = device;

	return ret;
}

/*---------------------------------------*/

/**
 * CODEC set standby
 *
 * this is just a demo function, and it will be use as default
 * if it is not realized depend on difficent boards
 *
 */

static int codec_set_standby(unsigned int sw)
{
#ifdef CONFIG_CODEC_STANDBY
	printk("JZ_CODEC: waring, %s() is a default function\n", __func__);
	switch(cur_out_device) {
	case SND_DEVICE_SPEAKER:
	case SND_DEVICE_HEADSET_AND_SPEAKER:
		if (sw == STANDBY) {
			/* set the relevant route */
			gpio_enable_hp_mute();
			gpio_disable_spk_en();
			//__codec_switch_sb(POWER_OFF);
		} else {
			/* clean the relevant route */
			//__codec_switch_sb(POWER_ON);
			mdelay(250);
			gpio_enable_spk_en();
			gpio_disable_hp_mute();
		}
		break;
	}
#endif

	return 0;
}

/*---------------------------------------*/
/**
 * CODEC set record rate & data width & volume & channel
 *
 */

static int codec_set_record_rate(unsigned long *rate)
{
	int speed = 0, val;
	unsigned long mrate[MAX_RATE_COUNT] = {
		8000,  11025, 12000, 16000, 22050,
		24000, 32000, 44100, 48000, 88200,96000,
	};

	for (val = 0; val < MAX_RATE_COUNT; val++) {
		if (*rate <= mrate[val]) {
			speed = val;
			break;
		}
	}
	if (*rate > mrate[MAX_RATE_COUNT - 1]) {
		speed = MAX_RATE_COUNT - 1;
	}
	//__codec_set_wind_filter_mode(CODEC_WIND_FILTER_MODE3);
	__codec_enable_adc_high_pass();
	__codec_select_adc_samp_rate(speed);
	if ((val = __codec_get_adc_samp_rate()) == speed) {
		*rate = mrate[speed];
		return 0;
	}
	if (val < MAX_RATE_COUNT && val >=0 )
		*rate = mrate[val];
	else
		*rate = 0;
	return -EIO;
}

static int codec_set_record_data_width(int width)
{
	int supported_width[4] = {16, 18, 20, 24};
	int fix_width;

	for(fix_width = 0; fix_width < 3; fix_width ++)
	{
		if (width <= supported_width[fix_width])
			break;
	}

	__codec_select_adc_word_length(fix_width);
	if (__codec_get_adc_word_length() == fix_width)
		return 0;
	return -EIO;
}

static int codec_set_record_volume(int *val)
{
	int val_tmp;
	if(*val < 0){
                *val = 0;
        }
        if(*val > 100){
                *val = 100;
        }
        val_tmp = *val;

	codec_set_gain_adc_left(val_tmp / 5);
	codec_set_gain_adc_right(val_tmp / 5);
	return 0;
}

static int codec_set_mic_volume(int* val)
{
#ifndef CONFIG_SOUND_XBURST_DEBUG
	/*just set analog gm1 and gm2*/
	int fixed_vol;
	int volume_base;

	if (codec_platform_data &&
	    codec_platform_data->record_volume_base >= 0 &&
	    codec_platform_data->record_volume_base <=20)
	{
		volume_base = codec_platform_data->record_volume_base;

		fixed_vol = (volume_base >> 2) +
			((5 - (volume_base >> 2)) * (*val) / 100);
	}
	else
		fixed_vol = (5 * (*val) / 100);

	__codec_set_gm1(fixed_vol);
	__codec_set_gm2(fixed_vol);
	user_record_volume = *val;
#else
	int val_tmp = *val;
	*val = codec_set_gain_input_left(val_tmp);
	val_tmp = *val;
	*val = codec_set_gain_input_right(val_tmp);
#endif
	return *val;
}

static int codec_set_record_channel(int *channel)
{
	if (*channel != 1)
		*channel = 2;
	return 1;
}

/*---------------------------------------*/
/**
 * CODEC set replay rate & data width & volume & channel
 *
 */

static int codec_set_replay_rate(unsigned long *rate)
{
	int speed = 0, val;
	unsigned long mrate[MAX_RATE_COUNT] = {
		8000,  11025, 12000, 16000, 22050,
		24000, 32000, 44100, 48000, 88200, 96000,
	};
	for (val = 0; val < MAX_RATE_COUNT; val++) {
		if (*rate <= mrate[val]) {
			speed = val;
			break;
		}
	}
	if (*rate > mrate[MAX_RATE_COUNT - 1]) {
		speed = MAX_RATE_COUNT - 1;
	}

	__codec_select_dac_samp_rate(speed);

	if ((val=__codec_get_dac_samp_rate())== speed) {
		*rate = mrate[speed];
		return 0;
	}
	if (val >=0 && val< MAX_RATE_COUNT)
		*rate = mrate[val];
	else
		*rate = 0;
	return -EIO;
}

static int codec_set_replay_data_width(int width)
{
	int supported_width[4] = {16, 18, 20, 24};
	int fix_width;

	for(fix_width = 0; fix_width < 3; fix_width ++)
	{
		if (width <= supported_width[fix_width])
			break;
	}

	__codec_select_dac_word_length(fix_width);

	if (__codec_get_dac_word_length()== fix_width)
		return 0;
	return -EIO;
}

static int codec_set_replay_volume(int *val)
{
	/*just set analog gol and gor*/
	unsigned long fixed_vol;
	int volume_base = 0;

	if (*val >100 || *val < 0) {
		*val = user_replay_volume;
		return -EINVAL;
	}

	if (codec_platform_data &&
	    codec_platform_data->replay_volume_base >= -25 &&
	    codec_platform_data->replay_volume_base <= 6)
		volume_base = codec_platform_data->replay_volume_base;

	fixed_vol = (6 - volume_base) + ((25 + volume_base) * (100 -(*val))/ 100);

	__codec_set_gol(fixed_vol);
	__codec_set_gor(fixed_vol);

	user_replay_volume = *val;

	return 0;
}

static int codec_set_replay_channel(int* channel)
{
	if (*channel != 1)
		*channel = 2;
	return 0;
}
/*---------------------------------------*/
/**
 * CODEC set mute
 *
 * set dac mute used for anti pop
 *
 */
static int codec_mute(int val,int mode)
{
	if (mode & CODEC_WMODE) {
		if (__codec_get_dac_interface_state()) {
			printk("codec_mute dac interface is shutdown\n");
			return 0;
		}
		if (val) {
			if (!__codec_get_dac_mute()) {
				/* enable dac mute */
				__codec_set_irq_flag(1 << IFR_DAC_MUTE_EVENT);
				__codec_enable_dac_mute();
				codec_wait_event_complete(IFR_DAC_MUTE_EVENT,CODEC_IN_MUTE);
			}
		} else {
			if (__codec_get_dac_mute()) {
				/* disable dac mute */
				__codec_set_irq_flag(1 << IFR_DAC_MUTE_EVENT);
				__codec_disable_dac_mute();
				codec_wait_event_complete(IFR_DAC_MUTE_EVENT,CODEC_NOT_MUTE);
			}
		}
	}
	if (mode & CODEC_RMODE) {
		if (__codec_get_adc_interface_state()) {
			printk("codec_mute adc interface is shutdown\n");
			return 0;
		}
		if (val) {
			if (!__codec_get_adc_mute()) {
				/* enable dac mute */
				__codec_set_irq_flag(1 << IFR_ADC_MUTE_EVENT);
				__codec_enable_adc_mute();
				codec_wait_event_complete(IFR_ADC_MUTE_EVENT,CODEC_IN_MUTE);
			}
		} else {
			if (__codec_get_adc_mute()) {
				/* disable dac mute */
				__codec_set_irq_flag(1 << IFR_ADC_MUTE_EVENT);
				__codec_disable_adc_mute();
				codec_wait_event_complete(IFR_ADC_MUTE_EVENT,CODEC_NOT_MUTE);
			}
		}
	}
	return 0;
}

/*---------------------------------------*/
static int codec_debug_routine(void *arg)
{
	return 0;
}

/**
 * CODEC short circut handler
 *
 * To protect CODEC, CODEC will be shutdown when short circut occured.
 * Then we have to restart it.
 */
static inline void codec_short_circut_handler(void)
{
	int	curr_hp_left_vol;
	int	curr_hp_right_vol;
	unsigned int	codec_ifr, delay;

#define VOL_DELAY_BASE 22               //per VOL delay time in ms

	printk("JZ CODEC: Short circut detected! restart CODEC.\n");

	curr_hp_left_vol = codec_get_gain_hp_left();
	curr_hp_right_vol = codec_get_gain_hp_right();

	/* delay */
	delay = VOL_DELAY_BASE * (25 + (curr_hp_left_vol + curr_hp_right_vol)/2);

	/* set hp gain to min */
	codec_set_gain_hp_left(-25);
	codec_set_gain_hp_right(-25);

	printk("Short circut volume delay %d ms curr_hp_left_vol=%x curr_hp_right_vol=%x \n",
	       delay, curr_hp_left_vol, curr_hp_right_vol);
	codec_sleep(delay);

	/*  turn off sb_hp */
	g_short_circut_state = 1;
	while (1) {
		codec_ifr = __codec_get_irq_flag();
		if ((codec_ifr & (1 << IFR_SCLR_EVENT)) == 0) {
			printk("Out short circut .\n");
			g_short_circut_state = 0;
			break;
		}

		codec_set_hp(HP_DISABLE);
		__codec_switch_sb_sleep(POWER_OFF);
		codec_sleep(10);
		__codec_switch_sb(POWER_OFF);
		codec_sleep(10);
		printk("Short circut shutdown codec.\n");

		__codec_set_irq_flag(1 << IFR_SCLR_EVENT);
		codec_sleep(10);
		__codec_switch_sb(POWER_ON);
		codec_sleep(250);
		__codec_switch_sb_sleep(POWER_ON);
		codec_sleep(400);
		codec_set_hp(HP_ENABLE);
	}


	/* restore hp gain */
	codec_set_gain_hp_left(curr_hp_left_vol);
	codec_set_gain_hp_right(curr_hp_right_vol);

	codec_sleep(delay);

#undef VOL_DELAY_BASE

	printk("JZ CODEC: Short circut restart CODEC hp out finish.\n");
}

/**
 * IRQ routine
 */
static int codec_irq_handle(struct work_struct *detect_work)
{
	unsigned char codec_ifr;
	unsigned char codec_ifr2;
#ifdef CONFIG_JZ_HP_DETECT_CODEC
	int old_status = 0;
	int new_status = 0;
	int i;
#endif /*CONFIG_JZ_HP_DETECT_CODEC*/
	codec_ifr = __codec_get_irq_flag();
	codec_ifr2 = __codec_get_irq_flag2();

	/* Mask all irq temporarily */
	if ((codec_ifr & (~ICR_COMMON_MASK & ICR_ALL_MASK))){
		do {
			if (codec_ifr & (1 << IFR_SCLR_EVENT)) {
				printk("JZ CODEC: Short circut detected! codec_ifr = 0x%02x\n",codec_ifr);
				codec_short_circut_handler();
			}

			/* Updata SCMC/SCLR */
			__codec_set_irq_flag(1 << IFR_SCLR_EVENT);

			codec_ifr = __codec_get_irq_flag();

		} while(codec_ifr & (1 << IFR_SCLR_EVENT));

#ifdef CONFIG_JZ_HP_DETECT_CODEC

		if (codec_ifr & (1 << IFR_JACK_EVENT)) {
		_ensure_stable:
			old_status = ((__codec_get_sr() & CODEC_JACK_MASK) != 0);
			/* Read status at least 3 times to make sure it is stable. */
			for (i = 0; i < 3; ++i) {
				old_status = ((__codec_get_sr() & CODEC_JACK_MASK) != 0);
				codec_sleep(50);
			}
		}
		__codec_set_irq_flag(codec_ifr);

		codec_ifr = __codec_get_irq_flag();
		codec_sleep(10);

		/* If the jack status has changed, we have to redo the process. */
		if (codec_ifr & (1 << IFR_JACK_EVENT)) {
			codec_sleep(50);
			new_status = ((__codec_get_sr() & CODEC_JACK_MASK) != 0);
			if (new_status != old_status) {
				goto _ensure_stable;
			}
		}

		/* Report status */
		if(!work_pending(detect_work))
			schedule_work(detect_work);

#endif /*CONFIG_JZ_HP_DETECT_CODEC*/
	}

	__codec_set_irq_flag(codec_ifr);
	/* Unmask SCMC & JACK (ifdef CONFIG_JZ_HP_DETECT_CODEC) */
	__codec_set_irq_mask(ICR_COMMON_MASK);

	return 0;
}

static int codec_get_hp_state(int *state)
{
#if defined(CONFIG_JZ_HP_DETECT_CODEC)
	*state = ((__codec_get_sr() & CODEC_JACK_MASK) >> SR_JACK) ^
		(!codec_platform_data->hpsense_active_level);
	if (*state < 0)
		return -EIO;
#elif defined(CONFIG_JZ_HP_DETECT_GPIO)
	if(codec_platform_data &&
	   (codec_platform_data->gpio_hp_detect.gpio != -1)) {
		*state  = __gpio_get_value(codec_platform_data->gpio_hp_detect.gpio);
	}
	else
		return -EIO;
#endif
	return 0;
}

static void codec_get_format_cap(unsigned long *format)
{
	*format = AFMT_S24_LE|AFMT_U24_LE|AFMT_U16_LE|AFMT_S16_LE|AFMT_S8|AFMT_U8;
}

static void codec_debug_default(void)
{
	int ret = 4;
	while(ret--) {
		gpio_disable_spk_en();
		printk("disable %d\n",ret);
		mdelay(10000);
		gpio_enable_spk_en();
		printk("enable %d\n",ret);
		mdelay(10000);
	}
	ret = 4;
	while(ret--) {
		printk("============\n");
		printk("hp disable\n");
		codec_set_hp(HP_DISABLE);
		mdelay(10000);
		printk("spk gpio disable\n");
		gpio_disable_spk_en();
		mdelay(10000);
		printk("hp enable\n");
		codec_set_hp(HP_ENABLE);
		mdelay(10000);
		printk("spk gpio enable\n");
		gpio_enable_spk_en();
		mdelay(10000);

	}
}

static void codec_debug(char arg)
{
	switch(arg) {
		/*...*/
	default:
		codec_debug_default();
	}
}

/***************************************************************************************\
 *                                                                                     *
 *control interface                                                                    *
 *                                                                                     *
 \***************************************************************************************/
/**
 * CODEC ioctl (simulated) routine
 *
 * Provide control interface for i2s driver
 */
static int jzcodec_ctl(unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	DUMP_IOC_CMD("enter");
	{
		switch (cmd) {

		case CODEC_INIT:
			ret = codec_init();
			break;

		case CODEC_TURN_OFF:
			ret = codec_turn_off(arg);
			break;

		case CODEC_SHUTDOWN:
			ret = codec_shutdown();
			break;

		case CODEC_RESET:
			ret = codec_reset();
			break;

		case CODEC_SUSPEND:
			ret = codec_suspend();
			break;

		case CODEC_RESUME:
			ret = codec_resume();
			break;

		case CODEC_ANTI_POP:
			ret = codec_anti_pop_start((int)arg);
			break;

		case CODEC_SET_DEFROUTE:
			ret = codec_set_default_route((int )arg);
			break;

		case CODEC_SET_DEVICE:
			ret = codec_set_device(*(enum snd_device_t *)arg);
			break;

		case CODEC_SET_STANDBY:
			ret = codec_set_standby(*(unsigned int *)arg);
			break;

		case CODEC_SET_RECORD_RATE:
			ret = codec_set_record_rate((unsigned long *)arg);
			break;

		case CODEC_SET_RECORD_DATA_WIDTH:
			ret = codec_set_record_data_width((int)arg);
			break;

		case CODEC_SET_MIC_VOLUME:
			ret = codec_set_mic_volume((int *)arg);
			break;

		case CODEC_SET_RECORD_VOLUME:
			ret = codec_set_record_volume((int *)arg);
			break;

		case CODEC_SET_RECORD_CHANNEL:
			ret = codec_set_record_channel((int*)arg);
			break;

		case CODEC_SET_REPLAY_RATE:
			ret = codec_set_replay_rate((unsigned long*)arg);
			break;

		case CODEC_SET_REPLAY_DATA_WIDTH:
			ret = codec_set_replay_data_width((int)arg);
			break;

		case CODEC_SET_REPLAY_VOLUME:
			ret = codec_set_replay_volume((int*)arg);
			break;

		case CODEC_SET_REPLAY_CHANNEL:
			ret = codec_set_replay_channel((int*)arg);
			break;

		case CODEC_GET_REPLAY_FMT_CAP:
		case CODEC_GET_RECORD_FMT_CAP:
			codec_get_format_cap((unsigned long *)arg);
			break;

		case CODEC_DAC_MUTE:
			ret = codec_mute((int)arg,CODEC_WMODE);
			break;
		case CODEC_ADC_MUTE:
			ret = codec_mute((int)arg,CODEC_RMODE);
			break;
		case CODEC_DEBUG_ROUTINE:
			ret = codec_debug_routine((void *)arg);
			break;

		case CODEC_IRQ_HANDLE:
			ret = codec_irq_handle((struct work_struct*)arg);
			break;

		case CODEC_GET_HP_STATE:
			ret = codec_get_hp_state((int*)arg);
			break;

		case CODEC_DUMP_REG:
			dump_codec_regs();
		case CODEC_DUMP_GPIO:
			dump_gpio_state();
			ret = 0;
			break;
		case CODEC_CLR_ROUTE:
			if ((int)arg == CODEC_RMODE)
				codec_set_route(SND_ROUTE_RECORD_CLEAR);
			else if ((int)arg == CODEC_WMODE)
				codec_set_route(SND_ROUTE_REPLAY_CLEAR);
			else {
				codec_set_route(SND_ROUTE_ALL_CLEAR);
			}
			ret = 0;
			break;
		case CODEC_DEBUG:
			codec_debug((char)arg);
			ret = 0;
			break;
		default:
			printk("JZ CODEC:%s:%d: Unkown IOC commond\n", __FUNCTION__, __LINE__);
			ret = -1;
		}
	}

	DUMP_IOC_CMD("leave");
	return ret;
}

static int jz_codec_probe(struct platform_device *pdev)
{
	codec_platform_data = pdev->dev.platform_data;

	if (!codec_platform_data->codec_sys_clk)
		codec_platform_data->codec_sys_clk = CODEC_SYS_CLK_12M;
	else
		codec_platform_data->codec_sys_clk = CODEC_SYS_CLK_13M;

	if (!codec_platform_data->codec_dmic_clk)
		codec_platform_data->codec_dmic_clk = CODEC_DMIC_CLK_OFF;
	else
		codec_platform_data->codec_dmic_clk = CODEC_DMIC_CLK_ON;

#if defined(CONFIG_JZ_HP_DETECT_CODEC)
	jz_set_hp_detect_type(SND_SWITCH_TYPE_CODEC,NULL,
			      &codec_platform_data->gpio_mic_detect,
			      &codec_platform_data->gpio_mic_detect_en,
			      &codec_platform_data->gpio_buildin_mic_select,
			      codec_platform_data->hook_active_level);
#elif  defined(CONFIG_JZ_HP_DETECT_GPIO)
	jz_set_hp_detect_type(SND_SWITCH_TYPE_GPIO,
			      &codec_platform_data->gpio_hp_detect,
			      &codec_platform_data->gpio_mic_detect,
			      &codec_platform_data->gpio_mic_detect_en,
			      &codec_platform_data->gpio_buildin_mic_select,
			      codec_platform_data->hook_active_level);
#endif
	if (codec_platform_data->gpio_mic_detect.gpio != -1 )
		if (gpio_request(codec_platform_data->gpio_mic_detect.gpio,"gpio_mic_detect") < 0) {
			gpio_free(codec_platform_data->gpio_mic_detect.gpio);
			gpio_request(codec_platform_data->gpio_mic_detect.gpio,"gpio_mic_detect");
		}

	if (codec_platform_data->gpio_buildin_mic_select.gpio != -1 )
		if (gpio_request(codec_platform_data->gpio_buildin_mic_select.gpio,"gpio_buildin_mic_switch") < 0) {
			gpio_free(codec_platform_data->gpio_buildin_mic_select.gpio);
			gpio_request(codec_platform_data->gpio_buildin_mic_select.gpio,"gpio_buildin_mic_switch");
		}

	if (codec_platform_data->gpio_mic_detect_en.gpio != -1 &&
	    codec_platform_data->gpio_buildin_mic_select.gpio !=
	    codec_platform_data->gpio_mic_detect_en.gpio)
		/*many times gpio_mic_detect_en is equel to gpio_buildin_mic_select*/
		if (gpio_request(codec_platform_data->gpio_mic_detect_en.gpio,"gpio_mic_detect_en") < 0) {
			gpio_free(codec_platform_data->gpio_mic_detect_en.gpio);
			gpio_request(codec_platform_data->gpio_mic_detect_en.gpio,"gpio_mic_detect_en");
		}

	if (codec_platform_data->gpio_hp_mute.gpio != -1 )
		if (gpio_request(codec_platform_data->gpio_hp_mute.gpio,"gpio_hp_mute") < 0) {
			gpio_free(codec_platform_data->gpio_hp_mute.gpio);
			gpio_request(codec_platform_data->gpio_hp_mute.gpio,"gpio_hp_mute");
		}
	if (codec_platform_data->gpio_spk_en.gpio != -1 )
		if (gpio_request(codec_platform_data->gpio_spk_en.gpio,"gpio_spk_en") < 0) {
			gpio_free(codec_platform_data->gpio_spk_en.gpio);
			gpio_request(codec_platform_data->gpio_spk_en.gpio,"gpio_spk_en");
		}
	if (codec_platform_data->gpio_handset_en.gpio != -1 )
		if (gpio_request(codec_platform_data->gpio_handset_en.gpio,"gpio_handset_en") < 0) {
			gpio_free(codec_platform_data->gpio_handset_en.gpio);
			gpio_request(codec_platform_data->gpio_handset_en.gpio,"gpio_handset_en");
		}

	return 0;
}

static int __devexit jz_codec_remove(struct platform_device *pdev)
{
	gpio_free(codec_platform_data->gpio_hp_mute.gpio);
	gpio_free(codec_platform_data->gpio_spk_en.gpio);
	codec_platform_data = NULL;

	return 0;
}

static struct platform_driver jz_codec_driver = {
	.probe		= jz_codec_probe,
	.remove		= __devexit_p(jz_codec_remove),
	.driver		= {
		.name	= "jz_codec",
		.owner	= THIS_MODULE,
	},
};

void codec_irq_set_mask()
{
	__codec_set_irq_mask(ICR_ALL_MASK);
	__codec_set_irq_mask2(ICR_ALL_MASK2);
};

/***************************************************************************************\
 *module init                                                                          *
\***************************************************************************************/
/**
 * Module init
 */
#define JZ4780_INTERNAL_CODEC_CLOCK 12000000
static int __init init_codec(void)
{
	int retval;

	i2s_register_codec("internal_codec", (void *)jzcodec_ctl,JZ4780_INTERNAL_CODEC_CLOCK,CODEC_MASTER);
	retval = platform_driver_register(&jz_codec_driver);
	if (retval) {
		printk("JZ CODEC: Could net register jz_codec_driver\n");
		return retval;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.suspend = codec_early_suspend;
	early_suspend.resume = codec_late_resume;
	early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&early_suspend);
#endif


	return 0;
}

/**
 * Module exit
 */
static void __exit cleanup_codec(void)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&early_suspend);
	platform_driver_unregister(&jz_codec_driver);
#endif
}
arch_initcall(init_codec);
module_exit(cleanup_codec);
